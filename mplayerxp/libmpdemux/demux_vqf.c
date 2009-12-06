#include "../mp_config.h"

#include <stdlib.h>
#include <stdio.h>
#include "bswap.h"

#include "stream.h"
#include "demuxer.h"
#include "stheader.h"
#include "../cfgparser.h"
#include "../libmpcodecs/interface/vqf.h"
#include "../libmpcodecs/dec_audio.h"

static int vqf_probe(demuxer_t* demuxer) 
{
  char buf[12];
  stream_t *s;
  s = demuxer->stream;
  stream_read(s,buf,12);
  if(memcmp(buf,"TWIN",4)==0) return 1; /*version: 97012000*/
  return 0;
}

static demuxer_t* vqf_open(demuxer_t* demuxer) {
  sh_audio_t* sh_audio;
  WAVEFORMATEX* w;
  stream_t *s;
  headerInfo *hi;

  s = demuxer->stream;

  sh_audio = new_sh_audio(demuxer,0);
  sh_audio->wf = w = (WAVEFORMATEX*)malloc(sizeof(WAVEFORMATEX)+sizeof(headerInfo));
  hi = (headerInfo *)&w[1];
  memset(hi,0,sizeof(headerInfo));
  w->wFormatTag = 0x1; sh_audio->format = mmioFOURCC('T','W','I','N'); /* TWinVQ */
  w->nChannels = sh_audio->channels = 2;
  w->nSamplesPerSec = sh_audio->samplerate = 44100;
  w->nAvgBytesPerSec = w->nSamplesPerSec*sh_audio->channels*2;
  w->nBlockAlign = 0;
  sh_audio->samplesize = 2;
  w->wBitsPerSample = 8*sh_audio->samplesize;
  w->cbSize = 0;
  stream_reset(s);
  stream_seek(s,0);
  stream_read(s,hi->ID,12); /* fourcc+version_id */
  while(1)
  {
    char chunk_id[4];
    unsigned chunk_size;
    hi->size=chunk_size=stream_read_dword(s); /* include itself */
    stream_read(s,chunk_id,4);
    if(*((uint32_t *)&chunk_id[0])==mmioFOURCC('C','O','M','M'))
    {
	char buf[chunk_size-8];
	unsigned i,subchunk_size;
	if(stream_read(s,buf,chunk_size-8)!=chunk_size-8) return NULL;
	i=0;
	subchunk_size=be2me_32(*((uint32_t *)&buf[0]));
	hi->channelMode=be2me_32(*((uint32_t *)&buf[4]));
	w->nChannels=sh_audio->channels=hi->channelMode+1; /*0-mono;1-stereo*/
	hi->bitRate=be2me_32(*((uint32_t *)&buf[8]));
	sh_audio->i_bps=hi->bitRate*1000/8; /* bitrate kbit/s */
	w->nAvgBytesPerSec = sh_audio->i_bps;
	hi->samplingRate=be2me_32(*((uint32_t *)&buf[12]));
	switch(hi->samplingRate){
	case 44:
		w->nSamplesPerSec=44100;
		break;
	case 22:
		w->nSamplesPerSec=22050;
		break;
	case 11:
		w->nSamplesPerSec=11025;
		break;
	default:
		w->nSamplesPerSec=hi->samplingRate*1000;
		break;
	}
	sh_audio->samplerate=w->nSamplesPerSec;
	hi->securityLevel=be2me_32(*((uint32_t *)&buf[16]));
	w->nBlockAlign = 0;
	sh_audio->samplesize = 4;
	w->wBitsPerSample = 8*sh_audio->samplesize;
	w->cbSize = 0;
	i+=subchunk_size+4;
	while(i<chunk_size-8)
	{
	    unsigned slen,sid;
	    char sdata[chunk_size];
	    sid=*((uint32_t *)&buf[i]); i+=4;
	    slen=be2me_32(*((uint32_t *)&buf[i])); i+=4;
	    if(sid==mmioFOURCC('D','S','I','Z'))
	    {
		hi->Dsiz=be2me_32(*((uint32_t *)&buf[i]));
		continue; /* describes the same info as size of DATA chunk */
	    }
	    memcpy(sdata,&buf[i],slen); sdata[slen]=0; i+=slen;
	    if(sid==mmioFOURCC('N','A','M','E'))
	    {
		memcpy(hi->Name,sdata,min(BUFSIZ,slen));
		demux_info_add(demuxer,INFOT_NAME,sdata);
	    }
	    else
	    if(sid==mmioFOURCC('A','U','T','H'))
	    {
		memcpy(hi->Auth,sdata,min(BUFSIZ,slen));
		demux_info_add(demuxer,INFOT_AUTHOR,sdata);
	    }
	    else
	    if(sid==mmioFOURCC('C','O','M','T'))
	    {
		memcpy(hi->Comt,sdata,min(BUFSIZ,slen));
		demux_info_add(demuxer,INFOT_COMMENTS,sdata);
	    }
	    else
	    if(sid==mmioFOURCC('(','c',')',' '))
	    {
		memcpy(hi->Cpyr,sdata,min(BUFSIZ,slen));
		demux_info_add(demuxer,INFOT_COPYRIGHT,sdata);
	    }
	    else
	    if(sid==mmioFOURCC('F','I','L','E'))
	    {
		memcpy(hi->File,sdata,min(BUFSIZ,slen));
	    }
	    else
	    if(sid==mmioFOURCC('A','L','B','M')) demux_info_add(demuxer,INFOT_ALBUM,sdata);
	    else
	    if(sid==mmioFOURCC('Y','E','A','R')) demux_info_add(demuxer,INFOT_DATE,sdata);
	    else
	    if(sid==mmioFOURCC('T','R','A','C')) demux_info_add(demuxer,INFOT_TRACK,sdata);
	    else
	    if(sid==mmioFOURCC('E','N','C','D')) demux_info_add(demuxer,INFOT_ENCODER,sdata);
	    else
	    MSG_V("Unhandled subchunk '%c%c%c%c'='%s'\n",((char *)&sid)[0],((char *)&sid)[1],((char *)&sid)[2],((char *)&sid)[3],sdata);
	    /* other stuff is unrecognized due untranslatable japan's idiomatics */
	}
    }
    else
    if(*((uint32_t *)&chunk_id[0])==mmioFOURCC('D','A','T','A'))
    {
	demuxer->movi_start=stream_tell(s);
	demuxer->movi_end=demuxer->movi_start+chunk_size-8;
	MSG_V("Found data at %llX size %llu\n",demuxer->movi_start,demuxer->movi_end);
	/* Done! play it */
	break;
    }
    else
    {
	MSG_V("Unhandled chunk '%c%c%c%c' %lu bytes\n",((char *)&chunk_id)[0],((char *)&chunk_id)[1],((char *)&chunk_id)[2],((char *)&chunk_id)[3],chunk_size);
	stream_skip(s,chunk_size-8); /*unknown chunk type */
    }
  }

  demuxer->movi_length = (demuxer->movi_end-demuxer->movi_start)/w->nAvgBytesPerSec;
  demuxer->audio->sh = sh_audio;
  sh_audio->ds = demuxer->audio;
  stream_seek(s,demuxer->movi_start);
  return demuxer;
}

static int vqf_demux(demuxer_t* demuxer, demux_stream_t *ds) {
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

static void vqf_seek(demuxer_t *demuxer,float rel_seek_secs,int flags){
  stream_t* s = demuxer->stream;
  sh_audio_t* sh_audio = demuxer->audio->sh;
  off_t base,pos;

  base = (flags&DEMUX_SEEK_SET) ? demuxer->movi_start : stream_tell(s);
  pos=base+(flags&DEMUX_SEEK_PERCENTS?demuxer->movi_end-demuxer->movi_start:sh_audio->i_bps)*rel_seek_secs;
  pos -= (pos % (sh_audio->channels * sh_audio->samplesize) );
  stream_seek(s,pos);
  resync_audio_stream(sh_audio);
}

static void vqf_close(demuxer_t* demuxer) {}

static int vqf_control(demuxer_t *demuxer,int cmd,void *args)
{
    return DEMUX_UNKNOWN;
}

demuxer_driver_t demux_vqf =
{
    "TwinVQ - Transform-domain Weighted Interleave Vector Quantization",
    ".vqf",
    NULL,
    vqf_probe,
    vqf_open,
    vqf_demux,
    vqf_seek,
    vqf_close,
    vqf_control
};
