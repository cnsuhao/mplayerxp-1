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

#include "osdep/fastmemcpy.h"
#include "libvo/sub.h"
#include "libao2/afmt.h"

#include "demux_msg.h"
#include "demuxer.h"
#include "demuxer_internal.h"
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
extern const demuxer_driver_t demux_realaud;
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
    &demux_realaud,
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

void libmpdemux_register_options(m_config_t* cfg)
{
    unsigned i;
    for(i=0;ddrivers[i];i++) {
	if(ddrivers[i]->options)
	    m_config_register_options(cfg,ddrivers[i]->options);
	if(ddrivers[i]==&demux_null) break;
    }
}

demuxer_t* new_demuxer(stream_t *stream,int a_id,int v_id,int s_id){
  demuxer_t *d=new(zeromem) demuxer_t;
  fill_false_pointers(d->antiviral_hole,reinterpret_cast<long>(&d->pin)-reinterpret_cast<long>(&d->antiviral_hole));
  d->pin=DEMUX_PIN;
  d->stream=stream;
  d->movi_start=stream->start_pos;
  d->movi_end=stream->end_pos;
  d->movi_length=UINT_MAX;
  d->flags|=DEMUXF_SEEKABLE;
  d->synced=0;
  d->filepos=0;
  d->audio=new(zeromem) Demuxer_Stream(d,a_id);
  d->video=new(zeromem) Demuxer_Stream(d,v_id);
  d->sub=new(zeromem) Demuxer_Stream(d,s_id);
  stream_reset(stream);
  stream_seek(stream,stream->start_pos);
  return d;
}

demuxer_t::demuxer_t()
	:_info(new(zeromem) Demuxer_Info)
{
}

demuxer_t::~demuxer_t() {}

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
    ((sh_audio_t *)demuxer->a_streams[id])->id = aid;
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
    ((sh_video_t *)demuxer->v_streams[id])->id = vid;
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

	// free streams:
	for(i=0;i<MAX_A_STREAMS;i++) if(demuxer->a_streams[i]) free_sh_audio(reinterpret_cast<sh_audio_t*>(demuxer->a_streams[i]));
	for(i=0;i<MAX_V_STREAMS;i++) if(demuxer->v_streams[i]) free_sh_video(reinterpret_cast<sh_video_t*>(demuxer->v_streams[i]));
	// free demuxers:
	delete demuxer->audio; demuxer->audio=NULL;
	delete demuxer->video; demuxer->video=NULL;
	delete demuxer->sub; demuxer->sub=NULL;
	delete demuxer;
    }
}

int demux_fill_buffer(demuxer_t *demux,Demuxer_Stream *ds){
    /* Note: parameter 'ds' can be NULL! */
    return demux->driver->demux(demux,ds);
}

// ====================================================================
struct demux_conf {
    const char* audio_stream;
    const char* sub_stream;
    const char* type;
};
static demux_conf demux_conf;

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

static const demuxer_driver_t* demux_find_driver(const char *name) {
    unsigned i=0;
    for(;ddrivers[i]!=&demux_null;i++)
	if(strcmp(name,ddrivers[i]->short_name)==0) return ddrivers[i];
    return NULL;
}

static demuxer_t* demux_open_stream(stream_t *stream,int audio_id,int video_id,int dvdsub_id)
{
    unsigned i;
    demuxer_t *demuxer=NULL,*new_demux=NULL;

    i=0;
    if(demux_conf.type) {
	const demuxer_driver_t* drv;
	drv=demux_find_driver(demux_conf.type);
	if(!drv) {
	    MSG_ERR("Can't find demuxer driver: '%s'\n",demux_conf.type);
	    goto err_exit;
	}
	MSG_V("Forcing %s ... ",drv->name);
	/* don't remove it from loop!!! (for initializing) */
	demuxer = new_demuxer(stream,audio_id,video_id,dvdsub_id);
	stream_reset(demuxer->stream);
	stream_seek(demuxer->stream,demuxer->stream->start_pos);
	if(drv->probe(demuxer)!=MPXP_Ok) {
	    MSG_ERR("Can't probe stream with driver: '%s'\n",demux_conf.type);
	    goto err_exit;
	}
	demuxer->driver = drv;
	goto force_driver;
    }
again:
    for(;ddrivers[i]!=&demux_null;i++) {
	MSG_V("Probing %s ... ",ddrivers[i]->name);
	/* don't remove it from loop!!! (for initializing) */
	demuxer = new_demuxer(stream,audio_id,video_id,dvdsub_id);
	stream_reset(demuxer->stream);
	stream_seek(demuxer->stream,demuxer->stream->start_pos);
	if(ddrivers[i]->probe(demuxer)==MPXP_Ok) {
	    MSG_V("OK\n");
	    demuxer->driver = ddrivers[i];
	    break;
	}
	MSG_V("False\n");
	free_demuxer(demuxer); demuxer=NULL;
    }
    if(!demuxer || !demuxer->driver) {
err_exit:
	MSG_ERR(MSGTR_FormatNotRecognized);
	if(demuxer) { free_demuxer(demuxer); demuxer=NULL; }
	return NULL;
    }
force_driver:
    if(!(new_demux=demuxer->driver->open(demuxer))) {
	MSG_ERR("Can't open stream with '%s'\n", demuxer->driver->name);
	demuxer->driver=NULL;
	i++;
	if(demux_conf.type)	goto err_exit;
	else			goto again;
    }
    demuxer=new_demux;
    MSG_OK("Using: %s\n",demuxer->driver->name);
    for(i=0;i<sizeof(stream_txt_ids)/sizeof(struct s_stream_txt_ids);i++)
    if(!demuxer->info().get(stream_txt_ids[i].demuxer_id)) {
	char stream_name[256];
	if(stream_control(demuxer->stream,stream_txt_ids[i].stream_id,stream_name) == MPXP_Ok) {
		demuxer->info().add(stream_txt_ids[i].demuxer_id,stream_name);
	}
    }
    stream->demuxer=demuxer;
    return demuxer;
}

demuxer_t* demux_open(stream_t *vs,int audio_id,int video_id,int dvdsub_id){
  stream_t *as = NULL,*ss = NULL;
  demuxer_t *vd,*ad = NULL,*sd = NULL;
  int afmt = 0,sfmt = 0;
  any_t* libinput=NULL;
#ifdef HAVE_STREAMIN
    libinput=vs->streaming_strl->libinput;
#endif

  if(demux_conf.audio_stream) {
    as = open_stream(libinput,demux_conf.audio_stream,&afmt,NULL);
    if(!as) {
      MSG_ERR("Can't open audio stream: %s\n",demux_conf.audio_stream);
      return NULL;
    }
  }
  if(demux_conf.sub_stream) {
    ss = open_stream(libinput,demux_conf.sub_stream,&sfmt,NULL);
    if(!ss) {
      MSG_ERR("Can't open subtitles stream: %s\n",demux_conf.sub_stream);
      return NULL;
    }
  }

  vd = demux_open_stream(vs,audio_id,video_id,dvdsub_id);
  if(!vd)
    return NULL;
  if(as) {
    ad = demux_open_stream(as,audio_id,-2,-2);
    if(!ad)
      MSG_WARN("Failed to open audio demuxer: %s\n",demux_conf.audio_stream);
    else if(ad->audio->sh && ((sh_audio_t*)ad->audio->sh)->wtag == 0x55) // MP3
      m_config_set_flag(mpxp_context().mconfig,"mp3.hr-seek",1); // Enable high res seeking
  }
  if(ss) {
    sd = demux_open_stream(ss,-2,-2,dvdsub_id);
    if(!sd)
      MSG_WARN("Failed to open subtitles demuxer: %s\n",demux_conf.sub_stream);
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
    Demuxer_Stream* d_audio=demuxer->audio;
    Demuxer_Stream* d_video=demuxer->video;
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
    if(sh_audio) { d_audio->free_packs(); sh_audio->a_buffer_len=0;}
    d_video->free_packs();

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

/******************* Options stuff **********************/

static const config_t demux_opts[] = {
  { "audiofile", &demux_conf.audio_stream, CONF_TYPE_STRING, 0, 0, 0, "forces reading of audio-stream from other file" },
  { "subfile", &demux_conf.sub_stream, CONF_TYPE_STRING, 0, 0, 0, "forces reading of subtitles from other file" },
  { "type", &demux_conf.type, CONF_TYPE_STRING, 0, 0, 0, "forces demuxer by given name" },
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

