/*
 15/09/2019
 AndyNRG 
*/

#ifndef Oscillator_h
#define Oscillator_h

class Oscillator
{
	
	private:
		float fs;											// sampleFrequency
		float samplingPeriod;							
		float freq;											// osc freq
		char waveType;										// which wave we are generating
		float phase;
		float phase2;										// used to generate antialiased squareWavein antiAliasedSquareWave()
		bool changePhase;									// used for pwm
		float phaseOffset;									// used for pwm
		float previousPhaseOffset;							// used for pwm
		float x_1ramp;										// variable for differentiating osc saw
		float x_1secondRamp;								// variable for differentiating osc pulse 
		float phase_TrivialSquareWave;						// used to generate antialiased triangular waveform antiAliasedTriangularWave()

		float sineWave();
		float antiAliasedRampWave();
		float antiAliasedPulseWave();
		float antiAliasedTriangularWave();
		

	public:
		Oscillator();										// default constructor

		void setup(
		int audioSampleRate,
		float freq = 440,									// setup function for the oscillator 
		char waveType ='s' 
		);
		
		void frequencyInput(float);							// method to set the osc freq
		void phaseInput(float);
		float output();										// call this method in a loop to return each phase of the waveform

};


#endif

