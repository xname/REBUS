#include <Oscillator.h>
#include <math.h>

/*
 15/09/2019
 AndyNRG
 */

//Constructor
Oscillator::Oscillator(){}

void Oscillator::setup(int audioSampleRate, float freq, char waveType)
{
	fs = audioSampleRate;															// store sampling frequency
	samplingPeriod = 1.0f / (float)fs;												//calculate 1/SR
	phase = 0;																		// first ramp wave for ramp, triangular, pulse scripts
	phase2 = 0.5;																	// second ramp wave for pulse wave script
	phaseOffset = 0;
	previousPhaseOffset = 0;
	changePhase = false;
	phase_TrivialSquareWave = 0;													// trivial pulse wave for triangular script
	this->freq = freq;																//pass freq value	(440 by default)
	this->waveType = waveType;
}



void Oscillator::frequencyInput(const float freq)
{				
	this->freq = freq;
}


void Oscillator::phaseInput(float phaseOffset){
	if(phaseOffset!=previousPhaseOffset){
		previousPhaseOffset = phaseOffset;
		this->phaseOffset = phaseOffset;
		changePhase = true;
	}	
}


// sine wave
float Oscillator::sineWave(){
	
	float out = sin(phase);
	phase += 2.0 * M_PI * freq * samplingPeriod;
	while(phase >= 2.0 * M_PI)
		phase -= 2.0 * M_PI;
		
	return out;
}


// saw wave
float Oscillator::antiAliasedRampWave(){

	phase += freq * samplingPeriod;	
	while(phase >= 1.0f)
		phase -= 1.0f;
	float bipolarPhase = phase * 2.0 - 1.0;
	float squaredPhase = bipolarPhase*bipolarPhase;
  	float averageDifferentiatedPhase = ((squaredPhase-x_1ramp)*(squaredPhase+x_1ramp))/2.0f;
	x_1ramp = squaredPhase;
	float c = (float)fs/(4.0f*freq*(1.0f-freq*samplingPeriod));
	
	return c*averageDifferentiatedPhase;
}


// square waveform
float Oscillator::antiAliasedPulseWave(){

	phase += freq * samplingPeriod;	
	while(phase >= 1.0f)
		phase -= 1.0f;
	float bipolarPhase = phase * 2.0 - 1.0;
	float squaredPhase = bipolarPhase*bipolarPhase;
  	float averageDifferentiatedSquaredPhase = squaredPhase-x_1ramp;
  	x_1ramp = squaredPhase;
	float c = (float)fs/(4.0f*freq*(1.0f-freq/(float)fs));
	float phaseFirstRampRescaled = c*averageDifferentiatedSquaredPhase;


	phase2 += (freq * samplingPeriod);
	while(phase2 >= 1.0f)
		phase2 -= 1.0f;
	float bipolarPhase2 = phase2 * 2.0 - 1.0;
	float squaredPhase2 = bipolarPhase2*bipolarPhase2;
  	float averageDifferentiatedSquaredPhase2 = squaredPhase2-x_1secondRamp;
	x_1secondRamp = squaredPhase2;
	float phaseSecondRampWaveRescaled = c*averageDifferentiatedSquaredPhase2;
	
	float phasePulseWave = phaseFirstRampRescaled-phaseSecondRampWaveRescaled;
	
	return phasePulseWave;
}



// triangular waveform
float Oscillator::antiAliasedTriangularWave(){

	float twoTimesFreq = 2*freq;																							// we need to start with two times the frequency we want to achieve	
	phase += twoTimesFreq * samplingPeriod;
	if(phase > 1.0f){
		phase_TrivialSquareWave = !phase_TrivialSquareWave;												// calculate phase for a trivial square wave. We'll use it later to modulate the phase of the upcoming triangular waveform
	}
	while(phase >= 1.0f)
		phase -= 1.0f;

	float bipolarPhase = phase * 2.0 - 1.0;
	float squaredPhase = bipolarPhase*bipolarPhase;
	float phaseTurnedUpsideDown = 1-squaredPhase;
	
	float bipolarPhase_TrivialSquareWave = phase_TrivialSquareWave * 2.0 - 1.0;

	float phaseModulatedWithTrivialSquareWave = phaseTurnedUpsideDown*bipolarPhase_TrivialSquareWave;
  	float differentiatedPhase= phaseModulatedWithTrivialSquareWave-x_1ramp;
	x_1ramp = phaseModulatedWithTrivialSquareWave;
	float c = (float)fs/(4.0f*(2*freq)*(1.0f-(2*freq)*samplingPeriod));
	
	return differentiatedPhase*c;
}


// public method to calculate and get each sampleValue of the waveform in realtime
float Oscillator::output()
{
	float output = 0;
	
	if(waveType == 's')
		output = sineWave();
	
	if(waveType =='r')
		output = antiAliasedRampWave();

	if(waveType =='p')
		output = antiAliasedPulseWave();
		
	if(waveType =='t')
		output = antiAliasedTriangularWave();
		
	return output;
}

