#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
#include <stdio.h>
#include <stdlib.h>

#include "vd_internal.h"
#include "osdep/bswap.h"

struct vd_private_t {
    sh_video_t* sh;
    video_decoder_t* parent;
};

static const vd_info_t info = {
    "RAW Uncompressed Video",
    "raw",
    "A'rpi & Alex",
    "build-in"
};

static const config_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

LIBVD_EXTERN(raw)

static const video_probe_t probes[] = {
    { "raw", "raw", FOURCC_TAG('R','G','B',32), VCodecStatus_Working, {IMGFMT_RGB32}, {VideoFlag_None, VideoFlag_None } },
    { "raw", "raw", FOURCC_TAG('R','G','B',24), VCodecStatus_Working, {IMGFMT_RGB24}, {VideoFlag_None, VideoFlag_None } },
    { "raw", "raw", FOURCC_TAG('R','G','B',16), VCodecStatus_Working, {IMGFMT_RGB16}, {VideoFlag_None, VideoFlag_None } },
    { "raw", "raw", FOURCC_TAG('R','G','B',15), VCodecStatus_Working, {IMGFMT_RGB15}, {VideoFlag_None, VideoFlag_None } },
    { "raw", "raw", FOURCC_TAG('R','G','B',8),  VCodecStatus_Working, {IMGFMT_RGB8}, {VideoFlag_None, VideoFlag_None } },
    { "raw", "raw", FOURCC_TAG('R','G','B',1),  VCodecStatus_Working, {IMGFMT_RGB1}, {VideoFlag_None, VideoFlag_None } },
    { "raw", "raw", FOURCC_TAG('B','G','R',32), VCodecStatus_Working, {IMGFMT_BGR32}, {VideoFlag_None, VideoFlag_None } },
    { "raw", "raw", FOURCC_TAG('B','G','R',24), VCodecStatus_Working, {IMGFMT_BGR24}, {VideoFlag_None, VideoFlag_None } },
    { "raw", "raw", FOURCC_TAG('B','G','R',16), VCodecStatus_Working, {IMGFMT_BGR16}, {VideoFlag_None, VideoFlag_None } },
    { "raw", "raw", FOURCC_TAG('B','G','R',15), VCodecStatus_Working, {IMGFMT_BGR15}, {VideoFlag_None, VideoFlag_None } },
    { "raw", "raw", FOURCC_TAG('B','G','R',8),  VCodecStatus_Working, {IMGFMT_BGR8}, {VideoFlag_None, VideoFlag_None } },
    { "raw", "raw", FOURCC_TAG('B','G','R',1),  VCodecStatus_Working, {IMGFMT_BGR1}, {VideoFlag_None, VideoFlag_None } },

    { "raw", "raw", FOURCC_TAG('V','4','2','2'),VCodecStatus_Working, {IMGFMT_YUY2}, {VideoFlag_None, VideoFlag_None } },
    { "raw", "raw", FOURCC_TAG('V','Y','U','Y'),VCodecStatus_Working, {IMGFMT_YUY2}, {VideoFlag_None, VideoFlag_None } },
    { "raw", "raw", FOURCC_TAG('Y','U','N','V'),VCodecStatus_Working, {IMGFMT_YUY2}, {VideoFlag_None, VideoFlag_None } },
    { "raw", "raw", FOURCC_TAG('Y','U','V','2'),VCodecStatus_Working, {IMGFMT_YUY2}, {VideoFlag_None, VideoFlag_None } },
    { "raw", "raw", FOURCC_TAG('Y','U','V','S'),VCodecStatus_Working, {IMGFMT_YUY2}, {VideoFlag_None, VideoFlag_None } },
    { "raw", "raw", FOURCC_TAG('Y','U','Y','2'),VCodecStatus_Working, {IMGFMT_YUY2}, {VideoFlag_None, VideoFlag_None } },

    { "raw", "raw", FOURCC_TAG('2','V','U','Y'),VCodecStatus_Working, {IMGFMT_UYVY}, {VideoFlag_None, VideoFlag_None } },
    { "raw", "raw", FOURCC_TAG('U','Y','V','1'),VCodecStatus_Working, {IMGFMT_UYVY}, {VideoFlag_None, VideoFlag_None } },
    { "raw", "raw", FOURCC_TAG('U','Y','V','Y'),VCodecStatus_Working, {IMGFMT_UYVY}, {VideoFlag_None, VideoFlag_None } },
    { "raw", "raw", FOURCC_TAG('U','Y','N','V'),VCodecStatus_Working, {IMGFMT_UYVY}, {VideoFlag_None, VideoFlag_None } },
    { "raw", "raw", FOURCC_TAG('U','Y','N','Y'),VCodecStatus_Working, {IMGFMT_UYVY}, {VideoFlag_None, VideoFlag_None } },

    { "raw", "raw", FOURCC_TAG('4','4','4','P'),VCodecStatus_Working, {IMGFMT_444P}, {VideoFlag_None, VideoFlag_None } },
    { "raw", "raw", FOURCC_TAG('P','4','4','4'),VCodecStatus_Working, {IMGFMT_444P}, {VideoFlag_None, VideoFlag_None } },
    { "raw", "raw", FOURCC_TAG('Y','V','2','4'),VCodecStatus_Working, {IMGFMT_444P}, {VideoFlag_None, VideoFlag_None } },

    { "raw", "raw", FOURCC_TAG('4','2','2','P'),VCodecStatus_Working, {IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "raw", "raw", FOURCC_TAG('P','4','2','2'),VCodecStatus_Working, {IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "raw", "raw", FOURCC_TAG('Y','4','2','B'),VCodecStatus_Working, {IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },
    { "raw", "raw", FOURCC_TAG('Y','V','1','6'),VCodecStatus_Working, {IMGFMT_422P}, {VideoFlag_None, VideoFlag_None } },

    { "raw", "raw", FOURCC_TAG('Y','V','1','2'),VCodecStatus_Working, {IMGFMT_YV12}, {VideoFlag_None, VideoFlag_None } },
    { "raw", "raw", FOURCC_TAG('I','4','2','0'),VCodecStatus_Working, {IMGFMT_I420}, {VideoFlag_None, VideoFlag_None } },
    { "raw", "raw", FOURCC_TAG('I','Y','U','V'),VCodecStatus_Working, {IMGFMT_I420}, {VideoFlag_None, VideoFlag_None } },

    { "raw", "raw", FOURCC_TAG('N','V','2','1'),VCodecStatus_Working, {IMGFMT_NV21}, {VideoFlag_None, VideoFlag_None } },
    { "raw", "raw", FOURCC_TAG('N','V','1','2'),VCodecStatus_Working, {IMGFMT_NV12}, {VideoFlag_None, VideoFlag_None } },
    { "raw", "raw", FOURCC_TAG('H','M','1','2'),VCodecStatus_Working, {IMGFMT_YV12}, {VideoFlag_None, VideoFlag_None } },

    { "raw", "raw", FOURCC_TAG('Y','V','U','9'),VCodecStatus_Working, {IMGFMT_YVU9}, {VideoFlag_None, VideoFlag_None } },
    { "raw", "raw", FOURCC_TAG('Y','8','0','0'),VCodecStatus_Working, {IMGFMT_Y800}, {VideoFlag_None, VideoFlag_None } },
    { NULL, NULL, 0x0, VCodecStatus_NotWorking, {0x0}, { VideoFlag_None }}
};

static const video_probe_t* __FASTCALL__ probe(vd_private_t *ctx,uint32_t fourcc) {
    UNUSED(ctx);
    unsigned i;
    for(i=0;probes[i].driver;i++)
	if(fourcc==probes[i].fourcc)
	    return &probes[i];
    return NULL;
}

// to set/get/query special features/parameters
static MPXP_Rc control_vd(vd_private_t *ctx,int cmd,any_t* arg,...){
    UNUSED(ctx);
    switch(cmd) {
	case VDCTRL_QUERY_FORMAT:
	    return MPXP_True;
	default: break;
    }
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
    // set format fourcc for raw RGB:
    if(sh->fourcc==0){
	switch(sh->bih->biBitCount){
	case 8: sh->fourcc=IMGFMT_BGR8; break;
	case 15:
	case 16: sh->fourcc=IMGFMT_BGR15; break;
	case 24: sh->fourcc=IMGFMT_BGR24; break;
	case 32: sh->fourcc=IMGFMT_BGR32; break;
	default:
	    MSG_WARN("RAW: depth %d not supported\n",sh->bih->biBitCount);
	}
    }
    priv->parent=opaque;
    return mpcodecs_config_vf(opaque,sh->src_w,sh->src_h);
}

// uninit driver
static void uninit(vd_private_t *ctx) { delete ctx; }

// decode a frame
static mp_image_t* decode(vd_private_t *priv,const enc_frame_t* frame){
    sh_video_t* sh = priv->sh;
    mp_image_t* mpi;
    if(frame->len<=0) return NULL; // skipped frame

    mpi=mpcodecs_get_image(priv->parent, MP_IMGTYPE_EXPORT, 0, sh->src_w, sh->src_h);
    if(mpi->flags&MP_IMGFLAG_DIRECT) mpi->flags|=MP_IMGFLAG_RENDERED;

    if(mpi->flags&MP_IMGFLAG_PLANAR){
	mpi->planes[0]=reinterpret_cast<unsigned char*>(frame->data);
	mpi->stride[0]=mpi->width;
	switch(sh->codec->outfmt[sh->outfmtidx])
	{
	    default:
	    case IMGFMT_I420:
	    case IMGFMT_IYUV:
	    case IMGFMT_YV12:
		mpi->planes[1]=reinterpret_cast<unsigned char *>(frame->data)+mpi->width*mpi->height;
		mpi->stride[1]=mpi->width/2;
		mpi->planes[2]=mpi->planes[1]+(mpi->width/2)*(mpi->height/2);
		mpi->stride[2]=mpi->width/2;
		break;
	    case IMGFMT_IF09:
		/*
		skipped direction level:
		mpi->planes[3]=data+mpi->width*mpi->height*10/8;
		mpi->stride[3]=mpi->width/4;
		*/
	    case IMGFMT_YVU9:
		mpi->planes[1]=reinterpret_cast<unsigned char *>(frame->data)+mpi->width*mpi->height;
		mpi->stride[1]=mpi->width/4;
		mpi->planes[2]=mpi->planes[1]+(mpi->width/4)*(mpi->height/4);
		mpi->stride[2]=mpi->width/4;
		break;
	    case IMGFMT_Y800:
		break;
	}
    } else {
	mpi->planes[0]=reinterpret_cast<unsigned char *>(frame->data);
	mpi->stride[0]=mpi->width*((mpi->bpp+7)/8);
    }
    return mpi;
}

