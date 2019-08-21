#include "pty.h"
#include <stdio.h>
#include <poll.h>
#include <unistd.h>
#include <pulse/pulseaudio.h>
#include <string.h>

void audio_init(pa_stream_request_cb_t stream_write_cb, pa_stream_notify_cb_t stream_underflow_cb, pa_stream_request_cb_t stream_read_cb);

#define LEN(a) (sizeof(a)/sizeof((a)[0]))

#define IDLE 22 //ASCII synchronous idle
#define ESC 27 //ASCII escape

#define WS 2 //offset of each phase, in samples
#define SYMBOL 8 //length of symbol, in samples
#define SAMPLES_PER_BYTE (4*SYMBOL)
const Sample WAVE[][WS] = {
	/* sine wave calculated using TI-80 graphing calculator */
	{10, 79},{176,245},{245,176},{ 79, 10},{ 10, 79},{176,245},{245,176},{ 79, 10}
};
int sent=-1; //number of bytes sent since last sync
int odd=0; //whether the current symbol is odd numbered, and will be offset 45 degrees
#define SYNC_INTERVAL 1000 //how many bytes to send before sync
Sample SYNC_DATA[48+51*SYMBOL]; //sync wave
int inesc=0; //in ANSI escape sequence flag

int written_samples = 0;
void modulate(pa_stream *stream, unsigned char byte){
	unsigned char parity = (byte ^ byte>>1 ^ byte>>2 ^ byte>>3 ^ byte>>4 ^ byte>>5 ^ byte>>6) & 1;
	byte |= parity<<7;
	if(sent >= SYNC_INTERVAL){
		sent=0;
		pa_stream_write(stream, SYNC_DATA, LEN(SYNC_DATA), NULL,0,PA_SEEK_RELATIVE);
		written_samples += LEN(SYNC_DATA);
		odd=0;
	}
	static Sample buffer[SAMPLES_PER_BYTE];
	int i;
	for(i=0; i<8; i+=2){
		memcpy(buffer+i/2*SYMBOL, WAVE[byte & 3]+odd, SYMBOL);
		odd = !odd;
		byte >>= 2;
	}
	sent++;
	pa_stream_write(stream, buffer, LEN(buffer), NULL,0,PA_SEEK_RELATIVE);
	written_samples += LEN(buffer);
}

// Ideally this should cleanly stop audio playback (and wait for buffer to be empty)
//  but I can't figure out how to do that...
void modem_exit(){
	exit_pty(0);
}

// Callback function for ANSI escape translator
void ansidec_cb(char *data, int data_len, void *user){
	int i;
	pa_stream *stream = (pa_stream*)user;
	for(i=0; i<data_len; i++)
		modulate(stream, data[i]);
}

struct pollfd pollfds[1];
int read_nonblocking(char *buffer, int bytes){
	if(poll(pollfds, 1, 0)>0){
		if(pollfds[0].revents & POLLIN)
			return read(pollfds[0].fd, buffer, bytes);
		if(pollfds[0].revents & POLLHUP) //connection closed
			return -1;
	}
	return 0;
}

// round `num` up to the nearest multiple of `step`
int iceil(int num, int step){
	return (num+step-1)/step;
}

// make sure `*val` is not higher than `max`
void limit(int *val, int max){
	if(*val>max)
		*val = max;
}

// This is called when the audio output buffer is getting empty
// and more samples need to be written.
void stream_write_cb(pa_stream *stream, size_t r_samples, void *u){
	// The first time, fill with silence
	if(sent==-1){
		Sample *silence;
		pa_stream_begin_write(stream, (void*)&silence, &r_samples);
		int i;
		for(i=0; i<r_samples; i++)
			silence[i] = 0x80;
		pa_stream_write(stream, silence, r_samples, NULL, 0, PA_SEEK_RELATIVE);
		sent = SYNC_INTERVAL;
		return;
	}

	// Generate samples until correct amount have been generated
	// (or there is no more data to read)
	// This will usually generate more samples than requested (especially during a sync), but never fewer, I hope..
	written_samples = 0;
	while(written_samples < r_samples){
		static char read_buffer[30*50];
		int read_chars = iceil(r_samples-written_samples, SAMPLES_PER_BYTE);
		limit(&read_chars, LEN(read_buffer));
		limit(&read_chars, 1000-sent);
		read_chars = read_nonblocking(read_buffer, read_chars);

		if(read_chars < 0) //error in read() (connection closed)
			modem_exit();
		else if(read_chars == 0) //no more data to read()
			break;
		else{ //normal
			int i;
			for(i=0; i<read_chars; i++){
				if(inesc){
					inesc = !proc_esc(read_buffer[i], ansidec_cb, (void*)stream);
				}else if(read_buffer[i]==ESC){
					inesc = 1;
					init_esc();
				}else{
					modulate(stream, read_buffer[i]);
				}
			}
		}
		
	}
	//fill remaining space with sync idles
	while(written_samples < r_samples){
		modulate(stream, IDLE);
	}	
}

void stream_read_cb(pa_stream *stream, size_t samples, void *userdata){
	//printf("Got %d samples\n\r", (int)samples);
	const Sample *data;
	pa_stream_peek(stream, (const void **)&data, &samples);
	pa_stream_drop(stream);
}

void stream_underflow_cb(pa_stream *stream, void *userdata){
	printf("Underflow!\r\n");
	sent = -1;
}

// Initialize some things, and set up audio. `fd` is file descriptor of PTY
void modem_init(int fd){
	// Generate sync samples
	int i;
	for(i=0; i<48; i++)
		SYNC_DATA[i] = 0x80;
	for(i=0; i<50; i++)
		memcpy(SYNC_DATA+48+i*SYMBOL, WAVE[0], SYMBOL);
	memcpy(SYNC_DATA+48+i*SYMBOL, WAVE[1], SYMBOL);
	//init poll
	pollfds[0].fd = fd;
	pollfds[0].events = POLLIN | POLLPRI;
	// See: pulse.c
	audio_init(stream_write_cb, stream_underflow_cb, stream_read_cb);
}
