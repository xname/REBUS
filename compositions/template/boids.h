//---------------------------------------------------------------------
/*

untitled example with boids
by Claude Heiland-Allen 2023-04-21, 2023-06-14, 2023-06-19

For each member of the flock:
a complex oscillator is retriggered
when its volume decays below a threshold.
Magnitude controls decay target.
Phase controls pitch targer.

*/

//---------------------------------------------------------------------
// abstracted composition core
// this file is included by render.cpp

//---------------------------------------------------------------------
// added to audio recording filename

const char *COMPOSITION_name = "boids";

//---------------------------------------------------------------------
// dependencies

#include <complex>

//---------------------------------------------------------------------
// composition state

#define COUNT 16

struct COMPOSITION
{
	std::complex<float> velocity[COUNT];
	std::complex<float> position[COUNT];
	std::complex<float> increment[COUNT];
	std::complex<float> oscillator[COUNT];
	float rms[COUNT];
	int n;
};

//---------------------------------------------------------------------
// called during setup

static inline
bool COMPOSITION_setup(BelaContext *context, struct COMPOSITION *C)
{
	for (int i = 0; i < COUNT; ++i)
	{
		C->position[i] = std::complex<float>(rand() / (float) RAND_MAX, rand() / (float) RAND_MAX);
		C->velocity[i] = std::complex<float>(rand() / (float) RAND_MAX - 0.5, rand() / (float) RAND_MAX - 0.5);
		C->increment[i] = 0;
		C->oscillator[i] = 0;
		C->rms[i] = 0;
	}
	C->n = 0;
	return true;
}

#define kFriction 0.99f
#define kVelocity 0.1f
#define kPosition 0.5f
#define kAvoidance 1.0f
#define dt 0.01f

//---------------------------------------------------------------------
static inline
void COMPOSITION_boids(BelaContext *context, struct COMPOSITION *C, float magnitude, float phase)
{
	std::complex<float> position = std::complex<float>(magnitude, phase);
	std::complex<float> velocity = 0;
	for (int i = 0; i < COUNT; ++i)
	{
		velocity += C->velocity[i];
	}
	velocity /= COUNT;
	for (int i = 0; i < COUNT; ++i)
	{
		complex<float> target = C->velocity[i];
		target *= kFriction;
		target += kVelocity * (velocity - C->velocity[i]);
		target += kPosition * (position - C->position[i]);
		for (int j = 0; j < COUNT; ++j)
		{
			if (i == j)
			{
				continue;
			}
			std::complex<float> d = C->position[i] - C->position[j];
			if (std::norm(d) < 0.000001f)
			{
				target += kAvoidance * d / std::norm(d);
			}
		}
		C->velocity[i] *= 0.9;
		C->velocity[i] += 0.1 * target;
	}
	for (int i = 0; i < COUNT; ++i)
	{
		C->position[i] += C->velocity[i] * dt;
	}
}

//---------------------------------------------------------------------
// called once per audio frame (default 44100Hz sample rate)

static inline
void COMPOSITION_render(BelaContext *context, struct COMPOSITION *C, float out[2], const float in[2], const float magnitude, const float phase)
{
	if (C->n == 0)
	{
		COMPOSITION_boids(context, C, magnitude, phase);
		for (int i = 0; i < COUNT; ++i)
		{
			float gain = constrain(C->position[i].real(), 0.0f, 1.0f);
			float f = 0.25f * C->position[i].imag();
			float m = expf(map(gain, 0.0f, 1.0f, logf(0.99f), logf(0.999999f)));
			C->increment[i] = m * std::complex<float>(std::cos(f), std::sin(f));
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
			C->oscillator[i] += 0.5;
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

static inline
void COMPOSITION_cleanup(BelaContext *context, struct COMPOSITION *C)
{
}

//---------------------------------------------------------------------
