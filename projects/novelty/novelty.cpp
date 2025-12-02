//---------------------------------------------------------------------
/*

Novelty
by Claude Heiland-Allen 2023-10-10, 2023-10-12, 2023-11-07, 2023-11-09

Audio granulator with gesture matching.

Training process associates gestures with sounds.
This happens in the first few seconds after launch.
Sound can be from live input or pre-recorded file.

Then gesture input plays the sound with best-matching gesture.

Recommended block size with default settings: 512 samples.


NOTE: if you set LIVEINPUT to 0 in the configuration below
you then need to add your own audio file called "input.wav"
and set INPUTDURATION to its length in seconds (fractions allowed)

*/

//---------------------------------------------------------------------
// options

//#define RECORD 1
//#define SCOPE 0
//#define CONTROL_LOP 1
//#define CONTROL_NOTCH 1

//---------------------------------------------------------------------
// dependencies

#include <libraries/REBUS/REBUS.h>
#include <libraries/REBUS/dsp.h>
#include <libraries/sndfile/sndfile.h>

//---------------------------------------------------------------------
// added to audio recording filename

const char *COMPOSITION_name = "novelty";

//---------------------------------------------------------------------
// configuration

// cost per sample is O(OVERLAP * COUNT / GRAINLENGTH)
// in bursts every GRAINLENGTH / OVERLAP samples
// so block size GRAINLENGTH / OVERLAP is appropriate

// channels must currently both be 2
#define CONTROLCHANNELS 2 // magnitude and phase
#define AUDIOCHANNELS 2 // stereo
#define LIVEINPUT 1 // set to 1 to use live input, otherwise input.wav
#define INPUTDURATION 10.0 // in seconds, should match input.wav if LIVEINPUT is not set
#define PASSTHRU 1 // pass input to output while recording (set to 0 with mic + speakers)
#define GRAINLENGTH 4096 // audio frames per grain
#define OVERLAP 8 // number of simultaneous playback grains
#define GAIN (2.0f / OVERLAP) // output volume level
#define COUNT ((int)(INPUTDURATION * 44100 * OVERLAP / GRAINLENGTH)) // size of database
#define SUBSAMPLING 128 // ratio of audio to control sample rate
#define CONTROLCUTOFF 100 // control data filter cutoff frequency
#define GESTURELENGTH (GRAINLENGTH / SUBSAMPLING) // points per gesture
#define JITTER 1 // set to 1 to enable pseudo-random jitter for variety

#define AUDIOFRAMES (COUNT * GRAINLENGTH / OVERLAP)
#define CONTROLFRAMES (COUNT * GRAINLENGTH / OVERLAP / SUBSAMPLING)

#define NONE (-1) // sentinel value for playbackOffset, for silence

//---------------------------------------------------------------------
// composition state

enum COMPOSITIONMODE { RECORDING = 0, PLAYING = 1 };

struct COMPOSITION
{
	// database
	float audio[AUDIOFRAMES][AUDIOCHANNELS];
	float control[CONTROLFRAMES][CONTROLCHANNELS];

	float audioWindow[GRAINLENGTH]; // raised cosine
	float controlWindow[GESTURELENGTH]; // raised cosine

	LOP controlFilter[CONTROLCHANNELS]; // for anti-aliasing
	int controlCounter; // [0..SUBSAMPLING), for decimation

	// control gesture is recorded here
	float gestureRingBuffer[GESTURELENGTH][CONTROLCHANNELS];
	int gestureOffset; // index of the seam in the circular buffer
	float gesture[GESTURELENGTH][CONTROLCHANNELS]; // windowed and unwrapped

	COMPOSITIONMODE mode;

	int recordingAudioFrame; // [0..AUDIOFRAMES)
	int recordingControlFrame; // [0..CONTROLFRAMES)

	int playbackFrame[OVERLAP]; // [0..GRAINLENGTH)
	int playbackOffset[OVERLAP]; // [-1..AUDIOFRAMES-GRAINLENGTH]
};

//---------------------------------------------------------------------
// called during setup

inline
bool
COMPOSITION_setup(BelaContext *context, struct COMPOSITION *C)
{

	// clear everything to 0
	std::memset(C, 0, sizeof(*C));

	// initialize raised cosine windows
	for (int i = 0; i < GRAINLENGTH; ++i)
	{
		C->audioWindow[i] = GAIN * (1 - cos(2 * M_PI * (i + 0.5) / GRAINLENGTH)) / 2;
	}
	for (int i = 0; i < GESTURELENGTH; ++i)
	{
		// gain is not important here
		C->controlWindow[i] = 1 - cos(2 * M_PI * (i + 0.5) / GESTURELENGTH);
	}

	// initialize database
	for (int i = 0; i < OVERLAP; ++i)
	{
		C->playbackFrame[i] = i * GRAINLENGTH / OVERLAP;
		C->playbackOffset[i] = NONE;
	}

	if (! LIVEINPUT)
	{
		// read audio file
		SF_INFO info = {0};
		SNDFILE *in = sf_open("input.wav", SFM_READ, &info);
		if (! in)
		{
			rt_printf("error: could not open 'input.wav'\n");
			return false;
		}
		if (info.channels != AUDIOCHANNELS)
		{
			rt_printf("error: 'input.wav' has %d != %d audio channels\n", info.channels, AUDIOCHANNELS);
			return false;
		}
		sf_readf_float(in, &C->audio[0][0], AUDIOFRAMES);
		sf_close(in);
	}

	// all ok
	return true;
}

//---------------------------------------------------------------------
// called once per audio frame (default 44100Hz float rate)

inline
void
COMPOSITION_render(BelaContext *context, struct COMPOSITION *C, int n,
	float out[2], const float in[2], const float magnitude, const float phase)
{

	// filter control signals
	float control[2] =
		{ lop(&C->controlFilter[0], magnitude, CONTROLCUTOFF)
		, lop(&C->controlFilter[1], phase, CONTROLCUTOFF)
		};
	// decimate control signals
	if (++(C->controlCounter) >= SUBSAMPLING)
	{
		C->controlCounter = 0;
		// record control signals
		for (int c = 0; c < CONTROLCHANNELS; ++c)
		{
			C->gestureRingBuffer[C->gestureOffset][c] = control[c];
		}
		// wrap around buffer
		if (++(C->gestureOffset) >= GESTURELENGTH)
		{
			C->gestureOffset = 0;
		}
	}

	// fill buffers with incoming audio and control signals
	if (C->mode == RECORDING)
	{
		// pass through input to output
		if (PASSTHRU)
		{
			for (int c = 0; c < AUDIOCHANNELS; ++c)
			{
				if (LIVEINPUT)
				{
					out[c] = in[c];
				}
				else
				{
					out[c] = C->audio[C->recordingAudioFrame][c];
				}
			}
		}
		// report progress as countdown from 10 to 0
		int new_perdecage = C->recordingAudioFrame * 10 / AUDIOFRAMES;
		int old_perdecage = (C->recordingAudioFrame - 1) * 10 / AUDIOFRAMES;
		if (new_perdecage != old_perdecage)
		{
			rt_printf("%d\n", 10 - new_perdecage);
		}

		// record control signals
		if (C->controlCounter == 0)
		{
			for (int c = 0; c < CONTROLCHANNELS; ++c)
			{
				C->control[C->recordingControlFrame][c] = control[c];
			}
			// check for full buffer
			if (++(C->recordingControlFrame) >= CONTROLFRAMES)
			{
				C->mode = PLAYING;
			}
		}

		// record audio
		if (LIVEINPUT)
		{
			for (int c = 0; c < AUDIOCHANNELS; ++c)
			{
				C->audio[C->recordingAudioFrame][c] = in[c];
			}
		}
		if (++(C->recordingAudioFrame) >= AUDIOFRAMES)
		{
			C->mode = PLAYING;
		}
	}

	// play back
	else
	{
		// overlap-add grains
		float output[2] = { 0, 0 };
		for (int o = 0; o < OVERLAP; ++o)
		{
			if (C->playbackOffset[o] != NONE)
			{
				float gain = C->audioWindow[C->playbackFrame[o]];
				int playbackIndex = C->playbackOffset[o] + C->playbackFrame[o];
				for (int c = 0; c < AUDIOCHANNELS; ++c)
				{
					output[c] += gain * C->audio[playbackIndex][c];
				}
			}
		}
		out[0] = output[0];
		out[1] = output[1];

		// advance pointers and look up next matches if necessary
		for (int o = 0; o < OVERLAP; ++o)
		{
			// this will be true for only one overlap at a time,
			// so inside here is not duplicated work
			if (++(C->playbackFrame[o]) >= GRAINLENGTH)
			{
				C->playbackFrame[o] = 0;
				// unwrap the recorded gesture
				for (int i = 0; i < GESTURELENGTH; ++i)
				{
					for (int c = 0; c < CONTROLCHANNELS; ++c)
					{
						C->gesture[i][c] = C->gestureRingBuffer[(C->gestureOffset + i) % GESTURELENGTH][c];
					}
				}

				// find the best match
				int bestOffset = NONE;
				float bestDistance = 1.0 / 0.0;
				for (int i = 0; i < COUNT; ++i)
				{
					// add pseudo-random jitter to increase variety
					int jitter = JITTER ? rand() % GRAINLENGTH : 0;
					int audioOffset = (i * GRAINLENGTH + jitter) / OVERLAP;
					if (audioOffset + GRAINLENGTH > AUDIOFRAMES) continue;
					int gestureOffset = (i * GESTURELENGTH + jitter / SUBSAMPLING) / OVERLAP;
					if (gestureOffset + GESTURELENGTH > CONTROLFRAMES) continue;
					// compute a goodness-of-fit metric (lower is better)
					// currently weights all control channels equally
					float distance = 0;
					for (int g = 0; g < GESTURELENGTH; ++g)
					{
						float window = C->controlWindow[g];
						for (int c = 0; c < CONTROLCHANNELS; ++c)
						{
							float delta = C->gesture[g][c] - C->control[gestureOffset + g][c];
							distance += window * delta * delta;
						}
					}
					if (distance < bestDistance)
					{
						bestDistance = distance;
						bestOffset = audioOffset;
					}
				}
				C->playbackOffset[o] = bestOffset;
			}
		}
	}
}

//---------------------------------------------------------------------
// called during cleanup

inline
void COMPOSITION_cleanup(BelaContext *context, struct COMPOSITION *C)
{
	// nothing to do
}

//---------------------------------------------------------------------

REBUS

//---------------------------------------------------------------------
