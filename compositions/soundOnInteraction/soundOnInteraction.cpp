/*
 30/11/2019
 AndyNRG
 
 Code by Andrea Guidi aka UVCORE 
 
 The idea of this composition (xname 2019) is to have sound only when there is interaction, 
 for REBUS to be more akin to acoustic instruments. Thanks to Andrew McPherson for talking me through this
 
 this sketch
 	takes a sensor signal
 	filter the sensor signal to remove noise
 	measure the signal at startup for x seconds (see calibrationTimeInSeconds var)
 	use the measurements to determine the reference value produced by the sensor in absence of interaction
 	
 	when there's interaction the sketch fades in and plays a square wave
 	when there's no interaction the sketch fades out the sound of the square wave
 	there's no interaction when the value of the sensor sits for x seconds (see timeToSleepInSeconds var) within the range determined by
 		bottom of the no interaction range = reference value - toleranceRange/2
 		top of the no interaction range  = reference value + toleranceRange/2
 	
 	adjust the variables at the top of the code as you wish. avoid case limits (i.e. assigning 0 to one of them)
 
 	everything related to the sensor happens into the sensor class
 
 */

#include <Bela.h>
#include <Sensor.h>
#include <Filter.h>
#include <Oscillator.h>
#include <ADSR_Envelope.h>
#include <libraries/Scope/Scope.h>

Sensor sensor1;
Filter lowPass;
Oscillator osc0;
ADSR_Envelope env;
Scope scope;

float calibrationTimeInSeconds = 1;
float timeToSleepInSeconds = 1;
float toleranceValForNoInteraction = 0.1;

float oscFreq = 50;
char oscType = 'p';

float fadeInTimeInSec = 0.5;
float fadeOutTimeInSec = 0.5;

float gain = 0.5;


bool setup(BelaContext *context, void *userData){
    
    //SENSOR
    sensor1.setup(
    	context->audioSampleRate, 
    	calibrationTimeInSeconds, 
    	timeToSleepInSeconds, 
    	0, 								// refValForNoInteraction. It'll be set inside the render function during the calibration process 
    	toleranceValForNoInteraction
    );
    
    //FILTER
    lowPass.setup(
    	context->audioSampleRate, 
    	30.0							// cutoff freq for removing noise in sensor signal
    ); 
    
    //OSC
    osc0.setup(
    	context->audioSampleRate, 
    	oscFreq, 						// osc freq
    	'p'								// osc waveType
    );
    
    //ENVELOPE
	env.setup(
		context->audioSampleRate, 
		fadeInTimeInSec, 
		0,								// decayTime
		1, 								// sustain level
		fadeOutTimeInSec
	);
    
    scope.setup(4, context->audioSampleRate);
    
    return true;
}


void render(BelaContext *context, void *userData){
    
    for (unsigned int n = 0; n < context->audioFrames; n++){

        float sensorVal = analogRead(context, n/2, 0);              			//read sensor current val
        float filteredSensorVal = lowPass.audioInput(sensorVal);    			//smooth the reading by filtering (remove noise from the reading)
        scope.log(sensorVal, filteredSensorVal);								//visualise signal

		bool calibrationState = sensor1.sensorCalibration(filteredSensorVal);	//pass filteredSensorValue to calibration process. Returns calibrationState as bool
        if(calibrationState==false){											//if the sensorCalibration process it's over,we can consider the rest of the code

        	int sensorState = sensor1.stateOutput(filteredSensorVal);			//check the state of the sensor (0 = no interaction; 1 = interaction) and store it into sensorState variable
   			env.gateInput(sensorState);											//use sensorState variable to trigger the attack phase of the envelope (sensorState = 1 = fade in) or to trigger the release phase of the envelope (sensorState = 0 = fade out) 

            float out = osc0.output()*env.output();								//multiply oscillator signal for the envelope signal to achieve fade in or fade out (as described above)  || osc.output = sDesign
            for(int i = 0; i < 2; i++){											//for channel left and right
                audioWrite(context, n, i, out*gain);							//output audio signal
            }
        }
    }
}


void cleanup(BelaContext *context, void *userData){
}



