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

#include "_dsp_neon.h"

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
	VCF4 bp[2];
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
	static const sample4 prime2pi = { 2 * float(twopi), 3 * float(twopi), 5 * float(twopi), 7 * float(twopi) };
	const sample4 one = vmovq_n_f32(1.0f);
	// smoothing to avoid zipper noise with non-REBUS controllers
	// reduces EMI noise (?) with REBUS controller
	float in[2] = { magnitude, phase };
	in[0] = lopf(&s->lo[0], in[0], 10);
	in[1] = lopf(&s->lo[1], in[1], 10);
	float m = clamp(in[0], 0, 1);
	float p = clamp(in[1], 0, 1);
	// drum sounds
	sample tempo = 0.4;
	sample clock = phasor(&s->clock, tempo);
	clock = 1 - clock;
	clock *= clock;
	clock *= clock;
	clock *= clock;
	clock *= clock;
	clock *= clock;
	clock *= clock;
	sample kick = sinf(32 * float(pi) * clock);
	sample bass = kick - biquad(&s->bq[0], kick);
	sample sub = biquad(&s->bq[1], kick);
	// rotate delayed feedback in stereo field
	sample4 sip, cop;
	sincos4(sip, cop, vmulq_n_f32(prime2pi, p));
	sample4 fb0, fb1;
	for (int i = 0; i < 4; ++i)
	{
		fb0[i] = delread1(&s->del[0].del, 1000 / tempo * 1 / (i + 1.5));
		fb1[i] = delread1(&s->del[1].del, 1000 / tempo * 1 / (i + 1.5));
	}
	sample4 tmp0 = vsubq_f32(vmulq_f32(cop, fb0), vmulq_f32(sip, fb1));
	sample4 tmp1 = vaddq_f32(vmulq_f32(sip, fb0), vmulq_f32(cop, fb1));
	fb0 = tmp0;
	fb1 = tmp1;
	// resonant bandpass filters
	sample q = 3;
	sample4 gain, ignored;
	sincos4(gain, ignored, vmulq_n_f32(prime2pi, m));
	sample4 hz = mtof4(vaddq_f32(vmulq_n_f32(vmulq_n_f32(vsubq_f32(one, cop), 0.5f), 84.0f - 36.0f), vmovq_n_f32(36.0f)));
	{ sample t = hz[3]; hz[3] = hz[0]; hz[0] = t; }
	{ sample t = hz[2]; hz[2] = hz[1]; hz[1] = t; }
	fb0 = vcf4(&s->bp[0], vmulq_f32(gain, fb0), hz, q);
	fb1 = vcf4(&s->bp[1], vmulq_f32(gain, fb1), hz, q);
	// waveshaping / mixing
	sample g = 0.5 * sinf(float(pi) * m);
	sample common = (3 * kick + 2 * bass + 4 * sub) * g;
	out[0] = sinf(common + fb0[0] + fb0[1] + fb0[2] + fb0[3]);
	out[1] = sinf(common + fb1[0] + fb1[1] + fb1[2] + fb1[3]);
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
