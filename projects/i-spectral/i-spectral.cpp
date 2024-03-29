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
// #define CONTROL_LOP 1

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
	// audio outputs get accumulated here (stereo)
	float *output[2]; //[BUFFER];
	// FFT of phase motion for input
	ne10_fft_cpx_float32_t *motion; //[BLOCK];
	// FFT of spectrum for output
	ne10_fft_cpx_float32_t *synth; //[BLOCK];
	// real time domain of (I)FFT operations
	float *block; //[BLOCK];
	// raised cosine window
	float *window; //[BLOCK];
	// counts up to hop size
	unsigned int sampleIx;
	// ring buffer index for input buffer writing
	unsigned int inputIx;
	// ring buffer index for output buffer writing
	unsigned int outputIxW;
	// ring buffer index for output buffer reading
	unsigned int outputIxR;
	// (I)FFT configuration data
	ne10_fft_r2c_cfg_float32_t fftConfig;
	// copies of the ring buffer indices for use by the FFT process
	unsigned int fftOutputIxW;
	unsigned int fftInputIx;
	// background task for FFT
	AuxiliaryTask processTask;
	// flag to detect too-slow computation
	volatile bool inProcess;
	// audio output DC blocking filter state (stereo)
	float dc[2];
	// phase input bandpasss filter
	LOP phaseLowpass;
	HIP phaseHighpass;
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
	C->inProcess = true;

	// reblock and window input
	unsigned int inputIx = (C->fftInputIx + BUFFER - BLOCK) % BUFFER;
	for (unsigned int n = 0; n < BLOCK; ++n)
	{
		C->block[n] = C->input[inputIx] * C->window[n];
		++inputIx;
		inputIx %= BUFFER;
	}

	// fft
	ne10_fft_r2c_1d_float32_neon(C->motion, C->block, C->fftConfig);

	// stereo sound synthesizer
	for (unsigned int channel = 0; channel < 2; ++channel)
	{
		for (unsigned int n = 0; n < BLOCK; ++n)
		{
			// exploit symmetry of FFT of real-valued signal
			if (n > BLOCK / 2)
			{
				C->synth[n].r = C->synth[BLOCK - n].r;
				C->synth[n].i = -C->synth[BLOCK - n].i;
			}
			else
			{
				std::complex<float> spectrum(C->motion[n].r, C->motion[n].i);
				// paulstretch-style phase randomisation
				// a bit spacier than a phase vocoder but no unpleasant artifacts
				// and much easier to implement
				float p = 2.0f * 3.141592653f * rand() / (float) RAND_MAX;
				spectrum *= std::complex<float>(std::cos(p), std::sin(p));
				C->synth[n].r = spectrum.real();
				C->synth[n].i = spectrum.imag();
			}
		}

		// ifft
		ne10_fft_c2r_1d_float32_neon(C->block, C->synth, C->fftConfig);

		// windowed overlap add into output buffer
		unsigned int output_ptr = C->fftOutputIxW % BUFFER;
		for (unsigned int n = 0; n < BLOCK; ++n)
		{
			C->output[channel][output_ptr] += C->block[n] * C->window[n];
			++output_ptr;
			output_ptr %= BUFFER;
		}
	}

	// we're done
	C->inProcess = false;
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
	NEW(C->output[0], float, BUFFER)
	NEW(C->output[1], float, BUFFER)
	NEW(C->motion, ne10_fft_cpx_float32_t, BLOCK)
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
	C->outputIxW = HOP;
	if (! (C->processTask = Bela_createAuxiliaryTask(&COMPOSITION_process, 90, "fft-process")))
	{
		return false;
	}
	if (! (C->fftConfig = ne10_fft_alloc_r2c_float32(BLOCK)))
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
	float phaseBandpass = 2 * phase - 1;
	phaseBandpass = lop(&C->phaseLowpass, hip(&C->phaseHighpass, phaseBandpass, 10.0f / (float)HOP), 100.0f);
	if (C->sampleIx == 0)
	{
		// decimate (one sample per HOP)
		C->input[C->inputIx] = phaseBandpass;
		// advance
		++C->inputIx;
		C->inputIx %= BUFFER;
	}
	// amplify
	float gain = magnitude;
	gain *= gain * GAIN;
	for (unsigned int channel = 0; channel < 2; ++channel)
	{
		float o = gain * C->output[channel][C->outputIxR];

		// simple dc-blocking high pass filter
		C->dc[channel] *= 0.999f;
		C->dc[channel] += 0.001f * o;
		o -= C->dc[channel];

		// write output with waveshaping
		out[channel] = std::tanh(o);

		// reset for next overlap add
		C->output[channel][(C->outputIxR + BUFFER / 2) % BUFFER] = 0;
	}

	// advance
	++C->outputIxR;
	C->outputIxR %= BUFFER;
	++C->outputIxW;
	C->outputIxW %= BUFFER;
	++C->sampleIx;
	if (C->sampleIx >= HOP)
	{
		// enqueue FFT process
		C->fftInputIx = C->inputIx;
		C->fftOutputIxW = C->outputIxW;
		if (C->inProcess)
		{
			// the FFT task is still in progress, should not happen
			rt_printf("TOO SLOW\n");
		}
		else
		{
			Bela_scheduleAuxiliaryTask(C->processTask);
		}
		C->sampleIx = 0;
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
		NE10_FREE(C->output[0]);
		NE10_FREE(C->output[1]);
		NE10_FREE(C->motion);
		NE10_FREE(C->synth);
		NE10_FREE(C->block);
		NE10_FREE(C->window);
	}
	gC = nullptr;
}

//---------------------------------------------------------------------

REBUS

//---------------------------------------------------------------------
