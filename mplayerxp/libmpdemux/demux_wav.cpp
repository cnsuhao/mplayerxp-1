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

struct da_priv_t : public Opaque {
    public:
	da_priv_t() {}
	virtual ~da_priv_t() {}

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

static MPXP_Rc audio_probe(Demuxer* demuxer)
{
  uint32_t fcc1,fcc2;
  Stream *s;
  uint8_t *p;
  s = demuxer->stream;
  fcc1=s->read_dword();
  fcc1=me2be_32(fcc1);
  p = (uint8_t *)&fcc1;
  if(fcc1 == mmioFOURCC('R','I','F','F'))
  {
    s->skip(4);
    fcc2 = s->read_fourcc();
    if(fcc2 == mmioFOURCC('W','A','V','E')) return MPXP_Ok;
  }
  return MPXP_False;
}

static Opaque* audio_open(Demuxer* demuxer) {
  Stream *s;
  sh_audio_t* sh_audio;
  uint8_t hdr[HDR_SIZE];
  uint32_t fcc,fcc2;
  int found = 0, n = 0, pos = 0, step;
  off_t st_pos = 0;
  da_priv_t* priv;
  const unsigned char *pfcc;
#ifdef MP_DEBUG
  assert(demuxer != NULL);
  assert(demuxer->stream != NULL);
#endif

  priv = new(zeromem) da_priv_t;
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
    if(fcc == mmioFOURCC('R','I','F','F'))
    {
	MSG_DBG2("Found RIFF\n");
	s->skip(4);
	if(s->eof()) break;
	s->read(hdr,4);
	if(s->eof()) break;
	fcc2 = le2me_32(*(uint32_t *)hdr);
	pfcc= (const unsigned char *)&fcc2;
	MSG_DBG2("RIFF fcc=%c%c%c%c\n",pfcc[0],pfcc[1],pfcc[2],pfcc[3]);
	if(fcc2!=mmioFOURCC('W','A','V','E')) s->skip(-8);
	else
	{
	    /* We found wav header. Now we should find 'fmt '*/
	    off_t fpos;
	    fpos=s->tell();
	    MSG_DBG2("RIFF WAVE found. Start detection from %llu\n",fpos);
	    step = 4;
	    while(1)
	    {
		unsigned chunk_len;
		fcc=s->read_fourcc();
		pfcc= (const unsigned char *)&fcc;
		MSG_DBG2("fmt fcc=%c%c%c%c\n",pfcc[0],pfcc[1],pfcc[2],pfcc[3]);
		if(fcc==mmioFOURCC('f','m','t',' '))
		{
		    MSG_DBG2("RIFF WAVE fmt found\n");
		    found =1;
		    break;
		}
		if(s->eof()) break;
		chunk_len=s->read_dword_le();
		s->skip(chunk_len);
	    }
	    MSG_DBG2("Restore stream pos %llu\n",fpos);
	    s->seek(fpos);
	    if(found) break;
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
    off_t fpos,data_off=-1;
    unsigned int chunk_type;
    unsigned int chunk_size;
    WAVEFORMATEX* w;
    int l;
    sh_audio->wf = w = (WAVEFORMATEX*)mp_malloc(sizeof(WAVEFORMATEX));
    do
    {
      chunk_type = s->read_fourcc();
      chunk_size = s->read_dword_le();
      fpos=s->tell();
      switch(chunk_type)
      {
	case mmioFOURCC('f','m','t',' '):
		{
		    l = chunk_size;
		    MSG_DBG2("Found %u bytes WAVEFORMATEX\n",l);
		    if(l < 16) {
			MSG_ERR("Bad wav header length : too short !!!\n");
			delete sh_audio;
			return NULL;
		    }
		    w->wFormatTag = sh_audio->wtag = s->read_word_le();
		    w->nChannels = sh_audio->nch = s->read_word_le();
		    w->nSamplesPerSec = sh_audio->rate = s->read_dword_le();
		    w->nAvgBytesPerSec = s->read_dword_le();
		    w->nBlockAlign = s->read_word_le();
		    w->wBitsPerSample =  s->read_word_le();
		    sh_audio->afmt = bps2afmt((w->wBitsPerSample+7)/8);
		    w->cbSize = 0;
		    l -= 16;
		    if(l) s->skip(l);
		}
		break;
	case mmioFOURCC('d', 'a', 't', 'a'):
		MSG_DBG2("Found data chunk at %llu\n",fpos);
		data_off=fpos;
		s->skip(chunk_size);
		break;
	case mmioFOURCC('l', 'i', 's', 't'):
	    {
		uint32_t cfcc;
		MSG_DBG2("RIFF 'list' found\n");
		cfcc=s->read_fourcc();
		if(cfcc!=mmioFOURCC('a', 'd', 't', 'l')) { s->seek(fpos); break; }
		do
		{
		    unsigned int subchunk_type;
		    unsigned int subchunk_size;
		    unsigned int subchunk_id;
		    unsigned slen,rlen;
		    char note[256];
		    MSG_DBG2("RIFF 'list' accepted\n");
		    subchunk_type = s->read_fourcc();
		    subchunk_size = s->read_dword_le();
		    subchunk_id = s->read_dword_le();
		    if(subchunk_type==mmioFOURCC('l','a','b','l'))
		    {
			slen=subchunk_size-4;
			rlen=std::min(sizeof(note),size_t(slen));
			s->read(note,rlen);
			note[rlen]=0;
			if(slen>rlen) s->skip(slen-rlen);
			demuxer->info().add(INFOT_NAME,note);
			MSG_DBG2("RIFF 'labl' %u %s accepted\n",slen,note);
		    }
		    else
		    if(subchunk_type==mmioFOURCC('n','o','t','e'))
		    {
			slen=subchunk_size-4;
			rlen=std::min(sizeof(note),size_t(slen));
			s->read(note,rlen);
			note[rlen]=0;
			if(slen>rlen) s->skip(slen-rlen);
			demuxer->info().add(INFOT_COMMENTS,note);
			MSG_DBG2("RIFF 'note' %u %s accepted\n",slen,note);
		    }
		    else s->skip(subchunk_size);
		}while(s->tell()<fpos+chunk_size);
		s->seek(fpos+chunk_size);
	    }
	    break;
	default:
	    s->skip( chunk_size);
	    pfcc=(unsigned char *)&chunk_type;
	    MSG_DBG2("RIFF unhandled '%c%c%c%c' chunk skipped\n",pfcc[0],pfcc[1],pfcc[2],pfcc[3]);
	    break;
      }
    } while (!s->eof());
    if(data_off==-1)
    {
	MSG_ERR("RIFF WAVE - no 'data' chunk found\n");
	return NULL;
    }
    s->seek(data_off);
    demuxer->movi_start = s->tell();
    s->seek(data_off);
    /* id3v1 tags may exist within WAV */
#if 0
    if(sh_audio->wtag==0x50 || sh_audio->wtag==0x55)
    {
	s->seek(data_off);
	s->read(hdr,4);
	MSG_DBG2("Trying id3v1 at %llX\n",data_off);
	if(!read_mp3v1_tags(demuxer,hdr,data_off)) demuxer->movi_end = s->end_pos();
    }
    else
#endif
	demuxer->movi_end = s->end_pos();
    s->seek(data_off);

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

static int audio_demux(Demuxer *demuxer,Demuxer_Stream *ds) {
  sh_audio_t* sh_audio;
  Demuxer* demux;
  da_priv_t* priv;
  Stream* s;
  int seof;
#ifdef MP_DEBUG
  assert(ds != NULL);
  assert(ds->sh != NULL);
  assert(ds->demuxer != NULL);
#endif
  sh_audio = reinterpret_cast<sh_audio_t*>(demuxer->audio->sh);
  demux = demuxer;
  priv = static_cast<da_priv_t*>(demux->priv);
  s = demux->stream;

  seof=s->eof();
  if(seof || (demux->movi_end && s->tell() >= demux->movi_end)) {
    MSG_DBG2("audio_demux: EOF due: %s\n",
	    seof?"s->eof()":"s->tell() >= demux->movi_end");
    if(!seof) {
	MSG_DBG2("audio_demux: stream_pos=%llu movi_end=%llu\n",
		s->tell(),
		demux->movi_end);
    }
    return 0;
  }
    int l = sh_audio->wf->nAvgBytesPerSec;
    Demuxer_Packet* dp =new(zeromem) Demuxer_Packet(l);
    l=s->read(dp->buffer(),l);
    dp->resize(l);
    priv->last_pts = priv->last_pts < 0 ? 0 : priv->last_pts + l/(float)sh_audio->i_bps;
    dp->pts = priv->last_pts - (demux->audio->tell_pts()-sh_audio->a_in_buffer_len)/(float)sh_audio->i_bps;
    dp->flags=DP_NONKEYFRAME;
    ds->add_packet(dp);
    return 1;
}

static void audio_seek(Demuxer *demuxer,const seek_args_t* seeka){
  sh_audio_t* sh_audio;
  Stream* s;
  int base,pos;
  da_priv_t* priv;

  if(!(sh_audio = reinterpret_cast<sh_audio_t*>(demuxer->audio->sh))) return;
  s = demuxer->stream;
  priv = static_cast<da_priv_t*>(demuxer->priv);

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

static void audio_close(Demuxer* demuxer) {
    da_priv_t* priv = static_cast<da_priv_t*>(demuxer->priv);

    if(!priv) return;
    delete priv;
}

static MPXP_Rc audio_control(const Demuxer *demuxer,int cmd,any_t*args)
{
    UNUSED(demuxer);
    UNUSED(cmd);
    UNUSED(args);
    return MPXP_Unknown;
}

/****************** Options stuff ******************/

#include "libmpconf/cfgparser.h"

static const mpxp_option_t audio_opts[] = {
  {NULL, NULL, 0, 0, 0, 0, NULL}
};

extern const demuxer_driver_t demux_wav =
{
    "wav",
    "WAV parser",
    ".wav",
    audio_opts,
    audio_probe,
    audio_open,
    audio_demux,
    audio_seek,
    audio_close,
    audio_control
};
