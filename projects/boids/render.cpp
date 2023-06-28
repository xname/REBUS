//---------------------------------------------------------------------
/*

example with boids
by Claude Heiland-Allen 2023-04-21, 2023-06-14, 2023-06-19, 2023-06-28

For each member of the flock:
a complex oscillator is retriggered
when its volume decays below a threshold.
Magnitude controls decay target.
Phase controls pitch targer.

*/

//---------------------------------------------------------------------
// dependencies

#include <libraries/REBUS/REBUS.h>
#include <complex>
#include "boids3d.h"

//---------------------------------------------------------------------
// added to audio recording filename

const char *COMPOSITION_name = "boids";

//---------------------------------------------------------------------
// composition state

#define COUNT 8

struct COMPOSITION
{
	t_boids *boids;
	std::complex<float> increment[COUNT];
	std::complex<float> oscillator[COUNT];
	std::complex<float> trigger[COUNT];
	float rms[COUNT];
	int n;
};

//---------------------------------------------------------------------
// called during setup

inline
bool COMPOSITION_setup(BelaContext *context, struct COMPOSITION *C)
{
	C->boids = Flock_new(COUNT);
	Flock_flyRect(C->boids, 0, 0, 0, 1, 1, 1);
	Flock_attractPt(C->boids, 0.5, 0.5, 0.5);
	Flock_minSpeed(C->boids, 10.0);
	Flock_maxSpeed(C->boids, 100.0);
	Flock_attractWeight(C->boids, 10.0);
	Flock_resetBoids(C->boids);
	for (int i = 0; i < COUNT; ++i)
	{
		C->increment[i] = 0;
		C->oscillator[i] = 0;
		C->trigger[i] = 0;
		C->rms[i] = 0;
	}
	C->n = 0;
	return true;
}

//---------------------------------------------------------------------
inline
void COMPOSITION_boids(BelaContext *context, struct COMPOSITION *C, float magnitude, float phase)
{
	Flock_attractPt(C->boids, magnitude, phase, 0.5f);
	FlightStep(C->boids);
}

//---------------------------------------------------------------------
// called once per audio frame (default 44100Hz sample rate)

inline
void COMPOSITION_render(BelaContext *context, struct COMPOSITION *C, float out[2], const float in[2], const float magnitude, const float phase)
{
	if (C->n == 0)
	{
		COMPOSITION_boids(context, C, magnitude, phase);
		for (int i = 0; i < COUNT; ++i)
		{
			float gain = constrain(C->boids->boid[i].newPos.x, 0.0f, 1.0f);
			float f = 0.25f * constrain(C->boids->boid[i].newPos.y, 0.0f, 1.0f);
			float t = 16.0f * 2.0f * 3.141592653f * C->boids->boid[i].newPos.z;
			float m = expf(map(gain, 0.0f, 1.0f, logf(0.99f), logf(0.999999f)));
			C->increment[i] = m * std::complex<float>(std::cos(f), std::sin(f));
			C->trigger[i] = 0.5f * std::complex<float>(std::cos(t), std::sin(t));
		}
	}
	if (++C->n >= 256)
	{
		C->n = 0;
	}
  out[0] = 0;
  out[1] = 0;
  for (int i = 0; i < COUNT; ++i)
  {
		C->rms[i] *= 0.99;
		C->rms[i] += 0.01 * std::norm(C->oscillator[i]);
		if (C->rms[i] < 3.0e-3f)
		{
			C->oscillator[i] += C->trigger[i];
		}
		C->oscillator[i] *= C->increment[i];
		out[0] += C->oscillator[i].real();
		out[1] += C->oscillator[i].imag();
	}
	out[0] /= COUNT;
	out[1] /= COUNT;
	out[0] = std::tanh(out[0]);
	out[1] = std::tanh(out[1]);
}

//---------------------------------------------------------------------
// called during cleanup

inline
void COMPOSITION_cleanup(BelaContext *context, struct COMPOSITION *C)
{
	Flock_free(C->boids);
}

//---------------------------------------------------------------------

REBUS

//---------------------------------------------------------------------
