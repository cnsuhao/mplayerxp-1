#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
#include <algorithm>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "help_mp.h"

#include "mplayerxp.h"
#include "xmpcore/xmp_core.h"

#include "libmpstream/stream.h"
#include "libmpdemux/demuxer.h"
#include "libmpdemux/stheader.h"
#include "libmpconf/codec-cfg.h"

#include "dec_audio.h"
#include "libao2/afmt.h"
#include "libao2/audio_out.h"
#include "mplayerxp.h"
#include "libmpdemux/demuxer_r.h"
#include "postproc/af.h"
#include "osdep/fastmemcpy.h"
#include "ad_msg.h"

/* used for ac3surround decoder - set using -channels option */
af_cfg_t af_cfg; // Configuration for audio filters

typedef struct priv_s {
    sh_audio_t* parent;
    const ad_functions_t* mpadec;
}priv_t;

any_t* mpca_init(sh_audio_t *sh_audio)
{
    const char *afm=NULL,*ac=NULL;
    const audio_probe_t* aprobe=NULL;
    priv_t* priv = new(zeromem) priv_t;
    priv->parent=sh_audio;
    if(!sh_audio->codec) {
	if(mp_conf.audio_family) {
	    afm=mp_conf.audio_family;
	    ac=afm;
	    priv->mpadec=afm_find_driver(afm);
	    if(priv->mpadec) aprobe=priv->mpadec->probe(sh_audio,sh_audio->wtag);
	}
	else aprobe = afm_probe_driver(sh_audio);
    }
    if(aprobe) {
	afm=aprobe->driver;
	ac=aprobe->codec_dll;
	/* fake struct codecs_st*/
	sh_audio->codec=new(zeromem) struct codecs_st;
	strcpy(sh_audio->codec->dll_name,aprobe->codec_dll);
	strcpy(sh_audio->codec->driver_name,aprobe->driver);
	strcpy(sh_audio->codec->codec_name,sh_audio->codec->dll_name);
	memcpy(sh_audio->codec->outfmt,aprobe->sample_fmt,sizeof(aprobe->sample_fmt));
	priv->mpadec=afm_find_driver(afm);
    }
    else if(sh_audio->codec) {
	afm=sh_audio->codec->driver_name;
	ac=sh_audio->codec->codec_name;
	priv->mpadec=afm_find_driver(afm);
    }
    if(!priv->mpadec){
	MSG_ERR(MSGTR_CODEC_BAD_AFAMILY,ac, afm);
	delete priv;
	return NULL; // no such driver
    }

    /* reset in/out buffer size/pointer: */
    sh_audio->a_buffer_size=0;
    sh_audio->a_buffer=NULL;
    sh_audio->a_in_buffer_size=0;
    sh_audio->a_in_buffer=NULL;

    /* Set up some common usefull defaults. ad->preinit() can override these: */
#ifdef WORDS_BIGENDIAN
    sh_audio->afmt=AFMT_S16_BE;
#else
    sh_audio->afmt=AFMT_S16_LE;
#endif
    sh_audio->rate=0;
    sh_audio->o_bps=0;
    if(sh_audio->wf) /* NK: We need to know i_bps before its detection by codecs param */
	sh_audio->i_bps=sh_audio->wf->nAvgBytesPerSec;

    sh_audio->audio_out_minsize=8192;/* default size, maybe not enough for Win32/ACM*/
    sh_audio->audio_in_minsize=0;

    if(priv->mpadec->preinit(sh_audio)!=MPXP_Ok) {
	MSG_ERR(MSGTR_CODEC_CANT_PREINITA);
	delete priv;
	return NULL;
    }

    /* allocate audio in buffer: */
    if(sh_audio->audio_in_minsize>0){
	sh_audio->a_in_buffer_size=sh_audio->audio_in_minsize;
	MSG_V("dec_audio: Allocating %d bytes for input buffer\n",
	    sh_audio->a_in_buffer_size);
	sh_audio->a_in_buffer=new char [sh_audio->a_in_buffer_size];
	sh_audio->a_in_buffer_len=0;
    }

    /* allocate audio out buffer: */
    sh_audio->a_buffer_size=sh_audio->audio_out_minsize+MAX_OUTBURST; /* worst case calc.*/

    MSG_V("dec_audio: Allocating %d + %d = %d bytes for output buffer\n",
	sh_audio->audio_out_minsize,MAX_OUTBURST,sh_audio->a_buffer_size);

    sh_audio->a_buffer=new char [sh_audio->a_buffer_size];
    if(!sh_audio->a_buffer) {
	MSG_ERR(MSGTR_CantAllocAudioBuf);
	delete priv;
	return NULL;
    }
    sh_audio->a_buffer_len=0;

    if(priv->mpadec->init(sh_audio)!=MPXP_Ok){
	MSG_WARN(MSGTR_CODEC_CANT_INITA);
	mpca_uninit(priv); /* mp_free buffers */
	return NULL;
    }

    sh_audio->inited=1;

    if(!sh_audio->nch || !sh_audio->rate) {
	MSG_WARN(MSGTR_UnknownAudio);
	mpca_uninit(priv); /* mp_free buffers */
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
    } else if(xp_core->initial_apts_corr.need_correction==1) {
	xp_core->initial_apts += ((float)(xp_core->initial_apts_corr.pts_bytes-xp_core->initial_apts_corr.nbytes))/(float)sh_audio->i_bps;
	xp_core->initial_apts_corr.need_correction=0;
    }
    MSG_OK("[AC] %s decoder: [%s] drv:%s.%s ratio %i->%i\n",mp_conf.audio_codec?"Forcing":"Selecting"
	,ac
	,priv->mpadec->info->driver_name
	,ac
	,sh_audio->i_bps,sh_audio->o_bps);
    if(sh_audio->codec) { delete sh_audio->codec; sh_audio->codec=NULL; }
    return priv;
}

void mpca_uninit(any_t *opaque)
{
    priv_t* priv = (priv_t*)opaque;
    sh_audio_t* parent = priv->parent;
    if(!parent) { delete priv; return; }
    if(priv->parent->afilter){
	MSG_V("Uninit audio filters...\n");
	af_uninit(parent->afilter);
	delete parent->afilter;
	parent->afilter=NULL;
    }
    if(parent->a_buffer) delete parent->a_buffer;
    parent->a_buffer=NULL;
    if(parent->a_in_buffer) delete parent->a_in_buffer;
    parent->a_in_buffer=NULL;
    if(!parent->inited) { delete priv; return; }
    MSG_V("uninit audio: ...\n");
    priv->mpadec->uninit(parent);
    if(parent->a_buffer) delete parent->a_buffer;
    parent->a_buffer=NULL;
    parent->inited=0;
    delete priv;
}

 /* Init audio filters */
MPXP_Rc mpca_preinit_filters(sh_audio_t *sh_audio,
	unsigned in_samplerate, unsigned in_channels, unsigned in_format,
	unsigned* out_samplerate, unsigned* out_channels, unsigned* out_format){
    char strbuf[200];
    af_stream_t* afs=af_new(sh_audio);

    // input format: same as codec's output format:
    afs->input.rate   = in_samplerate;
    afs->input.nch    = in_channels;
    afs->input.format = afmt2mpaf(in_format);

    // output format: same as ao driver's input format (if missing, fallback to input)
    afs->output.rate   = *out_samplerate ? *out_samplerate : afs->input.rate;
    afs->output.nch    = *out_channels ? *out_channels : afs->input.nch;
    if(*out_format)	afs->output.format = afmt2mpaf(*out_format);
    else		afs->output.format = afs->input.format;

    // filter config:
    memcpy(&afs->cfg,&af_cfg,sizeof(af_cfg_t));

    MSG_V("Checking audio filter chain for %dHz/%dch/%dbit...\n",
	afs->input.rate,afs->input.nch,(afs->input.format&MPAF_BPS_MASK)*8);

    // let's autoprobe it!
    if(MPXP_Ok != af_init(afs,0)){
	delete afs;
	return MPXP_False; // failed :(
    }

    *out_samplerate=afs->output.rate;
    *out_channels=afs->output.nch;
    *out_format=mpaf2afmt(afs->output.format);

    sh_audio->af_bps = afs->output.rate*afs->output.nch*(afs->output.format&MPAF_BPS_MASK);

    MSG_V("AF_pre: af format: %d ch, %d hz, %s af_bps=%i\n",
	afs->output.nch, afs->output.rate,
	mpaf_fmt2str(afs->output.format,strbuf,200),
	sh_audio->af_bps);

    sh_audio->afilter=afs;
    return MPXP_Ok;
}

 /* Init audio filters */
MPXP_Rc mpca_init_filters(sh_audio_t *sh_audio,
	unsigned in_samplerate, unsigned in_channels, mpaf_format_e in_format,
	unsigned out_samplerate, unsigned out_channels, mpaf_format_e out_format,
	unsigned out_minsize, unsigned out_maxsize){
    char strbuf[200];
    af_stream_t* afs=sh_audio->afilter;
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

    MSG_V("Building audio filter chain for %dHz/%dch/%dbit (%s) -> %dHz/%dch/%dbit (%s)...\n",
	afs->input.rate,afs->input.nch,(afs->input.format&MPAF_BPS_MASK)*8,ao_format_name(mpaf2afmt(afs->input.format)),
	afs->output.rate,afs->output.nch,(afs->output.format&MPAF_BPS_MASK)*8,ao_format_name(mpaf2afmt(afs->output.format)));

    // let's autoprobe it!
    if(MPXP_Ok != af_init(afs,1)){
	sh_audio->afilter=NULL;
	delete afs;
	return MPXP_False; // failed :(
    }

    // allocate the a_out_* buffers:
    if(out_maxsize<out_minsize) out_maxsize=out_minsize;
    if(out_maxsize<8192) out_maxsize=MAX_OUTBURST; // not sure this is ok

    sh_audio->af_bps = afs->output.rate*afs->output.nch*(afs->output.format&MPAF_BPS_MASK);

    MSG_V("AF_init: af format: %d ch, %d hz, %s af_bps=%i\n",
	afs->output.nch, afs->output.rate,
	mpaf_fmt2str(afs->output.format,strbuf,200),
	sh_audio->af_bps);

    sh_audio->a_buffer_size=out_maxsize;
    sh_audio->a_buffer=new char [sh_audio->a_buffer_size];
    sh_audio->a_buffer_len=0;

    af_showconf(afs->first);
    sh_audio->afilter=afs;
    sh_audio->afilter_inited=1;
    return MPXP_Ok;
}

 /* Init audio filters */
MPXP_Rc mpca_reinit_filters(sh_audio_t *sh_audio,
	unsigned in_samplerate, unsigned in_channels, mpaf_format_e in_format,
	unsigned out_samplerate, unsigned out_channels, mpaf_format_e out_format,
	unsigned out_minsize, unsigned out_maxsize)
{
    if(sh_audio->afilter){
	MSG_V("Uninit audio filters...\n");
	af_uninit(sh_audio->afilter);
	delete sh_audio->afilter;
	sh_audio->afilter=NULL;
    }
    return mpca_init_filters(sh_audio,in_samplerate,in_channels,
				in_format,out_samplerate,
				out_channels,out_format,
				out_minsize,out_maxsize);
}

unsigned mpca_decode(any_t *opaque,unsigned char *buf,unsigned minlen,unsigned maxlen,unsigned buflen,float *pts)
{
    priv_t* priv = (priv_t*)opaque;
    sh_audio_t* sh_audio = priv->parent;
    unsigned len;
    unsigned cp_size,cp_tile;

    if(!sh_audio->inited) return 0; // no codec
    MSG_DBG3("mpca_decode(%p,%p,%i,%i,%i,%p)\n",sh_audio,buf,minlen,maxlen,buflen,pts);

    if(minlen>maxlen) MSG_WARN(MSGTR_CODEC_XP_INT_ERR,minlen,maxlen);
    if(sh_audio->af_buffer_len) {
	cp_size=std::min(buflen,sh_audio->af_buffer_len);
	memcpy(buf,sh_audio->af_buffer,cp_size);
	*pts = sh_audio->af_pts;
	cp_tile=sh_audio->af_buffer_len-cp_size;
	MSG_DBG2("cache->buf %i bytes %f pts <PREDICTED PTS %f>\n",cp_size,*pts,*pts+(float)cp_tile/(float)sh_audio->af_bps);
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
    len=priv->mpadec->decode(sh_audio,buf, minlen, maxlen,pts);
    if(len>buflen) MSG_WARN(MSGTR_CODEC_BUF_OVERFLOW,sh_audio->codec->driver_name,len,buflen);
    MSG_DBG2("decaudio: %i bytes %f pts min %i max %i buflen %i o_bps=%i f_bps=%i\n",len,*pts,minlen,maxlen,buflen,sh_audio->o_bps,sh_audio->af_bps);
    if(len==0 || !sh_audio->afilter) return 0; // EOF?
    // run the filters:
    mp_aframe_t*  afd;  // filter input
    mp_aframe_t* pafd; // filter output
    afd=new_mp_aframe(	sh_audio->rate,
			sh_audio->nch,
			afmt2mpaf(sh_audio->afmt)
			,0); // xp_idx
    afd->audio=buf;
    afd->len=len;
    pafd=af_play(sh_audio->afilter,afd);
    afd->audio=NULL; // fake no buffer

    if(!pafd) {
	MSG_V("decaudio: filter error\n");
	return 0; // error
    }

    MSG_DBG2("decaudio: %X in=%d out=%d (min %d max %d buf %d)\n",
	pafd->format,len, pafd->len, minlen, maxlen, buflen);

    cp_size=pafd->len;
    cp_size=std::min(buflen,pafd->len);
    memcpy(buf,pafd->audio,cp_size);
    cp_tile=pafd->len-cp_size;
    if(cp_tile) {
	sh_audio->af_buffer=&((char *)pafd->audio)[cp_size];
	sh_audio->af_buffer_len=cp_tile;
	sh_audio->af_pts = *pts+(float)cp_size/(float)sh_audio->af_bps;
	MSG_DBG2("decaudio: afilter->cache %i bytes %f pts\n",cp_tile,*pts);
    } else sh_audio->af_buffer_len=0;
    if(pafd!=afd) free_mp_aframe(pafd);
    free_mp_aframe(afd);
    return cp_size;
}

/* Note: it is called once after seeking, to resync. */
void mpca_resync_stream(any_t *opaque)
{
    priv_t* priv = (priv_t*)opaque;
    sh_audio_t* sh_audio = priv->parent;
    if(sh_audio) {
	sh_audio->a_in_buffer_len=0; /* workaround */
	if(sh_audio->inited && priv->mpadec) priv->mpadec->control_ad(sh_audio,ADCTRL_RESYNC_STREAM,NULL);
    }
}

/* Note: it is called to skip (jump over) small amount (1/10 sec or 1 frame)
   of audio data - used to sync audio to video after seeking */
void mpca_skip_frame(any_t *opaque)
{
    priv_t* priv = (priv_t*)opaque;
    sh_audio_t* sh_audio = priv->parent;
    MPXP_Rc rc=MPXP_True;
    if(sh_audio)
    if(sh_audio->inited && priv->mpadec) rc=priv->mpadec->control_ad(sh_audio,ADCTRL_SKIP_FRAME,NULL);
    if(rc!=MPXP_True) sh_audio->ds->fill_buffer();
}
