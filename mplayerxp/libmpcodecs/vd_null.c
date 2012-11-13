#include <stdio.h>
#include <stdlib.h>

#include "mp_config.h"

#include "vd_internal.h"

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

static video_probe_t* __FASTCALL__ probe(uint32_t fourcc) { return NULL; }

// to set/get/query special features/parameters
static MPXP_Rc control(sh_video_t *sh,int cmd,any_t* arg,...){
    return MPXP_Unknown;
}

// init driver
static MPXP_Rc init(sh_video_t *sh,any_t* libinput){
    UNUSED(sh);
    UNUSED(libinput);
    return MPXP_Ok;
}

// uninit driver
static void uninit(sh_video_t *sh) { UNUSED(sh); }

// decode a frame
static mp_image_t* decode(sh_video_t *sh,const enc_frame_t* frame){
    UNUSED(sh);
    UNUSED(frame);
    return NULL;
}

