#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
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
#include "libao2/afmt.h"
#include "aviprint.h"
#include "osdep/bswap.h"
#include "mp3_hdr.h"
#include "demux_msg.h"

#define HDR_SIZE 4

struct musepack_priv_t : public Opaque {
    public:
	musepack_priv_t() {}
	virtual ~musepack_priv_t() {}

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

static int musepack_get_raw_id(Demuxer *demuxer,off_t fptr,unsigned *brate,unsigned *samplerate,unsigned *channels)
{
  int retval=0;
  uint32_t fcc,fcc1,fmt;
  uint8_t *p,b[32];
  Stream *s;
  *brate=*samplerate=*channels=0;
  s = demuxer->stream;
  s->seek(fptr);
  fcc=fcc1=s->read_dword();
  fcc1=me2be_32(fcc1);
  p = (uint8_t *)&fcc1;
  s->seek(fptr);
  s->read(b,sizeof(b));
  if(p[0] == 'M' && p[1] == 'P' && p[2] == '+' && (p[3] >= 4 && p[3] <= 0x20)) return 1;
  s->seek(fptr);
  return 0;
}

static MPXP_Rc musepack_probe(Demuxer* demuxer)
{
  uint32_t fcc1,fcc2;
  Stream *s;
  uint8_t *p;
  s = demuxer->stream;
  fcc1=s->read_dword();
  fcc1=me2be_32(fcc1);
  p = (uint8_t *)&fcc1;
  if(musepack_get_raw_id(demuxer,0,&fcc1,&fcc2,&fcc2)) return MPXP_Ok;
  return MPXP_False;
}

static Opaque* musepack_open(Demuxer* demuxer) {
  Stream *s;
  sh_audio_t* sh_audio;
  uint8_t hdr[HDR_SIZE];
  uint32_t fcc;
  int found = 0, n = 0, pos = 0, step;
  off_t st_pos = 0;
  musepack_priv_t* priv;
  const unsigned char *pfcc;
#ifdef MP_DEBUG
  assert(demuxer != NULL);
  assert(demuxer->stream != NULL);
#endif

  priv = new(zeromem) musepack_priv_t;
  s = demuxer->stream;
  s->reset();
  s->seek(s->start_pos());
  while(n < 5 && !s->eof())
  {
    st_pos = s->tell();
    step = 1;

    if(pos < HDR_SIZE) {
      s->read(&hdr[pos],HDR_SIZE-pos);
      pos = HDR_SIZE;
    }

    fcc = le2me_32(*(uint32_t *)hdr);
    pfcc = (const unsigned char *)&fcc;
    MSG_DBG2("AUDIO initial fcc=%c%c%c%c\n",pfcc[0],pfcc[1],pfcc[2],pfcc[3]);
    if( hdr[0] == 'M' && hdr[1] == 'P' && hdr[2] == '+' && (hdr[3] >= 4 && hdr[3] <= 0x20))
    {
	found=1;
	break;
    }
    /* Add here some other audio format detection */
    if(step < HDR_SIZE) memmove(hdr,&hdr[step],HDR_SIZE-step);
    pos -= step;
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
  {
    const unsigned freqs[4]={ 44100, 48000, 37800, 32000 };
    uint32_t frames;
    unsigned char bt;
    sh_audio->wtag = mmioFOURCC('M','P','C',' ');
    s->seek(4);
    frames = s->read_dword();
    s->skip(2);
    bt=s->read_char();
    sh_audio->wf = (WAVEFORMATEX *)mp_malloc(sizeof(WAVEFORMATEX));
    sh_audio->wf->wFormatTag = sh_audio->wtag;
    sh_audio->wf->nChannels = 2;
    sh_audio->wf->nSamplesPerSec = freqs[bt & 3];
    sh_audio->wf->nBlockAlign = 32 * 36;
    sh_audio->wf->wBitsPerSample = 16;
    sh_audio->i_bps = sh_audio->wf->nAvgBytesPerSec;
    sh_audio->rate = sh_audio->wf->nSamplesPerSec;
    sh_audio->audio.dwSampleSize = 0;
    sh_audio->audio.dwScale = 32 * 36;
    sh_audio->audio.dwRate = sh_audio->rate;
    priv->pts_per_packet = (32 * 36) / (float)sh_audio->wf->nSamplesPerSec;
    priv->dword = 0;
    priv->pos = 32; // empty bit buffer
    priv->length = 1152 * frames / (float)sh_audio->wf->nSamplesPerSec;
    demuxer->movi_start = 24; /* skip header */
    demuxer->movi_end = s->end_pos();
    if (demuxer->movi_end > demuxer->movi_start && priv->length > 1)
      sh_audio->wf->nAvgBytesPerSec = (demuxer->movi_end - demuxer->movi_start) / priv->length;
    else
      sh_audio->wf->nAvgBytesPerSec = 32 * 1024; // dummy to make mplayerxp not hang
    sh_audio->wf->cbSize = 24;
  }

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

static uint32_t mpc_get_bits(musepack_priv_t* priv, Stream* s, int bits) {
  uint32_t out = priv->dword;
  uint32_t mask = (1 << bits) - 1;
  priv->pos += bits;
  if (priv->pos < 32) {
    out >>= (32 - priv->pos);
  }
  else {
    s->read( (any_t*)&priv->dword, 4);
    priv->dword = le2me_32(priv->dword);
    priv->pos -= 32;
    if (priv->pos) {
      out <<= priv->pos;
      out |= priv->dword >> (32 - priv->pos);
    }
  }
  return out & mask;
}

static int musepack_demux(Demuxer *demuxer,Demuxer_Stream *ds) {
  sh_audio_t* sh_audio;
  Demuxer* demux;
  musepack_priv_t* priv;
  Stream* s;
  int seof;
#ifdef MP_DEBUG
  assert(ds != NULL);
  assert(ds->sh != NULL);
  assert(ds->demuxer != NULL);
#endif
  sh_audio = reinterpret_cast<sh_audio_t*>(demuxer->audio->sh);
  demux = demuxer;
  priv = static_cast<musepack_priv_t*>(demux->priv);
  s = demux->stream;

  seof=s->eof();
  if(seof || (demux->movi_end && s->tell() >= demux->movi_end)) {
    MSG_DBG2("musepack_demux: EOF due: %s\n",
	    seof?"s->eof()":"s->tell() >= demux->movi_end");
    if(!seof) {
	MSG_DBG2("musepack_demux: stream_pos=%llu movi_end=%llu\n",
		s->tell(),
		demux->movi_end);
    }
    return 0;
  }
    int l;
    int bit_len;
    s = demux->stream;
    sh_audio = reinterpret_cast<sh_audio_t*>(ds->sh);

    if (s->eof()) return 0;

    bit_len = mpc_get_bits(priv, s, 20);
    Demuxer_Packet* dp=new(zeromem) Demuxer_Packet((bit_len + 7) / 8);
    for (l = 0; l < (bit_len / 8); l++)
	dp->buffer()[l] = mpc_get_bits(priv, s, 8);
    bit_len %= 8;
    if (bit_len)
	dp->buffer()[l] = mpc_get_bits(priv, s, bit_len) << (8 - bit_len);
    if (priv->last_pts < 0)
	priv->last_pts = 0;
    else
	priv->last_pts += priv->pts_per_packet;
    dp->pts = priv->last_pts - (demux->audio->tell_pts() -
	      sh_audio->a_in_buffer_len)/(float)sh_audio->i_bps;
    dp->flags=DP_NONKEYFRAME;
    ds->add_packet(dp);
    return 1;
}

static void musepack_seek(Demuxer *demuxer,const seek_args_t* seeka){
  sh_audio_t* sh_audio;
  Stream* s;
  musepack_priv_t* priv;

  if(!(sh_audio = reinterpret_cast<sh_audio_t*>(demuxer->audio->sh))) return;
  s = demuxer->stream;
  priv = static_cast<musepack_priv_t*>(demuxer->priv);
  {
    s = demuxer->stream;
    float target = seeka->secs;
    if (seeka->flags & DEMUX_SEEK_PERCENTS) target *= priv->length;
    if (!(seeka->flags & DEMUX_SEEK_SET)) target += priv->last_pts;
    if (target < priv->last_pts) {
	s->seek( demuxer->movi_start);
	priv->pos = 32; // empty bit buffer
	mpc_get_bits(priv, s, 8); // discard first 8 bits
	priv->last_pts = 0;
    }
    while (target > priv->last_pts) {
	int bit_len = mpc_get_bits(priv, s, 20);
	if (bit_len > 32) {
	    s->skip( bit_len / 32 * 4 - 4);
	    mpc_get_bits(priv, s, 32); // make sure dword is reloaded
	}
	mpc_get_bits(priv, s, bit_len % 32);
	priv->last_pts += priv->pts_per_packet;
	if (s->eof()) break;
    }
    return;
  }
}

static void musepack_close(Demuxer* demuxer) {
    musepack_priv_t* priv = static_cast<musepack_priv_t*>(demuxer->priv);

    if(!priv) return;
    delete priv;
}

static MPXP_Rc musepack_control(const Demuxer *demuxer,int cmd,any_t*args)
{
    UNUSED(demuxer);
    UNUSED(cmd);
    UNUSED(args);
    return MPXP_Unknown;
}

/****************** Options stuff ******************/

#include "libmpconf/cfgparser.h"

static const config_t musepack_opts[] = {
  {NULL, NULL, 0, 0, 0, 0, NULL}
};

extern const demuxer_driver_t demux_musepack =
{
    "musepack",
    "MP+ parser",
    ".mpc",
    musepack_opts,
    musepack_probe,
    musepack_open,
    musepack_demux,
    musepack_seek,
    musepack_close,
    musepack_control
};
