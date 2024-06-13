/*
	
	xname 2023
	
	Generatibus v.0
	
	Template, Concept and Mapping
	
	Adaptation from 
	
	Niccolò Perego
	Arpeggiator
	
	notes of the arpeggiated chord can be added/removed with 1/0 in gNoteSet. each slot in this vector represents a semitone above the base frequency
	the base frequency and the speed of the arpeggio are controlled with the phase of the EMF
	the amplitude is controlled with the gain of the EMF
*/


#include <Bela.h>
#include <libraries/Gui/Gui.h>
#include <libraries/Scope/Scope.h>
#include <libraries/OnePole/OnePole.h>
#include <cmath>
#include <iostream>
#include "wavetable.h"

// helpers
void convertNoteSet();
void initWaveTable(BelaContext* context);
void guiAndOscSetup(BelaContext* context);

// USING_REBUS == 0 if using REBUS or analog wires
// USING_REBUS == 1 if testing with GUI
#define USING_REBUS 1

// GUI object declaration
Gui gui;

int renderCounter = 0;
int gBaseFreq = 220; // default base frequency of the arpeggio
int currentFreq, randomIndex, currentIncrement, lastIncrement = -1;
std::vector<int> gNoteSet = {1,0,0,0,1,0,0,1,0,0,1,0,0}; // notes (in semitones) to use in the arpeggio
std::vector<int> availableNotes; 

// smoothing filter
OnePole smootherPhase, smootherAmp;

// wavetable oscillator object
Wavetable gOsc;

// browser-based oscilloscope
Scope gScope;

// REBUS specific values
const float gMinPhase = 0.01;
const float gMaxPhase = 0.44;
const float gMinGain = 0.150467;
const float gMaxGain =  0.3;

float gInverseSampleRate;

float gainReading;
float phaseReading;

bool setup(BelaContext *context, void *userData)
{
	gInverseSampleRate = 1.0 / context->audioSampleRate;
	
	// initialize smoothing filters
	smootherPhase.setup(5, context->audioSampleRate);
	smootherAmp.setup(15, context->audioSampleRate);
	
	convertNoteSet();
	
	initWaveTable(context);

	guiAndOscSetup(context);

	return true;
}

void convertNoteSet(){
	// converts gNoteSet to a set of increments (in number of semitones) with respect to the fundametal
	// e.g. {1,0,1,0} -> {0,2}
	for (int i = 0; i < gNoteSet.size()  ; i++)
		if (gNoteSet[i] == 1)
			availableNotes.push_back(i);
}

void initWaveTable(BelaContext* context){
	// initializes the wavetable oscillator object to output a sinusoid, using wavetable.h

	std::vector<float> wavetableSin;
	unsigned int wavetableSize = 512;
	
	// populate a buffer with a sine wave
	wavetableSin.resize(wavetableSize);
	for(unsigned int n = 0; n < wavetableSin.size(); n++) {
		wavetableSin[n] = sinf(2.0 * M_PI * (float) n / (float) wavetableSin.size());
	}
	
	// initialise the wavetable, passing the sample rate and the buffer
	gOsc.setup(context->audioSampleRate, wavetableSin);
}

void guiAndOscSetup(BelaContext* context){
	// getup GUI
	gui.setup(context->projectName);
	// Setup buffer of floats (holding a maximum of 2 values)
	gui.setBuffer('f', 2); // Index = 0
	
	// Set up the oscilloscope
	gScope.setup(3, context->audioSampleRate);
}

void render(BelaContext *context, void *userData)
{	
	float* tempBuff;
	
	if (USING_REBUS == 1){
		// test mode, retrieve data from the GUI and fit it with the given REBUS values
	
		DataBuffer& buffer = gui.getDataBuffer(0);

		//Retrieve contents of the buffer as floats
		tempBuff = buffer.getAsFloat();

		// mimic phase and gain reading values
		phaseReading = map(tempBuff[0], 0, 1, gMinPhase, gMaxPhase);
		gainReading = map(tempBuff[1], 0, 1, gMinGain, gMaxGain);
	}
	
	for(unsigned int n = 0; n < context->audioFrames; n++) {
		
		// running on REBUS, read GAIN (PIN_4) and PHASE (PIN_0)
		if (USING_REBUS == 0) {
			phaseReading = analogRead(context, n/2, 4); // data[0]
			gainReading = analogRead(context, n/2, 0); // data[1]
		}
	
		// phase modulating the base frequency
		// use log to have a more linear mapping
		float currBaseFreq = map(phaseReading, gMinPhase, gMaxPhase, log10(200), log10(1500));
		// constrain frequency
		currBaseFreq = constrain(currBaseFreq, log10(200), log10(1500));
		// go back to frequencies
		currBaseFreq = pow(10.0, currBaseFreq);
		// smooth out artifacts
		currBaseFreq = smootherPhase.process(currBaseFreq);

		// phase modulating the speed
		float speed = map(phaseReading, gMinPhase, gMaxPhase, 2500, 300);
		// Constrain frequency
		speed = constrain(speed, 300, 2500);
		
		// get the midinote corresponding to the current base frequency 
		int baseMidiNote = 12 * log2(currBaseFreq/440) + 69;
		
		// according to the speed value, we update the frequency to be played
		if (renderCounter % (int) speed == 0){
			// update currentFreq
			
			// from the avaliable notes, get a random one and remove it so it's not picked in the next iteration
			randomIndex = rand() % availableNotes.size();
			currentIncrement = availableNotes[randomIndex]; // increment to apply to baseMidiNote
			availableNotes.erase(availableNotes.begin()+randomIndex);
			
			// add back the last iteration pick (avoided in the first iteration)
			if (lastIncrement != -1)
				availableNotes.push_back(lastIncrement);
			
			// calculate midinote to be played
			baseMidiNote += currentIncrement;
			
			// calculate frequency to be played, from the midinote
			currentFreq = 440 * pow(2, (float) (baseMidiNote - 69)/12);
			
			// update last increment
			lastIncrement = currentIncrement;
		}
		
		// gain modulating the amplitude
		float amplitude = map(gainReading, gMinGain, gMaxGain, 0, 1);
		amplitude = constrain(amplitude, 0, 1);
		// smooth out artifacts
		amplitude = smootherAmp.process(amplitude);
		
		// set the frequency of the wavetable oscillator object and retrieve its output
		gOsc.setFrequency(currentFreq);
		float out = gOsc.process()*0.2*amplitude;
		
		// write sinewave to left and right channels
		for(unsigned int channel = 0; channel < context->audioOutChannels; channel++) {
			audioWrite(context, n, channel, out);
		}

		// log the updated signals
		gScope.log(gainReading, phaseReading, out);
	}
	
	// update the number of times render has been called
	renderCounter++;
}

void cleanup(BelaContext *context, void *userData)
{

}
