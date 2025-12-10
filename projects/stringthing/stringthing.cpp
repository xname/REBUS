//---------------------------------------------------------------------
/*

stringthing
by Claude Heiland-Allen 2011-10-18, 2023-06-16

Spinning string drone physical model.

Note: requires bigger block size to avoid underruns.
Tested with block size 512, 60% CPU load.

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

//---------------------------------------------------------------------
// added to audio recording filename

const char *COMPOSITION_name = "stringthing";

//---------------------------------------------------------------------
// composition parameters

typedef float R;
#define PI ((R)(3.141592653589793))
#define DT ((R)(0.0625))
#define FRICTION ((R)(0.9999))
#define GRAVITY ((R)(2))
#define MINN 32
#define MAXN 2048
#define HOP 8 // increase this if CPU overload crash

//---------------------------------------------------------------------
// composition state

struct elem
{
	// position and velocity in 3D
	R x, y, z, dx, dy, dz;
};

struct COMPOSITION
{
	// physical model
	struct elem string[2][MAXN];
	// scanned waveform
	float buf[MAXN][2];
	// length of used part (2 * N <= MAXN)
	int N;
	// which string buffer for, ping-pong double buffering
	int w;
	// sample index (time)
	int phase;
	// sample rate
	R sampleRate;
	// desired pitch
	R HZ;
	// used by the physical model
	R f1; // input position frequency mapping
	R f2;
	R p1; // input position oscillators
	R p2;
	R lo; // left waveform filters
	R lh;
	R lb;
	R ro; // right waveform filters
	R rh;
	R rb;
	R ls; // low pass filter for string deviation
	// input mapping filters
	R magnitude_lop;
	R phase_lop;
};

//---------------------------------------------------------------------
// called during setup

inline
bool COMPOSITION_setup(BelaContext *context, struct COMPOSITION *C)
{
	std::memset(C, 0, sizeof(*C));
	C->sampleRate = context->audioSampleRate;
	C->HZ = 96000.0f / 1024.0f;
	C->N = std::fmin(std::fmax(C->sampleRate / C->HZ, (R)MINN), (R)MAXN);
	// initial string position is hanging vertically downwards
	for (int i = 0; i < C->N; ++i) {
		C->string[C->w][i].z = -i;
	}
	return true;
}

inline
void COMPOSITION_render(BelaContext *context, struct COMPOSITION *C, int n, float out[2], const float in[2], const float magnitude, const float phase)
{
	if (C->phase == 0) // when run out of buffer to play
	{
		// audio waveform based on shape of string
		// find RMS deviation of string from origin
		R s = 0;
		for (int i = 0; i < C->N; ++i) {
			s += C->string[C->w][i].dx * C->string[C->w][i].dx
			  +  C->string[C->w][i].dy * C->string[C->w][i].dy
			  +  C->string[C->w][i].dz * C->string[C->w][i].dz;
		}
		s = 4 * std::sqrt(s / C->N);
		// low pass filter over time
		C->ls = ((R)0.99) * C->ls + ((R)0.01) * s;
		// reset to 1 if it's not positive
		if (!(C->ls > 0)) { C->ls = 1; }
		// find orientation of last point in the string
		R a0 = -std::atan2(C->string[C->w][C->N-1].dy, C->string[C->w][C->N-1].dx);
		R s0 = std::sin(a0);
		R c0 = std::cos(a0);
		// loop over the string twice (forward followed by reverse)
		for (int j = 0; j < 2 * C->N; ++j) {
			int i = j >= C->N ? 2 * C->N - 1 - j : j;
			// rotate string x and y to match the orientation of the free end
			R x =  c0 * C->string[C->w][i].dx + s0 * C->string[C->w][i].dy;
			R y = -s0 * C->string[C->w][i].dx + c0 * C->string[C->w][i].dy;
			// compute left and right audio channel input,
			// based on position of string (z mono, x left, y right)
			R lv = x * x + C->string[C->w][i].dz * C->string[C->w][i].dz;
			R rv = y * y + C->string[C->w][i].dz * C->string[C->w][i].dz;
			// weight by position on string (highest weight at free end)
			R wd = (1 - std::cos(j * PI / C->N)) / 2;
			lv = std::sqrt(lv) / C->ls;
			rv = std::sqrt(rv) / C->ls;
			// low pass filter along string
			C->lo = ((R)0.999) * C->lo + ((R)0.001) * lv;
			C->ro = ((R)0.999) * C->ro + ((R)0.001) * rv;
			// difference is high pass filter
			C->lh = lv - C->lo;
			C->rh = rv - C->ro;
			// mix with previous with soft clipping
			C->lb = std::tanh(((R)0.5) * C->lb + ((R)0.5) * wd * C->lh);
			C->rb = std::tanh(((R)0.5) * C->rb + ((R)0.5) * wd * C->rh);
			// write to string audio buffer
			C->buf[j][0] = std::tanh(2 * C->lb);
			C->buf[j][1] = std::tanh(2 * C->rb);
		}
	}

#if 1 // FIXME
	// low pass filter inputs from REBUS antenna
	C->magnitude_lop *= 0.9999f;
	C->magnitude_lop += 0.0001f * magnitude;
	C->phase_lop *= 0.9999f;
	C->phase_lop += 0.0001f * phase;
#else
	// directly use inputs from REBUS antenna
	C->magnitude_lop = magnitude;
	C->phase_lop = phase;
#endif

	if (C->phase % HOP == 0) // periodically update string
	{
		// input mapping
		R f = 2.0f * (C->magnitude_lop - 0.5f);
		R d = 4.0f * (C->phase_lop - 0.5f);
		R m = (std::pow(2, std::fabs(f)) - 1) * (f < 0 ? -1 : 1);
		R r = std::pow(2, d);
		C->f1 = (m * r) / 8.0;
		C->f2 = (m / r) / 8.0;
		// physical model
		R a1 = 1 / (1 + C->f1 * C->f2);
		R a2 = 1 / (1 + C->f1 * C->f2);
		C->p1 += C->f1 * C->N / C->sampleRate;
		C->p2 += C->f2 * C->N / C->sampleRate;
		C->p1 -= std::floor(C->p1);
		C->p2 -= std::floor(C->p2);
		// first point is moved according to input
		{
			R fx = a1 * std::cos(2 * PI * C->p1) - a2 * std::sin(2 * PI * C->p2);
			R fy = a1 * std::sin(2 * PI * C->p1) + a2 * std::cos(2 * PI * C->p2);
			R fz = 0;
			C->string[1-C->w][0].dx = FRICTION * C->string[C->w][0].dx + fx * DT;
			C->string[1-C->w][0].dy = FRICTION * C->string[C->w][0].dy + fy * DT;
			C->string[1-C->w][0].dz = FRICTION * C->string[C->w][0].dz + fz * DT;
			C->string[1-C->w][0].x = C->string[C->w][0].x + C->string[1-C->w][0].dx * DT;
			C->string[1-C->w][0].y = C->string[C->w][0].y + C->string[1-C->w][0].dy * DT;
			C->string[1-C->w][0].z = C->string[C->w][0].z + C->string[1-C->w][0].dz * DT;
		}
		// rest of string moves according to physical model:
		// force[i] := (position[i-1] - 2 position[i] + position[i+1]) + gravity
		// velocity[i] := friction * velocity[i] + force[i] * dt
		// position[i] := position[i] + velocity[i] * dt
		for (int i = 1; i < C->N - 1; ++i) {
			R fx = C->string[C->w][i-1].x + C->string[C->w][i+1].x - 2 * C->string[C->w][i].x;
			R fy = C->string[C->w][i-1].y + C->string[C->w][i+1].y - 2 * C->string[C->w][i].y;
			R fz = C->string[C->w][i-1].z + C->string[C->w][i+1].z - 2 * C->string[C->w][i].z
								- GRAVITY;
			C->string[1-C->w][i].dx = FRICTION * C->string[C->w][i].dx + fx * DT;
			C->string[1-C->w][i].dy = FRICTION * C->string[C->w][i].dy + fy * DT;
			C->string[1-C->w][i].dz = FRICTION * C->string[C->w][i].dz + fz * DT;
			C->string[1-C->w][i].x = C->string[C->w][i].x + C->string[1-C->w][i].dx * DT;
			C->string[1-C->w][i].y = C->string[C->w][i].y + C->string[1-C->w][i].dy * DT;
			C->string[1-C->w][i].z = C->string[C->w][i].z + C->string[1-C->w][i].dz * DT;
		}
		// end of string is free
		for (int i = C->N - 1; i < C->N; ++i) {
			R fx = C->string[C->w][i-1].x - C->string[C->w][i].x;
			R fy = C->string[C->w][i-1].y - C->string[C->w][i].y;
			R fz = C->string[C->w][i-1].z - C->string[C->w][i].z - GRAVITY;
			C->string[1-C->w][i].dx = FRICTION * C->string[C->w][i].dx + fx * DT;
			C->string[1-C->w][i].dy = FRICTION * C->string[C->w][i].dy + fy * DT;
			C->string[1-C->w][i].dz = FRICTION * C->string[C->w][i].dz + fz * DT;
			C->string[1-C->w][i].x = C->string[C->w][i].x + C->string[1-C->w][i].dx * DT;
			C->string[1-C->w][i].y = C->string[C->w][i].y + C->string[1-C->w][i].dy * DT;
			C->string[1-C->w][i].z = C->string[C->w][i].z + C->string[1-C->w][i].dz * DT;
		}
		// toggle double-buffering index
		C->w = 1 - C->w;
	}

	// play waveform from buffer
	out[0] = C->buf[C->phase][0];
	out[1] = C->buf[C->phase][1];
	C->phase++;
	if (C->phase >= 2 * C->N)
	{
		C->phase = 0;
	}
}

//---------------------------------------------------------------------
// called during cleanup

inline
void COMPOSITION_cleanup(BelaContext *context, struct COMPOSITION *C)
{
}

//---------------------------------------------------------------------

REBUS

//---------------------------------------------------------------------
