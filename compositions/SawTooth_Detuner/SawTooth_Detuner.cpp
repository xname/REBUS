#include <Bela.h>
#include <cmath>
#include <unistd.h>
#include <libraries/Scope/Scope.h>

/*
 _____ _____ _____ _____ _____
| __  |   __| __  |  |  |   __|
|    -|   __| __ -|  |  |__   |
|__|__|_____|_____|_____|_____|
 
 REBUS SawTooth_Detuner
 xname 2019
 
*/


const int gSampleBufferLength = 512;		// Buffer length in frames
float gSampleBuffer[gSampleBufferLength];	// Buffer that holds the wavetable
float gReadPointers[2] = {0};	// Position of the last frame we played 

// Analogue in
float gMinPhase = 0.0;
float gMaxPhase = 0.44;
float gMinGain = 0.150467;
float gMaxGain =  0.212997;

// The Scope
Scope scope;

// Initialise Counter
int counter = 0;

bool setup(BelaContext *context, void *userData)
{
	// Generate a sawtooth waveform (a ramp from -1 to 1) and store it in the buffer. 
	for(unsigned int n = 0; n < gSampleBufferLength; n++) {
		gSampleBuffer[n] = -1.0 + 2.0 * (float)n / (float)(gSampleBufferLength - 1);
	}
	
	//Setup the scope
	scope.setup(4, context->audioSampleRate);

	return true;
}

void render(BelaContext *context, void *userData)
{
	
    for(unsigned int n = 0; n < context->audioFrames; n++) {
    	float out = 0.0;
    	
    	for (unsigned int oscillator = 0; oscillator < 2; oscillator++) {
	    	// The pointer will take a fractional index. Look for the sample on
	    	// either side which are indices we can actually read into the buffer.
	    	// If we get to the end of the buffer, wrap around to 0.
	    	
	    	float phaseReading = analogRead(context, n/2, 0);
			float frequency = map(phaseReading, gMinPhase, gMaxPhase, 40, 600);
			frequency = constrain(frequency, 40, 600);
		
			float gainReading = analogRead(context, n/2, 4);
			float amplitude = map(gainReading, gMinGain, gMaxGain, 0, 1);
			amplitude = constrain(amplitude, 0, 1);
			
			float detuning = 0.005+frequency;
			detuning *= 0.001;
			if (detuning >= 0.5)
				detuning = 0;
				
    		float frequencies[2];
			frequencies[0] = frequency*(1+detuning);
			frequencies[1] = frequency*(1-detuning);
			
			float frequency0 = frequencies[0];
			float frequency1 = frequencies[1];
			
			if(counter == 900) {
				rt_printf("detuning is  %f\n", detuning);
				rt_printf("frequency0 is  %f\n", frequency0);
				rt_printf("frequencies1 is  %f\n", frequency1);
					counter = 0;
			}
			counter++;
			
	    	int indexBelow = floorf(gReadPointers[oscillator]);
	    	int indexAbove = indexBelow + 1;
	    	if(indexAbove >= gSampleBufferLength)
	    		indexAbove = 0;
	    	
	    	
	    	scope.log(gainReading, phaseReading, detuning, out);
	    	
	    	// Linear interpolation: weight each sample, the closer to zero, 
	    	// the more weight to the "below" sample. The closer the fractional
	    	// part is to 1, the more weight to sample "above"
	    	float fractionAbove = gReadPointers[oscillator] - indexBelow;
	    	float fractionBelow = 1.0 - fractionAbove;
	    
    		// Calculate the weighted average of the "below" and "above" samples
    		// Mix this into the output
	        out += amplitude * (fractionBelow * gSampleBuffer[indexBelow] +
        						fractionAbove * gSampleBuffer[indexAbove]);
 
 			// Update the read pointer according to the frequency		
	        gReadPointers[oscillator] += gSampleBufferLength * frequencies[oscillator] / context->audioSampleRate;
	        while(gReadPointers[oscillator] >= gSampleBufferLength)
	            gReadPointers[oscillator] -= gSampleBufferLength;       
    	}
            
    	for(unsigned int channel = 0; channel < context->audioOutChannels; channel++) {
			// Write the sample to every audio output channel
    		audioWrite(context, n, channel, out);
    	}
    }
}

void cleanup(BelaContext *context, void *userData)
{

}

