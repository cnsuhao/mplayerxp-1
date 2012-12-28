#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h> /* GLIBC specific. Exists under cygwin too! */
#include "ad_internal.h"
#include "mpxp_help.h"
#include "win32loader/dshow/DS_AudioDecoder.h"
#include "codecs_ld.h"

static const ad_info_t info = {
    "Win32/DirectShow decoders",
    "dshow",
    "Nickols_K",
    "build-in"
};

static const mpxp_option_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

LIBAD_EXTERN(dshow)

struct adshow_private_t : public Opaque {
    adshow_private_t();
    virtual ~adshow_private_t();

    float pts;
    DS_AudioDecoder* ds_adec;
    sh_audio_t* sh;
};
adshow_private_t::adshow_private_t() {}
adshow_private_t::~adshow_private_t() {
    DS_AudioDecoder_Destroy(ds_adec);
}

static const audio_probe_t* __FASTCALL__ probe(uint32_t wtag) { return NULL; }

MPXP_Rc init(Opaque& ctx)
{
  UNUSED(ctx);
  return MPXP_Ok;
}

Opaque* preinit(const audio_probe_t& probe,sh_audio_t *sh_audio,audio_filter_info_t& afi)
{
    UNUSED(probe);
    UNUSED(afi);
    adshow_private_t *priv;
    if(!(priv=new(zeromem) adshow_private_t)) return NULL;
    priv->sh = sh_audio;
    if(!(priv->ds_adec=DS_AudioDecoder_Open(sh_audio->codec->dll_name,&sh_audio->codec->guid,sh_audio->wf))) {
	MSG_ERR(MSGTR_MissingDLLcodec,sh_audio->codec->dll_name);
	delete priv;
	return NULL;
    }
    sh_audio->i_bps=sh_audio->wf->nAvgBytesPerSec;
    sh_audio->nch=sh_audio->wf->nChannels;
    sh_audio->rate=sh_audio->wf->nSamplesPerSec;
    sh_audio->audio_in_minsize=2*sh_audio->wf->nBlockAlign;
    if(sh_audio->audio_in_minsize<8192) sh_audio->audio_in_minsize=8192;
    sh_audio->audio_out_minsize=16384;
    MSG_V("INFO: Win32/DShow init OK!\n");
    return priv;
}

void uninit(Opaque& ctx) { UNUSED(ctx); }


MPXP_Rc control_ad(Opaque& ctx,int cmd,any_t* arg, ...)
{
    adshow_private_t& priv=static_cast<adshow_private_t&>(ctx);
    sh_audio_t* sh_audio = priv.sh;
    int skip;
    UNUSED(arg);
    switch(cmd) {
	case ADCTRL_RESYNC_STREAM:
	    sh_audio->a_in_buffer_len=0; // reset ACM/DShow audio buffer
	    return MPXP_True;
	case ADCTRL_SKIP_FRAME: {
	    float pts;
	    skip=sh_audio->wf->nBlockAlign;
	    if(skip<16){
		skip=(sh_audio->wf->nAvgBytesPerSec/16)&(~7);
		if(skip<16) skip=16;
	    }
	    demux_read_data_r(*sh_audio->ds,NULL,skip,pts);
	}
	return MPXP_True;
	default:
	    return MPXP_Unknown;
    }
    return MPXP_Unknown;
}

unsigned decode(Opaque& ctx,unsigned char *buf,unsigned minlen,unsigned maxlen,float& pts)
{
    adshow_private_t& priv=static_cast<adshow_private_t&>(ctx);
    sh_audio_t* sh_audio = priv.sh;
  unsigned len=0;
  UNUSED(minlen);
      { unsigned size_in=0;
	unsigned size_out=0;
	unsigned srcsize=DS_AudioDecoder_GetSrcSize(priv.ds_adec, maxlen);
	MSG_DBG3("DShow says: srcsize=%d  (buffsize=%d)  out_size=%d\n",srcsize,sh_audio->a_in_buffer_size,maxlen);
	if(srcsize>sh_audio->a_in_buffer_size) srcsize=sh_audio->a_in_buffer_size; // !!!!!!
	if((unsigned)sh_audio->a_in_buffer_len<srcsize){
	  unsigned l;
	  l=demux_read_data_r(*sh_audio->ds,reinterpret_cast<unsigned char*>(&sh_audio->a_in_buffer[sh_audio->a_in_buffer_len]),
	    srcsize-sh_audio->a_in_buffer_len,pts);
	    sh_audio->a_in_buffer_len+=l;
	    priv.pts=pts;
	}
	else pts=priv.pts;
	DS_AudioDecoder_Convert(priv.ds_adec, reinterpret_cast<unsigned char*>(sh_audio->a_in_buffer),sh_audio->a_in_buffer_len,
	    buf,maxlen, &size_in,&size_out);
	MSG_DBG2("DShow: audio %d -> %d converted  (in_buf_len=%d of %d)  %f\n"
	,size_in,size_out,sh_audio->a_in_buffer_len,sh_audio->a_in_buffer_size,pts);
	if(size_in>=(unsigned)sh_audio->a_in_buffer_len){
	  sh_audio->a_in_buffer_len=0;
	} else {
	  sh_audio->a_in_buffer_len-=size_in;
	  memcpy(sh_audio->a_in_buffer,&sh_audio->a_in_buffer[size_in],sh_audio->a_in_buffer_len);
	  priv.pts=FIX_APTS(sh_audio,priv.pts,size_in);
	}
	len=size_out;
      }
  return len;
}
