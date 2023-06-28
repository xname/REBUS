/*
  _____ _____ _____ _____ _____ 
 | __  |   __| __  |  |  |   __|
 |    -|   __| __ -|  |  |__   |
 |__|__|_____|_____|_____|_____|
 
 REBUS - Electromagnetic Interactions 

 Workshop Template = CODE SKELETHON
 https://xname.cc/rebus
 xname 2023

 https://xname.cc/rebus 

 Originally created for a workshop helt at Goldsmiths University of London on 21 April 2023

	There’s no escape from Him.
	He’s so high you can’t get over Him.
	He’s so low you can’t get under Him.
	He’s so wide you can’t get around Him.
	If you make your bed in Heaven He’s there. 
	If you make your bed in Hell He’s there. 
	He’s everywhere.

		Eno and Byrne (1981)

*/

#include <Bela.h>
#include <libraries/Scope/Scope.h>
#include <cmath>
#include <vector>

//Istantiate the virtual oscilloscope
Scope gScope;

// SENSOR DATA >> EM GAIN and PHASE  
float gMinPhase = 0.01;
float gMaxPhase = 0.44;
float gMidPhase = 0.2;
float gMinGain = 0.150467;
float gMaxGain =  0.3;

bool setup(BelaContext *context, void *userData)
{

	// Set up the oscilloscope (n channels and samplerate)
	gScope.setup(3, context->audioSampleRate);
	return true;
}


void render(BelaContext *context, void *userData)
{
    for(unsigned int n = 0; n < context->audioFrames; n++) {
    	float out = 0;

    	// read GAIN (PIN_4) and PHASE (PIN_0)
    	float gainReading = analogRead(context, n/2, 4);
			float phaseReading = analogRead(context, n/2, 0);

		/* Mapping example 
		
			// map EM phase to frequency and keep it in range
			float frequency = map(phaseReading, gMinPhase, gMaxPhase, 100, 1000);
			frequency = constrain(frequency, 100.0, 1000.0);

			// map EM gain to amplitude and keep it in range		
			float amplitude = map(gainReading, gMinGain, gMaxGain, 0, 1);
			amplitude = constrain(amplitude, 0, 1); 

		*/	

      // use console to check kabbalah
			if(counter == 900) {
			rt_printf("gainReading is  %f\n", gainReading);
			//rt_printf("amplitude is  %f\n", amplitude);
			rt_printf("phaseReading is  %f\n", phaseReading);
			//rt_printf("frequency is  %f\n", frequency);
				counter = 0;
			}
			counter++;

			// send EM and audio data to Scope
			gScope.log(gainReading, phaseReading, out);

 			// your code here

			// Write the sample to every audio output channel            
    	for(unsigned int channel = 0; channel < context->audioOutChannels; channel++) {
    		audioWrite(context, n, channel, out);
    	}
}


void cleanup(BelaContext *context, void *userData)
{
	
}



