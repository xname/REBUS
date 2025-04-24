#include <Bela.h>
#include <cmath>
#include <libraries/Scope/Scope.h>
#include <unistd.h>

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
 
 */

float gMinPhase = 0.01;
float gMaxPhase = 0.44;
float gMinGain = 0.150467;
float gMaxGain =  0.212997;


float gPhase = 0;
int counter = 0;

// instantiate the scope
Scope scope;

// CV out
float kCVOutPin = 0;

bool setup(BelaContext *context, void *userData)
{
	// tell the scope how many channels and the sample rate
	scope.setup(5, context->audioSampleRate);
	return true;
}

void render(BelaContext *context, void *userData)
{
	for(unsigned int n = 0; n < context->audioFrames; n++) {
		float phaseReading = analogRead(context, n/2, 0);
		float frequency = map(phaseReading, gMinPhase, gMaxPhase, 100, 1000);
		frequency = constrain(frequency, 100.0, 1000.0);
		
		// Convert frequency to scale 0-5v		
		float cv = (frequency / 100.00)/2.0;

		
		float gainReading = analogRead(context, n/2, 4);
		float amplitude = map(gainReading, gMinGain, gMaxGain, 0, 1);
		amplitude = constrain(amplitude, 0, 1);
		
		float out;
		
		out = amplitude * sin(gPhase);
		gPhase += 2.0 * M_PI * frequency / context->audioSampleRate;
		if (gPhase >= M_PI)
			gPhase -= 2.0 * M_PI;
			
		for (unsigned int channel = 0; channel < context->audioOutChannels; channel++){
			audioWrite(context, n, channel, out);
		}
		
		
		if(counter == 800) {
		rt_printf("frequency is  %f\n", frequency);
		rt_printf("cv is  %f\n", cv);
			counter = 0;
		}
		counter++;
		
		// cv = pin 0 out
		if(context->analogOutChannels != 0){
		analogWriteOnce(context, n/2, kCVOutPin, cv);
		}
		scope.log(phaseReading, gainReading, frequency, cv, out);

		
	}
}

void cleanup(BelaContext *context, void *userData)
{

}

