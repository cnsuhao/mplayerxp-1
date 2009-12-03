#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/time.h>

#include "config.h"
#include "../bswap.h"

#include "ao_msg.h"
#include "../libmpdemux/mrl.h"
#include "afmt.h"
#include "audio_out.h"
#include "audio_out_internal.h"

static ao_info_t info = 
{
	"Null audio output",
	"null",
	"Tobias Diedrich",
	""
};

LIBAO_EXTERN(null)

static struct	timeval last_tv;
static int	buffer;
static FILE*    fd=NULL;
static int	fast_mode=0;
static int	wav_mode=0;

#define WAV_ID_RIFF MAKE_FOURCC(0x46,0x46,0x49,0x52) /* "RIFF" */
#define WAV_ID_WAVE MAKE_FOURCC(0x45,0x56,0x41,0x57) /* "WAVE" */
#define WAV_ID_FMT  MAKE_FOURCC(0x20,0x74,0x6d,0x66) /* "fmt " */
#define WAV_ID_DATA MAKE_FOURCC(0x61,0x74,0x61,0x64) /* "data" */
#define WAV_ID_PCM  MAKE_TWOCC(0x00,0x01)

struct WaveHeader
{
	uint32_t riff;
	uint32_t file_length;
	uint32_t wave;
	uint32_t fmt;
	uint32_t fmt_length;
	uint16_t fmt_tag;
	uint16_t channels;
	uint32_t sample_rate;
	uint32_t bytes_per_second;
	uint16_t block_align;
	uint16_t bits;
	uint32_t data;
	uint32_t data_length;
};

/* init with default values */
static struct WaveHeader wavhdr = {
	WAV_ID_RIFF,
        /* same conventions than in sox/wav.c/wavwritehdr() */
	0, //le2me_32(0x7ffff024),
	WAV_ID_WAVE,
	WAV_ID_FMT,
	16,
	WAV_ID_PCM,
	2,
	44100,
	192000,
	4,
	16,
	WAV_ID_DATA,
	0, //le2me_32(0x7ffff000)
};

static void drain(void){

    struct timeval now_tv;
    int temp, temp2;

    gettimeofday(&now_tv, 0);
    temp = now_tv.tv_sec - last_tv.tv_sec;
    temp *= ao_data.bps;
    
    temp2 = now_tv.tv_usec - last_tv.tv_usec;
    temp2 /= 1000;
    temp2 *= ao_data.bps;
    temp2 /= 1000;
    temp += temp2;

    buffer-=temp;
    if (buffer<0) buffer=0;

    if(temp>0) last_tv = now_tv;//mplayer is fast
}

// to set/get/query special features/parameters
static int __FASTCALL__ control(int cmd,long arg){
    return CONTROL_TRUE;
}

// open & setup audio device
// return: 1=success 0=fail
static int __FASTCALL__ init(int flags){
    char *null_dev=NULL,*mode=NULL;
    if (ao_subdevice) {
	mrl_parse_line(ao_subdevice,NULL,NULL,&null_dev,&mode);
	fd=NULL;
	if(null_dev) fd = fopen(null_dev, "wb");
	//if(fd) fast_mode=1;
	if(strcmp(mode,"wav")==0) wav_mode=1;
    } //end parsing ao_subdevice
    return 1;
}

static int __FASTCALL__ configure(int rate,int channels,int format){
    int bits;
    ao_data.buffersize= 0xFFFFF;
    ao_data.outburst=0xFFFF;//4096;
    ao_data.channels=channels;
    ao_data.samplerate=rate;
    ao_data.format=format;
    ao_data.bps=channels*rate;
    bits=8;
    switch(format)
      {
      case AFMT_S16_LE:
      case AFMT_U16_LE:
      case AFMT_S16_BE:
      case AFMT_U16_BE:
	bits=16;
	ao_data.bps *= 2;
	break;
      case AFMT_S32_LE:
      case AFMT_S32_BE:
      case AFMT_U32_LE:
      case AFMT_U32_BE:
      case AFMT_FLOAT32:
	bits=32;
	ao_data.bps *= 4;
	break;
      case AFMT_S24_LE:
      case AFMT_S24_BE:
      case AFMT_U24_LE:
      case AFMT_U24_BE:
	bits=24;
	ao_data.bps *= 3;
	break;
      default:
	break;	    
      }
    buffer=0;
    gettimeofday(&last_tv, 0);
    if(fd && wav_mode)
    {
	wavhdr.channels = le2me_16(ao_data.channels);
	wavhdr.sample_rate = le2me_32(ao_data.samplerate);
	wavhdr.bytes_per_second = le2me_32(ao_data.bps);
	wavhdr.bits = le2me_16(bits);
	wavhdr.block_align = le2me_16(ao_data.channels * (bits / 8));
	wavhdr.data_length=le2me_32(0x7ffff000);
	wavhdr.file_length = wavhdr.data_length + sizeof(wavhdr) - 8;

	fwrite(&wavhdr,sizeof(wavhdr),1,fd);
	wavhdr.file_length=wavhdr.data_length=0;
    }
    return 1;
}

// close audio device
static void uninit(void){
    if(fd && wav_mode && fseeko(fd, 0, SEEK_SET) == 0){ /* Write wave header */
	wavhdr.file_length = wavhdr.data_length + sizeof(wavhdr) - 8;
	wavhdr.file_length = le2me_32(wavhdr.file_length);
	wavhdr.data_length = le2me_32(wavhdr.data_length);
	fwrite(&wavhdr,sizeof(wavhdr),1,fd);
    }
    if(fd) fclose(fd);
}

// stop playing and empty buffers (for seeking/pause)
static void reset(void){
    buffer=0;
}

// stop playing, keep buffers (for pause)
static void audio_pause(void)
{
    // for now, just call reset();
    reset();
}

// resume playing, after audio_pause()
static void audio_resume(void)
{
}

// return: how many bytes can be played without blocking
static int get_space(void){

    drain();
    return fast_mode?INT_MAX:ao_data.outburst - buffer;
}

// plays 'len' bytes of 'data'
// it should round it down to outburst*n
// return: number of bytes played
static int __FASTCALL__ play(void* data,int len,int flags){


    int maxbursts = (ao_data.buffersize - buffer) / ao_data.outburst;
    int playbursts = len / ao_data.outburst;
    int bursts = playbursts > maxbursts ? maxbursts : playbursts;
    buffer += bursts * ao_data.outburst;

    if(fd && len)
    {
	MSG_DBG2("writing %u bytes into file\n",len);
	fwrite(data,len,1,fd);
	wavhdr.data_length += len;
    }

    return fast_mode?bursts * ao_data.outburst:len;
}

// return: delay in seconds between first and last sample in buffer
static float get_delay(void){

    drain();
    return fast_mode?0.0:(float) buffer / (float) ao_data.bps;
}






