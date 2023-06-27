//---------------------------------------------------------------------
/*

Dub Terrain
by Claude Heiland-Allen 2023-06-21

Dub delays with waveshaping and filters.
Phase and magnitude control many things in a non-uniform way.

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
	float buf[65536];
};

struct COMPOSITION
{
	PHASOR clock;
	BIQUAD bq[2];
	DLINE del[2];
	LOP lo[2];
	VCF bp[4][2];
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
	C->del[0].del.length = 65536;
	C->del[1].del.length = 65536;
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
	in[0] = lop(&s->lo[0], in[0], 10);
	in[1] = lop(&s->lo[1], in[1], 10);
	float m = clamp(in[0], 0, 1);
	float p = clamp(in[1], 0, 1);
	// drum sounds
	sample tempo = 0.4;
	sample clock = phasor(&s->clock, tempo);
	sample kick = sin(32 * pi * pow(1 - clock, 64));
	sample bass = kick - biquad(&s->bq[0], kick);
	sample sub = biquad(&s->bq[1], kick);
	sample fb[4][2];
	for (int i = 0; i < 4; ++i)
	{
		for (int j = 0; j < 2; ++j)
		{
			fb[i][j] = delread4(&s->del[j].del, 1000 / tempo * 1 / (i + 1.5));
		}
		sample co = cos(prime[i] * twopi * p);
		sample si = sin(prime[i] * twopi * p);
		sample tmp[2] = { co * fb[i][0] - si * fb[i][1], si * fb[i][0] + co * fb[i][1] };
		fb[i][0] = tmp[0];
		fb[i][1] = tmp[1];
	}
	// waveshaping / mixing
	sample g = 0.5 * sin(pi * m);
	sample q = 3;
	out[0] = sin
	(	(	(	3 * kick
			+	2 * bass
			+	4 * sub
			) * g
		+ vcf(&s->bp[0][0], sin(prime[0] * twopi * m) * fb[0][0], mtof(mix(36, 84, 0.5 * (1 - cos(prime[3] * twopi * p)))), q)
		+ vcf(&s->bp[1][0], sin(prime[1] * twopi * m) * fb[1][0], mtof(mix(36, 84, 0.5 * (1 - cos(prime[2] * twopi * p)))), q)
		+ vcf(&s->bp[2][0], sin(prime[2] * twopi * m) * fb[2][0], mtof(mix(36, 84, 0.5 * (1 - cos(prime[1] * twopi * p)))), q)
		+ vcf(&s->bp[3][0], sin(prime[3] * twopi * m) * fb[3][0], mtof(mix(36, 84, 0.5 * (1 - cos(prime[0] * twopi * p)))), q)
		)
	);
	out[1] = sin
	(	(	(	3 * kick
			+	2 * bass
			+	4 * sub
			) * g
		+ vcf(&s->bp[0][1], sin(prime[0] * twopi * m) * fb[0][1], mtof(mix(36, 84, 0.5 * (1 - cos(prime[3] * twopi * p)))), q)
		+ vcf(&s->bp[1][1], sin(prime[1] * twopi * m) * fb[1][1], mtof(mix(36, 84, 0.5 * (1 - cos(prime[2] * twopi * p)))), q)
		+ vcf(&s->bp[2][1], sin(prime[2] * twopi * m) * fb[2][1], mtof(mix(36, 84, 0.5 * (1 - cos(prime[1] * twopi * p)))), q)
		+ vcf(&s->bp[3][1], sin(prime[3] * twopi * m) * fb[3][1], mtof(mix(36, 84, 0.5 * (1 - cos(prime[0] * twopi * p)))), q)
		)
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
