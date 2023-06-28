#pragma once
//---------------------------------------------------------------------
/*

REBUS - Electromagnetic Interactions

https://xname.cc/rebus

composition template library
by Claude Heiland-Allen 2023-06-14
based on workshop template by xname 2023
audio recorder based on an example found on Bela forums
GUI mouse input based on an example found in Bela SDK
MODE_SNDFILE added 2023-06-27
converted to library 2023-06-28

*/

//---------------------------------------------------------------------
// modes of operation (aka platform support)

// BELA on REBUS hardware using control via antenna
#define MODE_REBUS 0

// BELA on regular board using control via mouse cursor in GUI window
#define MODE_BELA 1

//---------------------------------------------------------------------
// set mode here, can be overriden via compilation flags

#ifndef MODE
#define MODE MODE_REBUS
#endif

//---------------------------------------------------------------------
// set recording preference, can be overriden via compilation flags

#ifndef RECORD
#define RECORD 1
#endif

//---------------------------------------------------------------------

#if MODE != MODE_REBUS && MODE != MODE_BELA
#error REBUS: unsupported MODE
#endif

//---------------------------------------------------------------------

#include <Bela.h>
#include <libraries/Gui/Gui.h>
#include <libraries/Scope/Scope.h>
#include <libraries/Pipe/Pipe.h>
#include <libraries/ne10/NE10.h>
#include <libraries/sndfile/sndfile.h>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <time.h>
#include <new>

//---------------------------------------------------------------------
// composition API, to be implemented by client code

extern const char *COMPOSITION_name;
struct COMPOSITION;
bool COMPOSITION_setup(BelaContext *context, struct COMPOSITION *C);
void COMPOSITION_render(BelaContext *context, struct COMPOSITION *C, float out[2], const float in[2], const float magnitude, const float phase);
void COMPOSITION_cleanup(BelaContext *context, struct COMPOSITION *C);

//---------------------------------------------------------------------

#define SCOPE 1
#define GUI (MODE == MODE_BELA)
#ifndef RECORD
#define RECORD 1
#endif

//---------------------------------------------------------------------

#if RECORD
// must be >= block size * channels
#define RECORD_CHANNELS 6
#define RECORD_SIZE (1024 * RECORD_CHANNELS)
template <typename COMPOSITION_T>
void REBUS_record(void *);
#endif

// state

template <typename COMPOSITION_T>
struct STATE
{

//---------------------------------------------------------------------
	COMPOSITION_T composition;
//---------------------------------------------------------------------

#if RECORD
	// recording
	float record_out[RECORD_SIZE];
	float record_in[RECORD_SIZE];
	unsigned int record_ix;
	AuxiliaryTask record_task;
	Pipe pipe;
	SNDFILE *out_file;
	unsigned int items;
#endif

};

void *STATE_ptr = nullptr;

#if GUI
Gui gui;
#endif

#if SCOPE
Scope scope;
#endif

//---------------------------------------------------------------------
// setup

template <typename COMPOSITION_T>
bool REBUS_setup(BelaContext *context, void *userData)
{

#if GUI
	gui.setup(context->projectName);
	gui.setBuffer('f', 2);
#endif

#if SCOPE
	scope.setup(6, context->audioSampleRate);
#endif

	// allocate state
	auto S = new(std::nothrow) STATE<COMPOSITION_T>();
	if (! S)
	{
		return false;
	}
	STATE_ptr = S;

#if RECORD
	// recording
	SF_INFO info = {0};
	info.channels = RECORD_CHANNELS;
	info.samplerate = context->audioSampleRate;
	info.format = SF_FORMAT_WAV | SF_FORMAT_FLOAT;
	char record_path[1000];
	time_t seconds_since_epoch = time(0);
	struct tm *t = localtime(&seconds_since_epoch);
	if (t)
	{
		snprintf(record_path, sizeof(record_path), "%04d-%02d-%02d-%02d-%02d-%02d-%s.wav", 1900 + t->tm_year, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec, COMPOSITION_name);
	}
	else
	{
		snprintf(record_path, sizeof(record_path), "%08x-%s.wav", (unsigned int) seconds_since_epoch, COMPOSITION_name);
	}
	if (! (S->out_file = sf_open(record_path, SFM_WRITE, &info)))
	{
		return false;
	}
	S->pipe.setup("record-pipe", 65536, false, false);
	if (! (S->record_task = Bela_createAuxiliaryTask(&REBUS_record<COMPOSITION_T>, 90, "record")))
	{
		return false;
	}
	S->items = context->audioFrames * info.channels;
#endif

//---------------------------------------------------------------------
// composition render
	return COMPOSITION_setup(context, &S->composition);
//---------------------------------------------------------------------
}

//---------------------------------------------------------------------
// render

template <typename COMPOSITION_T>
void REBUS_render(BelaContext *context, void *userData)
{
	STATE<COMPOSITION_T> *S = (STATE<COMPOSITION_T> *) STATE_ptr;
	for (unsigned int n = 0; n < context->audioFrames; ++n)
	{
		// get audio inputs
		float in[2] = { 0.0f, 0.0f };
		in[0] = audioRead(context, n, 0);
		in[1] = audioRead(context, n, 1);

		// get controls

#if MODE == MODE_REBUS
		// magic numbers for mapping from antenna input
		const unsigned int phase_channel = 0;
		const float phase_min = 0.01;
		const float phase_max = 0.44;
		const unsigned int magnitude_channel = 4;
		const float magnitude_min = 0.150467;
		const float magnitude_max = 0.3;
		unsigned int m = n / 2; // FIXME depends on sample rate
		const float phase = map(analogRead(context, m, phase_channel), phase_min, phase_max, 0, 1);
		const float magnitude = map(analogRead(context, m, magnitude_channel), magnitude_min, magnitude_max, 0, 1);

#elif MODE == MODE_BELA
		DataBuffer& buffer = gui.getDataBuffer(0);
		float* data = buffer.getAsFloat();
		const float phase = data[0];
		const float magnitude = data[1];

#endif

		// render
		float out[2] = { 0.0f, 0.0f };

//---------------------------------------------------------------------
// composition render
		COMPOSITION_render(context, &S->composition, out, in, magnitude, phase);
//---------------------------------------------------------------------

		// output
#if SCOPE
		scope.log(out[0], out[1], in[0], in[1], magnitude, phase);
#endif
#if RECORD
		// interleaved
		S->record_out[RECORD_CHANNELS * n + 0] = out[0];
		S->record_out[RECORD_CHANNELS * n + 1] = out[1];
		S->record_out[RECORD_CHANNELS * n + 2] = in[0];
		S->record_out[RECORD_CHANNELS * n + 3] = in[1];
		S->record_out[RECORD_CHANNELS * n + 4] = magnitude;
		S->record_out[RECORD_CHANNELS * n + 5] = phase;
#endif
		audioWrite(context, n, 0, out[0]);
		audioWrite(context, n, 1, out[1]);
	}

#if RECORD
	// record
	S->pipe.writeRt(&S->record_out[0], S->items);
	Bela_scheduleAuxiliaryTask(S->record_task);
#endif
}

//---------------------------------------------------------------------
// recording task

#if RECORD
template <typename COMPOSITION_T>
void REBUS_record(void *)
{
	STATE<COMPOSITION_T> *S = (STATE<COMPOSITION_T> *) STATE_ptr;
	int ret;
	while (S->items && (ret = S->pipe.readNonRt(&S->record_in[0], S->items)) > 0 && S->out_file)
	{
		sf_write_float(S->out_file, &S->record_in[0], ret);
	}
}
#endif

//---------------------------------------------------------------------
// cleanup

template <typename COMPOSITION_T>
void REBUS_cleanup(BelaContext *context, void *userData)
{
	STATE<COMPOSITION_T> *S = (STATE<COMPOSITION_T> *) STATE_ptr;
	if (S)
	{

#if RECORD
		// stop recording
		if (S->out_file)
		{
			sf_write_sync(S->out_file);
			sf_close(S->out_file);
			S->out_file = nullptr;
		}
#endif

//---------------------------------------------------------------------
// composition cleanup
		COMPOSITION_cleanup(context, &S->composition);
//---------------------------------------------------------------------

		// free memory
		delete S;
		S = nullptr;

	}
}

//---------------------------------------------------------------------
// macro to instantiate default entry points

#define REBUS \
bool setup(BelaContext *context, void *userData) { return REBUS_setup<COMPOSITION>(context, userData); } \
void render(BelaContext *context, void *userData) { REBUS_render<COMPOSITION>(context, userData); } \
void cleanup(BelaContext *context, void *userData) { REBUS_cleanup<COMPOSITION>(context, userData); }

//---------------------------------------------------------------------
