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

#define RECORD 1
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

// state machine
enum state
{
	sPlay,
	sRecord,
	sStop,
	sMix
};

#define SAMPLES_PER_QUARTER_BEAT 5000
#define FADE_SAMPLES SAMPLES_PER_QUARTER_BEAT
#define MAXIMUM_LOOP_QUARTER_BEATS 32
#define MINIMUM_LOOP_QUARTER_BEATS 1
#define MAXIMUM_LOOP_SAMPLES (MAXIMUM_LOOP_QUARTER_BEATS * SAMPLES_PER_QUARTER_BEAT)
#define MINIMUM_LOOP_SAMPLES (MINIMUM_LOOP_QUARTER_BEATS * SAMPLES_PER_QUARTER_BEAT)
#define NUMBER_OF_LOOPS 2

#define SAMPLES_PER_CYCLE 800

#define STILLNESS_THRESHOLD 0.0001f
#define STILLNESS_HYSTERESIS 100.0f

struct COMPOSITION
{
	// audio loop data
	float loop[NUMBER_OF_LOOPS][MAXIMUM_LOOP_SAMPLES + FADE_SAMPLES][2];
	int loopLength[NUMBER_OF_LOOPS]; // in (1..MAXIMUM_LOOP_SAMPLES]
	int playbackIndex[NUMBER_OF_LOOPS]; // in [0..MAXIMUM_LOOP_SAMPLES)

	// state machine data
	enum state state;
	int stopIndex; // in [0..FADE_SAMPLES), only used in state sStop 
	int mixIndex; // in [0..FADE_SAMPLES), only used in state sMix
	int recordingLoopNumber; // in [0..NUMBER_OF_LOOPS)
	int recordingIndex; //  in [0..MAXIMUM_LOOP_SAMPLES)

	// gesture recognition
	LOP lowpassFilter[6];

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

	C->state = sPlay;
	// set loop lengths to minimum, to avoid modulo/division by 0
	for (int i = 0; i < NUMBER_OF_LOOPS; ++i)
	{
		C->loopLength[i] = 1;
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
void COMPOSITION_render(BelaContext *context, struct COMPOSITION *C, int n,
	float out[2], const float in[2], const float magnitude, const float phase)
{

	// compute gesture metrics
	// the controls are still when
	// the average value over the last 100ms (10 Hz low pass)
	// is close to
	// the average value over the last 1000ms (1 Hz low pass)
	float m10 = lop(&C->lowpassFilter[0], magnitude, 100);
	float p10 = lop(&C->lowpassFilter[1], phase, 100);
	float m100 = lop(&C->lowpassFilter[2], magnitude, 10);
	float p100 = lop(&C->lowpassFilter[3], phase, 10);
	float m1000 = lop(&C->lowpassFilter[4], magnitude, 1);
	float p1000 = lop(&C->lowpassFilter[5], phase, 1);
	float dmagnitude100 = m100 - m1000;
	float dphase100 = p100 - p1000;
	float stillness = dmagnitude100 * dmagnitude100 + dphase100 * dphase100;
	bool still = stillness < STILLNESS_THRESHOLD;
	bool nonStill = STILLNESS_THRESHOLD * STILLNESS_HYSTERESIS < stillness;
	// difference of average over last 10ms (100 Hz low pass) and 100ms (10 Hz low pass)
	// recorded into the loop and played back to control synth
	float dmagnitude10 = m10 - m100;
	float dphase10 = p10 - p100;

	// check for recording start / stop
	float crossFade = 0;
	switch (C->state)
	{
		case sPlay:
		{
			if (! nonStill)
			{
				// continue not recording
				break;
			}
			else
			{
				// significant motion
				// start recording
				C->state = sRecord;
				rt_printf("Recording loop %d\n", C->recordingLoopNumber);
			}
		}
		// fall-through
		case sRecord:
		{
			if ((still || C->recordingIndex >= MAXIMUM_LOOP_SAMPLES) && ! (C->recordingIndex < MINIMUM_LOOP_SAMPLES))
			{
				// start stopping recording
				C->state = sStop;
				// quantize length, rounded down
				int quarterBeats = C->recordingIndex / SAMPLES_PER_QUARTER_BEAT;
				rt_printf("Recorded loop %d with %d quarter beats\n", C->recordingLoopNumber, quarterBeats);
				C->loopLength[C->recordingLoopNumber] = quarterBeats * SAMPLES_PER_QUARTER_BEAT;
				// guard against modulo/division by 0
				if (C->loopLength[C->recordingLoopNumber] == 0)
				{
					C->loopLength[C->recordingLoopNumber] = 1;
				}
				C->stopIndex = C->loopLength[C->recordingLoopNumber] + FADE_SAMPLES;
			}
			else
			{
				// continue recording
				C->loop[C->recordingLoopNumber][C->recordingIndex][0] = magnitude;
				C->loop[C->recordingLoopNumber][C->recordingIndex][1] = phase;
				++C->recordingIndex;
				break;
			}
		}
		// fall-through
		case sStop:
		{
			if (C->recordingIndex < C->stopIndex)
			{
				// continue recording
				C->loop[C->recordingLoopNumber][C->recordingIndex][0] = magnitude;
				C->loop[C->recordingLoopNumber][C->recordingIndex][1] = phase;
				++C->recordingIndex;
				break;
			}
			else
			{
				C->mixIndex = 0;
				C->playbackIndex[C->recordingLoopNumber] = 0;
				C->state = sMix;
			}
		}
		// fall-through
		case sMix:
		{
			if (C->mixIndex < FADE_SAMPLES)
			{
				crossFade = C->mixIndex++ / (float) FADE_SAMPLES;
				break;
			}
			else
			{
				crossFade = 0;
				C->state = sPlay;
				C->recordingLoopNumber += 1;
				C->recordingLoopNumber %= NUMBER_OF_LOOPS;
				C->recordingIndex = 0;
			}
		}
	}

	// read parameters from loop arrays
	int playingLoopNumber = (C->recordingLoopNumber + 1) % NUMBER_OF_LOOPS;

	float m0 = C->loop[playingLoopNumber][C->playbackIndex[playingLoopNumber]][0];
	float p0 = C->loop[playingLoopNumber][C->playbackIndex[playingLoopNumber]][1];
	if (C->playbackIndex[playingLoopNumber] < FADE_SAMPLES)
	{
		// cross-fade end of loop
		float t = C->playbackIndex[playingLoopNumber] / (float) FADE_SAMPLES;
		int index = C->loopLength[playingLoopNumber] + C->playbackIndex[playingLoopNumber];
		m0 = mix(C->loop[playingLoopNumber][index][0], m0, t);
		p0 = mix(C->loop[playingLoopNumber][index][1], p0, t);
	}

	float m1 = C->loop[!playingLoopNumber][C->playbackIndex[!playingLoopNumber]][0];
	float p1 = C->loop[!playingLoopNumber][C->playbackIndex[!playingLoopNumber]][1];
	if (C->playbackIndex[!playingLoopNumber] < FADE_SAMPLES)
	{
		// cross-fade end of loop
		float t = C->playbackIndex[!playingLoopNumber] / (float) FADE_SAMPLES;
		int index = C->loopLength[!playingLoopNumber] + C->playbackIndex[!playingLoopNumber];
		m1 = mix(C->loop[!playingLoopNumber][index][0], m1, t);
		p1 = mix(C->loop[!playingLoopNumber][index][1], p1, t);
	}

	// mix with just-recorded loop
	m0 = mix(m0, m1, crossFade);
	p1 = mix(p0, p1, crossFade);

	// fix up legacy parameters
	m1 = 0;//C->loop[1][C->playbackIndex[1]][0] + C->loop[1][C->loopLength[1]-1 - C->playbackIndex[1]][0];
	p1 = 0;//C->loop[1][C->playbackIndex[1]][1] + C->loop[1][C->loopLength[1]-1 - C->playbackIndex[1]][1];
	float m2 = 0;//C->loop[2][C->playbackIndex[2]][0] + C->loop[2][C->loopLength[2]-1 - C->playbackIndex[2]][0];
	float p2 = 0;//C->loop[2][C->playbackIndex[2]][1] + C->loop[2][C->loopLength[2]-1 - C->playbackIndex[2]][1];
	float m3 = 0;//C->loop[3][C->playbackIndex[3]][0] + C->loop[3][C->loopLength[3]-1 - C->playbackIndex[3]][0];
	float p3 = 0;//C->loop[3][C->playbackIndex[3]][1] + C->loop[3][C->loopLength[3]-1 - C->playbackIndex[3]][1];

	// advance playback
	for (int i = 0; i < NUMBER_OF_LOOPS; ++i)
	{
		++C->playbackIndex[i];
		C->playbackIndex[i] %= C->loopLength[i];
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
