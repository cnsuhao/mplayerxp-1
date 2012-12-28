#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>

#include "codecs_ld.h"

#include "mplayerxp.h"
#include "mpxp_help.h"
#include "ad_internal.h"
#include "ad_msg.h"

static const ad_info_t info = {
    "Win32/DMO decoders",
    "dmo",
    "A'rpi",
    "build-in"
};

static const config_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

LIBAD_EXTERN(dmo)

#include "win32loader/dmo/DMO_AudioDecoder.h"

struct admo_private_t : public Opaque {
    admo_private_t();
    virtual ~admo_private_t();

    float pts;
    DMO_AudioDecoder* ds_adec;
    sh_audio_t* sh;
};
admo_private_t::admo_private_t() {}
admo_private_t::~admo_private_t() {
    DMO_AudioDecoder_Destroy(ds_adec);
}

static const audio_probe_t* __FASTCALL__ probe(uint32_t wtag) { return NULL; }

static MPXP_Rc init(admo_private_t *p)
{
    UNUSED(p);
    return MPXP_Ok;
}

static Opaque* preinit(const audio_probe_t& probe,sh_audio_t *sh_audio,audio_filter_info_t& afi)
{
    UNUSED(probe);
    UNUSED(afi);
    admo_private_t*priv;
    int chans=(mp_conf.ao_channels==sh_audio->wf->nChannels) ?
	mp_conf.ao_channels : (sh_audio->wf->nChannels>=2 ? 2 : 1);
    if(!(priv=new(zeromem) admo_private_t)) return NULL;
    priv->sh=sh_audio;
    if(!(priv->ds_adec=DMO_AudioDecoder_Open(sh_audio->codec->dll_name,&sh_audio->codec->guid,sh_audio->wf,chans))) {
	MSG_ERR(MSGTR_MissingDLLcodec,sh_audio->codec->dll_name);
	delete priv;
	return NULL;
    }
    sh_audio->i_bps=sh_audio->wf->nAvgBytesPerSec;
    sh_audio->nch=chans;
    sh_audio->rate=sh_audio->wf->nSamplesPerSec;
    sh_audio->audio_in_minsize=4*sh_audio->wf->nBlockAlign;
    if(sh_audio->audio_in_minsize<8192) sh_audio->audio_in_minsize=8192;
    sh_audio->audio_out_minsize=4*16384;
    MSG_V("INFO: Win32/DMO audio codec init OK!\n");
    return priv;
}

static void uninit(Opaque& ctx) { UNUSED(ctx); }

static MPXP_Rc control_ad(Opaque& ctx,int cmd,any_t* arg, ...)
{
    admo_private_t& priv=static_cast<admo_private_t&>(ctx);
    sh_audio_t* sh_audio = priv.sh;
    int skip;
    UNUSED(arg);
    switch(cmd) {
	case ADCTRL_SKIP_FRAME: {
	    float pts;
	    skip=sh_audio->wf->nBlockAlign;
	    if(skip<16){
		skip=(sh_audio->wf->nAvgBytesPerSec/16)&(~7);
		if(skip<16) skip=16;
	    }
	    demux_read_data_r(*sh_audio->ds,NULL,skip,pts);
	    return MPXP_True;
	}
    }
    return MPXP_Unknown;
}

static unsigned decode(Opaque& ctx,unsigned char *buf,unsigned minlen,unsigned maxlen,float& pts)
{
    admo_private_t& priv=static_cast<admo_private_t&>(ctx);
    sh_audio_t* sh_audio = priv.sh;
  unsigned len=0;
  UNUSED(minlen);
  {
	unsigned size_in=0;
	unsigned size_out=0;
	unsigned srcsize=DMO_AudioDecoder_GetSrcSize(priv.ds_adec, maxlen);
	MSG_DBG2("DMO says: srcsize=%d  (buffsize=%d)  out_size=%d\n",srcsize,sh_audio->a_in_buffer_size,maxlen);
	if(srcsize>sh_audio->a_in_buffer_size) srcsize=sh_audio->a_in_buffer_size; // !!!!!!
	if((unsigned)sh_audio->a_in_buffer_len<srcsize){
	  unsigned l;
	  l=demux_read_data_r(*sh_audio->ds,reinterpret_cast<unsigned char*>(&sh_audio->a_in_buffer[sh_audio->a_in_buffer_len]),
	    srcsize-sh_audio->a_in_buffer_len,pts);
	    sh_audio->a_in_buffer_len+=l;
	    priv.pts=pts;
	}
	else pts=priv.pts;
	DMO_AudioDecoder_Convert(priv.ds_adec, sh_audio->a_in_buffer,sh_audio->a_in_buffer_len,
	    buf,maxlen, &size_in,&size_out);
	MSG_DBG2("DMO: audio %d -> %d converted  (in_buf_len=%d of %d)  %f\n"
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
