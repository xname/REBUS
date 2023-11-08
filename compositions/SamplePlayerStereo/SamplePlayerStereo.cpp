/*
 _____ _____ _____ _____ _____ 
| __  |   __| __  |  |  |   __|
|    -|   __| __ -|  |  |__   |
|__|__|_____|_____|_____|_____|

REBUS Sample Player xname 2020

*/

#include <Bela.h>
#include <cmath>
#include <libraries/AudioFile/AudioFile.h>
#include <libraries/Scope/Scope.h>

// Rebus specific Gain and Phase 
float gMinPhase = 0.01;
float gMaxPhase = 0.44;
float gMidPhase = 0.2;
float gMinGain = 0.150467;
float gMaxGain = 0.299;

std::string gFilename = "myvoice.wav";				// Name of the sound file (in project folder)
std::vector<std::vector<float >> gSampleBuffer;		// Buffer that holds the sound file
int gReadPointer = 0;								// Position of the last frame we played 

float gPhase = 0.0;

// Browser-based oscilloscope
Scope gScope;


bool setup(BelaContext *context, void *userData)
{
	// Load the sample from storage into a buffer
	gSampleBuffer = AudioFileUtilities::load(gFilename);
	
	// Check if the load succeeded
	if(gSampleBuffer.size() == 0) {
    	rt_printf("Error loading audio file '%s'\n", gFilename.c_str());
    	return false;
	}
	
	// Check you have enough sound files for the output channels 
	if(gSampleBuffer.size() < context->audioOutChannels) {
		rt_printf("Audio file '%s' has %d channels, but needs at least %d.\n", gFilename.c_str(), gSampleBuffer.size(), context->audioOutChannels);	
		return false;
	}

    	rt_printf("Loaded the audio file '%s' with %d frames (%.1f seconds) and %d channels\n", 
    			gFilename.c_str(), gSampleBuffer[0].size(),
    			gSampleBuffer[0].size() / context->audioSampleRate,
    			gSampleBuffer.size());
    			
    // Set up the oscilloscope
	gScope.setup(3, context->audioSampleRate);

	return true;
}

void render(BelaContext *context, void *userData)
{
    for(unsigned int n = 0; n < context->audioFrames; n++) {
        // Update the read pointer.
		gReadPointer++;
		if (gReadPointer >= gSampleBuffer[0].size())
			gReadPointer = 0;
			
		// Read Gain and Phase
    	float phaseReading = analogRead(context, n/2, 0);
    	float gainReading = analogRead(context, n/2, 4);

    	
    	//Map the EM waves to your sample
    	float amplitude = map(gainReading, gMinGain, gMaxGain, 0, 1);
		amplitude = constrain(amplitude, 0, 1);
		
 		float frequency = map(phaseReading, gMinPhase, gMaxPhase, 100, 1000);
		frequency = constrain(frequency, 50.0, 1000.0);
 

    	for(unsigned int channel = 0; channel < context->audioOutChannels; channel++) {
			// read a different buffer for each channel
			float out = amplitude * sin(gPhase) * gSampleBuffer[channel][gReadPointer]; //
			
			
			gPhase += 2.0 * M_PI * frequency / context->audioSampleRate;
				if (gPhase >= M_PI)
				gPhase -= 2.0 * M_PI;
			
			// Write the output to the oscilloscope
    		gScope.log(out, gainReading, phaseReading);
			

    		audioWrite(context, n, channel, out);
    		
    	
    	}
    }
}

void cleanup(BelaContext *context, void *userData)
{

}

