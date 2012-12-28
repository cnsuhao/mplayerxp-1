#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "mpxp_help.h"

#include <dlfcn.h> /* GLIBC specific. Exists under cygwin too! */

#include "vd_internal.h"
#include "codecs_ld.h"
#include "win32loader/dmo/DMO_VideoDecoder.h"
#include "vd_msg.h"

static const vd_info_t info = {
    "Win32/DMO video codecs",
    "dmo",
    "A'rpi",
    "build-in"
};

static const mpxp_option_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

LIBVD_EXTERN(dmo)

struct vdmo_private_t : public Opaque {
    vdmo_private_t();
    virtual ~vdmo_private_t();

    DMO_VideoDecoder* dmo;
    sh_video_t*	sh;
    video_decoder_t* parent;
};
vdmo_private_t::vdmo_private_t() {}
vdmo_private_t::~vdmo_private_t() {
    DMO_VideoDecoder_Destroy(dmo);
}

static const video_probe_t* __FASTCALL__ probe(uint32_t fourcc) { return NULL; }

// to set/get/query special features/parameters
static MPXP_Rc control_vd(Opaque& ctx,int cmd,any_t* arg,...){
    UNUSED(ctx);
    switch(cmd){
      case VDCTRL_QUERY_FORMAT:
	    if (*((int*)arg) == IMGFMT_YV12 ||
		*((int*)arg) == IMGFMT_I420 ||
		*((int*)arg) == IMGFMT_IYUV ||
		*((int*)arg) == IMGFMT_YVU9 ||
		*((int*)arg) == IMGFMT_YUY2 ||
		*((int*)arg) == IMGFMT_UYVY)
			return MPXP_True;
	    else 	return MPXP_False;
      default: break;
    }
    return MPXP_Unknown;
}

static Opaque* preinit(const video_probe_t& probe,sh_video_t *sh,put_slice_info_t& psi){
    UNUSED(probe);
    UNUSED(psi);
    vdmo_private_t* priv = new(zeromem) vdmo_private_t;
    priv->sh=sh;
    return priv;
}

// init driver
static MPXP_Rc init(Opaque& ctx,video_decoder_t& opaque){
    unsigned int out_fmt;
    vdmo_private_t& priv=static_cast<vdmo_private_t&>(ctx);
    priv.parent = &opaque;
    sh_video_t* sh = priv.sh;
    if(!(priv.dmo=DMO_VideoDecoder_Open(sh->codec->dll_name,&sh->codec->guid, sh->bih, 0, 0))){
	MSG_ERR(MSGTR_MissingDLLcodec,sh->codec->dll_name);
	MSG_HINT("Maybe you forget to upgrade your win32 codecs?? It's time to download the new\n"
		 "package from:  ftp://mplayerhq.hu/MPlayer/releases/w32codec.tar.bz2!\n");
	return MPXP_False;
    }
    if(!mpcodecs_config_vf(opaque,sh->src_w,sh->src_h)) return MPXP_False;
    out_fmt=sh->codec->outfmt[sh->outfmtidx];
    switch(out_fmt){
    case IMGFMT_YUY2:
    case IMGFMT_UYVY:
	DMO_VideoDecoder_SetDestFmt(priv.dmo,16,out_fmt);break; // packed YUV
    case IMGFMT_YV12:
    case IMGFMT_I420:
    case IMGFMT_IYUV:
	DMO_VideoDecoder_SetDestFmt(priv.dmo,12,out_fmt);break; // planar YUV
    case IMGFMT_YVU9:
	DMO_VideoDecoder_SetDestFmt(priv.dmo,9,out_fmt);break;
    default:
	DMO_VideoDecoder_SetDestFmt(priv.dmo,out_fmt&255,0);    // RGB/BGR
    }
    DMO_VideoDecoder_StartInternal(priv.dmo);
    MSG_V("INFO: Win32/DMOhow video codec init OK!\n");
    return MPXP_Ok;
}

// uninit driver
static void uninit(Opaque& ctx){ UNUSED(ctx); }

// decode a frame
static mp_image_t* decode(Opaque& ctx,const enc_frame_t& frame){
    vdmo_private_t& priv=static_cast<vdmo_private_t&>(ctx);
    sh_video_t* sh = priv.sh;
    mp_image_t* mpi;
    if(frame.len<=0) return NULL; // skipped frame

    if(frame.flags&3){
	// framedrop:
	DMO_VideoDecoder_DecodeInternal(priv.dmo, frame.data, frame.len, sh->ds->flags&1, NULL);
	return NULL;
    }

    mpi=mpcodecs_get_image(*priv.parent, MP_IMGTYPE_TEMP, 0 /*MP_IMGFLAG_ACCEPT_STRIDE*/,
	sh->src_w, sh->src_h);
    if(mpi->flags&MP_IMGFLAG_DIRECT) mpi->flags|=MP_IMGFLAG_RENDERED;

    if(!mpi){	// temporary!
	MSG_ERR("couldn't allocate image for cinepak codec\n");
	return NULL;
    }

    DMO_VideoDecoder_DecodeInternal(priv.dmo, frame.data, frame.len, sh->ds->flags&1, reinterpret_cast<char*>(mpi->planes[0]));

    return mpi;
}
