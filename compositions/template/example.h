//---------------------------------------------------------------------
// abstracted composition core
// this file is included by render.cpp

//---------------------------------------------------------------------
// added to audio recording filename

const char *COMPOSITION_name = "example";

//---------------------------------------------------------------------
// dependencies

#include <complex>

//---------------------------------------------------------------------
// composition state

struct COMPOSITION
{
	std::complex<double> increment;
	std::complex<double> oscillator;
	float rms;
};

//---------------------------------------------------------------------
// called during setup

static inline
bool COMPOSITION_setup(BelaContext *context, struct COMPOSITION *C)
{
	C->increment = 0;
	C->oscillator = 0;
	C->rms = 0;
	return true;
}

//---------------------------------------------------------------------
// called once per audio frame (default 44100Hz sample rate)

static inline
void COMPOSITION_render(BelaContext *context, struct COMPOSITION *C, float out[2], const float in[2], const float magnitude, const float phase)
{
	float gain = constrain(magnitude, 0.0f, 1.0f);
	float f = 0.25f * phase;
	float m = expf(map(gain, 0.0f, 1.0f, logf(0.99f), logf(0.999999f)));
	C->increment = double(m) * std::complex<double>(std::cos(f), std::sin(f));
	C->rms *= 0.99;
	C->rms += 0.01 * std::norm(C->oscillator);
	if (C->rms < 3.0e-3f)
	{
		C->oscillator += 0.5;
	}
	C->oscillator *= C->increment;
	out[0] = std::tanh(C->oscillator.real());
	out[1] = std::tanh(C->oscillator.imag());
}

//---------------------------------------------------------------------
// called during cleanup

static inline
void COMPOSITION_cleanup(BelaContext *context, struct COMPOSITION *C)
{
}

//---------------------------------------------------------------------
