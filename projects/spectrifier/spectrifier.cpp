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

#include <libraries/REBUS/REBUS.h>

// read square wave table
#include "square_table.h"

const char *COMPOSITION_name = "spectrifier";

struct COMPOSITION
{
	float gPhase;
	float gInverseSampleRate;
	float gFrequency;
	float gEmGain;
	float gEmPhase;
	// High Pass Coefficients
	float gHP_a1;
	float gHP_a2;
	float gHP_b0;
	float gHP_b1;
	float gHP_b2;
	// store states High Pass
	float prevStateInHP;  //gHP_b1
	float pastStateInHP;  //gHP_b2
	float prevStateOutHP;  //gHP_a1
	float pastStateOutHP;  //gHP_a2
};

float gMinPhase = 0.2;
float gMaxPhase = 1.8;
float gMinGain = 0.3;
float gMaxGain = 1.4;

float gMinFrequency = 20.0;
float gMaxFrequency = 2000.0;

float gADCFullScale = 4.096;

bool COMPOSITION_setup(BelaContext *context, COMPOSITION *C)
{
	std::memset(C, 0, sizeof(*C));

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
	C->gHP_a1= (-8+2*(T*T*w*w))/delta;
	C->gHP_a2= (4 - (2*T*w/Q)+(T*T*w*w))/delta;
	C->gHP_b0= 4/delta; // 0.904152;
	C->gHP_b1= -8/delta; //-1.808304;
	C->gHP_b2= C->gHP_b0;// 0.904152;

	C->gInverseSampleRate = 1.0 / context->audioSampleRate;
	C->gPhase = 0.0;
	C->gFrequency = 440;

	return true;
}

inline
float square(float phase){
	//  TODO:
	// - use different tables for different frequency ranges so no aliasing

	// stay in range
phase /= (2.0 * M_PI);
phase -= floorf(phase);
phase *= (2.0 * M_PI);
#if 0
	while(phase >= 2.0 * M_PI)
	            phase -= 2.0 * M_PI;
	while(phase < 0)
	            phase +=2.0*M_PI;
#endif

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

inline
void spectrify(COMPOSITION *C, float phaseReading, float gainReading){
	// it receives phase and gain from the VNA
	// transforms the GPIO input phase reading from Voltage to radiant
	// 1 degree = 0.0174532925 radians
	// formula is
	// radians = degrees * M_PI / 180
	// only the phase should be in radians!!!

	//gEmPhase = phaseReading * M_PI / 0.03;
    float phaseVoltage = phaseReading * gADCFullScale;  //4.096 full scale value of the converter of context->analogIn;
    C->gEmPhase = (- phaseVoltage * 100 +180)/180 * M_PI; // negative slope therefore minus, approx 0.03 to 0

	// stay in range
C->gEmPhase /= (2.0 * M_PI);
C->gEmPhase -= floorf(C->gEmPhase);
C->gEmPhase *= (2.0 * M_PI);
#if 0
	while(C->gEmPhase >= 2.0 * M_PI)
	            C->gEmPhase -= 2.0 * M_PI;
	while(C->gEmPhase < 0)
	            C->gEmPhase +=2.0 * M_PI;
#endif

	float gainVoltage = gainReading *  gADCFullScale;
	C->gEmGain =  gainVoltage * (gMaxGain - gMinGain) + gMinGain;

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

inline
void COMPOSITION_render(BelaContext* context, COMPOSITION *C, int n, float out[2], const float in[2], float mapped_magnitude, float mapped_phase)
{
	// these must match the magic numbers in render.cpp
	// to correctly undo the template's mapping
	// and reconstruct the analog readings
	const float phase_min = 0.01;
	const float phase_max = 0.44;
	const float magnitude_min = 0.150467;
	const float magnitude_max = 0.3;
	float phaseReading = map(mapped_phase, 0, 1, phase_min, phase_max);
	float gainReading = map(mapped_magnitude, 0, 1, magnitude_min, magnitude_max);

		// read the  PHASE 
		float phaseVoltage = phaseReading * gADCFullScale;  //4.096 full scale value of the converter of matrixIn
//		gEmPhase = (- phaseVoltage * 100 +180)/180 * M_PI; // negative slope therefore minus, approx 0.03 to 0

		// read the  amplitude / GAIN 
		float gainVoltage = gainReading *  gADCFullScale;

	    //gEmPhase = phaseReading * M_PI / 0.03; // wrong mapping but nice result and interaction

		float phase = (phaseVoltage - gMinPhase) / (gMaxPhase - gMinPhase);
		float gain = (gainVoltage - gMinGain) / (gMaxGain - gMinGain);

		C->gFrequency = phase*(gMaxFrequency - gMinFrequency) + gMinFrequency;

		spectrify (C, phaseVoltage, gainVoltage);

		// pass to output as filterIn to a high pass filter NO SQUARE
		float FilterIn_A = C->gEmGain *  C->gPhase  + C->gEmGain * (C->gPhase+C->gEmPhase) + (C->gEmPhase*C->gEmGain)  + ((gain + phase)*2 - 1); // eliminate offset and remap from 0:1 to -1:1 to keep in range

        // pass to output as filterIn to a high pass filter // with square
        float FilterIn_B = C->gEmGain *  square(C->gPhase)  + C->gEmGain * square(C->gPhase+C->gEmPhase) + (C->gEmPhase*C->gEmGain)  + ((gain + phase)*2 - 1); // eliminate offset and remap from 0:1 to -1:1 to keep in range

		float FilterIn =  FilterIn_A * (FilterIn_B /2);    

		/* anular modulation between phase and gain,
		 * not very significant from a physical point of view
		 * but it sounds good (gEmPhase*gEmGain), now commented
		 */

		//high pass filter on out with cutoff~20Hz to make sure no
		//DC offset is present at the DAC

		//2nd-order IIR (differential equation): y[n]= b0 x[n] + b1 x[n-1] + b2 x[n-2] - a1 y[n-1] - a2 y[n-2]

		//apply filter on output

		float o=(C->gHP_b0*FilterIn) + (C->gHP_b1*C->prevStateInHP) + (C->gHP_b2*C->pastStateInHP) - (C->gHP_a1*C->prevStateOutHP) - (C->gHP_a2*C->pastStateOutHP);

		// rt_printf("filterIn is  %f\n", filterIn);
		
		C->pastStateInHP=C->prevStateInHP;    //store current x[n-1] that will become x[n-2]
		C->prevStateInHP=FilterIn;			//store current x[n] that will become x[n-1]

		C->pastStateOutHP=C->prevStateOutHP;  //store current y[n-1] that will become y[n-1]
		C->prevStateOutHP=o; //store current y[n] that will become y[n-1]

		o/=2;

		C->gPhase += 2.0 * M_PI * C->gFrequency * C->gInverseSampleRate;

		if(C->gPhase > 2.0 * M_PI)
			C->gPhase -= 2.0 * M_PI;

		out[0] = o;
		out[1] = o;
}

void COMPOSITION_cleanup(BelaContext *context, COMPOSITION *C)
{

}

REBUS
