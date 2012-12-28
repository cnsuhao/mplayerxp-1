#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "mplayerxp.h"
#include <dlfcn.h> /* GLIBC specific. Exists under cygwin too! */

#include "mpxp_help.h"

#include "vd_internal.h"

#include "win32loader/dshow/DS_VideoDecoder.h"
#include "codecs_ld.h"
#include "osdep/bswap.h"

static const vd_info_t info = {
    "Win32/DirectShow video codecs",
    "dshow",
    "A'rpi",
    "build-in"
};

static const config_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

LIBVD_EXTERN(dshow)

struct vdshow_private_t : public Opaque {
    vdshow_private_t(); 
    virtual ~vdshow_private_t();

    DS_VideoDecoder* dshow;
    sh_video_t* sh;
    video_decoder_t* parent;
};
vdshow_private_t::vdshow_private_t() {}
vdshow_private_t::~vdshow_private_t() {
    DS_VideoDecoder_Destroy(dshow);
}

static const video_probe_t* __FASTCALL__ probe(uint32_t fourcc) { return NULL; }

// to set/get/query special features/parameters
static MPXP_Rc control_vd(Opaque& ctx,int cmd,any_t* arg,...){
    vdshow_private_t& priv=static_cast<vdshow_private_t&>(ctx);
    switch(cmd){
	case VDCTRL_QUERY_MAX_PP_LEVEL:
	    *((unsigned*)arg)=4;
	    return MPXP_Ok;
	case VDCTRL_SET_PP_LEVEL:
	    DS_VideoDecoder_SetValue(priv.dshow,"Quality",*((int*)arg));
	    return MPXP_Ok;
	case VDCTRL_SET_EQUALIZER: {
	    va_list ap;
	    int value;
	    va_start(ap, arg);
	    value=va_arg(ap, int);
	    va_end(ap);
	    value=(value/2)+50;
	    if(DS_VideoDecoder_SetValue(priv.dshow,reinterpret_cast<char*>(arg),value)==0) return MPXP_Ok;
	    return MPXP_False;
	}
	case VDCTRL_QUERY_FORMAT:
	    if (*((int*)arg) == IMGFMT_YV12 ||
		*((int*)arg) == IMGFMT_I420 ||
		*((int*)arg) == IMGFMT_IYUV ||
		*((int*)arg) == IMGFMT_YVU9 ||
		*((int*)arg) == IMGFMT_YUY2 ||
		*((int*)arg) == IMGFMT_UYVY)
		    return MPXP_True;
	    else return MPXP_False;
      default: break;
    }
    return MPXP_Unknown;
}

static Opaque* preinit(const video_probe_t& probe,sh_video_t *sh,put_slice_info_t& psi){
    UNUSED(probe);
    UNUSED(psi);
    vdshow_private_t* priv = new(zeromem) vdshow_private_t;
    priv->sh=sh;
    return priv;
}

// init driver
static MPXP_Rc init(Opaque& ctx,video_decoder_t& opaque){
    vdshow_private_t& priv=static_cast<vdshow_private_t&>(ctx);
    sh_video_t* sh = priv.sh;
    unsigned int out_fmt;
    priv.parent = &opaque;
    if(!(priv.dshow=DS_VideoDecoder_Open(sh->codec->dll_name,&sh->codec->guid, sh->bih, 0, 0))){
	MSG_ERR(MSGTR_MissingDLLcodec,sh->codec->dll_name);
	MSG_HINT("Maybe you forget to upgrade your win32 codecs?? It's time to download the new\n");
	MSG_HINT("package from:  ftp://mplayerhq.hu/MPlayer/releases/w32codec.zip  !\n");
	return MPXP_False;
    }
    if(!mpcodecs_config_vf(opaque,sh->src_w,sh->src_h)) return MPXP_False;
    out_fmt=sh->codec->outfmt[sh->outfmtidx];
    switch(out_fmt){
	case IMGFMT_YUY2:
	case IMGFMT_UYVY:
	    DS_VideoDecoder_SetDestFmt(priv.dshow,16,out_fmt);break; // packed YUV
	case IMGFMT_YV12:
	case IMGFMT_I420:
	case IMGFMT_IYUV:
	    DS_VideoDecoder_SetDestFmt(priv.dshow,12,out_fmt);break; // planar YUV
	case IMGFMT_YVU9:
	    DS_VideoDecoder_SetDestFmt(priv.dshow,9,out_fmt);break;
	default:
	    DS_VideoDecoder_SetDestFmt(priv.dshow,out_fmt&255,0);    // RGB/BGR
    }
    DS_SetAttr_DivX("Quality",mpxp_context().output_quality);
    DS_VideoDecoder_StartInternal(priv.dshow);
    MSG_V("INFO: Win32/DShow init OK!\n");
    return MPXP_Ok;
}

// uninit driver
static void uninit(Opaque& ctx){ UNUSED(ctx); }

// decode a frame
static mp_image_t* decode(Opaque& ctx,const enc_frame_t& frame){
    vdshow_private_t& priv=static_cast<vdshow_private_t&>(ctx);
    sh_video_t* sh = priv.sh;
    mp_image_t* mpi;
    if(frame.len<=0) return NULL; // skipped frame

    if(frame.flags&3){
	// framedrop:
	DS_VideoDecoder_DecodeInternal(priv.dshow, frame.data, frame.len, sh->ds->flags&1, NULL);
	return NULL;
    }

    mpi=mpcodecs_get_image(*priv.parent, MP_IMGTYPE_TEMP, 0 /*MP_IMGFLAG_ACCEPT_STRIDE*/,
	sh->src_w, sh->src_h);
    if(mpi->flags&MP_IMGFLAG_DIRECT) mpi->flags|=MP_IMGFLAG_RENDERED;

    DS_VideoDecoder_DecodeInternal(priv.dshow, frame.data, frame.len, sh->ds->flags&1, reinterpret_cast<char*>(mpi->planes[0]));

    return mpi;
}
