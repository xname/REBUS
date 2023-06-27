//---------------------------------------------------------------------
/*

Dub Terrain
by Claude Heiland-Allen 2023-06-21, 2023-06-27

Dub delays with waveshaping and filters.
Phase and magnitude control many things in a non-uniform way.

Note: requires a large block size to avoid underruns.
Tested and seems working with block size 512.

*/

//---------------------------------------------------------------------
// abstracted composition core
// this file is included by render.cpp

//---------------------------------------------------------------------
// added to audio recording filename

const char *COMPOSITION_name = "dub-terrain";

//---------------------------------------------------------------------
// dependencies

#include "_dsp.h"

//---------------------------------------------------------------------
// composition state

struct DLINE
{
	DELAY del;
	float buf[1 << 17];
};

struct COMPOSITION
{
	PHASOR clock;
	BIQUAD bq[2];
	DLINE del[2];
	LOPF lo[2];
	VCFF bp[4][2];
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
	highpass(&C->bq[0], 32, 20);
	lowpass(&C->bq[1], 64, 50);
	C->del[0].del.length = 1 << 17;
	C->del[1].del.length = 1 << 17;
	return true;
}

//---------------------------------------------------------------------
// called once per audio frame (default 44100Hz sample rate)

static inline
void COMPOSITION_render(BelaContext *context, struct COMPOSITION *s, float out[2], const float in_audio[2], const float magnitude, const float phase)
{
	static const int prime[4] = { 2, 3, 5, 7 };
	using std::sin;
	using std::cos;
	using std::pow;
	// smoothing to avoid zipper noise with non-REBUS controllers
	// reduces EMI noise (?) with REBUS controller
	float in[2] = { magnitude, phase };
	in[0] = lopf(&s->lo[0], in[0], 10);
	in[1] = lopf(&s->lo[1], in[1], 10);
	float m = clamp(in[0], 0, 1);
	float p = clamp(in[1], 0, 1) * float(twopi);
	// drum sounds
	sample tempo = 0.4;
	sample clock = phasor(&s->clock, tempo);
	sample kick = sinf(32 * float(pi) * pow(1 - clock, 64));
	sample bass = kick - biquad(&s->bq[0], kick);
	sample sub = biquad(&s->bq[1], kick);
	sample fb[4][2];
	sample cop[4];
	sample sip[4];
	for (int i = 0; i < 4; ++i)
	{
		for (int j = 0; j < 2; ++j)
		{
			fb[i][j] = delread1(&s->del[j].del, 1000 / tempo * 1 / (i + 1.5));
		}
		cop[i] = cosf(prime[i] * p);
		sip[i] = sinf(prime[i] * p);
		sample tmp[2] = { cop[i] * fb[i][0] - sip[i] * fb[i][1], sip[i] * fb[i][0] + cop[i] * fb[i][1] };
		fb[i][0] = tmp[0];
		fb[i][1] = tmp[1];
	}
	// waveshaping / mixing
	sample g = 0.5 * sinf(float(pi) * m);
	sample q = 3;
	sample gm[4], gp[4];
	m *= float(twopi);
	for (int i = 0; i < 4; ++i)
	{
		gm[i] = sinf(prime[i] * m);
		gp[i] = mtof(mix(36, 84, 0.5 * (1 - cop[i])));
	}
	sample common =
		( 3 * kick
		+ 2 * bass
		+ 4 * sub
		) * g;
	out[0] = sinf
		( common
		+ vcff(&s->bp[0][0], gm[0] * fb[0][0], gp[3], q)
		+ vcff(&s->bp[1][0], gm[1] * fb[1][0], gp[2], q)
		+ vcff(&s->bp[2][0], gm[2] * fb[2][0], gp[1], q)
		+ vcff(&s->bp[3][0], gm[3] * fb[3][0], gp[0], q)
		);
	out[1] = sinf
		( common
		+ vcff(&s->bp[0][1], gm[0] * fb[0][1], gp[3], q)
		+ vcff(&s->bp[1][1], gm[1] * fb[1][1], gp[2], q)
		+ vcff(&s->bp[2][1], gm[2] * fb[2][1], gp[1], q)
		+ vcff(&s->bp[3][1], gm[3] * fb[3][1], gp[0], q)
		);
	delwrite(&s->del[0].del, out[0]);
	delwrite(&s->del[1].del, out[1]);
}

//---------------------------------------------------------------------
// called during cleanup

static inline
void COMPOSITION_cleanup(BelaContext *context, struct COMPOSITION *C)
{
}

//---------------------------------------------------------------------
