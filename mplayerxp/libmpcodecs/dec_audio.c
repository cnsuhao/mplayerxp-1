#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "help_mp.h"

#include "../mplayer.h"

#include "stream.h"
#include "demuxer.h"

#include "codec-cfg.h"
#include "stheader.h"

#include "dec_audio.h"
#include "ad.h"
#include "../libao2/afmt.h"
#include "../libao2/audio_out.h"
#include "../mplayer.h"
#include "../libmpdemux/demuxer_r.h"
#include "../postproc/af.h"
#define MSGT_CLASS MSGT_DECAUDIO
#include "../__mp_msg.h"
#include "../libvo/fastmemcpy.h"

#ifdef USE_FAKE_MONO
int fakemono=0;
#endif
/* used for ac3surround decoder - set using -channels option */
int audio_output_channels = 2;
af_cfg_t af_cfg; // Configuration for audio filters

static const ad_functions_t* mpadec;

static sh_audio_t *dec_audio_sh;

extern int force_srate;
extern char *audio_codec;
int init_audio(sh_audio_t *sh_audio)
{
  unsigned i;
  dec_audio_sh = sh_audio;
  for (i=0; mpcodecs_ad_drivers[i] != NULL; i++)
    if(strcmp(mpcodecs_ad_drivers[i]->info->driver_name,sh_audio->codec->driver_name)==0){
	mpadec=mpcodecs_ad_drivers[i]; break;
    }
  if(!mpadec){
      MSG_ERR(MSGTR_CODEC_BAD_AFAMILY,sh_audio->codec->codec_name, sh_audio->codec->driver_name);
      return 0; // no such driver
  }
  
  /* reset in/out buffer size/pointer: */
  sh_audio->a_buffer_size=0;
  sh_audio->a_buffer=NULL;
  sh_audio->a_in_buffer_size=0;
  sh_audio->a_in_buffer=NULL;
  /* Set up some common usefull defaults. ad->preinit() can override these: */
  
  sh_audio->samplesize=2;
#ifdef WORDS_BIGENDIAN
  sh_audio->sample_format=AFMT_S16_BE;
#else
  sh_audio->sample_format=AFMT_S16_LE;
#endif
  sh_audio->samplerate=0;
  sh_audio->o_bps=0;
  if(sh_audio->wf) /* NK: We need to know i_bps before its detection by codecs param */
    sh_audio->i_bps=sh_audio->wf->nAvgBytesPerSec; 

  sh_audio->audio_out_minsize=8192;/* default size, maybe not enough for Win32/ACM*/
  sh_audio->audio_in_minsize=0;
  
  if(!mpadec->preinit(sh_audio))
  {
      MSG_ERR(MSGTR_CODEC_CANT_PREINITA);
      return 0;
  }

/* allocate audio in buffer: */
  if(sh_audio->audio_in_minsize>0){
      sh_audio->a_in_buffer_size=sh_audio->audio_in_minsize;
      MSG_V("dec_audio: Allocating %d bytes for input buffer\n",
          sh_audio->a_in_buffer_size);
      sh_audio->a_in_buffer=malloc(sh_audio->a_in_buffer_size);
      memset(sh_audio->a_in_buffer,0,sh_audio->a_in_buffer_size);
      sh_audio->a_in_buffer_len=0;
  }

/* allocate audio out buffer: */
  sh_audio->a_buffer_size=sh_audio->audio_out_minsize+MAX_OUTBURST; /* worst case calc.*/

  MSG_V("dec_audio: Allocating %d + %d = %d bytes for output buffer\n",
      sh_audio->audio_out_minsize,MAX_OUTBURST,sh_audio->a_buffer_size);

  sh_audio->a_buffer=malloc(sh_audio->a_buffer_size);
  if(!sh_audio->a_buffer){
      MSG_ERR(MSGTR_CantAllocAudioBuf);
      return 0;
  }
  memset(sh_audio->a_buffer,0,sh_audio->a_buffer_size);
  sh_audio->a_buffer_len=0;

  if(!mpadec->init(sh_audio)){
      MSG_WARN(MSGTR_CODEC_CANT_INITA);
      uninit_audio(sh_audio); /* free buffers */
      return 0;
  }

  sh_audio->inited=1;
  
  if(!sh_audio->channels || !sh_audio->samplerate){
    MSG_WARN(MSGTR_UnknownAudio);
    uninit_audio(sh_audio); /* free buffers */
    return 0;
  }

  if(!sh_audio->o_bps)
    sh_audio->o_bps=sh_audio->channels*sh_audio->samplerate*sh_audio->samplesize;
  if(!sh_audio->i_bps)
  {
	static int warned=0;
	if(!warned)
	{
	    warned=1;
	    MSG_WARN(MSGTR_CODEC_INITAL_AV_RESYNC);
	}
  }
  else
  if(initial_audio_pts_corr.need_correction==1)
  {
    initial_audio_pts += ((float)(initial_audio_pts_corr.pts_bytes-initial_audio_pts_corr.nbytes))/(float)sh_audio->i_bps;
    initial_audio_pts_corr.need_correction=0;
  }
  MSG_OK("[AC] %s decoder: [%s] drv:%s.%s ratio %i->%i\n",audio_codec?"Forcing":"Selecting"
  ,sh_audio->codec->codec_name
  ,mpadec->info->driver_name
  ,sh_audio->codec->dll_name
  ,sh_audio->i_bps,sh_audio->o_bps);
  return 1;
}

void uninit_audio(sh_audio_t *sh_audio)
{
    if(sh_audio->afilter){
	MSG_V("Uninit audio filters...\n");
	af_uninit(sh_audio->afilter);
	free(sh_audio->afilter);
	sh_audio->afilter=NULL;
    }
    if(sh_audio->a_buffer) free(sh_audio->a_buffer);
    sh_audio->a_buffer=NULL;
    if(sh_audio->a_in_buffer) free(sh_audio->a_in_buffer);
    sh_audio->a_in_buffer=NULL;
    if(!sh_audio->inited) return;
    MSG_V("uninit audio: %d  \n",sh_audio->codec->driver_name);
    mpadec->uninit(sh_audio);
    if(sh_audio->a_buffer) free(sh_audio->a_buffer);
    sh_audio->a_buffer=NULL;
    sh_audio->inited=0;
}

 /* Init audio filters */
int preinit_audio_filters(sh_audio_t *sh_audio, 
	int in_samplerate, int in_channels, int in_format, int in_bps,
	int* out_samplerate, int* out_channels, int* out_format, int out_bps){
  char strbuf[200];
  af_stream_t* afs=af_new(sh_audio);

  // input format: same as codec's output format:
  afs->input.rate   = in_samplerate;
  afs->input.nch    = in_channels;
  afs->input.format = af_format_decode(in_format,&afs->input.bps);

  // output format: same as ao driver's input format (if missing, fallback to input)
  afs->output.rate   = *out_samplerate ? *out_samplerate : afs->input.rate;
  afs->output.nch    = *out_channels ? *out_channels : afs->input.nch;
  if(*out_format) afs->output.format = af_format_decode(*out_format,&afs->output.bps);
  else
  {
    afs->output.format = afs->input.format;
    afs->output.bps    = out_bps ? out_bps : afs->input.bps;
  }
  // filter config:  
  memcpy(&afs->cfg,&af_cfg,sizeof(af_cfg_t));
  
  MSG_V("Checking audio filter chain for %dHz/%dch/%dbit...\n",
      afs->input.rate,afs->input.nch,afs->input.bps*8);
  
  // let's autoprobe it!
  if(0 != af_init(afs,0)){
    free(afs);
    return 0; // failed :(
  }
  
  *out_samplerate=afs->output.rate;
  *out_channels=afs->output.nch;
  *out_format=af_format_encode((void*)(&afs->output));

  sh_audio->f_bps = afs->output.rate*afs->output.nch*afs->output.bps;
  
  MSG_V("AF_pre: af format: %d bps, %d ch, %d hz, %s\n",
      afs->output.bps, afs->output.nch, afs->output.rate,
      fmt2str(afs->output.format,afs->output.bps,strbuf,200));
  
  sh_audio->afilter=(void*)afs;
  return 1;
}

 /* Init audio filters */
int init_audio_filters(sh_audio_t *sh_audio, 
	int in_samplerate, int in_channels, int in_format, int in_bps,
	int out_samplerate, int out_channels, int out_format, int out_bps,
	int out_minsize, int out_maxsize){
  af_stream_t* afs=sh_audio->afilter;
  if(!afs) afs = af_new(sh_audio);

  // input format: same as codec's output format:
  afs->input.rate   = in_samplerate;
  afs->input.nch    = in_channels;
  afs->input.format = af_format_decode(in_format,&afs->input.bps);

  // output format: same as ao driver's input format (if missing, fallback to input)
  afs->output.rate   = out_samplerate ? out_samplerate : afs->input.rate;
  afs->output.nch    = out_channels ? out_channels : afs->input.nch;
  afs->output.format = af_format_decode(out_format ? out_format : afs->input.format,&afs->output.bps);

  // filter config:  
  memcpy(&afs->cfg,&af_cfg,sizeof(af_cfg_t));
  
  MSG_V("Building audio filter chain for %dHz/%dch/%dbit (%s) -> %dHz/%dch/%dbit (%s)...\n",
      afs->input.rate,afs->input.nch,afs->input.bps*8,ao_format_name(af_format_encode(&afs->input)),
      afs->output.rate,afs->output.nch,afs->output.bps*8,ao_format_name(af_format_encode(&afs->output)));
  
  // let's autoprobe it!
  if(0 != af_init(afs,1)){
    sh_audio->afilter=NULL;
    free(afs);
    return 0; // failed :(
  }
  
  // allocate the a_out_* buffers:
  if(out_maxsize<out_minsize) out_maxsize=out_minsize;
  if(out_maxsize<8192) out_maxsize=MAX_OUTBURST; // not sure this is ok

  sh_audio->f_bps = afs->output.rate*afs->output.nch*afs->output.bps;

  sh_audio->a_buffer_size=out_maxsize;
  sh_audio->a_buffer=malloc(sh_audio->a_buffer_size);
  memset(sh_audio->a_buffer,0,sh_audio->a_buffer_size);
  sh_audio->a_buffer_len=0;

  af_showconf(afs->first);
  sh_audio->afilter=(void*)afs;
  sh_audio->afilter_inited=1;
  return 1;
}

 /* Init audio filters */
int reinit_audio_filters(sh_audio_t *sh_audio, 
	int in_samplerate, int in_channels, int in_format, int in_bps,
	int out_samplerate, int out_channels, int out_format, int out_bps,
	int out_minsize, int out_maxsize)
{
    if(sh_audio->afilter){
	MSG_V("Uninit audio filters...\n");
	af_uninit(sh_audio->afilter);
	free(sh_audio->afilter);
	sh_audio->afilter=NULL;
    }
    return init_audio_filters(sh_audio,in_samplerate,in_channels,
				in_format,in_bps,out_samplerate,
				out_channels,out_format,out_bps,
				out_minsize,out_maxsize);
}

int decode_audio(sh_audio_t *sh_audio,unsigned char *buf,int minlen,int maxlen,int buflen,float *pts)
{
  int len;
  unsigned cp_size,cp_tile;
  af_data_t  afd;  // filter input
  af_data_t* pafd; // filter output

  if(!sh_audio->inited) return -1; // no codec
  MSG_DBG3("decode_audio(%p,%p,%i,%i,%i,%p)\n",sh_audio,buf,minlen,maxlen,buflen,pts);
  
  if(minlen>maxlen) MSG_WARN(MSGTR_CODEC_XP_INT_ERR,minlen,maxlen);
  if(sh_audio->f_buffer_len)
  {
    cp_size=min(buflen,sh_audio->f_buffer_len);
    memcpy(buf,sh_audio->f_buffer,cp_size);
    *pts = sh_audio->f_pts;
    cp_tile=sh_audio->f_buffer_len-cp_size;
    MSG_DBG2("cache->buf %i bytes %f pts <PREDICTED PTS %f>\n",cp_size,*pts,*pts+(float)cp_tile/(float)sh_audio->f_bps);
    if(cp_tile)
    {
	sh_audio->f_buffer=&sh_audio->f_buffer[cp_size];
	sh_audio->f_buffer_len=cp_tile;
	sh_audio->f_pts += (float)cp_size/(float)sh_audio->f_bps;
    }
    else sh_audio->f_buffer_len=0;
    return cp_size;
  }
  if(sh_audio->f_bps>sh_audio->o_bps)
      maxlen=min(maxlen,(long long int)buflen*sh_audio->o_bps/sh_audio->f_bps);
  len=mpadec->decode_audio(sh_audio,buf, minlen, maxlen,pts);
  if(len>buflen) MSG_WARN(MSGTR_CODEC_BUF_OVERFLOW,sh_audio->codec->driver_name,len,buflen);
  MSG_DBG2("decaudio: %i bytes %f pts min %i max %i buflen %i o_bps=%i f_bps=%i\n",len,*pts,minlen,maxlen,buflen,sh_audio->o_bps,sh_audio->f_bps);
  if(len<=0 || !sh_audio->afilter) return len; // EOF?
  // run the filters:
  afd.audio=buf;
  afd.len=len;
  afd.rate=sh_audio->samplerate;
  afd.nch=sh_audio->channels;
  afd.format=af_format_decode(sh_audio->sample_format,&afd.bps);
  pafd=af_play(sh_audio->afilter,&afd);

  if(!pafd) {
      MSG_V("decaudio: filter error\n");
      return -1; // error
  }

  MSG_DBG2("decaudio: %X in=%d out=%d (min %d max %d buf %d)\n",
      pafd->format,len, pafd->len, minlen, maxlen, buflen);

  cp_size=pafd->len;
  if(buf != pafd->audio)
  {
    cp_size=min(buflen,pafd->len);
    memcpy(buf,pafd->audio,cp_size);
    cp_tile=pafd->len-cp_size;
    if(cp_tile)
    {
	sh_audio->f_buffer=&((char *)pafd->audio)[cp_size];
	sh_audio->f_buffer_len=cp_tile;
	sh_audio->f_pts = *pts+(float)cp_size/(float)sh_audio->f_bps;
	MSG_DBG2("decaudio: afilter->cache %i bytes %f pts\n",cp_tile,*pts);
    }
    else sh_audio->f_buffer_len=0;
  }
  return cp_size;
}

void resync_audio_stream(sh_audio_t *sh_audio)
{
  if(sh_audio)
  if(sh_audio->inited && mpadec) mpadec->control(sh_audio,ADCTRL_RESYNC_STREAM,NULL);
}

void skip_audio_frame(sh_audio_t *sh_audio)
{
  if(sh_audio)
  if(sh_audio->inited && mpadec) mpadec->control(sh_audio,ADCTRL_SKIP_FRAME,NULL);
}

/* MP3 decoder buffer callback:*/
int mplayer_audio_read(char *buf,int size,float *pts){
  int len;
  len=demux_read_data_r(dec_audio_sh->ds,buf,size,pts);
  MSG_DBG2("%i=mplayer_audio_read(%p,%i,%f)\n",len,buf,size,*pts);
  return len;
}
