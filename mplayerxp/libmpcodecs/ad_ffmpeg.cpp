#include <stdio.h>
#include <stdlib.h>
#define __USE_GNU 1
#define __USE_XOPEN 1
#include <unistd.h>
#include <assert.h>
#include <dlfcn.h> /* GLIBC specific. Exists under cygwin too! */
#include "libao2/afmt.h"
#include "ad_internal.h"

#include "mp_config.h"
#include "help_mp.h"
#include "osdep/bswap.h"
#include "osdep/mplib.h"
#include "libmpconf/codec-cfg.h"

#include "mp_conf_lavc.h""
#include "codecs_ld.h"

typedef struct priv_s {
    AVCodecContext *lavc_ctx;
    audio_probe_t*  probe;
}priv_t;

static const audio_probe_t* __FASTCALL__ probe(sh_audio_t* sh,uint32_t wtag) {
    unsigned i;
    audio_probe_t* acodec = NULL;
    const char *what="AVCodecID";
    priv_t* priv=reinterpret_cast<priv_t*>(sh->context);
    enum AVCodecID id = ff_codec_get_id(ff_codec_wav_tags,wtag);
    if (id <= 0) {
	prn_err:
	MSG_ERR("Cannot find %s for '0x%X' tag! Try force -ac option\n"
		,what
		,sh->wtag);
	return NULL;
    }
    if(!priv){
	priv=new(zeromem) priv_t;
	sh->context=priv;
//	avcodec_init();
	avcodec_register_all();
    }
    AVCodec *codec=avcodec_find_decoder(id);
    if(!codec) { what="AVCodec"; goto prn_err; }

    acodec=new(zeromem) audio_probe_t;
    acodec->codec_dll=mp_strdup(avcodec_get_name(id));
    acodec->driver="ffmpeg";
    acodec->wtag=wtag;
    if(codec->sample_fmts)
    for(i=0;i<Audio_MaxOutSample;i++) {
	if(codec->sample_fmts[i]==-1) break;
	acodec->sample_fmt[i]=ff_codec_get_tag(ff_codec_wav_tags,id);
    }
    priv->probe=acodec;
    return acodec;
}

struct codecs_st* __FASTCALL__ find_ffmpeg_audio(sh_audio_t* sh) {
    const audio_probe_t* aprobe=probe(sh,sh->wtag);
    struct codecs_st* acodec = NULL;
    if(aprobe) {
	acodec=new(zeromem) struct codecs_st;
	strcpy(acodec->dll_name,aprobe->codec_dll);
	strcpy(acodec->driver_name,aprobe->driver);
	strcpy(acodec->codec_name,acodec->dll_name);
	memcpy(acodec->outfmt,aprobe->sample_fmt,sizeof(aprobe->sample_fmt));
    }
    return acodec;
}

#ifndef FF_INPUT_BUFFER_PADDING_SIZE
#define FF_INPUT_BUFFER_PADDING_SIZE 8
#endif

static const ad_info_t info = {
    "FFmpeg/libavcodec audio decoders",
    "ffmpeg",
    "Nickols_K",
    "build-in"
};

static const config_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

LIBAD_EXTERN(ffmpeg)

MPXP_Rc preinit(sh_audio_t *sh)
{
    sh->audio_out_minsize=AVCODEC_MAX_AUDIO_FRAME_SIZE;
    return MPXP_Ok;
}

MPXP_Rc init(sh_audio_t *sh_audio)
{
    int x;
    float pts;
    AVCodec *lavc_codec=NULL;
    priv_t* priv=reinterpret_cast<priv_t*>(sh_audio->context);
    MSG_V("FFmpeg's libavcodec audio codec\n");
    if(!priv){
	priv=new(zeromem) priv_t;
	sh_audio->context=priv;
//	avcodec_init();
	avcodec_register_all();
    }
    lavc_codec = (AVCodec *)avcodec_find_decoder_by_name(sh_audio->codec->dll_name);
    if(!lavc_codec) {
	MSG_ERR(MSGTR_MissingLAVCcodec,sh_audio->codec->dll_name);
	return MPXP_False;
    }
    priv->lavc_ctx = avcodec_alloc_context3(lavc_codec);
    if(sh_audio->wf) {
	priv->lavc_ctx->channels = sh_audio->wf->nChannels;
	priv->lavc_ctx->sample_rate = sh_audio->wf->nSamplesPerSec;
	priv->lavc_ctx->bit_rate = sh_audio->wf->nAvgBytesPerSec * 8;
	priv->lavc_ctx->block_align = sh_audio->wf->nBlockAlign;
	priv->lavc_ctx->bits_per_coded_sample = sh_audio->wf->wBitsPerSample;
	/* alloc extra data */
	if (sh_audio->wf->cbSize > 0) {
	    priv->lavc_ctx->extradata = new uint8_t[sh_audio->wf->cbSize+FF_INPUT_BUFFER_PADDING_SIZE];
	    priv->lavc_ctx->extradata_size = sh_audio->wf->cbSize;
	    memcpy(priv->lavc_ctx->extradata, (char *)sh_audio->wf + sizeof(WAVEFORMATEX),
		    priv->lavc_ctx->extradata_size);
	}
    }
    // for QDM2
    if (sh_audio->codecdata_len && sh_audio->codecdata && !priv->lavc_ctx->extradata) {
	priv->lavc_ctx->extradata = new uint8_t[sh_audio->codecdata_len];
	priv->lavc_ctx->extradata_size = sh_audio->codecdata_len;
	memcpy(priv->lavc_ctx->extradata, (char *)sh_audio->codecdata,
		priv->lavc_ctx->extradata_size);
    }
    priv->lavc_ctx->codec_tag = sh_audio->wtag;
    priv->lavc_ctx->codec_type = lavc_codec->type;
    priv->lavc_ctx->codec_id = lavc_codec->id;
    /* open it */
    if (avcodec_open2(priv->lavc_ctx, lavc_codec, NULL) < 0) {
	MSG_ERR( MSGTR_CantOpenCodec);
	return MPXP_False;
    }
    MSG_V("INFO: libavcodec init OK!\n");

    // Decode at least 1 byte:  (to get header filled)
    x=decode(sh_audio,reinterpret_cast<unsigned char*>(sh_audio->a_buffer),1,sh_audio->a_buffer_size,&pts);
    if(x>0) sh_audio->a_buffer_len=x;

    sh_audio->nch=priv->lavc_ctx->channels;
    sh_audio->rate=priv->lavc_ctx->sample_rate;
    switch(priv->lavc_ctx->sample_fmt) {
	case AV_SAMPLE_FMT_U8:  ///< unsigned 8 bits
	    sh_audio->afmt=AFMT_U8;
	    break;
	default:
	case AV_SAMPLE_FMT_S16:             ///< signed 16 bits
	    sh_audio->afmt=AFMT_S16_LE;
	    break;
	case AV_SAMPLE_FMT_S32:             ///< signed 32 bits
	    sh_audio->afmt=AFMT_S32_LE;
	    break;
	case AV_SAMPLE_FMT_FLT:             ///< float
	    sh_audio->afmt=AFMT_FLOAT32;
	    break;
    }
    sh_audio->i_bps=priv->lavc_ctx->bit_rate/8;
    return MPXP_Ok;
}

void uninit(sh_audio_t *sh)
{
    priv_t* priv=reinterpret_cast<priv_t*>(sh->context);
    avcodec_close(priv->lavc_ctx);
    if (priv->lavc_ctx->extradata) delete priv->lavc_ctx->extradata;
    delete priv->lavc_ctx;
    if(priv->probe) { delete priv->probe->codec_dll; delete priv->probe; }
    delete priv;
    sh->context=NULL;
}

MPXP_Rc control(sh_audio_t *sh,int cmd,any_t* arg, ...)
{
    UNUSED(arg);
    priv_t* priv = reinterpret_cast<priv_t*>(sh->context);
    switch(cmd){
	case ADCTRL_RESYNC_STREAM:
	    avcodec_flush_buffers(priv->lavc_ctx);
	    return MPXP_True;
	default: break;
    }
    return MPXP_Unknown;
}

unsigned decode(sh_audio_t *sh_audio,unsigned char *buf,unsigned minlen,unsigned maxlen,float *pts)
{
    priv_t* priv = reinterpret_cast<priv_t*>(sh_audio->context);
    unsigned char *start=NULL;
    int y;
    unsigned len=0;
    float apts=0.,null_pts;
    while(len<minlen){
	AVPacket pkt;
	int len2=maxlen;
	int x=ds_get_packet_r(sh_audio->ds,&start,apts?&null_pts:&apts);
	if(x<=0) break; // error
	if(sh_audio->wtag==mmioFOURCC('d','n','e','t')) swab(start,start,x&(~1));
	av_init_packet(&pkt);
	pkt.data = start;
	pkt.size = x;
	y=avcodec_decode_audio3(priv->lavc_ctx,(int16_t*)buf,&len2,&pkt);
	if(y<0){ MSG_V("lavc_audio: error\n");break; }
	if(y<x)
	{
	    sh_audio->ds->buffer_pos+=y-x;  // put back data (HACK!)
	    if(sh_audio->wtag==mmioFOURCC('d','n','e','t'))
		swab(start+y,start+y,(x-y)&~(1));
	}
	if(len2>0){
	  //len=len2;break;
	  if(len==0) len=len2; else len+=len2;
	  buf+=len2;
	}
	MSG_DBG2("Decoded %d -> %d  \n",y,len2);
    }
  *pts=apts;
  return len;
}
