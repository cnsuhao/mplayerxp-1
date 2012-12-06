#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
#include <stdio.h>
#include <stdlib.h>
#include "libnuppelvideo/nuppelvideo.h"
#include "vd_internal.h"
#include "codecs_ld.h"
#include "osdep/bswap.h"

struct vd_private_t {
    video_decoder_t* parent;
    sh_video_t* sh;
};

static const vd_info_t info = {
    "NuppelVideo decoder",
    "nuv",
    "A'rpi (Alex & Panagiotis Issaris <takis@lumumba.luc.ac.be>)",
    "build-in"
};

static const config_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

LIBVD_EXTERN(nuv)

static const video_probe_t probes[] = {
    { "nuv", "nuv", FOURCC_TAG('N','U','V','1'), VCodecStatus_Working, {IMGFMT_I420}, {VideoFlag_None, VideoFlag_None } },
    { "nuv", "nuv", FOURCC_TAG('R','J','P','G'), VCodecStatus_Working, {IMGFMT_I420}, {VideoFlag_None, VideoFlag_None } },
    { NULL, NULL, 0x0, VCodecStatus_NotWorking, {0x0}, { VideoFlag_None }}
};

static const video_probe_t* __FASTCALL__ probe(uint32_t fourcc) {
    unsigned i;
    for(i=0;probes[i].driver;i++)
	if(fourcc==probes[i].fourcc)
	    return &probes[i];
    return NULL;
}

// to set/get/query special features/parameters
static MPXP_Rc control_vd(vd_private_t *ctx,int cmd,any_t* arg,...){
    sh_video_t* sh=ctx->sh;
    switch(cmd) {
	case VDCTRL_QUERY_FORMAT:
	    if (*((int*)arg) == IMGFMT_I420 ||
		*((int*)arg) == IMGFMT_IYUV)
			return MPXP_True;
	    else 	return MPXP_False;
	default: break;
    }
    return MPXP_Unknown;
}

static vd_private_t* preinit(const video_probe_t* probe,sh_video_t *sh,put_slice_info_t* psi){
    UNUSED(probe);
    UNUSED(psi);
    vd_private_t* priv = new(zeromem) vd_private_t;
    priv->sh=sh;
    return priv;
}

// init driver
static MPXP_Rc init(vd_private_t *priv,video_decoder_t* opaque){
    sh_video_t* sh = priv->sh;
    priv->parent = opaque;
    return mpcodecs_config_vf(opaque,sh->src_w,sh->src_h);
}

// uninit driver
static void uninit(vd_private_t *ctx) { delete ctx; }

// decode a frame
static mp_image_t* decode(vd_private_t *ctx,const enc_frame_t* frame){
    sh_video_t* sh = ctx->sh;
    mp_image_t* mpi;
    if(frame->len<=0) return NULL; // skipped frame

    mpi=mpcodecs_get_image(ctx->parent, MP_IMGTYPE_TEMP, 0,
	sh->src_w, sh->src_h);
    if(mpi->flags&MP_IMGFLAG_DIRECT) mpi->flags|=MP_IMGFLAG_RENDERED;

    decode_nuv(reinterpret_cast<unsigned char*>(frame->data), frame->len, mpi->planes[0], sh->src_w, sh->src_h);

    return mpi;
}
