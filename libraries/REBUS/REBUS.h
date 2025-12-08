#pragma once
//---------------------------------------------------------------------
/*

REBUS - Electromagnetic Interactions

https://xname.cc/rebus

composition template library
by Claude Heiland-Allen 2023-06-14
based on workshop template by xname 2023
audio recorder based on an example found on Bela forums
converted to library 2023-06-28
configurable scope and record channels added 2024-07-22

*/

//---------------------------------------------------------------------
// modes of operation (aka platform support)

// BELA on REBUS hardware using control via antenna
#define MODE_REBUS 0

// BELA on regular board using control via wires in pins
#define MODE_PINS 1

//---------------------------------------------------------------------
// set mode here, can be overriden via compilation flags

#ifndef MODE
#define MODE MODE_REBUS
#endif

//---------------------------------------------------------------------
// available channels for scope and recording
enum CHANNEL
{
	OUT_LEFT = 0,
	OUT_RIGHT = 1,
	IN_LEFT = 2,
	IN_RIGHT = 3,
	GAIN = 4,
	PHASE = 5
};

//---------------------------------------------------------------------
// set recording preference, can be overriden via compilation flags
// #define RECORD 1 before including to enable recording.
// #define RECORD 0 before including to keep recording disabled
// and hide explanatory startup messages.
//
// default to no recording, to avoid accidentally filling the storage.
//
// #define RECORD_CHANNELS n to set number of recording channels
// #define RECORD_CHANNEL_x y to configure recording channels
// where x is in 1 to 6 and y is a channel name listed above
//
// with default settings, recording adds 10-15% to the CPU load
// this additional cost reduces with larger block sizes

#ifdef RECORD
// RECORD was defined externally, hide messages
#define RECORD_DEFINED 1
#else
// RECORD was not defined externally, print documentation messages
#define RECORD_DEFINED 0
// default to no recording
#define RECORD 0
#endif

// set recorder channel preferences
#if RECORD
#ifdef RECORD_CHANNELS
#if ! (1 <= RECORD_CHANNELS && RECORD_CHANNELS <= 6)
#error RECORD_CHANNELS must be between 1 and 6 inclusive
#endif
#define RECORD_CHANNELS_DEFINED 1
#else
#define RECORD_CHANNELS_DEFINED 0
#define RECORD_CHANNELS 6
#endif
#ifndef RECORD_CHANNEL_1
#define RECORD_CHANNEL_1 OUT_LEFT
#endif
#ifndef RECORD_CHANNEL_2
#define RECORD_CHANNEL_2 OUT_RIGHT
#endif
#ifndef RECORD_CHANNEL_3
#define RECORD_CHANNEL_3 IN_LEFT
#endif
#ifndef RECORD_CHANNEL_4
#define RECORD_CHANNEL_4 IN_RIGHT
#endif
#ifndef RECORD_CHANNEL_5
#define RECORD_CHANNEL_5 GAIN
#endif
#ifndef RECORD_CHANNEL_6
#define RECORD_CHANNEL_6 PHASE
#endif
#endif

//---------------------------------------------------------------------
// set oscilloscope preference, can be overriden via compilation flags
// #define SCOPE 1 before including to enable oscilloscope
// and hide explanatory startup messages.
// #define SCOPE 0 before including to disable oscilloscope.
//
// default to enabled oscilloscope, because it is useful
//
// #define SCOPE_CHANNELS n to set number of scope channels
// #define SCOPE_CHANNEL_x y to configure scope channels
// where x is in 1 to 6 and y is a channel name listed above

#ifdef SCOPE
// SCOPE was defined externally, hide messages
#define SCOPE_DEFINED 1
#else
// SCOPE was not defined externally, print documentation messages
#define SCOPE_DEFINED 0
// default to enabled oscilloscope
#define SCOPE 1
#endif

// set oscilloscope channel preferences
#if SCOPE
#ifdef SCOPE_CHANNELS
#if ! (1 <= SCOPE_CHANNELS && SCOPE_CHANNELS <= 6)
#error SCOPE_CHANNELS must be between 1 and 6 inclusive
#endif
#define SCOPE_CHANNELS_DEFINED 1
#else
#define SCOPE_CHANNELS_DEFINED 0
#define SCOPE_CHANNELS 6
#endif
#ifndef SCOPE_CHANNEL_1
#define SCOPE_CHANNEL_1 OUT_LEFT
#endif
#ifndef SCOPE_CHANNEL_2
#define SCOPE_CHANNEL_2 OUT_RIGHT
#endif
#ifndef SCOPE_CHANNEL_3
#define SCOPE_CHANNEL_3 IN_LEFT
#endif
#ifndef SCOPE_CHANNEL_4
#define SCOPE_CHANNEL_4 IN_RIGHT
#endif
#ifndef SCOPE_CHANNEL_5
#define SCOPE_CHANNEL_5 GAIN
#endif
#ifndef SCOPE_CHANNEL_6
#define SCOPE_CHANNEL_6 PHASE
#endif
#endif

//---------------------------------------------------------------------

#if MODE != MODE_REBUS && MODE != MODE_PINS
#error REBUS: unsupported MODE
#endif

//---------------------------------------------------------------------

#if MODE == MODE_REBUS

// the antenna is connected to these analog inputs
#define PHASE_PIN 0
#define MAGNITUDE_PIN 4

// magic numbers for mapping from antenna input
#define PHASE_MIN 0.01
#define PHASE_MAX 0.44
#define MAGNITUDE_MIN 0.150467
#define MAGNITUDE_MAX 0.3

// REBUS doesn't need a mains hum notch filter
#ifdef CONTROL_NOTCH
#define CONTROL_NOTCH_DEFINED 1
#else
#define CONTROL_NOTCH_DEFINED 0
#define CONTROL_NOTCH 0
#endif

// REBUS sometimes needs a noise reduction low pass filter
// default to off, enable only when necessary
#ifdef CONTROL_LOP
#define CONTROL_LOP_DEFINED 1
#else
#define CONTROL_LOP_DEFINED 0
#define CONTROL_LOP 0
#endif

#else

//---------------------------------------------------------------------

#if MODE == MODE_PINS

// the wires are connected to these analog inputs
#define PHASE_PIN 0
#define MAGNITUDE_PIN 4

// magic numbers for mapping from wire input
#define PHASE_MIN 0
#define PHASE_MAX 1
#define MAGNITUDE_MIN 0
#define MAGNITUDE_MAX 1

// wires benefit from a mains hum notch filter
#ifdef CONTROL_NOTCH
#define CONTROL_NOTCH_DEFINED 1
#else
#define CONTROL_NOTCH_DEFINED 0
#define CONTROL_NOTCH 1
#endif

// wires benefit from a noise reduction low pass filter
#ifdef CONTROL_LOP
#define CONTROL_LOP_DEFINED 1
#else
#define CONTROL_LOP_DEFINED 0
#define CONTROL_LOP 1
#endif

#endif
#endif

//---------------------------------------------------------------------

// mains hum removal notch filter parameters
// only used when CONTROL_NOTCH is not 0

#ifdef MAINS_HUM_FREQUENCY
#define MAINS_HUM_FREQUENCY_DEFINED 1
#else
#define MAINS_HUM_FREQUENCY_DEFINED 0
#define MAINS_HUM_FREQUENCY 50
#endif

#ifdef MAINS_HUM_QFACTOR
#define MAINS_HUM_QFACTOR_DEFINED 1
#else
#define MAINS_HUM_QFACTOR_DEFINED 0
#define MAINS_HUM_QFACTOR 3
#endif

//---------------------------------------------------------------------

// composition status reporting
// defaults to enabled
// #define REPORT_STATUS 0 before including to disable
#ifdef REPORT_STATUS
#define REPORT_STATUS_DEFINED 1
#else
#define REPORT_STATUS_DEFINED 0
#define REPORT_STATUS 1
#endif

//---------------------------------------------------------------------
// dependencies

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <time.h>
// for the nothrow version of new (memory allocation and construction)
#include <new>

#include <Bela.h>

#if SCOPE
#include <libraries/Scope/Scope.h>
#endif

#if RECORD
#include <libraries/Pipe/Pipe.h>
#include <libraries/sndfile/sndfile.h>
#endif

#if CONTROL_NOTCH || CONTROL_LOP
#include "dsp.h"
#endif

//---------------------------------------------------------------------
// composition API, to be implemented by client code

extern const char *COMPOSITION_name;
struct COMPOSITION;
bool COMPOSITION_setup(BelaContext *context, struct COMPOSITION *C);
void COMPOSITION_render(BelaContext *context, struct COMPOSITION *C, int n, float out[2], const float in[2], const float magnitude, const float phase);
void COMPOSITION_cleanup(BelaContext *context, struct COMPOSITION *C);

//---------------------------------------------------------------------

#if RECORD

// RECORD_SIZE must be >= block size * channels
#define RECORD_SIZE 65536

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

	// composition state
	COMPOSITION_T composition;

//---------------------------------------------------------------------

#if RECORD

	// recording state

	// this interleaved buffer is written by
	// the realtime audio thread
	// during the render callback
	float recordOut[RECORD_SIZE];

	// this interleaved buffer is written by
	// the non-realtime sound file writer task
	// during the task callback
	float recordIn[RECORD_SIZE];

	// the non-realtime sound file writer task
	AuxiliaryTask recordTask;

	// communication from realtime audio to non-realtime sound file writer
	Pipe pipe;

	// output sound file handle
	SNDFILE *outFile;

	// the number of items stored in the recordOut buffer
	// during each run of the realtime audio render callback
	unsigned int items;

#endif

//---------------------------------------------------------------------

#if SCOPE

	// oscilloscope state
	Scope scope;

#endif

//---------------------------------------------------------------------

#if CONTROL_NOTCH

	// notch filter state
	BIQUAD notch[2];

#endif

//---------------------------------------------------------------------

#if CONTROL_LOP

	// low pass filter state
	LOP lop[2];

#endif

//---------------------------------------------------------------------

#if REPORT_STATUS

	// how often to report composition status
	int blocksPerReport;

	// counts elapsed
	int blocksElapsed;

#endif

//---------------------------------------------------------------------

};

void *STATE_ptr = nullptr;

//---------------------------------------------------------------------
// setup

template <typename COMPOSITION_T>
bool REBUS_setup(BelaContext *context, void *userData)
{

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
	char recordPath[1000];
	time_t secondsSinceEpoch = time(0);
	struct tm *t = localtime(&secondsSinceEpoch);
	if (t)
	{
		// human-readable datetime format
		snprintf(recordPath, sizeof(recordPath), "%04d-%02d-%02d-%02d-%02d-%02d-%s.wav", 1900 + t->tm_year, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec, COMPOSITION_name);
	}
	else
	{
		// cryptically-named fallback in case the localtime() function fails
		snprintf(recordPath, sizeof(recordPath), "%08x-%s.wav", (unsigned int) secondsSinceEpoch, COMPOSITION_name);
	}

	// open the output sound file
	if (! (S->outFile = sf_open(recordPath, SFM_WRITE, &info)))
	{
		rt_printf("Could not open '%s' for recording.\n", recordPath);
		return false; // FIXME should this be a hard failure?
	}

	// create the pipe to communicate the record
	// from realtime audio to non-realtime writer
	S->pipe.setup("record-pipe", 65536, false, false);

	// create the non-realtime sound file writer task
	if (! (S->recordTask = Bela_createAuxiliaryTask(&REBUS_record<COMPOSITION_T>, 90, "record")))
	{
		rt_printf("Could not create recorder task.\n");
		return false; // FIXME should this be a hard failure?
	}
	S->items = context->audioFrames * info.channels;

#endif

//---------------------------------------------------------------------

#if SCOPE

	// set up six channel oscilloscope at audio rate
	S->scope.setup(SCOPE_CHANNELS, context->audioSampleRate);

#endif

//---------------------------------------------------------------------

#if CONTROL_NOTCH

	// clear filter state to 0
	std::memset(&S->notch, 0, sizeof(S->notch));

	// compute biquad filter coefficients
	notch(&S->notch[0], MAINS_HUM_FREQUENCY, MAINS_HUM_QFACTOR);
	notch(&S->notch[1], MAINS_HUM_FREQUENCY, MAINS_HUM_QFACTOR);

#endif

//---------------------------------------------------------------------

#if CONTROL_LOP

	// clear filter state to 0
	std::memset(&S->lop, 0, sizeof(S->lop));

#endif

//---------------------------------------------------------------------

#if REPORT_STATUS

	// report status every 30secs
	float secondsPerReport = 30;
	S->blocksPerReport = secondsPerReport * context->audioSampleRate / context->audioFrames;
	S->blocksElapsed = 0;

#endif

//---------------------------------------------------------------------

	// composition setup
	bool ok = COMPOSITION_setup(context, &S->composition);

//---------------------------------------------------------------------

	if (ok)
	{
		rt_printf("Starting composition '%s'.\n", COMPOSITION_name);

		// print messages about the state of recording
#if RECORD
		rt_printf(
			"Recording to '%s'."
#if ! RECORD_DEFINED
			"  '#define RECORD 0' before including REBUS to disable recording."
#endif
			"\n"
		, recordPath);
#else
		rt_printf(
			"Recording disabled."
#if ! RECORD_DEFINED
			"  '#define RECORD 1' before including REBUS to enable recording."
#endif
			"\n"
		);
#endif

		// print messages about the state of oscilloscope
#if SCOPE
		rt_printf(
			"Oscilloscope enabled."
#if ! SCOPE_DEFINED
			"  '#define SCOPE 0' before including REBUS to disable oscilloscope."
#endif
			"\n"
		);
#else
		rt_printf(
			"Oscilloscope disabled.\n"
#if ! SCOPE_DEFINED
			"  '#define SCOPE 1' before including REBUS to enable oscilloscope.\n"
#endif
			"\n"
		);
#endif

		// print messages about the state of control filtering
#if CONTROL_NOTCH
		rt_printf("Using notch filter at %f Hz, Q %f to reduce mains hum.\n", (double) MAINS_HUM_FREQUENCY, (double) MAINS_HUM_QFACTOR);
#endif
#if CONTROL_LOP
		rt_printf("Using low pass filter at %f Hz to reduce noise.\n", (double) 10);
#endif

	}
	else
	{
		rt_printf("Setup failed for composition '%s'.\n", COMPOSITION_name);
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

		// get controls from analog IO pins
		unsigned int m = n / 2; // FIXME depends on analog IO sample rate
		const float rawPhase = analogRead(context, m, PHASE_PIN);
		const float rawMagnitude = analogRead(context, m, MAGNITUDE_PIN);

		float phase = rawPhase;
		float magnitude = rawMagnitude;

#if CONTROL_NOTCH
		// try to remove mains hum using a biquad notch filter
		phase = biquad(&S->notch[0], phase);
		magnitude = biquad(&S->notch[1], magnitude);
#endif

#if CONTROL_LOP
		// try to remove noise using a low pass filter at 10 Hz
		phase = lop(&S->lop[0], phase, 10);
		magnitude = lop(&S->lop[1], magnitude, 10);
#endif

		// map to 0..1 range (FIXME remove this:
		// this mapping should be done in the composition for efficiency
		// because composition likely needs to do mapping too
		// and mapping twice is waste of computational resources)
		phase = map(phase, PHASE_MIN, PHASE_MAX, 0, 1);
		magnitude = map(magnitude, MAGNITUDE_MIN, MAGNITUDE_MAX, 0, 1);

		// render
		float out[2] = { 0.0f, 0.0f };

//---------------------------------------------------------------------
// composition render
		COMPOSITION_render(context, &S->composition, n, out, in, magnitude, phase);
//---------------------------------------------------------------------

		// output
		// compile-time conditionals avoid wasted per-sample work

#if SCOPE || RECORD
		// store available channels for permuation below
		float channels[6] = { out[0], out[1], in[0], in[1], magnitude, phase };
#endif

#if SCOPE
		// write data to oscilloscope
		float scope_data[SCOPE_CHANNELS];
#if SCOPE_CHANNELS > 0
		scope_data[0] = channels[CHANNEL::SCOPE_CHANNEL_1];
#endif
#if SCOPE_CHANNELS > 1
		scope_data[1] = channels[CHANNEL::SCOPE_CHANNEL_2];
#endif
#if SCOPE_CHANNELS > 2
		scope_data[2] = channels[CHANNEL::SCOPE_CHANNEL_3];
#endif
#if SCOPE_CHANNELS > 3
		scope_data[3] = channels[CHANNEL::SCOPE_CHANNEL_4];
#endif
#if SCOPE_CHANNELS > 4
		scope_data[4] = channels[CHANNEL::SCOPE_CHANNEL_5];
#endif
#if SCOPE_CHANNELS > 5
		scope_data[5] = channels[CHANNEL::SCOPE_CHANNEL_6];
#endif
		S->scope.log(scope_data);
#endif

#if RECORD
		// write data to interleaved buffer
#if RECORD_CHANNELS > 0
		S->recordOut[RECORD_CHANNELS * n + 0] = channels[CHANNEL::RECORD_CHANNEL_1];
#endif
#if RECORD_CHANNELS > 1
		S->recordOut[RECORD_CHANNELS * n + 1] = channels[CHANNEL::RECORD_CHANNEL_2];
#endif
#if RECORD_CHANNELS > 2
		S->recordOut[RECORD_CHANNELS * n + 2] = channels[CHANNEL::RECORD_CHANNEL_3];
#endif
#if RECORD_CHANNELS > 3
		S->recordOut[RECORD_CHANNELS * n + 3] = channels[CHANNEL::RECORD_CHANNEL_4];
#endif
#if RECORD_CHANNELS > 4
		S->recordOut[RECORD_CHANNELS * n + 4] = channels[CHANNEL::RECORD_CHANNEL_5];
#endif
#if RECORD_CHANNELS > 5
		S->recordOut[RECORD_CHANNELS * n + 5] = channels[CHANNEL::RECORD_CHANNEL_6];
#endif
#endif

		// write audio output
		audioWrite(context, n, 0, out[0]);
		audioWrite(context, n, 1, out[1]);
	}

#if RECORD
	// send data to non-realtime audio file writer
	S->pipe.writeRt(&S->recordOut[0], S->items);
	Bela_scheduleAuxiliaryTask(S->recordTask);
#endif

#if REPORT_STATUS
	// report status every so often
	if (++(S->blocksElapsed) >= S->blocksPerReport)
	{
		rt_printf("Composition '%s' is still running.\n", COMPOSITION_name);
		S->blocksElapsed = 0;
	}
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
	while (S->items && (ret = S->pipe.readNonRt(&S->recordIn[0], S->items)) > 0 && S->outFile)
	{
		// ...and write it to the recording sound file
		sf_write_float(S->outFile, &S->recordIn[0], ret);
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
		if (S->outFile)
		{
			sf_write_sync(S->outFile);
			sf_close(S->outFile);
			S->outFile = nullptr;
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
