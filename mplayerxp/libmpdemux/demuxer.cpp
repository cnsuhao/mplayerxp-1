#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
//=================== DEMUXER v2.5 =========================
#include <iomanip>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "mpxp_help.h"
#include "libmpsub/subreader.h"
#include "libmpconf/cfgparser.h"

#include "input2/input.h"
#include "osdep/fastmemcpy.h"
#include "libvo2/sub.h"
#include "libao3/afmt.h"

#include "demux_msg.h"
#include "demuxer.h"
#include "demuxer_internal.h"
#include "libmpstream2/stream.h"
#include "stheader.h"
#include "mplayerxp.h"

extern const demuxer_driver_t demux_ac3;
extern const demuxer_driver_t demux_aiff;
extern const demuxer_driver_t demux_dca;
extern const demuxer_driver_t demux_flac;
extern const demuxer_driver_t demux_mp3;
extern const demuxer_driver_t demux_musepack;
extern const demuxer_driver_t demux_snd_au;
extern const demuxer_driver_t demux_voc;
extern const demuxer_driver_t demux_wav;

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
extern const demuxer_driver_t demux_lavf;
extern const demuxer_driver_t demux_null;

namespace	usr {
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
    &demux_ac3,
    &demux_dca,
    &demux_flac,
    &demux_mp3,
    &demux_musepack,
    &demux_snd_au,
    &demux_voc,
    &demux_wav,
    &demux_mpgts,
    &demux_ty,
    &demux_lavf,
    &demux_null,
    NULL
};


struct demuxer_priv_t : public Opaque {
    public:
	demuxer_priv_t() {}
	virtual ~demuxer_priv_t() {}

	sh_audio_t*		a_streams[MAX_A_STREAMS]; /**< audio streams (sh_audio_t) for multilanguage movies */
	sh_video_t*		v_streams[MAX_V_STREAMS]; /**< video streams (sh_video_t) for multipicture movies  */
	char			s_streams[MAX_S_STREAMS]; /**< DVD's subtitles (flag) streams for multilanguage movies */
	const demuxer_driver_t*	driver;	/**< driver associated with this demuxer */
};

void libmpdemux_register_options(M_Config& cfg)
{
    unsigned i;
    for(i=0;ddrivers[i];i++) {
	if(ddrivers[i]->options)
	    cfg.register_options(ddrivers[i]->options);
	if(ddrivers[i]==&demux_null) break;
    }
}

void Demuxer::_init(Stream *_stream,int a_id,int v_id,int s_id)
{
  stream=_stream;
  movi_start=_stream->start_pos();
  movi_end=_stream->end_pos();
  movi_length=UINT_MAX;
  flags|=Seekable;
  synced=0;
  filepos=0;
  audio=new(zeromem) Demuxer_Stream(this,a_id);
  video=new(zeromem) Demuxer_Stream(this,v_id);
  sub=new(zeromem) Demuxer_Stream(this,s_id);
  _stream->reset();
  _stream->seek(stream->start_pos());
}

Demuxer::Demuxer()
	:demuxer_priv(new(zeromem) demuxer_priv_t),
	_info(new(zeromem) Demuxer_Info)
{
  pin=DEMUX_PIN;
}

Demuxer::Demuxer(Stream *_stream,int a_id,int v_id,int s_id)
	:demuxer_priv(new(zeromem) demuxer_priv_t),
	_info(new(zeromem) Demuxer_Info)
{
  pin=DEMUX_PIN;
  _init(_stream,a_id,v_id,s_id);
}

Demuxer::~Demuxer() {
    demuxer_priv_t& dpriv = static_cast<demuxer_priv_t&>(*demuxer_priv);
    unsigned i;
    mpxp_v<<"[Demuxer]: "<<"freeing demuxer at 0x"<<std::hex<<reinterpret_cast<long>(this)<<std::endl;

    if(dpriv.driver) dpriv.driver->close(this);

    // free streams:
    for(i=0;i<MAX_A_STREAMS;i++) if(dpriv.a_streams[i]) delete dpriv.a_streams[i];
    for(i=0;i<MAX_V_STREAMS;i++) if(dpriv.v_streams[i]) delete dpriv.v_streams[i];
    // free demuxers:
    delete audio; audio=NULL;
    delete video; video=NULL;
    delete sub; sub=NULL;
}

sh_audio_t* Demuxer::get_sh_audio(int id) const
{
    demuxer_priv_t& dpriv = static_cast<demuxer_priv_t&>(*demuxer_priv);
    if(id > MAX_A_STREAMS-1 || id < 0) {
	mpxp_warn<<"[Demuxer]: "<<"Requested audio stream id overflow ("<<id<<" > "<<MAX_A_STREAMS<<")"<<std::endl;
	return NULL;
    }
    check_pin("demuxer",pin,DEMUX_PIN);
    return dpriv.a_streams[id];
}

sh_audio_t* Demuxer::new_sh_audio_aid(int id,int aid) {
    demuxer_priv_t& dpriv = static_cast<demuxer_priv_t&>(*demuxer_priv);
    if(id > MAX_A_STREAMS-1 || id < 0) {
	mpxp_warn<<"[Demuxer]: "<<"Requested audio stream id overflow ("<<id<<" > "<<MAX_A_STREAMS<<")"<<std::endl;
	return NULL;
    }
    if(dpriv.a_streams[id]) {
	mpxp_warn<<MSGTR_AudioStreamRedefined<<": "<<id<<std::endl;
    } else {
	sh_audio_t *sh;
	mpxp_v<<"Demuxer: "<<"==> Found audio stream: "<<id<<std::endl;
	dpriv.a_streams[id]=new(zeromem) sh_audio_t;
	sh = dpriv.a_streams[id];
	// set some defaults
	sh->afmt=bps2afmt(2); /* PCM */
	sh->audio_out_minsize=8192;/* default size, maybe not enough for Win32/ACM*/
	  mpxp_v<<"Demuxer: "<<"ID_AUDIO_ID="<<aid<<std::endl;
    }
    dpriv.a_streams[id]->id = aid;
    check_pin("demuxer",pin,DEMUX_PIN);
    return dpriv.a_streams[id];
}

sh_video_t* Demuxer::get_sh_video(int id) const
{
    demuxer_priv_t& dpriv = static_cast<demuxer_priv_t&>(*demuxer_priv);
    if(id > MAX_V_STREAMS-1 || id < 0) {
	mpxp_warn<<"[Demuxer]: "<<"Requested video stream id overflow ("<<id<<" > "<<MAX_V_STREAMS<<")"<<std::endl;
	return NULL;
    }
    check_pin("demuxer",pin,DEMUX_PIN);
    return dpriv.v_streams[id];
}

sh_video_t* Demuxer::new_sh_video_vid(int id,int vid) {
    demuxer_priv_t& dpriv = static_cast<demuxer_priv_t&>(*demuxer_priv);
    if(id > MAX_V_STREAMS-1 || id < 0) {
	mpxp_warn<<"[Demuxer]: "<<"Requested video stream id overflow ("<<id<<" > "<<MAX_V_STREAMS<<")"<<std::endl;
	return NULL;
    }
    if(dpriv.v_streams[id]) {
	mpxp_warn<<"[Demuxer]: "<<MSGTR_VideoStreamRedefined<<": "<<id<<std::endl;
    } else {
	mpxp_v<<"[Demuxer]: "<<"==> Found video stream: "<<id<<std::endl;
	dpriv.v_streams[id]=new(zeromem) sh_video_t;
	 mpxp_v<<"[Demuxer]: "<<"ID_VIDEO_ID="<<vid<<std::endl;
    }
    dpriv.v_streams[id]->id = vid;
    check_pin("demuxer",pin,DEMUX_PIN);
    return dpriv.v_streams[id];
}

char Demuxer::get_sh_sub(int id) const
{
    demuxer_priv_t& dpriv = static_cast<demuxer_priv_t&>(*demuxer_priv);
    if(id > MAX_A_STREAMS-1 || id < 0) {
	mpxp_warn<<"[Demuxer]: "<<"Requested sub stream id overflow ("<<id<<" > "<<MAX_S_STREAMS<<")"<<std::endl;
	return NULL;
    }
    check_pin("demuxer",pin,DEMUX_PIN);
    return dpriv.s_streams[id];
}

char Demuxer::new_sh_sub(int id) {
    demuxer_priv_t& dpriv = static_cast<demuxer_priv_t&>(*demuxer_priv);
    if(id > MAX_V_STREAMS-1 || id < 0) {
	mpxp_warn<<"[Demuxer]: "<<"Requested sub stream id overflow ("<<id<<" > "<<MAX_S_STREAMS<<")"<<std::endl;
	return NULL;
    }
    if(dpriv.s_streams[id]) {
	mpxp_warn<<"[Demuxer]: "<<MSGTR_SubStreamRedefined<<": "<<id<<std::endl;
    } else {
	mpxp_v<<"[Demuxer]: "<<"==> Found video stream: "<<id<<std::endl;
	dpriv.s_streams[id]=1;
    }
    check_pin("demuxer",pin,DEMUX_PIN);
    return dpriv.s_streams[id];
}

int Demuxer::fill_buffer(Demuxer_Stream *ds){
    demuxer_priv_t& dpriv = static_cast<demuxer_priv_t&>(*demuxer_priv);
    /* Note: parameter 'ds' can be NULL! */
    return dpriv.driver->demux(this,ds);
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
};

static const demuxer_driver_t* demux_find_driver(const char *name) {
    unsigned i=0;
    for(;ddrivers[i]!=&demux_null;i++)
	if(strcmp(name,ddrivers[i]->short_name)==0) return ddrivers[i];
    return NULL;
}

static const struct mime_type_table_t {
    const char *mime_type;
    const demuxer_driver_t* driver;
} mime_type_table[] = {
    // Raw-Audio
    { "audio/PCMA", &demux_rawaudio },
    // MP3 streaming, some MP3 streaming server answer with audio/mpeg
    { "audio/mpeg", &demux_mp3 },
    // MPEG streaming
    { "video/mpeg", &demux_mpgps },
    { "video/x-mpeg", &demux_mpgps },
    { "video/x-mpeg2", &demux_mpgps },
    // AVI ??? => video/x-msvideo
    { "video/x-msvideo", &demux_avi },
    // MOV => video/quicktime
    { "video/quicktime", &demux_mov },
    // ASF
    { "audio/x-ms-wax", &demux_asf },
    { "audio/x-ms-wma", &demux_asf },
    { "video/x-ms-asf", &demux_asf },
    { "video/x-ms-afs", &demux_asf },
    { "video/x-ms-wvx", &demux_asf },
    { "video/x-ms-wmv", &demux_asf },
    { "video/x-ms-wma", &demux_asf },
    { "application/x-mms-framed", &demux_asf },
    { "application/vnd.ms.wms-hdr.asfv1", &demux_asf },
#if 0
    // Playlists
    { "video/x-ms-wmx", Demuxer::Type_PLAYLIST },
    { "video/x-ms-wvx", Demuxer::Type_PLAYLIST },
    { "audio/x-scpls", Demuxer::Type_PLAYLIST },
    { "audio/x-mpegurl", Demuxer::Type_PLAYLIST },
    { "audio/x-pls", Demuxer::Type_PLAYLIST },
#endif
    // Real Media
    { "audio/x-pn-realaudio", &demux_realaud },
    // OGG Streaming
#ifdef HAVE_LIBVORBIS
    { "application/ogg", &demux_ogg },
    { "application/x-ogg", &demux_ogg },
#endif
    // NullSoft Streaming Video
    { "video/nsv", &demux_nsv},
    { "misc/ultravox", &demux_nsv},
    { NULL, &demux_null }
};

static const demuxer_driver_t* demuxer_driver_by_name(const std::string& name)
{
    unsigned i=0;
    while(mime_type_table[i].driver!=&demux_null) {
	if(name==mime_type_table[i].mime_type) return mime_type_table[i].driver;
	i++;
    }
    return &demux_null;
}

MPXP_Rc Demuxer::open()
{
    demuxer_priv_t& dpriv = static_cast<demuxer_priv_t&>(*demuxer_priv);
    unsigned i;

    i=0;
    if(demux_conf.type) {
	const demuxer_driver_t* drv;
	drv=demux_find_driver(demux_conf.type);
	if(!drv) {
	    mpxp_err<<"[Demuxer]: "<<"Can't find demuxer driver: '"<<demux_conf.type<<"'"<<std::endl;
	    goto err_exit;
	}
	mpxp_v<<"[Demuxer]: "<<"Forcing "<<drv->name<<" ..."<<std::endl;
	/* don't remove it from loop!!! (for initializing) */
	stream->reset();
	stream->seek(stream->start_pos());
	if(drv->probe(this)!=MPXP_Ok) {
	    mpxp_err<<"[Demuxer]: "<<"Can't probe stream with driver: '"<<demux_conf.type<<"'"<<std::endl;
	    goto err_exit;
	}
	dpriv.driver = drv;
	goto force_driver;
    }
again:
    for(;ddrivers[i]!=&demux_null;i++) {
	mpxp_v<<"[Demuxer]: "<<"Probing "<<ddrivers[i]->name<<" ...:";
	/* don't remove it from loop!!! (for initializing) */
	stream->reset();
	stream->seek(stream->start_pos());
	if(ddrivers[i]->probe(this)==MPXP_Ok) {
	    mpxp_v<<"Ok"<<std::endl;
	    dpriv.driver = ddrivers[i];
	    break;
	}
	mpxp_v<<"False"<<std::endl;
    }
    if(!dpriv.driver) {
	dpriv.driver=demuxer_driver_by_name(stream->mime_type());
	if(dpriv.driver!=&demux_null) goto force_driver;
	dpriv.driver=NULL;
err_exit:
	mpxp_err<<"[Demuxer]: "<<MSGTR_FormatNotRecognized<<std::endl;
	return MPXP_False;
    }
force_driver:
    if(!(priv=dpriv.driver->open(this))) {
	mpxp_err<<"[Demuxer]: "<<"Can't open stream with '"<<dpriv.driver->name<<"'"<<std::endl;
	dpriv.driver=NULL;
	i++;
	if(demux_conf.type)	goto err_exit;
	else			goto again;
    }
    mpxp_ok<<"[Demuxer]: "<<"Using: "<<dpriv.driver->name<<std::endl;
    for(i=0;i<sizeof(stream_txt_ids)/sizeof(struct s_stream_txt_ids);i++)
    if(!info().get(stream_txt_ids[i].demuxer_id)) {
	char stream_name[256];
	if(stream->ctrl(stream_txt_ids[i].stream_id,stream_name) == MPXP_Ok) {
	    info().add(stream_txt_ids[i].demuxer_id,stream_name);
	}
    }
    return MPXP_Ok;
}

Demuxer* Demuxer::open(Stream *vs,libinput_t& libinput,int audio_id,int video_id,int dvdsub_id){
  Stream *as = NULL,*ss = NULL;
  Demuxer *vd,*ad = NULL,*sd = NULL;
  int afmt = 0,sfmt = 0;

  if(demux_conf.audio_stream) {
    as = new(zeromem) Stream();
    if(as->open(libinput,demux_conf.audio_stream,&afmt)!=MPXP_Ok) {
      mpxp_err<<"[Demuxer]: "<<"Can't open audio stream: "<<demux_conf.audio_stream<<std::endl;
      delete as;
      return NULL;
    }
  }
  if(demux_conf.sub_stream) {
    ss = new(zeromem) Stream();
    if(ss->open(libinput,demux_conf.sub_stream,&sfmt)!=MPXP_Ok) {
      mpxp_err<<"[Demuxer]: "<<"Can't open subtitles stream: "<<demux_conf.sub_stream<<std::endl;
      delete ss;
      return NULL;
    }
  }

  vd = new(zeromem) Demuxer(vs,audio_id,video_id,dvdsub_id);
  if(vd->open()!=MPXP_Ok) {
    delete vd;
    return NULL;
  }
  if(as) {
    ad = new(zeromem) Demuxer(as,audio_id,-2,-2);
    if(ad->open()!=MPXP_Ok) {
      mpxp_warn<<"[Demuxer]: "<<"Failed to open audio demuxer: "<<demux_conf.audio_stream<<std::endl;
      delete ad; ad = NULL;
    }
    else if(ad->audio->sh && ((sh_audio_t*)ad->audio->sh)->wtag == 0x55) // MP3
      mpxp_context().mconfig->set_flag("mp3.hr-seek",1); // Enable high res seeking
  }
  if(ss) {
    sd = new(zeromem) Demuxer(ss,-2,-2,dvdsub_id);
    if(sd->open()!=MPXP_Ok) {
      mpxp_warn<<"[Demuxer]: "<<"Failed to open subtitles demuxer: "<<demux_conf.sub_stream<<std::endl;
      delete sd; sd = NULL;
    }
  }

  if(ad && sd)
    return new_demuxers_demuxer(vd,ad,sd);
  else if(ad)
    return new_demuxers_demuxer(vd,ad,vd);
  else if(sd)
    return new_demuxers_demuxer(vd,vd,sd);
  return vd;
}

int Demuxer::seek(const seek_args_t* seeka){
    demuxer_priv_t& dpriv = static_cast<demuxer_priv_t&>(*demuxer_priv);
    sh_audio_t *sh_audio=reinterpret_cast<sh_audio_t*>(audio->sh);

    if(!(stream->type()&Stream::Type_Seekable))
    {
	mpxp_warn<<"[Demuxer]: "<<"Stream is not seekable"<<std::endl;
	return 0;
    }
    if(!(flags&Seekable))
    {
	mpxp_warn<<"[Demuxer]: "<<"Demuxer is not seekable"<<std::endl;
	return 0;
    }

    // clear demux buffers:
    if(sh_audio) { audio->free_packs(); sh_audio->a_buffer_len=0;}
    video->free_packs();

    stream->eof(0); // clear eof flag
    video->eof=0;
    audio->eof=0;
    video->prev_pts=0;
    audio->prev_pts=0;

    if(sh_audio) sh_audio->timer=0;
    if(dpriv.driver->seek) dpriv.driver->seek(this,seeka);
    else mpxp_warn<<"[Demuxer]: "<<"seek error"<<std::endl;
    check_pin("demuxer",pin,DEMUX_PIN);
    return 1;
}

/******************* Options stuff **********************/

static const mpxp_option_t demux_opts[] = {
  { "audiofile", &demux_conf.audio_stream, CONF_TYPE_STRING, 0, 0, 0, "forces reading of audio-stream from other file" },
  { "subfile", &demux_conf.sub_stream, CONF_TYPE_STRING, 0, 0, 0, "forces reading of subtitles from other file" },
  { "type", &demux_conf.type, CONF_TYPE_STRING, 0, 0, 0, "forces demuxer by given name" },
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

static const mpxp_option_t demuxer_opts[] = {
  { "demuxer", (any_t*)&demux_opts, CONF_TYPE_SUBCONFIG, 0, 0, 0, "Demuxer related options" },
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

void demuxer_register_options(M_Config& cfg) {
    cfg.register_options(demuxer_opts);
}

int Demuxer::demux(Demuxer_Stream* ds) {
    demuxer_priv_t& dpriv = static_cast<demuxer_priv_t&>(*demuxer_priv);
    if(dpriv.driver) return dpriv.driver->demux(this,ds);
    return 0;
}

MPXP_Rc Demuxer::ctrl(int cmd, any_t*arg) const {

    demuxer_priv_t& dpriv = static_cast<demuxer_priv_t&>(*demuxer_priv);
    if(dpriv.driver)
	return dpriv.driver->control(this,cmd,arg);
    check_pin("demuxer",pin,DEMUX_PIN);
    return MPXP_Unknown;
}

int Demuxer::switch_audio(int id) const
{
    if(id>MAX_A_STREAMS) id=0;
    if (ctrl(Demuxer::Switch_Audio, &id) == MPXP_Unknown)
	id = audio->id;
    check_pin("demuxer",pin,DEMUX_PIN);
    return id;
}

int Demuxer::switch_video(int id) const
{
    if(id>MAX_V_STREAMS) id=0;
    if (ctrl(Demuxer::Switch_Video, &id) == MPXP_Unknown)
	id = audio->id;
    check_pin("demuxer",pin,DEMUX_PIN);
    return id;
}

int Demuxer::switch_subtitle(int id) const
{
    if(id>MAX_S_STREAMS) id=0;
    if (ctrl(Demuxer::Switch_Subs, &id) == MPXP_Unknown)
	id = audio->id;
    check_pin("demuxer",pin,DEMUX_PIN);
    return id;
}

} // namespace	usr
