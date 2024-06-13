/*
 *
 *
 * 	 _____  _           _   _               _               
 *	|  __ \| |         | | | |             | |              
 *	| |__) | |__  _   _| |_| |__  _ __ ___ | |__  _   _ ___ 
 *	|  _  /| '_ \| | | | __| '_ \| '_ ` _ \| '_ \| | | / __|
 *	| | \ \| | | | |_| | |_| | | | | | | | | |_) | |_| \__ \
 *	|_|  \_\_| |_|\__, |\__|_| |_|_| |_| |_|_.__/ \__,_|___/
 * 	              __/ |                                    
 * 	             |___/                                     
 *			   
 * Template for REBUS controlled sequencer based drum machine
 *  
 *   (C) Eleonora Maria Irene Oreggia 
 *   xname 2023
 * 
 * Drum machine grid inspired by
 * Andrew McPherson, Becky Stewart and Victor Zappi
 * 2015-2019
 * 
 * Rebecca Superbo
 * Rhythmbus: a drum rhythm machine
 * May 2023
 *
 * [[Phase]] timber of filter. 
 *
 * [[Gain]] velocity. 
 *
 * [[Ghost buttons]] turn on or off the different components of the drum machine. 
 *	Ghost button 1 : triggers the samples to play normally or backwards
 *	Ghost button 2 : triggers a fill pattern 
 *
 */


#include <Bela.h>
#include <cmath>
#include "drums.h"
#include <vector>
#include <libraries/Scope/Scope.h>
#include <libraries/AudioFile/AudioFile.h>
#include <libraries/sndfile/sndfile.h>
#include <libraries/OnePole/OnePole.h>
#include <Gpio.h>

//Istantiate the virtual oscilloscope
Scope gScope;

// SENSOR DATA >> EM GAIN and PHASE  
float gMinPhase = 0.01;
float gMaxPhase = 0.44;
float gMidPhase = 0.2;
float gMinGain = 0.150467;
float gMaxGain =  0.3;

// Declare GPIO pins
Gpio phasePin;
Gpio gainPin;

OnePole smootherPhase, smootherAmp;

/* Drum samples are pre-loaded in these buffers. Length of each
 * buffer is given in gDrumSampleBufferLengths.
 */
extern float *gDrumSampleBuffers[NUMBER_OF_DRUMS];
extern int gDrumSampleBufferLengths[NUMBER_OF_DRUMS];

int gIsPlaying = 0;			/* Whether we should play or not. Implemented in Step 4b. */
extern int gIsPlaying;

// Read pointer into the current drum sample buffer.
int gReadPointers[16];	// array of read pointers
int gDrumBufferForReadPointer[16];	// array of values saying which buffer each read pointer corresponds to

/* Patterns indicate which drum(s) should play on which beat.
 * Each element of gPatterns is an array, whose length is given
 * by gPatternLengths.
 */
extern int *gPatterns[NUMBER_OF_PATTERNS];
extern int gPatternLengths[NUMBER_OF_PATTERNS];

/* These variables indicate which pattern we're playing, and
 * where within the pattern we currently are. Used in Step 4c.
 */
int gCurrentPattern = 0;
int gCurrentIndexInPattern = 0;

/* This variable holds the interval between events in **milliseconds**
 * To use it (Step 4a), you will need to work out how many samples
 * it corresponds to (milliseconds * the sample rate = # of samples)
 */
int gEventIntervalMilliseconds = 250;
int gAudioFramesPerAnalogFrame = 0;
int gCounter=0; // counter used in render() function

/* This variable indicates whether samples should be triggered or
 * not. It is used in Step 4b, and should be set in gpio.cpp.
 */
extern int gIsPlaying;

/* This indicates whether we should play the samples backwards.
 * Ghost Button 1: when gPlaysBackwards goes from 0 to 1
 */
int gPlaysBackwards = 0;

/* These variables help implement a fill (temporary pattern).
 * Ghost Button 2: a fill pattern comes in depending on both amplitude and timbre
 */
int gShouldPlayFill = 0;
int gPreviousPattern = 0;

/*further global variables*/

float *audioOutputBuffer;  // Pointer to audio buffer allocated with dynamic memory

bool setup(BelaContext *context, void *userData)
{
	/* Step 1: initialise GPIO pins */
	phasePin.open(0, Gpio::INPUT); // Open the pin as an input
    gainPin.open(4, Gpio::INPUT); // Open the pin as an input
    
    // audioOutputBuffer has to be the same length as the Bela Context's buffer
	// as inside render() buffer information will be copied into it
	audioOutputBuffer = new float[context->audioFrames];
	audioOutputBuffer = 0;	// initialise this buffer to 0
	
	smootherPhase.setup(5, context->audioSampleRate);
	smootherAmp.setup(15, context->audioSampleRate);
	
	/* Useful calculations: audio is sampled twice as often as analog data
	 * The audio sampling rate is 44.1kHz and the analog sampling rate is 22.05kHz
	 * We are processing the analog data and updating it only on every second audio sample, 
	 * since the analog sampling rate is half that of the audio.*/
	if(context->analogFrames)
                gAudioFramesPerAnalogFrame = context->audioFrames / context->analogFrames;

	// This loop sets the index to 0
	for (int i = 0; i < 16; i++) {
		gReadPointers[i] = 0;
		gDrumBufferForReadPointer[i] = -10;  // A value of -10 designates the index is inactive
	}
                
    // setup the scope with 3 channels at the audio sample rate
    gScope.setup(3, context->audioSampleRate);
	
	return true;
}

void render(BelaContext *context, void *userData)
{

    for(unsigned int n = 0; n < context->audioFrames; n++) {
        float out = 0;

        // read GAIN (PIN_4) and PHASE (PIN_0)
        float gainReading = analogRead(context, n/gAudioFramesPerAnalogFrame, 4);
        float phaseReading = analogRead(context, n/gAudioFramesPerAnalogFrame, 0);
        
        /*audio processing code*/

        float amplitude = map(gainReading, gMinGain, gMaxGain, 1, 0); // gain = amplitude and in this case controls velocity
        amplitude = constrain(amplitude, 0, 1);
		amplitude = smootherAmp.process(amplitude);
        int timbre = map(phaseReading, gMinPhase, gMaxPhase, 0, 7); // phase = timbre
		timbre = constrain(timbre, 0, 7);
		//rt_printf("%d", timbre); //debug
		
		/* Step 2: use gReadPointer to play a drum sound */
		/* Step 3: use multiple read pointers to play multiple drums */
		
		// if timbre != 0 then gIsPlaying=1, timbre value from 0 to 7 determines the sample to play
		if(timbre != 0){
			gIsPlaying=1;
			// Ghost Button 1: samples can be played either forward or backwards depending on the amplitude value
			if(amplitude <= 0.5) gPlaysBackwards=1; //play backwards
			else if(amplitude > 0.5) gPlaysBackwards=0; //play forward
			// Ghost Button 2: a fill pattern comes in depending on both amplitude and timbre
			if(amplitude*timbre > 3.5) gShouldPlayFill=1; //fill pattern comes in
			else if (amplitude*timbre <=3.5) gShouldPlayFill=0; // no fill pattern
		}
		else {
			gIsPlaying=0;
		}
	
		/* Step 4: count samples and decide when to trigger the next event */
		
		gEventIntervalMilliseconds=map(amplitude, 0,1,50,1000);
		gCounter++;
		if(gCounter >= (gEventIntervalMilliseconds*context->audioSampleRate)/1000){
			startNextEvent(gIsPlaying, timbre);
			// Reset counter after user defined number of samples
			gCounter=0;
		}
		
		for (int i = 0; i < 16; i++) {
   		// Check if the drum buffer at index i is inactive
	   		if (gDrumBufferForReadPointer[i] != -10) {
	   			// If it is, copy the contents of the drum buffer at index gDrumBufferForReadPointer[i] to the secondary audio buffer audioOutputBuffer
	   			audioOutputBuffer = gDrumSampleBuffers[gDrumBufferForReadPointer[i]];
	   			if (gPlaysBackwards == 0) {
	   				// If we are not playing the sample backwards, read through the samples in audioOutputBuffer with the read pointer at index i,
	   				// and add them to out through compound addition
		  			out += amplitude * audioOutputBuffer[gReadPointers[i]++];
		  			// If the read pointer reaches the end of the buffer, indicate gDrumBufferForReadPointer[i] is no longer active
		  			if (gReadPointers[i] >= gDrumSampleBufferLengths[gDrumBufferForReadPointer[i]]) {
		  				gDrumBufferForReadPointer[i] = -10;
		   			}
	   			} else if (gPlaysBackwards == 1) {
	   				// If we are playing the sample backwards, count through the samples in the buffer negatively
	   				out += amplitude * audioOutputBuffer[gReadPointers[i]--];
	   				// If the read pointer reaches 0 or goes beyond it, indicate gDrumBufferForReadPointer[i] is no longer active
	   				if (gReadPointers[i] <= 0) {
		   				gDrumBufferForReadPointer[i] = -10;
		   			}
	   			}	
	   		}
		}
		
	   	for(unsigned int channel = 0; channel < context->audioOutChannels; channel++) {
			// Write the sample to every audio output channel
	    	audioWrite(context, n, channel, out);
	  	}
	  	
	  	// send EM and audio data to Scope
        gScope.log(gainReading, phaseReading, out);
	}
}

/* Start playing a particular drum sound given by drumIndex.*/
void startPlayingDrum(int drumIndex) {
	/* Steps 3a and 3b */
	// Read through all 16 indexes in the gReadPointers and gDrumBufferForReadPointer arrays
	for (int i = 0; i < 16; i++) {
		if (gPlaysBackwards == 0) { //samples playing forward
			// check that the gDrumBufferForReadPointer at index i is inactive,
			// and if it is, assign the drum index passed as an argument into the startPlayingDrum() function to it and reset gReadPointers[i]
			if (gDrumBufferForReadPointer[i] == -10) {
				gDrumBufferForReadPointer[i] = drumIndex;
				gReadPointers[i] = 0;
				break;
			}
		} else if (gPlaysBackwards == 1) { // samples playing backwards
			// check that the gDrumBufferForReadPointer at index i is inactive,
			// and if it is, assign the drum index passed as an argument into the startPlayingDrum() function to it and set gReadPointers[i]
			// to length of the drum buffer (so it can start playing backwards)
			if (gDrumBufferForReadPointer[i] == -10) {
				gDrumBufferForReadPointer[i] = drumIndex;
				gReadPointers[i] = gDrumSampleBufferLengths[gDrumBufferForReadPointer[i]];
				break;
			}
		}	
	}
}

/* Start playing the next event in the pattern */
void startNextEvent(int toggle, int timbre) {
	/* Step 4 */
	if (toggle == 1) {
		// If gIsPlaying is 1 and passed as an argument into startNextEvent, iterate through number of drum buffers
		for (int i = 0; i < NUMBER_OF_DRUMS-1; i++) {
			// If the eventContainsDrum() function (at specific index of specific drum pattern, in relation to drum at index i) returns
			// a 1, call the startPlayingDrum() function at that index indicated by the value of timbre
			if (eventContainsDrum(gPatterns[gCurrentPattern][gCurrentIndexInPattern], i) == 1) {
				startPlayingDrum(timbre);
			}
		}
		
		// Modulo arithmetic ensures the pattern index wraps around the length of the pattern, hence staying within it
		gCurrentIndexInPattern = (gCurrentIndexInPattern + 1 + gPatternLengths[gCurrentPattern]) % gPatternLengths[gCurrentPattern];
		
		// If modulo is 0 (i.e. the end of pattern has been reached) set the current index in pattern to 0, and if that pattern was a
		// pattern, also set gShouldPlayFill to 0 and reinstate the pattern that was playing before the fill
		if (gCurrentIndexInPattern == 0) {
			if (gCurrentPattern == FILL_PATTERN) {
				gShouldPlayFill = 0;
				gCurrentPattern = gPreviousPattern;
			}
			gCurrentIndexInPattern = 0;
		}
	}	
}

/* Returns whether the given event contains the given drum sound */
int eventContainsDrum(int event, int drum) {
	if(event & (1 << drum))
		return 1;
	return 0;
}

void cleanup(BelaContext *context, void *userData)
{
	delete[] audioOutputBuffer;
}
