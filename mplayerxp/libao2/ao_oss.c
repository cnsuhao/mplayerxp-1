#include <stdio.h>
#include <stdlib.h>

#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include "mp_config.h"
#include "mixer.h"
#include "osdep/mplib.h"

#include "afmt.h"
#include "audio_out.h"
#include "audio_out_internal.h"
#include "ao_msg.h"

static ao_info_t info =
{
	"OSS/ioctl audio output",
	"oss",
	"A'rpi",
	""
};

/* Support for >2 output channels added 2001-11-25 - Steve Davies <steve@daviesfam.org> */

LIBAO_EXTERN(oss)

typedef struct priv_s {
    const char *	dsp;
    int			mixer_channel;
    int			fd;
    audio_buf_info	zz;
    int			delay_method;
}priv_t;

const char *oss_mixer_device = PATH_DEV_MIXER;

// to set/get/query special features/parameters
static ControlCodes __FASTCALL__ control(ao_data_t* ao,int cmd,long arg){
    priv_t*priv=ao->priv;
    int rval;
    switch(cmd){
	case AOCONTROL_SET_DEVICE:
	    priv->dsp=(char*)arg;
	    return CONTROL_OK;
	case AOCONTROL_QUERY_FORMAT:
	    if (ioctl (priv->fd, SNDCTL_DSP_GETFMTS, &rval) != -1)
	    {
		if((rval & AFMT_MU_LAW) && arg==AFMT_MU_LAW) return CONTROL_OK;
		if((rval & AFMT_A_LAW) && arg==AFMT_A_LAW) return CONTROL_OK;
		if((rval & AFMT_IMA_ADPCM) && arg==AFMT_IMA_ADPCM) return CONTROL_OK;
		if((rval & AFMT_U8) && arg==AFMT_U8) return CONTROL_OK;
		if((rval & AFMT_S16_LE) && arg==AFMT_S16_LE) return CONTROL_OK;
		if((rval & AFMT_S16_BE) && arg==AFMT_S16_BE) return CONTROL_OK;
		if((rval & AFMT_S8) && arg==AFMT_S8) return CONTROL_OK;
		if((rval & AFMT_U16_LE) && arg==AFMT_U16_LE) return CONTROL_OK;
		if((rval & AFMT_U16_BE) && arg==AFMT_U16_BE) return CONTROL_OK;
		if((rval & AFMT_MPEG) && arg==AFMT_MPEG) return CONTROL_OK;
		if((rval & AFMT_AC3) && arg==AFMT_AC3) return CONTROL_OK;
		if((rval & AFMT_S24_LE) && arg==AFMT_S24_LE) return CONTROL_OK;
		if((rval & AFMT_S24_BE) && arg==AFMT_S24_BE) return CONTROL_OK;
		if((rval & AFMT_U24_LE) && arg==AFMT_U24_LE) return CONTROL_OK;
		if((rval & AFMT_U24_BE) && arg==AFMT_U24_BE) return CONTROL_OK;
		if((rval & AFMT_S32_LE) && arg==AFMT_S32_LE) return CONTROL_OK;
		if((rval & AFMT_S32_BE) && arg==AFMT_S32_BE) return CONTROL_OK;
		if((rval & AFMT_U32_LE) && arg==AFMT_U32_LE) return CONTROL_OK;
		if((rval & AFMT_U32_BE) && arg==AFMT_U32_BE) return CONTROL_OK;
	    }
	    return CONTROL_FALSE;
	case AOCONTROL_QUERY_CHANNELS:
	    rval=arg;
	    if (rval > 2) {
		if ( ioctl(priv->fd, SNDCTL_DSP_CHANNELS, &rval) == -1 ||
		rval != arg ) return CONTROL_FALSE;
	    }
	    else {
		int c = rval-1;
		if (ioctl (priv->fd, SNDCTL_DSP_STEREO, &c) == -1) return CONTROL_FALSE;
	    }
	    return CONTROL_TRUE;
	case AOCONTROL_QUERY_RATE:
	    rval=arg;
	    if (ioctl(priv->fd, SNDCTL_DSP_SPEED, &rval) != -1)
	    {
		if(rval == arg) return CONTROL_OK;
	    }
	    return CONTROL_FALSE;
	case AOCONTROL_GET_VOLUME:
	case AOCONTROL_SET_VOLUME:
	{
	    ao_control_vol_t *vol = (ao_control_vol_t *)arg;
	    int fd, v, devs;

	    if(ao->format == AFMT_AC3)
		return CONTROL_TRUE;

	    if ((fd = open(oss_mixer_device, O_RDONLY)) > 0)
	    {
		ioctl(fd, SOUND_MIXER_READ_DEVMASK, &devs);
		if (devs & (1 << priv->mixer_channel))
		{
		    if (cmd == AOCONTROL_GET_VOLUME)
		    {
		        ioctl(fd, MIXER_READ(priv->mixer_channel), &v);
			vol->right = (v & 0xFF00) >> 8;
			vol->left = v & 0x00FF;
		    }
		    else
		    {
		        v = ((int)vol->right << 8) | (int)vol->left;
			ioctl(fd, MIXER_WRITE(priv->mixer_channel), &v);
		    }
		}
		else
		{
		    close(fd);
		    return CONTROL_ERROR;
		}
		close(fd);
		return CONTROL_OK;
	    }
	}
	return CONTROL_ERROR;
    }
    return CONTROL_UNKNOWN;
}

static void show_fmts(ao_data_t* ao)
{
    priv_t*priv=ao->priv;
  int rval;
  rval=0;
  if (ioctl (priv->fd, SNDCTL_DSP_GETFMTS, &rval) != -1)
  {
	MSG_INFO("AO-INFO: List of supported formats: ");
	if(rval & AFMT_MU_LAW) MSG_INFO("AFMT_MU_LAW ");
	if(rval & AFMT_A_LAW) MSG_INFO("AFMT_A_LAW ");
	if(rval & AFMT_IMA_ADPCM) MSG_INFO("AFMT_IMA_ADPCM ");
	if(rval & AFMT_U8) MSG_INFO("AFMT_U8 ");
	if(rval & AFMT_S16_LE) MSG_INFO("AFMT_S16_LE ");
	if(rval & AFMT_S16_BE) MSG_INFO("AFMT_S16_BE ");
	if(rval & AFMT_S8) MSG_INFO("AFMT_S8 ");
	if(rval & AFMT_U16_LE) MSG_INFO("AFMT_U16_LE ");

	if(rval & AFMT_U16_BE) MSG_INFO("AFMT_U16_BE ");
	if(rval & AFMT_MPEG) MSG_INFO("AFMT_MPEG ");
	if(rval & AFMT_AC3) MSG_INFO("AFMT_AC3 ");
	if(rval & AFMT_S24_LE) MSG_INFO("AFMT_S24_LE ");
	if(rval & AFMT_S24_BE) MSG_INFO("AFMT_S24_BE ");
	if(rval & AFMT_U24_LE) MSG_INFO("AFMT_U24_LE ");
	if(rval & AFMT_U24_BE) MSG_INFO("AFMT_U24_LE ");
	if(rval & AFMT_S32_LE) MSG_INFO("AFMT_S32_LE ");
	if(rval & AFMT_S32_BE) MSG_INFO("AFMT_S32_BE ");
	if(rval & AFMT_U32_LE) MSG_INFO("AFMT_U32_LE ");
	if(rval & AFMT_U32_BE) MSG_INFO("AFMT_U32_LE ");
	MSG_INFO("\n");	
  }
}

static void show_caps(ao_data_t* ao)
{
    priv_t*priv=ao->priv;
  int rval;
#ifdef __linux__
  priv->fd=open(priv->dsp, O_WRONLY | O_NONBLOCK);
#else
  priv->fd=open(priv->dsp, O_WRONLY);
#endif
  if(priv->fd<0){
    MSG_ERR("Can't open audio device %s: %s\n", priv->dsp, strerror(errno));
    return ;
  }
  show_fmts(ao);
  rval=0;
  if (ioctl (priv->fd, SNDCTL_DSP_GETCAPS, &rval) != -1)
  {
	MSG_INFO("AO-INFO: Capabilities: ");
	MSG_INFO("rev-%u ",rval & DSP_CAP_REVISION);
	if(rval & DSP_CAP_DUPLEX) MSG_INFO("duplex ");
	if(rval & DSP_CAP_REALTIME) MSG_INFO("realtime ");
	if(rval & DSP_CAP_BATCH) MSG_INFO("batch ");
	if(rval & DSP_CAP_COPROC) MSG_INFO("coproc ");
	if(rval & DSP_CAP_TRIGGER) MSG_INFO("trigger ");
	if(rval & DSP_CAP_MMAP) MSG_INFO("mmap ");
	if(rval & DSP_CAP_MULTI) MSG_INFO("multiopen ");
	if(rval & DSP_CAP_BIND) MSG_INFO("bind ");
	MSG_INFO("\n");
  }
  close(priv->fd);
}
// open & setup audio device
// return: 1=success 0=fail
static int __FASTCALL__ init(ao_data_t* ao,unsigned flags){
  char *mixer_channels [SOUND_MIXER_NRDEVICES] = SOUND_DEVICE_NAMES;
  UNUSED(flags);
  ao->priv=mp_mallocz(sizeof(priv_t));
  priv_t*priv=ao->priv;
  priv->dsp=PATH_DEV_DSP;
  priv->mixer_channel=SOUND_MIXER_PCM;
  priv->fd=-1;
  priv->delay_method=2;
  if (ao->subdevice)
  {
    char *p;
    p=strrchr(ao->subdevice,':');
    priv->dsp = ao->subdevice;
    if(p) { *p=0; p++;  if(strcmp(p,"-1")==0) { show_caps(ao); return 0; } }
  }

  MSG_V("audio_setup: using '%s' priv->dsp device\n", priv->dsp);
  MSG_V("audio_setup: using '%s'(%s) mixer device\n", oss_mixer_device,mixer_channels[priv->mixer_channel]);

#ifdef __linux__
  priv->fd=open(priv->dsp, O_WRONLY | O_NONBLOCK);
#else
  priv->fd=open(priv->dsp, O_WRONLY);
#endif
  if(priv->fd<0){
    MSG_ERR("Can't open audio device %s: %s\n", priv->dsp, strerror(errno));
    return 0;
  }

#ifdef __linux__
  /* Remove the non-blocking flag */
  if(fcntl(priv->fd, F_SETFL, 0) < 0) {
   MSG_ERR("Can't make filedescriptor non-blocking: %s\n", strerror(errno));
   return 0;
  }
#endif

#if defined(FD_CLOEXEC) && defined(F_SETFD)
  fcntl(priv->fd, F_SETFD, FD_CLOEXEC);
#endif

    return 1;
}

static int __FASTCALL__ configure(ao_data_t* ao,unsigned rate,unsigned channels,unsigned format)
{
    priv_t*priv=ao->priv;
    MSG_V("ao2: %d Hz  %d chans  %s\n",rate,channels,
    ao_format_name(format));

  if(format == AFMT_AC3) {
    ao->samplerate=rate;
    ioctl (priv->fd, SNDCTL_DSP_SPEED, &ao->samplerate);
  }

ac3_retry:
  ao->format=format;
  if( ioctl(priv->fd, SNDCTL_DSP_SETFMT, &ao->format)<0 ||
      ao->format != format)
  {
   if(format == AFMT_AC3){
    MSG_WARN("OSS-CONF: Can't set audio device %s to AC3 output, trying S16...\n", priv->dsp);
#ifdef WORDS_BIGENDIAN
    format=AFMT_S16_BE;
#else
    format=AFMT_S16_LE;
#endif
    goto ac3_retry;
   }
   else
   {
    MSG_ERR("OSS-CONF: Can't configure for: %s\n",ao_format_name(format));
    show_fmts(ao);
    ao->format=format;
    return 0;
   }
  }
  ao->channels = channels;
  if(format != AFMT_AC3) {
    // We only use SNDCTL_DSP_CHANNELS for >2 channels, in case some drivers don't have it
    if (ao->channels > 2) {
      if ( ioctl(priv->fd, SNDCTL_DSP_CHANNELS, &ao->channels) == -1 ||
	   ao->channels != channels ) {
	MSG_ERR("OSS-CONF: Failed to set audio device to %d channels\n", channels);
	return 0;
      }
    }
    else {
      int c = ao->channels-1;
      if (ioctl (priv->fd, SNDCTL_DSP_STEREO, &c) == -1) {
	MSG_ERR("OSS-CONF: Failed to set audio device to %d channels\n", ao->channels);
	return 0;
      }
      ao->channels=c+1;
    }
    MSG_V("OSS-CONF: using %d channels (requested: %d)\n", ao->channels, channels);
    // set rate
    ao->samplerate=rate;
    ioctl (priv->fd, SNDCTL_DSP_SPEED, &ao->samplerate);
    MSG_V("OSS-CONF: using %d Hz samplerate (requested: %d)\n",ao->samplerate,rate);
  }

  if(ioctl(priv->fd, SNDCTL_DSP_GETOSPACE, &priv->zz)==-1){
      int r=0;
      MSG_WARN("OSS-CONF: driver doesn't support SNDCTL_DSP_GETOSPACE :-(\n");
      if(ioctl(priv->fd, SNDCTL_DSP_GETBLKSIZE, &r)==-1){
          MSG_V("OSS-CONF: %d bytes/frag (mp_config.h)\n",ao->outburst);
      } else {
          ao->outburst=r;
          MSG_V("OSS-CONF: %d bytes/frag (GETBLKSIZE)\n",ao->outburst);
      }
  } else {
      MSG_V("OSS-CONF: frags: %3d/%d  (%d bytes/frag)  mp_free: %6d\n",
          priv->zz.fragments, priv->zz.fragstotal, priv->zz.fragsize, priv->zz.bytes);
      if(ao->buffersize==0) ao->buffersize=priv->zz.bytes;
      ao->outburst=priv->zz.fragsize;
  }

  if(ao->buffersize==0){
    // Measuring buffer size:
    any_t* data;
    ao->buffersize=0;
#ifdef HAVE_AUDIO_SELECT
    data=mp_mallocz(ao->outburst);
    while(ao->buffersize<0x40000){
      fd_set rfds;
      struct timeval tv;
      FD_ZERO(&rfds); FD_SET(priv->fd,&rfds);
      tv.tv_sec=0; tv.tv_usec = 0;
      if(!select(priv->fd+1, NULL, &rfds, NULL, &tv)) break;
      write(priv->fd,data,ao->outburst);
      ao->buffersize+=ao->outburst;
    }
    mp_free(data);
    if(ao->buffersize==0){
        MSG_ERR("\n   *** OSS-CONF: Your audio driver DOES NOT support select()  ***\n"
          "Recompile mplayerxp with #undef HAVE_AUDIO_SELECT in mp_config.h !\n\n");
        return 0;
    }
#endif
  }

  ao->bps=ao->channels;
  if(ao->format != AFMT_U8 && ao->format != AFMT_S8)
    ao->bps*=2;

  ao->outburst-=ao->outburst % ao->bps; // round down
  ao->bps*=ao->samplerate;

  return 1;
}

// close audio device
static void uninit(ao_data_t* ao){
    priv_t*priv=ao->priv;
    if(priv->fd == -1) return;
#ifdef SNDCTL_DSP_RESET
    ioctl(priv->fd, SNDCTL_DSP_RESET, NULL);
#endif
    close(priv->fd);
    priv->fd = -1;
    mp_free(ao->priv);
}

// stop playing and empty buffers (for seeking/pause)
static void reset(ao_data_t* ao){
    priv_t*priv=ao->priv;
    uninit(ao);
    priv->fd=open(priv->dsp, O_WRONLY);
    if(priv->fd < 0){
	MSG_FATAL("\nFatal error: *** CANNOT RE-OPEN / RESET AUDIO DEVICE *** %s\n", strerror(errno));
	return;
    }

#if defined(FD_CLOEXEC) && defined(F_SETFD)
  fcntl(priv->fd, F_SETFD, FD_CLOEXEC);
#endif
  ioctl (priv->fd, SNDCTL_DSP_SETFMT, &ao->format);
  if(ao->format != AFMT_AC3) {
    if (ao->channels > 2)
      ioctl (priv->fd, SNDCTL_DSP_CHANNELS, &ao->channels);
    else {
      int c = ao->channels-1;
      ioctl (priv->fd, SNDCTL_DSP_STEREO, &c);
    }
    ioctl (priv->fd, SNDCTL_DSP_SPEED, &ao->samplerate);
  }
}

// stop playing, keep buffers (for pause)
static void audio_pause(ao_data_t* ao)
{
    // for now, just call reset();
    reset(ao);
}

// resume playing, after audio_pause()
static void audio_resume(ao_data_t* ao)
{
    reset(ao);
}


// return: how many bytes can be played without blocking
static unsigned get_space(ao_data_t* ao){
    priv_t*priv=ao->priv;
  unsigned playsize=ao->outburst;

#ifdef SNDCTL_DSP_GETOSPACE
  if(ioctl(priv->fd, SNDCTL_DSP_GETOSPACE, &priv->zz)!=-1){
      // calculate exact buffer space:
      playsize = priv->zz.fragments*priv->zz.fragsize;
      if (playsize > MAX_OUTBURST)
	playsize = (MAX_OUTBURST / priv->zz.fragsize) * priv->zz.fragsize;
      return playsize;
  }
#endif

    // check buffer
#ifdef HAVE_AUDIO_SELECT
    {  fd_set rfds;
       struct timeval tv;
       FD_ZERO(&rfds);
       FD_SET(priv->fd, &rfds);
       tv.tv_sec = 0;
       tv.tv_usec = 0;
       if(!select(priv->fd+1, NULL, &rfds, NULL, &tv)) return 0; // not block!
    }
#endif

  return ao->outburst;
}

// plays 'len' bytes of 'data'
// it should round it down to outburst*n
// return: number of bytes played
static unsigned __FASTCALL__ play(ao_data_t* ao,any_t* data,unsigned len,unsigned flags){
    priv_t*priv=ao->priv;
    UNUSED(flags);
    len/=ao->outburst;
    len=write(priv->fd,data,len*ao->outburst);
    return len;
}

// return: delay in seconds between first and last sample in buffer
static float get_delay(ao_data_t* ao){
    priv_t*priv=ao->priv;
  int ierr;
  /* Calculate how many bytes/second is sent out */
  if(priv->delay_method==2){
#ifdef SNDCTL_DSP_GETODELAY
      int r=0;
      ierr=ioctl(priv->fd, SNDCTL_DSP_GETODELAY, &r);
      if(ierr!=-1)
      {
         return ((float)r)/(float)ao->bps;
      }
#endif
      priv->delay_method=1; // fallback if not supported
  }
  if(priv->delay_method==1){
      // SNDCTL_DSP_GETOSPACE
      ierr=ioctl(priv->fd, SNDCTL_DSP_GETOSPACE, &priv->zz);
      if(ierr!=-1)
      {
         return ((float)(ao->buffersize-priv->zz.bytes))/(float)ao->bps;
      }
      priv->delay_method=0; // fallback if not supported
  }
  return ((float)ao->buffersize)/(float)ao->bps;
}
