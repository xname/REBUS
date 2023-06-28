#include <Bela.h>
#include <cmath>
#include <libraries/Scope/Scope.h>
#include <unistd.h>

/*
 _____ _____ _____ _____ _____ 
| __  |   __| __  |  |  |   __|
|    -|   __| __ -|  |  |__   |
|__|__|_____|_____|_____|_____|

 REBUS Pitch2
 xname 2019
 emulate a Theremin mapping PHASE to frequency and GAIN to amplitude
 
 */

float gMinPhase = 0.01;
float gMaxPhase = 0.44;
float gMinGain = 0.150467;
float gMaxGain =  0.212997;


float gPhase = 0;
int counter = 0;

// instantiate the scope
Scope scope;

bool setup(BelaContext *context, void *userData)
{
	// tell the scope how many channels and the sample rate
	scope.setup(3, context->audioSampleRate);
	return true;
}

void render(BelaContext *context, void *userData)
{
	for(unsigned int n = 0; n < context->audioFrames; n++) {
		float phaseReading = analogRead(context, n/2, 0);
		float frequency = map(phaseReading, gMinPhase, gMaxPhase, 100, 1000);
		frequency = constrain(frequency, 100.0, 1000.0);
		
		float gainReading = analogRead(context, n/2, 4);
		float amplitude = map(gainReading, gMinGain, gMaxGain, 0, 1);
		amplitude = constrain(amplitude, 0, 1);

		if(counter == 500) {
		rt_printf("gainReading is  %f\n", gainReading);
		rt_printf("amplitude is  %f\n", amplitude);
		rt_printf("phaseReading is  %f\n", phaseReading);
		rt_printf("frequency is  %f\n", frequency);
			counter = 0;
		}
		counter++;

		float out;

		scope.log(gainReading, phaseReading, out);		

		float out = amplitude * sin(gPhase);
		gPhase += 2.0 * M_PI * frequency / context->audioSampleRate;
		if (gPhase >= M_PI)
			gPhase -= 2.0 * M_PI;
			
			
		for (unsigned int channel = 0; channel < context->audioOutChannels; channel++){
			audioWrite(context, n, channel, out);
		}
		
	}
}

void cleanup(BelaContext *context, void *userData)
{

}

