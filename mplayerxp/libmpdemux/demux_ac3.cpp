#include "mpxp_config.h"
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
#include "libao3/afmt.h"
#include "aviprint.h"
#include "osdep/bswap.h"
#include "mp3_hdr.h"
#include "demux_msg.h"

#define HDR_SIZE 4

struct ac3_priv_t : public Opaque {
    public:
	ac3_priv_t() {}
	virtual ~ac3_priv_t() {}

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

#define AC3_CHANNEL 0
#define AC3_MONO 1
#define AC3_STEREO 2
#define AC3_3F 3
#define AC3_2F1R 4
#define AC3_3F1R 5
#define AC3_2F2R 6
#define AC3_3F2R 7
#define AC3_CHANNEL1 8
#define AC3_CHANNEL2 9
#define AC3_DOLBY 10
#define AC3_CHANNEL_MASK 15
#define AC3_LFE 16
#define AC3_ADJUST_LEVEL 32
static int ac3_decode_header (const uint8_t * buf,unsigned* sample_rate,unsigned* bit_rate,unsigned* channels)
{
    static int rate[] = { 32,  40,  48,  56,  64,  80,  96, 112,
			 128, 160, 192, 224, 256, 320, 384, 448,
			 512, 576, 640};
    static uint8_t lfeon[8] = {0x10, 0x10, 0x04, 0x04, 0x04, 0x01, 0x04, 0x01};
    static uint8_t halfrate[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3};
    int frmsizecod;
    int bitrate;
    int half;
    int acmod;
    int flags;

    if ((buf[0] != 0x0b) || (buf[1] != 0x77))	/* syncword */
	return 0;

    if (buf[5] >= 0x60)		/* bsid >= 12 */
	return 0;
    half = halfrate[buf[5] >> 3];

    /* acmod, dsurmod and lfeon */
    acmod = buf[6] >> 5;
    flags = ((((buf[6] & 0xf8) == 0x50) ? AC3_DOLBY : acmod) |
	      ((buf[6] & lfeon[acmod]) ? AC3_LFE : 0));

    switch(flags & AC3_CHANNEL_MASK)
    {
	default:
	case AC3_CHANNEL1:
	case AC3_CHANNEL2:
	case AC3_CHANNEL:
	case AC3_MONO:
	    *channels=1;
	    break;
	case AC3_STEREO:
	    *channels=2;
	    break;
	case AC3_3F:
	case AC3_2F1R:
	    *channels=3;
	    break;
	case AC3_3F1R:
	case AC3_2F2R:
	    *channels=4;
	    break;
	case AC3_3F2R:
	    *channels=5;
	    break;
	case AC3_DOLBY:
	    *channels=6;
	    break;
    }
    if((flags & AC3_LFE)==AC3_LFE) (*channels)++;
    frmsizecod = buf[4] & 63;
    if (frmsizecod >= 38)
	return 0;
    bitrate = rate [frmsizecod >> 1];
    *bit_rate = (bitrate * 1000) >> half;

    switch (buf[4] & 0xc0) {
    case 0:
	*sample_rate = 48000 >> half;
	return 4 * bitrate;
    case 0x40:
	*sample_rate = 44100 >> half;
	return 2 * (320 * bitrate / 147 + (frmsizecod & 1));
    case 0x80:
	*sample_rate = 32000 >> half;
	return 6 * bitrate;
    default:
	return 0;
    }
}

static int read_ac3_tags(Demuxer *demuxer,uint8_t *hdr, off_t pos,unsigned *bitrate,unsigned *samplerate,unsigned *channels)
{
    uint8_t b[8];
    unsigned n;
    Stream *s=demuxer->stream;
    demuxer->movi_end = s->end_pos();
    memcpy(b,hdr,4);
    s->seek(pos+4);
    s->read(&b[4],4);
    for(n = 0; n < 5 ; n++) {
      MSG_DBG2("read_ac3_tags\n");
      pos = ac3_decode_header(b,bitrate,samplerate,channels);
      if(pos < 0)
	return 0;
      s->skip(pos-8);
      if(s->eof())
	return 0;
      s->read(b,8);
      if(s->eof())
	return 0;
    }
    return 1;
}

static int ac3_get_raw_id(Demuxer *demuxer,off_t fptr,unsigned *brate,unsigned *samplerate,unsigned *channels)
{
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
  /* ac3 header check */
  if(ac3_decode_header(b,samplerate,brate,channels)>0) return 1;
  s->seek(fptr);
  return 0;
}

static MPXP_Rc ac3_probe(Demuxer* demuxer)
{
  uint32_t fcc1,fcc2;
  Stream *s;
  uint8_t *p;
  s = demuxer->stream;
  fcc1=s->read_dword();
  fcc1=me2be_32(fcc1);
  p = (uint8_t *)&fcc1;
  if(ac3_get_raw_id(demuxer,0,&fcc1,&fcc2,&fcc2)) return MPXP_Ok;
  return MPXP_False;
}

static Opaque* ac3_open(Demuxer* demuxer) {
  Stream *s;
  sh_audio_t* sh_audio;
  uint8_t hdr[HDR_SIZE];
  uint32_t fcc;
  int frmt = 0, n = 0, pos = 0, step;
  off_t st_pos = 0;
  ac3_priv_t* priv;
  const unsigned char *pfcc;
#ifdef MP_DEBUG
  assert(demuxer != NULL);
  assert(demuxer->stream != NULL);
#endif

  priv = new(zeromem) ac3_priv_t;
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
    unsigned fmt;
    uint8_t b[21];
    MSG_DBG2("initial mp3_header: 0x%08X at %lu\n",*(uint32_t *)hdr,st_pos);
    memcpy(b,hdr,HDR_SIZE);
    s->read(&b[HDR_SIZE],12-HDR_SIZE);
    if((n = ac3_decode_header(b,&fmt,&fmt,&fmt)) > 0) {
	demuxer->movi_start = st_pos;
	break;
    }
    /* Add here some other audio format detection */
    if(step < HDR_SIZE) memmove(hdr,&hdr[step],HDR_SIZE-step);
    pos -= step;
  }

  if(!frmt)
  {
    MSG_ERR("Can't detect audio format\n");
    return NULL;
  }
  sh_audio = demuxer->new_sh_audio();
  MSG_DBG2("mp3_header off: st_pos=%lu n=%lu HDR_SIZE=%u\n",st_pos,n,HDR_SIZE);
  demuxer->movi_start = s->tell();
  demuxer->movi_end = s->end_pos();
  sh_audio->wtag = 0x2000;
  if(!read_ac3_tags(demuxer,hdr,pos,&sh_audio->i_bps,&sh_audio->rate,&sh_audio->nch)) return 0;
  demuxer->movi_end = s->end_pos();
  s->seek(demuxer->movi_start);

  priv->frmt = frmt;
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

static int ac3_demux(Demuxer *demuxer,Demuxer_Stream *ds) {
  sh_audio_t* sh_audio;
  Demuxer* demux;
  ac3_priv_t* priv;
  Stream* s;
  int frmt,seof;
#ifdef MP_DEBUG
  assert(ds != NULL);
  assert(ds->sh != NULL);
  assert(ds->demuxer != NULL);
#endif
  sh_audio = reinterpret_cast<sh_audio_t*>(demuxer->audio->sh);
  demux = demuxer;
  priv = static_cast<ac3_priv_t*>(demux->priv);
  s = demux->stream;

  seof=s->eof();
  if(seof || (demux->movi_end && s->tell() >= demux->movi_end)) {
    MSG_DBG2("ac3_demux: EOF due: %s\n",
	    seof?"s->eof()":"s->tell() >= demux->movi_end");
    if(!seof) {
	MSG_DBG2("ac3_demux: stream_pos=%llu movi_end=%llu\n",
		s->tell(),
		demux->movi_end);
    }
    return 0;
  }
  frmt=priv->frmt;
    while(!s->eof() || (demux->movi_end && s->tell() >= demux->movi_end) ) {
      uint8_t hdr[8];
      int len;
      unsigned dummy;
      s->read(hdr,8);
      len = ac3_decode_header(hdr,&dummy,&dummy,&dummy);
      MSG_DBG2("ac3_fillbuffer %u bytes\n",len);
      if(s->eof()) return 0; /* workaround for dead-lock (skip(-7)) below */
      if(len < 0) {
	s->skip(-7);
      } else {
	if(s->eof() || (demux->movi_end && s->tell() >= demux->movi_end) )
	  return 0;
	if(len>8)
	{
	    Demuxer_Packet* dp = new(zeromem) Demuxer_Packet(len);
	    memcpy(dp->buffer(),hdr,8);
	    dp->resize(len+8);
	    len=s->read(dp->buffer()+8,len-8);
	    priv->last_pts = priv->last_pts < 0 ? 0 : priv->last_pts + len/(float)sh_audio->i_bps;
	    dp->pts = priv->last_pts - (demux->audio->tell_pts()-sh_audio->a_in_buffer_len)/(float)sh_audio->i_bps;
	    dp->flags=DP_NONKEYFRAME;
	    ds->add_packet(dp);
	}
	else s->skip(len);
	return 1;
      }
    }
  return 0;
}

static void high_res_ac3_seek(Demuxer *demuxer,float _time) {
  uint8_t hdr[8];
  int len,nf;
  unsigned tmp;
  ac3_priv_t* priv = static_cast<ac3_priv_t*>(demuxer->priv);
  sh_audio_t* sh = (sh_audio_t*)demuxer->audio->sh;

  nf = _time*sh->rate/1152;
  while(nf > 0) {
    demuxer->stream->read(hdr,8);
    MSG_DBG2("high_res_mp3_seek\n");
    len = ac3_decode_header(hdr,&tmp,&tmp,&tmp);
    if(len < 0) {
      demuxer->stream->skip(-7);
      continue;
    }
    demuxer->stream->skip(len-8);
    priv->last_pts += 1152/(float)sh->rate;
    nf--;
  }
}

static void ac3_seek(Demuxer *demuxer,const seek_args_t* seeka){
  sh_audio_t* sh_audio;
  Stream* s;
  int base,pos;
  float len;
  ac3_priv_t* priv;

  if(!(sh_audio = reinterpret_cast<sh_audio_t*>(demuxer->audio->sh))) return;
  s = demuxer->stream;
  priv = static_cast<ac3_priv_t*>(demuxer->priv);

  base = seeka->flags&DEMUX_SEEK_SET ? demuxer->movi_start : s->tell();
  pos=base+(seeka->flags&DEMUX_SEEK_PERCENTS?(demuxer->movi_end - demuxer->movi_start):sh_audio->i_bps)*seeka->secs;

  if(demuxer->movi_end && pos >= demuxer->movi_end) {
    sh_audio->timer = (s->tell() - demuxer->movi_start)/(float)sh_audio->i_bps;
    return;
  } else if(pos < demuxer->movi_start)
    pos = demuxer->movi_start;

  priv->last_pts = (pos-demuxer->movi_start)/(float)sh_audio->i_bps;
  sh_audio->timer = priv->last_pts - (demuxer->audio->tell_pts()-sh_audio->a_in_buffer_len)/(float)sh_audio->i_bps;

  len = (seeka->flags & DEMUX_SEEK_SET) ? seeka->secs - priv->last_pts : seeka->secs;
  if(len < 0) {
      s->seek(demuxer->movi_start);
      len = priv->last_pts + len;
      priv->last_pts = 0;
  }
  if(len > 0)
      high_res_ac3_seek(demuxer,len);
  sh_audio->timer = priv->last_pts - (demuxer->audio->tell_pts()-sh_audio->a_in_buffer_len)/(float)sh_audio->i_bps;
  return;
}

static void ac3_close(Demuxer* demuxer) {
    ac3_priv_t* priv = static_cast<ac3_priv_t*>(demuxer->priv);

    if(!priv) return;
    delete priv;
}

static MPXP_Rc ac3_control(const Demuxer *demuxer,int cmd,any_t*args)
{
    UNUSED(demuxer);
    UNUSED(cmd);
    UNUSED(args);
    return MPXP_Unknown;
}

/****************** Options stuff ******************/

#include "libmpconf/cfgparser.h"

static const mpxp_option_t ac3_opts[] = {
  {NULL, NULL, 0, 0, 0, 0, NULL}
};

extern const demuxer_driver_t demux_ac3 =
{
    "ac3",
    "AC3 parser",
    ".ac3",
    ac3_opts,
    ac3_probe,
    ac3_open,
    ac3_demux,
    ac3_seek,
    ac3_close,
    ac3_control
};
