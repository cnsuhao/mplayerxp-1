
#define DUMP_PCM 1

// gcc test.c -I.. -L. -lMP3 -lm -o test1 -O4

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include <sys/time.h>
#include "../config.h"
#include "../mm_accel.h"
#include "mp3.h"

static inline unsigned int GetTimer(){
  struct timeval tv;
  struct timezone tz;
//  float s;
  gettimeofday(&tv,&tz);
//  s=tv.tv_usec;s*=0.000001;s+=tv.tv_sec;
  return (tv.tv_sec*1000000+tv.tv_usec);
}

static FILE* mp3file=NULL;

int mplayer_audio_read(char *buf,int size,float *pts){
    return fread(buf,1,size,mp3file);
}

#define BUFFLEN 4608*1024
static unsigned char buffer[BUFFLEN];

#define MP3_ACCEL MM_ACCEL_X86_MMX | MM_ACCEL_X86_3DNOW | MM_ACCEL_X86_MMXEXT | MM_ACCEL_X86_3DNOWEXT// | MM_ACCEL_X86_SSE

int main(int argc,char* argv[]){
  int len;
  int total=0,accel=MP3_ACCEL;
  unsigned int time1;
  float length,pts;
#ifdef DUMP_PCM
  FILE *f=NULL;
  f=fopen("test.pcm","wb");
#endif

  if(argc>2) if(strcmp(argv[2],"noaccel")==0) accel=0;
  mp3file=fopen((argc>1)?argv[1]:"test.mp3","rb");
  if(!mp3file){  printf("file not found\n");  exit(1); }

  // MPEG Audio:
  MP3_Init(0,accel,mplayer_audio_read,"scaler=1.0");
  printf("Using %s accelerated decoding\n",accel?"":"no");
  MP3_samplerate=MP3_channels=0;

  time1=GetTimer();
  while((len=MP3_DecodeFrame(buffer,-1,&pts))>0 && total<2000000){
      total+=len;
      // play it
#ifdef DUMP_PCM
      fwrite(buffer,len,1,f);
#endif
      putchar('.');fflush(stdout);
  }
  time1=GetTimer()-time1;
  length=(float)total/(float)(MP3_samplerate*MP3_channels*2);
  printf("\nDecoding time: %8.6f\n",(float)time1*0.000001f);
  printf("Uncompressed size: %d bytes  (%8.3f secs)\n",total,length);
  printf("CPU usage at normal playback: %5.2f %\n",(float)time1*0.0001f/length);

  fclose(mp3file);
  return 0;
}
