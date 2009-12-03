#include <stdio.h>
#include <stdlib.h>

#include "config.h"

#include "vd_internal.h"

static const vd_info_t info =
{
	"MPEG 1/2 Video passthrough",
	"mpegpes",
	"A'rpi",
	"A'rpi",
	"for hw decoders"
};

static const config_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

LIBVD_EXTERN(mpegpes)

// to set/get/query special features/parameters
static int control(sh_video_t *sh,int cmd,void* arg,...){
    return CONTROL_UNKNOWN;
}

// init driver
static int init(sh_video_t *sh){
    return mpcodecs_config_vo(sh,sh->disp_w,sh->disp_h,NULL);
}

// uninit driver
static void uninit(sh_video_t *sh){
}

// decode a frame
static mp_image_t* decode(sh_video_t *sh,void* data,int len,int flags){
    mp_image_t* mpi;
    static vo_mpegpes_t packet;
    mpi=mpcodecs_get_image(sh, MP_IMGTYPE_EXPORT, 0,sh->disp_w, sh->disp_h);
    if(mpi->flags&MP_IMGFLAG_DIRECT) mpi->flags|=MP_IMGFLAG_RENDERED;
    packet.data=data;
    packet.size=len-4;
    packet.timestamp=sh->timer*90000.0;
    packet.id=0x1E0; //+sh_video->ds->id;
    mpi->planes[0]=(uint8_t*)(&packet);
    return mpi;
}
