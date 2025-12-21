#include <Bela.h>
#include <ADSR_Envelope.h>
#include <math.h>
#include <iostream>
using namespace std;

ADSR_Envelope::ADSR_Envelope(){}


// SETUP env
void ADSR_Envelope::setup(int audioSampleRate, float attackTimeInSeconds, float decayTimeInSeconds, float sustainLevel, float releaseTimeInSeconds, char curveType){
	this->audioSampleRate = audioSampleRate;
	this->curveType = curveType;
	this->sustainLevel = constrain(sustainLevel, 0, 0.99);
	attackStartLevel = 0;				
	attackEndLevel = 1;
	releaseStartLevel = 1;
	releaseEndLevel = 0;					
	envelopeState = attackPhase;
	gateValue = false;
	phase = 0;							
	attackTimeInput(attackTimeInSeconds);
	decayTimeInput(decayTimeInSeconds);
	releaseTimeInput(releaseTimeInSeconds);
	envelopeState = stateOff;
}


void ADSR_Envelope::attackTimeInput(float attackTimeInSeconds){
	float attackTimeInSamples = attackTimeInSeconds*(float)audioSampleRate;
	if(attackTimeInSamples == 0)														// then we can't divide by 0 (i.e. b2-b1/0)so we add one sample
		linearAttackIncrement = 1;
	else 
		linearAttackIncrement = (attackEndLevel - attackStartLevel)/attackTimeInSamples;
}

void ADSR_Envelope::decayTimeInput(float decayTimeInSeconds){
	decayTimeInSeconds = constrain(decayTimeInSeconds, 0.001, 1);
	float decayTimeInSamples = decayTimeInSeconds*(float)audioSampleRate;
	if(decayTimeInSamples == 0)
		decayTimeInSamples = 1;
	else{
	linearDecayDecrement = (sustainLevel- attackEndLevel)/(decayTimeInSamples);
	}
}

void ADSR_Envelope::releaseTimeInput(float releaseTimeInSeconds){
	float releaseTimeInSamples = releaseTimeInSeconds*(float)audioSampleRate;
	if(releaseTimeInSamples == 0)
		linearReleaseDecrement = 1;
	else{
	linearReleaseDecrement = (releaseEndLevel- releaseStartLevel)/(releaseTimeInSamples);
	}
}

void ADSR_Envelope::sustainLevelInput(float sustainLevel){
	this->sustainLevel = constrain(sustainLevel, 0, 99);
}

void ADSR_Envelope::gateInput(bool gateValue){
	this->gateValue = gateValue;
}

// calculate env function and return its current val
float ADSR_Envelope::output(){

	if(curveType == 'l'){
		if(envelopeState == stateOff){
			if(gateValue == true)
				envelopeState = attackPhase;
		}			
		else if(envelopeState == attackPhase){
			if(phase < attackEndLevel && gateValue == true)
				phase += linearAttackIncrement;
			else{
				envelopeState = decayPhase;
			}
		}
		else if(envelopeState == decayPhase){
			if(phase > sustainLevel)
				phase += linearDecayDecrement;				//increment with negative number; it's a subtraction
			else
				envelopeState = sustainPhase;
		}
		else if(envelopeState == sustainPhase){
			if(gateValue == false)
				envelopeState = releasePhase;
			
		}
		else if(envelopeState == releasePhase){
			if(phase > releaseEndLevel)
				phase += linearReleaseDecrement;			//increment with negative number; it's a subtraction
			else{
				envelopeState = stateOff;
				phase = 0;
			}
			if(gateValue)									//allow retrig during the release phase
				envelopeState = attackPhase;
		}
	} 

	return phase;
}



