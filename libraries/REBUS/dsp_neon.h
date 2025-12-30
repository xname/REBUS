#pragma once

//---------------------------------------------------------------------
// NEON SIMD specialisations of dsp stuff ported from clive
// by Claude Heiland-Allen 2023-06-27, 2023-07-14, 2025-12-10

#include <arm_neon.h>
#include <math.h>
#include "dsp.h"

//---------------------------------------------------------------------
// common definitions
//---------------------------------------------------------------------

// a vector of 4 individual audio samples (4 single numbers)
typedef float32x4_t sample4;

//---------------------------------------------------------------------
// maths
//---------------------------------------------------------------------

// Compute sine and cosine of a vector of 4 angles.
// Each angle should be smaller than 2 pi
// otherwise accuracy will suffer.
// Equivalent to (but faster than) the unrolled code:
/*
for (int i = 0; i < 4; ++i)
{
  si[i] = sin(theta[i]);
  co[i] = cos(theta[i]);
}
*/
static inline
void sincos4(sample4 &si, sample4 &co, const sample4 &theta)
{
	// precondition: |theta| < 2 pi
	// scale down by large power of 2
	// (t = theta / 2**28)
	// so that approximately
	// sin(t)=t and cos(t)=1
	co = vmovq_n_f32(1.0f);
	si = vmulq_n_f32(theta, 3.72529e-09f); // 2**(-28)
	// scale back up by repeated angle doubling
	for (unsigned int i = 0; i < 28; ++i)
	{
		// apply double angle formulae:
		// cos(2t) = cos(t)^2 - sin(t)^2
		// sin(2t) = 2 * cos(t) * sin(t)
		// co_next = co * co - si * si
		sample4 co_next = vsubq_f32(vmulq_f32(co, co), vmulq_f32(si, si));
		// si_next = 2 * co * si
		sample4 si_next = vmulq_n_f32(vmulq_f32(co, si), 2.0f);
		co = co_next;
		si = si_next;
	}
	// postcondition: si = sin(theta), co = cos(theta)
}

// Compute exponential of a vector of 4 values.
// Each value should be small and positive
// otherwise accuracy will suffer.
// Equivalent to (but faster than) the unrolled code:
/*
for (int i = 0; i < 4; ++i)
{
  returned_value[i] = exp(x[i]);
}
*/
static inline
sample4 exp4(const sample4 &x)
{
	// precondition: 0 <= x <= 8
	// (range needed by mtof with human-audible result frequencies)
	// Taylor series 28 terms needed for x = 8 at float32 accuracy
	// smaller x need fewer terms (x = 0.5 needs 9 terms)
	// identity: exp(x) = exp(x/a)^a
	// strategy: divide by 16, use the Taylor series, then square 4 times
	sample4 x16 = vmulq_n_f32(x, 1.0f/16.0f); // x16 = x / 16
	sample4 xn = vmovq_n_f32(1.0f); // xn = 1
	sample4 sum = vmovq_n_f32(1.0f); // sum = 1
	for (unsigned int n = 1; n < 10; ++n)
	{
		xn = vmulq_n_f32(vmulq_f32(xn, x16), 1.0f / n); // xn *= x16 / n
		sum = vaddq_f32(sum, xn); // sum += xn;
	}
	for (unsigned int n = 0; n < 4; ++n)
	{
		sum = vmulq_f32(sum, sum); // sum *= sum
	}
	return sum;
}

// Based on code from Pure-data:
// pd-0.45-5/src/d_math.c

// Convert a vector of 4 MIDI note numbers
// to a vector of 4 frequencies in Hz (A 440)
static inline
sample4 mtof4(const sample4 &f)
{
	return vmulq_n_f32(exp4(vmulq_n_f32(f, 0.0577622650f)), 8.17579891564f);
}

//---------------------------------------------------------------------
// filters
//---------------------------------------------------------------------

//---------------------------------------------------------------------
// Variable cutoff filter, based on pd's [vcf~].
// See dsp.h vcff() for a non-vectorized implementation.

// 4 parallel bandpass resonators (4 inputs and outputs).
// 4 different frequencies, all same Q-factor.

// Filter state.
typedef struct { float re[4], im[4]; } VCF4;

// Filter function.
// Four input signals 'x'.
// Four filter frequencies 'hz' in Hz.
// One common filter q-factor 'q'.
static inline
sample4 vcf4(VCF4 *s, const sample4 &x, const sample4 &hz, const sample &q)
{
	// precondition: 0 < q, 0 <= hz < SR
	// q is scalar, have not yet needed vector version
	sample qinv = 1 / q;
	sample ampcorrect = 2 - 2 / (q + 2);
	sample4 cf = vmulq_n_f32(hz, float(twopi) / SR); // cf = hz * 2 pi / SR
	sample4 one = vmovq_n_f32(1.0f); // one = 1
	sample4 r = vsubq_f32(one, vmulq_n_f32(cf, qinv)); // r = 1 - cf * qinv
	r = vmaxq_f32(r, vmovq_n_f32(0.0f)); // r = max(r, 0)
	sample4 oneminusr = vsubq_f32(one, r); // oneminusr = 1 - r
	sample4 cre, cim;
	sincos4(cim, cre, cf); // cim = sin(cf), cre = cos(cf)
	cre = vmulq_f32(cre, r); // cre *= r
	cim = vmulq_f32(cim, r); // cim *= r
	sample4 re2 = vld1q_f32(s->re); // load re from filter state
	sample4 im2 = vld1q_f32(s->im); // load im from filter state
	// re = ampcorrect * oneminusr * x + cre *re2 - cim * im2
	sample4 re = vaddq_f32(vmulq_n_f32(vmulq_f32(oneminusr, x), ampcorrect)
	           , vsubq_f32(vmulq_f32(cre, re2), vmulq_f32(cim, im2)));
	// im = cim * re2 + cre * im2
	sample4 im = vaddq_f32(vmulq_f32(cim, re2), vmulq_f32(cre, im2));
	vst1q_f32(s->re, re); // store re to filter state
	vst1q_f32(s->im, im); // store im to filter state
	return re;
}

//---------------------------------------------------------------------
