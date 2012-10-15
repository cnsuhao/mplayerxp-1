#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/time.h>

#include "mp_config.h"
#include "../bswap.h"

#include "../libmpdemux/mrl.h"
#include "afmt.h"
#include "audio_out.h"
#include "audio_out_internal.h"
#include "ao_msg.h"

static ao_info_t info = 
{
	"Null audio output",
	"null",
	"Tobias Diedrich",
	""
};

LIBAO_EXTERN(null)

typedef struct null_priv_s
{
    struct	timeval last_tv;
    int		buffer;
    FILE*	fd;
    int		fast_mode;
    int		wav_mode;
}null_priv_t;

static null_priv_t null = { { 0, 0 }, 0, NULL, 0, 0 };

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
    temp = now_tv.tv_sec - null.last_tv.tv_sec;
    temp *= ao_data.bps;

    temp2 = now_tv.tv_usec - null.last_tv.tv_usec;
    temp2 /= 1000;
    temp2 *= ao_data.bps;
    temp2 /= 1000;
    temp += temp2;

    null.buffer-=temp;
    if (null.buffer<0) null.buffer=0;

    if(temp>0) null.last_tv = now_tv;//mplayer is fast
}

// to set/get/query special features/parameters
static int __FASTCALL__ control(int cmd,long arg){
    UNUSED(cmd);
    UNUSED(arg);
    return CONTROL_TRUE;
}

// open & setup audio device
// return: 1=success 0=fail
static int __FASTCALL__ init(unsigned flags){
    char *null_dev=NULL,*mode=NULL;
    UNUSED(flags);
    if (ao_subdevice) {
	mrl_parse_line(ao_subdevice,NULL,NULL,&null_dev,&mode);
	null.fd=NULL;
	if(null_dev) null.fd = fopen(null_dev, "wb");
	//if(null.fd) null.fast_mode=1;
	if(strcmp(mode,"wav")==0) null.wav_mode=1;
    } //end parsing ao_subdevice
    return 1;
}

static int __FASTCALL__ configure(unsigned rate,unsigned channels,unsigned format){
    unsigned bits;
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
    null.buffer=0;
    gettimeofday(&null.last_tv, 0);
    if(null.fd && null.wav_mode)
    {
	wavhdr.channels = le2me_16(ao_data.channels);
	wavhdr.sample_rate = le2me_32(ao_data.samplerate);
	wavhdr.bytes_per_second = le2me_32(ao_data.bps);
	wavhdr.bits = le2me_16(bits);
	wavhdr.block_align = le2me_16(ao_data.channels * (bits / 8));
	wavhdr.data_length=le2me_32(0x7ffff000);
	wavhdr.file_length = wavhdr.data_length + sizeof(wavhdr) - 8;

	fwrite(&wavhdr,sizeof(wavhdr),1,null.fd);
	wavhdr.file_length=wavhdr.data_length=0;
    }
    return 1;
}

// close audio device
static void uninit(void){
    if(null.fd && null.wav_mode && fseeko(null.fd, 0, SEEK_SET) == 0){ /* Write wave header */
	wavhdr.file_length = wavhdr.data_length + sizeof(wavhdr) - 8;
	wavhdr.file_length = le2me_32(wavhdr.file_length);
	wavhdr.data_length = le2me_32(wavhdr.data_length);
	fwrite(&wavhdr,sizeof(wavhdr),1,null.fd);
    }
    if(null.fd) fclose(null.fd);
}

// stop playing and empty null.buffers (for seeking/pause)
static void reset(void) { null.buffer=0; }

// stop playing, keep null.buffers (for pause)
static void audio_pause(void)
{
    // for now, just call reset();
    reset();
}

// resume playing, after audio_pause()
static void audio_resume(void) {}

// return: how many bytes can be played without blocking
static unsigned get_space(void){
    drain();
    return null.fast_mode?INT_MAX:ao_data.outburst - null.buffer;
}

// plays 'len' bytes of 'data'
// it should round it down to outburst*n
// return: number of bytes played
static unsigned __FASTCALL__ play(void* data,unsigned len,unsigned flags)
{
    unsigned maxbursts = (ao_data.buffersize - null.buffer) / ao_data.outburst;
    unsigned playbursts = len / ao_data.outburst;
    unsigned bursts = playbursts > maxbursts ? maxbursts : playbursts;
    null.buffer += bursts * ao_data.outburst;
    UNUSED(flags);
    if(null.fd && len)
    {
	MSG_DBG2("writing %u bytes into file\n",len);
	fwrite(data,len,1,null.fd);
	wavhdr.data_length += len;
    }

    return null.fast_mode?bursts * ao_data.outburst:len;
}

// return: delay in seconds between first and last sample in null.buffer
static float get_delay(void){
    drain();
    return null.fast_mode?0.0:(float) null.buffer / (float) ao_data.bps;
}
