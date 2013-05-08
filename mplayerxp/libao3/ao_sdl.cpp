#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
/*
 * ao_sdl.c - libao3 SDLlib Audio Output Driver for MPlayer
 *
 * This driver is under the same license as MPlayer.
 * (http://mplayer.sf.net)
 *
 * Copyleft 2001 by Felix Bünemann (atmosfear@users.sf.net)
 *
 * Thanks to Arpi for nice ringbuffer-code!
 *
 */
#include <algorithm>
#include <iomanip>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <SDL/SDL.h>

#include "audio_out.h"
#include "audio_out_internal.h"
#include "afmt.h"
#include "osdep/fastmemcpy.h"
#include "ao_msg.h"

namespace	usr {
// Samplesize used by the SDLlib AudioSpec struct
static const int SAMPLESIZE=1024;

// General purpose Ring-buffering routines

static const int BUFFSIZE=4096;
static const int NUM_BUFS=16;
class SDL_AO_Interface : public AO_Interface {
    public:
	SDL_AO_Interface(const std::string& subdevice);
	virtual ~SDL_AO_Interface();

	virtual MPXP_Rc		open(unsigned flags);
	virtual MPXP_Rc		configure(unsigned rate,unsigned channels,unsigned format);
	virtual unsigned	samplerate() const;
	virtual unsigned	channels() const;
	virtual unsigned	format() const;
	virtual unsigned	buffersize() const;
	virtual unsigned	outburst() const;
	virtual MPXP_Rc		test_rate(unsigned r) const;
	virtual MPXP_Rc		test_channels(unsigned c) const;
	virtual MPXP_Rc		test_format(unsigned f) const;
	virtual void		reset();
	virtual unsigned	get_space();
	virtual float		get_delay();
	virtual unsigned	play(const any_t* data,unsigned len,unsigned flags);
	virtual void		pause();
	virtual void		resume();
	virtual MPXP_Rc		ctrl(int cmd,long arg) const;
	virtual int		read_buffer(uint8_t* data,int len);
    private:
	unsigned	_channels,_samplerate,_format;
	unsigned	_buffersize,_outburst;
	unsigned	bps() const { return _channels*_samplerate*afmt2bps(_format); }
	int		write_buffer(const uint8_t* data,int len);

	uint8_t*	buffer[NUM_BUFS];
	unsigned	buf_read;
	unsigned	buf_write;
	unsigned	buf_read_pos;
	unsigned	buf_write_pos;
	unsigned*	volume;
	int		full_buffers;
	int		buffered_bytes;
};

SDL_AO_Interface::SDL_AO_Interface(const std::string& _subdevice)
		:AO_Interface(_subdevice),volume(new unsigned) {}

SDL_AO_Interface::~SDL_AO_Interface() {
    mpxp_v<<"SDL: Audio Subsystem shutting down!"<<std::endl;
    SDL_CloseAudio();
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    delete volume;
}

int SDL_AO_Interface::write_buffer(const uint8_t* data,int len){
    int len2=0;
    int x;
    while(len>0){
	if(full_buffers==NUM_BUFS) break;
	x=std::min(unsigned(len),BUFFSIZE-buf_write_pos);
	memcpy(buffer[buf_write]+buf_write_pos,data+len2,x);
	len2+=x; len-=x;
	buffered_bytes+=x; buf_write_pos+=x;
	if(buf_write_pos>=BUFFSIZE){
	    // block is full, find next!
	    buf_write=(buf_write+1)%NUM_BUFS;
	    ++full_buffers;
	    buf_write_pos=0;
	}
    }
    return len2;
}

int SDL_AO_Interface::read_buffer(uint8_t* data,int len){
    int len2=0;
    int x;
    while(len>0){
	if(full_buffers==0) break; // no more data buffered!
	x=std::min(unsigned(len),BUFFSIZE-buf_read_pos);
	memcpy(data+len2,buffer[buf_read]+buf_read_pos,x);
	SDL_MixAudio(data+len2, data+len2, x, *volume);
	len2+=x; len-=x;
	buffered_bytes-=x; buf_read_pos+=x;
	if(buf_read_pos>=BUFFSIZE){
	    // block is empty, find next!
	    buf_read=(buf_read+1)%NUM_BUFS;
	    --full_buffers;
	    buf_read_pos=0;
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
MPXP_Rc SDL_AO_Interface::ctrl(int cmd,long arg) const {
    switch (cmd) {
	case AOCONTROL_GET_VOLUME: {
	    ao_control_vol_t* vol = (ao_control_vol_t*)arg;
	    vol->left = vol->right = (float)((*volume + 127)/2.55);
	    return MPXP_Ok;
	}
	case AOCONTROL_SET_VOLUME: {
	    float diff;
	    ao_control_vol_t* vol = (ao_control_vol_t*)arg;
	    diff = (vol->left+vol->right) / 2;
	    *volume = (int)(diff * 2.55) - 127;
	    return MPXP_Ok;
	    return MPXP_False;
	}
    }
    return MPXP_Unknown;
}

// SDL Callback function
static void outputaudio(any_t* ao, Uint8 *stream, int len) {
    SDL_AO_Interface& _this=*reinterpret_cast<SDL_AO_Interface*>(ao);
    _this.read_buffer(stream, len);
}

// open & setup audio device
// return: 1=success 0=fail
MPXP_Rc SDL_AO_Interface::open(unsigned flags)
{
    unsigned i;
    UNUSED(flags);
    *volume=127;
    /* Allocate ring-priv->buffer memory */
    for(i=0;i<NUM_BUFS;i++) buffer[i]=new uint8_t[BUFFSIZE];

    if(!subdevice.empty()) ::setenv("SDL_AUDIODRIVER", subdevice.c_str(), 1);
    return MPXP_Ok;
}

MPXP_Rc SDL_AO_Interface::configure(unsigned r,unsigned c,unsigned f)
{
    /* SDL Audio Specifications */
    SDL_AudioSpec aspec, obtained;
    char drv_name[80];

    _channels=c;
    _samplerate=r;
    _format=f;

    /* The desired audio format (see SDL_AudioSpec) */
    switch(_format) {
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
	    mpxp_err<<"SDL: Unsupported audio format: 0x"<<std::hex<<_format<<std::endl;
	    return MPXP_False;
    }

    /* The desired audio frequency in samples-per-second. */
    aspec.freq     = _samplerate;

    /* Number of channels (mono/stereo) */
    aspec.channels = _channels;

    /* The desired size of the audio priv->buffer in samples. This number should be a power of two, and may be adjusted by the audio driver to a value more suitable for the hardware. Good values seem to range between 512 and 8192 inclusive, depending on the application and CPU speed. Smaller values yield faster response time, but can lead to underflow if the application is doing heavy processing and cannot fill the audio priv->buffer in time. A stereo sample consists of both right and left channels in LR ordering. Note that the number of samples is directly related to time by the following formula: ms = (samples*1000)/freq */
    aspec.samples  = SAMPLESIZE;

    /* This should be set to a function that will be called when the audio device is ready for more data. It is passed a pointer to the audio priv->buffer, and the length in bytes of the audio priv->buffer. This function usually runs in a separate thread, and so you should protect data structures that it accesses by calling SDL_LockAudio and SDL_UnlockAudio in your code. The callback prototype is:
	void callback(any_t*userdata, Uint8 *stream, int len); userdata is the pointer stored in userdata field of the SDL_AudioSpec. stream is a pointer to the audio priv->buffer you want to fill with information and len is the length of the audio priv->buffer in bytes. */
    aspec.callback = outputaudio;

    /* This pointer is passed as the first parameter to the callback function. */
    aspec.userdata = this;

    /* initialize the SDL Audio system */
    if (SDL_Init (SDL_INIT_AUDIO/*|SDL_INIT_NOPARACHUTE*/)) {
	mpxp_err<<"SDL: Initializing of SDL Audio failed: "<<SDL_GetError()<<std::endl;
	return MPXP_False;
    }

    /* Open the audio device and start playing sound! */
    if(SDL_OpenAudio(&aspec, &obtained) < 0) {
	mpxp_err<<"SDL: Unable to open audio: "<<SDL_GetError()<<std::endl;
	return MPXP_False;
    }

    /* did we got what we wanted ? */
    _channels=obtained.channels;
    _samplerate=obtained.freq;

    switch(obtained.format) {
	case AUDIO_U8 :
	    _format = AFMT_U8;
	    break;
	case AUDIO_S16LSB :
	    _format = AFMT_S16_LE;
	    break;
	case AUDIO_S16MSB :
	    _format = AFMT_S16_BE;
	    break;
	case AUDIO_S8 :
	    _format = AFMT_S8;
	    break;
	case AUDIO_U16LSB :
	    _format = AFMT_U16_LE;
	    break;
	case AUDIO_U16MSB :
	    _format = AFMT_U16_BE;
	    break;
	default:
	    mpxp_warn<<"SDL: Unsupported SDL audio format: 0x"<<std::hex<<obtained.format<<std::endl;
	    return MPXP_False;
    }

    mpxp_v<<"SDL: buf size = "<<aspec.size<<std::endl;
    _buffersize=obtained.size;

    SDL_AudioDriverName(drv_name, sizeof(drv_name));
    mpxp_ok<<"SDL: using "<<drv_name<<" audio driver ("<<_samplerate<<"Hz "<<(_channels>4?"Surround":_channels>2?"Quadro":_channels>1?"Stereo":"Mono")<<" \""<<ao_format_name(_format)<<"\")"<<std::endl;

    /* unsilence audio, if callback is ready */
    SDL_PauseAudio(0);

    return MPXP_Ok;
}

// stop playing and empty buffers (for seeking/pause)
void SDL_AO_Interface::reset(){
    /* Reset ring-priv->buffer state */
    buf_read=0;
    buf_write=0;
    buf_read_pos=0;
    buf_write_pos=0;

    full_buffers=0;
    buffered_bytes=0;
}

// stop playing, keep buffers (for pause)
void SDL_AO_Interface::pause() { SDL_PauseAudio(1); }

// resume playing, after audio_pause()
void SDL_AO_Interface::resume() { SDL_PauseAudio(0); }

// return: how many bytes can be played without blocking
unsigned SDL_AO_Interface::get_space(){
    return (NUM_BUFS-full_buffers)*BUFFSIZE - buf_write_pos;
}

// plays 'len' bytes of 'data'
// it should round it down to outburst*n
// return: number of bytes played
unsigned SDL_AO_Interface::play(const any_t* data,unsigned len,unsigned flags)
{
    UNUSED(flags);
    return write_buffer(reinterpret_cast<const uint8_t*>(data), len);
}

// return: delay in seconds between first and last sample in priv->buffer
float SDL_AO_Interface::get_delay(){
    return (float)(buffered_bytes + _buffersize)/(float)bps();
}

unsigned SDL_AO_Interface::samplerate() const { return _samplerate; }
unsigned SDL_AO_Interface::channels() const { return _channels; }
unsigned SDL_AO_Interface::format() const { return _format; }
unsigned SDL_AO_Interface::buffersize() const { return _buffersize; }
unsigned SDL_AO_Interface::outburst() const { return _outburst; }
MPXP_Rc  SDL_AO_Interface::test_channels(unsigned c) const { UNUSED(c); return MPXP_Ok; }
MPXP_Rc  SDL_AO_Interface::test_rate(unsigned r) const { UNUSED(r); return MPXP_Ok; }
MPXP_Rc  SDL_AO_Interface::test_format(unsigned f) const {
    switch (f) {
	case AFMT_U8:
	case AFMT_S8:
	case AFMT_U16_LE:
	case AFMT_S16_LE:
	case AFMT_U16_BE:
	case AFMT_S16_BE: return MPXP_Ok;
	default: break;
    }
    return MPXP_False;
}

static AO_Interface* query_interface(const std::string& sd) { return new(zeromem) SDL_AO_Interface(sd); }

extern const ao_info_t audio_out_sdl = {
    "SDLlib audio output",
    "sdl",
    "Felix Buenemann <atmosfear@users.sourceforge.net>",
    "",
    query_interface
};
} // namespace	usr
