#include <Bela.h>
#include <cmath>
#include <libraries/Scope/Scope.h>
#include <unistd.h>

/*
  _____ _____ _____ _____ _____ 
| __  |   __| __  |  |  |   __|
|    -|   __| __ -|  |  |__   |
|__|__|_____|_____|_____|_____|
 
 REBUS Pitch3
 xname 2019
 emulate a Theremin mapping PHASE to frequency and GAIN to amplitude
 summing two sinewaves where f2 = f1 * 10.1 (within range)
 they are out of phase of a 0.2 factor and  
 and a2 = a1 * 1.1 (within range)
 
 */
 
float gMinPhase = 0.0;
float gMaxPhase = 0.44;

float gMinGain = 0.150467;
float gMaxGain =  0.212997;

// phase difference is 0.2 * 360 = 72 degrees
float gPhase = 0.0;
float gPhase2 = 0.2;

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
		
		float frequency2 = frequency*10.01;
		
		float gainReading = analogRead(context, n/2, 4);
		float amplitude = map(gainReading, gMinGain, gMaxGain, 0, 1);
		amplitude = constrain(amplitude, 0, 1);
		          
		float amplitude2 = amplitude * 1.1;
		
		if (amplitude2 >1)
			amplitude2 = 0;

		if(counter == 900) {
		rt_printf("gainReading is  %f\n", gainReading);
		rt_printf("amplitude is  %f\n", amplitude);
		rt_printf("phaseReading is  %f\n", phaseReading);
		rt_printf("frequency is  %f\n", frequency);
		rt_printf("amplitude2 is %f\n", amplitude2);
			counter = 0;
		}
		counter++;
		
		float out = amplitude * sin(gPhase) + amplitude2 * sin(gPhase2);
		
		scope.log(gainReading, phaseReading, out);
		
		gPhase += 2.0 * M_PI * frequency / context->audioSampleRate;
		if (gPhase >= M_PI)
			gPhase -= 2.0 * M_PI;
			
		gPhase2 += 2.0 * M_PI * frequency2 / context->audioSampleRate;
		if (gPhase2 >= M_PI)
			gPhase2 -= 2.0 * M_PI;
			
		
		for (unsigned int channel = 0; channel < context->audioOutChannels; channel++){
			audioWrite(context, n, channel, out);
		}
		
	}
}

void cleanup(BelaContext *context, void *userData)
{

}

