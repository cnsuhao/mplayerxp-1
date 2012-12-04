#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
#include <stdio.h>
#include <stdlib.h>

#include "vd_internal.h"

struct vd_private_t {
    sh_video_t* sh;
};

static const vd_info_t info = {
    "Null video decoder",
    "null",
    "A'rpi",
    "build-in"
};

static const config_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

LIBVD_EXTERN(null)

static const video_probe_t* __FASTCALL__ probe(vd_private_t *sh,uint32_t fourcc) { return NULL; }

// to set/get/query special features/parameters
static MPXP_Rc control_vd(vd_private_t *sh,int cmd,any_t* arg,...){
    return MPXP_Unknown;
}

static vd_private_t* preinit(sh_video_t *sh,put_slice_info_t* psi){
    UNUSED(psi);
    vd_private_t* priv = new(zeromem) vd_private_t;
    priv->sh=sh;
    return priv;
}

// init driver
static MPXP_Rc init(vd_private_t *priv,video_decoder_t* opaque){
    sh_video_t* sh = priv->sh;
    return mpcodecs_config_vf(opaque,sh->src_w,sh->src_h);
}

// uninit driver
static void uninit(vd_private_t *ctx) { delete ctx; }

// decode a frame
static mp_image_t* decode(vd_private_t *ctx,const enc_frame_t* frame){
    UNUSED(ctx);
    UNUSED(frame);
    return NULL;
}

