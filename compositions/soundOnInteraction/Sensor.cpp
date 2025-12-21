/*
 15/09/2019
 AndyNRG
 
 This class deals with sensors
	methods
		identifies the sensor value that corresponds to no interaction automatically: sensorCalibration
		returns the state of the sensor (interacted, not interacted): stateOutput
		
*/

#include <Sensor.h>
#include <Bela.h>

Sensor::Sensor(){}

void Sensor::setup(int sampleRate, float calibrationTimeInSeconds, float timeToSleepInSeconds, float sensorReferenceValueForNoInteraction, float sensorToleranceRangeForNoInteraction)
{
    this-> sampleRate = sampleRate;
    
    // initialise variables in case we call the sensor calibration method (defines the ref value for no interaction)
	stateOfTheCalibrationProcess = true;
	this-> calibrationTimeInSeconds = calibrationTimeInSeconds;
    calibrationTimeInSamples = calibrationTimeInSeconds*sampleRate;
	sensorDataArray = new float [calibrationTimeInSamples];
    currentSampleNumber = 0;
    
    
    // initialise variables in case we want to check the state of the sensor (interaction - noInteraction)
    this-> timeToSleepInSeconds         		= timeToSleepInSeconds;
    this-> sensorReferenceValueForNoInteraction = sensorReferenceValueForNoInteraction;
    this-> sensorToleranceRangeForNoInteraction	= sensorToleranceRangeForNoInteraction;
    
    noInteractionState      = 0;
    interactionState    	= 1;
    notSureState        	= 2;
    currentState        	= noInteractionState;                                       	// we start from the noInteractionState (noInteraction)
    
    timeToSleepInSamples    = 44100*timeToSleepInSeconds;                               	// convert time to sleep from seconds to samples
    countdown               = timeToSleepInSamples;                                     	// initialise countdown (start from timeToSleepInSamples and decrement until 0 )
    halfToleranceRange      = sensorToleranceRangeForNoInteraction*0.5;                 	// divide range by 2
    toleranceRangeBottom    = sensorReferenceValueForNoInteraction - halfToleranceRange;    // buttom of the range
    toleranceRangeTop       = sensorReferenceValueForNoInteraction + halfToleranceRange;    // top of the range

}

bool Sensor::sensorCalibration(float sensorValue){
	if(stateOfTheCalibrationProcess == true){
		if(currentSampleNumber<calibrationTimeInSamples){
			    sensorDataArray[currentSampleNumber] = sensorValue;    						//store the reading into a dedicated array
				currentSampleNumber++;
		} 
		else{
		    stateOfTheCalibrationProcess = false;                   						//we can stop this process
		    float result = 0.0;
		    for(int i = 0; i < calibrationTimeInSamples; i++)
		        result += sensorDataArray[i];														// sum
		    delete[] sensorDataArray;																// we can free the space in the heap memory
		    result /= calibrationTimeInSamples;														// divide to get the average
		    sensorReferenceValueForNoInteraction = result;											// assign result and re-calculate tolerance range, bottom and top, according to the new reference
    		halfToleranceRange      = sensorToleranceRangeForNoInteraction*0.5;                 	// divide range by 2
    		toleranceRangeBottom    = sensorReferenceValueForNoInteraction - halfToleranceRange;    // buttom of the range
    		toleranceRangeTop       = sensorReferenceValueForNoInteraction + halfToleranceRange;    // top of the range
		    
		    rt_printf("end of calibration \n");							//print result
		    rt_printf("reference value for no interaction set to ");
		    rt_printf("%f",result);
		    rt_printf("\n");		
		}
	}
	return stateOfTheCalibrationProcess;
}

void Sensor::setReferenceValueForNoInteraction(float val){
	sensorReferenceValueForNoInteraction = val;												// update sensorReferenceValue which identifies no interaction
																							// re-calculate tolerance range, bottom and top, according to the new reference
    halfToleranceRange      = sensorToleranceRangeForNoInteraction*0.5;                 	// divide range by 2
    toleranceRangeBottom    = sensorReferenceValueForNoInteraction - halfToleranceRange;    // buttom of the range
    toleranceRangeTop       = sensorReferenceValueForNoInteraction + halfToleranceRange;    // top of the range

}

int Sensor::stateOutput(float sensorValue){
    int sensorState;                                                                    	// variable to store the sensorState 0 = noInteraction = sleep; 1 = interaction = awake
    
    if(currentState == noInteractionState){                                             	// if in noInteractionState it means there's no interaction
        sensorState = 0;                                                                	// set the interactionState to 0
        if(sensorValue <= toleranceRangeBottom || sensorValue >= toleranceRangeTop){    	// if sensorValue is outside the noInteractionRange it means we can weak up
            rt_printf("interactionState \n");
            currentState = interactionState;                                        	 	// enter the interaction state
        }
    }
    
    if(currentState == interactionState){                                               	// if in the interaction state
        sensorState = 1;                                                                	// set the interactionState to 1
        if(sensorValue >= toleranceRangeBottom && sensorValue <= toleranceRangeTop){    	// if sensorValue falls within the noInteractionRange
            rt_printf("notSureState \n");
            currentState = notSureState;                                                	// then we enter the notSureState (see sensorSleep.h file for description)
        }
    }
    
    if(currentState == notSureState){                                                   	// if in not Sure State
        sensorState = 1;                                                                	// keep the interaction state to 1 (we need time to decide)
        countdown--;                                                                    	// decrement the countdown until 0, one sample at the time
        
        if(sensorValue < toleranceRangeBottom ||sensorValue > toleranceRangeTop){       	// if sensorValue exits the the no interaction range
            rt_printf("countdown stopped at ");
            rt_printf("%i",countdown/sampleRate);
            rt_printf(" seconds from noInteractionState \n");
            rt_printf("interactionState \n");
            currentState = interactionState;                                            	// then we set the currenState to interactionState
            countdown = timeToSleepInSamples;                                           	// reset the countdown to its default value
        }
        else if((sensorValue >= toleranceRangeBottom && sensorValue <= toleranceRangeTop) && countdown <= 0){    // if the sensorValue remains inside the no interactino range until the end of the countdown                                                                    // reset the countdown to its default value
            rt_printf("noInteractionState \n");
            currentState = noInteractionState;                                                                   // it means there's no interaction, so we enter the noInteractionState
            countdown = timeToSleepInSamples;                                                                    // reset countdown
        }
    }
    
    return sensorState;
}





