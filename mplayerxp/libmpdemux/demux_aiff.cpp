#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
#include <stdlib.h>
#include <stdio.h>

#include "osdep/bswap.h"
#include "libmpstream2/stream.h"
#include "demuxer.h"
#include "demuxer_internal.h"
#include "stheader.h"
#include "libmpconf/cfgparser.h"
#include "libmpcodecs/dec_audio.h"
#include "libao2/afmt.h"
#include "demux_msg.h"


static unsigned long cvt_extended(const char * buf)
{
    unsigned long mantissa,last=0;
    unsigned char exp;
    mantissa=be2me_32(*(uint32_t *)&buf[2]);
    exp=30-buf[1];
    while(exp--)
    {
	last=mantissa;
	mantissa>>=1;
    }
    if(last&0x00000001) mantissa++;
    return mantissa;
}


struct priv_t : public Opaque
{
    public:
	priv_t() {}
	virtual ~priv_t() {}

	int verc;
};

static MPXP_Rc aiff_probe(Demuxer* demuxer)
{
  char buf[12];
  Stream *s;
  s = demuxer->stream;
  s->read(buf,12);
  if(*((uint32_t *)&buf[0])==mmioFOURCC('F','O','R','M') && *((uint32_t *)&buf[8])==mmioFOURCC('A','I','F','F')) return MPXP_Ok;
  if(*((uint32_t *)&buf[0])==mmioFOURCC('F','O','R','M') && *((uint32_t *)&buf[8])==mmioFOURCC('A','I','F','C')) return MPXP_Ok;
  return MPXP_False;
}

static Opaque* aiff_open(Demuxer* demuxer) {
  sh_audio_t* sh_audio;
  WAVEFORMATEX* w;
  Stream *s;
  priv_t *priv;
  char preamble[8];
  s = demuxer->stream;

  sh_audio = demuxer->new_sh_audio();
  priv=new(zeromem) priv_t;
  sh_audio->wf = w = new(zeromem) WAVEFORMATEX;
  w->wFormatTag = 0x1; sh_audio->wtag = mmioFOURCC('r','a','w',' '); /* S16BE */
  w->nChannels = sh_audio->nch =
  w->nSamplesPerSec = sh_audio->rate =
  w->nAvgBytesPerSec =
  w->nBlockAlign = 0;
  sh_audio->afmt = bps2afmt(2);
  w->wBitsPerSample = 8*afmt2bps(sh_audio->afmt);
  w->cbSize = 0;
  s->reset();
  s->seek(8);
  if(s->read(preamble,4)!=4) return NULL;
  if(*((uint32_t *)&preamble[0])==mmioFOURCC('A','I','F','C')) priv->verc=1;
  demuxer->movi_start=0;
  while(1)
  {
    unsigned frames=0;
    int chunk_size;
    if(s->read(preamble,8)!=8) break;
    chunk_size=be2me_32(*((uint32_t *)&preamble[4]));
    MSG_V("Got preamble: %c%c%c%c\n",preamble[0],preamble[1],preamble[2],preamble[3]);
    if(*((uint32_t *)&preamble[0])==mmioFOURCC('F','V','E','R'))
    {
	if(be2me_32(*((uint32_t *)&preamble[4]))!=4)
	{
	    MSG_V("Wrong length of VFER chunk %lu\n",be2me_32(*((uint32_t *)&preamble[4])));
	    return NULL;
	}
	if(s->read(preamble,4)!=4) return NULL;
	if(be2me_32(*((uint32_t *)&preamble[0])) == 0xA2805140)	priv->verc=1;
	else
	{
	    MSG_V("Unknown VFER chunk found %08X\n",be2me_32(*((uint32_t *)&preamble[0])));
	    return NULL;
	}
    }
    else
    if(*((uint32_t *)&preamble[0])==mmioFOURCC('C','O','M','M'))
    {
	char buf[chunk_size];
	unsigned clen=priv->verc?22:18;
	if(chunk_size < clen)
	{
	    MSG_V("Invalid COMM length %u\n",chunk_size);
	    return NULL;
	}
	if(s->read(buf,chunk_size)!=chunk_size) return NULL;
	w->nChannels = sh_audio->nch = be2me_16(*((uint16_t *)&buf[0]));
	frames=be2me_32(*((uint32_t *)&buf[2]));
	w->wBitsPerSample = be2me_16(*((uint16_t *)&buf[6]));
	sh_audio->afmt = bps2afmt(w->wBitsPerSample/8);
	w->nSamplesPerSec = sh_audio->rate = cvt_extended(&buf[8]);
	w->nAvgBytesPerSec = sh_audio->rate*afmt2bps(sh_audio->afmt)*sh_audio->nch;
	w->nBlockAlign = sh_audio->nch*afmt2bps(sh_audio->afmt);
	if(priv->verc)
	{
	    sh_audio->wtag = *((uint32_t *)&buf[18]);
	    if(sh_audio->wtag == mmioFOURCC('N','O','N','E')) sh_audio->wtag=mmioFOURCC('r','a','w',' ');
	    MSG_V("AIFC: fourcc %08X ch %u frames %u bps %u rate %u\n",sh_audio->wtag,sh_audio->nch,frames,w->wBitsPerSample,sh_audio->rate);
	}
	else
	    MSG_V("AIFF: ch %u frames %u bps %u rate %u\n",sh_audio->nch,frames,w->wBitsPerSample,sh_audio->rate);
    }
    else
    if(*((uint32_t *)&preamble[0])==mmioFOURCC('S','S','N','D') ||
       *((uint32_t *)&preamble[0])==mmioFOURCC('A','P','C','M'))
    {
	demuxer->movi_start=s->tell()+8;
	demuxer->movi_end=demuxer->movi_start+chunk_size;
	if(priv->verc && (chunk_size&1)) chunk_size++;
	s->skip(chunk_size);
    }
    else
    if(*((uint32_t *)&preamble[0])==mmioFOURCC('N','A','M','E'))
    {
	char buf[chunk_size+1];
	s->read(buf,chunk_size);
	buf[chunk_size]=0;
	demuxer->info().add(INFOT_NAME, buf);
	if(priv->verc && (chunk_size&1)) s->read_char();
    }
    else
    if(*((uint32_t *)&preamble[0])==mmioFOURCC('A','U','T','H'))
    {
	char buf[chunk_size+1];
	s->read(buf,chunk_size);
	buf[chunk_size]=0;
	demuxer->info().add(INFOT_AUTHOR, buf);
	if(priv->verc && (chunk_size&1)) s->read_char();
    }
    else
    if(*((uint32_t *)&preamble[0])==mmioFOURCC('(','c',')',' '))
    {
	char buf[chunk_size+1];
	s->read(buf,chunk_size);
	buf[chunk_size]=0;
	demuxer->info().add(INFOT_COPYRIGHT, buf);
	if(priv->verc && (chunk_size&1)) s->read_char();
    }
    else
    if(*((uint32_t *)&preamble[0])==mmioFOURCC('A','N','N','O'))
    {
	char buf[chunk_size+1];
	s->read(buf,chunk_size);
	buf[chunk_size]=0;
	demuxer->info().add(INFOT_DESCRIPTION, buf);
	if(priv->verc && (chunk_size&1)) s->read_char();
    }
    else
    {
	if(priv->verc && (chunk_size&1)) chunk_size++;
	s->skip(chunk_size); /*unknown chunk type */
    }
  }
  if(!w->nAvgBytesPerSec) { MSG_V("COMM chunk not found\n"); return NULL; /* memleak here!!! */}
  if(!demuxer->movi_start) { MSG_V("SSND(APCM) chunks not found\n"); return NULL; /* memleak here!!! */}
  demuxer->movi_length = (demuxer->movi_end-demuxer->movi_start)/w->nAvgBytesPerSec;
  demuxer->audio->sh = sh_audio;
  sh_audio->ds = demuxer->audio;
  s->seek(demuxer->movi_start);
  return priv;
}

static int aiff_demux(Demuxer* demuxer, Demuxer_Stream *ds) {
  sh_audio_t* sh_audio = reinterpret_cast<sh_audio_t*>(demuxer->audio->sh);
  int l = sh_audio->wf->nAvgBytesPerSec;
  off_t spos = demuxer->stream->tell();

  if(demuxer->stream->eof())
    return 0;

  Demuxer_Packet* dp = new(zeromem) Demuxer_Packet(l);
  dp->pts = spos / (float)(sh_audio->wf->nAvgBytesPerSec);
  dp->pos = spos;
  dp->flags = DP_NONKEYFRAME;

  l=demuxer->stream->read(dp->buffer(),l);
  dp->resize(l);
  ds->add_packet(dp);

  return 1;
}

static void aiff_seek(Demuxer *demuxer,const seek_args_t* seeka){
  Stream* s = demuxer->stream;
  sh_audio_t* sh_audio = reinterpret_cast<sh_audio_t*>(demuxer->audio->sh);
  off_t base,pos;

  base = (seeka->flags&DEMUX_SEEK_SET) ? demuxer->movi_start : s->tell();
  pos=base+(seeka->flags&DEMUX_SEEK_PERCENTS?(demuxer->movi_end - demuxer->movi_start):sh_audio->i_bps)*seeka->secs;
  pos -= (pos % (sh_audio->nch * afmt2bps(sh_audio->afmt)));
  s->seek(pos);
}

static void aiff_close(Demuxer* demuxer)
{
    delete demuxer->priv;
}

static MPXP_Rc aiff_control(const Demuxer *demuxer,int cmd,any_t*args)
{
    UNUSED(demuxer);
    UNUSED(cmd);
    UNUSED(args);
    return MPXP_Unknown;
}

extern const demuxer_driver_t demux_aiff =
{
    "aiff",
    "AIFF - Audio Interchange File Format parser",
    ".aiff",
    NULL,
    aiff_probe,
    aiff_open,
    aiff_demux,
    aiff_seek,
    aiff_close,
    aiff_control
};
