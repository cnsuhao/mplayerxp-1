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

struct voc_priv_t : public Opaque {
    public:
	voc_priv_t() {}
	virtual ~voc_priv_t() {}

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

static int voc_get_raw_id(Demuxer *demuxer,off_t fptr,unsigned *brate,unsigned *samplerate,unsigned *channels)
{
  uint32_t fcc1,fmt;
  Stream *s;
  *brate=*samplerate=*channels=0;
  s = demuxer->stream;
  s->seek(fptr);
  fcc1=s->read(type_dword);
  fcc1=me2be_32(fcc1);
  s->seek(fptr);
  binary_packet bp=s->read(32);
  if(memcmp(bp.data(),"Creative Voice File\x1A",20)==0) return 1;
  s->seek(fptr);
  return 0;
}

static MPXP_Rc voc_probe(Demuxer* demuxer)
{
  uint32_t fcc1,fcc2;
  Stream *s;
  uint8_t *p;
  s = demuxer->stream;
  fcc1=s->read(type_dword);
  fcc1=me2be_32(fcc1);
  p = (uint8_t *)&fcc1;
  if(voc_get_raw_id(demuxer,0,&fcc1,&fcc2,&fcc2)) return MPXP_Ok;
  return MPXP_False;
}

static Opaque* voc_open(Demuxer* demuxer) {
  Stream *s;
  sh_audio_t* sh_audio;
  uint8_t hdr[HDR_SIZE];
  uint32_t fcc;
  int found = 0, n = 0, pos = 0, step;
  off_t st_pos = 0;
  voc_priv_t* priv;
  const unsigned char *pfcc;
    binary_packet bp(1);
#ifdef MP_DEBUG
  assert(demuxer != NULL);
  assert(demuxer->stream != NULL);
#endif

  priv = new(zeromem) voc_priv_t;
  s = demuxer->stream;
  s->reset();
  s->seek(s->start_pos());
  while(n < 5 && !s->eof())
  {
    st_pos = s->tell();
    step = 1;

    if(pos < HDR_SIZE) {
      bp=s->read(HDR_SIZE-pos); memcpy(&hdr[pos],bp.data(),bp.size());
      pos = HDR_SIZE;
    }

    fcc = le2me_32(*(uint32_t *)hdr);
    pfcc = (const unsigned char *)&fcc;
    MSG_DBG2("AUDIO initial fcc=%c%c%c%c\n",pfcc[0],pfcc[1],pfcc[2],pfcc[3]);
    {
	uint8_t b[21];
	MSG_DBG2("initial mp3_header: 0x%08X at %lu\n",*(uint32_t *)hdr,st_pos);
	memcpy(b,hdr,HDR_SIZE);
	bp=s->read(12-HDR_SIZE); memcpy(&b[HDR_SIZE],bp.data(),bp.size());
	if(memcmp(b,"Creative Voice File\x1A",20)==0)
	{
	    found = 1;
	    break;
	}
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
    char chunk[4];
    unsigned size;
    WAVEFORMATEX* w;
    s->seek(0x14);
    bp=s->read(2); memcpy(chunk,bp.data(),bp.size());
    size=le2me_16(*reinterpret_cast<uint16_t*>(&chunk[0]));
    s->seek(size);
    bp=s->read(4); memcpy(chunk,bp.data(),bp.size());
    if(chunk[0]!=0x01) { MSG_V("VOC unknown block type %02X\n",chunk[0]); return NULL; }
    size=chunk[1]|(chunk[2]<<8)|(chunk[3]<<16);
    sh_audio->wtag = 0x01; /* PCM */
    bp=s->read(2); memcpy(chunk,bp.data(),bp.size());
    if(chunk[1]!=0) { MSG_V("VOC unknown compression type %02X\n",chunk[1]); return NULL; }
    demuxer->movi_start=s->tell();
    demuxer->movi_end=demuxer->movi_start+size;
    sh_audio->rate=256-(1000000/chunk[0]);
    sh_audio->nch=1;
    sh_audio->afmt=bps2afmt(1);
    sh_audio->wf = w = new WAVEFORMATEX;
    w->wFormatTag = sh_audio->wtag;
    w->nChannels = sh_audio->nch;
    w->nSamplesPerSec = sh_audio->rate;
    w->nAvgBytesPerSec = sh_audio->rate*afmt2bps(sh_audio->afmt)*sh_audio->nch;
    w->nBlockAlign = 1024;
    w->wBitsPerSample = (afmt2bps(sh_audio->afmt)+7)/8;
    w->cbSize = 0;
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

static int voc_demux(Demuxer *demuxer,Demuxer_Stream *ds) {
  sh_audio_t* sh_audio;
  Demuxer* demux;
  voc_priv_t* priv;
  Stream* s;
  int seof;
#ifdef MP_DEBUG
  assert(ds != NULL);
  assert(ds->sh != NULL);
  assert(ds->demuxer != NULL);
#endif
  sh_audio = reinterpret_cast<sh_audio_t*>(demuxer->audio->sh);
  demux = demuxer;
  priv = static_cast<voc_priv_t*>(demux->priv);
  s = demux->stream;

  seof=s->eof();
  if(seof || (demux->movi_end && s->tell() >= demux->movi_end)) {
    MSG_DBG2("voc_demux: EOF due: %s\n",
	    seof?"s->eof()":"s->tell() >= demux->movi_end");
    if(!seof) {
	MSG_DBG2("voc_demux: stream_pos=%llu movi_end=%llu\n",
		s->tell(),
		demux->movi_end);
    }
    return 0;
  }
    int l = 65536;
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

static void voc_seek(Demuxer *demuxer,const seek_args_t* seeka){
  sh_audio_t* sh_audio;
  Stream* s;
  int base,pos;
  voc_priv_t* priv;

  if(!(sh_audio = reinterpret_cast<sh_audio_t*>(demuxer->audio->sh))) return;
  s = demuxer->stream;
  priv = static_cast<voc_priv_t*>(demuxer->priv);

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

static void voc_close(Demuxer* demuxer) {
    voc_priv_t* priv = static_cast<voc_priv_t*>(demuxer->priv);

    if(!priv) return;
    delete priv;
}

static MPXP_Rc voc_control(const Demuxer *demuxer,int cmd,any_t*args)
{
    UNUSED(demuxer);
    UNUSED(cmd);
    UNUSED(args);
    return MPXP_Unknown;
}

/****************** Options stuff ******************/

#include "libmpconf/cfgparser.h"

static const mpxp_option_t voc_opts[] = {
  {NULL, NULL, 0, 0, 0, 0, NULL}
};

extern const demuxer_driver_t demux_voc =
{
    "voc",
    "VOC parser",
    ".voc",
    voc_opts,
    voc_probe,
    voc_open,
    voc_demux,
    voc_seek,
    voc_close,
    voc_control
};
