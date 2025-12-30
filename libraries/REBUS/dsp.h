#pragma once

//---------------------------------------------------------------------
// dsp stuff ported from clive
// by Claude Heiland-Allen 2023-06-19
// added float-specialisations 2023-06-27
// added more documentation 2025-12-10

//---------------------------------------------------------------------
// common definitions
//---------------------------------------------------------------------

// Type alias for a single number.
// Change 'float' to 'double' if more precision is needed.
typedef float sample;

// Audio sample rate defined at compile time.
// Should match 'context->audioSampleRate' defined at runtime,
// otherwise everything will be out of tune.
#ifndef SR
#define SR 44100
#endif

// Mathematical constants.
#define pi 3.141592653589793
#define twopi 6.283185307179586

//---------------------------------------------------------------------
// functions
//---------------------------------------------------------------------

// Fractional part of 'x' in the range 0 (inclusive) to 1 (exclusive).
static inline sample wrap(sample x) {
  return x - floor(x);
}

static inline sample wrapto(sample x, sample n) {
  return wrap(x / n) * n;
}

static inline sample wrapat(sample x, sample n) {
  return wrap(x * n) / n;
}

// Clamp the value 'x' to lie between the bounds 'lo' <= 'x' <= 'hi'.
static inline sample clamp(sample x, sample lo, sample hi) {
  return fmin(fmax(x, lo), hi);
}

// Linearly interpolate between 'x' (at 't' = 0) and 'y' (at 't' = 1).
static inline sample mix(sample x, sample y, sample t) {
  return (1 - t) * x + t * y;
}

// Turn a phasor 'x' in the range 0 <= 'x' < 1
// into a variable triangle/sawtooth hybrid
// with shape controlled by 't' in the range 0 < 't' < 1.
static inline sample trisaw(sample x, sample t) {
  sample s = clamp(t, (sample)0.0000001, (sample)0.9999999);
  sample y = clamp(x, 0, 1);
  return y < s ? y / s : (1 - y) / (1 - s);
}

// Uniformly distributed white noise in the range -1 to +1.
static inline sample noise() {
  return 2 * (rand() / (sample) RAND_MAX - (sample)0.5);
}

// Logarithmic remapping functions
// based on implementations of Pure-data,
// pd-0.45-5/src/d_math.c

#define log10overten 0.23025850929940458 // log(10)/10
#define tenoverlog10 4.3429448190325175 // 10/log(10)

// Convert MIDI note number to audio frequency in Hz
static inline sample mtof(sample f) {
  return (sample)8.17579891564 * exp((sample)0.0577622650 * fmin(f, 1499));
}

// convert audio frequency in Hz to MIDI note number
static inline sample ftom(sample f) {
  return (sample)17.3123405046 * log((sample)0.12231220585 * f);
}

// convert decibels (where 100 = full scale) to audio level (RMS)
static inline sample dbtorms(sample f) {
  return exp((sample)0.5 * (sample)log10overten * (fmin(f, 870) - 100));
}

// convert audio level (RMS) to decibels (where 100 = full scale)
static inline sample rmstodb(sample f) {
  return 100 + 2 * (sample)tenoverlog10 * log(f);
}

// convert decibels (where 100 = full scale) to audio power
static inline sample dbtopow(sample f) {
  return exp((sample)log10overten * (fmin(f, 485) - 100));
}

// convert audio power to decibels (where 100 = full scale)
static inline sample powtodb(sample f) {
  return 100 + (sample)tenoverlog10 * log(f);
}

// convert the least significant byte of an int to the range -1 to +1
static inline sample bytebeat(int x) {
  return ((x & 0xFF) - 0x80) / (sample) 0x80;
}

// reduce the precision of 'x' to 'bits'-many bits,
// considered as a fixed point number
static inline sample bitcrush(sample x, sample bits) {
  sample n = pow(2, bits);
  return round(x * n) / n;
}

//---------------------------------------------------------------------
// oscillators
//---------------------------------------------------------------------

//---------------------------------------------------------------------
// phasor

// need accurate phase otherwise low frequency range is too quantized
// hence explicitly use a double here instead of the sample alias
typedef struct { double phase; } PHASOR;

static inline double phasor(PHASOR *p, double hz) {
  // can't use wrap as it is defined for sample which might be lower precision
  p->phase += hz / SR; // increment according to frequency
  p->phase -= floor(p->phase); // wrap to 0 to 1 range
  return p->phase;
}

//---------------------------------------------------------------------
// filters
//---------------------------------------------------------------------

//---------------------------------------------------------------------
// sample and hold
// a falling trigger signal stores the incoming value
// a rising or constant signal outputs the stored value

// sample and hold state
typedef struct {
  sample trigger;
  sample value;
} SAMPHOLD;

// sample and hold function
sample samphold(SAMPHOLD *s, sample value, sample trigger) {
  if (trigger < s->trigger) {
        s->value = value;
  }
  s->trigger = trigger;
  return s->value;
}

//---------------------------------------------------------------------
// biquad
// two-pole two-zero filter
// formulas taken from RBJ's Audio EQ Cookbook:
// http://musicdsp.org/files/Audio-EQ-Cookbook.txt

// when filter 'q' is set to 'flatq' or lower,
// the filters do not have a resonant peak in the frequency response
#define flatq 0.7071067811865476

// Generic biquad filter state.
// Filter coefficients and input/output history
// are stored in the same structure;
// separating coefficients from signal history
// would allow more sharing.
// Coefficients and feedback history
// are stored in double precision rather than sample
// because the extra precision is necessary for accurate response.
typedef struct { double b0, b1, b2, a1, a2, y1, y2; sample x1, x2; } BIQUAD;

// Generic biquad filter function.
static inline sample biquad(BIQUAD *bq, sample x0) {
  double b0 = bq->b0, b1 = bq->b1, b2 = bq->b2, a1 = bq->a1, a2 = bq->a2;
  double x1 = bq->x1, x2 = bq->x2, y1 = bq->y1, y2 = bq->y2;
  double y0 = b0 * x0 + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
  bq->y2 = y1;
  bq->y1 = y0;
  bq->x2 = x1;
  bq->x1 = x0;
  return y0;
}

// Calculate the coefficients for a lowpass biquad filter
// with cutoff frequency 'hz' (in Hz), and q-factor 'q'.
static inline BIQUAD *lowpass(BIQUAD *bq, sample hz, sample q) {
  double w0 = hz * twopi / SR;
  double a = fabs(sin(w0) / (2 * q));
  double c = cos(w0);
  double b0 = (1 - c) / 2, b1 = 1 - c, b2 = (1 - c) / 2;
  double a0 = 1 + a, a1 = -2 * c, a2 = 1 - a;
  bq->b0 = b0 / a0;
  bq->b1 = b1 / a0;
  bq->b2 = b2 / a0;
  bq->a1 = a1 / a0;
  bq->a2 = a2 / a0;
  return bq;
}

// Calculate the coefficients for a highpass biquad filter
// with cutoff frequency 'hz' (in Hz), and q-factor 'q'.
static inline BIQUAD *highpass(BIQUAD *bq, sample hz, sample q) {
  double w0 = hz * twopi / SR;
  double a = fabs(sin(w0) / (2 * q));
  double c = cos(w0);
  double b0 = (1 + c) / 2, b1 = -(1 + c), b2 = (1 + c) / 2;
  double a0 = 1 + a, a1 = -2 * c, a2 = 1 - a;
  bq->b0 = b0 / a0;
  bq->b1 = b1 / a0;
  bq->b2 = b2 / a0;
  bq->a1 = a1 / a0;
  bq->a2 = a2 / a0;
  return bq;
}

// Calculate the coefficients for a bandpass biquad filter
// with center frequency 'hz' (in Hz), and q-factor 'q'.
static inline BIQUAD *bandpass(BIQUAD *bq, sample hz, sample q) {
  double w0 = hz * twopi / SR;
  double a = fabs(sin(w0) / (2 * q));
  double c = cos(w0);
  double b0 = a, b1 = 0, b2 = -a;
  double a0 = 1 + a, a1 = -2 * c, a2 = 1 - a;
  bq->b0 = b0 / a0;
  bq->b1 = b1 / a0;
  bq->b2 = b2 / a0;
  bq->a1 = a1 / a0;
  bq->a2 = a2 / a0;
  return bq;
}

// Calculate the coefficients for a notch biquad filter
// with center frequency 'hz' (in Hz), and q-factor 'q'.
static inline BIQUAD *notch(BIQUAD *bq, sample hz, sample q) {
  double w0 = hz * twopi / SR;
  double a = fabs(sin(w0) / (2 * q));
  double c = cos(w0);
  double b0 = 1, b1 = -2 * c, b2 = 1;
  double a0 = 1 + a, a1 = -2 * c, a2 = 1 - a;
  bq->b0 = b0 / a0;
  bq->b1 = b1 / a0;
  bq->b2 = b2 / a0;
  bq->a1 = a1 / a0;
  bq->a2 = a2 / a0;
  return bq;
}

//---------------------------------------------------------------------
// simple filters
// based on pd's [vcf~] [lop~] [hip~]

// vcf is a resonant low-pass filter
// with cutoff frequency 'hz' in Hz
// and q-factor 'q'.

// variable cutoff filter state
// (double precision version for more accuracy, less speed)
typedef struct { double re, im; } VCF;

// variable cutoff filter function
// (double precision version for more accuracy, less speed)
static inline sample vcf(VCF *s, sample x, sample hz, sample q) {
  double qinv = q > 0 ? 1 / q : 0;
  double ampcorrect = 2 - 2 / (q + 2);
  double cf = hz * twopi / SR;
  if (cf < 0) { cf = 0; }
  double r = qinv > 0 ? 1 - cf * qinv : 0;
  if (r < 0) { r = 0; }
  double oneminusr = 1 - r;
  double cre = r * cos(cf);
  double cim = r * sin(cf);
  double re2 = s->re;
  s->re = ampcorrect * oneminusr * x + cre * re2 - cim * s->im;
  s->im = cim * re2 + cre * s->im;
  return s->re;
}

// variable cutoff filter state
// (single precision version for more speed, less accuracy)
typedef struct { float re, im; } VCFF;

// variable cutoff filter function
// (single precision version for more speed, less accuracy)
static inline sample vcff(VCFF *s, sample x, sample hz, sample q) {
  float qinv = q > 0 ? 1 / q : 0;
  float ampcorrect = 2 - 2 / (q + 2);
  float cf = hz * twopi / SR;
  if (cf < 0) { cf = 0; }
  float r = qinv > 0 ? 1 - cf * qinv : 0;
  if (r < 0) { r = 0; }
  float oneminusr = 1 - r;
  float cre = r * cosf(cf);
  float cim = r * sinf(cf);
  float re2 = s->re;
  s->re = ampcorrect * oneminusr * x + cre * re2 - cim * s->im;
  s->im = cim * re2 + cre * s->im;
  return s->re;
}

// lop is a simple low pass filter

// low pass filter state
// (double precision version for more accuracy, less speed)
typedef struct { double y; } LOP;

// low pass filter function
// (double precision version for more accuracy, less speed)
static inline sample lop(LOP *s, sample x, sample hz) {
  double c = clamp(twopi * hz / SR, 0, 1);
  return s->y = mix(x, s->y, 1 - c);
}

// low pass filter state
// (single precision version for more speed, less accuracy)
typedef struct { float y; } LOPF;

// low pass filter function
// (single precision version for more speed, less accuracy)
static inline sample lopf(LOPF *s, sample x, sample hz) {
  float c = clamp((float(twopi) / SR) * hz, 0, 1);
  return s->y = mix(s->y, x, c);
}

// high pass filter state
// (double precision version for more accuracy, less speed)
typedef struct { double y; } HIP;

// high pass filter state
// (double precision version for more accuracy, less speed)
static inline sample hip(HIP *s, sample x, sample hz) {
  double c = clamp(1 - twopi * hz / SR, 0, 1);
  double n = (1 + c) / 2;
  double y = x + c * s->y;
  double o = n * (y - s->y);
  s->y = y;
  return o;
}

//---------------------------------------------------------------------
// delay

// Delay line with separate read and write.
// Inspired by Pure-data.

// Delay state structure.
// Must be followed in memory by an array of float
// at least 'length' items in length.
// Method of operation is a ring-buffer.
// The maximum delay time in samples is determined by 'length'.
typedef struct { int length, woffset; } DELAY;

// For example, a delay line with a maximum delay time of 1 second:
typedef struct { DELAY delay; float buffer[SR]; } DELAY1s;

// Write a value to the delay line, incrementing the write index.
static inline void delwrite(DELAY *del, sample x0) {
  float *buffer = (float *) (del + 1);
  int l = del->length;
  l = (l > 0) ? l : 1;
  int w = del->woffset;
  buffer[w++] = x0;
  if (w >= l) { w -= l; }
  del->woffset = w;
}

// Read a value from the delay line without interpolation,
// at 'ms' milliseconds behind the write index.
// Lack of interpolation means the resulting delay time
// is quantized to the nearest sample,
// which can cause audible effects
// especially when the desired delay time 'ms' is changing.

static inline sample delread1(DELAY *del, sample ms) {
  float *buffer = (float *) (del + 1);
  int l = del->length;
  l = (l > 0) ? l : 1;
  int w = del->woffset;
  int d = ms / (sample)1000 * SR; // convert milliseconds to samples
  d = (0 < d && d < l) ? d : 0; // check in range
  int r = w - d; // make relative to write index
  r = r < 0 ? r + l : r; // wrap around the ring buffer
  return buffer[r];
}

// Read a value from the delay line with linear interpolation,
// at 'ms' milliseconds behind the write index.
// Linear interpolation reduces artifacts,
// especially when the delay time ('ms') is changing.
static inline sample delread2(DELAY *del, sample ms) {
  float *buffer = (float *) (del + 1);
  int l = del->length;
  l = (l > 0) ? l : 1;
  int w = del->woffset;
  sample d = ms / (sample)1000 * SR; // convert milliseconds to samples
  int d0 = floor(d); // find neighbouring indices
  int d1 = d0 + 1;
  sample t = d - d0; // find fractional distance through range
  d0 = (0 < d0 && d0 < l) ? d0 : 0; // check in range
  d1 = (0 < d1 && d1 < l) ? d1 : d0;
  int r0 = w - d0; // make relative to write index
  int r1 = w - d1;
  r0 = r0 < 0 ? r0 + l : r0; // wrap around the ring buffer
  r1 = r1 < 0 ? r1 + l : r1;
  sample y0 = buffer[r0]; // read from the buffer
  sample y1 = buffer[r1];
  return (1 - t) * y0 + t * y1; // linear interpolation
}

// Read a value from the delay line with cubic interpolation,
// at 'ms' milliseconds behind the write index.
// Cubic interpolation reduces artifacts further than linear,
// especially when the delay time ('ms') is changing.
// https://en.wikipedia.org/wiki/Cubic_Hermite_spline#Interpolation_on_the_unit_interval_without_exact_derivatives
static inline sample delread4(DELAY *del, sample ms) {
  float *buffer = (float *) (del + 1);
  int l = del->length;
  l = (l > 0) ? l : 1;
  int w = del->woffset;
  sample d = ms / (sample) 1000 * SR; // convert milliseconds to samples
  int d1 = floor(d); // find neighbouring indices
  int d0 = d1 - 1;
  int d2 = d1 + 1;
  int d3 = d1 + 2;
  sample t = d - d1; // find fractional distance through range
  d0 = (0 < d0 && d0 < l) ? d0 : 0; // check in range
  d1 = (0 < d1 && d1 < l) ? d1 : d0;
  d2 = (0 < d2 && d2 < l) ? d2 : d1;
  d3 = (0 < d3 && d3 < l) ? d3 : d2;
  int r0 = w - d0; // make relative to write index
  int r1 = w - d1;
  int r2 = w - d2;
  int r3 = w - d3;
  r0 = r0 < 0 ? r0 + l : r0; // wrap around the ring buffer
  r1 = r1 < 0 ? r1 + l : r1;
  r2 = r2 < 0 ? r2 + l : r2;
  r3 = r3 < 0 ? r3 + l : r3;
  sample y0 = buffer[r0]; // read from the buffer
  sample y1 = buffer[r1];
  sample y2 = buffer[r2];
  sample y3 = buffer[r3];
  sample a0 = -t*t*t + 2*t*t - t; // cubic interpolation
  sample a1 = 3*t*t*t - 5*t*t + 2;
  sample a2 = -3*t*t*t + 4*t*t + t;
  sample a3 = t*t*t - t*t;
  return (a0 * y0 + a1 * y1 + a2 * y2 + a3 * y3) / 2;
}

//---------------------------------------------------------------------
