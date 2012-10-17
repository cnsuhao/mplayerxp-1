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
#include "../libvo/fastmemcpy.h"
#include "ao_msg.h"

static ao_info_t info = 
{
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
typedef struct sdl_priv_s {
    unsigned char *	buffer[NUM_BUFS];
    unsigned int	buf_read;
    unsigned int	buf_write;
    unsigned int	buf_read_pos;
    unsigned int	buf_write_pos;
    unsigned int	volume;
    int			full_buffers;
    int			buffered_bytes;
}sdl_priv_t;
static sdl_priv_t sdl = { {NULL}, 0, 0, 0, 0, 127, 0, 0};

static int __FASTCALL__ write_buffer(unsigned char* data,int len){
  int len2=0;
  int x;
  while(len>0){
    if(sdl.full_buffers==NUM_BUFS) break;
    x=BUFFSIZE-sdl.buf_write_pos;
    if(x>len) x=len;
    memcpy(sdl.buffer[sdl.buf_write]+sdl.buf_write_pos,data+len2,x);
    len2+=x; len-=x;
    sdl.buffered_bytes+=x; sdl.buf_write_pos+=x;
    if(sdl.buf_write_pos>=BUFFSIZE){
       // block is full, find next!
       sdl.buf_write=(sdl.buf_write+1)%NUM_BUFS;
       ++sdl.full_buffers;
       sdl.buf_write_pos=0;
    }
  }
  return len2;
}

static int __FASTCALL__ read_buffer(unsigned char* data,int len){
  int len2=0;
  int x;
  while(len>0){
    if(sdl.full_buffers==0) break; // no more data buffered!
    x=BUFFSIZE-sdl.buf_read_pos;
    if(x>len) x=len;
    memcpy(data+len2,sdl.buffer[sdl.buf_read]+sdl.buf_read_pos,x);
    SDL_MixAudio(data+len2, data+len2, x, sdl.volume);
    len2+=x; len-=x;
    sdl.buffered_bytes-=x; sdl.buf_read_pos+=x;
    if(sdl.buf_read_pos>=BUFFSIZE){
       // block is empty, find next!
       sdl.buf_read=(sdl.buf_read+1)%NUM_BUFS;
       --sdl.full_buffers;
       sdl.buf_read_pos=0;
    }
  }
  return len2;
}

// end ring sdl.buffer stuff

#if	 defined(HPUX) || defined(sun) && defined(__svr4__)
/* setenv is missing on solaris and HPUX */
static void setenv(const char *name, const char *val, int _xx)
{
  int len  = strlen(name) + strlen(val) + 2;
  char *env = malloc(len);

  if (env != NULL) {
    strcpy(env, name);
    strcat(env, "=");
    strcat(env, val);
    putenv(env);
  }
}
#endif


// to set/get/query special features/parameters
static int __FASTCALL__ control(int cmd,long arg){
	switch (cmd) {
		case AOCONTROL_QUERY_FORMAT:
		case AOCONTROL_QUERY_CHANNELS:
    		case AOCONTROL_QUERY_RATE:
		    return CONTROL_FALSE;
		case AOCONTROL_GET_VOLUME:
		{
			ao_control_vol_t* vol = (ao_control_vol_t*)arg;
			vol->left = vol->right = (float)((sdl.volume + 127)/2.55);
			return CONTROL_OK;
		}
		case AOCONTROL_SET_VOLUME:
		{
			float diff;
			ao_control_vol_t* vol = (ao_control_vol_t*)arg;
			diff = (vol->left+vol->right) / 2;
			sdl.volume = (int)(diff * 2.55) - 127;
			return CONTROL_OK;
		}
	}
	return -1;
}

// SDL Callback function
static void outputaudio(any_t*unused, Uint8 *stream, int len) {
    UNUSED(unused);
    read_buffer(stream, len);
}

// open & setup audio device
// return: 1=success 0=fail
static int __FASTCALL__ init(unsigned flags)
{
	unsigned i;
	UNUSED(flags);
	/* Allocate ring-sdl.buffer memory */
	for(i=0;i<NUM_BUFS;i++) sdl.buffer[i]=(unsigned char *) malloc(BUFFSIZE);

	if(ao_subdevice) {
		setenv("SDL_AUDIODRIVER", ao_subdevice, 1);
	}
	return 1;
}

static int __FASTCALL__ configure(unsigned rate,unsigned channels,unsigned format)
{
	/* SDL Audio Specifications */
	SDL_AudioSpec aspec, obtained;
	char drv_name[80];

	ao_data.channels=channels;
	ao_data.samplerate=rate;
	ao_data.format=format;

	ao_data.bps=channels*rate;
	if(format != AFMT_U8 && format != AFMT_S8)
	  ao_data.bps*=2;
	
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
                return 0;
	}

	/* The desired audio frequency in samples-per-second. */
	aspec.freq     = rate;

	/* Number of channels (mono/stereo) */
	aspec.channels = channels;

	/* The desired size of the audio sdl.buffer in samples. This number should be a power of two, and may be adjusted by the audio driver to a value more suitable for the hardware. Good values seem to range between 512 and 8192 inclusive, depending on the application and CPU speed. Smaller values yield faster response time, but can lead to underflow if the application is doing heavy processing and cannot fill the audio sdl.buffer in time. A stereo sample consists of both right and left channels in LR ordering. Note that the number of samples is directly related to time by the following formula: ms = (samples*1000)/freq */
	aspec.samples  = SAMPLESIZE;

	/* This should be set to a function that will be called when the audio device is ready for more data. It is passed a pointer to the audio sdl.buffer, and the length in bytes of the audio sdl.buffer. This function usually runs in a separate thread, and so you should protect data structures that it accesses by calling SDL_LockAudio and SDL_UnlockAudio in your code. The callback prototype is:
void callback(any_t*userdata, Uint8 *stream, int len); userdata is the pointer stored in userdata field of the SDL_AudioSpec. stream is a pointer to the audio sdl.buffer you want to fill with information and len is the length of the audio sdl.buffer in bytes. */
	aspec.callback = outputaudio;

	/* This pointer is passed as the first parameter to the callback function. */
	aspec.userdata = NULL;

	/* initialize the SDL Audio system */
	if (SDL_Init (SDL_INIT_AUDIO/*|SDL_INIT_NOPARACHUTE*/)) {
		MSG_ERR("SDL: Initializing of SDL Audio failed: %s.\n", SDL_GetError());
		return 0;
        }

	/* Open the audio device and start playing sound! */
	if(SDL_OpenAudio(&aspec, &obtained) < 0) {
		MSG_ERR("SDL: Unable to open audio: %s\n", SDL_GetError());
		return(0);
	}
	
	/* did we got what we wanted ? */
	ao_data.channels=obtained.channels;
	ao_data.samplerate=obtained.freq;

	switch(obtained.format) {
	    case AUDIO_U8 :
		ao_data.format = AFMT_U8;
	    break;
	    case AUDIO_S16LSB :
		ao_data.format = AFMT_S16_LE;
	    break;
	    case AUDIO_S16MSB :
		ao_data.format = AFMT_S16_BE;
	    break;
	    case AUDIO_S8 :
		ao_data.format = AFMT_S8;
	    break;
	    case AUDIO_U16LSB :
		ao_data.format = AFMT_U16_LE;
	    break;
	    case AUDIO_U16MSB :
		ao_data.format = AFMT_U16_BE;
	    break;
	    default:
		MSG_WARN("SDL: Unsupported SDL audio format: 0x%x.\n", obtained.format);
		return 0;
	}

	MSG_V("SDL: buf size = %d\n",aspec.size);
	ao_data.buffersize=obtained.size;

	SDL_AudioDriverName(drv_name, sizeof(drv_name));
	MSG_OK("SDL: using %s audio driver (%iHz %s \"%s\")\n"
		,drv_name
		,rate
		,channels>4?"Surround":channels>2?"Quadro":channels>1?"Stereo":"Mono"
		,ao_format_name(format));

	/* unsilence audio, if callback is ready */
	SDL_PauseAudio(0);

	return 1;
}

// close audio device
static void uninit(void){
	MSG_V("SDL: Audio Subsystem shutting down!\n");
	SDL_CloseAudio();
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

// stop playing and empty buffers (for seeking/pause)
static void reset(void){

	/* Reset ring-sdl.buffer state */
	sdl.buf_read=0;
	sdl.buf_write=0;
	sdl.buf_read_pos=0;
	sdl.buf_write_pos=0;

	sdl.full_buffers=0;
	sdl.buffered_bytes=0;

}

// stop playing, keep buffers (for pause)
static void audio_pause(void)
{
	SDL_PauseAudio(1);
}

// resume playing, after audio_pause()
static void audio_resume(void)
{
	SDL_PauseAudio(0);
}


// return: how many bytes can be played without blocking
static unsigned get_space(void){
    return (NUM_BUFS-sdl.full_buffers)*BUFFSIZE - sdl.buf_write_pos;
}

// plays 'len' bytes of 'data'
// it should round it down to outburst*n
// return: number of bytes played
static unsigned __FASTCALL__ play(any_t* data,unsigned len,unsigned flags)
{
    UNUSED(flags);
#if 0
	int ret;

	/* Audio locking prohibits call of outputaudio */
	SDL_LockAudio();
	// copy audio stream into ring-sdl.buffer 
	ret = write_buffer(data, len);
	SDL_UnlockAudio();

	return ret;
#else
	return write_buffer(data, len);
#endif
}

// return: delay in seconds between first and last sample in sdl.buffer
static float get_delay(void){
    return (float)(sdl.buffered_bytes + ao_data.buffersize)/(float)ao_data.bps;
}
