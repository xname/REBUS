// modes of operation (aka platform support)

// BELA on REBUS hardware using control via antenna
#define MODE_REBUS 0

// BELA on regular board using control via mouse cursor in GUI window
#define MODE_BELA 1

// JACK client for desktop using control via audio input
#define MODE_JACK 2

//---------------------------------------------------------------------
// set mode here, can be overriden via compilation flags
#ifndef MODE
#define MODE MODE_REBUS
#endif
//---------------------------------------------------------------------



#if MODE != MODE_REBUS && MODE != MODE_BELA && MODE != MODE_JACK
#error unknown MODE
#endif


#if MODE == MODE_REBUS || MODE == MODE_BELA

#include <Bela.h>
#include <libraries/Gui/Gui.h>
#include <libraries/Scope/Scope.h>
#include <libraries/Pipe/Pipe.h>
#include <libraries/sndfile/sndfile.h>

#define SCOPE 1
#define RECORD 1

#define GUI (MODE == MODE_BELA)

#elif MODE == MODE_JACK

#include <cstdlib>
#include <unistd.h>
#include <jack/jack.h>

struct BelaContext
{
	jack_client_t *client;
	jack_port_t *in[2];
	jack_port_t *out[2];

	const float *in_buffer[2];
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
	context->in_buffer[0] = (const float *) jack_port_get_buffer(context->in[0], nframes);
	context->in_buffer[1] = (const float *) jack_port_get_buffer(context->in[1], nframes);
	context->out_buffer[0] = (float *) jack_port_get_buffer(context->out[0], nframes);
	context->out_buffer[1] = (float *) jack_port_get_buffer(context->out[1], nframes);
	context->audioFrames = nframes;
	render(context, nullptr);
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
	context->client = jack_client_open("REBUS", JackOptions(0), nullptr);
	if (context->client)
	{
		jack_set_process_callback(context->client, process, context);
		context->in[0] = jack_port_register(context->client, "in_1", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
		context->in[1] = jack_port_register(context->client, "in_2", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
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
	std::free(context);
	return retval;
}

#define SCOPE 0
#define RECORD 0
#define GUI 0

#endif


#include <cmath>
#include <cstdlib>
#include <cstring>

#if RECORD
// must be >= block size * channels
#define RECORD_SIZE (1024 * 4)
void record(void *);
#endif

// state

struct
{




//---------------------------------------------------------------------
// {{{ synthesis state

	float phase[3];

// }}} synthesis state
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

} *S;

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
	scope.setup(4, context->audioSampleRate);
#endif

	// allocate state
	S = (decltype(S)) std::malloc(sizeof(*S));
	if (! S)
	{
		return false;
	}
	std::memset(S, 0, sizeof(*S));

#if RECORD
	// recording
	SF_INFO info = {0};
	info.channels = 4;
	info.samplerate = context->audioSampleRate;
	info.format = SF_FORMAT_WAV | SF_FORMAT_FLOAT;
	char record_path[100];
	snprintf(record_path, sizeof(record_path), "%08x.wav", (int) time(0));
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
// {{{ synthesis setup

	// ...

// }}} synthesis setup
//---------------------------------------------------------------------




	return true;
}

// render

void render(BelaContext *context, void *userData)
{
	for (unsigned int n = 0; n < context->audioFrames; ++n)
	{
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

#elif MODE == MODE_JACK
		const float phase = audioRead(context, n, 0) * 0.5f + 0.5f;
		const float magnitude = audioRead(context, n, 1) * 0.5f + 0.5f;
#endif

		// render
		float out[2] = { 0.0f, 0.0f };




//---------------------------------------------------------------------
// {{{ synthesis render

		// stereo detuned sawtooth/PWM
		// FIXME bandlimited oscillators
		float increment = 50.0f / context->audioSampleRate;
		S->phase[0] += increment;
		S->phase[1] += increment * (1.0f + 0.1f * phase);
		S->phase[2] += increment * (1.0f - 0.1f * phase);
		S->phase[0] -= std::floor(S->phase[0]);
		S->phase[1] -= std::floor(S->phase[1]);
		S->phase[2] -= std::floor(S->phase[2]);
		out[0] = (S->phase[1] - S->phase[0]) * magnitude;
		out[1] = (S->phase[2] - S->phase[0]) * magnitude;

// }}} synthesis render
//---------------------------------------------------------------------




		// output
#if SCOPE
		scope.log(out[0], out[1], magnitude, phase);
#endif
#if RECORD
		// interleaved
		S->record_out[4 * n + 0] = out[0];
		S->record_out[4 * n + 1] = out[1];
		S->record_out[4 * n + 2] = magnitude;
		S->record_out[4 * n + 3] = phase;
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
// {{{ synthesis cleanup

	// ...

// }}} synthesis cleanup
//---------------------------------------------------------------------




		// free memory
		std::free(S);
		S = nullptr;

	}
}
