//Based on example code: http://stackoverflow.com/questions/29977651/ by Gavin Haynes

#include "pty.h"
#include <pulse/pulseaudio.h>
#include <assert.h>
#include <stdio.h>

pa_threaded_mainloop *mainloop;
pa_mainloop_api *mainloop_api;
pa_context *context;
pa_sample_spec sample_spec = {.format = PA_SAMPLE_U8, .rate = 32728/*+1*/, .channels = 1};
pa_stream *mic_stream;
pa_buffer_attr buffer_attr = {.maxlength = -1, .tlength = 2048, .prebuf = -1, .minreq = -1};
pa_buffer_attr in_buf_attr = {.maxlength = -1, .fragsize = 2048};

void context_state_cb(pa_context* context, void* mainloop) {
	pa_threaded_mainloop_signal(mainloop, 0);
}
void stream_state_cb(pa_stream *s, void *mainloop) {
	pa_threaded_mainloop_signal(mainloop, 0);
}
void in_stream_state_cb(pa_stream *s, void *mainloop) {
	pa_threaded_mainloop_signal(mainloop, 0);
}
void stream_success_cb(pa_stream *stream, int success, void *userdata) {
	return;
}

void audio_init(pa_stream_request_cb_t stream_write_cb, pa_stream_notify_cb_t stream_underflow_cb, pa_stream_request_cb_t stream_read_cb){
	// Get a mainloop and its context
	mainloop = pa_threaded_mainloop_new();
	mainloop_api = pa_threaded_mainloop_get_api(mainloop);
	context = pa_context_new(mainloop_api, "pcm-playback");
	// Set a callback so we can wait for the context to be ready
	pa_context_set_state_callback(context, &context_state_cb, mainloop);
	// Lock the mainloop so that it does not run and crash before the context is ready
	pa_threaded_mainloop_lock(mainloop);
	// Start the mainloop
	assert(pa_threaded_mainloop_start(mainloop) == 0);
	assert(pa_context_connect(context, NULL, PA_CONTEXT_NOAUTOSPAWN, NULL) == 0);
	// Wait for the context to be ready
	for(;;) {
		pa_context_state_t context_state = pa_context_get_state(context);
		assert(PA_CONTEXT_IS_GOOD(context_state));
		if (context_state == PA_CONTEXT_READY) break;
		pa_threaded_mainloop_wait(mainloop);
	}
	// Create a playback stream
	pa_stream *stream = pa_stream_new(context, "Playback", &sample_spec, NULL);
	pa_stream_set_state_callback(stream, stream_state_cb, mainloop);
	pa_stream_set_write_callback(stream, stream_write_cb, mainloop);
	pa_stream_set_underflow_callback(stream, stream_underflow_cb, NULL);
	pa_stream_flags_t stream_flags = PA_STREAM_START_CORKED | PA_STREAM_ADJUST_LATENCY | PA_STREAM_RELATIVE_VOLUME;
	assert(pa_stream_connect_playback(stream, NULL, &buffer_attr, stream_flags, NULL, NULL) == 0);
	// create mic stream
	pa_stream *in_stream = pa_stream_new(context, "Record", &sample_spec, NULL);
	pa_stream_set_state_callback(in_stream, in_stream_state_cb, mainloop);
	pa_stream_set_read_callback(in_stream, stream_read_cb, mainloop);
	pa_stream_connect_record(in_stream, NULL, &in_buf_attr, 0);
	// Wait for the stream to be ready
	for(;;) {
		pa_stream_state_t stream_state = pa_stream_get_state(stream);
		assert(PA_STREAM_IS_GOOD(stream_state));
		pa_stream_state_t in_stream_state = pa_stream_get_state(in_stream);
		assert(PA_STREAM_IS_GOOD(in_stream_state));
		if (stream_state == PA_STREAM_READY && in_stream_state == PA_STREAM_READY)
			break;
		pa_threaded_mainloop_wait(mainloop);
	}	
	pa_threaded_mainloop_unlock(mainloop);
	// Uncork the stream so it will start playing
	pa_stream_cork(stream, 0, stream_success_cb, mainloop);
	pa_stream_cork(in_stream, 0, stream_success_cb, mainloop);
	printf("audio started\r\n");
}
