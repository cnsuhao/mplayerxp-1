
//gcc test2.c -O2 -I.. -L. ../libvo/aclib.c -lMP3 -lm -o test2
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/soundcard.h>

#include "mp3.h"
#include "config.h"
#include "../mm_accel.h"

static FILE* mp3file=NULL;

int mplayer_audio_read(char *buf,int size,float *pts){
    return fread(buf,1,size,mp3file);
}

#define BUFFLEN 4608
static unsigned char buffer[BUFFLEN];

#define MM_FULL_ACCEL (MM_ACCEL_X86_MMX|MM_ACCEL_X86_3DNOW|MM_ACCEL_X86_3DNOWEXT|MM_ACCEL_X86_MMXEXT)

int main(int argc,char* argv[]){
  int len;
  int total=0,accel=MM_FULL_ACCEL;
  float length,pts;
  int r;
  int audio_fd;

  mp3file=fopen((argc>1)?argv[1]:"test.mp3","rb");
  if(!mp3file){  printf("file not found\n");  exit(1); }

  if(argc>2) if(strcmp(argv[2],"noaccel")==0) accel=0;
  // MPEG Audio:
  MP3_Init(0,accel,mplayer_audio_read,"scaler=0.2");
  MP3_samplerate=MP3_channels=0;
  len=MP3_DecodeFrame(buffer,-1,&pts);

  audio_fd=open("/dev/dsp", O_WRONLY);
  if(audio_fd<0){  printf("Can't open audio device\n");exit(1); }
  r=AFMT_S16_LE; ioctl (audio_fd, SNDCTL_DSP_SETFMT, &r);
  r=AFMT_QUERY;  ioctl (audio_fd, SNDCTL_DSP_SETFMT, &r);
  printf("audio_setup: %08X fmts\n",r);
  r=MP3_channels-1; ioctl (audio_fd, SNDCTL_DSP_STEREO, &r);
  r=MP3_samplerate; ioctl (audio_fd, SNDCTL_DSP_SPEED, &r);
  printf("audio_setup: using %d Hz samplerate (requested: %d)\n",r,MP3_samplerate);

  while(1){
      int len2;
      unsigned i;
      if(len==0) len=MP3_DecodeFrame(buffer,BUFFLEN,&pts);
      if(len<=0) break; // EOF

      // play it
      len2=0;
      for(i=0;i<len;i++) {
        float f;
        short s;
        f = ((const float *)buffer)[i];
        s = (short)f;
        len2+=write(audio_fd,&s,2);
      }
      len2=write(audio_fd,buffer,len);
      if(len2<0) break; // ERROR?
      len-=len2; total+=len2;
      if(len>0){
          // this shouldn't happen...
          memcpy(buffer,buffer+len2,len);
          putchar('!');fflush(stdout);
      }
  }

  fclose(mp3file);

}
