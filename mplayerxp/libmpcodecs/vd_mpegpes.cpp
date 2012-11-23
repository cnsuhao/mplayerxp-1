#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
#include <stdio.h>
#include <stdlib.h>

#include "vd_internal.h"
#include "osdep/bswap.h"

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

static const video_probe_t probes[] = {
    { "mpeg2", "libmpeg2", 0x10000001,                  VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", 0x10000002,                  VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('A','V','M','P'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('D','V','R',' '), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('L','M','P','2'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('M','1','V',' '), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('M','1','V','1'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('M','2','V','1'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('M','7','0','1'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('M','M','E','S'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('M','P','2','V'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('M','P','G','V'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('M','P','E','G'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('M','P','G','1'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('M','P','G','2'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('M','X','5','P'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('P','I','M','1'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('P','I','M','2'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('H','D','V','1'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('H','D','V','2'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('H','D','V','3'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('H','D','V','4'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('H','D','V','5'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('H','D','V','6'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('H','D','V','7'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('H','D','V','8'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('H','D','V','9'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('H','D','V','A'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('M','X','3','N'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('M','X','3','P'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('M','X','4','N'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('M','X','4','P'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('M','X','5','N'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('X','D','5','1'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('X','D','5','2'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('X','D','5','3'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('X','D','5','4'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('X','D','5','5'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('X','D','5','6'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('X','D','5','7'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('X','D','5','8'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('X','D','5','9'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('X','D','5','A'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('X','D','5','B'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('X','D','5','C'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('X','D','5','D'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('X','D','5','E'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('X','D','5','F'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('X','D','V','1'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('X','D','V','2'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('X','D','V','3'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('X','D','V','4'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('X','D','V','5'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('X','D','V','6'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('X','D','V','7'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('X','D','V','8'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('X','D','V','9'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('X','D','V','A'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('X','D','V','B'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('X','D','V','C'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('X','D','V','D'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('X','D','V','E'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpeg2", "libmpeg2", FOURCC_TAG('X','D','V','F'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
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
static MPXP_Rc control(sh_video_t *sh,int cmd,any_t* arg,...){
    return MPXP_Unknown;
}

// init driver
static MPXP_Rc init(sh_video_t *sh,any_t* libinput){
    return mpcodecs_config_vo(sh,sh->src_w,sh->src_h,libinput);
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
