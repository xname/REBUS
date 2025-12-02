#include <Bela.h>
#include <cmath>
#include <libraries/Scope/Scope.h>
#include <unistd.h>

/*
 _____ _____ _____ _____ _____ 
| __  |   __| __  |  |  |   __|
|    -|   __| __ -|  |  |__   |
|__|__|_____|_____|_____|_____|

 REBUS ThereminBulb
 xname 2025
 emulate a Theremin mapping PHASE to frequency and GAIN to amplitude
 pop value to OUT to control a Light Bulb with SSR (Solid State Relays)
 
 
 Notes: your REBUS will need to have Bulb OUT on pin 1 of Analogue OUT
 Tested with oscilloscope and it is beautiful and still mysterious
 
 */

float gMinPhase = 0.01;
float gMaxPhase = 0.44;
float gMinGain = 0.000467;
float gMaxGain =  0.312997;


float gPhase = 0.0;
int counter = 0;

float bulb = 0.0;

// instantiate the scope
Scope scope;

// BULB out
float kBULBOutPin = 1;

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
		amplitude = constrain(amplitude, -1, 1);
		
		// Convert amplitude to strobify		
		//float bulb = amplitude*1.0001;
		
		if (gainReading >= 0.3)
			bulb = 1;
		else
			bulb = 0;
		
		float out;
		
		out = amplitude * sin(gPhase);
		gPhase += 2.0 * M_PI * frequency / context->audioSampleRate;
		if (gPhase >= M_PI)
			gPhase -= 2.0 * M_PI;
			
		for (unsigned int channel = 0; channel < context->audioOutChannels; channel++){
			audioWrite(context, n, channel, out);
		}
		
		
		if(counter == 800) {
		rt_printf("amplitude is  %f\n", amplitude);
		rt_printf("bulb is  %f\n", bulb);
		rt_printf("gainReading is %f\n", gainReading);
			counter = 0;
		}
		counter++;
		
		
		// bulb = pin 1 out
		if(context->analogOutChannels != 0){
		analogWriteOnce(context, n/2, kBULBOutPin, bulb);
		}
		scope.log(phaseReading, gainReading, frequency, bulb, out);

		
	}
}

void cleanup(BelaContext *context, void *userData)
{

}

