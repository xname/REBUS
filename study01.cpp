/*
 *  study01.cpp
 *  Eleonora Maria Irene Oreggia ((xname))
 *  Spectrifier :: an electromagnetic instrument
 *  User Study 01 - November 2018
 *  Please refer to up-to-date Bela API if you wish to use this code as it will need adaptation!!
 * 
 */



// #include "../include/render.h"
#include <Bela.h>
#include <cmath>
#include <rtdk.h>
#include <unistd.h>
#include <WriteFile.h>

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

/* TODO on/off button */
//extern int gIsPlaying;
//extern int gTriggerButton1;

/* log data */ 
WriteFile file1;
WriteFile file2; 


bool setup(BelaContext* context, void* userData)
{
	int numMatrixChannels = context->analogInChannels;
	int numAudioChannels = context->audioInChannels;
	int numAudioFramesPerPeriod = context->audioFrames;
	float matrixSampleRate = context->analogSampleRate;
	float audioSampleRate = context->audioSampleRate;

// logging data
	file1.init("out.bin"); //set the file name to write to
	file1.setEchoInterval(10000); // only print to the console every 10000 calls to log
	file1.setFileType(kBinary);
	// set the format that you want to use for your output.
	// Please use %f only (with modifiers).
	// When in binary mode, this is used only for echoing to console
	file1.setFormat("binary: %.4f %.4f\n"); 
	file2.init("out.m"); //set the file name to write to
	file2.setHeader("myvar=[\n"); //set one or more lines to be printed at the beginning of the file
	file2.setFooter("];\n"); //set one or more lines to be printed at the end of the file
	file2.setFormat("%.4f\n"); // set the format that you want to use for your output. Please use %f only (with modifiers)
	file2.setFileType(kText);
	file2.setEchoInterval(10000); // only print to the console 1 line every other 10000 /*



	/*
	 * Calculating filter coefficients
	 * default is 20Hz
	 */

	float filterFrequency = 300;
	float Q = sqrt(2.0)*0.5; // quality factor for Butterworth filter
	float T = 1/audioSampleRate; //period T, inverse of sampling frequency
	float w = 2.0*M_PI*filterFrequency; //angular freq in radians per sec

	float delta = (4.0+((2*T)*(w/Q))+(w*w*T*T)); //factor to simplify the equation

	//calculate High Pass coefficients
	gHP_a1= (-8+2*(T*T*w*w))/delta;
	gHP_a2= (4 - (2*T*w/Q)+(T*T*w*w))/delta;
	gHP_b0= 4/delta; // 0.904152;
	gHP_b1= -8/delta; //-1.808304;
	gHP_b2= gHP_b0;// 0.904152;

	/* //check and debug
	rt_printf("gHP_a1 is: %f\n", gHP_a1);
	rt_printf("gHP_a2 is: %f\n", gHP_a2);
	rt_printf("gHP_b0 is: %f\n", gHP_b0);
	rt_printf("gHP_b1 is: %f\n", gHP_b1);
	rt_printf("gHP_b2 is: %f\n", gHP_b2); */

	gInverseSampleRate = 1.0 / audioSampleRate;
	gPhase = 0.0;
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


	//gEmPhase = phaseReading * M_PI / 0.03;
    float phaseVoltage = phaseReading * gADCFullScale;  //4.096 full scale value of the converter of matrixIn
    gEmPhase = (- phaseVoltage * 100 +180)/180 * M_PI; // negative slope therefore minus, approx 0.03 to 0
	// stay in range
	while(gEmPhase >= 2.0 * M_PI)
	            gEmPhase -= 2.0 * M_PI;
	while(gEmPhase < 0)
	            gEmPhase +=2.0*M_PI;

	float gainVoltage = gainReading *  gADCFullScale;
	gEmGain =  gainVoltage*(gMaxGain - gMinGain) + gMinGain;
	// it is a value in decibel 30 mV per decibel
	/* TODO mapping dB to amplitude, exponential function
	 *  0dB = 1
	 *  -6 dB = 0.5
	 *  -12dB = 0.25
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
///	int numMatrixFrames = context->analogChannels;
//	int numAudioFrames = context->audioChannels;
//	float* audioIn = context->audioIn;
//	float* audioOut = context->audioOut;
//	float* matrixIn = context->analogIn;
//	float* matrixOut = context->analogOut;
//  now use analogIn or analogOut 
//  for the pointer
	
    int numMatrixFrames = context->analogInChannels;
	int numAudioFrames = context->audioInChannels;
	const float* audioIn = context->audioIn;
	float* audioOut = context->audioOut;
	const float* matrixIn = context->analogIn;
	float* matrixOut = context->analogOut;
	
	
 
	for(int n = 0; n < numAudioFrames; n++) {

         //matrixIn is defunct analogIn(context, frame to read on, channel);
		// analogIn(context, context->gAnalogFramesPerAudioFrames, 1);

		/* log data */
		file1.log(&(context->analogIn[n*context->analogFrames]), 2); // log an array of values
		file2.log(context->analogIn[n*context->analogFrames]); // log a single value


		// read the  PHASE (on the right)
		float phaseReading = matrixIn[n/2*8]; // pot value, interleaved, half audio rate
		//rt_printf("pot raw values are %f\n", matrixIn[n/2*8]); // test to set min and max value to be mapped */

		float phase = (gADCFullScale * phaseReading - gMinPhase) / (gMaxPhase - gMinPhase);
		// rt_printf("raw phase: %f, phase: %f\n", matrixIn[n/2*8], phase); // test to set min and max value to be mapped */

		// read the  amplitude / GAIN (on the left)
		float gainReading = matrixIn[n/2*8 + 4]; // pot value, interleaved, half audio rate
		//rt_printf("gain pot raw values are %f\n", matrixIn[n/2*8 + 4]); //test to set min and max value to be mapped */

		float gain = (gADCFullScale * gainReading - gMinGain) / (gMaxGain - gMinGain);

		//rt_printf("gain is  %f\n", gain);
		//rt_printf("gEmGain is  %f\n", gEmGain);
		gFrequency = phase*(gMaxFrequency - gMinFrequency) + gMinFrequency;

		float gainVoltage = gainReading *  gADCFullScale;

	        //gEmPhase = phaseReading * M_PI / 0.03; // wrong mapping but nice result and interaction

		float phaseVoltage = phaseReading * gADCFullScale;  //4.096 full scale value of the converter of matrixIn
//		gEmPhase = (- phaseVoltage * 100 +180)/180 * M_PI; // negative slope therefore minus, approx 0.03 to 0

		spectrify (phaseVoltage, gainVoltage);
		// rt_printf("phaseVoltage: %f; gEmPhase: %f\n", phaseVoltage, gEmPhase);

		//usleep(1000);

		if(counter == 100) {
			rt_printf("phaseVoltage: %f; gEmPhase: %f\n", phaseVoltage, gEmPhase);
                       rt_printf("gain: %f; gEmGain: %f\n", gain, gEmGain);
			counter = 0;
		}
		counter++;

		// rt_printf("gain: %f; gEmGain: %f\n", gain, gEmGain);

		// pass to output as filterIn to a high pass filter NO SQUARE
		float filterIn = gEmGain *  gPhase  + gEmGain * (gPhase+gEmPhase)
			+ (gEmPhase*gEmGain)  + ((gain + phase)*2 - 1); // eliminate offset and remap from 0:1 to -1:1 to keep in range


                // pass to output as filterIn to a high pass filter // with square
                //float filterIn = gEmGain *  square(gPhase)  + gEmGain * square(gPhase+gEmPhase)
                //        + (gEmPhase*gEmGain)  + ((gain + phase)*2 - 1); // eliminate offset and remap from 0:1 to -1:1 to keep in range



		/* anular modulation between phase and gain,
		 * not very significant from a physical point of view
		 * but it sounds good (gEmPhase*gEmGain), now commented
		 */

		//high pass filter on out with cutoff~20Hz to make sure no
		//DC offset is present at the DAC

		//2nd-order IIR (differential equation): y[n]= b0 x[n] + b1 x[n-1] + b2 x[n-2] - a1 y[n-1] - a2 y[n-2]

		//apply filter on output

		float out=(gHP_b0*filterIn)  +(gHP_b1*prevStateInHP) + (gHP_b2*pastStateInHP) - (gHP_a1*prevStateOutHP) - (gHP_a2*pastStateOutHP);

		// rt_printf("filterIn is  %f\n", filterIn);

		pastStateInHP=prevStateInHP;    //store current x[n-1] that will become x[n-2]
		prevStateInHP=filterIn;			//store current x[n] that will become x[n-1]

		pastStateOutHP=prevStateOutHP;  //store current y[n-1] that will become y[n-1]
		prevStateOutHP=out; //store current y[n] that will become y[n-1]

		out/=2;


		gPhase += 2.0 * M_PI * gFrequency * gInverseSampleRate;

		if(gPhase > 2.0 * M_PI)
			gPhase -= 2.0 * M_PI;

		for(int channel = 0; channel < context->audioInChannels; channel++)
			audioOut[n * context->audioOutChannels + channel] = out;

	}
}


void cleanup(BelaContext* context, void* userData)
{

}

