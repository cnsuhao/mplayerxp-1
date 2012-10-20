#include "../mp_config.h"

#include <stdlib.h>
#include <stdio.h>
#include "bswap.h"

#include "stream.h"
#include "demuxer.h"
#include "stheader.h"
#include "../cfgparser.h"
#include "../libmpcodecs/dec_audio.h"

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


typedef struct priv_s
{
    int verc;
}priv_t;

static int aiff_probe(demuxer_t* demuxer) 
{
  char buf[12];
  stream_t *s;
  s = demuxer->stream;
  stream_read(s,buf,12);
  if(*((uint32_t *)&buf[0])==mmioFOURCC('F','O','R','M') && *((uint32_t *)&buf[8])==mmioFOURCC('A','I','F','F')) return 1;
  if(*((uint32_t *)&buf[0])==mmioFOURCC('F','O','R','M') && *((uint32_t *)&buf[8])==mmioFOURCC('A','I','F','C')) return 1;
  return 0;
}

static demuxer_t* aiff_open(demuxer_t* demuxer) {
  sh_audio_t* sh_audio;
  WAVEFORMATEX* w;
  stream_t *s;
  priv_t *priv;
  char preamble[8];
  s = demuxer->stream;

  sh_audio = new_sh_audio(demuxer,0);
  priv=demuxer->priv=malloc(sizeof(priv_t));
  memset(priv,0,sizeof(priv_t));
  sh_audio->wf = w = (WAVEFORMATEX*)malloc(sizeof(WAVEFORMATEX));
  w->wFormatTag = 0x1; sh_audio->format = mmioFOURCC('r','a','w',' '); /* S16BE */
  w->nChannels = sh_audio->channels =
  w->nSamplesPerSec = sh_audio->samplerate =
  w->nAvgBytesPerSec =
  w->nBlockAlign = 0;
  sh_audio->samplesize = 2;
  w->wBitsPerSample = 8*sh_audio->samplesize;
  w->cbSize = 0;
  stream_reset(s);
  stream_seek(s,8);
  if(stream_read(s,preamble,4)!=4) return NULL;
  if(*((uint32_t *)&preamble[0])==mmioFOURCC('A','I','F','C')) priv->verc=1;
  demuxer->movi_start=0;
  while(1)
  {
    unsigned frames=0;
    int chunk_size;
    if(stream_read(s,preamble,8)!=8) break;
    chunk_size=be2me_32(*((uint32_t *)&preamble[4]));
    MSG_V("Got preamble: %c%c%c%c\n",preamble[0],preamble[1],preamble[2],preamble[3]);
    if(*((uint32_t *)&preamble[0])==mmioFOURCC('F','V','E','R'))
    {
	if(be2me_32(*((uint32_t *)&preamble[4]))!=4)
	{
	    MSG_V("Wrong length of VFER chunk %lu\n",be2me_32(*((uint32_t *)&preamble[4])));
	    return NULL;
	}
	if(stream_read(s,preamble,4)!=4) return NULL;
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
	if(stream_read(s,buf,chunk_size)!=chunk_size) return NULL;
	w->nChannels = sh_audio->channels = be2me_16(*((uint16_t *)&buf[0]));
	frames=be2me_32(*((uint32_t *)&buf[2]));
	w->wBitsPerSample = be2me_16(*((uint16_t *)&buf[6]));
	sh_audio->samplesize = w->wBitsPerSample/8;
	w->nSamplesPerSec = sh_audio->samplerate = cvt_extended(&buf[8]);
	w->nAvgBytesPerSec = sh_audio->samplerate*sh_audio->samplesize*sh_audio->channels;
	w->nBlockAlign = sh_audio->channels*sh_audio->samplesize;
	if(priv->verc)
	{
	    sh_audio->format = *((uint32_t *)&buf[18]);
	    if(sh_audio->format == mmioFOURCC('N','O','N','E')) sh_audio->format=mmioFOURCC('r','a','w',' ');
	    MSG_V("AIFC: fourcc %08X ch %u frames %u bps %u rate %u\n",sh_audio->format,sh_audio->channels,frames,w->wBitsPerSample,sh_audio->samplerate);
	}
	else
	    MSG_V("AIFF: ch %u frames %u bps %u rate %u\n",sh_audio->channels,frames,w->wBitsPerSample,sh_audio->samplerate);
    }
    else
    if(*((uint32_t *)&preamble[0])==mmioFOURCC('S','S','N','D') ||
       *((uint32_t *)&preamble[0])==mmioFOURCC('A','P','C','M'))
    {
	demuxer->movi_start=stream_tell(s)+8;
	demuxer->movi_end=demuxer->movi_start+chunk_size;
	if(priv->verc && (chunk_size&1)) chunk_size++;
	stream_skip(s,chunk_size);
    }
    else
    if(*((uint32_t *)&preamble[0])==mmioFOURCC('N','A','M','E'))
    {
	char buf[chunk_size+1];
	stream_read(s,buf,chunk_size);
	buf[chunk_size]=0;
	demux_info_add(demuxer, INFOT_NAME, buf);
	if(priv->verc && (chunk_size&1)) stream_read_char(s);
    }
    else
    if(*((uint32_t *)&preamble[0])==mmioFOURCC('A','U','T','H'))
    {
	char buf[chunk_size+1];
	stream_read(s,buf,chunk_size);
	buf[chunk_size]=0;
	demux_info_add(demuxer, INFOT_AUTHOR, buf);
	if(priv->verc && (chunk_size&1)) stream_read_char(s);
    }
    else
    if(*((uint32_t *)&preamble[0])==mmioFOURCC('(','c',')',' '))
    {
	char buf[chunk_size+1];
	stream_read(s,buf,chunk_size);
	buf[chunk_size]=0;
	demux_info_add(demuxer, INFOT_COPYRIGHT, buf);
	if(priv->verc && (chunk_size&1)) stream_read_char(s);
    }
    else
    if(*((uint32_t *)&preamble[0])==mmioFOURCC('A','N','N','O'))
    {
	char buf[chunk_size+1];
	stream_read(s,buf,chunk_size);
	buf[chunk_size]=0;
	demux_info_add(demuxer, INFOT_DESCRIPTION, buf);
	if(priv->verc && (chunk_size&1)) stream_read_char(s);
    }
    else
    {
	if(priv->verc && (chunk_size&1)) chunk_size++;
	stream_skip(s,chunk_size); /*unknown chunk type */
    }
  }
  if(!w->nAvgBytesPerSec) { MSG_V("COMM chunk not found\n"); return NULL; /* memleak here!!! */}
  if(!demuxer->movi_start) { MSG_V("SSND(APCM) chunks not found\n"); return NULL; /* memleak here!!! */}
  demuxer->movi_length = (demuxer->movi_end-demuxer->movi_start)/w->nAvgBytesPerSec;
  demuxer->audio->sh = sh_audio;
  sh_audio->ds = demuxer->audio;
  stream_seek(s,demuxer->movi_start);
  return demuxer;
}

static int aiff_demux(demuxer_t* demuxer, demux_stream_t *ds) {
  sh_audio_t* sh_audio = demuxer->audio->sh;
  int l = sh_audio->wf->nAvgBytesPerSec;
  off_t spos = stream_tell(demuxer->stream);
  demux_packet_t*  dp;

  if(stream_eof(demuxer->stream))
    return 0;

  dp = new_demux_packet(l);
  dp->pts = spos / (float)(sh_audio->wf->nAvgBytesPerSec);
  dp->pos = spos;
  dp->flags = DP_NONKEYFRAME;

  l=stream_read(demuxer->stream,dp->buffer,l);
  resize_demux_packet(dp,l);
  ds_add_packet(ds,dp);

  return 1;
}

static void aiff_seek(demuxer_t *demuxer,const seek_args_t* seeka){
  stream_t* s = demuxer->stream;
  sh_audio_t* sh_audio = demuxer->audio->sh;
  off_t base,pos;

  base = (seeka->flags&DEMUX_SEEK_SET) ? demuxer->movi_start : stream_tell(s);
  pos=base+(seeka->flags&DEMUX_SEEK_PERCENTS?(demuxer->movi_end - demuxer->movi_start):sh_audio->i_bps)*seeka->secs;
  pos -= (pos % (sh_audio->channels * sh_audio->samplesize) );
  stream_seek(s,pos);
  mpca_resync_stream(sh_audio);
}

static void aiff_close(demuxer_t* demuxer)
{
    free(demuxer->priv);
}

static int aiff_control(demuxer_t *demuxer,int cmd,any_t*args)
{
    return DEMUX_UNKNOWN;
}

demuxer_driver_t demux_aiff =
{
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
