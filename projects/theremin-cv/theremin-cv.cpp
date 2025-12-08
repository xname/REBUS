/*
 _____ _____ _____ _____ _____ 
| __  |   __| __  |  |  |   __|
|    -|   __| __ -|  |  |__   |
|__|__|_____|_____|_____|_____|

 REBUS ThereminCV
 xname 2025
 emulate a Theremin mapping PHASE to frequency and GAIN to amplitude
 pop value to CV
 
 
 Notes: your REBUS will need to have CV OUT on pin 0 of Analogue OUT
 Tested with oscilloscope as well as input to a Moog Subharmonicon
 works particularly well as input to 'cutoff' 


 ported to composition API by Claude 2025-12-02, original version:
 REBUS/compositions/ThereminCV/ThereminCV.cpp

*/

#include <libraries/REBUS/REBUS.h>

//---------------------------------------------------------------------
// this composition does its own mapping from analogRead
// these are the magic numbers

const float gMinPhase = 0.01;
const float gMaxPhase = 0.44;
const float gMinGain = 0.150467;
const float gMaxGain =  0.212997;

//---------------------------------------------------------------------
// this composition sends CV with analogWrite

const int kCVOutPin = 0;

//---------------------------------------------------------------------
// composition name
// printed on startup during setup
// added to audio recording filename if recording is enabled

const char *COMPOSITION_name = "theremin-cv";

//---------------------------------------------------------------------
// composition state
// initialize it in COMPOSITION_setup
// or via C++ default (no-argument) constructor

struct COMPOSITION
{
  int counter;
  float gPhase;
};

//---------------------------------------------------------------------
// called during setup
// after REBUS setup is done

inline
bool COMPOSITION_setup(BelaContext *context, COMPOSITION *C)
{
	// initialize state
	C->counter = 0;
	C->gPhase = 0;
	return true;
}

//---------------------------------------------------------------------
// called once per audio frame (default 44100Hz sample rate)
// store mutable state in the COMPOSITION struct, or in globals

inline
void COMPOSITION_render(BelaContext *context, COMPOSITION *C, int n,
  float audio_out[2], const float audio_in[2], const float _magnitude, const float _phase)
{
		float phaseReading = analogRead(context, n/2, 0);
		float frequency = map(phaseReading, gMinPhase, gMaxPhase, 100, 1000);
		frequency = constrain(frequency, 100.0, 1000.0);
		
		// Convert frequency to scale 0-5v		
		float cv = (frequency / 100.00)/2.0;

		
		float gainReading = analogRead(context, n/2, 4);
		float amplitude = map(gainReading, gMinGain, gMaxGain, 0, 1);
		amplitude = constrain(amplitude, 0, 1);
		
		float out;
		
		out = amplitude * sin(C->gPhase);
		C->gPhase += 2.0 * M_PI * frequency / context->audioSampleRate;
		if (C->gPhase >= M_PI)
			C->gPhase -= 2.0 * M_PI;
			
		for (unsigned int channel = 0; channel < context->audioOutChannels; channel++){
			if (channel < 2){
				audio_out[channel] = out;
			}
			/*
			// can't use audioWrite in composition API
			// for audio channels 0 and 1
			// because they will be overwritten
			// by the REBUS library with the value of audio_out
			// after this function returns
			audioWrite(context, n, channel, out);
			*/
		}
		
		if(C->counter == 800) {
		rt_printf("frequency is  %f\n", frequency);
		rt_printf("cv is  %f\n", cv);
			C->counter = 0;
		}
		C->counter++;
		
		// cv = pin 0 out
		if(context->analogOutChannels != 0){
		analogWriteOnce(context, n/2, kCVOutPin, cv);
		}

		/*
		// can't use scope.log in composition API
		// because the scope is already stolen for
		// phase, gain, audio in l+r, audio out l+r
		scope.log(phaseReading, gainReading, frequency, cv, out);
		*/

}

//---------------------------------------------------------------------
// called during cleanup
// do not free the composition struct, it will be deleted automatically
// (it's possible to do the cleanup in the C++ destructor instead)

inline
void COMPOSITION_cleanup(BelaContext *context, COMPOSITION *C)
{
	// nothing to do for this composition
}

//---------------------------------------------------------------------
// instantiate Bela API with default REBUS implementations
// this should be the last line of code in each composition

REBUS

//---------------------------------------------------------------------
