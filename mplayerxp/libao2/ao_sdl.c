/*
 * ao_sdl.c - libao2 SDLlib Audio Output Driver for MPlayer
 *
 * This driver is under the same license as MPlayer.
 * (http://mplayer.sf.net)
 *
 * Copyleft 2001 by Felix Bünemann (atmosfear@users.sf.net)
 *
 * Thanks to Arpi for nice ringbuffer-code!
 *
 */

#include <stdio.h>
#include <stdlib.h>

#include "audio_out.h"
#include "audio_out_internal.h"
#include "afmt.h"
#include <SDL/SDL.h>
#include "osdep/fastmemcpy.h"
#include "osdep/mplib.h"
#include "ao_msg.h"

static ao_info_t info = {
    "SDLlib audio output",
    "sdl",
    "Felix Buenemann <atmosfear@users.sourceforge.net>",
    ""
};

LIBAO_EXTERN(sdl)

// Samplesize used by the SDLlib AudioSpec struct
#define SAMPLESIZE 1024

// General purpose Ring-buffering routines

#define BUFFSIZE 4096
#define NUM_BUFS 16
typedef struct priv_s {
    unsigned char *	buffer[NUM_BUFS];
    unsigned int	buf_read;
    unsigned int	buf_write;
    unsigned int	buf_read_pos;
    unsigned int	buf_write_pos;
    unsigned int	volume;
    int			full_buffers;
    int			buffered_bytes;
}priv_t;

static int __FASTCALL__ write_buffer(ao_data_t* ao,const unsigned char* data,int len){
    priv_t*priv=ao->priv;
  int len2=0;
  int x;
  while(len>0){
    if(priv->full_buffers==NUM_BUFS) break;
    x=BUFFSIZE-priv->buf_write_pos;
    if(x>len) x=len;
    memcpy(priv->buffer[priv->buf_write]+priv->buf_write_pos,data+len2,x);
    len2+=x; len-=x;
    priv->buffered_bytes+=x; priv->buf_write_pos+=x;
    if(priv->buf_write_pos>=BUFFSIZE){
       // block is full, find next!
       priv->buf_write=(priv->buf_write+1)%NUM_BUFS;
       ++priv->full_buffers;
       priv->buf_write_pos=0;
    }
  }
  return len2;
}

static int __FASTCALL__ read_buffer(ao_data_t* ao,unsigned char* data,int len){
    priv_t*priv=ao->priv;
  int len2=0;
  int x;
  while(len>0){
    if(priv->full_buffers==0) break; // no more data buffered!
    x=BUFFSIZE-priv->buf_read_pos;
    if(x>len) x=len;
    memcpy(data+len2,priv->buffer[priv->buf_read]+priv->buf_read_pos,x);
    SDL_MixAudio(data+len2, data+len2, x, priv->volume);
    len2+=x; len-=x;
    priv->buffered_bytes-=x; priv->buf_read_pos+=x;
    if(priv->buf_read_pos>=BUFFSIZE){
       // block is empty, find next!
       priv->buf_read=(priv->buf_read+1)%NUM_BUFS;
       --priv->full_buffers;
       priv->buf_read_pos=0;
    }
  }
  return len2;
}

// end ring priv->buffer stuff

#if	 defined(HPUX) || defined(sun) && defined(__svr4__)
/* setenv is missing on solaris and HPUX */
static void setenv(const char *name, const char *val, int _xx)
{
  int len  = strlen(name) + strlen(val) + 2;
  char *env = mp_malloc(len);

  if (env != NULL) {
    strcpy(env, name);
    strcat(env, "=");
    strcat(env, val);
    putenv(env);
  }
}
#endif

// to set/get/query special features/parameters
static MPXP_Rc __FASTCALL__ control(const ao_data_t* ao,int cmd,long arg){
    priv_t*priv=ao->priv;
	switch (cmd) {
		case AOCONTROL_QUERY_FORMAT:
		case AOCONTROL_QUERY_CHANNELS:
		case AOCONTROL_QUERY_RATE:
		    return MPXP_False;
		case AOCONTROL_GET_VOLUME:
		{
			ao_control_vol_t* vol = (ao_control_vol_t*)arg;
			vol->left = vol->right = (float)((priv->volume + 127)/2.55);
			return MPXP_Ok;
		}
		case AOCONTROL_SET_VOLUME:
		{
			float diff;
			ao_control_vol_t* vol = (ao_control_vol_t*)arg;
			diff = (vol->left+vol->right) / 2;
			priv->volume = (int)(diff * 2.55) - 127;
			return MPXP_Ok;
		}
	}
	return -1;
}

// SDL Callback function
static void outputaudio(any_t* ao, Uint8 *stream, int len) {
    read_buffer(ao,stream, len);
}

// open & setup audio device
// return: 1=success 0=fail
static MPXP_Rc __FASTCALL__ init(ao_data_t* ao,unsigned flags)
{
    unsigned i;
    UNUSED(flags);
    ao->priv=mp_mallocz(sizeof(priv_t));
    priv_t*priv=ao->priv;
    priv->volume=127;
    /* Allocate ring-priv->buffer memory */
    for(i=0;i<NUM_BUFS;i++) priv->buffer[i]=(unsigned char *) mp_malloc(BUFFSIZE);

    if(ao->subdevice) {
	setenv("SDL_AUDIODRIVER", ao->subdevice, 1);
    }
    return MPXP_Ok;
}

static MPXP_Rc __FASTCALL__ configure(ao_data_t* ao,unsigned rate,unsigned channels,unsigned format)
{
    /* SDL Audio Specifications */
    SDL_AudioSpec aspec, obtained;
    char drv_name[80];

    ao->channels=channels;
    ao->samplerate=rate;
    ao->format=format;

    ao->bps=channels*rate;
    if(format != AFMT_U8 && format != AFMT_S8)
	 ao->bps*=2;

    /* The desired audio format (see SDL_AudioSpec) */
    switch(format) {
	case AFMT_U8:
	    aspec.format = AUDIO_U8;
	    break;
	case AFMT_S16_LE:
	    aspec.format = AUDIO_S16LSB;
	    break;
	case AFMT_S16_BE:
	    aspec.format = AUDIO_S16MSB;
	    break;
	case AFMT_S8:
	    aspec.format = AUDIO_S8;
	    break;
	case AFMT_U16_LE:
	    aspec.format = AUDIO_U16LSB;
	    break;
	case AFMT_U16_BE:
	    aspec.format = AUDIO_U16MSB;
	    break;
	default:
	    MSG_ERR("SDL: Unsupported audio format: 0x%x.\n", format);
	    return MPXP_False;
    }

    /* The desired audio frequency in samples-per-second. */
    aspec.freq     = rate;

    /* Number of channels (mono/stereo) */
    aspec.channels = channels;

    /* The desired size of the audio priv->buffer in samples. This number should be a power of two, and may be adjusted by the audio driver to a value more suitable for the hardware. Good values seem to range between 512 and 8192 inclusive, depending on the application and CPU speed. Smaller values yield faster response time, but can lead to underflow if the application is doing heavy processing and cannot fill the audio priv->buffer in time. A stereo sample consists of both right and left channels in LR ordering. Note that the number of samples is directly related to time by the following formula: ms = (samples*1000)/freq */
    aspec.samples  = SAMPLESIZE;

    /* This should be set to a function that will be called when the audio device is ready for more data. It is passed a pointer to the audio priv->buffer, and the length in bytes of the audio priv->buffer. This function usually runs in a separate thread, and so you should protect data structures that it accesses by calling SDL_LockAudio and SDL_UnlockAudio in your code. The callback prototype is:
	void callback(any_t*userdata, Uint8 *stream, int len); userdata is the pointer stored in userdata field of the SDL_AudioSpec. stream is a pointer to the audio priv->buffer you want to fill with information and len is the length of the audio priv->buffer in bytes. */
    aspec.callback = outputaudio;

    /* This pointer is passed as the first parameter to the callback function. */
    aspec.userdata = ao;

    /* initialize the SDL Audio system */
    if (SDL_Init (SDL_INIT_AUDIO/*|SDL_INIT_NOPARACHUTE*/)) {
	MSG_ERR("SDL: Initializing of SDL Audio failed: %s.\n", SDL_GetError());
	return MPXP_False;
    }

    /* Open the audio device and start playing sound! */
    if(SDL_OpenAudio(&aspec, &obtained) < 0) {
	MSG_ERR("SDL: Unable to open audio: %s\n", SDL_GetError());
	return MPXP_False;
    }

    /* did we got what we wanted ? */
    ao->channels=obtained.channels;
    ao->samplerate=obtained.freq;

    switch(obtained.format) {
	case AUDIO_U8 :
	    ao->format = AFMT_U8;
	    break;
	case AUDIO_S16LSB :
	    ao->format = AFMT_S16_LE;
	    break;
	case AUDIO_S16MSB :
	    ao->format = AFMT_S16_BE;
	    break;
	case AUDIO_S8 :
	    ao->format = AFMT_S8;
	    break;
	case AUDIO_U16LSB :
	    ao->format = AFMT_U16_LE;
	    break;
	case AUDIO_U16MSB :
	    ao->format = AFMT_U16_BE;
	    break;
	default:
	    MSG_WARN("SDL: Unsupported SDL audio format: 0x%x.\n", obtained.format);
	    return MPXP_False;
    }

    MSG_V("SDL: buf size = %d\n",aspec.size);
    ao->buffersize=obtained.size;

    SDL_AudioDriverName(drv_name, sizeof(drv_name));
    MSG_OK("SDL: using %s audio driver (%iHz %s \"%s\")\n"
		,drv_name
		,rate
		,channels>4?"Surround":channels>2?"Quadro":channels>1?"Stereo":"Mono"
		,ao_format_name(format));

    /* unsilence audio, if callback is ready */
    SDL_PauseAudio(0);

    return MPXP_Ok;
}

// close audio device
static void uninit(ao_data_t* ao){
	MSG_V("SDL: Audio Subsystem shutting down!\n");
	SDL_CloseAudio();
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
	mp_free(ao->priv);
}

// stop playing and empty buffers (for seeking/pause)
static void reset(ao_data_t* ao){
    priv_t*priv=ao->priv;
	/* Reset ring-priv->buffer state */
	priv->buf_read=0;
	priv->buf_write=0;
	priv->buf_read_pos=0;
	priv->buf_write_pos=0;

	priv->full_buffers=0;
	priv->buffered_bytes=0;

}

// stop playing, keep buffers (for pause)
static void audio_pause(ao_data_t* ao)
{
    UNUSED(ao);
    SDL_PauseAudio(1);
}

// resume playing, after audio_pause()
static void audio_resume(ao_data_t* ao)
{
    UNUSED(ao);
    SDL_PauseAudio(0);
}


// return: how many bytes can be played without blocking
static unsigned get_space(const ao_data_t* ao){
    priv_t*priv=ao->priv;
    return (NUM_BUFS-priv->full_buffers)*BUFFSIZE - priv->buf_write_pos;
}

// plays 'len' bytes of 'data'
// it should round it down to outburst*n
// return: number of bytes played
static unsigned __FASTCALL__ play(ao_data_t* ao,const any_t* data,unsigned len,unsigned flags)
{
    UNUSED(flags);
    return write_buffer(ao,data, len);
}

// return: delay in seconds between first and last sample in priv->buffer
static float get_delay(const ao_data_t* ao){
    priv_t*priv=ao->priv;
    return (float)(priv->buffered_bytes + ao->buffersize)/(float)ao->bps;
}
