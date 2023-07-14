//---------------------------------------------------------------------
/*

Dub Terrain
by Claude Heiland-Allen 2023-06-21, 2023-06-27, 2023-06-28, 2023-07-11

Dub delays with waveshaping and filters.
Phase and magnitude control things in a non-uniform way.
Magnitude controls overall and feedback gains.
Phase controls stereo rotation of the feedback and filter frequencies.

*/

//---------------------------------------------------------------------
// options

// #define MODE 1
// #define RECORD 1
// #define SCOPE 0
// #define CONTROL_LOP 1

//---------------------------------------------------------------------
// dependencies

#include <libraries/REBUS/REBUS.h>

// ARM Neon SIMD does 4 calculations in 1 without costing much more.
// Using this makes this composition computationally feasible.
#include <libraries/REBUS/dsp_neon.h>

//---------------------------------------------------------------------
// added to audio recording filename

const char *COMPOSITION_name = "dub-terrain";

//---------------------------------------------------------------------
// composition state

// delay line
struct DLINE
{
	DELAY del; // delay ugen control data
	float buf[1 << 17]; // delay buffer
};

// composition
struct COMPOSITION
{
	// ramps once per bar, used for kick
	PHASOR clock;
	// resonant filter for bass
	BIQUAD bass;
	// resonant filter for sub
	BIQUAD sub;
	// delay lines for feedback (stereo)
	DLINE del[2];
	// four parallel bandpass filters (stereo)
	VCF4 bandpass[2];
};

//---------------------------------------------------------------------
// called during setup

inline
bool COMPOSITION_setup(BelaContext *context, struct COMPOSITION *C)
{
	if (context->audioSampleRate != SR)
	{
		rt_printf("Sample rate mismatch: using %f, expected %f\n",
			(double) context->audioSampleRate, (double) SR);
		// return false; // FIXME should this be a hard error?
		// pitch and tempo will not be as intended...
	}

	// clear the memory to 0
	std::memset(C, 0, sizeof(*C));

	// calculate resonant filter coefficients
	highpass(&C->bass, 32, 20); // 32 Hz, Q 20
	lowpass(&C->sub, 64, 50); // 64 Hz, Q 50

	// set the length of the delay buffers (should match DLINE struct)
	C->del[0].del.length = 1 << 17;
	C->del[1].del.length = 1 << 17;
	return true;
}

//---------------------------------------------------------------------
// called once per audio frame (default 44100Hz sample rate)

inline
void COMPOSITION_render(BelaContext *context, struct COMPOSITION *s,
  float out[2], const float in[2], const float magnitude, const float phase)
{
	static const sample4 prime2pi =
		{ 2 * float(twopi), 3 * float(twopi), 5 * float(twopi), 7 * float(twopi) };
	const sample4 one = vmovq_n_f32(1.0f);

	// dry drum sounds
	sample tempo = 0.4; // about 96bpm
	// ramps from 0 to 1 once each bar
	sample clock = phasor(&s->clock, tempo);
	clock = 1 - clock;
	// these squarings are more computationally efficient than powf(clock, 64)
	clock *= clock;
	clock *= clock;
	clock *= clock;
	clock *= clock;
	clock *= clock;
	clock *= clock;
	// the kick is a sine wave with decaying frequency (chirp)
	sample kick = sinf(32 * float(pi) * clock);
	// the bass is resonant high pass filtered
	sample bass = kick - biquad(&s->bass, kick);
	// the sub is resonant low pass filtered
	sample sub = biquad(&s->sub, kick);

	// four channels of feedback with different delay times (stereo)
	sample4 feedback0, feedback1;
	for (int i = 0; i < 4; ++i)
	{
		feedback0[i] = delread1(&s->del[0].del, 1000 / tempo * 1 / (i + 1.5));
		feedback1[i] = delread1(&s->del[1].del, 1000 / tempo * 1 / (i + 1.5));
	}

	// rotate delayed feedbacks in stereo field
	// rotation angles are multiples of the phase control
	// compute sine and cosine of four multiples of the phase control
	sample4 sin_phase, cos_phase;
	sincos4(sin_phase, cos_phase, vmulq_n_f32(prime2pi, phase));
	// do four matrix-vector multiplications for the four channels
	sample4 tmp0 = vsubq_f32(vmulq_f32(cos_phase, feedback0),
	                         vmulq_f32(sin_phase, feedback1));
	sample4 tmp1 = vaddq_f32(vmulq_f32(sin_phase, feedback0),
	                         vmulq_f32(cos_phase, feedback1));
	feedback0 = tmp0;
	feedback1 = tmp1;

	// calculate filter parameters (q, hz)
	// compute sine and cosine of four multiples of the magnitude control
	sample4 sin_magnitude, ignored;
	sincos4(sin_magnitude, ignored, vmulq_n_f32(prime2pi, magnitude));
	// filter q factor is constant for all of them
	sample q = 3;
	// filter frequency is based on multiples of the phase control
	sample4 hz = mtof4(vaddq_f32(vmulq_n_f32(
		vmulq_n_f32(vsubq_f32(one, cos_phase), 0.5f),
		84.0f - 36.0f), vmovq_n_f32(36.0f)));
	// permute the frequencies for a bit more variation
	{ sample t = hz[3]; hz[3] = hz[0]; hz[0] = t; }
	{ sample t = hz[2]; hz[2] = hz[1]; hz[1] = t; }

	// four parallel bandpass filter for each of two channels
	// the feedback gain is applied here too
	feedback0 = vcf4(&s->bandpass[0], vmulq_f32(sin_magnitude, feedback0), hz, q);
	feedback1 = vcf4(&s->bandpass[1], vmulq_f32(sin_magnitude, feedback1), hz, q);

	// waveshaping / mixing
	// overall non-feedback gain is based on the magnitude control
	sample gain = 0.5 * sinf(float(pi) * magnitude);
	sample common = (3 * kick + 2 * bass + 4 * sub) * gain;
	// add the four filtered feedbacks to the dry sounds and waveshape
	out[0] = sinf(common
		+ feedback0[0] + feedback0[1] + feedback0[2] + feedback0[3]);
	out[1] = sinf(common
		+ feedback1[0] + feedback1[1] + feedback1[2] + feedback1[3]);

	// write the output to the delay lines for future feedback
	delwrite(&s->del[0].del, out[0]);
	delwrite(&s->del[1].del, out[1]);
}

//---------------------------------------------------------------------
// called during cleanup

inline
void COMPOSITION_cleanup(BelaContext *context, struct COMPOSITION *C)
{
	// nothing to do here in this composition
}

//---------------------------------------------------------------------

REBUS

//---------------------------------------------------------------------
