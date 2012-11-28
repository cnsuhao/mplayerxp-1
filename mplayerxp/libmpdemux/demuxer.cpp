#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
//=================== DEMUXER v2.5 =========================
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "help_mp.h"
#include "libmpsub/subreader.h"
#include "libmpconf/cfgparser.h"

#include "nls/nls.h"

#include "osdep/fastmemcpy.h"
#include "libvo/sub.h"
#include "libao2/afmt.h"

#include "demux_msg.h"
#include "demuxer.h"
#include "libmpstream/stream.h"
#include "stheader.h"
#include "mplayerxp.h"

extern const demuxer_driver_t demux_aiff;
extern const demuxer_driver_t demux_rawaudio;
extern const demuxer_driver_t demux_rawvideo;
extern const demuxer_driver_t demux_avi;
extern const demuxer_driver_t demux_y4m;
extern const demuxer_driver_t demux_asf;
extern const demuxer_driver_t demux_nuv;
extern const demuxer_driver_t demux_nsv;
extern const demuxer_driver_t demux_mov;
extern const demuxer_driver_t demux_mkv;
extern const demuxer_driver_t demux_vivo;
extern const demuxer_driver_t demux_ra;
extern const demuxer_driver_t demux_real;
extern const demuxer_driver_t demux_fli;
extern const demuxer_driver_t demux_film;
extern const demuxer_driver_t demux_roq;
extern const demuxer_driver_t demux_bmp;
extern const demuxer_driver_t demux_ogg;
extern const demuxer_driver_t demux_pva;
extern const demuxer_driver_t demux_smjpeg;
extern const demuxer_driver_t demux_vqf;
extern const demuxer_driver_t demux_mpxpav64;
extern const demuxer_driver_t demux_mpgps;
extern const demuxer_driver_t demux_mpgts;
extern const demuxer_driver_t demux_ty;
extern const demuxer_driver_t demux_audio;
extern const demuxer_driver_t demux_lavf;
extern const demuxer_driver_t demux_null;

static const demuxer_driver_t *ddrivers[] =
{
    &demux_rawaudio,
    &demux_rawvideo,
    &demux_avi,
    &demux_y4m,
    &demux_asf,
    &demux_nsv,
    &demux_nuv,
    &demux_mov,
    &demux_mkv,
    &demux_vivo,
    &demux_ra,
    &demux_real,
    &demux_fli,
    &demux_film,
    &demux_roq,
    &demux_bmp,
#ifdef HAVE_LIBVORBIS
    &demux_ogg,
#endif
    &demux_pva,
    &demux_smjpeg,
    &demux_vqf,
    &demux_mpxpav64,
    &demux_mpgps,
    &demux_aiff,
    &demux_audio,
    &demux_mpgts,
    &demux_ty,
    &demux_lavf,
    &demux_null,
    NULL
};

typedef struct demuxer_info_st {
    char *id[INFOT_MAX];
} demuxer_info_t;

void libmpdemux_register_options(m_config_t* cfg)
{
    unsigned i;
    for(i=0;ddrivers[i];i++) {
	if(ddrivers[i]->options)
	    m_config_register_options(cfg,ddrivers[i]->options);
	if(ddrivers[i]==&demux_null) break;
    }
}

void free_demuxer_stream(demux_stream_t *ds){
    if(ds) {
	ds_free_packs(ds);
	delete ds;
    }
}

int demux_aid_vid_mismatch = 0;

demux_stream_t* new_demuxer_stream(struct demuxer_s *demuxer,int id){
  demux_stream_t* ds=(demux_stream_t*)mp_malloc(sizeof(demux_stream_t));
  rnd_fill(ds->antiviral_hole,offsetof(demux_stream_t,pin)-offsetof(demux_stream_t,antiviral_hole));
  ds->pin=DS_PIN;
  ds->buffer_pos=ds->buffer_size=0;
  ds->buffer=NULL;
  ds->pts=0;
  ds->pts_bytes=0;
  ds->eof=0;
  ds->pos=0;
  ds->dpos=0;
  ds->pack_no=0;
//---------------
  ds->packs=0;
  ds->bytes=0;
  ds->first=ds->last=ds->current=NULL;
  ds->id=id;
  ds->demuxer=demuxer;
//----------------
  ds->asf_seq=-1;
  ds->asf_packet=NULL;
//----------------
  ds->sh=NULL;
  ds->pts_flags=0;
  ds->prev_pts=ds->pts_corr=0;
  return ds;
}

demuxer_t* new_demuxer(stream_t *stream,int type,int a_id,int v_id,int s_id){
  demuxer_t *d=(demuxer_t*)mp_mallocz(sizeof(demuxer_t));
  rnd_fill(d->antiviral_hole,offsetof(demuxer_t,pin)-offsetof(demuxer_t,antiviral_hole));
  d->pin=DEMUX_PIN;
  d->stream=stream;
  d->movi_start=stream->start_pos;
  d->movi_end=stream->end_pos;
  d->movi_length=UINT_MAX;
  d->flags|=DEMUXF_SEEKABLE;
  d->synced=0;
  d->filepos=0;
  d->audio=new_demuxer_stream(d,a_id);
  d->video=new_demuxer_stream(d,v_id);
  d->sub=new_demuxer_stream(d,s_id);
  d->file_format=type;
  d->info=mp_mallocz(sizeof(demuxer_info_t));
  stream_reset(stream);
  stream_seek(stream,stream->start_pos);
  return d;
}

sh_audio_t *get_sh_audio(demuxer_t *demuxer, int id)
{
    if(id > MAX_A_STREAMS-1 || id < 0) {
	MSG_WARN("Requested audio stream id overflow (%d > %d)\n",
	    id, MAX_A_STREAMS);
	return NULL;
    }
    check_pin("demuxer",demuxer->pin,DEMUX_PIN);
    return reinterpret_cast<sh_audio_t*>(demuxer->a_streams[id]);
}

sh_audio_t* new_sh_audio_aid(demuxer_t *demuxer,int id,int aid){
    if(id > MAX_A_STREAMS-1 || id < 0) {
	MSG_WARN("Requested audio stream id overflow (%d > %d)\n",
	    id, MAX_A_STREAMS);
	return NULL;
    }
    if(demuxer->a_streams[id]) {
	MSG_WARN(MSGTR_AudioStreamRedefined,id);
    } else {
	sh_audio_t *sh;
	MSG_V("==> Found audio stream: %d\n",id);
	demuxer->a_streams[id]=mp_calloc(1, sizeof(sh_audio_t));
	sh = reinterpret_cast<sh_audio_t*>(demuxer->a_streams[id]);
	// set some defaults
	sh->afmt=bps2afmt(2); /* PCM */
	sh->audio_out_minsize=8192;/* default size, maybe not enough for Win32/ACM*/
	  MSG_V("ID_AUDIO_ID=%d\n", aid);
    }
    ((sh_audio_t *)demuxer->a_streams[id])->aid = aid;
    check_pin("demuxer",demuxer->pin,DEMUX_PIN);
    return reinterpret_cast<sh_audio_t*>(demuxer->a_streams[id]);
}

void free_sh_audio(sh_audio_t* sh){
    MSG_V("DEMUXER: freeing sh_audio at %p  \n",sh);
    if(sh->wf) delete sh->wf;
    delete sh;
}

sh_video_t* get_sh_video(demuxer_t *demuxer, int id)
{
    if(id > MAX_V_STREAMS-1 || id < 0) {
	MSG_WARN("Requested video stream id overflow (%d > %d)\n",
	    id, MAX_V_STREAMS);
	return NULL;
    }
    check_pin("demuxer",demuxer->pin,DEMUX_PIN);
    return reinterpret_cast<sh_video_t*>(demuxer->v_streams[id]);
}

sh_video_t* new_sh_video_vid(demuxer_t *demuxer,int id,int vid){
    if(id > MAX_V_STREAMS-1 || id < 0) {
	MSG_WARN("Requested video stream id overflow (%d > %d)\n",
	    id, MAX_V_STREAMS);
	return NULL;
    }
    if(demuxer->v_streams[id]) {
	MSG_WARN(MSGTR_VideoStreamRedefined,id);
    } else {
	MSG_V("==> Found video stream: %d\n",id);
	demuxer->v_streams[id]=mp_calloc(1, sizeof(sh_video_t));
	  MSG_V("ID_VIDEO_ID=%d\n", vid);
    }
    ((sh_video_t *)demuxer->v_streams[id])->vid = vid;
    check_pin("demuxer",demuxer->pin,DEMUX_PIN);
    return reinterpret_cast<sh_video_t*>(demuxer->v_streams[id]);
}

void free_sh_video(sh_video_t* sh){
    MSG_V("DEMUXER: freeing sh_video at %p  \n",sh);
    if(sh->bih) delete sh->bih;
    delete sh;
}

void free_demuxer(demuxer_t *demuxer){
    unsigned i;
    if(demuxer)
    {
	MSG_V("DEMUXER: freeing demuxer at %p  \n",demuxer);

	if(demuxer->driver) demuxer->driver->close(demuxer);

	// mp_free streams:
	for(i=0;i<MAX_A_STREAMS;i++) if(demuxer->a_streams[i]) free_sh_audio(reinterpret_cast<sh_audio_t*>(demuxer->a_streams[i]));
	for(i=0;i<MAX_V_STREAMS;i++) if(demuxer->v_streams[i]) free_sh_video(reinterpret_cast<sh_video_t*>(demuxer->v_streams[i]));
	//if(sh_audio) free_sh_audio(sh_audio);
	//if(sh_video) free_sh_video(sh_video);
	// mp_free demuxers:
	FREE_DEMUXER_STREAM(demuxer->audio);
	FREE_DEMUXER_STREAM(demuxer->video);
	FREE_DEMUXER_STREAM(demuxer->sub);
	demux_info_free(demuxer);
	delete demuxer;
    }
}

Demux_Packet::Demux_Packet(unsigned _len)
	    :pts(0),
	    pos(0),
	    flags(0),
	    next(NULL)
{
  len=_len;
  buffer=new unsigned char [len];
}

Demux_Packet::~Demux_Packet(){
    if(buffer) delete buffer;
}

void Demux_Packet::resize(unsigned newlen)
{
    if(len!=newlen) {
	if(newlen) {
	    buffer=(unsigned char *)mp_realloc(buffer,newlen+8);
	    memset(buffer+newlen,0,8);
	} else {
	    if(buffer) delete buffer;
	    buffer=NULL;
	}
	len=newlen;
    }
}

Demux_Packet* Demux_Packet::clone() const {
  Demux_Packet* dp=new Demux_Packet(len);
  dp->pts=pts;
  dp->pos=pos;
  dp->flags=flags;
  dp->next=next;
  memcpy(dp->buffer,buffer,len);
  return dp;
}

void ds_add_packet(demux_stream_t *ds,Demux_Packet* dp){
//    Demux_Packet* dp=new_demux_packet(len);
//    stream_read(stream,dp->buffer,len);
//    dp->pts=pts; //(float)pts/90000.0f;
//    dp->pos=pos;
    // append packet to DS stream:
    if(dp->len>0) {
	++ds->packs;
	ds->bytes+=dp->len;
	if(ds->last) {
	    // next packet in stream
	    ds->last->next=dp;
	    ds->last=dp;
	} else {
	    // first packet in stream
	    ds->first=ds->last=dp;
	}
	MSG_DBG2("DEMUX: Append packet to %s, len=%d  pts=%5.3f  pos=%u  [packs: A=%d V=%d]\n",
	    (ds==ds->demuxer->audio)?"d_audio":"d_video",
	    dp->len,dp->pts,(unsigned int)dp->pos,ds->demuxer->audio->packs,ds->demuxer->video->packs);
    }
    else
	MSG_DBG2("DEMUX: Skip packet for %s, len=%d  pts=%5.3f  pos=%u  [packs: A=%d V=%d]\n",
	    (ds==ds->demuxer->audio)?"d_audio":"d_video",
	    dp->len,dp->pts,(unsigned int)dp->pos,ds->demuxer->audio->packs,ds->demuxer->video->packs);
}

void ds_read_packet(demux_stream_t *ds,stream_t *stream,int len,float pts,off_t pos,int flags){
    Demux_Packet* dp=new(zeromem) Demux_Packet(len);
    len=stream_read(stream,dp->buffer,len);
    dp->resize(len);
    dp->pts=pts; //(float)pts/90000.0f;
    dp->pos=pos;
    dp->flags=flags;
    // append packet to DS stream:
    ds_add_packet(ds,dp);
    MSG_DBG2("ds_read_packet(%s,%u,%f,%llu,%i)\n",ds==ds->demuxer->video?"video":"audio",len,pts,pos,flags);
}

int demux_fill_buffer(demuxer_t *demux,demux_stream_t *ds){
    /* Note: parameter 'ds' can be NULL! */
    return demux->driver->demux(demux,ds);
}

// return value:
//     0 = EOF
//     1 = succesfull
int ds_fill_buffer(demux_stream_t *ds){
  demuxer_t *demux=ds->demuxer;
  if(ds->buffer) delete ds->buffer;
/*  ds_free_packs(ds); */
  if(mp_conf.verbose>2) {
    if(ds==demux->audio)
	MSG_DBG3("ds_fill_buffer(d_audio) called\n");
    else
    if(ds==demux->video)
	MSG_DBG3("ds_fill_buffer(d_video) called\n");
    else
    if(ds==demux->sub)
	MSG_DBG3("ds_fill_buffer(d_sub) called\n");
    else
	MSG_DBG3("ds_fill_buffer(unknown %p) called\n",ds);
  }
  check_pin("demuxer",ds->pin,DS_PIN);
  while(1){
    if(ds->packs){
      Demux_Packet *p=ds->first;
      // copy useful data:
      ds->buffer=p->buffer;
      ds->buffer_pos=0;
      ds->buffer_size=p->len;
      ds->pos=p->pos;
      ds->dpos+=p->len; // !!!
      ++ds->pack_no;
      if(p->pts){
	ds->pts=p->pts;
	ds->pts_bytes=0;
      }
      ds->pts_bytes+=p->len; // !!!
      ds->flags=p->flags;
      // mp_free packet:
      ds->bytes-=p->len;
      ds->current=p;
      ds->first=p->next;
      if(!ds->first) ds->last=NULL;
      --ds->packs;
      return 1; //ds->buffer_size;
    }
    if(demux->audio->bytes>=MAX_PACK_BYTES){
      MSG_ERR(MSGTR_TooManyAudioInBuffer,demux->audio->packs,demux->audio->bytes);
      MSG_HINT(MSGTR_MaybeNI);
      break;
    }
    if(demux->video->bytes>=MAX_PACK_BYTES){
      MSG_ERR(MSGTR_TooManyVideoInBuffer,demux->video->packs,demux->video->bytes);
      MSG_HINT(MSGTR_MaybeNI);
      break;
    }
    if(!demux->driver){
       MSG_DBG2("ds_fill_buffer: demux->driver==NULL failed\n");
       break; // EOF
    }
    if(!demux->driver->demux(demux,ds)){
       MSG_DBG2("ds_fill_buffer: demux->driver->demux() failed\n");
       break; // EOF
    }
  }
  ds->buffer_pos=ds->buffer_size=0;
  ds->buffer=NULL;
  ds->current=NULL;
  MSG_V("ds_fill_buffer: EOF reached (stream: %s)  \n",ds==demux->audio?"audio":"video");
  ds->eof=1;
  return 0;
}

int demux_read_data(demux_stream_t *ds,unsigned char* mem,int len){
int x;
int bytes=0;
while(len>0){
  x=ds->buffer_size-ds->buffer_pos;
  if(x==0){
    if(!ds_fill_buffer(ds)) return bytes;
  } else {
    if(x>len) x=len;
    if(x<0) return bytes; /* BAD!!! sometime happens. Broken stream, driver, gcc ??? */
    if(mem) memcpy(mem+bytes,&ds->buffer[ds->buffer_pos],x);
    bytes+=x;len-=x;ds->buffer_pos+=x;
  }
}
return bytes;
}

void ds_free_packs(demux_stream_t *ds){
  Demux_Packet *dp=ds->first;
  while(dp){
    Demux_Packet *dn=dp->next;
    delete dp;
    dp=dn;
  }
  if(ds->asf_packet){
    // mp_free unfinished .asf fragments:
    delete ds->asf_packet->buffer;
    delete ds->asf_packet;
    ds->asf_packet=NULL;
  }
  ds->first=ds->last=NULL;
  ds->packs=0; // !!!!!
  ds->bytes=0;
  if(ds->current) delete ds->current;
  ds->current=NULL;
  ds->buffer=NULL;
  ds->buffer_pos=ds->buffer_size;
  ds->pts=0; ds->pts_bytes=0;
}

void ds_free_packs_until_pts(demux_stream_t *ds,float pts){
  Demux_Packet *dp=ds->first;
  unsigned packs,bytes;
  packs=bytes=0;
  while(dp){
    Demux_Packet *dn=dp->next;
    if(dp->pts >= pts) break;
    packs++;
    bytes+=dp->len;
    delete dp;
    dp=dn;
  }
  if(!dp)
  {
    if(ds->asf_packet){
	// mp_free unfinished .asf fragments:
	delete ds->asf_packet->buffer;
	delete ds->asf_packet;
	ds->asf_packet=NULL;
    }
    ds->first=ds->last=NULL;
    ds->packs=0; // !!!!!
    ds->bytes=0;
    ds->pts=0;
  }
  else
  {
    ds->first=dp;
    ds->packs-=packs;
    ds->bytes-=bytes;
    ds->pts=dp->pts;
  }
  if(ds->current) delete ds->current;
  ds->current=NULL;
  ds->buffer=NULL;
  ds->buffer_pos=ds->buffer_size;
  ds->pts_bytes=0;
}

int ds_get_packet(demux_stream_t *ds,unsigned char **start){
    while(1){
	int len;
	if(ds->buffer_pos>=ds->buffer_size){
	  if(!ds_fill_buffer(ds)){
	    // EOF
	    *start = NULL;
	    return -1;
	  }
	}
	len=ds->buffer_size-ds->buffer_pos;
	*start = &ds->buffer[ds->buffer_pos];
	ds->buffer_pos+=len;
	return len;
    }
}

int ds_get_packet_sub(demux_stream_t *ds,unsigned char **start){
    while(1){
	int len;
	if(ds->buffer_pos>=ds->buffer_size){
	  *start = NULL;
	  if(!ds->packs) return -1; // no sub
	  if(!ds_fill_buffer(ds)) return -1; // EOF
	}
	len=ds->buffer_size-ds->buffer_pos;
	*start = &ds->buffer[ds->buffer_pos];
	ds->buffer_pos+=len;
	return len;
    }
}

float ds_get_next_pts(demux_stream_t *ds) {
  demuxer_t* demux = ds->demuxer;
  while(!ds->first) {
    if(demux->audio->bytes>=MAX_PACK_BYTES){
      MSG_ERR(MSGTR_TooManyAudioInBuffer,demux->audio->packs,demux->audio->bytes);
      MSG_HINT(MSGTR_MaybeNI);
      return -1;
    }
    if(demux->video->bytes>=MAX_PACK_BYTES){
      MSG_ERR(MSGTR_TooManyVideoInBuffer,demux->video->packs,demux->video->bytes);
      MSG_HINT(MSGTR_MaybeNI);
      return -1;
    }
    if(!demux_fill_buffer(demux,ds))
      return -1;
  }
  return ds->first->pts;
}

// ====================================================================
const struct s_stream_txt_ids
{
    unsigned demuxer_id;
    unsigned stream_id;
}stream_txt_ids[]=
{
    { INFOT_AUTHOR, 	SCTRL_TXT_GET_STREAM_AUTHOR },
    { INFOT_NAME, 	SCTRL_TXT_GET_STREAM_NAME },
    { INFOT_SUBJECT, 	SCTRL_TXT_GET_STREAM_SUBJECT },
    { INFOT_COPYRIGHT, 	SCTRL_TXT_GET_STREAM_COPYRIGHT },
    { INFOT_DESCRIPTION,SCTRL_TXT_GET_STREAM_DESCRIPTION },
    { INFOT_ALBUM, 	SCTRL_TXT_GET_STREAM_ALBUM },
    { INFOT_DATE, 	SCTRL_TXT_GET_STREAM_DATE },
    { INFOT_TRACK, 	SCTRL_TXT_GET_STREAM_TRACK },
    { INFOT_GENRE, 	SCTRL_TXT_GET_STREAM_GENRE },
    { INFOT_ENCODER, 	SCTRL_TXT_GET_STREAM_ENCODER },
    { INFOT_SOURCE_MEDIA,SCTRL_TXT_GET_STREAM_SOURCE_MEDIA },
    { INFOT_RATING, 	SCTRL_TXT_GET_STREAM_RATING },
    { INFOT_COMMENTS, 	SCTRL_TXT_GET_STREAM_COMMENT },
    { INFOT_MIME, 	SCTRL_TXT_GET_STREAM_MIME }
};
static demuxer_t* demux_open_stream(stream_t *stream,int file_format,int audio_id,int video_id,int dvdsub_id)
{
    unsigned i;
    demuxer_t *demuxer=NULL,*new_demux=NULL;

    demux_aid_vid_mismatch = 0;
    i=0;
again:
    for(;ddrivers[i]!=&demux_null;i++) {
	MSG_V("Probing %s ... ",ddrivers[i]->name);
	/* don't remove it from loop!!! (for initializing) */
	demuxer = new_demuxer(stream,DEMUXER_TYPE_UNKNOWN,audio_id,video_id,dvdsub_id);
	stream_reset(demuxer->stream);
	stream_seek(demuxer->stream,demuxer->stream->start_pos);
	if(ddrivers[i]->probe(demuxer)==MPXP_Ok) {
	    MSG_V("OK\n");
	    demuxer->driver = ddrivers[i];
	    break;
	}
	MSG_V("False\n");
	FREE_DEMUXER(demuxer);
    }
    if(!demuxer || !demuxer->driver) {
	MSG_ERR(MSGTR_FormatNotRecognized);
	FREE_DEMUXER(demuxer);
	return NULL;
    }

    if(!(new_demux=demuxer->driver->open(demuxer))) {
	MSG_ERR("Can't open stream with '%s'\n", demuxer->driver->name);
	demuxer->driver=NULL;
	i++;
	goto again;
    }
    demuxer=new_demux;
    MSG_OK("Using: %s\n",demuxer->driver->name);
    for(i=0;i<sizeof(stream_txt_ids)/sizeof(struct s_stream_txt_ids);i++)
    if(!demux_info_get(demuxer,stream_txt_ids[i].demuxer_id)) {
	char stream_name[256];
	if(demuxer->stream->driver->control) {
	    if(demuxer->stream->driver->control(demuxer->stream,stream_txt_ids[i].stream_id,stream_name) == MPXP_Ok) {
		demux_info_add(demuxer,stream_txt_ids[i].demuxer_id,stream_name);
	    }
	}
    }
    stream->demuxer=demuxer;
    return demuxer;
}

static char* audio_stream = NULL;
static char* sub_stream = NULL;
static int demuxer_type = 0, audio_demuxer_type = 0, sub_demuxer_type = 0;

demuxer_t* demux_open(stream_t *vs,int file_format,int audio_id,int video_id,int dvdsub_id){
  stream_t *as = NULL,*ss = NULL;
  demuxer_t *vd,*ad = NULL,*sd = NULL;
  int afmt = 0,sfmt = 0;
  any_t* libinput=NULL;
#ifdef HAVE_STREAMIN
    libinput=vs->streaming_strl->libinput;
#endif

  if(audio_stream) {
    as = open_stream(libinput,audio_stream,&afmt,NULL);
    if(!as) {
      MSG_ERR("Can't open audio stream: %s\n",audio_stream);
      return NULL;
    }
  }
  if(sub_stream) {
    ss = open_stream(libinput,sub_stream,&sfmt,NULL);
    if(!ss) {
      MSG_ERR("Can't open subtitles stream: %s\n",sub_stream);
      return NULL;
    }
  }

  vd = demux_open_stream(vs,demuxer_type ? demuxer_type : file_format,audio_stream ? -2 : audio_id,video_id, sub_stream ? -2 : dvdsub_id);
  if(!vd)
    return NULL;
  if(as) {
    ad = demux_open_stream(as,audio_demuxer_type ? audio_demuxer_type : afmt,audio_id,-2,-2);
    if(!ad)
      MSG_WARN("Failed to open audio demuxer: %s\n",audio_stream);
    else if(ad->audio->sh && ((sh_audio_t*)ad->audio->sh)->wtag == 0x55) // MP3
      m_config_set_flag(MPXPCtx->mconfig,"mp3.hr-seek",1); // Enable high res seeking
  }
  if(ss) {
    sd = demux_open_stream(ss,sub_demuxer_type ? sub_demuxer_type : sfmt,-2,-2,dvdsub_id);
    if(!sd)
      MSG_WARN("Failed to open subtitles demuxer: %s\n",sub_stream);
  }

  if(ad && sd)
    return new_demuxers_demuxer(vd,ad,sd);
  else if(ad)
    return new_demuxers_demuxer(vd,ad,vd);
  else if(sd)
    return new_demuxers_demuxer(vd,vd,sd);
  return vd;
}

int demux_seek(demuxer_t *demuxer,const seek_args_t* seeka){
    demux_stream_t *d_audio=demuxer->audio;
    demux_stream_t *d_video=demuxer->video;
    sh_audio_t *sh_audio=reinterpret_cast<sh_audio_t*>(d_audio->sh);

    if(!(demuxer->stream->type&STREAMTYPE_SEEKABLE))
    {
	MSG_WARN("Stream is not seekable\n");
	return 0;
    }
    if(!(demuxer->flags&DEMUXF_SEEKABLE))
    {
	MSG_WARN("Demuxer is not seekable\n");
	return 0;
    }

    // clear demux buffers:
    if(sh_audio){ ds_free_packs(d_audio);sh_audio->a_buffer_len=0;}
    ds_free_packs(d_video);

    stream_set_eof(demuxer->stream,0); // clear eof flag
    demuxer->video->eof=0;
    demuxer->audio->eof=0;
    demuxer->video->prev_pts=0;
    demuxer->audio->prev_pts=0;

    if(sh_audio) sh_audio->timer=0;
    if(demuxer->driver->seek) demuxer->driver->seek(demuxer,seeka);
    else MSG_WARN("Demuxer seek error\n");
    check_pin("demuxer",demuxer->pin,DEMUX_PIN);
    return 1;
}

static const char *info_names[INFOT_MAX] =
{
    "Author",
    "Name",
    "Subject",
    "Copyright",
    "Description",
    "Album",
    "Date",
    "Track",
    "Genre",
    "Encoder",
    "SrcMedia",
    "WWW",
    "Mail",
    "Rating",
    "Comments",
    "Mime"
};

int demux_info_add(demuxer_t *demuxer, unsigned opt, const char *param)
{
    if(!opt || opt > INFOT_MAX)
    {
	MSG_WARN("Unknown info type %u\n",opt);
	return 0;
    }
    opt--;
    check_pin("demuxer",demuxer->pin,DEMUX_PIN);
    if(((demuxer_info_t *)demuxer->info)->id[opt])
    {
	MSG_V( "Demuxer info '%s' already present as '%s'!\n",info_names[opt],((demuxer_info_t *)demuxer->info)->id[opt]);
	delete ((demuxer_info_t *)demuxer->info)->id[opt];
    }
    ((demuxer_info_t *)demuxer->info)->id[opt]=nls_recode2screen_cp(sub_data.cp,param,strlen(param));
    return 1;
}

int demux_info_print(const demuxer_t *demuxer,const char *filename)
{
    unsigned i;
    MSG_HINT(" CLIP INFO (%s):\n",filename);
    for(i=0;i<INFOT_MAX;i++)
	if(((demuxer_info_t *)demuxer->info)->id[i])
	    MSG_HINT("   %s: %s\n",info_names[i],((demuxer_info_t *)demuxer->info)->id[i]);
    return 0;
}

void demux_info_free(demuxer_t* demuxer)
{
    unsigned i;
    if(demuxer->info)
    {
	demuxer_info_t*dinfo = reinterpret_cast<demuxer_info_t*>(demuxer->info);
	for(i=0;i<INFOT_MAX;i++)
	    if(dinfo->id[i])
		delete dinfo->id[i];
	delete dinfo;
    }
}

const char* demux_info_get(const demuxer_t *demuxer, unsigned opt) {
    if(!opt || opt > INFOT_MAX) return NULL;
    return ((demuxer_info_t *)demuxer->info)->id[opt-1];
}

/******************* Options stuff **********************/

static const config_t demux_opts[] = {
  { "audiofile", &audio_stream, CONF_TYPE_STRING, 0, 0, 0, "forces reading of audio-stream from other file" },
  { "subfile", &sub_stream, CONF_TYPE_STRING, 0, 0, 0, "forces reading of subtitles from other file" },
  { "type", &demuxer_type, CONF_TYPE_INT, CONF_RANGE, 1, DEMUXER_TYPE_MAX, "forces demuxer by given number" },
  { "audio", &audio_demuxer_type, CONF_TYPE_INT, CONF_RANGE, 1, DEMUXER_TYPE_MAX, "forces using of audio-demuxer" },
  { "sub", &sub_demuxer_type, CONF_TYPE_INT, CONF_RANGE, 1, DEMUXER_TYPE_MAX, "forces using of subtitle-demuxer" },
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

static const config_t demuxer_opts[] = {
  { "demuxer", (any_t*)&demux_opts, CONF_TYPE_SUBCONFIG, 0, 0, 0, "Demuxer related options" },
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

void demuxer_register_options(m_config_t* cfg) {
  m_config_register_options(cfg,demuxer_opts);
}

static MPXP_Rc demux_control(const demuxer_t *demuxer, int cmd, any_t*arg) {

    if(demuxer->driver)
	return demuxer->driver->control(demuxer,cmd,arg);
    check_pin("demuxer",demuxer->pin,DEMUX_PIN);
    return MPXP_Unknown;
}

int demuxer_switch_audio(const demuxer_t *demuxer, int id)
{
    if(id>MAX_A_STREAMS) id=0;
    if (demux_control(demuxer, DEMUX_CMD_SWITCH_AUDIO, &id) == MPXP_Unknown)
	id = demuxer->audio->id;
    check_pin("demuxer",demuxer->pin,DEMUX_PIN);
    return id;
}

int demuxer_switch_video(const demuxer_t *demuxer, int id)
{
    if(id>MAX_V_STREAMS) id=0;
    if (demux_control(demuxer, DEMUX_CMD_SWITCH_VIDEO, &id) == MPXP_Unknown)
	id = demuxer->audio->id;
    check_pin("demuxer",demuxer->pin,DEMUX_PIN);
    return id;
}

int demuxer_switch_subtitle(const demuxer_t *demuxer, int id)
{
    if(id>MAX_S_STREAMS) id=0;
    if (demux_control(demuxer, DEMUX_CMD_SWITCH_SUBS, &id) == MPXP_Unknown)
	id = demuxer->audio->id;
    check_pin("demuxer",demuxer->pin,DEMUX_PIN);
    return id;
}

