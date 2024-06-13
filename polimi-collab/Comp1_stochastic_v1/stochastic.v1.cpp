#include <Bela.h>
#include <libraries/Gui/Gui.h>
#include <cmath>
#include <cstdlib>
#include <libraries/Scope/Scope.h>
#include <memory>
#include <algorithm>

Gui myGui;

float gInverseSampleRate;

const int gNBreakpoints = 20;
int gBreakpointDurations[gNBreakpoints];
int gBreakpointStartingSample[gNBreakpoints];
float gBreakpointAmplitudes[gNBreakpoints];
int gWaveformLength = 0;
int gWaveformIndex = -1;
int gMinBreakpointDuration = 20;
int gMaxBreakpointDuration = 100;
float gMinBreakpointAmp = -0.1f;
float gMaxBreakpointAmp = 0.1f;
float gWaveformData[100000];
float gStepSizeRatio = 0.02f;
float gGain = 0.2f;

float gMinPhase = 0.01;
float gMaxPhase = 0.44;
float gMidPhase = 0.2;
float gMinGain = 0.15;
float gMaxGain = 0.3;

Scope scope;

float randFloat(float a, float b)
{
    return ((b - a) * ((float)rand() / RAND_MAX)) + a;
}

int clampInt(int x, int a, int b){
	if(x < a) x = a;
	if(x > b) x = b;
	return x;
}

float clampFloat(float x, float a, float b){
	if(x < a) x = a;
	if(x > b) x = b;
	return x;
}

void generateWaveform(BelaContext *context){
	for(int n=0; n<gWaveformLength; n++){
		
		// find left and right breakpoint indexes
		int leftBreakpointIndex = 0;
		int rightBreakpointIndex = 1;
		for(int i=0; i<gNBreakpoints; i++){
			if(i == gNBreakpoints-1){ // if we're on the last breakpoint
				leftBreakpointIndex = i;
				rightBreakpointIndex = 0;
				break;
			}
			if(n >= gBreakpointStartingSample[i] && n <= gBreakpointStartingSample[i+1]){
				leftBreakpointIndex = i;
				rightBreakpointIndex = i+1;
				break;
			}
		}
		
		// create waveform by interpolating
		int leftDuration = gBreakpointDurations[leftBreakpointIndex];
		float leftAmp = gBreakpointAmplitudes[leftBreakpointIndex];
		float rightAmp = gBreakpointAmplitudes[rightBreakpointIndex];
		float progress = (float)(n - gBreakpointStartingSample[leftBreakpointIndex]) / (float)leftDuration;
		float out = (1.0f - progress) * leftAmp + progress * rightAmp;
		
		// write waveform
		gWaveformData[n] = out * gGain;
	}
}

bool setup(BelaContext *context, void *userData)
{
	scope.setup(3, context->audioSampleRate);
	for(int i=0; i<100000; i++) gWaveformData[i]=0.0f;
	srand (time(NULL));
	gInverseSampleRate = 1.0 / context->audioSampleRate;

	myGui.setup(context->projectName);
	myGui.setBuffer('f', 2);
	
	// Initialize waveform with random amp and duration for every breakpoint
	int startingSample = 0;
	for(int i = 0; i<gNBreakpoints; i++){
		gBreakpointDurations[i] = (int) randFloat(gMinBreakpointDuration, gMaxBreakpointDuration);
		gBreakpointAmplitudes[i] = randFloat(gMinBreakpointAmp, gMaxBreakpointAmp);
		gWaveformLength += gBreakpointDurations[i];
		gBreakpointStartingSample[i] = startingSample;
		startingSample += gBreakpointDurations[i];
	}
	gBreakpointAmplitudes[0] = 0.0f;
	generateWaveform(context);
	
	return true;
}

void alterAmpParameters(){
	// change the amplitude of breakpoints with a bounded random walk
	for(int i=0; i<gNBreakpoints; i++){
		float maxStep = ((gMaxBreakpointAmp - gMinBreakpointAmp) * gStepSizeRatio);
		float ampStep = randFloat(-maxStep, maxStep);
		gBreakpointAmplitudes[i] = clampFloat(gBreakpointAmplitudes[i] + ampStep, gMinBreakpointAmp, gMaxBreakpointAmp);
	}
	gBreakpointAmplitudes[0] = 0.0f;
}

void alterDurationParameters(){
	int startingSample = 0;
	gWaveformLength = 0;
	for(int i = 0; i<gNBreakpoints; i++){
		float maxStep = (gMaxBreakpointDuration - gMinBreakpointDuration) * gStepSizeRatio;
		int durationStep = (int) randFloat(-maxStep, maxStep);
		gBreakpointDurations[i] = clampInt(gBreakpointDurations[i] + durationStep, gMinBreakpointDuration, gMaxBreakpointDuration);
		gWaveformLength += gBreakpointDurations[i];
		gBreakpointStartingSample[i] = startingSample;
		startingSample += gBreakpointDurations[i];
	}
}

void mapControlValues(float a, float b){
	a = clampFloat(a, 0.0f, 1.0f);
	b = clampFloat(b, 0.0f, 1.0f);
	gStepSizeRatio = a/20;
	gMinBreakpointAmp = -0.1f - b / 5;
	gMinBreakpointDuration = (int) randFloat(10, 20 + a*40);
	gMaxBreakpointDuration = gMinBreakpointDuration + ((int)(b * 100));
}

void render(BelaContext *context, void *userData)
{
	float out = 0.0f;
	float a = 0, b = 0;
	
	// Read data from gui controller
	//DataBuffer& buffer = myGui.getDataBuffer(0);
	//float* data = buffer.getAsFloat();
	//a = data[0];
	//b = data[1];
	
	alterAmpParameters();
	
	for(unsigned int n = 0; n < context->audioFrames; n++) {
		
		// Read data from pins, map it, use it
		a = analogRead(context, n/2, 4);
		b = analogRead(context, n/2, 0);
		a = (a-gMinGain)/(gMaxGain-gMinGain);
		b = (b-gMinPhase)/(gMaxPhase-gMinPhase);
		mapControlValues(a, b);
		
		// Read waveform data
		gWaveformIndex++;
		if(gWaveformIndex == gWaveformLength){ // generate new waveform when necessary
			gWaveformIndex = 0;
			alterDurationParameters();
			generateWaveform(context);
		}
		out = gWaveformData[gWaveformIndex];
		
		// Write to left and right channels
		for(unsigned int channel = 0; channel < context->audioOutChannels; channel++) {
				audioWrite(context, n, channel, out);
		}
		
		// Scope
		scope.log(out, a, b);
	}

}

void cleanup(BelaContext *context, void *userData)
{
}