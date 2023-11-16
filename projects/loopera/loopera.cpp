//---------------------------------------------------------------------
/*

Loopera
by Claude Heiland-Allen 2023-09-26

Simple audio loop granulator.

Magnitude controls length.
Phase controls starting offset.

NOTE: audio loop is not included!
Audio loop file is expected to be called 'loop.wav' next to this file.

*/

//---------------------------------------------------------------------
// options

// #define RECORD 1
// #define CONTROL_LOP 1

//---------------------------------------------------------------------
// dependencies

#include <libraries/REBUS/REBUS.h>
#include <libraries/sndfile/sndfile.h>

//---------------------------------------------------------------------
// added to audio recording filename

const char *COMPOSITION_name = "loopera";

//---------------------------------------------------------------------
// composition state

struct COMPOSITION
{

	// audio loop data
	float *loop;
	int channels;
	int frames;

	// ramps through [0 to frames) continuously
	int clock;

};

//---------------------------------------------------------------------
// called during setup

inline
bool COMPOSITION_setup(BelaContext *context, struct COMPOSITION *C)
{

	// clear everything to 0
	std::memset(C, 0, sizeof(*C));

	// open sound file
	SF_INFO info;
	SNDFILE *in = sf_open("loop.wav", SFM_READ, &info);
	if (! in)
	{
		rt_printf("Could not open 'loop.wav'.\n");
		return false;
	}

	// check sample rate
	if (info.samplerate != context->audioSampleRate)
	{
		rt_printf("Sample rate mismatch: using %f, expected %f\n", (double) context->audioSampleRate, (double) info.samplerate);
		// return false; // it'll sound at a different pitch and speed, should this be a hard failure?
	}

	// allocate memory
	C->loop = (float *) std::calloc(1, sizeof(*C->loop) * info.channels * info.frames);
	if (! C->loop)
	{
		rt_printf("Could not allocate memory for 'loop.wav'\n");
		sf_close(in);
		return false;
	}

	// read audio data
	if (info.frames != sf_readf_float(in, C->loop, info.frames))
	{
		rt_printf("Could not read 'loop.wav'.\n");
		sf_close(in);
		return false;
	}

	// close input
	sf_close(in);

	// initialize remaining state
	C->channels = info.channels;
	C->frames = info.frames;
	C->clock = 0;
	return true;
}

//---------------------------------------------------------------------
// called once per audio frame (default 44100Hz float rate)

inline
void COMPOSITION_render(BelaContext *context, struct COMPOSITION *C,
  float out[2], const float in[2], const float magnitude, const float phase)
{

	// quantized mapping in powers of two
	float scale = 8; // magic number, tune to taste
	float unit = exp2f(fminf(0, ceilf(-scale * magnitude)));
	int length = C->frames * unit;
	int offset = max(0, C->frames * floorf(phase / unit) * unit);

	// prevent division by zero (replaces 0 with 1)
	length += ! length;
	
	// advance loop
	C->clock = (C->clock + 1) % C->frames;
	int index = ((C->clock % length) + offset) % C->frames;

	// read audio data from loop
	for (int channel = 0; channel < 2; ++channel)
	{
		out[channel] = C->loop[C->channels * index + (channel % C->channels)];
	}
}

//---------------------------------------------------------------------
// called during cleanup

inline
void COMPOSITION_cleanup(BelaContext *context, struct COMPOSITION *C)
{

	// free memory
	if (C->loop)
	{
		std::free(C->loop);
		C->loop = nullptr;
	}

}

//---------------------------------------------------------------------

REBUS

//---------------------------------------------------------------------
