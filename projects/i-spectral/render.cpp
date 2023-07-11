//---------------------------------------------------------------------
/*

I Spectral That Hand Motion
by Claude Heiland-Allen 2023-06-15, 2023-07-11

An extreme pitch shift (upwards by log2(HOP) octaves)
turns last few seconds of phase motion into sound.

Magnitude input adjust volume with wave-shaping distortion.

Faster oscillating motion makes higher pitched sounds.
Slower oscillating motion makes lower pitched sounds.

*/

//---------------------------------------------------------------------
// options

// #define MODE 1
// #define RECORD 1
// #define SCOPE 1

//---------------------------------------------------------------------
// dependencies

#include <libraries/REBUS/REBUS.h>

#include <libraries/REBUS/dsp.h>
#include <libraries/ne10/NE10.h>
#include <complex>

//---------------------------------------------------------------------
// added to audio recording filename

const char *COMPOSITION_name = "i-spectral";

//---------------------------------------------------------------------
// composition parameters

#define BUFFER 65536 // bigger than BLOCK * 2, for overlap add
#define BLOCK 1024 // power of 2, for FFT
#define HOP 256
#define OVERLAP (BLOCK / HOP)
#define GAIN (4.0f * (0.5f / OVERLAP)) // waveshaper gain * overlap-add gain

//---------------------------------------------------------------------

struct COMPOSITION
{
	// phase motion gets stored here
	float *input; //[BUFFER];
	// audio output gets accumulated here
	float *output; //[BUFFER];
	// two buffers of FFT of phase motion for input
	ne10_fft_cpx_float32_t *motion[2]; //[BLOCK];
	// one buffer of FFT for output
	ne10_fft_cpx_float32_t *synth; //[BLOCK];
	// real time domain of (I)FFT operations
	float *block; //[BLOCK];
	// raised cosine window
	float *window; //[BLOCK];
	// counts up to hop size
	unsigned int sample_ix;
	// ring buffer index for input buffer writing
	unsigned int input_ix;
	// ring buffer index for output buffer writing
	unsigned int output_ix_w;
	// ring buffer index for output buffer reading
	unsigned int output_ix_r;
	// (I)FFT configuration data
	ne10_fft_r2c_cfg_float32_t fft_config;
	// copies of the ring buffer indices for use by the FFT process
	unsigned int fft_output_ix_w;
	unsigned int fft_input_ix;
	// which of the two motion buffers is current
	unsigned int fft_motion_ix;
	// background task for FFT
	AuxiliaryTask process_task;
	// flag to detect too-slow computation
	volatile bool inprocess;
	// audio output DC blocking filter state
	float dc;
	// phase input bandpasss filter
	LOP phase_lop;
	HIP phase_hip;
};

//---------------------------------------------------------------------
// FFT process

// global composition pointer (set in setup function)
struct COMPOSITION *gC = nullptr;

void COMPOSITION_process(void *)
{
	// read the global composition pointer
	COMPOSITION *C = gC;

	// we're busy
	// if we're still busy by the time of the next block,
	// then the computer is too slow
	C->inprocess = true;

	// reblock and window input
	unsigned int input_ptr = (C->fft_input_ix + BUFFER - BLOCK) % BUFFER;
	for (unsigned int n = 0; n < BLOCK; ++n)
	{
		C->block[n] = C->input[input_ptr] * C->window[n];
		++input_ptr;
		input_ptr %= BUFFER;
	}

	// fft
	ne10_fft_r2c_1d_float32_neon(C->motion[C->fft_motion_ix], C->block, C->fft_config);

	// sound synthesizer
	for (unsigned int n = 0; n < BLOCK; ++n) // TODO exploit real FFT symmetry
	{
		std::complex<float> next(C->motion[C->fft_motion_ix][n].r, C->motion[C->fft_motion_ix][n].i);
		float gain = std::abs(next);

#if 0

		// phase vocoder phase advance
		// has strange uncontrolled low frequency chirp artifacts
		std::complex<float> now(C->motion[! C->fft_motion_ix][n].r, C->motion[! C->fft_motion_ix][n].i);
		std::complex<float> then(C->synth[n].r, C->synth[n].i);
		std::complex<float> advance = next / now;
		float a = std::abs(advance);
		if (a == 0) advance = 1; else advance /= a;
		float t = std::abs(then);
		if (t == 0) then = 1; else then /= t;
		// not sure if it should be advance**log2(HOP) (easy) or advance**(1/log2(HOP)) (hard)...
		for (unsigned int k = 1; k < HOP; k <<= 1)
		{
			advance *= advance;
		}
		then = then * gain * advance;

#else

		// paulstretch-style phase randomisation
		// a bit spacier but no unpleasant artifacts
		float p = 2.0f * 3.141592653 * rand() / (float) RAND_MAX;
		std::complex<float> then = gain * std::complex<float>(std::cos(p), std::sin(p));

#endif

		C->synth[n].r = then.real();
		C->synth[n].i = then.imag();
	}

	// swap buffer index
	C->fft_motion_ix = ! C->fft_motion_ix;

	// ifft
	ne10_fft_c2r_1d_float32_neon(C->block, C->synth, C->fft_config);

	// windowed overlap add into output buffer
	unsigned int output_ptr = C->fft_output_ix_w % BUFFER;
	for (unsigned int n = 0; n < BLOCK; ++n)
	{
		C->output[output_ptr] += C->block[n] * C->window[n];
		++output_ptr;
		output_ptr %= BUFFER;
	}

	// we're done
	C->inprocess = false;
}

//---------------------------------------------------------------------

inline
bool COMPOSITION_setup(BelaContext *context, COMPOSITION *C)
{

	// clear memory to 0
	std::memset(C, 0, sizeof(*C));

	// allocate aligned buffers and  clear them to 0
	// define a macro to avoid so much code repetition
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

	// macro is no longer necessary
#undef NEW

	// raised cosine window
	for (unsigned int n = 0; n < BLOCK; ++n)
	{
		C->window[n] = (1 - std::cos(2 * M_PI * n / BLOCK)) / 2;
	}

	// FFT task runs at lower priority possibly taking several DSP blocks to complete
	C->output_ix_w = HOP;
	if (! (C->process_task = Bela_createAuxiliaryTask(&COMPOSITION_process, 90, "fft-process")))
	{
		return false;
	}
	if (! (C->fft_config = ne10_fft_alloc_r2c_float32(BLOCK)))
	{
		return false;
	}

	// set global state pointer
	gC = C;

	rt_printf("I Spectral That Hand Motion\ninput length %.3f seconds\n",
		BLOCK * HOP / (double) context->audioSampleRate);
	return true;
}

//---------------------------------------------------------------------

inline
void COMPOSITION_render(BelaContext *context, COMPOSITION *C,
  float out[2], const float in[2], const float magnitude, const float phase)
{
	// read input
	// band-pass filter (hip to avoid DC offsets, lop to avoid aliasing)
	float phase_bp = 2 * phase - 1;
	phase_bp = lop(&C->phase_lop, hip(&C->phase_hip, phase_bp, 10.0f / (float)HOP), 100.0f);
	if (C->sample_ix == 0)
	{
		// decimate (one sample per HOP)
		C->input[C->input_ix] = phase_bp;
		// advance
		++C->input_ix;
		C->input_ix %= BUFFER;
	}
	// amplify
	float gain = magnitude;
	gain *= gain;
	float o = GAIN * gain * C->output[C->output_ix_r];

	// simple dc-blocking high pass filter
	C->dc *= 0.999f;
	C->dc += 0.001f * o;
	o -= C->dc;

	// write output with waveshaping
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
			// the FFT task is still in progress, should not happen
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

inline
void COMPOSITION_cleanup(BelaContext *context, COMPOSITION *C)
{
	if (C)
	{
		// free buffers
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

REBUS

//---------------------------------------------------------------------
