#pragma once

//---------------------------------------------------------------------
// dsp stuff ported from clive
// by Claude Heiland-Allen 2023-06-19
// added float-specialisations 2023-06-27

//---------------------------------------------------------------------
// common defs
//---------------------------------------------------------------------

typedef float sample;
#ifndef SR
#define SR 44100
#endif

#define pi 3.141592653589793
#define twopi 6.283185307179586

//---------------------------------------------------------------------
// functions
//---------------------------------------------------------------------

static inline sample wrap(sample x) {
  return x - floor(x);
}

static inline sample wrapto(sample x, sample n) {
  return wrap(x / n) * n;
}

static inline sample wrapat(sample x, sample n) {
  return wrap(x * n) / n;
}

static inline sample clamp(sample x, sample lo, sample hi) {
  return fmin(fmax(x, lo), hi);
}

static inline sample mix(sample x, sample y, sample t) {
  return (1 - t) * x + t * y;
}

static inline sample trisaw(sample x, sample t) {
  sample s = clamp(t, (sample)0.0000001, (sample)0.9999999);
  sample y = clamp(x, 0, 1);
  return y < s ? y / s : (1 - y) / (1 - s);
}

static inline sample noise() {
  return 2 * (rand() / (sample) RAND_MAX - (sample)0.5);
}

// pd-0.45-5/src/d_math.c

#define log10overten 0.23025850929940458
#define tenoverlog10 4.3429448190325175

static inline sample mtof(sample f) {
  return (sample)8.17579891564 * exp((sample)0.0577622650 * fmin(f, 1499));
}

static inline sample ftom(sample f) {
  return (sample)17.3123405046 * log((sample)0.12231220585 * f);
}

static inline sample dbtorms(sample f) {
  return exp((sample)0.5 * (sample)log10overten * (fmin(f, 870) - 100));
}

static inline sample rmstodb(sample f) {
  return 100 + 2 * (sample)tenoverlog10 * log(f);
}

static inline sample dbtopow(sample f) {
  return exp((sample)log10overten * (fmin(f, 485) - 100));
}

static inline sample powtodb(sample f) {
  return 100 + (sample)tenoverlog10 * log(f);
}


static inline sample bytebeat(int x) {
  return ((x & 0xFF) - 0x80) / (sample) 0x80;
}

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
typedef struct { double phase; } PHASOR;

static inline double phasor(PHASOR *p, double hz) {
  // can't use wrap as it is defined for sample which might be lower precision
  p->phase += hz / SR;
  p->phase -= floor(p->phase);
  return p->phase;
}

//---------------------------------------------------------------------
// filters
//---------------------------------------------------------------------

//---------------------------------------------------------------------
// sample and hold

typedef struct {
  sample trigger;
  sample value;
} SAMPHOLD;

sample samphold(SAMPHOLD *s, sample value, sample trigger) {
  if (trigger < s->trigger) {
        s->value = value;
  }
  s->trigger = trigger;
  return s->value;
}

//---------------------------------------------------------------------
// biquad

// http://musicdsp.org/files/Audio-EQ-Cookbook.txt

#define flatq 0.7071067811865476

typedef struct { double b0, b1, b2, a1, a2, y1, y2; sample x1, x2; } BIQUAD;

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
// based on pd's [vcf~] [lop~] [hip~]

typedef struct { double re, im; } VCF;

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

typedef struct { float re, im; } VCFF;

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

typedef struct { double y; } LOP;

static inline sample lop(LOP *s, sample x, sample hz) {
  double c = clamp(twopi * hz / SR, 0, 1);
  return s->y = mix(x, s->y, 1 - c);
}

typedef struct { float y; } LOPF;

static inline sample lopf(LOPF *s, sample x, sample hz) {
  float c = clamp((float(twopi) / SR) * hz, 0, 1);
  return s->y = mix(s->y, x, c);
}

typedef struct { double y; } HIP;

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

typedef struct { int length, woffset; } DELAY;

static inline void delwrite(DELAY *del, sample x0) {
  float *buffer = (float *) (del + 1);
  int l = del->length;
  l = (l > 0) ? l : 1;
  int w = del->woffset;
  buffer[w++] = x0;
  if (w >= l) { w -= l; }
  del->woffset = w;
}

static inline sample delread1(DELAY *del, sample ms) {
  float *buffer = (float *) (del + 1);
  int l = del->length;
  l = (l > 0) ? l : 1;
  int w = del->woffset;
  int d = ms / (sample)1000 * SR;
  d = (0 < d && d < l) ? d : 0;
  int r = w - d;
  r = r < 0 ? r + l : r;
  return buffer[r];
}

static inline sample delread2(DELAY *del, sample ms) {
  float *buffer = (float *) (del + 1);
  int l = del->length;
  l = (l > 0) ? l : 1;
  int w = del->woffset;
  sample d = ms / (sample)1000 * SR;
  int d0 = floor(d);
  int d1 = d0 + 1;
  sample t = d - d0;
  d0 = (0 < d0 && d0 < l) ? d0 : 0;
  d1 = (0 < d1 && d1 < l) ? d1 : d0;
  int r0 = w - d0;
  int r1 = w - d1;
  r0 = r0 < 0 ? r0 + l : r0;
  r1 = r1 < 0 ? r1 + l : r1;
  sample y0 = buffer[r0];
  sample y1 = buffer[r1];
  return (1 - t) * y0 + t * y1;
}

// https://en.wikipedia.org/wiki/Cubic_Hermite_spline#Interpolation_on_the_unit_interval_without_exact_derivatives

static inline sample delread4(DELAY *del, sample ms) {
  float *buffer = (float *) (del + 1);
  int l = del->length;
  l = (l > 0) ? l : 1;
  int w = del->woffset;
  sample d = ms / (sample) 1000 * SR;
  int d1 = floor(d);
  int d0 = d1 - 1;
  int d2 = d1 + 1;
  int d3 = d1 + 2;
  sample t = d - d1;
  d0 = (0 < d0 && d0 < l) ? d0 : 0;
  d1 = (0 < d1 && d1 < l) ? d1 : d0;
  d2 = (0 < d2 && d2 < l) ? d2 : d1;
  d3 = (0 < d3 && d3 < l) ? d3 : d2;
  int r0 = w - d0;
  int r1 = w - d1;
  int r2 = w - d2;
  int r3 = w - d3;
  r0 = r0 < 0 ? r0 + l : r0;
  r1 = r1 < 0 ? r1 + l : r1;
  r2 = r2 < 0 ? r2 + l : r2;
  r3 = r3 < 0 ? r3 + l : r3;
  sample y0 = buffer[r0];
  sample y1 = buffer[r1];
  sample y2 = buffer[r2];
  sample y3 = buffer[r3];
  sample a0 = -t*t*t + 2*t*t - t;
  sample a1 = 3*t*t*t - 5*t*t + 2;
  sample a2 = -3*t*t*t + 4*t*t + t;
  sample a3 = t*t*t - t*t;
  return (a0 * y0 + a1 * y1 + a2 * y2 + a3 * y3) / 2;
}

//---------------------------------------------------------------------
