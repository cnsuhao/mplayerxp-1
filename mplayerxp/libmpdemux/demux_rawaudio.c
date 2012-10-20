#include "../mp_config.h"

#include <stdlib.h>
#include <stdio.h>

#include "stream.h"
#include "demuxer.h"
#include "stheader.h"
#include "../cfgparser.h"
#include "../libmpcodecs/dec_audio.h"
#include "aviprint.h"

int use_rawaudio = 0;
static int channels = 2;
static int samplerate = 44100;
static int samplesize = 2;
static int format = 0x1; // Raw PCM

static const config_t demux_rawaudio_opts[] = {
  { "on", &use_rawaudio, CONF_TYPE_FLAG, 0,0, 1, NULL, "forces treating stream as raw-audio" },
  { "channels", &channels, CONF_TYPE_INT,CONF_RANGE,1,8, NULL, "specifies number of channels in raw-audio steram" },
  { "rate", &samplerate, CONF_TYPE_INT,CONF_RANGE,1000,8*48000, NULL, "specifies sample-rate of raw-audio steram" },
  { "samplesize", &samplesize, CONF_TYPE_INT,CONF_RANGE,1,8, NULL, "specifies sample-size of raw-audio steram" },
  { "format", &format, CONF_TYPE_INT, CONF_MIN, 0 , 0, NULL, "specifies compression-format of raw-audio stream" },
  {NULL, NULL, 0, 0, 0, 0, NULL, NULL}
};

static const config_t rawaudio_conf[] = {
  { "rawaudio", &demux_rawaudio_opts, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL, "Raw-audio specific commands"},
  { NULL,NULL, 0, 0, 0, 0, NULL, NULL}
};

static int rawaudio_probe(demuxer_t* demuxer) 
{
 if(demuxer->stream->type & STREAMTYPE_RAWAUDIO || use_rawaudio)
 {
    demuxer->file_format=DEMUXER_TYPE_RAWAUDIO;
    return 1;
 }
 return 0;
}

static demuxer_t* rawaudio_open(demuxer_t* demuxer) {
  sh_audio_t* sh_audio;
  WAVEFORMATEX* w;

  demuxer->stream->driver->control(demuxer->stream,SCTRL_AUD_GET_CHANNELS,&channels);
  demuxer->stream->driver->control(demuxer->stream,SCTRL_AUD_GET_SAMPLERATE,&samplerate);
  demuxer->stream->driver->control(demuxer->stream,SCTRL_AUD_GET_SAMPLESIZE,&samplesize);
  demuxer->stream->driver->control(demuxer->stream,SCTRL_AUD_GET_FORMAT,&format);
  sh_audio = new_sh_audio(demuxer,0);
  sh_audio->wf = w = (WAVEFORMATEX*)malloc(sizeof(WAVEFORMATEX));
  w->wFormatTag = sh_audio->format = format;
  w->nChannels = sh_audio->channels = channels;
  sh_audio->samplerate = samplerate;
  w->nSamplesPerSec = samplerate;
  w->nAvgBytesPerSec = samplerate*samplesize*channels;
  w->nBlockAlign = channels*samplesize*8;
  sh_audio->samplesize = samplesize;
  w->wBitsPerSample = samplesize*8;
  w->cbSize = 0;
  print_wave_header(w,sizeof(WAVEFORMATEX));
  demuxer->movi_start = demuxer->stream->start_pos;
  demuxer->movi_end = demuxer->stream->end_pos;
  demuxer->movi_length = (demuxer->movi_end-demuxer->movi_start)/w->nAvgBytesPerSec;

  demuxer->audio->sh = sh_audio;
  demuxer->audio->id = 0;
  sh_audio->ds = demuxer->audio;
  if(!(demuxer->stream->type & STREAMTYPE_SEEKABLE)) demuxer->flags &= ~DEMUXF_SEEKABLE;

  return demuxer;
}

static int rawaudio_demux(demuxer_t* demuxer, demux_stream_t *ds) {
  sh_audio_t* sh_audio = demuxer->audio->sh;
  int l = sh_audio->wf->nAvgBytesPerSec;
  off_t spos = stream_tell(demuxer->stream);
  demux_packet_t*  dp;

  if(stream_eof(demuxer->stream)) return 0;

  dp = new_demux_packet(l);
  dp->pts = spos / (float)(sh_audio->wf->nAvgBytesPerSec);
  dp->pos = spos;
  dp->flags=DP_NONKEYFRAME;

  l=stream_read(demuxer->stream,dp->buffer,l);
  resize_demux_packet(dp,l);
  ds_add_packet(ds,dp);

  return 1;
}

static void rawaudio_seek(demuxer_t *demuxer,const seek_args_t* seeka){
  stream_t* s = demuxer->stream;
  sh_audio_t* sh_audio = demuxer->audio->sh;
  off_t base,pos;

  base = (seeka->flags & DEMUX_SEEK_SET) ? demuxer->movi_start : stream_tell(s);
  pos=base+(seeka->flags&DEMUX_SEEK_PERCENTS?demuxer->movi_end-demuxer->movi_start:sh_audio->i_bps)*seeka->secs;
  pos -= (pos % (sh_audio->channels * sh_audio->samplesize) );
  stream_seek(s,pos);
  mpca_resync_stream(sh_audio);
}

static void rawaudio_close(demuxer_t* demuxer) {}

static int rawaudio_control(demuxer_t *demuxer,int cmd,any_t*args)
{
    return DEMUX_UNKNOWN;
}

demuxer_driver_t demux_rawaudio =
{
    "RAW audio parser",
    ".rawaudio",
    rawaudio_conf,
    rawaudio_probe,
    rawaudio_open,
    rawaudio_demux,
    rawaudio_seek,
    rawaudio_close,
    rawaudio_control
};
