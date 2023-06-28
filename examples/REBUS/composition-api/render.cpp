//---------------------------------------------------------------------
/*

REBUS - Electromagnetic Interactions

https://xname.cc/rebus

composition-api example
by Claude Heiland-Allen 2023-04-21, 2023-06-14, 2023-06-28

A complex oscillator is retriggered
when its volume decays below a threshold.
Magnitude controls decay.
Phase controls pitch.

For use on REBUS, nothing needs changing.

For use on Bela with GUI (for development/testing purposes):
Settings->Make Parameters: CPPFLAGS="-DMODE=1"

*/

//---------------------------------------------------------------------
// The REBUS composition API abstracts some of the repetitive code
// that would otherwise be duplicated across compositions.

// recordings take 64MB/min, 1GB/15mins
// uncomment the next line to disable recording
// #define RECORD 0

#include <libraries/REBUS/REBUS.h>

//---------------------------------------------------------------------
// additional dependencies of this composition

#include <complex>

//---------------------------------------------------------------------
// composition name
// added to audio recording filename if recording is enabled

const char *COMPOSITION_name = "composition-api";

//---------------------------------------------------------------------
// composition state
// initialize it in COMPOSITION_setup
// or via C++ default (no-argument) constructor

struct COMPOSITION
{
	std::complex<double> increment;
	std::complex<double> oscillator;
	float rms;
};

//---------------------------------------------------------------------
// called during setup
// after REBUS setup is done

inline
bool COMPOSITION_setup(BelaContext *context, COMPOSITION *C)
{
	C->increment = 0;
	C->oscillator = 0;
	C->rms = 0;
	return true;
}

//---------------------------------------------------------------------
// called once per audio frame (default 44100Hz sample rate)
// store mutable state in the COMPOSITION struct, or in globals

inline
void COMPOSITION_render(BelaContext *context, COMPOSITION *C,
  float out[2], const float in[2], const float magnitude, const float phase)
{
	float gain = constrain(magnitude, 0.0f, 1.0f);
	float f = 0.25f * phase;
	float m = expf(map(gain, 0.0f, 1.0f, logf(0.99f), logf(0.999999f)));
	C->increment = double(m) * std::complex<double>(cosf(f), sinf(f));
	C->rms *= 0.99;
	C->rms += 0.01 * std::norm(C->oscillator);
	if (C->rms < 3.0e-3f)
	{
		C->oscillator += 0.5;
	}
	C->oscillator *= C->increment;
	out[0] = tanhf(C->oscillator.real());
	out[1] = tanhf(C->oscillator.imag());
}

//---------------------------------------------------------------------
// called during cleanup
// do not free the composition struct, it will be deleted automatically

inline
void COMPOSITION_cleanup(BelaContext *context, COMPOSITION *C)
{
}

//---------------------------------------------------------------------
// instantiate Bela API with default REBUS implementations

REBUS

//---------------------------------------------------------------------
