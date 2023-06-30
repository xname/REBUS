/*
	 _____ _____ _____ _____ _____ 
	| __  |   __| __  |  |  |   __|
	|    -|   __| __ -|  |  |__   |
	|__|__|_____|_____|_____|_____|

	Tremolo Techno to forget the pain of life  
	xname >> July 2020 

	A tremolo effect is a simple type of amplitude modulation where the amplitude of one signal 
	is continuously modulated by the amplitude of another by multiplying the two signals together.
	The carrier signal is the audio input, the modulator is the low frequency oscillator (LFO), 
	in this case a generated sinetone, which is produced by incrementing the phase of a sine 
	function on every audio frame. The frequency of the sinetone is determined by the phase of 
	the electromagnetic signal, the amplitude is mapped to its gain.

	Inspired by the tremolo effect from the Bela library.

*/

#include <Bela.h>
#include <cmath>
#include <algorithm>
#include <libraries/Scope/Scope.h>

int gAudioChannelNum; // number of audio channels to iterate over
float gPhase;
float gInverseSampleRate;
int gAudioFramesPerAnalogFrame = 0;


// Browser-based oscilloscope
Scope gScope;

// Rebus specific Gain and Phase 
float gMinPhase = 0.01;
float gMaxPhase = 0.44;
float gMidPhase = 0.2;
float gMinGain = 0.150467;
float gMaxGain =  0.3;


bool setup(BelaContext *context, void *userData)
{
	// If the amout of audio input and output channels is not the same
	// we will use the minimum between input and output
	gAudioChannelNum = std::min(context->audioInChannels, context->audioOutChannels);
	
	// Check that we have the same number of inputs and outputs.
	if(context->audioInChannels != context->audioOutChannels){
		printf("Different number of audio outputs and inputs available. Using %d channels.\n", gAudioChannelNum);
	}

	if(context->analogSampleRate > context->audioSampleRate)
	{
		fprintf(stderr, "Error: for this project the sampling rate of the analog inputs has to be <= the audio sample rate\n");
		return false;
	}

	if(context->audioFrames)
		gAudioFramesPerAnalogFrame = context->audioFrames / context->analogFrames;
	gInverseSampleRate = 1.0 / context->audioSampleRate;
	gPhase = 0.0;
	
	// Set up the oscilloscope
	gScope.setup(5, context->audioSampleRate);
	
	return true;
}

void render(BelaContext *context, void *userData)
{
	// Nested for loops for audio channels
	for(unsigned int n = 0; n < context->audioFrames; n++) {

    	// read Gain and Phase
    	float phaseReading = analogRead(context, n/2, 0);
    	float gainReading = analogRead(context, n/2, 4);
    	
    	float amplitude = map(gainReading, gMinGain, gMaxGain, 0, 1);
		amplitude = constrain(amplitude, 0, 1);
		
		 float frequency = map(phaseReading, gMinPhase, gMaxPhase, 100, 1000);
		frequency = constrain(frequency, 50.0, 1000.0);

		// Generate a sinewave with frequency set by gFrequency
		// and amplitude from -0.5 to 0.5
		float lfo = sinf(gPhase) * 0.5f;
		// Keep track and wrap the phase of the sinewave
		gPhase += 2.0f * (float)M_PI * frequency * gInverseSampleRate;
		if(gPhase > M_PI)
			gPhase -= 2.0f * (float)M_PI;

		for(unsigned int channel = 0; channel < gAudioChannelNum; channel++) {
			// Read the audio input and half the amplitude
			float input = audioRead(context, n, channel) * 0.5f;
			
			float out = (input*lfo) * amplitude;
			
			// Write to audio output the audio input multiplied by the sinewave
			audioWrite(context, n, channel, out);
			
		// Write the output to the oscilloscope
		gScope.log(lfo, input, out,  gainReading, phaseReading); 

		}
	}
}

void cleanup(BelaContext *context, void *userData)
{

}


