#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
#include <stdio.h>
#include <stdlib.h>

#include "vd_internal.h"

struct vnull_private_t : public Opaque {
    vnull_private_t();
    virtual ~vnull_private_t();

    sh_video_t* sh;
};
vnull_private_t::vnull_private_t() {}
vnull_private_t::~vnull_private_t() {}

static const vd_info_t info = {
    "Null video decoder",
    "null",
    "A'rpi",
    "build-in"
};

static const mpxp_option_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

LIBVD_EXTERN(null)

static const video_probe_t* __FASTCALL__ probe(uint32_t fourcc) { return NULL; }

// to set/get/query special features/parameters
static MPXP_Rc control_vd(Opaque& ctx,int cmd,any_t* arg,...){
    UNUSED(ctx);
    UNUSED(cmd);
    UNUSED(arg);
    return MPXP_Unknown;
}

static Opaque* preinit(const video_probe_t& probe,sh_video_t *sh,put_slice_info_t& psi){
    UNUSED(probe);
    UNUSED(psi);
    vnull_private_t* priv = new(zeromem) vnull_private_t;
    priv->sh=sh;
    return priv;
}

// init driver
static MPXP_Rc init(Opaque& ctx,video_decoder_t& opaque){
    vnull_private_t& priv=static_cast<vnull_private_t&>(ctx);
    sh_video_t* sh = priv.sh;
    return mpcodecs_config_vf(opaque,sh->src_w,sh->src_h);
}

// uninit driver
static void uninit(Opaque& ctx) { UNUSED(ctx); }

// decode a frame
static mp_image_t* decode(Opaque& ctx,const enc_frame_t& frame){
    UNUSED(ctx);
    UNUSED(frame);
    return NULL;
}

