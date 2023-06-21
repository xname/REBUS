//---------------------------------------------------------------------
/*

Uneven Terrain
by Claude Heiland-Allen 2023-06-19

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
	sample clock = phasor(&s->clock, 2.222);
	sample kick = sin(16 * pi * pow(1 - clock, 32));
	sample bass = kick - biquad(highpass(&s->bq[0], 32, 12), kick);
	sample sub = biquad(lowpass(&s->bq[1], 48, 100), kick);
	sample fb0[2] =
	{ delread4(&s->del0, 1000 / 2.222 * 1 / 3)
	, delread4(&s->del1, 1000 / 2.222 * 1 / 3)
	};
	sample co = cos(twopi * in[1]);
	sample si = sin(twopi * in[1]);
	sample fb[2] = { co * fb0[0] - si * fb0[1], si * fb0[0] + co * fb0[1] };
	// waveshaping / mixing
	out[0] = sin
	(	( 3 * sin(7 * twopi * in[1]) * kick
		+ 2 * sin(5 * sin(3 * twopi * in[1]) * bass + pi * sin(3 * twopi * in[0]))
		+ 4 * sin(5 * twopi * in[0]) * sub
		+ 2 * sin(6 * twopi * in[0]) * fb[0]
		) * 0.5
	);
	out[1] = sin
	(	( 3 * sin(5 * twopi * in[1]) * kick
		+ 2 * sin(5 * sin(4 * twopi * in[1]) * bass + pi * sin(4 * twopi * in[0]))
		+ 4 * sin(7 * twopi * in[0]) * sub
		+ 2 * sin(8 * twopi * in[0]) * fb[1]
		) * 0.5
	);
	delwrite(&s->del0, out[0]);
	delwrite(&s->del1, out[1]);
}

//---------------------------------------------------------------------
// called during cleanup

static inline
void COMPOSITION_cleanup(BelaContext *context, struct COMPOSITION *C)
{
}

//---------------------------------------------------------------------
