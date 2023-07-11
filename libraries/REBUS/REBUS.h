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
// #define RECORD 1 before including to enable recording.
// #define RECORD 0 before including to keep recording disabled
// and hide explanatory startup messages.
// default to no recording, to avoid accidentally filling the storage.

#ifdef RECORD
// RECORD was defined externally, hide messages
#define RECORD_DEFINED 1
#else
// RECORD was not defined externally, print documentation messages
#define RECORD_DEFINED 0
// default to no recording
#define RECORD 0
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

// for the nothrow version of new (memory allocation and construction)
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

//---------------------------------------------------------------------

#if RECORD

// Record all the channels into a single multichannel file.
// Channel order:
// - audio output left
// - audio output right
// - audio input left
// - audio input right
// - magnitude/gain (from analog IO pin)
// - phase (from analog IO pin)
#define RECORD_CHANNELS 6

// RECORD_SIZE must be >= block size * channels
#define RECORD_SIZE (1024 * RECORD_CHANNELS)

// forward declare the non-realtime record task callback
template <typename COMPOSITION_T>
void REBUS_record(void *);

#endif

//---------------------------------------------------------------------

// state

template <typename COMPOSITION_T>
struct STATE
{

//---------------------------------------------------------------------
	COMPOSITION_T composition;
//---------------------------------------------------------------------


//---------------------------------------------------------------------
#if RECORD

	// recording state

	// this interleaved buffer is written by
	// the realtime audio thread
	// during the render callback
	float record_out[RECORD_SIZE];

	// this interleaved buffer is written by
	// the non-realtime sound file writer task
	// during the task callback
	float record_in[RECORD_SIZE];

	// the non-realtime sound file writer task
	AuxiliaryTask record_task;

	// communication from realtime audio to non-realtime sound file writer
	Pipe pipe;

	// output sound file handle
	SNDFILE *out_file;

	// the number of items stored in the record_out buffer
	// during each run of the realtime audio render callback
	unsigned int items;

#endif
//---------------------------------------------------------------------

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

//---------------------------------------------------------------------
#if RECORD

	// output audio file parameters
	SF_INFO info = {0};
	info.channels = RECORD_CHANNELS;
	info.samplerate = context->audioSampleRate;
	info.format = SF_FORMAT_WAV | SF_FORMAT_FLOAT;

	// create a filename based on the current date and time
	// with the composition name appended
	char record_path[1000];
	time_t seconds_since_epoch = time(0);
	struct tm *t = localtime(&seconds_since_epoch);
	if (t)
	{
		// human-readable datetime format
		snprintf(record_path, sizeof(record_path), "%04d-%02d-%02d-%02d-%02d-%02d-%s.wav", 1900 + t->tm_year, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec, COMPOSITION_name);
	}
	else
	{
		// cryptically-named fallback in case the localtime() function fails
		snprintf(record_path, sizeof(record_path), "%08x-%s.wav", (unsigned int) seconds_since_epoch, COMPOSITION_name);
	}

	// open the output sound file
	if (! (S->out_file = sf_open(record_path, SFM_WRITE, &info)))
	{
		rt_printf("Could not open '%s' for recording.\n", record_path);
		return false; // FIXME should this be a hard failure?
	}

	// create the pipe to communicate the record
	// from realtime audio to non-realtime writer
	S->pipe.setup("record-pipe", 65536, false, false);

	// create the non-realtime sound file writer task
	if (! (S->record_task = Bela_createAuxiliaryTask(&REBUS_record<COMPOSITION_T>, 90, "record")))
	{
		rt_printf("Could not create recorder task.\n");
		return false; // FIXME should this be a hard failure?
	}
	S->items = context->audioFrames * info.channels;
#endif
//---------------------------------------------------------------------

//---------------------------------------------------------------------
// composition render
	bool ok = COMPOSITION_setup(context, &S->composition);
//---------------------------------------------------------------------

	if (ok)
	{

		// print messages about the state of recording
		// if RECORD is externally defined to 0,
		// nothing is printed and recording is disabled
#if RECORD
		rt_printf("Recording to '%s'\n", record_path);
#else
#if ! RECORD_DEFINED
		rt_printf(
			"Not recording.\n"
			"'#define RECORD 1' before including the REBUS library to enable recording.\n"
			"'#define RECORD 0' before including the REBUS library to hide these messages.\n"
		);
#endif
#endif

	}
	return ok;
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
		// write data to interleaved buffer
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
	// send data to non-realtime audio file writer
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
	// cast to a pointer to the actual type of the structure in memory
	STATE<COMPOSITION_T> *S = (STATE<COMPOSITION_T> *) STATE_ptr;
	int ret;
	// receive interleaved data from the realtime audio thread...
	while (S->items && (ret = S->pipe.readNonRt(&S->record_in[0], S->items)) > 0 && S->out_file)
	{
		// ...and write it to the recording sound file
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
		// finish the recording
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
