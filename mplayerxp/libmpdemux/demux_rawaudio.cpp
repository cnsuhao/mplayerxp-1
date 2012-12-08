#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
#include <stdlib.h>
#include <stdio.h>

#include "mplayerxp.h"
#include "libmpstream/stream.h"
#include "demuxer.h"
#include "demuxer_internal.h"
#include "stheader.h"
#include "libmpconf/cfgparser.h"
#include "libmpcodecs/dec_audio.h"
#include "libao2/afmt.h"
#include "aviprint.h"

int use_rawaudio = 0;
static int channels = 2;
static int samplerate = 44100;
static int afmt = AFMT_S16_LE;
static int wtag = 0x1; // Raw PCM

static const config_t demux_rawaudio_opts[] = {
  { "on", &use_rawaudio, CONF_TYPE_FLAG, 0,0, 1, "forces treating stream as raw-audio" },
  { "channels", &channels, CONF_TYPE_INT,CONF_RANGE,1,8, "specifies number of channels in raw-audio steram" },
  { "rate", &samplerate, CONF_TYPE_INT,CONF_RANGE,1000,8*48000, "specifies sample-rate of raw-audio steram" },
  { "afmt", &afmt, CONF_TYPE_INT,CONF_RANGE,1,8, "specifies sample-format of raw-audio steram" },
  { "wtag", &wtag, CONF_TYPE_INT, CONF_MIN, 0 , 0, "specifies compression-wtag of raw-audio stream" },
  {NULL, NULL, 0, 0, 0, 0, NULL}
};

static const config_t rawaudio_conf[] = {
  { "rawaudio", (any_t*)&demux_rawaudio_opts, CONF_TYPE_SUBCONFIG, 0, 0, 0, "Raw-audio specific commands"},
  { NULL,NULL, 0, 0, 0, 0, NULL}
};

static MPXP_Rc rawaudio_probe(Demuxer* demuxer)
{
    if(demuxer->stream->type() & Stream::Type_RawAudio || use_rawaudio) {
	demuxer->file_format=Demuxer::Type_RAWAUDIO;
	return MPXP_Ok;
    }
    return MPXP_False;
}

static Opaque* rawaudio_open(Demuxer* demuxer) {
  unsigned samplesize;
  sh_audio_t* sh_audio;
  WAVEFORMATEX* w;

  demuxer->stream->ctrl(SCTRL_AUD_GET_CHANNELS,&channels);
  demuxer->stream->ctrl(SCTRL_AUD_GET_SAMPLERATE,&samplerate);
  demuxer->stream->ctrl(SCTRL_AUD_GET_SAMPLESIZE,&samplesize);
  demuxer->stream->ctrl(SCTRL_AUD_GET_FORMAT,&wtag);
  sh_audio = demuxer->new_sh_audio();
  sh_audio->wf = w = (WAVEFORMATEX*)mp_malloc(sizeof(WAVEFORMATEX));
  w->wFormatTag = sh_audio->wtag = wtag;
  w->nChannels = sh_audio->nch = channels;
  sh_audio->rate = samplerate;
  w->nSamplesPerSec = samplerate;
  w->nAvgBytesPerSec = samplerate*samplesize*channels;
  w->nBlockAlign = channels*samplesize*8;
  sh_audio->afmt = bps2afmt(samplesize);
  w->wBitsPerSample = samplesize*8;
  w->cbSize = 0;
  print_wave_header(w,sizeof(WAVEFORMATEX));
  demuxer->movi_start = demuxer->stream->start_pos();
  demuxer->movi_end = demuxer->stream->end_pos();
  demuxer->movi_length = (demuxer->movi_end-demuxer->movi_start)/w->nAvgBytesPerSec;

  demuxer->audio->sh = sh_audio;
  demuxer->audio->id = 0;
  sh_audio->ds = demuxer->audio;
  if(!(demuxer->stream->type() & Stream::Type_Seekable)) demuxer->flags &= ~Demuxer::Seekable;
    check_pin("demuxer",demuxer->pin,DEMUX_PIN);
    return demuxer;
}

static int rawaudio_demux(Demuxer* demuxer, Demuxer_Stream *ds) {
  sh_audio_t* sh_audio = reinterpret_cast<sh_audio_t*>(demuxer->audio->sh);
  int l = sh_audio->wf->nAvgBytesPerSec;
  off_t spos = demuxer->stream->tell();

  if(demuxer->stream->eof()) return 0;

  Demuxer_Packet* dp = new(zeromem) Demuxer_Packet(l);
  dp->pts = spos / (float)(sh_audio->wf->nAvgBytesPerSec);
  dp->pos = spos;
  dp->flags=DP_NONKEYFRAME;

  l=demuxer->stream->read(dp->buffer(),l);
  dp->resize(l);
  ds->add_packet(dp);

  return 1;
}

static void rawaudio_seek(Demuxer *demuxer,const seek_args_t* seeka){
  Stream* s = demuxer->stream;
  sh_audio_t* sh_audio = reinterpret_cast<sh_audio_t*>(demuxer->audio->sh);
  off_t base,pos;

  base = (seeka->flags & DEMUX_SEEK_SET) ? demuxer->movi_start : s->tell();
  pos=base+(seeka->flags&DEMUX_SEEK_PERCENTS?demuxer->movi_end-demuxer->movi_start:sh_audio->i_bps)*seeka->secs;
  pos -= (pos % (sh_audio->nch * afmt2bps(sh_audio->afmt)));
  s->seek(pos);
}

static void rawaudio_close(Demuxer* demuxer) { UNUSED(demuxer); }

static MPXP_Rc rawaudio_control(const Demuxer *demuxer,int cmd,any_t*args)
{
    UNUSED(demuxer);
    UNUSED(cmd);
    UNUSED(args);
    return MPXP_Unknown;
}

extern const demuxer_driver_t demux_rawaudio =
{
    "rawaudio",
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
