/*
 15/09/2019
 AndyNRG
 
 This class 
 	returns a 0 if the sensor is not being used after a certain amount of time
 	otherwise returns 1


 This class has therefore two methods
 	-) setup; parameters: sampleRate, timeToSleep, sensorReferenceValueForSleep, sensorToleranceRangeForSleep
 	-) stateOutput; returns 0 or 1
 	
 */

#ifndef Sensor_h
#define Sensor_h

class Sensor
{
	private:
		int sampleRate;
		
		//autoCalibration variables
		bool stateOfTheCalibrationProcess;			// false (happening) true (terminated)
		float calibrationTimeInSeconds;
		int calibrationTimeInSamples;
		float* sensorDataArray;
		int currentSampleNumber;					// this var stores the number of samples we read since the start of the calibration process 

		//sensorState variables
		float sensorReferenceValueForNoInteraction;	// the value produced by the sensor when no interaction is performed
		float sensorToleranceRangeForNoInteraction; // even if no interaction is performed, sensorValue might change slightly due to other reasons
													// so we specifiy a range as wide as the variations of the sensorValue in absence of human interaction
		float halfToleranceRange;					// in the implementation, the range is split in two halves which represents its extrems
													// bottom: sensorReferenceValueForSleep-halfToleranceRange; top: sensorReferenceValueForSleep+halfToleranceRange
													// i.e. referenceValue = 0.5; and we measured variations in a range of 0.2; then toleranceRange = 0.2 and range = (0.3 to 0.7)
		float toleranceRangeBottom;
		float toleranceRangeTop;
											
													// the code is implemented as a State_Machine
		int noInteractionState;						// this state identify a lack of interaction on the sensor
		int interactionState;						// this state identifies a clear interaction 
		int notSureState;							// there's a case where we're not sure if there's interaction or noInteraction
													// example: noInteractionReferenceValue = 0.5; but increasing the interaction the sensor goes from 0.6 to 1 and then fall to 0
													// increasing the interaction the sensor cross the noInteraction range (noInteractionReferenceValue-half range to noInteractionReferenceValue+halfRange)
													// how we discriminate if the sensorValue falls inside the noInteraction range because there's no interaction or because we are increasing the interaction? 
													// solution: whenever we enter the nointeractionRange we start a countdown. If we reach 0 it means that for x seconds there was no interaction. 
													// If so, we can assume the sensor is not being used; so we return 0. In the rest of the cases we return 1
		int currentState;							// This variable holds the currentState of the stateMachine


		float timeToSleepInSeconds;					// Time in seconds for the countdown to no interaction (metaphorically described as sleep)
		float timeToSleepInSamples;					// Variable to store the previous value in samples
	
		int countdown;								// Variable used to store the countdown
	
	
	public:
		Sensor();															// default constructor
	
		void setup(															// constructor
			int audioSampleRate = 44100,
			float calibrationTimeInSeconds = 1,
			float timeToSleepInSeconds = 3,
			float sensorReferenceValueForNoInteraction = 0.0,				// 0 by default (i.e. no interaction corresponds to a sensorVal = 0)
			float sensorToleranceRangeForNoInteraction = 0.1				// 0.1 by deafault (range = -0.1 to 0.1)
		);

		bool sensorCalibration(float sensorValue);							// method to automatically set the ref val for no interaction by taking an average x readings of sensor value
																			// return false when the process it's terminated, otherwise returns true (process terminated)
																			// by reading a given number of sensor values (while no interaction is performed) 
																			// and then by performing an average
		
		void setReferenceValueForNoInteraction(float val);					// method to manually set the ref val for no interaction

		int stateOutput(float sensorValue);									// returns 0 if no interaction is being performed or 1 if interactino is being performed
																			// takes the sensorValue as the input value to perform tests and return the sensorState
};	


#endif
