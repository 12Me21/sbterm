#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <pulse/simple.h>
#include <pulse/error.h>

#define WS 2

char WAVE[][WS] = {
/*
	0x40, 0x40, 0x60,
	0xA0, 0xC0, 0xD0,
	0xD0, 0xC0, 0xA0,
	0x60, 0x40, 0x30,
	0x30, 0x40, 0x60,
	0xA0, 0xC0, 0xD0,
	0xD0, 0xC0, 0xA0,
*/
	0x40, 0x40,// 0x40,
	0xC0, 0xC0,// 0xC0,
	0xC0, 0xC0,// 0xC0,
	0x40, 0x40,// 0x40,
	0x40, 0x40,// 0x40,
	0xC0, 0xC0,// 0xC0,
	0xC0, 0xC0,// 0xC0,
	0x40, 0x40,
};

pa_sample_spec ss = {.format = PA_SAMPLE_U8, .channels = 1, .rate = 32728/2*4*WS/8};
pa_simple *s;
pa_buffer_attr buf_attr = {
	.maxlength = 1024,
	.minreq = -1,
	.prebuf = -1,
	.tlength = -1,
};

int sent=0;
int odd=0;

void modem_init(void){
	s = pa_simple_new(NULL, "SBterm", PA_STREAM_PLAYBACK, NULL, "play", &ss, NULL, &buf_attr, NULL);
}

void modem_sync(void){
	sent=0;
	int i;
	for(i=0;i<50;i++){
		pa_simple_write(s, "\x80", 1, NULL);
	}
	for(i=0;i<50;i++){
		pa_simple_write(s, WAVE[0], WS*4, NULL);
	}
	pa_simple_write(s, WAVE[1], WS*4, NULL);
	odd=0;
}	

void modem_send(char * buffer, size_t length){
	while(length-->0){
		char c=*buffer++;
		int i;
		for(i=0;i<8;i+=2){
			pa_simple_write(s, WAVE[c & 0b11]+odd,WS*4,NULL);
			odd=!odd;
			c>>=2;
		}
		sent++;
		if(sent>1000)
			modem_sync();

	}
}

void modem_end(void){
	pa_simple_drain(s, NULL);
	pa_simple_free(s);
}
