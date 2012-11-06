#include <stdio.h>
#include <stdlib.h>
#define __USE_XOPEN 1
#include <unistd.h>
#include <assert.h>
#include <dlfcn.h> /* GLIBC specific. Exists under cygwin too! */
#include "ad_internal.h"
#include "codecs_ld.h"

#include "mp_config.h"
#include "mplayer.h"
#include "help_mp.h"
#include "osdep/cpudetect.h"

#include "osdep/mm_accel.h"
#include "mplayer.h"
#include "liba52/a52.h"
#include "libao2/afmt.h"
#include "libao2/audio_out.h"
#include "postproc/af.h"

typedef struct priv_s {
    float last_pts;
}priv_t;

#define MAX_AC3_FRAME 3840

a52_state_t* mpxp_a52_state;
uint32_t mpxp_a52_accel=0;
uint32_t mpxp_a52_flags=0;

#include "osdep/bswap.h"
#include "osdep/mplib.h"

static const ad_info_t info = 
{
	"liba52 AC3 driver",
	"liba52",
	"Nickols_K",
	"build-in"
};

static const config_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

LIBAD_EXTERN(a52)
int a52_fillbuff(sh_audio_t *sh_audio,float *pts){
int length=0;
int flags=0;
int sample_rate=0;
int bit_rate=0;
float apts=0.,null_pts;
priv_t *priv=sh_audio->context;

    sh_audio->a_in_buffer_len=0;
    /* sync frame:*/
while(1){
    while(sh_audio->a_in_buffer_len<8){
	int c=demux_getc_r(sh_audio->ds,apts?&null_pts:&apts);
	if(c<0) { priv->last_pts=*pts=apts; return -1; } /* EOF*/
        sh_audio->a_in_buffer[sh_audio->a_in_buffer_len++]=c;
    }
    if(sh_audio->wtag!=0x2000) swab(sh_audio->a_in_buffer,sh_audio->a_in_buffer,8);
    length = a52_syncinfo (sh_audio->a_in_buffer, &flags, &sample_rate, &bit_rate);
    if(length>=7 && length<=MAX_AC3_FRAME) break; /* we're done.*/
    /* bad file => resync*/
    if(sh_audio->wtag!=0x2000) swab(sh_audio->a_in_buffer,sh_audio->a_in_buffer,8);
    memmove(sh_audio->a_in_buffer,sh_audio->a_in_buffer+1,7);
    --sh_audio->a_in_buffer_len;
    apts=0;
}
    MSG_DBG2("a52: len=%d  flags=0x%X  %d Hz %d bit/s\n",length,flags,sample_rate,bit_rate);
    sh_audio->samplerate=sample_rate;
    sh_audio->i_bps=bit_rate/8;
    demux_read_data_r(sh_audio->ds,sh_audio->a_in_buffer+8,length-8,apts?&null_pts:&apts);
    if(sh_audio->wtag!=0x2000) swab(sh_audio->a_in_buffer+8,sh_audio->a_in_buffer+8,length-8);
    priv->last_pts=*pts=apts;
    if(crc16_block(sh_audio->a_in_buffer+2,length-2)!=0)
	MSG_STATUS("a52: CRC check failed!  \n");
    return length;
}

/* returns: number of available channels*/
static int a52_printinfo(sh_audio_t *sh_audio){
int flags, sample_rate, bit_rate;
char* mode="unknown";
int channels=0;
  a52_syncinfo (sh_audio->a_in_buffer, &flags, &sample_rate, &bit_rate);
  switch(flags&A52_CHANNEL_MASK){
    case A52_CHANNEL: mode="channel"; channels=2; break;
    case A52_MONO: mode="mono"; channels=1; break;
    case A52_STEREO: mode="stereo"; channels=2; break;
    case A52_3F: mode="3f";channels=3;break;
    case A52_2F1R: mode="2f+1r";channels=3;break;
    case A52_3F1R: mode="3f+1r";channels=4;break;
    case A52_2F2R: mode="2f+2r";channels=4;break;
    case A52_3F2R: mode="3f+2r";channels=5;break;
    case A52_CHANNEL1: mode="channel1"; channels=2; break;
    case A52_CHANNEL2: mode="channel2"; channels=2; break;
    case A52_DOLBY: mode="dolby"; channels=2; break;
  }
  MSG_INFO("AC3: %d.%d (%s%s)  %d Hz  %3.1f kbit/s Out: %u-bit\n",
	channels, (flags&A52_LFE)?1:0,
	mode, (flags&A52_LFE)?"+lfe":"",
	sample_rate, bit_rate*0.001f,
	sh_audio->samplesize*8);
  return (flags&A52_LFE) ? (channels+1) : channels;
}


int preinit(sh_audio_t *sh)
{
  /* Dolby AC3 audio: */
  /* however many channels, 2 bytes in a word, 256 samples in a block, 6 blocks in a frame */
#ifdef WORDS_BIGENDIAN
#define A52_FMT32 AFMT_S32_BE
#define A52_FMT24 AFMT_S24_BE
#else
#define A52_FMT32 AFMT_S32_LE
#define A52_FMT24 AFMT_S24_LE
#endif
  sh->samplesize=2;
  if(af_query_fmt(sh->afilter,AFMT_FLOAT32) == CONTROL_OK||
     af_query_fmt(sh->afilter,A52_FMT32) == CONTROL_OK ||
     af_query_fmt(sh->afilter,A52_FMT24) == CONTROL_OK)
  {
    sh->samplesize=4;
    sh->sample_format=AFMT_FLOAT32;
  }
  sh->audio_out_minsize=mp_conf.ao_channels*sh->samplesize*256*6;
  sh->audio_in_minsize=MAX_AC3_FRAME;
  sh->context=mp_malloc(sizeof(priv_t));
  return 1;
}

int init(sh_audio_t *sh_audio)
{
  sample_t level=1, bias=384;
  float pts;
  int flags=0;
  /* Dolby AC3 audio:*/
  mpxp_a52_accel = mp_data->mplayer_accel;
  mpxp_a52_state=a52_init (mpxp_a52_accel);
  if (mpxp_a52_state == NULL) {
	MSG_ERR("A52 init failed\n");
	return 0;
  }
  if(a52_fillbuff(sh_audio,&pts)<0){
	MSG_ERR("A52 sync failed\n");
	return 0;
  }
  /* 'a52 cannot upmix' hotfix:*/
  a52_printinfo(sh_audio);
  sh_audio->channels=mp_conf.ao_channels;
while(sh_audio->channels>0){
  switch(sh_audio->channels){
	    case 1: mpxp_a52_flags=A52_MONO; break;
/*	    case 2: mpxp_a52_flags=A52_STEREO; break; */
	    case 2: mpxp_a52_flags=A52_DOLBY; break;
/*	    case 3: mpxp_a52_flags=A52_3F; break;*/
	    case 3: mpxp_a52_flags=A52_2F1R; break;
	    case 4: mpxp_a52_flags=A52_2F2R; break; /* 2+2*/
	    case 5: mpxp_a52_flags=A52_3F2R; break;
	    case 6: mpxp_a52_flags=A52_3F2R|A52_LFE; break; /* 5.1*/
  }
  /* test:*/
  flags=mpxp_a52_flags|A52_ADJUST_LEVEL;
  MSG_V("A52 flags before a52_frame: 0x%X\n",flags);
  if (a52_frame (mpxp_a52_state, sh_audio->a_in_buffer, &flags, &level, bias)){
    MSG_ERR("a52: error decoding frame -> nosound\n");
    return 0;
  }
  MSG_V("A52 flags after a52_frame: 0x%X\n",flags);
  /* frame decoded, let's init resampler:*/
  if(sh_audio->samplesize==4)
  {
    if(a52_resample_init_float(mpxp_a52_state,mpxp_a52_accel,flags,sh_audio->channels)) break;
  }
  else
  {
    if(a52_resample_init(mpxp_a52_state,mpxp_a52_accel,flags,sh_audio->channels)) break;
  }
  --sh_audio->channels; /* try to decrease no. of channels*/
}
  if(sh_audio->channels<=0){
    MSG_ERR("a52: no resampler. try different channel setup!\n");
    return 0;
  }
  return 1;
}

void uninit(sh_audio_t *sh)
{
  mp_free(sh->context);
}

ControlCodes control(sh_audio_t *sh,int cmd,any_t* arg, ...)
{
    UNUSED(arg);
    switch(cmd)
    {
      case ADCTRL_RESYNC_STREAM:
          sh->a_in_buffer_len=0;   // reset ACM/DShow audio buffer
	  return CONTROL_TRUE;
      case ADCTRL_SKIP_FRAME:
	{
	  float pts;
	  a52_fillbuff(sh,&pts); // skip AC3 frame
	  return CONTROL_TRUE;
	}
      default:
	  return CONTROL_UNKNOWN;
    }
  return CONTROL_UNKNOWN;
}

unsigned decode(sh_audio_t *sh_audio,unsigned char *buf,unsigned minlen,unsigned maxlen,float *pts)
{
    sample_t level=1, bias=384;
    int flags=mpxp_a52_flags|A52_ADJUST_LEVEL;
    unsigned i;
    unsigned len=0;
    UNUSED(minlen);
    UNUSED(maxlen);
    priv_t *priv=sh_audio->context;
	if(!sh_audio->a_in_buffer_len)
	{
	    if(a52_fillbuff(sh_audio,pts)<0) return len; /* EOF */
	}
	else *pts=priv->last_pts;
	sh_audio->a_in_buffer_len=0;
	if (a52_frame (mpxp_a52_state, sh_audio->a_in_buffer, &flags, &level, bias)){
	    MSG_WARN("a52: error decoding frame\n");
	    return len;
	}
//	a52_dynrng(&mpxp_a52_state, NULL, NULL);
	len=0;
	for (i = 0; i < 6; i++) {
	    if (a52_block (mpxp_a52_state)){
		MSG_WARN("a52: error at resampling\n");
		break;
	    }
	    if(sh_audio->samplesize==4)
		len+=4*a52_resample32(a52_samples(mpxp_a52_state),(float *)&buf[len]);
	    else
		len+=2*a52_resample(a52_samples(mpxp_a52_state),(int16_t *)&buf[len]);
	}
  return len;
}
