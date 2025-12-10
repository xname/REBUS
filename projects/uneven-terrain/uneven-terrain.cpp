//---------------------------------------------------------------------
/*

Uneven Terrain
by Claude Heiland-Allen 2023-06-19, 2023-06-28, 2023-07-11, 2023-09-26

Techno with waveshaping and filters.
Phase and magnitude control many things in a non-uniform way.

*/

//---------------------------------------------------------------------
// options

// #define RECORD 1
// #define CONTROL_LOP 1

//---------------------------------------------------------------------
// dependencies

#include <libraries/REBUS/REBUS.h>
#include <libraries/REBUS/dsp.h>

//---------------------------------------------------------------------
// added to audio recording filename

const char *COMPOSITION_name = "uneven-terrain";

//---------------------------------------------------------------------
// composition state

struct COMPOSITION
{
	// ramps from 0 to 1 every two beats
	PHASOR clock;

	// resonant filters for pitched bass
	BIQUAD bq[2];

	// delay lines (with buffers) for stereo comb filter
	DELAY del0; float del0buf[65536];
	DELAY del1; float del1buf[65536];

	// low-pass filters for smoothing control signals
	LOP lo[2];

	// sample-and-hold filters for making pitched noise for snare
	SAMPHOLD snare[2];

	// high-pass filters for hi-hat
	HIP hat[2];

	// high-pass (DC-blocking) filters for comb filter
	HIP fb[2];

	// high-pass (DC-blocking) filters for audio output
	HIP dc[2];
};

//---------------------------------------------------------------------
// called during setup

inline
bool COMPOSITION_setup(BelaContext *context, struct COMPOSITION *C)
{
	// check that the actual float rate matches the dsp.h float rate
	if (context->audioSampleRate != SR)
	{
		rt_printf("Sample rate mismatch: using %f, expected %f\n", (double) context->audioSampleRate, (double) SR);
		// return false; // it'll sound at a different pitch and speed, should this be a hard failure?
	}

	// clear everything to 0
	std::memset(C, 0, sizeof(*C));

	// resonant high pass filter (32 Hz, Q 12) for bass
	highpass(&C->bq[0], 32, 12);

	// resonant low pass filter (48 Hz, Q 100) for sub
	lowpass(&C->bq[1], 48, 100);

	// set the length of the delay lines
	// should match the size of the arrays
	// immediately following in the data structure
	C->del0.length = 65536;
	C->del1.length = 65536;

	return true;
}

//---------------------------------------------------------------------
// called once per audio frame (default 44100Hz float rate)

inline
void COMPOSITION_render(BelaContext *context, struct COMPOSITION *C, int n,
  float out[2], const float in[2], const float magnitude, const float phase)
{
	// get values from REBUS antenna
	float m = magnitude;
	float p = phase;

#if 1 // FIXME
	// smoothing to avoid zipper noise with non-REBUS controllers
	// reduces EMI noise (?) with REBUS controller
	m = lop(&C->lo[0], m, 10);
	p = lop(&C->lo[1], p, 10);
#endif

	// make mapping less uneven
	m *= 0.25;
	p *= 0.25;

	// clock for drum sounds
	// phasor repeats every two beats at about 133bpm
	float clock = phasor(&C->clock, 2.222 / 2);

	// make hi-hat envelope at 4 notes per beat
	// the repeated multiplies are more efficient than powf(hat, 16)
	float hat = 1 - wrap(8 * clock);
	hat *= hat;
	hat *= hat;
	hat *= hat;
	hat *= hat;
	// modulate hi-hat amplitude by magnitude
	hat *= clamp(sinf(7 * float(twopi) * m), 0, 1);
	// make hi-hats as high-pass filtered noise in stereo
	float hats[2] =
		{ hip(&C->hat[0], noise() * hat, 4000)
		, hip(&C->hat[1], noise() * hat, 4000)
		};

	// make snare envelope every 2 beats (on the off-beat)
	// the repeated multiplies are more efficient than powf(snare, 32)
	float snare = 1 - wrap(clock - 0.5);
	snare *= snare;
	snare *= snare;
	snare *= snare;
	snare *= snare;
	snare *= snare;
	// increase gain of attack
	snare *= snare + 1;
	// modulate snare amplitude by magnitude
	snare *= clamp(sinf(6 * float(twopi) * m), 0, 1);
    // make snares as sample-and-hold filtered noise in stereo
    // modulate sample-and-hold frequency by magnitude
	float snarePitch = mix(1000, 2000, 0.5 * (1.0 - cosf(6 * float(twopi) * m)));
	float snares[2] =
		{ samphold(&C->snare[0], noise(), wrap(snarePitch * clock - 0.25)) * snare
		, samphold(&C->snare[1], noise(), wrap(snarePitch * clock + 0.25)) * snare
		};

	// make kick envelope every beat
	float kick = 1 - wrap(2 * clock);
	// the repeated multiplies are more efficient than powf(kick, 32)
	kick *= kick;
	kick *= kick;
	kick *= kick;
	kick *= kick;
	kick *= kick;
	// turn into a sine wave with decaying frequency (chirp)
	kick = sinf(16 * float(pi) * kick);

	// bass is kick minus a resonant high pass filter of kick
	float bass = kick - biquad(&C->bq[0], kick);

	// sub is resonant low pass filter of kick
	float sub = biquad(&C->bq[1], kick);

	// comb filter
	// frequency of filter is modulated by both magnitude and phase
	float fbhz = mix(64, 512, 0.5f * (1 - cosf(5 * float(twopi) * (m + p))));
	// read from delay lines
	float fb0[2] =
		{ delread1(&C->del0, 1000 / fbhz)
		, delread1(&C->del1, 1000 / fbhz)
		};
	// high pass filter to avoid DC offset
	fb0[0] = hip(&C->fb[0], fb0[0], 100);
	fb0[1] = hip(&C->fb[1], fb0[1], 100);
	// rotate in the stereo field modulated by phase
	// this is like a matrix-vector multiplication
	float co = cosf(float(twopi) * p);
	float si = sinf(float(twopi) * p);
	float fb[2] =
		{ co * fb0[0] - si * fb0[1]
		, si * fb0[0] + co * fb0[1]
		};

	// waveshaping / mixing with DC blocking high-pass filters
	// the outside numbers (overall levels) are the same in both channels
	// the inside numbers (modulation coefficients) are different
	// this gives stereo effects
	out[0] = hip(&C->dc[0], 0.5f * sinf
	(	( 3 * sinf(7 * float(twopi) * p) * kick
		+ 2 * sinf( 5 * sinf(3 * float(twopi) * p) * bass
		          + float(pi) * sinf(3 * float(twopi) * m))
		+ 4 * sinf(5 * float(twopi) * m) * sub
		+ 6 * sinf(6 * float(twopi) * m) * fb[0]
		+ snares[0] + hats[0]
		) * 0.5f
	), 1);
	out[1] = hip(&C->dc[1], 0.5f * sinf
	(	( 3 * sinf(5 * float(twopi) * p) * kick
		+ 2 * sinf( 5 * sinf(4 * float(twopi) * p) * bass
		          + float(pi) * sinf(4 * float(twopi) * m))
		+ 4 * sinf(7 * float(twopi) * m) * sub
		+ 6 * sinf(8 * float(twopi) * m) * fb[1]
		+ snares[1] + hats[1]
		) * 0.5f
	), 1);

	// write to the delay lines for the comb filter
	delwrite(&C->del0, out[0] + snares[0]);
	delwrite(&C->del1, out[1] + snares[1]);
}

//---------------------------------------------------------------------
// called during cleanup

inline
void COMPOSITION_cleanup(BelaContext *context, struct COMPOSITION *C)
{
	// nothing to do for this composition
}

//---------------------------------------------------------------------

REBUS

//---------------------------------------------------------------------
