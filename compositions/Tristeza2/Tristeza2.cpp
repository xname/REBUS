/*
 REBUS Tristeza2
 xname 2019
 emulate a Theremin mapping PHASE to frequency and GAIN to amplitude
 two f2 and f3 are derived from f1
 f2 = f1*(1+gPhase);
 f3 = f2*(1-gPhase);
 out = amp * sin(f2) + amp * sin(f3)
 the phase is updated and recalculated at each cycle using f1
*/

#include <Bela.h>
#include <cmath>
#include <Scope.h>
#include <unistd.h>

float gMinPhase = 0.0;
float gMaxPhase = 0.44;

float gMinGain = 0.196893;
float gMaxGain =  0.212997;

float gPhase = 0.0;
float gPhase2 = 0.0;

int counter = 0;

// instantiate the scope
Scope scope;

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
		
		float gainReading = analogRead(context, n/2, 4);
		float amplitude = map(gainReading, gMinGain, gMaxGain, 0, 1);
		amplitude = constrain(amplitude, 0, 1);
		          
		float frequency2 = frequency*(1+gPhase);
		float frequency3 = frequency*(1-gPhase);

		if(counter == 900) {
		rt_printf("gainReading is  %f\n", gainReading);
		rt_printf("amplitude is  %f\n", amplitude);
		rt_printf("phaseReading is  %f\n", phaseReading);
		rt_printf("frequency is  %f\n", frequency);
		rt_printf("frequency2 is  %f\n", frequency2);
			counter = 0;
		}
		counter++;
		
		float out2 = amplitude * sin(frequency2);
		float out3 = amplitude * sin(frequency3);
		
		float out = out2 + out3;
		
		scope.log(gainReading, phaseReading, out2, out3, out);
		
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
