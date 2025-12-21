/*
 15/09/2019
 AndyNRG 
*/

#include <Filter.h>
#include <math.h>

//Constructor
Filter::Filter(){}

void Filter::setup(int audioSampleRate, float cutoffFrequency, float Q, char filterType, int filterOrder){
	this->audioSampleRate = audioSampleRate;
	this->cutoffFrequency = cutoffFrequency;
	this->Q = Q;
	this->filterType = filterType;
	this->filterOrder = filterOrder;
	calculateCoefficients();
}


void Filter::cutoffFrequencyInput(float cutoffFrequency){
	this->cutoffFrequency = cutoffFrequency;
	calculateCoefficients();
}


void Filter::resonanceInput(float Q){
	this->Q = Q;
	calculateCoefficients();
}

void Filter::calculateCoefficients()
{
	float wc = 2.0 * M_PI* cutoffFrequency;					// calculate analog angular frequency. The filter is not pre-warped 				
	float T  = 1.0/audioSampleRate;							// calculate sampling period
	

	// as explained in the report, all the filter coefficients are normalised to the a0 coefficient 
	// the a0 coefficient is therefore also divided by a0 and gives 1 as a result
	// as a0*y[n] = 1*y[n] = y[n] we don't need to include a0 in the difference equation
	// then we just use a0 as a local variable in the calculate_coefficients fucntion
	// as the calculations produce float values, we need to convert any integer to a float
	// In C++ any number with a decimal value is a double by default so we add the character f to treat it as a float
	
	float a0 = (wc*wc*T*T)+((2.0f*wc*T)/(float)Q)+4.0f;	 // calculate coefficient a0
	
	a1	= (2.0f*wc*wc*T*T)-8.0f;						 // calculate coefficient a1
	a1	/= a0;											 // normalise a1 to a0

	a2	= (wc*wc*T*T)-((2.0f*wc*T)/(float)Q)+4.0f;		 // calculate coefficient a2
	a2	/= a0;											 // normalise a2 to a0

	b0	= wc*wc*T*T;									 // calculate coefficient b0									
	b0	/= a0;										     // normalise b0 to a0

	b1	= 2.0f*wc*wc*T*T;								 // calculate coefficient b1
	b1	/= a0;											 // normalise b1 to a0
	
	b2	= b0;											 // in this filter, coefficient b2 is equal to b0

}


float Filter::highPassOrLowPass(float sampleInput, float filterOutput){
	float output;
 	switch(filterType){
 	case 'l': output = filterOutput;
	break;
 	case 'h': output = sampleInput-filterOutput;
 	break;
 	}
 	return output;
}

float Filter::audioInput(float sampleInput)
{
		float outFilter1 = (b0*sampleInput) + (b1*x_1) + (b2*x_2) - (a1*y_1) - (a2*y_2);	
 		x_2 = x_1;// the incoming previous sample x[n-1] becomes the precedent incoming sample x[n-2] 
				  // we need to store x_1 into x_2 at this point of the code as the next step
				  // will override the content of x_1 with the new incoming sample
				  
		x_1 = sampleInput; // the incoming sample x[n] becomes the previous sample x[n-1]
 		
 		y_2 = y_1;// the outgoing previous sample y[n-1] becomes the precedent outgoing sample y[n-2] 
				  // we need to store y_1 into y_2 at this point of the code as the next step
				  // will override the content of y_1 with the new outgoing sample
 		y_1 = outFilter1;
 		
 		if(filterOrder == 1){
 			float output = highPassOrLowPass(sampleInput, outFilter1);
 			return output;
 		}
 		
 		// The output of the first filter is fed into the input of the second filter
 		float outFilter2 = (b0*outFilter1) + (b1*x_3) + (b2*x_4) - (a1*y_3) - (a2*y_4);
 	
  		x_4 = x_3;// the incoming previous sample x[n-1] becomes the precedent incoming sample x[n-2] 
				  // we need to store x_1 into x_2 at this point of the code as the next step
				  // will override the content of x_1 with the new incoming sample
				  
		x_3 = outFilter1; // the incoming sample x[n] becomes the previous sample x[n-1]
 		
 		y_4 = y_3;// the outgoing previous sample y[n-1] becomes the precedent outgoing sample y[n-2] 
				  // we need to store y_4 into y_3 at this point of the code as the next step
				  // will override the content of y_3 with the new outgoing sample
 		y_3 = outFilter2;
 
  		if(filterOrder == 2){
 			float output = highPassOrLowPass(sampleInput, outFilter2);
  			return output;
		}
		
		return 0;
 	
}
