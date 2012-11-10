#include <stdio.h>
#include <stdlib.h>

#include "mp_config.h"

#include "vd_internal.h"

static const vd_info_t info = {
    "MPEG 1/2 Video passthrough",
    "mpegpes",
    "A'rpi",
    "build-in"
};

static const config_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

LIBVD_EXTERN(mpegpes)

// to set/get/query special features/parameters
static MPXP_Rc control(sh_video_t *sh,int cmd,any_t* arg,...){
    return MPXP_Unknown;
}

// init driver
static MPXP_Rc init(sh_video_t *sh,any_t* libinput){
    return mpcodecs_config_vo(sh,sh->src_w,sh->src_h,NULL,libinput);
}

// uninit driver
static void uninit(sh_video_t *sh) {}

// decode a frame
static mp_image_t* decode(sh_video_t *sh,const enc_frame_t* frame){
    mp_image_t* mpi;
    static vo_mpegpes_t packet;
    mpi=mpcodecs_get_image(sh, MP_IMGTYPE_EXPORT, 0,sh->src_w, sh->src_h);
    if(mpi->flags&MP_IMGFLAG_DIRECT) mpi->flags|=MP_IMGFLAG_RENDERED;
    packet.data=frame->data;
    packet.size=frame->len-4;
    packet.timestamp=sh->ds->pts;
    packet.id=0x1E0; //+sh_video->ds->id;
    mpi->planes[0]=(uint8_t*)(&packet);
    return mpi;
}
