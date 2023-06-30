/*
	 _____ _____ _____ _____ _____ 
	| __  |   __| __  |  |  |   __|
	|    -|   __| __ -|  |  |__   |
	|__|__|_____|_____|_____|_____|

	REBUS Rhythm0
 	xname 2019
 	(50.0 < f1 < 55.0) 
 	f2 = 1 + f1;
 	these frequencies can let objects vibrate in a room
	ch1 is different from ch2, pressure waves propagate horizontally
 
*/

#include <Bela.h>
#include <cmath>
#include <Scope.h>
#include <unistd.h>

float gMinPhase = 0.01;
float gMaxPhase = 0.44;

float gMinGain = 0.150467;
float gMaxGain =  0.3;

float gPhase = 0;
float gPhase2 = 0;

int counter = 0;

Scope scope;

bool setup(BelaContext *context, void *userData)
{
	// tell the scope how many channels and the sample rate
	scope.setup(4, context->audioSampleRate);
	return true;
}

void render(BelaContext *context, void *userData)
{
	for(unsigned int n = 0; n < context->audioFrames; n++) {
		float phaseReading = analogRead(context, n/2, 0);
		float gainReading = analogRead(context, n/2, 4);
		
		float f1 = map(phaseReading, gMinPhase, gMaxPhase, 50, 55);
		f1 = constrain(f1, 50.0, 55.0);

		float f2 = 1 + f1;
		
		float amplitude = map(gainReading, gMinGain, gMaxGain, 0.1, 0.99);
		amplitude = constrain(amplitude, 0.1, 0.99);

/*		if(counter == 1500) {
		rt_printf("gainReading is  %f\n", gainReading);
		rt_printf("amplitude is  %f\n", amplitude);
		rt_printf("phaseReading is  %f\n", phaseReading);
		rt_printf("f1 is  %f\n", f1);
	    	rt_printf("f2 is  %f\n", f2);
		rt_printf("gPhase is  %f\n", gPhase);
	    	rt_printf("gPhase2 is  %f\n", gPhase2);
	    	rt_printf("amplitude is  %f\n", amplitude);
			counter = 0;
		}
		counter++;
*/		
		
		float out1 =  amplitude * sin(gPhase);
		gPhase += 2.0 * M_PI * f1 / context->audioSampleRate;
		if (gPhase >= M_PI)
			gPhase -= 2.0 * M_PI;

		float out2 =  amplitude * sin(gPhase2);
		gPhase2 += 2.0 * M_PI * f2 / context->audioSampleRate;
		if (gPhase2 >= M_PI)
			gPhase2 -= 2.0 * M_PI;	

		scope.log(gainReading, phaseReading, out1, out2);			
						
		for (unsigned int channel = 0; channel < context->audioOutChannels; channel++){
			audioWrite(context, n, 0, out1);
		    audioWrite(context, n, 1, out2);
			
		}
		
	}
}

void cleanup(BelaContext *context, void *userData)
{

}
