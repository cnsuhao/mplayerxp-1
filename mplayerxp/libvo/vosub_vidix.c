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

#include "../mp_config.h"
#include "../mplayer.h"
#ifdef HAVE_MEMALIGN
#include <malloc.h>
#endif

#include <vidix/vidixlib.h>

#include "video_out.h"
#include "vosub_vidix.h"
#include "fastmemcpy.h"
#include "osd.h"
#include "sub.h"
#include "video_out.h"
#include "dri_vo.h"
#include "../mp_image.h"
#include "vo_msg.h"

#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif
#ifndef max
#define max(a,b) ((a)>(b)?(a):(b))
#endif

#define NUM_FRAMES MAX_DRI_BUFFERS /* Temporary: driver will overwrite it */
#define UNUSED(x) ((void)(x)) /* Removes warning about unused arguments */

static VDL_HANDLE vidix_handler = NULL;
static uint8_t *vidix_mem = NULL;
static unsigned image_Bpp,image_height,image_width,src_format,forced_fourcc=0;
static int video_on=0;

static vidix_capability_t *vidix_cap;
static vidix_playback_t   *vidix_play;
static vidix_fourcc_t	  *vidix_fourcc;
static vidix_yuv_t	  *dstrides;
static const vo_functions_t *   vo_server;
static uint32_t (* __FASTCALL__ server_control)(uint32_t request, void *data);
static int plugin_inited=0;

/* bus mastering */
static int dma_mem_locked=0; /* requires root privelegies */
static uint8_t *bm_buffs[NUM_FRAMES];
static unsigned bm_total_frames,bm_slow_frames;

static int __FASTCALL__ vidix_get_video_eq(vo_videq_t *info);
static int __FASTCALL__ vidix_set_video_eq(const vo_videq_t *info);
static int __FASTCALL__ vidix_get_num_fx(unsigned *info);
static int __FASTCALL__ vidix_get_oem_fx(vidix_oem_fx_t *info);
static int __FASTCALL__ vidix_set_oem_fx(const vidix_oem_fx_t *info);
static int __FASTCALL__ vidix_set_deint(const vidix_deinterlace_t *info);

static vidix_video_eq_t vid_eq;

int vidix_start(void)
{
    int err;
    if((err=vdlPlaybackOn(vidix_handler))!=0)
    {
	MSG_FATAL("Can't start playback: %s\n",strerror(err));
	return -1;
    }
    video_on=1;
    if (vidix_cap->flags & FLAG_EQUALIZER)
    {
	MSG_V("vo_gamma_brightness=%i\n"
	      "vo_gamma_saturation=%i\n"
	      "vo_gamma_contrast=%i\n"
	      "vo_gamma_hue=%i\n"
	      "vo_gamma_red_intensity=%i\n"
	      "vo_gamma_green_intensity=%i\n"
	      "vo_gamma_blue_intensity=%i\n"
	       ,vo_gamma_brightness
	       ,vo_gamma_saturation
	       ,vo_gamma_contrast
	       ,vo_gamma_hue
	       ,vo_gamma_red_intensity
	       ,vo_gamma_green_intensity
	       ,vo_gamma_blue_intensity);
        /* To use full set of vid_eq.cap */
	if(vdlPlaybackGetEq(vidix_handler,&vid_eq) == 0)
	{
		vid_eq.brightness = vo_gamma_brightness;
		vid_eq.saturation = vo_gamma_saturation;
		vid_eq.contrast = vo_gamma_contrast;
		vid_eq.hue = vo_gamma_hue;
		vid_eq.red_intensity = vo_gamma_red_intensity;
		vid_eq.green_intensity = vo_gamma_green_intensity;
		vid_eq.blue_intensity = vo_gamma_blue_intensity;
		vid_eq.flags = VEQ_FLG_ITU_R_BT_601;
		vdlPlaybackSetEq(vidix_handler,&vid_eq);
	}
    }
    return 0;
}

int vidix_stop(void)
{
    int err;
    if((err=vdlPlaybackOff(vidix_handler))!=0)
    {
	MSG_ERR("Can't stop playback: %s\n",strerror(err));
	return -1;
    }
    video_on=0;
    return 0;
}

void vidix_term( void )
{
    size_t i;
    plugin_inited=0;
    MSG_DBG2("vidix_term() was called\n");
    vidix_stop();
    vdlClose(vidix_handler);
    if(vo_use_bm)
    {
	for(i=0;i<vo_da_buffs;i++)
	{
	    if(dma_mem_locked) munlock(bm_buffs[i],vidix_play->frame_size);
	    free(bm_buffs[i]);
	    bm_buffs[i]=NULL;
	}
	if(bm_slow_frames)
		MSG_WARN("from %u frames %u were copied through memcpy()\n"
			,bm_total_frames,bm_slow_frames);
    }
    vdlFreeCapabilityS(vidix_cap); vidix_cap=NULL;
    vdlFreePlaybackS(vidix_play); vidix_play=NULL;
    vdlFreeFourccS(vidix_fourcc); vidix_fourcc=NULL;
    vdlFreeYUVS(dstrides); dstrides=NULL;
}

static	vidix_dma_t vdma;
static void __FASTCALL__ vidix_copy_dma(unsigned idx,int sync_mode)
{
	int err,i;
	int dma_busy;
	MSG_DBG2("vidix_copy_dma(%u,%i) was called\n",idx,sync_mode);
	bm_total_frames++;
	if(idx > vidix_play->num_frames-1 && vidix_play->num_frames>1)
	{
	    MSG_FATAL("\nDetected internal error!\n"
			"Request to copy %u frame into %u array\n",idx,vidix_play->num_frames);
	    return;
	}
	dma_busy = vdlQueryDMAStatus(vidix_handler);
	i=5;
	if(!sync_mode)
	    while(dma_busy && i) { usleep(0); dma_busy = vdlQueryDMAStatus(vidix_handler); i--; }
	if(!dma_busy || sync_mode)
  	{
		vdma.src = bm_buffs[idx];
		vdma.dest_offset = vidix_play->offsets[vidix_play->num_frames>1?idx:0];
		vdma.size = vidix_play->frame_size;
		vdma.flags = sync_mode?BM_DMA_SYNC:BM_DMA_ASYNC;
		if(dma_mem_locked) vdma.flags |= BM_DMA_FIXED_BUFFS;
		vdma.idx = idx;
		err=vdlPlaybackCopyFrame(vidix_handler,&vdma);
		if(err)
		{
	        /* We can switch back to DR here but for now exit */
  		MSG_FATAL("\nerror '%s' occured during DMA transfer\n"
  			"Please send BUGREPORT to developers!!!\n",strerror(err));
		exit(EXIT_FAILURE); /* it's OK vidix_term will be called */ 
		}
#if 0
printf("frame is DMA copied\n");
#endif
	}
	else
	{
		memcpy(vidix_play->dga_addr+vidix_play->offsets[0],bm_buffs[idx],vidix_play->frame_size);
		MSG_WARN("DMA frame is memcpy() copied\n");
		bm_slow_frames++;
	}
}

void __FASTCALL__ vidix_flip_page(unsigned idx)
{
    MSG_DBG2("vidix_flip_page() was called\n");
    if(vo_use_bm == 1) vidix_copy_dma(idx,0);
    else vdlPlaybackFrameSelect(vidix_handler,idx);
}

uint32_t __FASTCALL__ vidix_query_fourcc(vo_query_fourcc_t* format)
{
  MSG_DBG2("query_format was called: %x (%s)\n",format->fourcc,vo_format_name(format->fourcc));
  vidix_fourcc->fourcc = format->fourcc;
  vidix_fourcc->srcw = format->w;
  vidix_fourcc->srch = format->h;
  vdlQueryFourcc(vidix_handler,vidix_fourcc);
  return vidix_fourcc->depth == VID_DEPTH_NONE ? 0 : 0x02;
}

int __FASTCALL__ vidix_grkey_support(void)
{
    int retval = vidix_fourcc->flags & VID_CAP_COLORKEY;
    MSG_DBG2("query_grkey_support: %i\n",retval);
    return retval;
}

int __FASTCALL__ vidix_grkey_get(vidix_grkey_t *gr_key)
{
    return(vdlGetGrKeys(vidix_handler, gr_key));
}

int __FASTCALL__ vidix_grkey_set(const vidix_grkey_t *gr_key)
{
    return(vdlSetGrKeys(vidix_handler, gr_key));
}

static int __FASTCALL__ vidix_get_video_eq(vo_videq_t *info)
{
  int rval;
  vidix_video_eq_t eq;
  if(!video_on) return EPERM;
  rval = vdlPlaybackGetEq(vidix_handler, &eq);
  if(!rval)
  {
    if(!strcmp(info->name,VO_EC_BRIGHTNESS) && eq.cap&VEQ_CAP_BRIGHTNESS)
	info->value=eq.brightness;
    else
    if(!strcmp(info->name,VO_EC_SATURATION) && eq.cap&VEQ_CAP_SATURATION)
	info->value=eq.saturation;
    else
    if(!strcmp(info->name,VO_EC_CONTRAST) && eq.cap&VEQ_CAP_CONTRAST)
	info->value=eq.contrast;
    else
    if(!strcmp(info->name,VO_EC_HUE) && eq.cap&VEQ_CAP_HUE)
	info->value=eq.hue;
    else
    if(!strcmp(info->name,VO_EC_RED_INTENSITY) && eq.cap&VEQ_CAP_RGB_INTENSITY)
	info->value=eq.red_intensity;
    else
    if(!strcmp(info->name,VO_EC_GREEN_INTENSITY) && eq.cap&VEQ_CAP_RGB_INTENSITY)
	info->value=eq.green_intensity;
    else
    if(!strcmp(info->name,VO_EC_BLUE_INTENSITY) && eq.cap&VEQ_CAP_RGB_INTENSITY)
	info->value=eq.blue_intensity;
  }
  return rval;
}

static int __FASTCALL__ vidix_set_video_eq(const vo_videq_t *info)
{
  int rval;
  vidix_video_eq_t eq;
  if(!video_on) return EPERM;
  rval= vdlPlaybackGetEq(vidix_handler, &eq);
  if(!rval)
  {
    if(!strcmp(info->name,VO_EC_BRIGHTNESS) && eq.cap&VEQ_CAP_BRIGHTNESS)
	eq.brightness=info->value;
    else
    if(!strcmp(info->name,VO_EC_SATURATION) && eq.cap&VEQ_CAP_SATURATION)
	eq.saturation=info->value;
    else
    if(!strcmp(info->name,VO_EC_CONTRAST) && eq.cap&VEQ_CAP_CONTRAST)
	eq.contrast=info->value;
    else
    if(!strcmp(info->name,VO_EC_HUE) && eq.cap&VEQ_CAP_HUE)
	eq.hue=info->value;
    else
    if(!strcmp(info->name,VO_EC_RED_INTENSITY) && eq.cap&VEQ_CAP_RGB_INTENSITY)
	eq.red_intensity=info->value;
    else
    if(!strcmp(info->name,VO_EC_GREEN_INTENSITY) && eq.cap&VEQ_CAP_RGB_INTENSITY)
	eq.green_intensity=info->value;
    else
    if(!strcmp(info->name,VO_EC_BLUE_INTENSITY) && eq.cap&VEQ_CAP_RGB_INTENSITY)
	eq.blue_intensity=info->value;
    rval= vdlPlaybackSetEq(vidix_handler, &eq);
  }
  return rval;
}

static int __FASTCALL__ vidix_get_num_fx(unsigned *info)
{
  if(!video_on) return EPERM;
  return vdlQueryNumOemEffects(vidix_handler, info);
}

static int __FASTCALL__ vidix_get_oem_fx(vidix_oem_fx_t *info)
{
  if(!video_on) return EPERM;
  return vdlGetOemEffect(vidix_handler, info);
}

static int __FASTCALL__ vidix_set_oem_fx(const vidix_oem_fx_t *info)
{
  if(!video_on) return EPERM;
  return vdlSetOemEffect(vidix_handler, info);
}

static int __FASTCALL__ vidix_set_deint(const vidix_deinterlace_t *info)
{
  if(!video_on) return EPERM;
  return vdlPlaybackSetDeint(vidix_handler, info);
}

#ifndef HAVE_MLOCK
/* stubs */
int mlock(const void *addr,size_t len) { return ENOSYS; }
int munlock(const void *addr,size_t len) { return ENOSYS; }
#endif

#define ALLOC_VIDIX_STRUCTS()\
{\
	if(!vidix_cap) vidix_cap = vdlAllocCapabilityS();\
	if(!vidix_play) vidix_play = vdlAllocPlaybackS();\
	if(!vidix_fourcc) vidix_fourcc = vdlAllocFourccS();\
	if(!dstrides) dstrides = vdlAllocYUVS();\
	if(!(vidix_cap && vidix_play && vidix_fourcc && dstrides))\
	{\
	  MSG_FATAL("Can not alloc certain structures\n");\
	  return -1;\
	}\
}

int  __FASTCALL__ vidix_init(unsigned src_width,unsigned src_height,
		   unsigned x_org,unsigned y_org,unsigned dst_width,
		   unsigned dst_height,unsigned format,unsigned dest_bpp,
		   unsigned vid_w,unsigned vid_h,const void *info)
{
  size_t i;
  int err;
  static int video_clean=0;
  uint32_t apitch;
  MSG_DBG2("vidix_init() was called\n"
    	    "src_w=%u src_h=%u dest_x_y_w_h = %u %u %u %u\n"
	    "format=%s dest_bpp=%u vid_w=%u vid_h=%u\n"
	    ,src_width,src_height,x_org,y_org,dst_width,dst_height
	    ,vo_format_name(format),dest_bpp,vid_w,vid_h);
	if(((vidix_cap->maxwidth != -1) && (vid_w > (unsigned)vidix_cap->maxwidth)) ||
	    ((vidix_cap->minwidth != -1) && (vid_w < (unsigned)vidix_cap->minwidth)) ||
	    ((vidix_cap->maxheight != -1) && (vid_h > (unsigned)vidix_cap->maxheight)) ||
	    ((vidix_cap->minwidth != -1 ) && (vid_h < (unsigned)vidix_cap->minheight)))
	{
	  MSG_FATAL("video server has unsupported resolution (%dx%d), supported: %dx%d-%dx%d\n",
	    vid_w, vid_h, vidix_cap->minwidth, vidix_cap->minheight,
	    vidix_cap->maxwidth, vidix_cap->maxheight);
	  return -1;
	}

	vidix_fourcc->fourcc = format;
	vdlQueryFourcc(vidix_handler,vidix_fourcc);
	err = 0;
	switch(dest_bpp)
	{
	  case 1: err = ((vidix_fourcc->depth & VID_DEPTH_1BPP) != VID_DEPTH_1BPP); break;
	  case 2: err = ((vidix_fourcc->depth & VID_DEPTH_2BPP) != VID_DEPTH_2BPP); break;
	  case 4: err = ((vidix_fourcc->depth & VID_DEPTH_4BPP) != VID_DEPTH_4BPP); break;
	  case 8: err = ((vidix_fourcc->depth & VID_DEPTH_8BPP) != VID_DEPTH_8BPP); break;
	  case 12:err = ((vidix_fourcc->depth & VID_DEPTH_12BPP) != VID_DEPTH_12BPP); break;
	  case 15:err = ((vidix_fourcc->depth & VID_DEPTH_15BPP) != VID_DEPTH_15BPP); break;
	  case 16:err = ((vidix_fourcc->depth & VID_DEPTH_16BPP) != VID_DEPTH_16BPP); break;
	  case 24:err = ((vidix_fourcc->depth & VID_DEPTH_24BPP) != VID_DEPTH_24BPP); break;
	  case 32:err = ((vidix_fourcc->depth & VID_DEPTH_32BPP) != VID_DEPTH_32BPP); break;
	  default: err=1; break;
	}
	if(err)
	{
	  MSG_FATAL("video server has unsupported color depth by vidix (%d)\n"
	  ,vidix_fourcc->depth);
	  return -1;
	}
	if((dst_width > src_width || dst_height > src_height) && (vidix_cap->flags & FLAG_UPSCALER) != FLAG_UPSCALER)
	{
	  MSG_FATAL("vidix driver can't upscale image (%d%d -> %d%d)\n",
	  src_width, src_height, dst_width, dst_height);
	  return -1;
	}
	if((dst_width > src_width || dst_height > src_height) && (vidix_cap->flags & FLAG_DOWNSCALER) != FLAG_DOWNSCALER)
	{
	    MSG_FATAL("vidix driver can't downscale image (%d%d -> %d%d)\n",
	    src_width, src_height, dst_width, dst_height);
	    return -1;
	}
	image_width = src_width;
	image_height = src_height;
	src_format = format;
	if(forced_fourcc) format = forced_fourcc;
	memset(vidix_play,0,sizeof(vidix_playback_t));
	vidix_play->fourcc = format;
	vidix_play->capability = vidix_cap->flags; /* every ;) */
	vidix_play->blend_factor = 0; /* for now */
	/* display the full picture.
	   Nick: we could implement here zooming to a specified area -- alex */
	vidix_play->src.x = vidix_play->src.y = 0;
	vidix_play->src.w = src_width;
	vidix_play->src.h = src_height;
	vidix_play->dest.x = x_org;
	vidix_play->dest.y = y_org;
	vidix_play->dest.w = dst_width;
	vidix_play->dest.h = dst_height;
	vidix_play->num_frames=(vo_doublebuffering && vo_use_bm != 1)?NUM_FRAMES-1:1;
	if(vidix_play->num_frames > vo_da_buffs) vidix_play->num_frames = vo_da_buffs;
	vidix_play->src.pitch.y = vidix_play->src.pitch.u = vidix_play->src.pitch.v = 0;
	if(info)
	{
	switch(((const vo_tune_info_t *)info)->pitch[0])
	{
	    case 2:
	    case 4:
	    case 8:
	    case 16:
	    case 32:
	    case 64:
	    case 128:
	    case 256: vidix_play->src.pitch.y = ((const vo_tune_info_t *)info)->pitch[0];
		      break;
	    default: break;
	}
	switch(((const vo_tune_info_t *)info)->pitch[1])
	{
	    case 2:
	    case 4:
	    case 8:
	    case 16:
	    case 32:
	    case 64:
	    case 128:
	    case 256: vidix_play->src.pitch.u = ((const vo_tune_info_t *)info)->pitch[1];
		      break;
	    default: break;
	}
	switch(((const vo_tune_info_t *)info)->pitch[2])
	{
	    case 2:
	    case 4:
	    case 8:
	    case 16:
	    case 32:
	    case 64:
	    case 128:
	    case 256: vidix_play->src.pitch.v = ((const vo_tune_info_t *)info)->pitch[2];
		      break;
	    default: break;
	}
	}
	if((err=vdlConfigPlayback(vidix_handler,vidix_play))!=0)
	{
		MSG_FATAL("Can't configure playback: %s\n",strerror(err));
		return -1;
	}
	MSG_V("using %d buffers\n", vidix_play->num_frames);
	/* configure busmastering */
	if(vo_use_bm)
	{
#ifdef HAVE_MEMALIGN
	    if(vidix_cap->flags & FLAG_DMA)
	    {
		int psize = getpagesize();
		dma_mem_locked=1;
		for(i=0;i<vo_da_buffs;i++)
		{
		    if(!bm_buffs[i]) bm_buffs[i] = memalign(psize, vidix_play->frame_size);
		    if(!(bm_buffs[i]))
		    {
			MSG_ERR("Can't allocate memory for busmastering\n");
			return -1;
		    }
		    if(mlock(bm_buffs[i],vidix_play->frame_size) != 0)
		    {
			unsigned j;
			MSG_WARN("Can't lock memory for busmastering\n");
			for(j=0;j<i;j++) munlock(bm_buffs[i],vidix_play->frame_size);
			dma_mem_locked=0;
		    }
		}
		memset(&vdma,0,sizeof(vidix_dma_t));
		bm_total_frames=bm_slow_frames=0;
	    }
	    else
#else
		MSG_ERR("Won't configure bus mastering: your system doesn't support memalign()\n");
#endif
	    {
		MSG_ERR("Can not configure bus mastering: your driver is not DMA capable\n");
		vo_use_bm = 0;
	    }
	}
	if(vo_use_bm) MSG_OK("using BUSMASTERING\n");
	vidix_mem = vidix_play->dga_addr;

	if(!video_clean)
	{
	/*  clear every frame with correct address and frame_size
	    only once per session */
	    for (i = 0; i < vidix_play->num_frames; i++)
		memset(vidix_mem + vidix_play->offsets[i], 0x80,
		    vidix_play->frame_size);
	    video_clean=1;
	}
	MSG_DBG2("vidix returns pitches %u %u %u\n",vidix_play->dest.pitch.y,vidix_play->dest.pitch.u,vidix_play->dest.pitch.v);
	switch(format)
	{
	    case IMGFMT_Y800:
	    case IMGFMT_YVU9:
	    case IMGFMT_IF09:
	    case IMGFMT_I420:
	    case IMGFMT_IYUV:
	    case IMGFMT_YV12:
		apitch = vidix_play->dest.pitch.y-1;
		dstrides->y = (image_width + apitch) & ~apitch;
		apitch = vidix_play->dest.pitch.v-1;
		dstrides->v = (image_width + apitch) & ~apitch;
		apitch = vidix_play->dest.pitch.u-1;
		dstrides->u = (image_width + apitch) & ~apitch;
		image_Bpp=1;
		break;
	    case IMGFMT_RGB32:
	    case IMGFMT_BGR32:
		apitch = vidix_play->dest.pitch.y-1;
		dstrides->y = (image_width*4 + apitch) & ~apitch;
		dstrides->u = dstrides->v = 0;
		image_Bpp=4;
		break;
	    case IMGFMT_RGB24:
	    case IMGFMT_BGR24:
		apitch = vidix_play->dest.pitch.y-1;
		dstrides->y = (image_width*3 + apitch) & ~apitch;
		dstrides->u = dstrides->v = 0;
		image_Bpp=3;
		break;
	    default:
		apitch = vidix_play->dest.pitch.y-1;
		dstrides->y = (image_width*2 + apitch) & ~apitch;
		dstrides->u = dstrides->v = 0;
		image_Bpp=2;
		break;
	}
	switch(format)
	{
	    case IMGFMT_YVU9:
	    case IMGFMT_IF09:
		dstrides->u /= 4;
		dstrides->v /= 4;
		break;
	    case IMGFMT_I420:
	    case IMGFMT_IYUV:
	    case IMGFMT_YV12:
		dstrides->u /= 2;
		dstrides->v /= 2;
		break;
	}
	return 0;
}

static void __FASTCALL__ vidix_dri_get_surface_caps(dri_surface_cap_t *caps)
{
    caps->caps = vo_use_bm ? DRI_CAP_TEMP_VIDEO : DRI_CAP_VIDEO_MMAPED;
    caps->caps |= DRI_CAP_HORZSCALER | DRI_CAP_VERTSCALER;
    if((vidix_cap->flags & FLAG_DOWNSCALER) == FLAG_DOWNSCALER)
	    caps->caps |= DRI_CAP_DOWNSCALER;
    if((vidix_cap->flags & FLAG_UPSCALER) == FLAG_UPSCALER)
	    caps->caps |= DRI_CAP_UPSCALER;
    caps->fourcc = vidix_play->fourcc;
    caps->width=vidix_play->src.w;
    caps->height=vidix_play->src.h;
    /* in case of vidix movie fit surface */
    caps->x = caps->y=0;
    caps->w=caps->width;
    caps->h=caps->height;
    if(dstrides)
    {
	caps->strides[0] = dstrides->y;
	caps->strides[1] = dstrides->v;
	caps->strides[2] = dstrides->u;
	caps->strides[3] = 0;
    }
}

static void __FASTCALL__ vidix_dri_get_surface(dri_surface_t *surf)
{
    if(vo_use_bm)
    {
	surf->planes[0] = bm_buffs[surf->idx] + vidix_play->offset.y;
	surf->planes[1] = bm_buffs[surf->idx] + vidix_play->offset.v;
	surf->planes[2] = bm_buffs[surf->idx] + vidix_play->offset.u;
    }
    else
    {
	surf->planes[0] = vidix_mem + vidix_play->offsets[surf->idx] + vidix_play->offset.y;
	surf->planes[1] = vidix_mem + vidix_play->offsets[surf->idx] + vidix_play->offset.v;
	surf->planes[2] = vidix_mem + vidix_play->offsets[surf->idx] + vidix_play->offset.u;
    }
    surf->planes[3] = 0;
}

uint32_t __FASTCALL__ vidix_control(uint32_t request, void *data)
{
  switch (request) {
  case VOCTRL_QUERY_FORMAT:
    return vidix_query_fourcc((vo_query_fourcc_t*)data);
  case VOCTRL_FULLSCREEN:
  case VOCTRL_CHECK_EVENTS:
    if(plugin_inited) return (*server_control)(request,data);
    break;
  case VOCTRL_GET_NUM_FRAMES:
	*(uint32_t *)data = (vo_use_bm == 1) ? vo_da_buffs : vidix_play->num_frames;
	return VO_TRUE;
  case DRI_GET_SURFACE_CAPS:
	vidix_dri_get_surface_caps(data);
	return VO_TRUE;
  case DRI_GET_SURFACE: 
	vidix_dri_get_surface(data);
	return VO_TRUE;
  case VOCTRL_FLUSH_FRAME:
	if(vo_use_bm > 1) vidix_copy_dma(*(uint32_t *)data,1);
	return VO_TRUE;
  case VOCTRL_GET_EQUALIZER:
	if(!vidix_get_video_eq(data)) return VO_TRUE;
	else return VO_FALSE;
  case VOCTRL_SET_EQUALIZER:
	if(!vidix_set_video_eq(data)) return VO_TRUE;
	else return VO_FALSE;
  }
  return VO_NOTIMPL;
}

int __FASTCALL__ vidix_preinit(const char *drvname,const void *server)
{
  int err;
  static int reent=0;
  MSG_DBG2("vidix_preinit(%s) was called\n",drvname);
	memset(bm_buffs,0,sizeof(bm_buffs));
  ALLOC_VIDIX_STRUCTS()
	if(vdlGetVersion() != VIDIX_VERSION)
	{
	  MSG_FATAL("You have wrong version of VIDIX library\n");
	  return -1;
	}
	vidix_handler = vdlOpen(VIDIX_PATH,
				drvname ? drvname[0] == ':' ? &drvname[1] : drvname[0] ? drvname : NULL : NULL,
				TYPE_OUTPUT,
				verbose);
	if(vidix_handler == NULL)
	{
		MSG_FATAL("Couldn't find working VIDIX driver\n");
		return -1;
	}
	if((err=vdlGetCapability(vidix_handler,vidix_cap)) != 0)
	{
		MSG_FATAL("Couldn't get capability: %s\n",strerror(err));
		return -1;
	}
	else MSG_V("Driver capability: %X\n",vidix_cap->flags);
	MSG_V("Using: %s by %s\n",vidix_cap->name,vidix_cap->author);
	/* we are able to tune up this stuff depend on fourcc format */
	((vo_functions_t *)server)->flip_page=vidix_flip_page;
	if(!reent) 
	{
	  server_control = ((vo_functions_t *)server)->control;
	  ((vo_functions_t *)server)->control=vidix_control;
	  reent=1;
	}
	vo_server = server;
	plugin_inited=1;
	return 0;
}
