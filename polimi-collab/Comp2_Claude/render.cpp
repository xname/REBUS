/*
 
 REBUS - Electromagnetic Interactions 

 Workshop Template = CODE SKELETHON
 https://xname.cc/rebus
 xname 2023

 https://xname.cc/rebus 

 ringing retriggered complex oscillator 
 by claude mathr.co.uk

*/

// change this to 1 for different analogRead mapping with rebus magic numbers
#define USING_REBUS 1

#include <Bela.h>
#include <libraries/Biquad/Biquad.h>
#include <libraries/OnePole/OnePole.h>
#include <libraries/Scope/Scope.h>
#include <libraries/Oscillator/Oscillator.h>
#include <libraries/Gui/Gui.h>
#include <libraries/GuiController/GuiController.h>

#include <cmath>
#include <complex>

//Gui gui;
//GuiController controller;
//unsigned int gGainSliderIdx;
//unsigned int gPhaseSliderIdx;

//Istantiate the virtual oscilloscope
Scope gScope;

// SENSOR DATA >> EM GAIN and PHASE  
float gMinPhase = 0.01;
float gMaxPhase = 0.44;
float gMidPhase = 0.2;
float gMinGain = 0.150467;
float gMaxGain =  0.3;

Biquad gNotchFilter[2];
float gNotchFreq = 50.0;
Biquad gLowpassFilter[2];

bool setup(BelaContext *context, void *userData)
{

	// Set up the oscilloscope (n channels and samplerate)
	gScope.setup(6, context->audioSampleRate);

	//gui.setup(context->projectName);
	//controller.setup(&gui, "Controls");
	//gGainSliderIdx = controller.addSlider("Gain", 0.5, 0, 1, 0);
	//gPhaseSliderIdx = controller.addSlider("Phase", 0.5, 0, 1, 0);

    float qButterworth = sqrt(0.5);
	Biquad::Settings settings{
			.fs = context->audioSampleRate,
			.type = Biquad::notch,
			.cutoff = gNotchFreq,
			.q = qButterworth,
			.peakGainDb = 0,
			};
	gNotchFilter[0].setup(settings);
    gNotchFilter[1].setup(settings);
    settings.type = Biquad::lowpass;
    settings.cutoff = 10.0f;
    gLowpassFilter[0].setup(settings);
    gLowpassFilter[1].setup(settings);
	return true;
}

std::complex<double> gOscIncrement = 0;
std::complex<double> gOscillator = 0;
float gOscRMS = 0;

void render(BelaContext *context, void *userData)
{
	//float gainControl = controller.getSliderValue(gGainSliderIdx);
	//float phaseControl = controller.getSliderValue(gPhaseSliderIdx);

    for(unsigned int n = 0; n < context->audioFrames; n++) {
    	float out[2] = {0, 0};

	  	// read GAIN (PIN_4) and PHASE (PIN_0)
	  	float gainReading = analogRead(context, n/2, 4);
		float phaseReading = analogRead(context, n/2, 0);

#if USING_REBUS
        // magic for rebus
        float gain = map(gainReading, gMinGain, gMaxGain, 0.0f, 1.0f);
        float phase = map(phaseReading, gMinPhase, gMaxPhase, 0.0f, 1.0f);
#else
        // magic for wires
	    float gain  = gLowpassFilter[0].process(gNotchFilter[0].process(gainReading));
		float phase = gLowpassFilter[1].process(gNotchFilter[1].process(phaseReading));
#endif

        gain = constrain(gain, 0.0f, 1.0f);

        float f = 0.25f * phase;
        float m = expf(map(gain, 0.0f, 1.0f, logf(0.99f), logf(0.999999f)));
        gOscIncrement = double(m) * std::complex<double>(cosf(f), sinf(f));

    	gOscRMS *= 0.99;
    	gOscRMS += 0.01 * std::norm(gOscillator);
        if (gOscRMS < 3.0e-3f)
        {
           gOscillator += 0.5;
        }

        gOscillator *= gOscIncrement;

	    out[0] = tanhf(gOscillator.real());
	    out[1] = tanhf(gOscillator.imag());
    
		// send EM and audio data to Scope
		gScope.log(gainReading, phaseReading, gain, phase, out[0], out[1]);

		// Write the sample to every audio output channel            
    	for(unsigned int channel = 0; channel < context->audioOutChannels; channel++) {
    		audioWrite(context, n, channel, channel < 2 ? out[channel] : 0.0f);
    	}
    }
}

void cleanup(BelaContext *context, void *userData)
{
	
}
