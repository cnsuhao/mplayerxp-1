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

#include <vidix/vidix.h>
#include <vidix/vidixlibxx.h>

#include "video_out.h"
#include "vosub_vidix.h"
#include "osdep/fastmemcpy.h"
#include "osd.h"
#include "sub.h"
#include "video_out.h"
#include "dri_vo.h"
#include "xmpcore/mp_image.h"
#include "vo_msg.h"

using namespace vidix;

#define NUM_FRAMES MAX_DRI_BUFFERS /* Temporary: driver will overwrite it */

typedef struct priv_s {
    priv_s(Vidix& it) : vidix(it) {}
    virtual ~priv_s() {}

    unsigned		image_Bpp,image_height,image_width,src_format,forced_fourcc;

    Vidix&		vidix;
    uint8_t *		mem;
    int			video_on;

    const vo_functions_t*vo_server;

    int			inited;

/* bus mastering */
    int			bm_locked; /* requires root privelegies */
    uint8_t *		bm_buffs[NUM_FRAMES];
    unsigned		bm_total_frames,bm_slow_frames;
}priv_t;

static int __FASTCALL__ vidix_get_video_eq(const vo_data_t* vo,vo_videq_t *info);
static int __FASTCALL__ vidix_set_video_eq(const vo_data_t* vo,const vo_videq_t *info);
static int __FASTCALL__ vidix_get_num_fx(const vo_data_t* vo,unsigned *info);
static int __FASTCALL__ vidix_get_oem_fx(const vo_data_t* vo,vidix_oem_fx_t *info);
static int __FASTCALL__ vidix_set_oem_fx(const vo_data_t* vo,const vidix_oem_fx_t *info);
static int __FASTCALL__ vidix_set_deint(const vo_data_t* vo,const vidix_deinterlace_t *info);

int vidix_start(vo_data_t* vo)
{
    priv_t& priv=*reinterpret_cast<priv_t*>(vo->priv3);
    int err;
    if((err=priv.vidix.playback_on())!=0) {
	MSG_FATAL("Can't start playback: %s\n",strerror(err));
	return -1;
    }
    priv.video_on=1;
    if (priv.vidix.cap.flags & FLAG_EQUALIZER) {
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
	/* To use full set of priv.video_eq.cap */
	if(priv.vidix.get_eq() == 0) {
	    priv.vidix.video_eq.brightness = vo_conf.gamma.brightness;
	    priv.vidix.video_eq.saturation = vo_conf.gamma.saturation;
	    priv.vidix.video_eq.contrast = vo_conf.gamma.contrast;
	    priv.vidix.video_eq.hue = vo_conf.gamma.hue;
	    priv.vidix.video_eq.red_intensity = vo_conf.gamma.red_intensity;
	    priv.vidix.video_eq.green_intensity = vo_conf.gamma.green_intensity;
	    priv.vidix.video_eq.blue_intensity = vo_conf.gamma.blue_intensity;
	    priv.vidix.video_eq.flags = VEQ_FLG_ITU_R_BT_601;
	    priv.vidix.set_eq();
	}
    }
    return 0;
}

int vidix_stop(vo_data_t* vo)
{
    priv_t& priv=*reinterpret_cast<priv_t*>(vo->priv3);
    int err;
    if((err=priv.vidix.playback_off())!=0) {
	MSG_ERR("Can't stop playback: %s\n",strerror(err));
	return -1;
    }
    priv.video_on=0;
    return 0;
}

void vidix_term(vo_data_t* vo)
{
    priv_t& priv=*reinterpret_cast<priv_t*>(vo->priv3);
    size_t i;
    priv.inited=0;
    MSG_DBG2("vidix_term() was called\n");
    vidix_stop(vo);
    if(vo_conf.use_bm) {
	for(i=0;i<vo_conf.xp_buffs;i++) {
	    if(priv.bm_locked) munlock(priv.bm_buffs[i],priv.vidix.playback.frame_size);
	    delete priv.bm_buffs[i];
	    priv.bm_buffs[i]=NULL;
	}
	if(priv.bm_slow_frames)
	    MSG_WARN("from %u frames %u were copied through memcpy()\n"
			,priv.bm_total_frames,priv.bm_slow_frames);
    }
}

static void __FASTCALL__ vidix_copy_dma(const vo_data_t* vo,unsigned idx,int sync_mode)
{
    priv_t& priv=*reinterpret_cast<priv_t*>(vo->priv3);
    int err,i;
    int dma_busy;
    MSG_DBG2("vidix_copy_dma(%u,%i) was called\n",idx,sync_mode);
    priv.bm_total_frames++;
    if(idx > priv.vidix.playback.num_frames-1 && priv.vidix.playback.num_frames>1) {
	MSG_FATAL("\nDetected internal error!\n"
		"Request to copy %u frame into %u array\n",idx,priv.vidix.playback.num_frames);
	return;
    }
    dma_busy = priv.vidix.dma_status();
    i=5;
    if(!sync_mode)
	while(dma_busy && i) {
	    usleep(0);
	    dma_busy = priv.vidix.dma_status();
	    i--;
	}
    if(!dma_busy || sync_mode) {
	priv.vidix.dma.src = priv.bm_buffs[idx];
	priv.vidix.dma.dest_offset = priv.vidix.playback.offsets[priv.vidix.playback.num_frames>1?idx:0];
	priv.vidix.dma.size = priv.vidix.playback.frame_size;
	priv.vidix.dma.flags = sync_mode?BM_DMA_SYNC:BM_DMA_ASYNC;
	if(priv.bm_locked) priv.vidix.dma.flags |= BM_DMA_FIXED_BUFFS;
	priv.vidix.dma.idx = idx;
	err=priv.vidix.dma_copy_frame();
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
	memcpy(reinterpret_cast<any_t*>(reinterpret_cast<long>(priv.vidix.playback.dga_addr)+priv.vidix.playback.offsets[0]),priv.bm_buffs[idx],priv.vidix.playback.frame_size);
	MSG_WARN("DMA frame is memcpy() copied\n");
	priv.bm_slow_frames++;
    }
}

static void __FASTCALL__ vidix_select_frame(vo_data_t* vo,unsigned idx)
{
    priv_t& priv=*reinterpret_cast<priv_t*>(vo->priv3);
    MSG_DBG2("vidix_select_frame() was called\n");
    if(vo_conf.use_bm == 1) vidix_copy_dma(vo,idx,0);
    else priv.vidix.frame_select(idx);
}

static MPXP_Rc __FASTCALL__ vidix_query_fourcc(const vo_data_t* vo,vo_query_fourcc_t* format)
{
    priv_t& priv=*reinterpret_cast<priv_t*>(vo->priv3);
    MSG_DBG2("query_format was called: %x (%s)\n",format->fourcc,vo_format_name(format->fourcc));
    priv.vidix.fourcc.fourcc = format->fourcc;
    priv.vidix.fourcc.srcw = format->w;
    priv.vidix.fourcc.srch = format->h;
    priv.vidix.query_fourcc();
    format->flags = (priv.vidix.fourcc.depth==VID_DEPTH_NONE)? VOCAP_NA : VOCAP_SUPPORTED|VOCAP_HWSCALER;
    return MPXP_Ok;
}

int __FASTCALL__ vidix_grkey_support(const vo_data_t* vo)
{
    priv_t& priv=*reinterpret_cast<priv_t*>(vo->priv3);
    int retval = priv.vidix.fourcc.flags & VID_CAP_COLORKEY;
    MSG_DBG2("query_grkey_support: %i\n",retval);
    return retval;
}

int __FASTCALL__ vidix_grkey_get(const vo_data_t* vo,vidix_grkey_t *gr_key)
{
    int rc;
    priv_t& priv=*reinterpret_cast<priv_t*>(vo->priv3);
    rc=priv.vidix.get_gr_keys();
    *gr_key=priv.vidix.grkey;
    return rc;
}

int __FASTCALL__ vidix_grkey_set(const vo_data_t* vo,const vidix_grkey_t *gr_key)
{
    priv_t& priv=*reinterpret_cast<priv_t*>(vo->priv3);
    priv.vidix.grkey=*gr_key;
    return priv.vidix.set_gr_keys();
}

static int __FASTCALL__ vidix_get_video_eq(const vo_data_t* vo,vo_videq_t *info)
{
    priv_t& priv=*reinterpret_cast<priv_t*>(vo->priv3);
    int rval;
    if(!priv.video_on) return EPERM;
    rval = priv.vidix.get_eq();
    if(!rval) {
	if(!strcmp(info->name,VO_EC_BRIGHTNESS) && priv.vidix.video_eq.cap&VEQ_CAP_BRIGHTNESS)
	    info->value=priv.vidix.video_eq.brightness;
	else if(!strcmp(info->name,VO_EC_SATURATION) && priv.vidix.video_eq.cap&VEQ_CAP_SATURATION)
	    info->value=priv.vidix.video_eq.saturation;
	else if(!strcmp(info->name,VO_EC_CONTRAST) && priv.vidix.video_eq.cap&VEQ_CAP_CONTRAST)
	    info->value=priv.vidix.video_eq.contrast;
	else if(!strcmp(info->name,VO_EC_HUE) && priv.vidix.video_eq.cap&VEQ_CAP_HUE)
	    info->value=priv.vidix.video_eq.hue;
	else if(!strcmp(info->name,VO_EC_RED_INTENSITY) && priv.vidix.video_eq.cap&VEQ_CAP_RGB_INTENSITY)
	    info->value=priv.vidix.video_eq.red_intensity;
	else if(!strcmp(info->name,VO_EC_GREEN_INTENSITY) && priv.vidix.video_eq.cap&VEQ_CAP_RGB_INTENSITY)
	    info->value=priv.vidix.video_eq.green_intensity;
	else if(!strcmp(info->name,VO_EC_BLUE_INTENSITY) && priv.vidix.video_eq.cap&VEQ_CAP_RGB_INTENSITY)
	    info->value=priv.vidix.video_eq.blue_intensity;
    }
    return rval;
}

static int __FASTCALL__ vidix_set_video_eq(const vo_data_t* vo,const vo_videq_t *info)
{
    priv_t& priv=*reinterpret_cast<priv_t*>(vo->priv3);
    int rval;
    if(!priv.video_on) return EPERM;
    rval = priv.vidix.get_eq();
    if(!rval) {
	if(!strcmp(info->name,VO_EC_BRIGHTNESS) && priv.vidix.video_eq.cap&VEQ_CAP_BRIGHTNESS)
	    priv.vidix.video_eq.brightness=info->value;
	else if(!strcmp(info->name,VO_EC_SATURATION) && priv.vidix.video_eq.cap&VEQ_CAP_SATURATION)
	    priv.vidix.video_eq.saturation=info->value;
	else if(!strcmp(info->name,VO_EC_CONTRAST) && priv.vidix.video_eq.cap&VEQ_CAP_CONTRAST)
	    priv.vidix.video_eq.contrast=info->value;
	else if(!strcmp(info->name,VO_EC_HUE) && priv.vidix.video_eq.cap&VEQ_CAP_HUE)
	    priv.vidix.video_eq.hue=info->value;
	else if(!strcmp(info->name,VO_EC_RED_INTENSITY) && priv.vidix.video_eq.cap&VEQ_CAP_RGB_INTENSITY)
	    priv.vidix.video_eq.red_intensity=info->value;
	else if(!strcmp(info->name,VO_EC_GREEN_INTENSITY) && priv.vidix.video_eq.cap&VEQ_CAP_RGB_INTENSITY)
	    priv.vidix.video_eq.green_intensity=info->value;
	else if(!strcmp(info->name,VO_EC_BLUE_INTENSITY) && priv.vidix.video_eq.cap&VEQ_CAP_RGB_INTENSITY)
	    priv.vidix.video_eq.blue_intensity=info->value;
	rval= priv.vidix.set_eq();
    }
    return rval;
}

static int __FASTCALL__ vidix_get_num_fx(const vo_data_t* vo,unsigned *info)
{
    priv_t& priv=*reinterpret_cast<priv_t*>(vo->priv3);
    if(!priv.video_on) return EPERM;
    return priv.vidix.num_oemfx(*info);
}

static int __FASTCALL__ vidix_get_oem_fx(const vo_data_t* vo,vidix_oem_fx_t *info)
{
    int rc;
    priv_t& priv=*reinterpret_cast<priv_t*>(vo->priv3);
    if(!priv.video_on) return EPERM;
    rc=priv.vidix.get_oemfx();
    *info = priv.vidix.oemfx;
    return rc;
}

static int __FASTCALL__ vidix_set_oem_fx(const vo_data_t* vo,const vidix_oem_fx_t *info)
{
    priv_t& priv=*reinterpret_cast<priv_t*>(vo->priv3);
    if(!priv.video_on) return EPERM;
    priv.vidix.oemfx=*info;
    return priv.vidix.set_oemfx();
}

static int __FASTCALL__ vidix_set_deint(const vo_data_t* vo,const vidix_deinterlace_t *info)
{
    priv_t& priv=*reinterpret_cast<priv_t*>(vo->priv3);
    if(!priv.video_on) return EPERM;
    priv.vidix.deint=*info;
    return priv.vidix.set_deint();
}

#ifndef HAVE_MLOCK
/* stubs */
int mlock(const any_t*addr,size_t len) { return ENOSYS; }
int munlock(const any_t*addr,size_t len) { return ENOSYS; }
#endif

MPXP_Rc  __FASTCALL__ vidix_init(vo_data_t* vo,unsigned src_width,unsigned src_height,
		   unsigned x_org,unsigned y_org,unsigned dst_width,
		   unsigned dst_height,unsigned format,unsigned dest_bpp,
		   unsigned vid_w,unsigned vid_h)
{
    priv_t& priv=*reinterpret_cast<priv_t*>(vo->priv3);
    size_t i;
    int err;
    static int video_clean=0;
    uint32_t apitch;
    MSG_DBG2("vidix_init() was called\n"
	    "src_w=%u src_h=%u dest_x_y_w_h = %u %u %u %u\n"
	    "format=%s dest_bpp=%u vid_w=%u vid_h=%u\n"
	    ,src_width,src_height,x_org,y_org,dst_width,dst_height
	    ,vo_format_name(format),dest_bpp,vid_w,vid_h);
    if(((priv.vidix.cap.maxwidth != -1) && (vid_w > (unsigned)priv.vidix.cap.maxwidth)) ||
	    ((priv.vidix.cap.minwidth != -1) && (vid_w < (unsigned)priv.vidix.cap.minwidth)) ||
	    ((priv.vidix.cap.maxheight != -1) && (vid_h > (unsigned)priv.vidix.cap.maxheight)) ||
	    ((priv.vidix.cap.minwidth != -1 ) && (vid_h < (unsigned)priv.vidix.cap.minheight))) {
	MSG_FATAL("video server has unsupported resolution (%dx%d), supported: %dx%d-%dx%d\n",
		vid_w, vid_h, priv.vidix.cap.minwidth, priv.vidix.cap.minheight,
		priv.vidix.cap.maxwidth, priv.vidix.cap.maxheight);
	return MPXP_False;
    }
    priv.vidix.fourcc.fourcc = format;
    priv.vidix.query_fourcc();
    err = 0;
    switch(dest_bpp) {
	case 1: err = ((priv.vidix.fourcc.depth & VID_DEPTH_1BPP) != VID_DEPTH_1BPP); break;
	case 2: err = ((priv.vidix.fourcc.depth & VID_DEPTH_2BPP) != VID_DEPTH_2BPP); break;
	case 4: err = ((priv.vidix.fourcc.depth & VID_DEPTH_4BPP) != VID_DEPTH_4BPP); break;
	case 8: err = ((priv.vidix.fourcc.depth & VID_DEPTH_8BPP) != VID_DEPTH_8BPP); break;
	case 12:err = ((priv.vidix.fourcc.depth & VID_DEPTH_12BPP) != VID_DEPTH_12BPP); break;
	case 15:err = ((priv.vidix.fourcc.depth & VID_DEPTH_15BPP) != VID_DEPTH_15BPP); break;
	case 16:err = ((priv.vidix.fourcc.depth & VID_DEPTH_16BPP) != VID_DEPTH_16BPP); break;
	case 24:err = ((priv.vidix.fourcc.depth & VID_DEPTH_24BPP) != VID_DEPTH_24BPP); break;
	case 32:err = ((priv.vidix.fourcc.depth & VID_DEPTH_32BPP) != VID_DEPTH_32BPP); break;
	default: err=1; break;
    }
    if(err) {
	MSG_FATAL("video server has unsupported color depth by vidix (%d)\n"
		,priv.vidix.fourcc.depth);
		return MPXP_False;
    }
    if((dst_width > src_width || dst_height > src_height) && (priv.vidix.cap.flags & FLAG_UPSCALER) != FLAG_UPSCALER) {
	MSG_FATAL("vidix driver can't upscale image (%d%d -> %d%d)\n",
		src_width, src_height, dst_width, dst_height);
	return MPXP_False;
    }
    if((dst_width > src_width || dst_height > src_height) && (priv.vidix.cap.flags & FLAG_DOWNSCALER) != FLAG_DOWNSCALER) {
	MSG_FATAL("vidix driver can't downscale image (%d%d -> %d%d)\n",
		src_width, src_height, dst_width, dst_height);
	return MPXP_False;
    }
    priv.image_width = src_width;
    priv.image_height = src_height;
    priv.src_format = format;
    if(priv.forced_fourcc) format = priv.forced_fourcc;
    priv.vidix.playback.fourcc = format;
    priv.vidix.playback.capability = priv.vidix.cap.flags; /* every ;) */
    priv.vidix.playback.blend_factor = 0; /* for now */
    /* display the full picture.
	Nick: we could implement here zooming to a specified area -- alex */
    priv.vidix.playback.src.x = priv.vidix.playback.src.y = 0;
    priv.vidix.playback.src.w = src_width;
    priv.vidix.playback.src.h = src_height;
    priv.vidix.playback.dest.x = x_org;
    priv.vidix.playback.dest.y = y_org;
    priv.vidix.playback.dest.w = dst_width;
    priv.vidix.playback.dest.h = dst_height;
    priv.vidix.playback.num_frames=(vo_conf.use_bm!=1)?NUM_FRAMES-1:1;
    if(priv.vidix.playback.num_frames > vo_conf.xp_buffs) priv.vidix.playback.num_frames = vo_conf.xp_buffs;
    priv.vidix.playback.src.pitch.y = priv.vidix.playback.src.pitch.u = priv.vidix.playback.src.pitch.v = 0;
    if((err=priv.vidix.config_playback())!=0) {
	MSG_FATAL("Can't configure playback: %s\n",strerror(err));
	return MPXP_False;
    }
    MSG_V("using %d buffers\n", priv.vidix.playback.num_frames);
    /* configure busmastering */
    if(vo_conf.use_bm) {
#ifdef HAVE_MEMALIGN
	if(priv.vidix.cap.flags & FLAG_DMA) {
	    int psize = getpagesize();
	    priv.bm_locked=1;
	    for(i=0;i<vo_conf.xp_buffs;i++) {
		if(!priv.bm_buffs[i]) priv.bm_buffs[i] = new(alignmem,psize) uint8_t[priv.vidix.playback.frame_size];
		if(!(priv.bm_buffs[i])) {
		    MSG_ERR("Can't allocate memory for busmastering\n");
		    return MPXP_False;
		}
		if(mlock(priv.bm_buffs[i],priv.vidix.playback.frame_size) != 0) {
		    unsigned j;
		    MSG_WARN("Can't lock memory for busmastering\n");
		    for(j=0;j<i;j++) munlock(priv.bm_buffs[i],priv.vidix.playback.frame_size);
		    priv.bm_locked=0;
		}
	    }
	    memset(&priv.vidix.dma,0,sizeof(vidix_dma_t));
	    priv.bm_total_frames=priv.bm_slow_frames=0;
	}
	else
#else
	    MSG_ERR("Won't configure bus mastering: your system doesn't support mp_memalign()\n");
#endif
	MSG_ERR("Can not configure bus mastering: your driver is not DMA capable\n");
	vo_conf.use_bm = 0;
    }
    if(vo_conf.use_bm) MSG_OK("using BUSMASTERING\n");
    priv.mem = reinterpret_cast<uint8_t*>(priv.vidix.playback.dga_addr);

    if(!video_clean) {
	/*  clear every frame with correct address and frame_size
	    only once per session */
	for (i = 0; i < priv.vidix.playback.num_frames; i++)
	    memset(priv.mem + priv.vidix.playback.offsets[i], 0x80, priv.vidix.playback.frame_size);
	video_clean=1;
    }
    MSG_DBG2("vidix returns pitches %u %u %u\n",priv.vidix.playback.dest.pitch.y,priv.vidix.playback.dest.pitch.u,priv.vidix.playback.dest.pitch.v);
    switch(format) {
	case IMGFMT_Y800:
	case IMGFMT_YVU9:
	case IMGFMT_IF09:
	case IMGFMT_I420:
	case IMGFMT_IYUV:
	case IMGFMT_YV12:
	    apitch = priv.vidix.playback.dest.pitch.y-1;
	    priv.vidix.playback.offset.y = (priv.image_width + apitch) & ~apitch;
	    apitch = priv.vidix.playback.dest.pitch.v-1;
	    priv.vidix.playback.offset.v = (priv.image_width + apitch) & ~apitch;
	    apitch = priv.vidix.playback.dest.pitch.u-1;
	    priv.vidix.playback.offset.u = (priv.image_width + apitch) & ~apitch;
	    priv.image_Bpp=1;
	    break;
	case IMGFMT_RGB32:
	case IMGFMT_BGR32:
	    apitch = priv.vidix.playback.dest.pitch.y-1;
	    priv.vidix.playback.offset.y = (priv.image_width*4 + apitch) & ~apitch;
	    priv.vidix.playback.offset.u = priv.vidix.playback.offset.v = 0;
	    priv.image_Bpp=4;
	    break;
	case IMGFMT_RGB24:
	case IMGFMT_BGR24:
	    apitch = priv.vidix.playback.dest.pitch.y-1;
	    priv.vidix.playback.offset.y = (priv.image_width*3 + apitch) & ~apitch;
	    priv.vidix.playback.offset.u = priv.vidix.playback.offset.v = 0;
	    priv.image_Bpp=3;
	    break;
	default:
	    apitch = priv.vidix.playback.dest.pitch.y-1;
	    priv.vidix.playback.offset.y = (priv.image_width*2 + apitch) & ~apitch;
	    priv.vidix.playback.offset.u = priv.vidix.playback.offset.v = 0;
	    priv.image_Bpp=2;
	    break;
    }
    switch(format) {
	case IMGFMT_YVU9:
	case IMGFMT_IF09:
	    priv.vidix.playback.offset.u /= 4;
	    priv.vidix.playback.offset.v /= 4;
	    break;
	case IMGFMT_I420:
	case IMGFMT_IYUV:
	case IMGFMT_YV12:
	    priv.vidix.playback.offset.u /= 2;
	    priv.vidix.playback.offset.v /= 2;
	    break;
    }
    return MPXP_Ok;
}

static void __FASTCALL__ vidix_dri_get_surface_caps(const vo_data_t* vo,dri_surface_cap_t *caps)
{
    priv_t& priv=*reinterpret_cast<priv_t*>(vo->priv3);
    caps->caps = vo_conf.use_bm ? DRI_CAP_TEMP_VIDEO|DRI_CAP_BUSMASTERING : DRI_CAP_VIDEO_MMAPED;
    caps->caps |= DRI_CAP_HORZSCALER | DRI_CAP_VERTSCALER;
    if((priv.vidix.cap.flags & FLAG_DOWNSCALER) == FLAG_DOWNSCALER)
	    caps->caps |= DRI_CAP_DOWNSCALER;
    if((priv.vidix.cap.flags & FLAG_UPSCALER) == FLAG_UPSCALER)
	    caps->caps |= DRI_CAP_UPSCALER;
    caps->fourcc = priv.vidix.playback.fourcc;
    caps->width=priv.vidix.playback.src.w;
    caps->height=priv.vidix.playback.src.h;
    /* in case of vidix movie fit surface */
    caps->x = caps->y=0;
    caps->w=caps->width;
    caps->h=caps->height;
    caps->strides[0] = priv.vidix.playback.offset.y;
    caps->strides[1] = priv.vidix.playback.offset.v;
    caps->strides[2] = priv.vidix.playback.offset.u;
    caps->strides[3] = 0;
}

static void __FASTCALL__ vidix_dri_get_surface(const vo_data_t* vo,dri_surface_t *surf)
{
    priv_t& priv=*reinterpret_cast<priv_t*>(vo->priv3);
    if(vo_conf.use_bm) {
	surf->planes[0] = priv.bm_buffs[surf->idx] + priv.vidix.playback.offset.y;
	surf->planes[1] = priv.bm_buffs[surf->idx] + priv.vidix.playback.offset.v;
	surf->planes[2] = priv.bm_buffs[surf->idx] + priv.vidix.playback.offset.u;
    } else {
	surf->planes[0] = priv.mem + priv.vidix.playback.offsets[surf->idx] + priv.vidix.playback.offset.y;
	surf->planes[1] = priv.mem + priv.vidix.playback.offsets[surf->idx] + priv.vidix.playback.offset.v;
	surf->planes[2] = priv.mem + priv.vidix.playback.offsets[surf->idx] + priv.vidix.playback.offset.u;
    }
    surf->planes[3] = 0;
}

MPXP_Rc __FASTCALL__ vidix_control(vo_data_t* vo,uint32_t request, any_t*data)
{
    priv_t& priv=*reinterpret_cast<priv_t*>(vo->priv3);
    switch (request) {
	case VOCTRL_QUERY_FORMAT:
	    return vidix_query_fourcc(vo,reinterpret_cast<vo_query_fourcc_t*>(data));
	case VOCTRL_GET_NUM_FRAMES:
	    *(uint32_t *)data = (vo_conf.use_bm == 1) ? vo_conf.xp_buffs : priv.vidix.playback.num_frames;
	    return MPXP_True;
	case DRI_GET_SURFACE_CAPS:
	    vidix_dri_get_surface_caps(vo,reinterpret_cast<dri_surface_cap_t*>(data));
	    return MPXP_True;
	case DRI_GET_SURFACE:
	    vidix_dri_get_surface(vo,reinterpret_cast<dri_surface_t*>(data));
	    return MPXP_True;
	case VOCTRL_FLUSH_PAGES:
	    if(vo_conf.use_bm > 1) vidix_copy_dma(vo,*(uint32_t *)data,1);
	    return MPXP_True;
	case VOCTRL_GET_EQUALIZER:
	    if(!vidix_get_video_eq(vo,reinterpret_cast<vo_videq_t*>(data))) return MPXP_True;
	    else return MPXP_False;
	case VOCTRL_SET_EQUALIZER:
	    if(!vidix_set_video_eq(vo,reinterpret_cast<vo_videq_t*>(data))) return MPXP_True;
	    else return MPXP_False;
    }
    return MPXP_NA;
}

vidix_server_t* __FASTCALL__ vidix_preinit(vo_data_t*vo,const char *drvname,const any_t*_server)
{
    const vo_functions_t* server=reinterpret_cast<const vo_functions_t*>(_server);
    int err;
    static int reent=0;
    MSG_DBG2("vidix_preinit(%s) was called\n",drvname);
    Vidix& _vidix = *new Vidix(drvname ? drvname[0] == ':' ? &drvname[1] : drvname[0] ? drvname : NULL : NULL,
			TYPE_OUTPUT,
			mp_conf.verbose);
    priv_t& priv=*new(zeromem) priv_t(_vidix);
    vo->priv3=&priv;
    if(priv.vidix.version() != VIDIX_VERSION) {
	MSG_FATAL("You have wrong version of VIDIX library\n");
	delete &priv;
	return NULL;
    }
    if(priv.vidix.is_error()) {
	MSG_FATAL("Couldn't find working VIDIX driver\n");
	delete &priv;
	return NULL;
    }
    if((err=priv.vidix.get_capabilities()) != 0) {
	MSG_FATAL("Couldn't get capability: %s\n",strerror(err));
	delete &priv;
	return NULL;
    }
    else MSG_V("Driver capability: %X\n",priv.vidix.cap.flags);
    MSG_V("Using: %s by %s\n",priv.vidix.cap.name,priv.vidix.cap.author);
    if(!reent) {
	reent=1;
    }
    priv.vo_server = reinterpret_cast<const vo_functions_t*>(server);
    priv.inited=1;
    /* we are able to tune up this stuff depend on fourcc format */
    vidix_server_t* rs = new vidix_server_t;
    rs->control=vidix_control;
    rs->select_frame=vidix_select_frame;
    return rs;
}
