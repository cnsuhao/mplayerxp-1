/*  vosub_vidix.c
 *
 *	Copyright (C) Nickols_K <nickols_k@mail.ru> - 2002
 *	Copyright (C) Alex Beregszaszi
 *
 *  You can redistribute this file under terms and conditions
 *  of GNU General Public licence v2.
 *
 * This file contains vidix interface to any mplayer's VO plugin.
 * (Partly based on vesa_lvo.c from mplayer's package)
 */
#include <errno.h>
#include <inttypes.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "mp_config.h"
#include "mplayerxp.h"
#include "osdep/mplib.h"

#include <vidix/vidixlib.h>

#include "video_out.h"
#include "vosub_vidix.h"
#include "osdep/fastmemcpy.h"
#include "osd.h"
#include "sub.h"
#include "video_out.h"
#include "dri_vo.h"
#include "xmpcore/mp_image.h"
#include "vo_msg.h"

#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif
#ifndef max
#define max(a,b) ((a)>(b)?(a):(b))
#endif

#define NUM_FRAMES MAX_DRI_BUFFERS /* Temporary: driver will overwrite it */

typedef struct priv_s {
    unsigned		image_Bpp,image_height,image_width,src_format,forced_fourcc;

    VDL_HANDLE		handler;
    uint8_t *		mem;
    int			video_on;

    vidix_capability_t*	cap;
    vidix_playback_t*	play;
    vidix_fourcc_t*	fourcc;
    vidix_yuv_t*	dstrides;

    const vo_functions_t*vo_server;

    int			inited;
    vidix_video_eq_t	eq;

/* bus mastering */
    int			bm_locked; /* requires root privelegies */
    uint8_t *		bm_buffs[NUM_FRAMES];
    unsigned		bm_total_frames,bm_slow_frames;

    vidix_dma_t		vdma;
}priv_t;

static uint32_t (* __FASTCALL__ server_control)(vo_data_t*,uint32_t request, any_t*data);

static int __FASTCALL__ vidix_get_video_eq(const vo_data_t* vo,vo_videq_t *info);
static int __FASTCALL__ vidix_set_video_eq(const vo_data_t* vo,const vo_videq_t *info);
static int __FASTCALL__ vidix_get_num_fx(const vo_data_t* vo,unsigned *info);
static int __FASTCALL__ vidix_get_oem_fx(const vo_data_t* vo,vidix_oem_fx_t *info);
static int __FASTCALL__ vidix_set_oem_fx(const vo_data_t* vo,const vidix_oem_fx_t *info);
static int __FASTCALL__ vidix_set_deint(const vo_data_t* vo,const vidix_deinterlace_t *info);

int vidix_start(vo_data_t* vo)
{
    priv_t*priv=(priv_t*)vo->priv3;
    int err;
    if((err=vdlPlaybackOn(priv->handler))!=0) {
	MSG_FATAL("Can't start playback: %s\n",strerror(err));
	return -1;
    }
    priv->video_on=1;
    if (priv->cap->flags & FLAG_EQUALIZER) {
	MSG_V("vo_gamma_brightness=%i\n"
	      "vo_gamma_saturation=%i\n"
	      "vo_gamma_contrast=%i\n"
	      "vo_gamma_hue=%i\n"
	      "vo_gamma_red_intensity=%i\n"
	      "vo_gamma_green_intensity=%i\n"
	      "vo_gamma_blue_intensity=%i\n"
	       ,vo_conf.gamma.brightness
	       ,vo_conf.gamma.saturation
	       ,vo_conf.gamma.contrast
	       ,vo_conf.gamma.hue
	       ,vo_conf.gamma.red_intensity
	       ,vo_conf.gamma.green_intensity
	       ,vo_conf.gamma.blue_intensity);
	/* To use full set of priv->eq.cap */
	if(vdlPlaybackGetEq(priv->handler,&priv->eq) == 0) {
	    priv->eq.brightness = vo_conf.gamma.brightness;
	    priv->eq.saturation = vo_conf.gamma.saturation;
	    priv->eq.contrast = vo_conf.gamma.contrast;
	    priv->eq.hue = vo_conf.gamma.hue;
	    priv->eq.red_intensity = vo_conf.gamma.red_intensity;
	    priv->eq.green_intensity = vo_conf.gamma.green_intensity;
	    priv->eq.blue_intensity = vo_conf.gamma.blue_intensity;
	    priv->eq.flags = VEQ_FLG_ITU_R_BT_601;
	    vdlPlaybackSetEq(priv->handler,&priv->eq);
	}
    }
    return 0;
}

int vidix_stop(vo_data_t* vo)
{
    priv_t*priv=(priv_t*)vo->priv3;
    int err;
    if((err=vdlPlaybackOff(priv->handler))!=0) {
	MSG_ERR("Can't stop playback: %s\n",strerror(err));
	return -1;
    }
    priv->video_on=0;
    return 0;
}

void vidix_term(vo_data_t* vo)
{
    priv_t*priv=(priv_t*)vo->priv3;
    size_t i;
    priv->inited=0;
    MSG_DBG2("vidix_term() was called\n");
    vidix_stop(vo);
    vdlClose(priv->handler);
    if(vo_conf.use_bm) {
	for(i=0;i<vo_conf.xp_buffs;i++) {
	    if(priv->bm_locked) munlock(priv->bm_buffs[i],priv->play->frame_size);
	    mp_free(priv->bm_buffs[i]);
	    priv->bm_buffs[i]=NULL;
	}
	if(priv->bm_slow_frames)
	    MSG_WARN("from %u frames %u were copied through memcpy()\n"
			,priv->bm_total_frames,priv->bm_slow_frames);
    }
    vdlFreeCapabilityS(priv->cap); priv->cap=NULL;
    vdlFreePlaybackS(priv->play); priv->play=NULL;
    vdlFreeFourccS(priv->fourcc); priv->fourcc=NULL;
    vdlFreeYUVS(priv->dstrides); priv->dstrides=NULL;
}

static void __FASTCALL__ vidix_copy_dma(const vo_data_t* vo,unsigned idx,int sync_mode)
{
    priv_t*priv=(priv_t*)vo->priv3;
    int err,i;
    int dma_busy;
    MSG_DBG2("vidix_copy_dma(%u,%i) was called\n",idx,sync_mode);
    priv->bm_total_frames++;
    if(idx > priv->play->num_frames-1 && priv->play->num_frames>1) {
	MSG_FATAL("\nDetected internal error!\n"
		"Request to copy %u frame into %u array\n",idx,priv->play->num_frames);
	return;
    }
    dma_busy = vdlQueryDMAStatus(priv->handler);
    i=5;
    if(!sync_mode)
	while(dma_busy && i) { usleep(0); dma_busy = vdlQueryDMAStatus(priv->handler); i--; }
    if(!dma_busy || sync_mode) {
	priv->vdma.src = priv->bm_buffs[idx];
	priv->vdma.dest_offset = priv->play->offsets[priv->play->num_frames>1?idx:0];
	priv->vdma.size = priv->play->frame_size;
	priv->vdma.flags = sync_mode?BM_DMA_SYNC:BM_DMA_ASYNC;
	if(priv->bm_locked) priv->vdma.flags |= BM_DMA_FIXED_BUFFS;
	priv->vdma.idx = idx;
	err=vdlPlaybackCopyFrame(priv->handler,&priv->vdma);
	if(err) {
	    /* We can switch back to DR here but for now exit */
	    MSG_FATAL("\nerror '%s' occured during DMA transfer\n"
			"Please send BUGREPORT to developers!!!\n",strerror(err));
	    exit(EXIT_FAILURE); /* it's OK vidix_term will be called */
	}
#if 0
	printf("frame is DMA copied\n");
#endif
    } else {
	memcpy(priv->play->dga_addr+priv->play->offsets[0],priv->bm_buffs[idx],priv->play->frame_size);
	MSG_WARN("DMA frame is memcpy() copied\n");
	priv->bm_slow_frames++;
    }
}

void __FASTCALL__ vidix_select_frame(vo_data_t* vo,unsigned idx)
{
    priv_t*priv=(priv_t*)vo->priv3;
    MSG_DBG2("vidix_select_frame() was called\n");
    if(vo_conf.use_bm == 1) vidix_copy_dma(vo,idx,0);
    else vdlPlaybackFrameSelect(priv->handler,idx);
}

uint32_t __FASTCALL__ vidix_query_fourcc(const vo_data_t* vo,vo_query_fourcc_t* format)
{
    priv_t*priv=(priv_t*)vo->priv3;
    MSG_DBG2("query_format was called: %x (%s)\n",format->fourcc,vo_format_name(format->fourcc));
    priv->fourcc->fourcc = format->fourcc;
    priv->fourcc->srcw = format->w;
    priv->fourcc->srch = format->h;
    vdlQueryFourcc(priv->handler,priv->fourcc);
    return priv->fourcc->depth == VID_DEPTH_NONE ? 0 : 0x02;
}

int __FASTCALL__ vidix_grkey_support(const vo_data_t* vo)
{
    priv_t*priv=(priv_t*)vo->priv3;
    int retval = priv->fourcc->flags & VID_CAP_COLORKEY;
    MSG_DBG2("query_grkey_support: %i\n",retval);
    return retval;
}

int __FASTCALL__ vidix_grkey_get(const vo_data_t* vo,vidix_grkey_t *gr_key)
{
    priv_t*priv=(priv_t*)vo->priv3;
    return vdlGetGrKeys(priv->handler, gr_key);
}

int __FASTCALL__ vidix_grkey_set(const vo_data_t* vo,const vidix_grkey_t *gr_key)
{
    priv_t*priv=(priv_t*)vo->priv3;
    return vdlSetGrKeys(priv->handler, gr_key);
}

static int __FASTCALL__ vidix_get_video_eq(const vo_data_t* vo,vo_videq_t *info)
{
    priv_t*priv=(priv_t*)vo->priv3;
    int rval;
    vidix_video_eq_t eq;
    if(!priv->video_on) return EPERM;
    rval = vdlPlaybackGetEq(priv->handler, &eq);
    if(!rval) {
	if(!strcmp(info->name,VO_EC_BRIGHTNESS) && eq.cap&VEQ_CAP_BRIGHTNESS)
	    info->value=eq.brightness;
	else if(!strcmp(info->name,VO_EC_SATURATION) && eq.cap&VEQ_CAP_SATURATION)
	    info->value=eq.saturation;
	else if(!strcmp(info->name,VO_EC_CONTRAST) && eq.cap&VEQ_CAP_CONTRAST)
	    info->value=eq.contrast;
	else if(!strcmp(info->name,VO_EC_HUE) && eq.cap&VEQ_CAP_HUE)
	    info->value=eq.hue;
	else if(!strcmp(info->name,VO_EC_RED_INTENSITY) && eq.cap&VEQ_CAP_RGB_INTENSITY)
	    info->value=eq.red_intensity;
	else if(!strcmp(info->name,VO_EC_GREEN_INTENSITY) && eq.cap&VEQ_CAP_RGB_INTENSITY)
	    info->value=eq.green_intensity;
	else if(!strcmp(info->name,VO_EC_BLUE_INTENSITY) && eq.cap&VEQ_CAP_RGB_INTENSITY)
	    info->value=eq.blue_intensity;
    }
    return rval;
}

static int __FASTCALL__ vidix_set_video_eq(const vo_data_t* vo,const vo_videq_t *info)
{
    priv_t*priv=(priv_t*)vo->priv3;
    int rval;
    vidix_video_eq_t eq;
    if(!priv->video_on) return EPERM;
    rval= vdlPlaybackGetEq(priv->handler, &eq);
    if(!rval) {
	if(!strcmp(info->name,VO_EC_BRIGHTNESS) && eq.cap&VEQ_CAP_BRIGHTNESS)
	    eq.brightness=info->value;
	else if(!strcmp(info->name,VO_EC_SATURATION) && eq.cap&VEQ_CAP_SATURATION)
	    eq.saturation=info->value;
	else if(!strcmp(info->name,VO_EC_CONTRAST) && eq.cap&VEQ_CAP_CONTRAST)
	    eq.contrast=info->value;
	else if(!strcmp(info->name,VO_EC_HUE) && eq.cap&VEQ_CAP_HUE)
	    eq.hue=info->value;
	else if(!strcmp(info->name,VO_EC_RED_INTENSITY) && eq.cap&VEQ_CAP_RGB_INTENSITY)
	    eq.red_intensity=info->value;
	else if(!strcmp(info->name,VO_EC_GREEN_INTENSITY) && eq.cap&VEQ_CAP_RGB_INTENSITY)
	    eq.green_intensity=info->value;
	else if(!strcmp(info->name,VO_EC_BLUE_INTENSITY) && eq.cap&VEQ_CAP_RGB_INTENSITY)
	    eq.blue_intensity=info->value;
	rval= vdlPlaybackSetEq(priv->handler, &eq);
    }
    return rval;
}

static int __FASTCALL__ vidix_get_num_fx(const vo_data_t* vo,unsigned *info)
{
    priv_t*priv=(priv_t*)vo->priv3;
    if(!priv->video_on) return EPERM;
    return vdlQueryNumOemEffects(priv->handler, info);
}

static int __FASTCALL__ vidix_get_oem_fx(const vo_data_t* vo,vidix_oem_fx_t *info)
{
    priv_t*priv=(priv_t*)vo->priv3;
    if(!priv->video_on) return EPERM;
    return vdlGetOemEffect(priv->handler, info);
}

static int __FASTCALL__ vidix_set_oem_fx(const vo_data_t* vo,const vidix_oem_fx_t *info)
{
    priv_t*priv=(priv_t*)vo->priv3;
    if(!priv->video_on) return EPERM;
    return vdlSetOemEffect(priv->handler, info);
}

static int __FASTCALL__ vidix_set_deint(const vo_data_t* vo,const vidix_deinterlace_t *info)
{
    priv_t*priv=(priv_t*)vo->priv3;
    if(!priv->video_on) return EPERM;
    return vdlPlaybackSetDeint(priv->handler, info);
}

#ifndef HAVE_MLOCK
/* stubs */
int mlock(const any_t*addr,size_t len) { return ENOSYS; }
int munlock(const any_t*addr,size_t len) { return ENOSYS; }
#endif

static MPXP_Rc alloc_vidix_structs(any_t* ctx) {
    priv_t* priv=(priv_t*)ctx;
    if(!priv->cap) priv->cap = vdlAllocCapabilityS();
    if(!priv->play) priv->play = vdlAllocPlaybackS();
    if(!priv->fourcc) priv->fourcc = vdlAllocFourccS();
    if(!priv->dstrides) priv->dstrides = vdlAllocYUVS();
    if(!(priv->cap && priv->play && priv->fourcc && priv->dstrides)) {
	 MSG_FATAL("Can not alloc certain structures\n");
	 return MPXP_False;
    }
    return MPXP_Ok;
}

MPXP_Rc  __FASTCALL__ vidix_init(vo_data_t* vo,unsigned src_width,unsigned src_height,
		   unsigned x_org,unsigned y_org,unsigned dst_width,
		   unsigned dst_height,unsigned format,unsigned dest_bpp,
		   unsigned vid_w,unsigned vid_h)
{
    priv_t*priv=(priv_t*)vo->priv3;
    size_t i;
    int err;
    static int video_clean=0;
    uint32_t apitch;
    MSG_DBG2("vidix_init() was called\n"
	    "src_w=%u src_h=%u dest_x_y_w_h = %u %u %u %u\n"
	    "format=%s dest_bpp=%u vid_w=%u vid_h=%u\n"
	    ,src_width,src_height,x_org,y_org,dst_width,dst_height
	    ,vo_format_name(format),dest_bpp,vid_w,vid_h);
    if(((priv->cap->maxwidth != -1) && (vid_w > (unsigned)priv->cap->maxwidth)) ||
	    ((priv->cap->minwidth != -1) && (vid_w < (unsigned)priv->cap->minwidth)) ||
	    ((priv->cap->maxheight != -1) && (vid_h > (unsigned)priv->cap->maxheight)) ||
	    ((priv->cap->minwidth != -1 ) && (vid_h < (unsigned)priv->cap->minheight))) {
	MSG_FATAL("video server has unsupported resolution (%dx%d), supported: %dx%d-%dx%d\n",
		vid_w, vid_h, priv->cap->minwidth, priv->cap->minheight,
		priv->cap->maxwidth, priv->cap->maxheight);
	return MPXP_False;
    }
    priv->fourcc->fourcc = format;
    vdlQueryFourcc(priv->handler,priv->fourcc);
    err = 0;
    switch(dest_bpp) {
	case 1: err = ((priv->fourcc->depth & VID_DEPTH_1BPP) != VID_DEPTH_1BPP); break;
	case 2: err = ((priv->fourcc->depth & VID_DEPTH_2BPP) != VID_DEPTH_2BPP); break;
	case 4: err = ((priv->fourcc->depth & VID_DEPTH_4BPP) != VID_DEPTH_4BPP); break;
	case 8: err = ((priv->fourcc->depth & VID_DEPTH_8BPP) != VID_DEPTH_8BPP); break;
	case 12:err = ((priv->fourcc->depth & VID_DEPTH_12BPP) != VID_DEPTH_12BPP); break;
	case 15:err = ((priv->fourcc->depth & VID_DEPTH_15BPP) != VID_DEPTH_15BPP); break;
	case 16:err = ((priv->fourcc->depth & VID_DEPTH_16BPP) != VID_DEPTH_16BPP); break;
	case 24:err = ((priv->fourcc->depth & VID_DEPTH_24BPP) != VID_DEPTH_24BPP); break;
	case 32:err = ((priv->fourcc->depth & VID_DEPTH_32BPP) != VID_DEPTH_32BPP); break;
	default: err=1; break;
    }
    if(err) {
	MSG_FATAL("video server has unsupported color depth by vidix (%d)\n"
		,priv->fourcc->depth);
		return MPXP_False;
    }
    if((dst_width > src_width || dst_height > src_height) && (priv->cap->flags & FLAG_UPSCALER) != FLAG_UPSCALER) {
	MSG_FATAL("vidix driver can't upscale image (%d%d -> %d%d)\n",
		src_width, src_height, dst_width, dst_height);
	return MPXP_False;
    }
    if((dst_width > src_width || dst_height > src_height) && (priv->cap->flags & FLAG_DOWNSCALER) != FLAG_DOWNSCALER) {
	MSG_FATAL("vidix driver can't downscale image (%d%d -> %d%d)\n",
		src_width, src_height, dst_width, dst_height);
	return MPXP_False;
    }
    priv->image_width = src_width;
    priv->image_height = src_height;
    priv->src_format = format;
    if(priv->forced_fourcc) format = priv->forced_fourcc;
    memset(priv->play,0,sizeof(vidix_playback_t));
    priv->play->fourcc = format;
    priv->play->capability = priv->cap->flags; /* every ;) */
    priv->play->blend_factor = 0; /* for now */
    /* display the full picture.
	Nick: we could implement here zooming to a specified area -- alex */
    priv->play->src.x = priv->play->src.y = 0;
    priv->play->src.w = src_width;
    priv->play->src.h = src_height;
    priv->play->dest.x = x_org;
    priv->play->dest.y = y_org;
    priv->play->dest.w = dst_width;
    priv->play->dest.h = dst_height;
    priv->play->num_frames=(vo_conf.use_bm!=1)?NUM_FRAMES-1:1;
    if(priv->play->num_frames > vo_conf.xp_buffs) priv->play->num_frames = vo_conf.xp_buffs;
    priv->play->src.pitch.y = priv->play->src.pitch.u = priv->play->src.pitch.v = 0;
    if((err=vdlConfigPlayback(priv->handler,priv->play))!=0) {
	MSG_FATAL("Can't configure playback: %s\n",strerror(err));
	return MPXP_False;
    }
    MSG_V("using %d buffers\n", priv->play->num_frames);
    /* configure busmastering */
    if(vo_conf.use_bm) {
#ifdef HAVE_MEMALIGN
	if(priv->cap->flags & FLAG_DMA) {
	    int psize = getpagesize();
	    priv->bm_locked=1;
	    for(i=0;i<vo_conf.xp_buffs;i++) {
		if(!priv->bm_buffs[i]) priv->bm_buffs[i] = mp_memalign(psize, priv->play->frame_size);
		if(!(priv->bm_buffs[i])) {
		    MSG_ERR("Can't allocate memory for busmastering\n");
		    return MPXP_False;
		}
		if(mlock(priv->bm_buffs[i],priv->play->frame_size) != 0) {
		    unsigned j;
		    MSG_WARN("Can't lock memory for busmastering\n");
		    for(j=0;j<i;j++) munlock(priv->bm_buffs[i],priv->play->frame_size);
		    priv->bm_locked=0;
		}
	    }
	    memset(&priv->vdma,0,sizeof(vidix_dma_t));
	    priv->bm_total_frames=priv->bm_slow_frames=0;
	}
	else
#else
	    MSG_ERR("Won't configure bus mastering: your system doesn't support mp_memalign()\n");
#endif
	MSG_ERR("Can not configure bus mastering: your driver is not DMA capable\n");
	vo_conf.use_bm = 0;
    }
    if(vo_conf.use_bm) MSG_OK("using BUSMASTERING\n");
    priv->mem = priv->play->dga_addr;

    if(!video_clean) {
	/*  clear every frame with correct address and frame_size
	    only once per session */
	for (i = 0; i < priv->play->num_frames; i++)
	    memset(priv->mem + priv->play->offsets[i], 0x80, priv->play->frame_size);
	video_clean=1;
    }
    MSG_DBG2("vidix returns pitches %u %u %u\n",priv->play->dest.pitch.y,priv->play->dest.pitch.u,priv->play->dest.pitch.v);
    switch(format) {
	case IMGFMT_Y800:
	case IMGFMT_YVU9:
	case IMGFMT_IF09:
	case IMGFMT_I420:
	case IMGFMT_IYUV:
	case IMGFMT_YV12:
	    apitch = priv->play->dest.pitch.y-1;
	    priv->dstrides->y = (priv->image_width + apitch) & ~apitch;
	    apitch = priv->play->dest.pitch.v-1;
	    priv->dstrides->v = (priv->image_width + apitch) & ~apitch;
	    apitch = priv->play->dest.pitch.u-1;
	    priv->dstrides->u = (priv->image_width + apitch) & ~apitch;
	    priv->image_Bpp=1;
	    break;
	case IMGFMT_RGB32:
	case IMGFMT_BGR32:
	    apitch = priv->play->dest.pitch.y-1;
	    priv->dstrides->y = (priv->image_width*4 + apitch) & ~apitch;
	    priv->dstrides->u = priv->dstrides->v = 0;
	    priv->image_Bpp=4;
	    break;
	case IMGFMT_RGB24:
	case IMGFMT_BGR24:
	    apitch = priv->play->dest.pitch.y-1;
	    priv->dstrides->y = (priv->image_width*3 + apitch) & ~apitch;
	    priv->dstrides->u = priv->dstrides->v = 0;
	    priv->image_Bpp=3;
	    break;
	default:
	    apitch = priv->play->dest.pitch.y-1;
	    priv->dstrides->y = (priv->image_width*2 + apitch) & ~apitch;
	    priv->dstrides->u = priv->dstrides->v = 0;
	    priv->image_Bpp=2;
	    break;
    }
    switch(format) {
	case IMGFMT_YVU9:
	case IMGFMT_IF09:
	    priv->dstrides->u /= 4;
	    priv->dstrides->v /= 4;
	    break;
	case IMGFMT_I420:
	case IMGFMT_IYUV:
	case IMGFMT_YV12:
	    priv->dstrides->u /= 2;
	    priv->dstrides->v /= 2;
	    break;
    }
    return MPXP_Ok;
}

static void __FASTCALL__ vidix_dri_get_surface_caps(const vo_data_t* vo,dri_surface_cap_t *caps)
{
    priv_t*priv=(priv_t*)vo->priv3;
    caps->caps = vo_conf.use_bm ? DRI_CAP_TEMP_VIDEO|DRI_CAP_BUSMASTERING : DRI_CAP_VIDEO_MMAPED;
    caps->caps |= DRI_CAP_HORZSCALER | DRI_CAP_VERTSCALER;
    if((priv->cap->flags & FLAG_DOWNSCALER) == FLAG_DOWNSCALER)
	    caps->caps |= DRI_CAP_DOWNSCALER;
    if((priv->cap->flags & FLAG_UPSCALER) == FLAG_UPSCALER)
	    caps->caps |= DRI_CAP_UPSCALER;
    caps->fourcc = priv->play->fourcc;
    caps->width=priv->play->src.w;
    caps->height=priv->play->src.h;
    /* in case of vidix movie fit surface */
    caps->x = caps->y=0;
    caps->w=caps->width;
    caps->h=caps->height;
    if(priv->dstrides) {
	caps->strides[0] = priv->dstrides->y;
	caps->strides[1] = priv->dstrides->v;
	caps->strides[2] = priv->dstrides->u;
	caps->strides[3] = 0;
    }
}

static void __FASTCALL__ vidix_dri_get_surface(const vo_data_t* vo,dri_surface_t *surf)
{
    priv_t*priv=(priv_t*)vo->priv3;
    if(vo_conf.use_bm) {
	surf->planes[0] = priv->bm_buffs[surf->idx] + priv->play->offset.y;
	surf->planes[1] = priv->bm_buffs[surf->idx] + priv->play->offset.v;
	surf->planes[2] = priv->bm_buffs[surf->idx] + priv->play->offset.u;
    } else {
	surf->planes[0] = priv->mem + priv->play->offsets[surf->idx] + priv->play->offset.y;
	surf->planes[1] = priv->mem + priv->play->offsets[surf->idx] + priv->play->offset.v;
	surf->planes[2] = priv->mem + priv->play->offsets[surf->idx] + priv->play->offset.u;
    }
    surf->planes[3] = 0;
}

MPXP_Rc __FASTCALL__ vidix_control(vo_data_t* vo,uint32_t request, any_t*data)
{
    priv_t*priv=(priv_t*)vo->priv3;
    switch (request) {
	case VOCTRL_QUERY_FORMAT:
	    return vidix_query_fourcc(vo,(vo_query_fourcc_t*)data);
	case VOCTRL_FULLSCREEN:
	case VOCTRL_CHECK_EVENTS:
	    if(priv->inited) return (*server_control)(vo,request,data);
	    break;
	case VOCTRL_GET_NUM_FRAMES:
	    *(uint32_t *)data = (vo_conf.use_bm == 1) ? vo_conf.xp_buffs : priv->play->num_frames;
	    return MPXP_True;
	case DRI_GET_SURFACE_CAPS:
	    vidix_dri_get_surface_caps(vo,data);
	    return MPXP_True;
	case DRI_GET_SURFACE:
	    vidix_dri_get_surface(vo,data);
	    return MPXP_True;
	case VOCTRL_FLUSH_PAGES:
	    if(vo_conf.use_bm > 1) vidix_copy_dma(vo,*(uint32_t *)data,1);
	    return MPXP_True;
	case VOCTRL_GET_EQUALIZER:
	    if(!vidix_get_video_eq(vo,data)) return MPXP_True;
	    else return MPXP_False;
	case VOCTRL_SET_EQUALIZER:
	    if(!vidix_set_video_eq(vo,data)) return MPXP_True;
	    else return MPXP_False;
    }
    return MPXP_NA;
}

MPXP_Rc __FASTCALL__ vidix_preinit(vo_data_t*vo,const char *drvname,const any_t*server)
{
    int err;
    static int reent=0;
    MSG_DBG2("vidix_preinit(%s) was called\n",drvname);
    priv_t* priv=mp_mallocz(sizeof(priv_t));
    vo->priv3=priv;
    if(alloc_vidix_structs(priv)!=MPXP_Ok) return MPXP_False;
    if(vdlGetVersion() != VIDIX_VERSION) {
	MSG_FATAL("You have wrong version of VIDIX library\n");
	mp_free(priv);
	return MPXP_False;
    }
    priv->handler = vdlOpen(VIDIX_PATH,
			drvname ? drvname[0] == ':' ? &drvname[1] : drvname[0] ? drvname : NULL : NULL,
			TYPE_OUTPUT,
			mp_conf.verbose);
    if(priv->handler == NULL) {
	MSG_FATAL("Couldn't find working VIDIX driver\n");
	mp_free(priv);
	return MPXP_False;
    }
    if((err=vdlGetCapability(priv->handler,priv->cap)) != 0) {
	MSG_FATAL("Couldn't get capability: %s\n",strerror(err));
	mp_free(priv);
	return MPXP_False;
    }
    else MSG_V("Driver capability: %X\n",priv->cap->flags);
    MSG_V("Using: %s by %s\n",priv->cap->name,priv->cap->author);
    /* we are able to tune up this stuff depend on fourcc format */
    ((vo_functions_t *)server)->select_frame=vidix_select_frame;
    if(!reent) {
	server_control = ((vo_functions_t *)server)->control;
	((vo_functions_t *)server)->control=vidix_control;
	reent=1;
    }
    priv->vo_server = server;
    priv->inited=1;
    return MPXP_Ok;
}
