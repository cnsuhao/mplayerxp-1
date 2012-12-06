#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <math.h>

#include "libvo/img_format.h"
#include "osdep/bswap.h"

#include <libdv/dv.h>
#include <libdv/dv_types.h>

#include "libmpstream/stream.h"
#include "libmpdemux/demuxer.h"
#include "libmpdemux/stheader.h"

#include "vd_internal.h"

struct vd_private_t {
    dv_decoder_t*	dvd;
    sh_video_t*		sh;
    video_decoder_t*	parent;
};

static const vd_info_t info = {
    "Raw DV Video Decoder",
    "libdv",
    "Alexander Neundorf <neundorf@kde.org>",
    "http://libdv.sourceforge.net"
};

static const config_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

LIBVD_EXTERN(libdv)

static const video_probe_t probes[] = {
    { "libdv", "libdv", FOURCC_TAG('A','V','d','1'), VCodecStatus_Working, {IMGFMT_YUY2}, {VideoFlag_None, VideoFlag_None } },
    { "libdv", "libdv", FOURCC_TAG('A','V','d','v'), VCodecStatus_Working, {IMGFMT_YUY2}, {VideoFlag_None, VideoFlag_None } },
    { "libdv", "libdv", FOURCC_TAG('D','V','C',' '), VCodecStatus_Working, {IMGFMT_YUY2}, {VideoFlag_None, VideoFlag_None } },
    { "libdv", "libdv", FOURCC_TAG('D','V','C','P'), VCodecStatus_Working, {IMGFMT_YUY2}, {VideoFlag_None, VideoFlag_None } },
    { "libdv", "libdv", FOURCC_TAG('D','V','5','0'), VCodecStatus_Working, {IMGFMT_YUY2}, {VideoFlag_None, VideoFlag_None } },
    { "libdv", "libdv", FOURCC_TAG('D','V','5','N'), VCodecStatus_Working, {IMGFMT_YUY2}, {VideoFlag_None, VideoFlag_None } },
    { "libdv", "libdv", FOURCC_TAG('D','V','5','P'), VCodecStatus_Working, {IMGFMT_YUY2}, {VideoFlag_None, VideoFlag_None } },
    { "libdv", "libdv", FOURCC_TAG('D','V','H','3'), VCodecStatus_Working, {IMGFMT_YUY2}, {VideoFlag_None, VideoFlag_None } },
    { "libdv", "libdv", FOURCC_TAG('D','V','H','5'), VCodecStatus_Working, {IMGFMT_YUY2}, {VideoFlag_None, VideoFlag_None } },
    { "libdv", "libdv", FOURCC_TAG('D','V','H','6'), VCodecStatus_Working, {IMGFMT_YUY2}, {VideoFlag_None, VideoFlag_None } },
    { "libdv", "libdv", FOURCC_TAG('D','V','H','Q'), VCodecStatus_Working, {IMGFMT_YUY2}, {VideoFlag_None, VideoFlag_None } },
    { "libdv", "libdv", FOURCC_TAG('D','V','H','P'), VCodecStatus_Working, {IMGFMT_YUY2}, {VideoFlag_None, VideoFlag_None } },
    { "libdv", "libdv", FOURCC_TAG('D','V','P','P'), VCodecStatus_Working, {IMGFMT_YUY2}, {VideoFlag_None, VideoFlag_None } },
    { "libdv", "libdv", FOURCC_TAG('D','V','S','C'), VCodecStatus_Working, {IMGFMT_YUY2}, {VideoFlag_None, VideoFlag_None } },
    { "libdv", "libdv", FOURCC_TAG('D','V','S','D'), VCodecStatus_Working, {IMGFMT_YUY2}, {VideoFlag_None, VideoFlag_None } },
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
static MPXP_Rc control_vd(vd_private_t *priv,int cmd,any_t* arg,...){
    UNUSED(priv);
    UNUSED(cmd);
    UNUSED(arg);
    return MPXP_Unknown;
}

static dv_decoder_t* global_rawdv_decoder=NULL;

dv_decoder_t* init_global_rawdv_decoder(void)
{
    if(!global_rawdv_decoder){
	global_rawdv_decoder=dv_decoder_new(TRUE,TRUE,FALSE);
	global_rawdv_decoder->quality=DV_QUALITY_BEST;
	global_rawdv_decoder->prev_frame_decoded = 0;
    }
    return global_rawdv_decoder;
}

static vd_private_t* preinit(const video_probe_t* probe,sh_video_t *sh,put_slice_info_t* psi){
    UNUSED(probe);
    UNUSED(psi);
    vd_private_t* priv = new(zeromem) vd_private_t;
    priv->sh=sh;
    return priv;
}

// init driver
static MPXP_Rc init(vd_private_t *priv,video_decoder_t* opaque)
{
    sh_video_t* sh = priv->sh;
    priv->parent = opaque;
    priv->dvd = init_global_rawdv_decoder();
    return mpcodecs_config_vf(opaque,sh->src_w,sh->src_h);
}

// uninit driver
static void uninit(vd_private_t *priv) { delete priv; }

// decode a frame
static mp_image_t* decode(vd_private_t *priv,const enc_frame_t* frame)
{
   mp_image_t* mpi;
   dv_decoder_t *dvd=priv->dvd;
   sh_video_t* sh = priv->sh;

   if(frame->len<=0 || (frame->flags&3)){
//      fprintf(stderr,"decode() (rawdv) SKIPPED\n");
      return NULL; // skipped frame
   }

   dv_parse_header(dvd, reinterpret_cast<uint8_t*>(frame->data));

   mpi=mpcodecs_get_image(priv->parent, MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE, sh->src_w, sh->src_h);

   if(!mpi){	// temporary!
      MSG_ERR("couldn't allocate image for stderr codec\n");
      return NULL;
   }

   dv_decode_full_frame(dvd, reinterpret_cast<uint8_t*>(frame->data), e_dv_color_yuv, mpi->planes, reinterpret_cast<int*>(mpi->stride));

   return mpi;
}
