//---------------------------------------------------------------------
/*

stringthing
by Claude Heiland-Allen 2011-10-18, 2023-06-16

Spinning string drone physical model.

Note: requires bigger block size to avoid underruns.
Tested with block size 256, 60% CPU load.

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
	R x, y, z, dx, dy, dz;
};

struct COMPOSITION
{
	struct elem string[2][MAXN];
	float buf[MAXN][2];
	int N;
	int w;
	int phase;
	R sampleRate;
	R HZ;
	R f1;
	R f2;
	R p1;
	R p2;
	R lo;
	R lh;
	R lb;
	R ro;
	R rh;
	R rb;
	R ls;
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
	for (int i = 0; i < C->N; ++i) {
		C->string[C->w][i].z = -i;
	}
	return true;
}

inline
void COMPOSITION_render(BelaContext *context, struct COMPOSITION *C, float out[2], const float in[2], const float magnitude, const float phase)
{
	if (C->phase == 0)
	{
		// audio waveform based on shape of string
		R s = 0;
		for (int i = 0; i < C->N; ++i) {
			s += C->string[C->w][i].dx * C->string[C->w][i].dx
			  +  C->string[C->w][i].dy * C->string[C->w][i].dy
			  +  C->string[C->w][i].dz * C->string[C->w][i].dz;
		}
		s = 4 * std::sqrt(s / C->N);
		C->ls = ((R)0.99) * C->ls + ((R)0.01) * s;
		if (!(C->ls > 0)) { C->ls = 1; }
		R a0 = -std::atan2(C->string[C->w][C->N-1].dy, C->string[C->w][C->N-1].dx);
		R s0 = std::sin(a0);
		R c0 = std::cos(a0);
		for (int j = 0; j < 2 * C->N; ++j) {
			int i = j >= C->N ? 2 * C->N - 1 - j : j;
			R x =  c0 * C->string[C->w][i].dx + s0 * C->string[C->w][i].dy;
			R y = -s0 * C->string[C->w][i].dx + c0 * C->string[C->w][i].dy;
			R lv = x * x + C->string[C->w][i].dz * C->string[C->w][i].dz;
			R rv = y * y + C->string[C->w][i].dz * C->string[C->w][i].dz;
			R wd = (1 - std::cos(j * PI / C->N)) / 2;
			lv = std::sqrt(lv) / C->ls;
			rv = std::sqrt(rv) / C->ls;
			C->lo = ((R)0.999) * C->lo + ((R)0.001) * lv;
			C->ro = ((R)0.999) * C->ro + ((R)0.001) * rv;
			C->lh = lv - C->lo;
			C->rh = rv - C->ro;
			C->lb = std::tanh(((R)0.5) * C->lb + ((R)0.5) * wd * C->lh);
			C->rb = std::tanh(((R)0.5) * C->rb + ((R)0.5) * wd * C->rh);
			C->buf[j][0] = std::tanh(2 * C->lb);
			C->buf[j][1] = std::tanh(2 * C->rb);
		}
	}
	C->magnitude_lop *= 0.9999f;
	C->magnitude_lop += 0.0001f * magnitude;
	C->phase_lop *= 0.9999f;
	C->phase_lop += 0.0001f * phase;
	if (C->phase % HOP == 0)
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
		// first point is moved
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
		// rest of string moves
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
		C->w = 1 - C->w;
	}
	// play buffer
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
