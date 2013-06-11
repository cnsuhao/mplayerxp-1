#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
#include <algorithm>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "mpxp_help.h"

#include "mplayerxp.h"
#include "xmpcore/xmp_core.h"

#include "libmpstream2/stream.h"
#include "libmpdemux/demuxer.h"
#include "libmpdemux/stheader.h"
#include "libmpconf/codec-cfg.h"

#include "dec_audio.h"
#include "libao3/afmt.h"
#include "libao3/audio_out.h"
#include "mplayerxp.h"
#include "libmpdemux/demuxer_r.h"
#include "postproc/af.h"
#include "osdep/fastmemcpy.h"
#include "ad_msg.h"

namespace	usr {
/* used for ac3surround decoder - set using -channels option */
af_cfg_t af_cfg; // Configuration for audio filters

struct decaudio_priv_t : public Opaque {
    public:
	decaudio_priv_t();
	virtual ~decaudio_priv_t();

	sh_audio_t* parent;
	Audio_Decoder* mpadec;
	audio_filter_info_t* afi;
};

decaudio_priv_t::decaudio_priv_t()
		:afi(new(zeromem) audio_filter_info_t)
{
}

decaudio_priv_t::~decaudio_priv_t() { delete afi; }

audio_decoder_t* mpca_init(sh_audio_t *sh_audio)
{
    const char *afm=NULL,*ac=NULL;
    audio_decoder_t* handle=new(zeromem) audio_decoder_t;
    decaudio_priv_t* priv = new(zeromem) decaudio_priv_t;
    priv->parent=sh_audio;
    handle->ad_private=priv;

    if(mp_conf.audio_family) afm=mp_conf.audio_family;

    if(afm) {
	const ad_info_t* ai=afm_find_driver(afm);
	try{
	    /* Set up some common usefull defaults. ad->preinit() can override these: */
#ifdef WORDS_BIGENDIAN
	    sh_audio->afmt=AFMT_S16_BE;
#else
	    sh_audio->afmt=AFMT_S16_LE;
#endif
	    sh_audio->rate=0;
	    sh_audio->o_bps=0;
	    priv->mpadec=ai->query_interface(*sh_audio,*priv->afi,sh_audio->wtag);
	} catch(const bad_format_exception&) {
	    MSG_ERR(MSGTR_CODEC_BAD_AFAMILY,ac, afm);
	    goto bye;
	}
    }
    else priv->mpadec = afm_probe_driver(*sh_audio,*priv->afi);
    if(!priv->mpadec) {
bye:
	delete handle;
	delete priv->mpadec;
	delete priv;
	return NULL;
    }

    audio_probe_t aprobe=priv->mpadec->get_probe_information();
    afm=aprobe.driver;
    ac=aprobe.codec_dll;
    /* fake struct codecs_st*/
    sh_audio->codec=new(zeromem) struct codecs_st;
    strcpy(sh_audio->codec->dll_name,aprobe.codec_dll);
    strcpy(sh_audio->codec->driver_name,aprobe.driver);
    strcpy(sh_audio->codec->codec_name,sh_audio->codec->dll_name);
    memcpy(sh_audio->codec->outfmt,aprobe.sample_fmt,sizeof(aprobe.sample_fmt));

    /* reset in/out buffer size/pointer: */
    sh_audio->a_buffer_size=0;
    sh_audio->a_buffer=NULL;
    sh_audio->a_in_buffer_size=0;
    sh_audio->a_in_buffer=NULL;

    if(sh_audio->wf) /* NK: We need to know i_bps before its detection by codecs param */
	sh_audio->i_bps=sh_audio->wf->nAvgBytesPerSec;

    sh_audio->audio_out_minsize=8192;/* default size, maybe not enough for Win32/ACM*/
    sh_audio->audio_in_minsize=0;

    /* allocate audio in buffer: */
    if(sh_audio->audio_in_minsize>0){
	sh_audio->a_in_buffer_size=sh_audio->audio_in_minsize;
	mpxp_v<<"dec_audio: Allocating "<<sh_audio->a_in_buffer_size<<" bytes for input buffer"<<std::endl;
	sh_audio->a_in_buffer=new char [sh_audio->a_in_buffer_size];
	sh_audio->a_in_buffer_len=0;
    }

    /* allocate audio out buffer: */
    sh_audio->a_buffer_size=sh_audio->audio_out_minsize+MAX_OUTBURST; /* worst case calc.*/

    mpxp_v<<"dec_audio: Allocating "<<sh_audio->audio_out_minsize<<" + "<<MAX_OUTBURST
	<<" = "<<sh_audio->a_buffer_size<<" bytes for output buffer"<<std::endl;

    sh_audio->a_buffer=new char [sh_audio->a_buffer_size];
    if(!sh_audio->a_buffer) {
	MSG_ERR(MSGTR_CantAllocAudioBuf);
	delete priv;
	delete handle;
	return NULL;
    }
    sh_audio->a_buffer_len=0;
    sh_audio->inited=1;

    if(!sh_audio->nch || !sh_audio->rate) {
	mpxp_v<<"audio format wrong: nch="<<sh_audio->nch<<" rate="<<sh_audio->rate<<std::endl;
	MSG_WARN(MSGTR_UnknownAudio);
	mpca_uninit(*handle); /* mp_free buffers */
	return NULL;
    }

    if(!sh_audio->o_bps)
	sh_audio->o_bps=sh_audio->nch*sh_audio->rate*afmt2bps(sh_audio->afmt);
    if(!sh_audio->i_bps) {
	static int warned=0;
	if(!warned) {
	    warned=1;
	    MSG_WARN(MSGTR_CODEC_INITAL_AV_RESYNC);
	}
    } else if(mpxp_context().engine().xp_core->initial_apts_corr.need_correction==1) {
	mpxp_context().engine().xp_core->initial_apts += ((float)(mpxp_context().engine().xp_core->initial_apts_corr.pts_bytes-mpxp_context().engine().xp_core->initial_apts_corr.nbytes))/(float)sh_audio->i_bps;
	mpxp_context().engine().xp_core->initial_apts_corr.need_correction=0;
    }
    mpxp_ok<<"[AC] "<<(mp_conf.audio_codec?"Forcing":"Selecting")<<" drv:"<<aprobe.driver<<"."<<ac<<" ratio "<<sh_audio->i_bps<<"->"<<sh_audio->o_bps<<std::endl;
    if(sh_audio->codec) { delete sh_audio->codec; sh_audio->codec=NULL; }
    return handle;
}

void mpca_uninit(audio_decoder_t& opaque)
{
    decaudio_priv_t* priv = reinterpret_cast<decaudio_priv_t*>(opaque.ad_private);
    sh_audio_t* parent = priv->parent;
    if(!parent) { delete &opaque; delete priv; return; }
    if(priv->afi->afilter){
	mpxp_v<<"Uninit audio filters..."<<std::endl;
	af_uninit(priv->afi->afilter);
	delete priv->afi->afilter;
	priv->afi->afilter=NULL;
    }
    if(parent->a_buffer) delete parent->a_buffer;
    parent->a_buffer=NULL;
    if(parent->a_in_buffer) delete parent->a_in_buffer;
    parent->a_in_buffer=NULL;
    if(!parent->inited) { delete &opaque; delete priv; return; }
    mpxp_v<<"uninit audio: ..."<<std::endl;
    if(parent->a_buffer) delete parent->a_buffer;
    parent->a_buffer=NULL;
    parent->inited=0;
    delete priv;
    delete &opaque;
}

 /* Init audio filters */
MPXP_Rc mpca_preinit_filters(audio_decoder_t& opaque,
	unsigned in_samplerate, unsigned in_channels, unsigned in_format,
	unsigned& out_samplerate, unsigned& out_channels, unsigned& out_format){
    decaudio_priv_t* priv = reinterpret_cast<decaudio_priv_t*>(opaque.ad_private);
    sh_audio_t* sh_audio = priv->parent;
    char strbuf[200];
    af_stream_t* afs=af_new(sh_audio);

    // input format: same as codec's output format:
    afs->input.rate   = in_samplerate;
    afs->input.nch    = in_channels;
    afs->input.format = afmt2mpaf(in_format);

    // output format: same as ao driver's input format (if missing, fallback to input)
    afs->output.rate   = out_samplerate ? out_samplerate : afs->input.rate;
    afs->output.nch    = out_channels ? out_channels : afs->input.nch;
    if(out_format)	afs->output.format = afmt2mpaf(out_format);
    else		afs->output.format = afs->input.format;

    // filter config:
    memcpy(&afs->cfg,&af_cfg,sizeof(af_cfg_t));

    mpxp_v<<"Checking audio filter chain for "<<afs->input.rate<<"Hz/"<<afs->input.nch
	<<"ch/"<<((afs->input.format&MPAF_BPS_MASK)*8)<<"bit..."<<std::endl;

    // let's autoprobe it!
    if(MPXP_Ok != af_init(afs,0)){
	delete afs;
	return MPXP_False; // failed :(
    }

    out_samplerate=afs->output.rate;
    out_channels=afs->output.nch;
    out_format=mpaf2afmt(afs->output.format);

    sh_audio->af_bps = afs->output.rate*afs->output.nch*(afs->output.format&MPAF_BPS_MASK);

    mpxp_v<<"AF_pre: af format: "<<afs->output.nch<<" ch, "<<afs->output.rate<<" hz, "<<mpaf_fmt2str(afs->output.format,strbuf,200)
	<<" af_bps="<<sh_audio->af_bps<<std::endl;

    priv->afi->afilter=afs;
    return MPXP_Ok;
}

 /* Init audio filters */
MPXP_Rc mpca_init_filters(audio_decoder_t& opaque,
	unsigned in_samplerate, unsigned in_channels, mpaf_format_e in_format,
	unsigned out_samplerate, unsigned out_channels, mpaf_format_e out_format,
	unsigned out_minsize, unsigned out_maxsize){
    decaudio_priv_t* priv = reinterpret_cast<decaudio_priv_t*>(opaque.ad_private);
    sh_audio_t* sh_audio = priv->parent;
    char strbuf[200];
    af_stream_t* afs=priv->afi->afilter;
    if(!afs) afs = af_new(sh_audio);

    // input format: same as codec's output format:
    afs->input.rate   = in_samplerate;
    afs->input.nch    = in_channels;
    afs->input.format = afmt2mpaf(in_format);

    // output format: same as ao driver's input format (if missing, fallback to input)
    afs->output.rate   = out_samplerate ? out_samplerate : afs->input.rate;
    afs->output.nch    = out_channels ? out_channels : afs->input.nch;
    afs->output.format = afmt2mpaf(out_format ? out_format : afs->input.format);

    // filter config:
    memcpy(&afs->cfg,&af_cfg,sizeof(af_cfg_t));

    mpxp_v<<"Building audio filter chain for "<<afs->input.rate<<"Hz/"<<afs->input.nch<<"ch/"<<((afs->input.format&MPAF_BPS_MASK)*8)
	<<"bit ("<<ao_format_name(mpaf2afmt(afs->input.format))<<") -> "<<afs->output.rate<<"Hz/"<<afs->output.nch
	<<"ch/"<<((afs->output.format&MPAF_BPS_MASK)*8)<<"bit ("<<ao_format_name(mpaf2afmt(afs->output.format))<<")..."<<std::endl;

    // let's autoprobe it!
    if(MPXP_Ok != af_init(afs,1)){
	priv->afi->afilter=NULL;
	delete afs;
	return MPXP_False; // failed :(
    }

    // allocate the a_out_* buffers:
    if(out_maxsize<out_minsize) out_maxsize=out_minsize;
    if(out_maxsize<8192) out_maxsize=MAX_OUTBURST; // not sure this is ok

    sh_audio->af_bps = afs->output.rate*afs->output.nch*(afs->output.format&MPAF_BPS_MASK);

    mpxp_v<<"AF_init: af format: "<<afs->output.nch<<" ch, "<<afs->output.rate
	<<" hz, "<<mpaf_fmt2str(afs->output.format,strbuf,200)<<" af_bps="<<sh_audio->af_bps<<std::endl;

    sh_audio->a_buffer_size=out_maxsize;
    sh_audio->a_buffer=new char [sh_audio->a_buffer_size];
    sh_audio->a_buffer_len=0;

    af_showconf(afs->first);
    priv->afi->afilter=afs;
    priv->afi->afilter_inited=1;
    return MPXP_Ok;
}

 /* Init audio filters */
MPXP_Rc mpca_reinit_filters(audio_decoder_t& opaque,
	unsigned in_samplerate, unsigned in_channels, mpaf_format_e in_format,
	unsigned out_samplerate, unsigned out_channels, mpaf_format_e out_format,
	unsigned out_minsize, unsigned out_maxsize)
{
    decaudio_priv_t* priv = reinterpret_cast<decaudio_priv_t*>(opaque.ad_private);
    if(priv->afi->afilter){
	mpxp_v<<"Uninit audio filters..."<<std::endl;
	af_uninit(priv->afi->afilter);
	delete priv->afi->afilter;
	priv->afi->afilter=NULL;
    }
    return mpca_init_filters(opaque,in_samplerate,in_channels,
				in_format,out_samplerate,
				out_channels,out_format,
				out_minsize,out_maxsize);
}

unsigned mpca_decode(audio_decoder_t& opaque,unsigned char *buf,unsigned minlen,unsigned maxlen,unsigned buflen,float& pts)
{
    decaudio_priv_t* priv = reinterpret_cast<decaudio_priv_t*>(opaque.ad_private);
    sh_audio_t* sh_audio = priv->parent;
    unsigned len;
    unsigned cp_size,cp_tile;

    if(!sh_audio->inited) return 0; // no codec

    if(minlen>maxlen) MSG_WARN(MSGTR_CODEC_XP_INT_ERR,minlen,maxlen);
    if(sh_audio->af_buffer_len) {
	cp_size=std::min(buflen,sh_audio->af_buffer_len);
	memcpy(buf,sh_audio->af_buffer,cp_size);
	pts = sh_audio->af_pts;
	cp_tile=sh_audio->af_buffer_len-cp_size;
	mpxp_dbg2<<"cache->buf "<<cp_size<<" bytes "<<pts<<" pts <PREDICTED PTS "<<(pts+(float)cp_tile/(float)sh_audio->af_bps)<<">"<<std::endl;
	if(cp_tile) {
	    sh_audio->af_buffer=&sh_audio->af_buffer[cp_size];
	    sh_audio->af_buffer_len=cp_tile;
	    sh_audio->af_pts += (float)cp_size/(float)sh_audio->af_bps;
	}
	else sh_audio->af_buffer_len=0;
	return cp_size;
    }
    if(sh_audio->af_bps>sh_audio->o_bps)
	maxlen=std::min(maxlen,buflen*sh_audio->o_bps/sh_audio->af_bps);
    len=priv->mpadec->run(buf, minlen, maxlen,pts);
    if(len>buflen) MSG_WARN(MSGTR_CODEC_BUF_OVERFLOW,sh_audio->codec->driver_name,len,buflen);
    mpxp_dbg2<<"decaudio: "<<len<<" bytes "<<pts<<" pts min "<<minlen<<" max "<<maxlen<<" buflen "<<buflen<<" o_bps="<<sh_audio->o_bps<<" f_bps="<<sh_audio->af_bps<<std::endl;
    if(len==0 || !priv->afi->afilter) return 0; // EOF?
    // run the filters:
    mp_aframe_t*  afd;  // filter input
    mp_aframe_t* pafd; // filter output
    afd=new_mp_aframe(	sh_audio->rate,
			sh_audio->nch,
			afmt2mpaf(sh_audio->afmt)
			,0); // xp_idx
    afd->audio=buf;
    afd->len=len;
    pafd=af_play(priv->afi->afilter,afd);
    afd->audio=NULL; // fake no buffer

    if(!pafd) {
	mpxp_v<<"decaudio: filter error"<<std::endl;
	return 0; // error
    }

    mpxp_dbg2<<"decaudio: "<<std::hex<<pafd->format<<" in="<<len<<" out="<<pafd->len<<" (min "<<minlen<<" max "<<maxlen<<" buf "<<buflen<<")"<<std::endl;

    cp_size=pafd->len;
    cp_size=std::min(buflen,pafd->len);
    memcpy(buf,pafd->audio,cp_size);
    cp_tile=pafd->len-cp_size;
    if(cp_tile) {
	sh_audio->af_buffer=&((char *)pafd->audio)[cp_size];
	sh_audio->af_buffer_len=cp_tile;
	sh_audio->af_pts = pts+(float)cp_size/(float)sh_audio->af_bps;
	mpxp_dbg2<<"decaudio: afilter->cache "<<cp_tile<<" bytes "<<pts<<" pts"<<std::endl;
    } else sh_audio->af_buffer_len=0;
    if(pafd!=afd) free_mp_aframe(pafd);
    free_mp_aframe(afd);
    return cp_size;
}

/* Note: it is called once after seeking, to resync. */
void mpca_resync_stream(audio_decoder_t& opaque)
{
    decaudio_priv_t* priv = reinterpret_cast<decaudio_priv_t*>(opaque.ad_private);
    sh_audio_t* sh_audio = priv->parent;
    if(sh_audio) {
	sh_audio->a_in_buffer_len=0; /* workaround */
	if(sh_audio->inited && priv->mpadec) priv->mpadec->ctrl(ADCTRL_RESYNC_STREAM,NULL);
    }
}

/* Note: it is called to skip (jump over) small amount (1/10 sec or 1 frame)
   of audio data - used to sync audio to video after seeking */
void mpca_skip_frame(audio_decoder_t& opaque)
{
    decaudio_priv_t* priv = reinterpret_cast<decaudio_priv_t*>(opaque.ad_private);
    sh_audio_t* sh_audio = priv->parent;
    MPXP_Rc rc=MPXP_True;
    if(sh_audio)
    if(sh_audio->inited && priv->mpadec) rc=priv->mpadec->ctrl(ADCTRL_SKIP_FRAME,NULL);
    if(rc!=MPXP_True) sh_audio->ds->fill_buffer();
}
} // namespace	usr