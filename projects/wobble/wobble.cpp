//---------------------------------------------------------------------
/*

Wobble
by Claude Heiland-Allen 2023-10-03

Wobble synthesizer with gesture control.

Stationary controls mean gesture end.
Non-stationary controls mean gesture start.
Each gesture is looped with quantized duration.
Successive gestures control different synthesis parameters
(harmonics of a phase modulation synthesizer).

TODO: figure out how to get rid of clicks.

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

const char *COMPOSITION_name = "wobble";

//---------------------------------------------------------------------
// composition state

#define SAMPLES_PER_QUARTER_BEAT 5000
#define MAXIMUM_LOOP_QUARTER_BEATS 32
#define MAXIMUM_LOOP_SAMPLES (MAXIMUM_LOOP_QUARTER_BEATS * SAMPLES_PER_QUARTER_BEAT)
#define NUMBER_OF_LOOPS 4

#define SAMPLES_PER_CYCLE 800

#define STILLNESS_THRESHOLD 0.00001f
#define STILLNESS_HYSTERESIS 10000.0f

struct COMPOSITION
{

	// audio loop data
	float loop[NUMBER_OF_LOOPS][MAXIMUM_LOOP_SAMPLES][2];
	int loop_length[NUMBER_OF_LOOPS]; // in (1..MAXIMUM_LOOP_SAMPLES]
	int playback_index[NUMBER_OF_LOOPS]; // in [0..MAXIMUM_LOOP_SAMPLES)
	bool recording_in_progress;
	int recording_loop_number; // in [0..NUMBER_OF_LOOPS)
	int recording_index; //  in [0..MAXIMUM_LOOP_SAMPLES)

	// gesture recognition
	LOP lowpass_filter[6];

	// synthesis
	// counts samples modulo SAMPLES_PER_CYCLE
	int clock;

};

//---------------------------------------------------------------------
// called during setup

inline
bool COMPOSITION_setup(BelaContext *context, struct COMPOSITION *C)
{

	// clear everything to 0
	std::memset(C, 0, sizeof(*C));

	// set loop lengths to minimum, to avoid modulo/division by 0
	for (int i = 0; i < NUMBER_OF_LOOPS; ++i)
	{
		C->loop_length[i] = 1;
	}

	// calculate audio parameters
	double bpm = (context->audioSampleRate * 60.0) / (4.0 * SAMPLES_PER_QUARTER_BEAT);
	double hz = context->audioSampleRate / (double) SAMPLES_PER_CYCLE;
	rt_printf("BPM = %f, Hz = %f\n", bpm, hz);

	// all ok
	return true;
}

//---------------------------------------------------------------------
// called once per audio frame (default 44100Hz float rate)

inline
void COMPOSITION_render(BelaContext *context, struct COMPOSITION *C,
	float out[2], const float in[2], const float magnitude, const float phase)
{

	// compute gesture metrics
	// the controls are still when
	// the average value over the last 100ms (10 Hz low pass)
	// is close to
	// the average value over the last 1000ms (1 Hz low pass)
	float m10 = lop(&C->lowpass_filter[0], magnitude, 100);
	float p10 = lop(&C->lowpass_filter[1], phase, 100);
	float m100 = lop(&C->lowpass_filter[2], magnitude, 10);
	float p100 = lop(&C->lowpass_filter[3], phase, 10);
	float m1000 = lop(&C->lowpass_filter[4], magnitude, 1);
	float p1000 = lop(&C->lowpass_filter[5], phase, 1);
	float dmagnitude100 = m100 - m1000;
	float dphase100 = p100 - p1000;
	float stillness = dmagnitude100 * dmagnitude100 + dphase100 * dphase100;
	bool still = stillness < STILLNESS_THRESHOLD;
	bool nonstill = STILLNESS_THRESHOLD * STILLNESS_HYSTERESIS < stillness;
	// difference of average over last 10ms (100 Hz low pass) and 100ms (10 Hz low pass)
	// recorded into the loop and played back to control synth
	float dmagnitude10 = m10 - m100;
	float dphase10 = p10 - p100;

	// check for recording start / stop
	if (! C->recording_in_progress)
	{
		if (! nonstill)
		{
			// continue not recording
		}
		else
		{
			// start recording
			C->recording_in_progress = true;
			rt_printf("Recording loop %d\n", C->recording_loop_number);
		}
	}
	if (C->recording_in_progress)
	{
		if (still || C->recording_index >= MAXIMUM_LOOP_SAMPLES)
		{
			// stop recording
			C->recording_in_progress = false;
			// quantize length, rounded down
			int quarter_beats = C->recording_index / SAMPLES_PER_QUARTER_BEAT;
			rt_printf("Recorded loop %d with %d quarter beats\n", C->recording_loop_number, quarter_beats);
			C->loop_length[C->recording_loop_number] = quarter_beats * SAMPLES_PER_QUARTER_BEAT;
			// guard against modulo/division by 0
			if (C->loop_length[C->recording_loop_number] == 0)
			{
				C->loop_length[C->recording_loop_number] = 1;
			}
			C->playback_index[C->recording_loop_number] %= C->loop_length[C->recording_loop_number]; // prevents crash, but clicks
			// prepare for next recording
			++C->recording_loop_number;
			C->recording_loop_number %= NUMBER_OF_LOOPS;
			C->recording_index = 0;
		}
		else
		{
			// continue recording
			C->loop[C->recording_loop_number][C->recording_index][0] = dmagnitude10;
			C->loop[C->recording_loop_number][C->recording_index][1] = dphase10;
			++C->recording_index;
		}
	}

	// read parameters from loop arrays
	// play forward and backward simultaneously to avoid clicks at loop boundaries
	float m0 = C->loop[0][C->playback_index[0]][0] + C->loop[0][C->loop_length[0]-1 - C->playback_index[0]][0];
	float p0 = C->loop[0][C->playback_index[0]][1] + C->loop[0][C->loop_length[0]-1 - C->playback_index[0]][1];
	float m1 = C->loop[1][C->playback_index[1]][0] + C->loop[1][C->loop_length[1]-1 - C->playback_index[1]][0];
	float p1 = C->loop[1][C->playback_index[1]][1] + C->loop[1][C->loop_length[1]-1 - C->playback_index[1]][1];
	float m2 = C->loop[2][C->playback_index[2]][0] + C->loop[2][C->loop_length[2]-1 - C->playback_index[2]][0];
	float p2 = C->loop[2][C->playback_index[2]][1] + C->loop[2][C->loop_length[2]-1 - C->playback_index[2]][1];
	float m3 = C->loop[3][C->playback_index[3]][0] + C->loop[3][C->loop_length[3]-1 - C->playback_index[3]][0];
	float p3 = C->loop[3][C->playback_index[3]][1] + C->loop[3][C->loop_length[3]-1 - C->playback_index[3]][1];

	// advance playback
	for (int i = 0; i < NUMBER_OF_LOOPS; ++i)
	{
		++C->playback_index[i];
		C->playback_index[i] %= C->loop_length[i];
	}

	// advance loop
	float t = C->clock++ / (float) SAMPLES_PER_CYCLE;
	C->clock %= SAMPLES_PER_CYCLE;
	
	// phase modulation synthesis
	t *= (float) (2 * M_PI);
	p0 *= (float) (2 * M_PI);
	p1 *= (float) (2 * M_PI);
	p2 *= (float) (2 * M_PI);
	p3 *= (float) (2 * M_PI);
	float p
		= t
		+ (float) (M_PI) * (
		+ m0 * sinf(4 * t + p0)
		+ m1 * sinf(5 * t + p1)
		+ m2 * sinf(6 * t + p2)
		+ m3 * sinf(7 * t + p3)
		);
	float q
		= t
		+ (float) (M_PI) * (
		+ m0 * sinf(4 * t - p0)
		+ m1 * sinf(5 * t - p1)
		+ m2 * sinf(6 * t - p2)
		+ m3 * sinf(7 * t - p3)
		);
	out[0] = sinf(p);
	out[1] = sinf(q);
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
