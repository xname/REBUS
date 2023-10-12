//---------------------------------------------------------------------
/*

Novelty
by Claude Heiland-Allen 2023-10-10, 2023-10-12

Audio granulator with gesture database.

Training process associates gestures with sounds.
This happens in the first ~20 seconds after launch.

Then gesture input plays the sound with best-matching gesture.

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

//---------------------------------------------------------------------
// added to audio recording filename

const char *COMPOSITION_name = "novelty";

//---------------------------------------------------------------------
// composition state

#define GESTURE_LENGTH 8

struct Gesture
{
	float x[2 * GESTURE_LENGTH];
};

static
inline
float
distance(const Gesture &a, const Gesture &b)
{
	float sum = 0;
	for (int i = 0; i < 2 * GESTURE_LENGTH; ++i)
	{
		float delta = a.x[i] - b.x[i];
		sum += delta * delta;
	}
	return sum;
}

#define CHANNELS 2
#define COUNT 2048 // size of database
#define LENGTH 4096 // audio frames per grain
#define OVERLAP 8 // number of simultaneous grains
#define GAIN (2.0f / OVERLAP) // output volume level
#define NONE (-1) // sentinel index meaning take no action

// cost per sample is O(OVERLAP * COUNT / LENGTH)
// in bursts every LENGTH / OVERLAP samples
// so block size LENGTH / OVERLAP is appropriate

struct COMPOSITION
{
	float audio[COUNT][LENGTH][CHANNELS]; // 32 MiB with settings above
	Gesture gestures[COUNT];
	float window[LENGTH]; // raised cosine
	int count; // [0..COUNT] current size of database
	int frame[OVERLAP]; // [0..LENGTH)
	int best[OVERLAP]; // [0..count)
	int worst[OVERLAP]; // [0..count)
	int gesture_frame[OVERLAP]; // [0..LENGTH / GESTURE_LENGTH)
	Gesture input_gesture[OVERLAP];
	float input_audio[OVERLAP][LENGTH][CHANNELS];
	LOP lowpass_filter[2];
};

//---------------------------------------------------------------------
// called during setup

inline
bool
COMPOSITION_setup(BelaContext *context, struct COMPOSITION *C)
{

	// clear everything to 0
	std::memset(C, 0, sizeof(*C));

	// initialize raised cosine window
	for (int i = 0; i < LENGTH; ++i)
	{
		C->window[i] = GAIN * (1 - cos(2 * M_PI * (i + 0.5) / LENGTH)) / 2;
	}

	// initialize database
	for (int i = 0; i < OVERLAP; ++i)
	{
		C->frame[i] = LENGTH / OVERLAP * i;
		C->best[i] = NONE;
		C->worst[i] = NONE;
		C->gesture_frame[i] = LENGTH / OVERLAP / GESTURE_LENGTH * i;
	}

	// all ok
	return true;
}

//---------------------------------------------------------------------
// called once per audio frame (default 44100Hz float rate)

inline
void
COMPOSITION_render(BelaContext *context, struct COMPOSITION *C,
	float out[2], const float in[2], const float magnitude, const float phase)
{
	// record audio
	for (int i = 0; i < OVERLAP; ++i)
	{
		if (C->worst[i] != NONE)
		{
			for (int c = 0; c < 2; ++c)
			{
				C->audio[C->worst[i]][C->frame[i]][c] = C->window[C->frame[i]] * in[c];
			}
		}
	}

	// record gesture
	float m = lop(&C->lowpass_filter[0], magnitude, 100);
	float p = lop(&C->lowpass_filter[1], phase, 100);
	for (int i = 0; i < OVERLAP; ++i)
	{
		if (++C->gesture_frame[i] == LENGTH / GESTURE_LENGTH)
		{
			C->gesture_frame[i] = 0;
			int j = C->frame[i] * GESTURE_LENGTH / LENGTH;
			float w = C->window[C->frame[i]];
			C->input_gesture[i].x[2 * j + 0] = w * m;
			C->input_gesture[i].x[2 * j + 1] = w * p;
		}
	}

	// playback audio
	if (C->count < COUNT)
	{
		// pass through during training
		out[0] = in[0];
		out[1] = in[1];
	}
	else
	{
		// play best matches
		out[0] = 0;
		out[1] = 0;
		for (int i = 0; i < OVERLAP; ++i)
		{
			if (C->best[i] != NONE)
			{
				for (int c = 0; c < 2; ++c)
				{
					out[c] += C->audio[C->best[i]][C->frame[i]][c];
				}
			}
		}
	}
	
	// advance audio
	for (int i = 0; i < OVERLAP; ++i)
	{
		if (++C->frame[i] == LENGTH)
		{
			C->frame[i] = 0;
			// look up database
			float best_distance = 1.0f/0.0f;
			float worst_distance = -1.0f/0.0f;
			int best_index = NONE;
			int worst_index = NONE;
			for (int j = 0; j < C->count; ++j)
			{
				float d = distance(C->input_gesture[i], C->gestures[j]);
				if (d < best_distance)
				{
					best_distance = d;
					best_index = j;
				}
				if (d > worst_distance)
				{
					worst_distance = d;
					worst_index = j;
				}
			}
			if (C->count < COUNT)
			{
				// fill database before overwriting
				worst_index = C->count++;
			}
			else
			{
				// don't overwrite
				worst_index = NONE;
			}
			C->best[i] = best_index;
			C->worst[i] = worst_index;
			if (worst_index != NONE)
			{
				rt_printf("%d%%\n", 100 * worst_index / COUNT);
				C->gestures[worst_index] = C->input_gesture[i];
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
