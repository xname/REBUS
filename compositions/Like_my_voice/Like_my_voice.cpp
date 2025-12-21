/* xname 2019 
	same pitch as my voice */

#include <Bela.h>
#include <cmath>
#include <libraries/Scope/Scope.h>
#include <unistd.h>

float gMinPhase = 0.01;
float gMaxPhase = 0.44;

float gMinGain = 0.150467;
float gMaxGain =  0.338852;

//float gMinGain = 0.3;
//float gMaxGain = 1.4;

float gPhase = 0;
float gPhase2 = 0.2;
int counter = 0;

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
		float f1 = map(phaseReading, gMinPhase, gMaxPhase, 100, 110);
		f1 = constrain(f1, 100.0, 110.0); //try also 95.0, 100.0);
		
		float f2 = (f1 - 10);
		
		float gainReading = analogRead(context, n/2, 4);
		float amplitude = map(gainReading, gMinGain, gMaxGain, 0.2, 0.9);
		amplitude = constrain(amplitude, 0, 1);
		
		if(counter == 1500) {
		rt_printf("gainReading is  %f\n", gainReading);
		rt_printf("amplitude is  %f\n", amplitude);
		rt_printf("phaseReading is  %f\n", phaseReading);
		rt_printf("f1 is  %f\n", f1);
	    rt_printf("f2 is  %f\n", f2);
			counter = 0;
		}
		counter++;
		

		// float fbeat = (f1 - f2)/2; => uncomment this and the fbeat below to make it a bit more acid drone
		
		float out1 = amplitude * sin(gPhase);
		gPhase += 2.0 * M_PI * f1 / context->audioSampleRate;
		if (gPhase >= M_PI)
			gPhase -= 2.0 * M_PI;

		float out2 = amplitude * sin(gPhase2);
		gPhase += 2.0 * M_PI * f2 / context->audioSampleRate;
		if (gPhase2 >= M_PI)
			gPhase2 -= 2.0 * M_PI;	
			
		float out = (out1 - out2); // * fbeat; 	=> uncomment this and the above for less of my voice and more acid drone ;P 
		
		scope.log(gainReading, phaseReading);
						
		for (unsigned int channel = 0; channel < context->audioOutChannels; channel++){
			audioWrite(context, n, channel, out);

			
		}
		
	}
}

void cleanup(BelaContext *context, void *userData)
{

}

