//---------------------------------------------------------------------
// abstracted composition core
// this file is included by render.cpp

//---------------------------------------------------------------------
// composition state

struct COMPOSITION
{
	float increment;
	float phase[3];
};

//---------------------------------------------------------------------
// called during setup

static inline
bool COMPOSITION_setup(BelaContext *context, struct COMPOSITION *C)
{
	C->increment = 50.0f / context->audioSampleRate;
  return true;
}

//---------------------------------------------------------------------
// called once per audio frame (default 44100Hz sample rate)

static inline
void COMPOSITION_render(BelaContext *context, struct COMPOSITION *C, float out[2], const float in[2], const float magnitude, const float phase)
{
	C->phase[0] += C->increment;
	C->phase[1] += C->increment * (1.0f + 0.1f * phase);
	C->phase[2] += C->increment * (1.0f - 0.1f * phase);
	C->phase[0] -= std::floor(C->phase[0]);
	C->phase[1] -= std::floor(C->phase[1]);
	C->phase[2] -= std::floor(C->phase[2]);
	out[0] = (C->phase[1] - C->phase[0]) * magnitude;
	out[1] = (C->phase[2] - C->phase[0]) * magnitude;
}

//---------------------------------------------------------------------
// called during cleanup

static inline
void COMPOSITION_cleanup(BelaContext *context, struct COMPOSITION *C)
{
}

//---------------------------------------------------------------------
