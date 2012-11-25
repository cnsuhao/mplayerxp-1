#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
#include <stdio.h>
#include <stdlib.h>
#include "libnuppelvideo/nuppelvideo.h"
#include "vd_internal.h"
#include "codecs_ld.h"
#include "osdep/bswap.h"

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

static const video_probe_t* __FASTCALL__ probe(sh_video_t *sh,uint32_t fourcc) {
    unsigned i;
    for(i=0;probes[i].driver;i++)
	if(fourcc==probes[i].fourcc)
	    return &probes[i];
    return NULL;
}

// to set/get/query special features/parameters
static MPXP_Rc control_vd(sh_video_t *sh,int cmd,any_t* arg,...){
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

// init driver
static MPXP_Rc init(sh_video_t *sh,any_t* libinput){
    return mpcodecs_config_vf(sh,sh->src_w,sh->src_h,libinput);
}

// uninit driver
static void uninit(sh_video_t *sh) {}

// decode a frame
static mp_image_t* decode(sh_video_t *sh,const enc_frame_t* frame){
    mp_image_t* mpi;
    if(frame->len<=0) return NULL; // skipped frame

    mpi=mpcodecs_get_image(sh, MP_IMGTYPE_TEMP, 0,
	sh->src_w, sh->src_h);
    if(mpi->flags&MP_IMGFLAG_DIRECT) mpi->flags|=MP_IMGFLAG_RENDERED;

    decode_nuv(reinterpret_cast<unsigned char*>(frame->data), frame->len, mpi->planes[0], sh->src_w, sh->src_h);

    return mpi;
}
