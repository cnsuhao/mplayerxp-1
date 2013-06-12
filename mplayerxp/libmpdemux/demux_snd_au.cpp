#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
#include <algorithm>

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#ifdef MP_DEBUG
#include <assert.h>
#endif

#include "libmpstream2/stream.h"
#include "demuxer.h"
#include "demuxer_internal.h"
#include "stheader.h"
#include "libmpcodecs/dec_audio.h"
#include "libao3/afmt.h"
#include "aviprint.h"
#include "osdep/bswap.h"
#include "mp3_hdr.h"
#include "demux_msg.h"

static const int HDR_SIZE=4;

struct snd_au_priv_t : public Opaque {
    public:
	snd_au_priv_t() {}
	virtual ~snd_au_priv_t() {}

	int	frmt;
	float	last_pts,pts_per_packet,length;
	uint32_t dword;
	int	pos;
	/* Xing's VBR specific extensions */
	int	is_xing;
	unsigned nframes;
	unsigned nbytes;
	int	scale;
	unsigned srate;
	int	lsf;
	unsigned char	toc[100]; /* like AVI's indexes */
};

static int snd_au_get_raw_id(Demuxer *demuxer,off_t fptr,unsigned *brate,unsigned *samplerate,unsigned *channels)
{
  uint32_t fcc1;
  Stream *s;
  *brate=*samplerate=*channels=0;
  s = demuxer->stream;
  s->seek(fptr);
  fcc1=s->read(type_dword);
  fcc1=me2be_32(fcc1);
  s->seek(fptr);
  binary_packet bp=s->read(32);
  if(fcc1 == mmioFOURCC('.','s','n','d')) return 1;
  s->seek(fptr);
  return 0;
}

static MPXP_Rc snd_au_probe(Demuxer* demuxer)
{
  uint32_t fcc1,fcc2;
  Stream *s;
  uint8_t *p;
  s = demuxer->stream;
  fcc1=s->read(type_dword);
  fcc1=me2be_32(fcc1);
  p = (uint8_t *)&fcc1;
  if(snd_au_get_raw_id(demuxer,0,&fcc1,&fcc2,&fcc2)) return MPXP_Ok;
  return MPXP_False;
}

static Opaque* snd_au_open(Demuxer* demuxer) {
  Stream *s;
  sh_audio_t* sh_audio;
  uint8_t hdr[HDR_SIZE];
  uint32_t fcc;
  int found = 0, n = 0, pos = 0, step;
  off_t st_pos = 0;
  snd_au_priv_t* priv;
  const unsigned char *pfcc;
#ifdef MP_DEBUG
  assert(demuxer != NULL);
  assert(demuxer->stream != NULL);
#endif

  priv = new(zeromem) snd_au_priv_t;
  s = demuxer->stream;
  s->reset();
  s->seek(s->start_pos());
  while(n < 5 && !s->eof())
  {
    st_pos = s->tell();
    step = 1;

    if(pos < HDR_SIZE) {
      binary_packet bp=s->read(HDR_SIZE-pos); memcpy(&hdr[pos],bp.data(),bp.size());
      pos = HDR_SIZE;
    }

    fcc = le2me_32(*(uint32_t *)hdr);
    pfcc = (const unsigned char *)&fcc;
    MSG_DBG2("AUDIO initial fcc=%c%c%c%c\n",pfcc[0],pfcc[1],pfcc[2],pfcc[3]);
    if( hdr[0] == '.' && hdr[1] == 's' && hdr[2] == 'n' && hdr[3] == 'd' )
    {
	found=1;
	break;
    }
  }
  if(!found)
  {
    MSG_ERR("Can't detect audio format\n");
    return NULL;
  }
  sh_audio = demuxer->new_sh_audio();
  MSG_DBG2("mp3_header off: st_pos=%lu n=%lu HDR_SIZE=%u\n",st_pos,n,HDR_SIZE);
  demuxer->movi_start = s->tell();
  demuxer->movi_end = s->end_pos();

	unsigned hsize,dsize;
	uint32_t id;
	WAVEFORMATEX* w;
	sh_audio->wf = w = new WAVEFORMATEX;
	hsize=s->read(type_dword);
	dsize=s->read(type_dword);
	id = s->read(type_dword);
	sh_audio->afmt=bps2afmt(2);
	if(id == 1) id = WAVE_FORMAT_MULAW;
	else
	if(id == 27) id=WAVE_FORMAT_ALAW;
	else
	if(id == 3) id=0x1;
	w->wFormatTag = sh_audio->wtag = id;
	/* Trickly mplayerxp will threat 'raw ' as big-endian */
	if(id == 0x1) sh_audio->wtag=mmioFOURCC('r','a','w',' ');
	w->nSamplesPerSec = sh_audio->rate = s->read(type_dword);
	w->nChannels = sh_audio->nch = s->read(type_dword);
	w->nAvgBytesPerSec = sh_audio->rate*afmt2bps(sh_audio->afmt)*sh_audio->nch;
	w->nBlockAlign = sh_audio->nch*afmt2bps(sh_audio->afmt);
	w->wBitsPerSample = 8*afmt2bps(sh_audio->afmt);
	w->cbSize = 0;
	demuxer->movi_start = demuxer->stream->start_pos()+hsize;
	demuxer->movi_end = demuxer->movi_start+hsize+dsize;
	demuxer->movi_length = (demuxer->movi_end-demuxer->movi_start)/w->nAvgBytesPerSec;

  priv->last_pts = -1;
  demuxer->priv = priv;
  demuxer->audio->id = 0;
  demuxer->audio->sh = sh_audio;
  sh_audio->ds = demuxer->audio;

  MSG_V("demux_audio: audio data 0x%llX - 0x%llX  \n",demuxer->movi_start,demuxer->movi_end);
  if(s->tell() != demuxer->movi_start)
    s->seek(demuxer->movi_start);
  if(mp_conf.verbose && sh_audio->wf) print_wave_header(sh_audio->wf,sizeof(WAVEFORMATEX));
  if(demuxer->movi_length==UINT_MAX && sh_audio->i_bps)
    demuxer->movi_length=(unsigned)(((float)demuxer->movi_end-(float)demuxer->movi_start)/(float)sh_audio->i_bps);
    check_pin("demuxer",demuxer->pin,DEMUX_PIN);
  return priv;
}

static int snd_au_demux(Demuxer *demuxer,Demuxer_Stream *ds) {
  sh_audio_t* sh_audio;
  Demuxer* demux;
  snd_au_priv_t* priv;
  Stream* s;
  int seof;
#ifdef MP_DEBUG
  assert(ds != NULL);
  assert(ds->sh != NULL);
  assert(ds->demuxer != NULL);
#endif
  sh_audio = reinterpret_cast<sh_audio_t*>(demuxer->audio->sh);
  demux = demuxer;
  priv = static_cast<snd_au_priv_t*>(demux->priv);
  s = demux->stream;

  seof=s->eof();
  if(seof || (demux->movi_end && s->tell() >= demux->movi_end)) {
    MSG_DBG2("snd_au_demux: EOF due: %s\n",
	    seof?"s->eof()":"s->tell() >= demux->movi_end");
    if(!seof) {
	MSG_DBG2("snd_au_demux: stream_pos=%llu movi_end=%llu\n",
		s->tell(),
		demux->movi_end);
    }
    return 0;
  }
    int l = sh_audio->wf->nAvgBytesPerSec;
    Demuxer_Packet* dp =new(zeromem) Demuxer_Packet(l);
    binary_packet bp=s->read(l); memcpy(dp->buffer(),bp.data(),bp.size());
    l=bp.size();
    dp->resize(l);
    priv->last_pts = priv->last_pts < 0 ? 0 : priv->last_pts + l/(float)sh_audio->i_bps;
    dp->pts = priv->last_pts - (demux->audio->tell_pts()-sh_audio->a_in_buffer_len)/(float)sh_audio->i_bps;
    dp->flags=DP_NONKEYFRAME;
    ds->add_packet(dp);
    return 1;
}

static void snd_au_seek(Demuxer *demuxer,const seek_args_t* seeka){
  sh_audio_t* sh_audio;
  Stream* s;
  int base,pos;
  snd_au_priv_t* priv;

  if(!(sh_audio = reinterpret_cast<sh_audio_t*>(demuxer->audio->sh))) return;
  s = demuxer->stream;
  priv = static_cast<snd_au_priv_t*>(demuxer->priv);

  base = seeka->flags&DEMUX_SEEK_SET ? demuxer->movi_start : s->tell();
  pos=base+(seeka->flags&DEMUX_SEEK_PERCENTS?(demuxer->movi_end - demuxer->movi_start):sh_audio->i_bps)*seeka->secs;

  if(demuxer->movi_end && pos >= demuxer->movi_end) {
    sh_audio->timer = (s->tell() - demuxer->movi_start)/(float)sh_audio->i_bps;
    return;
  } else if(pos < demuxer->movi_start)
    pos = demuxer->movi_start;

  priv->last_pts = (pos-demuxer->movi_start)/(float)sh_audio->i_bps;
  sh_audio->timer = priv->last_pts - (demuxer->audio->tell_pts()-sh_audio->a_in_buffer_len)/(float)sh_audio->i_bps;

    pos -= (pos % (sh_audio->nch * afmt2bps(sh_audio->afmt)));
    // We need to decrease the pts by one step to make it the "last one"
    priv->last_pts -= sh_audio->wf->nAvgBytesPerSec/(float)sh_audio->i_bps;

  s->seek(pos);
}

static void snd_au_close(Demuxer* demuxer) {
    snd_au_priv_t* priv = static_cast<snd_au_priv_t*>(demuxer->priv);

    if(!priv) return;
    delete priv;
}

static MPXP_Rc snd_au_control(const Demuxer *demuxer,int cmd,any_t*args)
{
    UNUSED(demuxer);
    UNUSED(cmd);
    UNUSED(args);
    return MPXP_Unknown;
}

/****************** Options stuff ******************/

#include "libmpconf/cfgparser.h"

static const mpxp_option_t snd_au_opts[] = {
  {NULL, NULL, 0, 0, 0, 0, NULL}
};

extern const demuxer_driver_t demux_snd_au =
{
    "sndau",
    "SDN-AU parser",
    ".snd",
    snd_au_opts,
    snd_au_probe,
    snd_au_open,
    snd_au_demux,
    snd_au_seek,
    snd_au_close,
    snd_au_control
};
