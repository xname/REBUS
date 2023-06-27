//---------------------------------------------------------------------
/*

REBUS - Electromagnetic Interactions

https://xname.cc/rebus

composition template
by Claude Heiland-Allen 2023-06-14
based on workshop template by xname 2023
audio recorder based on an example found on Bela forums
GUI mouse input based on an example found in Bela SDK
MODE_SNDFILE added 2023-06-27

*/

//---------------------------------------------------------------------
// composition to use, can be overriden via compilation flags
#ifndef COMPOSITION_INCLUDE
#define COMPOSITION_INCLUDE "example.h"
#endif
//---------------------------------------------------------------------

// modes of operation (aka platform support)

// BELA on REBUS hardware using control via antenna
#define MODE_REBUS 0

// BELA on regular board using control via mouse cursor in GUI window
#define MODE_BELA 1

// JACK client for desktop using control via audio input
#define MODE_JACK 2

// offline operation using control via audio sound file input
#define MODE_SNDFILE 3

//---------------------------------------------------------------------
// set mode here, can be overriden via compilation flags
#ifndef MODE
#define MODE MODE_REBUS
#endif
//---------------------------------------------------------------------

//---------------------------------------------------------------------
// set recording preference, can be overriden via compilation flags
#ifndef RECORD
#define RECORD 1
#endif
//---------------------------------------------------------------------


#if MODE != MODE_REBUS && MODE != MODE_BELA && MODE != MODE_JACK && MODE != MODE_SNDFILE
#error unknown MODE
#endif


#if MODE == MODE_REBUS || MODE == MODE_BELA

#include <Bela.h>
#include <libraries/Gui/Gui.h>
#include <libraries/Scope/Scope.h>
#include <libraries/Pipe/Pipe.h>
#include <libraries/ne10/NE10.h>
#include <libraries/sndfile/sndfile.h>

#define SCOPE 1
#ifndef RECORD
#define RECORD 1
#endif

#define GUI (MODE == MODE_BELA)

#elif MODE == MODE_JACK || MODE == MODE_SNDFILE

#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#if MODE == MODE_JACK
#include <jack/jack.h>
#endif
#if MODE == MODE_SNDFILE
typedef int jack_nframes_t;
#include <sndfile.h>
#endif

#define rt_printf printf

// begin Bela Utilities.h

static inline float map(float x, float in_min, float in_max, float out_min, float out_max)
{
	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

static inline float constrain(float x, float min_val, float max_val)
{
	if(x < min_val) return min_val;
	if(x > max_val) return max_val;
	return x;
}

static inline float max(float x, float y){
	return x > y ? x : y;
}

static inline float min(float x, float y){
	return x < y ? x : y;
}

// end Bela Utilities.h

struct BelaContext
{
#if MODE == MODE_JACK
	jack_client_t *client;
	jack_port_t *in[4];
	jack_port_t *out[2];
#endif

#if MODE == MODE_SNDFILE
#define SF_CHANNELS 6
#define SF_BLOCKSIZE 1024
	SF_INFO info, outfo;
	SNDFILE *in, *out;
	float buffer[SF_BLOCKSIZE][SF_CHANNELS];
#endif

	float *in_buffer[4];
	float *out_buffer[2];

	unsigned int audioFrames;
	float audioSampleRate;
};

bool setup(BelaContext *context, void *userData);
void render(BelaContext *context, void *userData);
void cleanup(BelaContext *context, void *userData);

inline float audioRead(BelaContext *context, unsigned int frame, unsigned int channel)
{
	return context->in_buffer[channel][frame];
}

inline void audioWrite(BelaContext *context, unsigned int frame, unsigned int channel, float data)
{
	context->out_buffer[channel][frame] = data;
}

int process(jack_nframes_t nframes, void *arg)
{
	BelaContext *context = (BelaContext *) arg;
#if MODE == MODE_JACK
	context->in_buffer[0] = (const float *) jack_port_get_buffer(context->in[0], nframes);
	context->in_buffer[1] = (const float *) jack_port_get_buffer(context->in[1], nframes);
	context->in_buffer[2] = (const float *) jack_port_get_buffer(context->in[2], nframes);
	context->in_buffer[3] = (const float *) jack_port_get_buffer(context->in[3], nframes);
	context->out_buffer[0] = (float *) jack_port_get_buffer(context->out[0], nframes);
	context->out_buffer[1] = (float *) jack_port_get_buffer(context->out[1], nframes);
	context->audioFrames = nframes;
	render(context, nullptr);
#endif

#if MODE == MODE_SNDFILE
	while (nframes > 0)
	{
		int count = nframes > SF_BLOCKSIZE ? SF_BLOCKSIZE : nframes;
		sf_readf_float(context->in, &context->buffer[0][0], count);
		nframes -= count;
		for (int i = 0; i < count; ++i)
		{
			context->in_buffer[0][i] = context->buffer[i][2];
			context->in_buffer[1][i] = context->buffer[i][3];
			context->in_buffer[2][i] = context->buffer[i][4];
			context->in_buffer[3][i] = context->buffer[i][5];
		}
		context->audioFrames = count;
		render(context, nullptr);
		for (int i = 0; i < count; ++i)
		{
			context->buffer[i][0] = context->out_buffer[0][i];
			context->buffer[i][1] = context->out_buffer[1][i];
		}
		sf_writef_float(context->out, &context->buffer[0][0], count);
	}
#endif
	return 0;
}

// FIXME make Ctrl-C set this to 0
volatile bool running = true;

int main(int argc, char **argv)
{
	int retval = 1;
	BelaContext *context = (BelaContext *) std::calloc(1, sizeof(*context));
	if (! context)
	{
		return retval;
	}

#if MODE == MODE_JACK
	context->client = jack_client_open("REBUS", JackOptions(0), nullptr);
	if (context->client)
	{
		jack_set_process_callback(context->client, process, context);
		context->in[0] = jack_port_register(context->client, "in_1", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
		context->in[1] = jack_port_register(context->client, "in_2", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
		context->in[2] = jack_port_register(context->client, "magnitude", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
		context->in[3] = jack_port_register(context->client, "phase", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
		context->out[0] = jack_port_register(context->client, "out_1", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
		context->out[1] = jack_port_register(context->client, "out_2", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
		context->audioSampleRate = jack_get_sample_rate(context->client);
		context->audioFrames = jack_get_buffer_size(context->client);
		if (setup(context, nullptr))
		{
			if (0 == jack_activate(context->client))
			{
				while (running)
				{
					sleep(1);
				}
				retval = 0;
				jack_deactivate(context->client);
			}
			cleanup(context, nullptr);
		}
		jack_port_unregister(context->client, context->in[0]);
		jack_port_unregister(context->client, context->in[1]);
		jack_port_unregister(context->client, context->out[0]);
		jack_port_unregister(context->client, context->out[1]);
		jack_client_close(context->client);
	}
#endif

#if MODE == MODE_SNDFILE
	if (argc == 3)
	{
		context->in_buffer[0] = new float[SF_BLOCKSIZE];
		context->in_buffer[1] = new float[SF_BLOCKSIZE];
		context->in_buffer[2] = new float[SF_BLOCKSIZE];
		context->in_buffer[3] = new float[SF_BLOCKSIZE];
		context->out_buffer[0] = new float[SF_BLOCKSIZE];
		context->out_buffer[1] = new float[SF_BLOCKSIZE];
		if ((context->in = sf_open(argv[1], SFM_READ, &context->info)))
		{
			if (context->info.channels == 6)
			{
				context->audioSampleRate = context->info.samplerate;
				context->outfo.samplerate = context->info.samplerate;
				context->outfo.channels = context->info.channels;
				context->outfo.format = SF_FORMAT_WAV | SF_FORMAT_FLOAT;
				if ((context->out = sf_open(argv[2], SFM_WRITE, &context->outfo)))
				{
					if (setup(context, nullptr))
					{
						process(context->info.frames, context);
						retval = 0;
						cleanup(context, nullptr);
					}
					sf_close(context->out);
				}
			}
			sf_close(context->in);
		}
	}
#endif

	std::free(context);
	return retval;
}

#define SCOPE 0
#undef RECORD
#define RECORD 0
#define GUI 0

#endif // mode specific stuff


#include <cmath>
#include <cstdlib>
#include <cstring>
#include <time.h>
#include <new>

#if RECORD
// must be >= block size * channels
#define RECORD_CHANNELS 6
#define RECORD_SIZE (1024 * RECORD_CHANNELS)
void record(void *);
#endif

// state

#include COMPOSITION_INCLUDE

struct STATE
{

//---------------------------------------------------------------------
	struct COMPOSITION composition;
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

struct STATE *S;

#if GUI
Gui gui;
#endif

#if SCOPE
Scope scope;
#endif

// setup

bool setup(BelaContext *context, void *userData)
{

#if GUI
	gui.setup(context->projectName);
	gui.setBuffer('f', 2);
#endif

#if SCOPE
	scope.setup(6, context->audioSampleRate);
#endif

	// allocate state
	S = new(std::nothrow) STATE();
	if (! S)
	{
		return false;
	}

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
	if (! (S->record_task = Bela_createAuxiliaryTask(&record, 90, "record")))
	{
		return false;
	}
	S->items = context->audioFrames * info.channels;
#endif

//---------------------------------------------------------------------
// compostion setup
	return COMPOSITION_setup(context, &S->composition);
//---------------------------------------------------------------------
}

// render

void render(BelaContext *context, void *userData)
{
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

#elif MODE == MODE_JACK || MODE == MODE_SNDFILE
		const float magnitude = audioRead(context, n, 2);
		const float phase = audioRead(context, n, 3);

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

#if RECORD
// recording task
void record(void *)
{
	int ret;
	while (S->items && (ret = S->pipe.readNonRt(&S->record_in[0], S->items)) > 0 && S->out_file)
	{
		sf_write_float(S->out_file, &S->record_in[0], ret);
	}
}
#endif

// cleanup

void cleanup(BelaContext *context, void *userData)
{
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
