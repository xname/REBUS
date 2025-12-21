#ifndef ADSR_Envelope_h
#define ADSR_Envelope_h

class ADSR_Envelope{
	
	private:
		int audioSampleRate;
		char curveType;

		float attackStartLevel;
		float attackEndLevel;
		float sustainLevel;
		float releaseStartLevel;
		float releaseEndLevel;		
		
		float linearAttackIncrement;
		float linearDecayDecrement;
		float linearReleaseDecrement;

		bool gateValue;

		void calculateEnvelope();

		enum{
			stateOff = 0,
			attackPhase,
			decayPhase,
			sustainPhase,
			releasePhase
		};
		int envelopeState;

		float phase;
		
	public:
		ADSR_Envelope();
		void setup(
			int sampleRate, 
			float attackTimeInSeconds = 0.5, 
			float decayTimeInSeconds = 0.5, 
			float sustainLevel = 0.5, 
			float releaseTimeInSeconds = 0.5, 
			char curveType = 'l' 
		);
		void attackTimeInput(float);
		void decayTimeInput(float);
		void releaseTimeInput(float);
		void sustainLevelInput(float);
		void gateInput(bool gateValue);
		float output();

};


#endif
