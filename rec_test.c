#include <stdio.h>
#include <pulse/simple.h>
#include "kissfft/kiss_fftr.h"
#include <math.h>

pa_simple *s;
pa_sample_spec ss;

kiss_fftr_cfg cfg;

kiss_fft_scalar samples[8];
kiss_fft_cpx freqdata[8];

double cdst(double a,double b,double c){
  double d=b-a;
  if(d>c/2)
    d=c-d;
  else if(d<c/-2)
    d=c+d;
  return d;
}

char demodulate(double nearest){
  pa_simple_read(s,samples,sizeof samples,NULL);
  kiss_fftr(cfg, samples, freqdata);
  double mag=pow(freqdata[1].r,2)+pow(freqdata[1].i,2);
  if(mag<10000000.0)
    return -1;
  double phase=atan2(freqdata[1].i,freqdata[1].r);
  if(nearest==-1)
    nearest=round(phase*4)/4;
  char drift=round(8*cdst(phase,nearest,1));
  return (int)(nearest*4)%4;
}

int main(void){
  printf("hey!");
  ss.format = PA_SAMPLE_FLOAT32LE;
  ss.channels = 1;
  ss.rate = 32728;
  s = pa_simple_new(NULL,               // Use the default server.
		    "rec_test",           // Our application's name.
		    PA_STREAM_RECORD,
		    NULL,               // Use the default device.
		    "modem_rec",            // Description of our stream.
		    &ss,                // Our sample format.
		    NULL,               // Use default channel map
		    NULL,               // Use default buffering attributes.
		    NULL               // Ignore error code.
		    );
  
  cfg = kiss_fftr_alloc(4,0,NULL,NULL);
  int i;

for(i=0;i<10;i++)
    if(demodulate(0)==-1)
      i=0;
  do;while(demodulate(-1)==0);

  char chr=0,bi=0;
  while(1){
    char matched=demodulate(-1);
    if(matched==-1)
      break;
    chr|=matched<<bi;
    bi+=2;
    if(bi==8){
      putchar(chr);
      chr=0;
      bi=0;
    }
  }
}

//gcc rec_test.c kissfft/kiss_fftr.c kissfft/kiss_fft.c /usr/lib/x86_64-linux-gnu/libpulse*.so -lm
m
