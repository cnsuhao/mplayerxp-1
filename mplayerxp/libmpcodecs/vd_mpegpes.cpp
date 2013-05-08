#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
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

static const mpxp_option_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

LIBVD_EXTERN(mpegpes)

static const video_probe_t probes[] = {
    { "mpegpes", "libmpegpes", 0x10000001,                  VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", 0x10000002,                  VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('A','V','M','P'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('D','V','R',' '), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('L','M','P','2'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('M','1','V',' '), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('M','1','V','1'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('M','2','V','1'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('M','7','0','1'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('M','M','E','S'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('M','P','2','V'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('M','P','G','V'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('M','P','E','G'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('M','P','G','1'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('M','P','G','2'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('M','X','5','P'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('P','I','M','1'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('P','I','M','2'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('H','D','V','1'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('H','D','V','2'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('H','D','V','3'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('H','D','V','4'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('H','D','V','5'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('H','D','V','6'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('H','D','V','7'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('H','D','V','8'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('H','D','V','9'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('H','D','V','A'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('M','X','3','N'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('M','X','3','P'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('M','X','4','N'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('M','X','4','P'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('M','X','5','N'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','5','1'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','5','2'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','5','3'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','5','4'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','5','5'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','5','6'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','5','7'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','5','8'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','5','9'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','5','A'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','5','B'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','5','C'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','5','D'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','5','E'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','5','F'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','V','1'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','V','2'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','V','3'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','V','4'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','V','5'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','V','6'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','V','7'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','V','8'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','V','9'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','V','A'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','V','B'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','V','C'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','V','D'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','V','E'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','V','F'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { NULL, NULL, 0x0, VCodecStatus_NotWorking, {0x0}, { VideoFlag_None }}
};

static const video_probe_t* __FASTCALL__ probe(uint32_t fourcc) {
    unsigned i;
    for(i=0;probes[i].driver;i++)
	if(fourcc==probes[i].fourcc)
	    return &probes[i];
    return NULL;
}

struct mpegpes_private_t : public Opaque {
    mpegpes_private_t();
    virtual ~mpegpes_private_t();

    sh_video_t* sh;
    video_decoder_t* parent;
};
mpegpes_private_t::mpegpes_private_t() {}
mpegpes_private_t::~mpegpes_private_t() {}
// to set/get/query special features/parameters
static MPXP_Rc control_vd(Opaque &ctx,int cmd,any_t* arg,...){
    UNUSED(ctx);
    UNUSED(cmd);
    UNUSED(arg);
    return MPXP_Unknown;
}

static Opaque* preinit(const video_probe_t& probe,sh_video_t *sh,put_slice_info_t& psi){
    UNUSED(probe);
    UNUSED(psi);
    mpegpes_private_t* priv = new(zeromem) mpegpes_private_t;
    priv->sh=sh;
    return priv;
}

// init driver
static MPXP_Rc init(Opaque& ctx,video_decoder_t& opaque){
    mpegpes_private_t& priv=static_cast<mpegpes_private_t&>(ctx);
    sh_video_t* sh = priv.sh;
    priv.parent = &opaque;
    return mpcodecs_config_vf(opaque,sh->src_w,sh->src_h);
}

// uninit driver
static void uninit(Opaque& ctx) { UNUSED(ctx); }

// decode a frame
static mp_image_t* decode(Opaque& ctx,const enc_frame_t& frame){
    mpegpes_private_t& priv=static_cast<mpegpes_private_t&>(ctx);
    sh_video_t* sh = priv.sh;
    mp_image_t* mpi;
    static vo_mpegpes_t packet;
    mpi=mpcodecs_get_image(*priv.parent, MP_IMGTYPE_EXPORT, 0,sh->src_w, sh->src_h);
    if(mpi->flags&MP_IMGFLAG_DIRECT) mpi->flags|=MP_IMGFLAG_RENDERED;
    packet.data=frame.data;
    packet.size=frame.len-4;
    packet.timestamp=sh->ds->pts;
    packet.id=0x1E0; //+sh_video->ds->id;
    mpi->planes[0]=(uint8_t*)(&packet);
    return mpi;
}
