#include <stdio.h>
#include <assert.h>
#include <pulse/pulseaudio.h>
#include <string.h>
#include <poll.h>
#include <unistd.h>
#include "pty.h"

#define IDLE 22

#define WS 2

char WAVE[][WS] = {
	/* sine wave calculated using TI-80 graphing calculator */
	 10, 79,
	176,245,
	245,176,
	 79, 10,
	 10, 79,
	176,245,
	245,176,
	 79, 10
};
int odd=0, sent=-1;
int inesc=0;
#define SYNC_INTERVAL 1000

pa_threaded_mainloop *mainloop;
pa_mainloop_api *mainloop_api;
pa_context *context;
pa_sample_spec sample_spec = {.format = PA_SAMPLE_U8, .rate = 32728, .channels = 1};
pa_channel_map map;
pa_stream *stream;
pa_buffer_attr buffer_attr = {.maxlength = -1, .tlength = 1024, .prebuf = -1, .minreq = -1};

//Number of samples per byte
#define SYMBOL 8
#define SAMPLES_PER_BYTE (4*SYMBOL)

Sample sync_data[48+51*SYMBOL];

Sample modulate(Sample *buffer, char byte){
	int i;
	for (i=0;i<8;i+=2){
		memcpy(buffer+i/2*SYMBOL,WAVE[byte & 3]+odd,SYMBOL);
		odd=!odd;
		byte>>=2;
	}
	sent++;
}

struct pollfd pollfds[1];

//drain output stream and exit cleanly
//lol no
void modem_exit(){
	exit_pty(0);
}

void stream_underflow_cb(pa_stream *stream, void *userdata){
	sent=-1;
}

void ansidec_cb(Sample *sam_buf, int *written, char *data, int data_len){
	int i;
	for(i=0;i<data_len;i++){
		modulate(sam_buf+*written*SAMPLES_PER_BYTE,data[i]);
		(*written)++;
	}
}

#define READ_BUF 50*30
char read_buffer[READ_BUF];
Sample sam_buf[READ_BUF*SAMPLES_PER_BYTE];
void stream_write_cb(pa_stream *stream, size_t r_samples, void *userdata) {
	if (sent == -1)
	{
		Sample shit[r_samples];
		int i;
		for(i=0;i<r_samples;i++)
			shit[i]=0x80;
		pa_stream_write(stream,shit,r_samples,NULL,0,PA_SEEK_RELATIVE);
		sent=SYNC_INTERVAL;
		return;
	}
	if(sent >= SYNC_INTERVAL){
		sent=0;
		pa_stream_write(stream, sync_data, 48+51*SYMBOL, NULL, 0, PA_SEEK_RELATIVE);
		odd=0;
		return;
	}
	int i=0;
	int r_chars=r_samples/SAMPLES_PER_BYTE; //number of bytes worth of data requested
	if (r_chars > 1000-sent)
		r_chars = 1000-sent;
	r_samples=r_chars*SAMPLES_PER_BYTE;
	int written = 0;
	if(poll(pollfds,1,0)>0){
		if(pollfds[0].revents & POLLIN){

			int read_bytes = r_chars; //number of bytes to read()
			if(read_bytes>READ_BUF)
				read_bytes=READ_BUF;
			read_bytes=read(pollfds[0].fd,read_buffer,read_bytes);
			//modulate data
			for(;i<read_bytes;i++){
				if(inesc){
					inesc=!proc_esc(read_buffer[i],ansidec_cb,sam_buf,&written);
				}else if(read_buffer[i]=='\033'){
					inesc=1;
					init_esc();
				}else{
					modulate(sam_buf+written*SAMPLES_PER_BYTE,read_buffer[i]);
					written++;
				}
			}
			printf("CB %d\n\r",read_bytes);
		}
		if(pollfds[0].revents & POLLHUP){
			modem_exit();
		}
	}
	
	//fill remaining space with sync idle
	for(;i<r_chars;i++){
		modulate(sam_buf+written*SAMPLES_PER_BYTE,IDLE);
		written++;
	}
	pa_stream_write(stream,sam_buf,written*SAMPLES_PER_BYTE,NULL,0,PA_SEEK_RELATIVE);
}

void context_state_cb(pa_context* context, void* mainloop) {
	pa_threaded_mainloop_signal(mainloop, 0);
}
void stream_state_cb(pa_stream *s, void *mainloop) {
	pa_threaded_mainloop_signal(mainloop, 0);
}
void stream_success_cb(pa_stream *stream, int success, void *userdata) {
	return;
}

//call first
void modem_init(int fd){
	//init sync
	int i;
	for(i=0;i<48;i++)
		sync_data[i]=0x80;
	for(i=0;i<50;i++)
		memcpy(sync_data+48+i*SYMBOL,WAVE[0],SYMBOL);
	memcpy(sync_data+48+i*SYMBOL,WAVE[1],SYMBOL);
	//code taken from stackoverflow 29977651 because pulseaudio documentation sucks
	//init poll
	pollfds[0].fd = fd;
	pollfds[0].events = POLLIN | POLLPRI;
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
	pa_channel_map_init_stereo(&map);
	pa_stream *stream = pa_stream_new(context, "Playback", &sample_spec, NULL);
	pa_stream_set_state_callback(stream, stream_state_cb, mainloop);
	pa_stream_set_write_callback(stream, stream_write_cb, mainloop);
	pa_stream_set_underflow_callback(stream, stream_underflow_cb, NULL);
	pa_stream_flags_t stream_flags = PA_STREAM_START_CORKED | PA_STREAM_ADJUST_LATENCY | PA_STREAM_RELATIVE_VOLUME;
	// Connect stream to the default audio output sink
	assert(pa_stream_connect_playback(stream, NULL, &buffer_attr, stream_flags, NULL, NULL) == 0);
	// Wait for the stream to be ready
	for(;;) {
		pa_stream_state_t stream_state = pa_stream_get_state(stream);
		assert(PA_STREAM_IS_GOOD(stream_state));
		if (stream_state == PA_STREAM_READY) break;
		pa_threaded_mainloop_wait(mainloop);
	}	
	pa_threaded_mainloop_unlock(mainloop);
	// Uncork the stream so it will start playing
	pa_stream_cork(stream, 0, stream_success_cb, mainloop);
}
