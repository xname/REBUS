//---------------------------------------------------------------------
/*

Uneven Terrain
by Claude Heiland-Allen 2023-06-19, 2023-06-28

Techno with waveshaping and filters.
Phase and magnitude control many things in a non-uniform way.

*/

//---------------------------------------------------------------------
// abstracted composition core
// this file is included by render.cpp

//---------------------------------------------------------------------
// added to audio recording filename

const char *COMPOSITION_name = "uneven-terrain";

//---------------------------------------------------------------------
// dependencies

#include "_dsp.h"

//---------------------------------------------------------------------
// composition state

struct COMPOSITION
{
	PHASOR clock;
	BIQUAD bq[2];
	DELAY del0; float del0buf[65536];
	DELAY del1; float del1buf[65536];
	LOP lo[2];
	SAMPHOLD snare[2];
	HIP hat[2];
	HIP fb[2];
};

//---------------------------------------------------------------------
// called during setup

static inline
bool COMPOSITION_setup(BelaContext *context, struct COMPOSITION *C)
{
	if (context->audioSampleRate != SR)
	{
		rt_printf("warning: using %f, expected %f\n", (double) context->audioSampleRate, (double) SR);
	}
	std::memset(C, 0, sizeof(*C));
	highpass(&C->bq[0], 32, 12);
	lowpass(&C->bq[1], 48, 100);
	C->del0.length = 65536;
	C->del1.length = 65536;
	return true;
}

//---------------------------------------------------------------------
// called once per audio frame (default 44100Hz sample rate)

static inline
void COMPOSITION_render(BelaContext *context, struct COMPOSITION *s, float out[2], const float in_audio[2], const float magnitude, const float phase)
{
	using std::sin;
	using std::cos;
	using std::pow;
	// smoothing to avoid zipper noise with non-REBUS controllers
	// reduces EMI noise (?) with REBUS controller
	float in[2] = { magnitude, phase };
	in[0] = lop(&s->lo[0], in[0], 10);
	in[1] = lop(&s->lo[1], in[1], 10);
	// drum sounds
	sample clock = phasor(&s->clock, 2.222 / 2);
	sample hat = 1 - wrap(8 * clock);
	hat *= hat;
	hat *= hat;
	hat *= hat;
	hat *= hat;
	hat *= clamp(sinf(7 * float(twopi) * in[0]), 0, 1);
	sample hats[2] =
		{ hip(&s->hat[0], noise() * hat, 4000)
		, hip(&s->hat[1], noise() * hat, 4000)
		};
	sample snare = 1 - wrap(clock - 0.5);
	snare *= snare;
	snare *= snare;
	snare *= snare;
	snare *= snare;
	snare *= snare;
	snare *= snare + 1;
	snare *= clamp(sinf(6 * float(twopi) * in[0]), 0, 1);
	sample snhz = mix(1000, 2000, 0.5 * (1.0 - cosf(6 * float(twopi) * in[0])));
	sample snares[2] =
		{ samphold(&s->snare[0], noise(), wrap(snhz * clock - 0.25)) * snare
		, samphold(&s->snare[1], noise(), wrap(snhz * clock + 0.25)) * snare
		};
	sample kick = 1 - wrap(2 * clock);
	kick *= kick;
	kick *= kick;
	kick *= kick;
	kick *= kick;
	kick *= kick;
	kick = sinf(16 * float(pi) * kick);
	sample bass = kick - biquad(&s->bq[0], kick);
	sample sub = biquad(&s->bq[1], kick);
	sample fbhz = mix(64, 512, 0.5f * (1 - cosf(5 * float(twopi) * (in[0] + in[1]))));
	sample fb0[2] =
	{ delread1(&s->del0, 1000 / fbhz)
	, delread1(&s->del1, 1000 / fbhz)
	};
	fb0[0] = hip(&s->fb[0], fb0[0], 100);
	fb0[1] = hip(&s->fb[1], fb0[1], 100);
	sample co = cosf(float(twopi) * in[1]);
	sample si = sinf(float(twopi) * in[1]);
	sample fb[2] = { co * fb0[0] - si * fb0[1], si * fb0[0] + co * fb0[1] };
	// waveshaping / mixing
	out[0] = sinf
	(	( 3 * sinf(7 * float(twopi) * in[1]) * kick
		+ 2 * sinf(5 * sinf(3 * float(twopi) * in[1]) * bass + float(pi) * sinf(3 * float(twopi) * in[0]))
		+ 4 * sinf(5 * float(twopi) * in[0]) * sub
		+ 3 * sinf(6 * float(twopi) * in[0]) * fb[0]
		+ snares[0] + hats[0]
		) * 0.5f
	);
	out[1] = sin
	(	( 3 * sinf(5 * float(twopi) * in[1]) * kick
		+ 2 * sinf(5 * sinf(4 * float(twopi) * in[1]) * bass + float(pi) * sinf(4 * float(twopi) * in[0]))
		+ 4 * sinf(7 * float(twopi) * in[0]) * sub
		+ 3 * sinf(8 * float(twopi) * in[0]) * fb[1]
		+ snares[1] + hats[1]
		) * 0.5f
	);
	delwrite(&s->del0, out[0] + snares[0]);
	delwrite(&s->del1, out[1] + snares[1]);
}

//---------------------------------------------------------------------
// called during cleanup

static inline
void COMPOSITION_cleanup(BelaContext *context, struct COMPOSITION *C)
{
}

//---------------------------------------------------------------------
