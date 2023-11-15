//---------------------------------------------------------------------
#include <Bela.h>
#include <Oscillator.h>
#include <cmath>
#include <libraries/Scope/Scope.h>
#include <unistd.h>

/*
 REBUS 8oscs
 xname 16/11/2019 >> 2023
 osc lib by AndyNRG aka UVCORE
 */

Oscillator osc0;
Oscillator osc1;
Oscillator osc2;
Oscillator osc3;
Oscillator osc4;
Oscillator osc5;
Oscillator osc6;
Oscillator osc7;
Oscillator osc8;

Scope scope;

float gMinPhase = 0.01;
float gMaxPhase = 0.44;
float gMinGain = 0.150467;
float gMaxGain =  0.3;

//float gMinGain = 0.3;
//float gMaxGain = 1.4;

int counter = 0;

float gain = 0.5;


bool setup(BelaContext *context, void *userData)
{
	//osc freq, s= sine; p=square; r=ramp; t=triangular; 
    osc0.setup(context->audioSampleRate, 253.0, 's');
    osc1.setup(context->audioSampleRate, 221.0, 'p');
    osc2.setup(context->audioSampleRate, 220.0, 'r');
    
    osc3.setup(context->audioSampleRate, 50.0, 't');
    osc4.setup(context->audioSampleRate, 51.0, 's');
    osc5.setup(context->audioSampleRate, 225.0, 'p');
  
    osc6.setup(context->audioSampleRate, 53.3, 'r');
    osc7.setup(context->audioSampleRate, 104.6, 't');
    osc8.setup(context->audioSampleRate, 57.0, 's');
	
	scope.setup(12, context->audioSampleRate);
	
	return true;
}

void render(BelaContext *context, void *userData){
	
	for (unsigned int n = 0; n < context->audioFrames; n++){
		
		float phaseReading = analogRead(context, n/2, 0);
		
		float frequency = map(phaseReading, gMinPhase, gMaxPhase, 100, 1000);
		frequency = constrain(frequency, 100.0, 1000.0);   
		
        
        osc0.frequencyInput(0.1 * frequency);
        osc1.frequencyInput(0.2 * frequency);
        osc2.frequencyInput(0.4 * frequency);
        osc3.frequencyInput(0.8 * frequency);
        osc4.frequencyInput(0.16 * frequency);
        osc5.frequencyInput(0.32 * frequency);
        osc6.frequencyInput(0.64 * frequency);
        osc7.frequencyInput(1.28 * frequency);
        osc8.frequencyInput(2.36 * frequency);

        
		// set wave --> tobe implemented 
	
		float gainReading = analogRead(context, n/2, 4);
		float amplitude = map(gainReading, gMinGain, gMaxGain, 0, 0.05);
		amplitude = constrain(amplitude, 0, 1);
		
		scope.log(gainReading, phaseReading);
		
		float mix0 = osc0.output() * amplitude;
		float mix1 =  osc1.output() *  amplitude;
		float mix2 = osc2.output() * -0.7 * amplitude;
		float mix3 = osc3.output() * amplitude; 
		float mix4 = osc4.output() * amplitude; 
		float mix5 = osc5.output() * amplitude; 
		float mix6 = osc6.output() * -0.1 * amplitude;
		float mix7 = osc7.output()* amplitude;
		float mix8 = osc8.output()* amplitude;
		
		float out = (mix0+mix1+mix2+mix3+mix4+mix5+mix6+mix7+mix8)*gain;
		
		scope.log(gainReading, phaseReading, mix0, mix1, mix2, mix3, mix4, mix5, mix6, mix7, mix8, out);  //mix0, mix1, mix2, mix3, mix4, mix5, mix6, mix7, mix8
		 
		for(int i = 0; i < 2; i++){
		audioWrite(context, n, i, out); 				   
	   	}
	}
	
}

void cleanup(BelaContext *context, void *userData){
}
