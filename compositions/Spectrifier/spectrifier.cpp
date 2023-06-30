/*

 _____ _____ _____ _____ _____ 
| __  |   __| __  |  |  |   __|
|    -|   __| __ -|  |  |__   |
|__|__|_____|_____|_____|_____|

spectrifier.cpp

Eleonora Maria Irene Oreggia ((xname)) 2017 - 2023
Spectrifier was the CODENAME of REBUS during development. You can still enjoy it as a composition!!
Thanks to Andrew McPherson, Giulio Moro and the Bela team (bela.io).
Developed at Queen Mary University of London, at Media & Arts Technology DTC
A collaboration between the Antennas & Electromagnetics Group and C4DM (Centre for Digital Music)

 */


#include <Bela.h>
#include <cmath>
#include <unistd.h>
#include <libraries/Scope/Scope.h>

// read square wave table
#include "square_table.h"

float gPhase;
float gInverseSampleRate;
float gFrequency = 440;

float gEmGain = 1;
float gEmPhase= 0;

float gMinPhase = 0.2;
float gMaxPhase = 1.8;
float gMinGain = 0.3;
float gMaxGain = 1.4;

float gMinFrequency = 20.0;
float gMaxFrequency = 2000.0;

float gADCFullScale = 4.096;

// High Pass Coefficients
float gHP_a1 = 0.0;
float gHP_a2 = 0.0;
float gHP_b0 = 0.0;
float gHP_b1 = 0.0;
float gHP_b2 = 0.0;

// store states High Pass
float prevStateInHP = 0.0;  //gHP_b1
float pastStateInHP = 0.0;  //gHP_b2
float prevStateOutHP = 0.0;  //gHP_a1
float pastStateOutHP = 0.0;  //gHP_a2

int counter = 0;

//Istantiate the virtual oscilloscope
Scope gScope;


bool setup(BelaContext *context, void *userData)
{

	/*
	 * Calculating filter coefficients
	 * default is 20Hz
	 */

	float filterFrequency = 300;
	float Q = sqrt(2.0)*0.5; // quality factor for Butterworth filter
	float T = 1/context->audioSampleRate; //period T, inverse of sampling frequency
	float w = 2.0*M_PI*filterFrequency; //angular freq in radians per sec

	float delta = (4.0+((2*T)*(w/Q))+(w*w*T*T)); //factor to simplify the equation

	//calculate High Pass coefficients
	gHP_a1= (-8+2*(T*T*w*w))/delta;
	gHP_a2= (4 - (2*T*w/Q)+(T*T*w*w))/delta;
	gHP_b0= 4/delta; // 0.904152;
	gHP_b1= -8/delta; //-1.808304;
	gHP_b2= gHP_b0;// 0.904152;

	gInverseSampleRate = 1.0 / context->audioSampleRate;
	gPhase = 0.0;
	
	// Set up the oscilloscope (n channels and samplerate)
	gScope.setup(3, context->audioSampleRate);

	
	return true;
}

float square(float phase){
	//  TODO:
	// - use different tables for different frequency ranges so no aliasing

	// stay in range
	while(phase >= 2.0 * M_PI)
	            phase -= 2.0 * M_PI;
	while(phase < 0)
	            phase +=2.0*M_PI;

	//map phase to table length
	float fracIndex=(phase/(2*M_PI) * squareTableLength);
	int n=(int)fracIndex;
	float x=fracIndex-n; // subtract to leave only the fraction value
	float y0=squareTable[n];
	float y1=squareTable[n+1];

	if(n<=255){
		y0 = squareTable[n];
		y1 = squareTable[n+1];
	}

	if(256 < n && n < 512){
		y0 = squareTable[255+(256-n)];
		y1 = squareTable[(255+(256-n))+1];
	}

	if(512<=n && n<768){
		y0 = -squareTable[n-512];
		y1 = -squareTable[(n-512)-1];
	}

     if(768<=n && n<1024){
    	y0 = -squareTable[255+(768-n)];
    	y1 = -squareTable[(255+(768-n)) - 1];
    }

	// simplifies linear interpolation formula assuming x0=0, x1=1
	float out=y0 + (y1-y0)*x;
	return out;
} 


void spectrify(float phaseReading, float gainReading){
	// it receives phase and gain from the VNA
	// transforms the GPIO input phase reading from Voltage to radiant
	// 1 degree = 0.0174532925 radians
	// formula is
	// radians = degrees * M_PI / 180
	// only the phase should be in radians!!!

	//gEmPhase = phaseReading * M_PI / 0.03;
    float phaseVoltage = phaseReading * gADCFullScale;  //4.096 full scale value of the converter of context->analogIn;
    gEmPhase = (- phaseVoltage * 100 +180)/180 * M_PI; // negative slope therefore minus, approx 0.03 to 0

	// stay in range
	while(gEmPhase >= 2.0 * M_PI)
	            gEmPhase -= 2.0 * M_PI;
	while(gEmPhase < 0)
	            gEmPhase +=2.0 * M_PI;

	float gainVoltage = gainReading *  gADCFullScale;
	gEmGain =  gainVoltage * (gMaxGain - gMinGain) + gMinGain;

	// it is a value in decibel 30 mV per decibel
	/* TODO mapping dB to amplitude, exponential function
	 *  0dB = 1 
	 *  -6 dB = 0.5 
	 *  -12dB = 0.25 
	 * dBmV is a measure of signal strength in wires and cables at RF and Af freq
	 * CLIVE:
	 * this is a measure of power in RF
	 * 0dBm = 1mW
	 * -30dB = 1/1000 W
	 * +30 dB = 1000 W
	 *
	 *
	 */

}

/*	float out=sinf(phase);
//	out= out>=0 ? 1 : -1;
	if(out>=0.2)
		out=0.2;
	else if(out<=0.2)
		out=-0.2;
	out*=5;
	return out; */


void render(BelaContext* context, void* arg)
{
	
 
	for(unsigned int n = 0; n < context->audioFrames; n++) {



		// read the  PHASE 
		float phaseReading = context->analogIn[n/2*8]; // pot value, interleaved, half audio rate

		float phaseVoltage = phaseReading * gADCFullScale;  //4.096 full scale value of the converter of matrixIn
//		gEmPhase = (- phaseVoltage * 100 +180)/180 * M_PI; // negative slope therefore minus, approx 0.03 to 0

		// read the  amplitude / GAIN 
		float gainReading = context->analogIn[n/2*8 + 4]; // pot value, interleaved, half audio rate

		float gainVoltage = gainReading *  gADCFullScale;

	    //gEmPhase = phaseReading * M_PI / 0.03; // wrong mapping but nice result and interaction

		float phase = (phaseVoltage - gMinPhase) / (gMaxPhase - gMinPhase);
		float gain = (gainVoltage - gMinGain) / (gMaxGain - gMinGain);

		gFrequency = phase*(gMaxFrequency - gMinFrequency) + gMinFrequency;

		spectrify (phaseVoltage, gainVoltage);

		// pass to output as filterIn to a high pass filter NO SQUARE
		float FilterIn_A = gEmGain *  gPhase  + gEmGain * (gPhase+gEmPhase) + (gEmPhase*gEmGain)  + ((gain + phase)*2 - 1); // eliminate offset and remap from 0:1 to -1:1 to keep in range

        // pass to output as filterIn to a high pass filter // with square
        float FilterIn_B = gEmGain *  square(gPhase)  + gEmGain * square(gPhase+gEmPhase) + (gEmPhase*gEmGain)  + ((gain + phase)*2 - 1); // eliminate offset and remap from 0:1 to -1:1 to keep in range

		float FilterIn =  FilterIn_A * (FilterIn_B /2);    

		/* anular modulation between phase and gain,
		 * not very significant from a physical point of view
		 * but it sounds good (gEmPhase*gEmGain), now commented
		 */

		//high pass filter on out with cutoff~20Hz to make sure no
		//DC offset is present at the DAC

		//2nd-order IIR (differential equation): y[n]= b0 x[n] + b1 x[n-1] + b2 x[n-2] - a1 y[n-1] - a2 y[n-2]

		//apply filter on output

		float out=(gHP_b0*FilterIn) + (gHP_b1*prevStateInHP) + (gHP_b2*pastStateInHP) - (gHP_a1*prevStateOutHP) - (gHP_a2*pastStateOutHP);

		// rt_printf("filterIn is  %f\n", filterIn);
		
		// send EM and audio data to Scope
		gScope.log(gainReading, phaseReading, out);

		pastStateInHP=prevStateInHP;    //store current x[n-1] that will become x[n-2]
		prevStateInHP=FilterIn;			//store current x[n] that will become x[n-1]

		pastStateOutHP=prevStateOutHP;  //store current y[n-1] that will become y[n-1]
		prevStateOutHP=out; //store current y[n] that will become y[n-1]

		out/=2;

		gPhase += 2.0 * M_PI * gFrequency * gInverseSampleRate;

		if(gPhase > 2.0 * M_PI)
			gPhase -= 2.0 * M_PI;

		for(unsigned int channel = 0; channel < context->audioInChannels; channel++)
			audioWrite(context, n, channel, out);

	}
}


void cleanup(BelaContext *context, void *userData)
{

}
