//---------------------------------------------------------------------
// abstracted composition core
// this file is included by render.cpp

#include <complex>

//---------------------------------------------------------------------
// composition parameters

#define BUFFER 65536 // bigger than BLOCK * 2, for overlap add
#define BLOCK 1024 // power of 2, for FFT
#define HOP 256
#define OVERLAP (BLOCK / HOP)
#define GAIN ((1.0f / BLOCK) * (0.5f / OVERLAP)) // fft->ifft gain * overlap-add gain

//---------------------------------------------------------------------

struct COMPOSITION
{
	float *input; //[BUFFER];
	float *output; //[BUFFER];
	ne10_fft_cpx_float32_t *motion[2]; //[BLOCK];
	ne10_fft_cpx_float32_t *synth; //[BLOCK];
	float *block; //[BLOCK];
	float *window; //[BLOCK];
	unsigned int sample_ix;
	unsigned int input_ix;
	unsigned int output_ix_w;
	unsigned int output_ix_r;
	ne10_fft_r2c_cfg_float32_t fft_config;
	unsigned int fft_output_ix_w;
	unsigned int fft_input_ix;
	unsigned int fft_motion_ix;
	AuxiliaryTask process_task;
	volatile bool inprocess;
};

//---------------------------------------------------------------------
// FFT process

struct COMPOSITION *gC = nullptr;

void COMPOSITION_process(void *)
{
	COMPOSITION *C = gC;
	C->inprocess = true;
	// reblock and window
	unsigned int input_ptr = (C->fft_input_ix + BUFFER - BLOCK) % BUFFER;
	for (unsigned int n = 0; n < BLOCK; ++n)
	{
		C->block[n] = C->input[input_ptr] * C->window[n];
		++input_ptr;
		input_ptr %= BUFFER;
	}
	// fft
	ne10_fft_r2c_1d_float32_neon(C->motion[C->fft_motion_ix], C->block, C->fft_config);
	// phase vocoder synthesizer
	for (unsigned int n = 0; n < BLOCK; ++n)
	{
		std::complex<float> next(C->motion[C->fft_motion_ix][n].r, C->motion[C->fft_motion_ix][n].i);
		std::complex<float> now(C->motion[! C->fft_motion_ix][n].r, C->motion[! C->fft_motion_ix][n].i);
		std::complex<float> then(C->synth[n].r, C->synth[n].i);
		std::complex<float> advance = next / now;
		float a = std::abs(advance);
		if (a == 0) advance = 1; else advance /= a;
		float t = std::abs(then);
		if (t == 0) then = 1; else then /= t;
		float gain = std::abs(next);
		for (unsigned int k = 1; k < HOP; k <<= 1)
		{
			advance *= advance;
		}
		then = then * gain * advance;
		C->synth[n].r = then.real();
		C->synth[n].i = then.imag();
	}
	C->fft_motion_ix = ! C->fft_motion_ix;
	// ifft
	ne10_fft_c2r_1d_float32_neon(C->block, C->synth, C->fft_config);
	// overlap add
	unsigned int output_ptr = C->fft_output_ix_w % BUFFER;
	for (unsigned int n = 0; n < BLOCK; ++n)
	{
		C->output[output_ptr] += C->block[n] * C->window[n];
		++output_ptr;
		output_ptr %= BUFFER;
	}
	C->inprocess = false;
}

//---------------------------------------------------------------------

static inline
bool COMPOSITION_setup(BelaContext *context, COMPOSITION *C)
{
	std::memset(C, 0, sizeof(*C));
#define NEW(ptr, type, count) \
	ptr = (type *) NE10_MALLOC(sizeof(*ptr) * count); \
	if (! ptr) { return false; } \
	std::memset(ptr, 0, sizeof(*ptr) * count);
	NEW(C->input, float, BUFFER)
	NEW(C->output, float, BUFFER)
	NEW(C->motion[0], ne10_fft_cpx_float32_t, BLOCK)
	NEW(C->motion[1], ne10_fft_cpx_float32_t, BLOCK)
	NEW(C->synth, ne10_fft_cpx_float32_t, BLOCK)
	NEW(C->block, float, BLOCK)
	NEW(C->window, float, BLOCK)
#undef NEW
	for (unsigned int n = 0; n < BLOCK; ++n)
	{
		C->window[n] = (1 - std::cos(2 * M_PI * n / BLOCK)) / 2;
	}
	C->output_ix_w = HOP;
	if (! (C->process_task = Bela_createAuxiliaryTask(&COMPOSITION_process, 90, "fft-process")))
	{
		return false;
	}
	if (! (C->fft_config = ne10_fft_alloc_r2c_float32(BLOCK)))
	{
		return false;
	}
	gC = C;
	rt_printf("I Spectral That Hand Motion\ninput length %.3f seconds\n", BLOCK * HOP / (float) context->audioSampleRate);
	return true;
}

//---------------------------------------------------------------------

static inline
void COMPOSITION_render(BelaContext *context, COMPOSITION *C, float out[2], const float in[2], const float magnitude, const float phase)
{
	// read input
	if (C->sample_ix == 0)
	{
		C->input[C->input_ix] = phase;
		// advance
		++C->input_ix;
		C->input_ix %= BUFFER;
	}
	// write output
	float o = GAIN * magnitude * C->output[C->output_ix_r];
	out[0] = std::tanh(o);
	out[1] = std::sin(o);
	// reset for next overlap add
	C->output[(C->output_ix_r + BUFFER / 2) % BUFFER] = 0;
	// advance
	++C->output_ix_r;
	C->output_ix_r %= BUFFER;
	++C->output_ix_w;
	C->output_ix_w %= BUFFER;
	++C->sample_ix;
	if (C->sample_ix >= HOP)
	{
		// enqueue FFT process
		C->fft_input_ix = C->input_ix;
		C->fft_output_ix_w = C->output_ix_w;
		if (C->inprocess)
		{
			rt_printf("TOO SLOW\n");
		}
		else
		{
			Bela_scheduleAuxiliaryTask(C->process_task);
		}
		C->sample_ix = 0;
	}
}

//---------------------------------------------------------------------

static inline
void COMPOSITION_cleanup(BelaContext *context, COMPOSITION *C)
{
	if (C)
	{
		NE10_FREE(C->input);
		NE10_FREE(C->output);
		NE10_FREE(C->motion[0]);
		NE10_FREE(C->motion[1]);
		NE10_FREE(C->synth);
		NE10_FREE(C->block);
		NE10_FREE(C->window);
	}
	gC = nullptr;
}

//---------------------------------------------------------------------
