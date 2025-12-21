/*
 15/09/2019
 AndyNRG
 */

#ifndef Filter_h
#define Filter_h

class Filter 			// 2nd or 4th Order Resonant Filter; HighPass or LowPass; (see setup function)
{
	private:
		float audioSampleRate;
		float cutoffFrequency; 
		float Q;
		char filterType;
		int filterOrder;

		// variables to store filter coefficients calculation
		float b0, b1, b2, a1, a2;					

		// Filter 1
		// global variables to store the value for the past incoming and outgoing samples
		float x_1 = 0, x_2 = 0; // past incoming samples x[n-1] and [n-2]
		float y_1 = 0, y_2 = 0; // past outgoing samples y[n-1] and y[n-2]

		// Filter 2
		// global variables to store the value for the past incoming and outgoing samples
		float x_3 = 0, x_4 = 0; // past incoming samples x[n-1] and [n-2]
		float y_3 = 0, y_4 = 0; // past outgoing samples y[n-1] and y[n-2]
		
		float highPassOrLowPass(float sampleInput, float filterOutput);

	public:
		Filter();											// default constructor

		void setup(int audioSampleRate, float cutoffFrequency = 100, float Q = 0.707, char filterType = 'l', int filterSteepness = 2); // 1 = 2nd order; 2 = 4th order
		void cutoffFrequencyInput(float cutoffFrequency);
		void resonanceInput(float Q);

		void calculateCoefficients();						// calculate filter coefficients for any samplingPeriod (passed internally, cutoffFrequency and QualityFactor
	
		float audioInput(float);							// call this method in a loop to return a filtered sample
		
};
	
	
#endif
