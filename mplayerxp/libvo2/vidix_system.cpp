#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
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
#include <iomanip>
#include <stdexcept>

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

#include "mplayerxp.h"

#include "vidix_system.h"
#include "osdep/fastmemcpy.h"
#include "xmpcore/xmp_image.h"
#include "vo_msg.h"

using namespace vidix;

#ifndef HAVE_MLOCK
/* stubs */
int mlock(const any_t*addr,size_t len) { return ENOSYS; }
int munlock(const any_t*addr,size_t len) { return ENOSYS; }
#endif

Vidix_System::Vidix_System(const std::string& drvname)
	    :vidix(new(zeromem) Vidix(!drvname.empty()?
					drvname[0]==':'?
					    drvname.substr(1):drvname:"",
			TYPE_OUTPUT,
			mp_conf.verbose))
{
    int err;
    mpxp_dbg2<<"vidix_preinit("<<drvname<<") was called"<<std::endl;
    if(vidix->version() != VIDIX_VERSION) {
	mpxp_fatal<<"You have wrong version of VIDIX library"<<std::endl;
	throw std::runtime_error("Vidix");
    }
    if(vidix->is_error()) {
	mpxp_fatal<<"Couldn't find working VIDIX driver"<<std::endl;
	throw std::runtime_error("Vidix");
    }
    if((err=vidix->get_capabilities()) != 0) {
	mpxp_fatal<<"Couldn't get capability: "<<strerror(err)<<std::endl;
	throw std::runtime_error("Vidix");
    }
    else mpxp_v<<"Driver capability: "<<std::hex<<vidix->cap.flags<<std::endl;
    mpxp_v<<"Using: "<<vidix->cap.name<<" by "<<vidix->cap.author<<std::endl;
}

Vidix_System::~Vidix_System() {
    size_t i;
    mpxp_dbg2<<"vidix_term() was called"<<std::endl;
    stop();
    if(vo_conf.use_bm) {
	for(i=0;i<vo_conf.xp_buffs;i++) {
	    if(bm_locked) munlock(bm_buffs[i],vidix->playback.frame_size);
	    delete bm_buffs[i];
	    bm_buffs[i]=NULL;
	}
	if(bm_slow_frames)
	    mpxp_warn<<"from "<<bm_total_frames<<" frames "<<bm_slow_frames<<" were copied through memcpy()"<<std::endl;
    }
}

int Vidix_System::start()
{
    int err;
    if((err=vidix->playback_on())!=0) {
	mpxp_fatal<<"Can't start playback: "<<strerror(err)<<std::endl;
	return -1;
    }
    video_on=1;
    if (vidix->cap.flags & FLAG_EQUALIZER) {
	mpxp_v<<"vo_gamma_brightness="<<vo_conf.gamma.brightness<<std::endl;
	mpxp_v<<"vo_gamma_saturation="<<vo_conf.gamma.saturation<<std::endl;
	mpxp_v<<"vo_gamma_contrast="<<vo_conf.gamma.contrast<<std::endl;
	mpxp_v<<"vo_gamma_hue="<<vo_conf.gamma.hue<<std::endl;
	mpxp_v<<"vo_gamma_red_intensity="<<vo_conf.gamma.red_intensity<<std::endl;
	mpxp_v<<"vo_gamma_green_intensity="<<vo_conf.gamma.green_intensity<<std::endl;
	mpxp_v<<"vo_gamma_blue_intensity="<<vo_conf.gamma.blue_intensity<<std::endl;
	/* To use full set of priv.video_eq.cap */
	if(vidix->get_eq() == 0) {
	    vidix->video_eq.brightness = vo_conf.gamma.brightness;
	    vidix->video_eq.saturation = vo_conf.gamma.saturation;
	    vidix->video_eq.contrast = vo_conf.gamma.contrast;
	    vidix->video_eq.hue = vo_conf.gamma.hue;
	    vidix->video_eq.red_intensity = vo_conf.gamma.red_intensity;
	    vidix->video_eq.green_intensity = vo_conf.gamma.green_intensity;
	    vidix->video_eq.blue_intensity = vo_conf.gamma.blue_intensity;
	    vidix->video_eq.flags = VEQ_FLG_ITU_R_BT_601;
	    vidix->set_eq();
	}
    }
    return 0;
}

int Vidix_System::stop()
{
    int err;
    if((err=vidix->playback_off())!=0) {
	mpxp_err<<"Can't stop playback: "<<strerror(err)<<std::endl;
	return -1;
    }
    video_on=0;
    return 0;
}

void Vidix_System::copy_dma(unsigned idx,int sync_mode)
{
    int err,i;
    int dma_busy;
    mpxp_dbg2<<"vidix_copy_dma("<<idx<<","<<sync_mode<<") was called"<<std::endl;
    bm_total_frames++;
    if(idx > vidix->playback.num_frames-1 && vidix->playback.num_frames>1) {
	mpxp_fatal<<"Detected internal error!"<<std::endl;
	mpxp_fatal<<"Request to copy "<<idx<<" frame into "<<vidix->playback.num_frames<<" array"<<std::endl;
	return;
    }
    dma_busy = vidix->dma_status();
    i=5;
    if(!sync_mode)
	while(dma_busy && i) {
	    yield_timeslice();
	    dma_busy = vidix->dma_status();
	    i--;
	}
    if(!dma_busy || sync_mode) {
	vidix->dma.src = bm_buffs[idx];
	vidix->dma.dest_offset = vidix->playback.offsets[vidix->playback.num_frames>1?idx:0];
	vidix->dma.size = vidix->playback.frame_size;
	vidix->dma.flags = sync_mode?BM_DMA_SYNC:BM_DMA_ASYNC;
	if(bm_locked) vidix->dma.flags |= BM_DMA_FIXED_BUFFS;
	vidix->dma.idx = idx;
	err=vidix->dma_copy_frame();
	if(err) {
	    /* We can switch back to DR here but for now exit */
	    mpxp_fatal<<"Please send BUGREPORT to developers!!!"<<std::endl;
	    throw std::runtime_error(std::string("error '")+strerror(err)+"' occured during DMA transfer");
	}
#if 0
	mpxp_info<<"frame is DMA copied"<<std::endl;
#endif
    } else {
	memcpy(reinterpret_cast<any_t*>(reinterpret_cast<long>(vidix->playback.dga_addr)+vidix->playback.offsets[0]),bm_buffs[idx],vidix->playback.frame_size);
	mpxp_warn<<"DMA frame is memcpy() copied"<<std::endl;
	bm_slow_frames++;
    }
}

MPXP_Rc Vidix_System::select_frame(unsigned idx)
{
    mpxp_dbg2<<"vidix_select_frame() was called"<<std::endl;
    if(vo_conf.use_bm == 1) copy_dma(idx,0);
    else vidix->frame_select(idx);
    return MPXP_Ok;
}

MPXP_Rc Vidix_System::query_fourcc(vo_query_fourcc_t* format)
{
    mpxp_dbg2<<"query_format was called: "<<std::hex<<format->fourcc<<" ("<<vo_format_name(format->fourcc)<<")"<<std::endl;
    vidix->fourcc.fourcc = format->fourcc;
    vidix->fourcc.srcw = format->w;
    vidix->fourcc.srch = format->h;
    vidix->query_fourcc();
    format->flags = (vidix->fourcc.depth==VID_DEPTH_NONE)? VOCAP_NA : VOCAP_SUPPORTED|VOCAP_HWSCALER;
    return MPXP_Ok;
}

MPXP_Rc Vidix_System::grkey_support() const
{
    MPXP_Rc retval = (vidix->fourcc.flags & VID_CAP_COLORKEY)?MPXP_Ok:MPXP_False;
    mpxp_dbg2<<"query_grkey_support: "<<retval<<std::endl;
    return retval;
}

int Vidix_System::grkey_get(vidix_grkey_t *gr_key) const
{
    int rc;
    rc=vidix->get_gr_keys();
    *gr_key=vidix->grkey;
    return rc;
}

int Vidix_System::grkey_set(const vidix_grkey_t *gr_key)
{
    vidix->grkey=*gr_key;
    return vidix->set_gr_keys();
}

int Vidix_System::get_video_eq(vo_videq_t *info) const
{
    int rval;
    if(!video_on) return EPERM;
    rval =vidix->get_eq();
    if(!rval) {
	if(!strcmp(info->name,VO_EC_BRIGHTNESS) && vidix->video_eq.cap&VEQ_CAP_BRIGHTNESS)
	    info->value=vidix->video_eq.brightness;
	else if(!strcmp(info->name,VO_EC_SATURATION) && vidix->video_eq.cap&VEQ_CAP_SATURATION)
	    info->value=vidix->video_eq.saturation;
	else if(!strcmp(info->name,VO_EC_CONTRAST) && vidix->video_eq.cap&VEQ_CAP_CONTRAST)
	    info->value=vidix->video_eq.contrast;
	else if(!strcmp(info->name,VO_EC_HUE) && vidix->video_eq.cap&VEQ_CAP_HUE)
	    info->value=vidix->video_eq.hue;
	else if(!strcmp(info->name,VO_EC_RED_INTENSITY) && vidix->video_eq.cap&VEQ_CAP_RGB_INTENSITY)
	    info->value=vidix->video_eq.red_intensity;
	else if(!strcmp(info->name,VO_EC_GREEN_INTENSITY) && vidix->video_eq.cap&VEQ_CAP_RGB_INTENSITY)
	    info->value=vidix->video_eq.green_intensity;
	else if(!strcmp(info->name,VO_EC_BLUE_INTENSITY) && vidix->video_eq.cap&VEQ_CAP_RGB_INTENSITY)
	    info->value=vidix->video_eq.blue_intensity;
    }
    return rval;
}

int Vidix_System::set_video_eq(const vo_videq_t *info)
{
    int rval;
    if(!video_on) return EPERM;
    rval = vidix->get_eq();
    if(!rval) {
	if(!strcmp(info->name,VO_EC_BRIGHTNESS) && vidix->video_eq.cap&VEQ_CAP_BRIGHTNESS)
	    vidix->video_eq.brightness=info->value;
	else if(!strcmp(info->name,VO_EC_SATURATION) && vidix->video_eq.cap&VEQ_CAP_SATURATION)
	    vidix->video_eq.saturation=info->value;
	else if(!strcmp(info->name,VO_EC_CONTRAST) && vidix->video_eq.cap&VEQ_CAP_CONTRAST)
	    vidix->video_eq.contrast=info->value;
	else if(!strcmp(info->name,VO_EC_HUE) && vidix->video_eq.cap&VEQ_CAP_HUE)
	    vidix->video_eq.hue=info->value;
	else if(!strcmp(info->name,VO_EC_RED_INTENSITY) && vidix->video_eq.cap&VEQ_CAP_RGB_INTENSITY)
	    vidix->video_eq.red_intensity=info->value;
	else if(!strcmp(info->name,VO_EC_GREEN_INTENSITY) && vidix->video_eq.cap&VEQ_CAP_RGB_INTENSITY)
	    vidix->video_eq.green_intensity=info->value;
	else if(!strcmp(info->name,VO_EC_BLUE_INTENSITY) && vidix->video_eq.cap&VEQ_CAP_RGB_INTENSITY)
	    vidix->video_eq.blue_intensity=info->value;
	rval= vidix->set_eq();
    }
    return rval;
}

int Vidix_System::get_num_fx(unsigned *info) const
{
    if(!video_on) return EPERM;
    return vidix->num_oemfx(*info);
}

int Vidix_System::get_oem_fx(vidix_oem_fx_t *info) const
{
    int rc;
    if(!video_on) return EPERM;
    rc=vidix->get_oemfx();
    *info = vidix->oemfx;
    return rc;
}

int Vidix_System::set_oem_fx(const vidix_oem_fx_t *info)
{
    if(!video_on) return EPERM;
    vidix->oemfx=*info;
    return vidix->set_oemfx();
}

int Vidix_System::get_deint(vidix_deinterlace_t *info) const
{
    int rc;
    if(!video_on) return EPERM;
    rc=vidix->get_deint();
    *info = vidix->deint;
    return rc;
}

int Vidix_System::set_deint(const vidix_deinterlace_t *info)
{
    if(!video_on) return EPERM;
    vidix->deint=*info;
    return vidix->set_deint();
}

MPXP_Rc Vidix_System::configure(unsigned src_width,unsigned src_height,
			unsigned x_org,unsigned y_org,unsigned dst_width,
			unsigned dst_height,unsigned format,unsigned dest_bpp,
			unsigned vid_w,unsigned vid_h)
{
    size_t i;
    int err;
    static int video_clean=0;
    uint32_t apitch;
    mpxp_dbg2<<"vidix_init() was called"<<std::endl;
    mpxp_dbg2<<"src_w="<<src_width<<" src_h="<<src_height<<" dest_x_y_w_h = "<<x_org<<" "<<y_org<<" "<<dst_width<<" "<<dst_height<<std::endl;
    mpxp_dbg2<<"format="<<vo_format_name(format)<<" dest_bpp="<<dest_bpp<<" vid_w="<<vid_w<<" vid_h="<<vid_h<<std::endl;
    if(((vidix->cap.maxwidth != -1) && (vid_w > (unsigned)vidix->cap.maxwidth)) ||
	    ((vidix->cap.minwidth != -1) && (vid_w < (unsigned)vidix->cap.minwidth)) ||
	    ((vidix->cap.maxheight != -1) && (vid_h > (unsigned)vidix->cap.maxheight)) ||
	    ((vidix->cap.minwidth != -1 ) && (vid_h < (unsigned)vidix->cap.minheight))) {
	mpxp_fatal<<"video server has unsupported resolution ("<<vid_w<<"x"<<vid_h<<"), supported: "<<vidix->cap.maxwidth<<"x"<<vidix->cap.minwidth<<"-"<<vidix->cap.minheight<<"x"<<vidix->cap.maxheight<<std::endl;
	return MPXP_False;
    }
    vidix->fourcc.fourcc = format;
    vidix->query_fourcc();
    err = 0;
    switch(dest_bpp) {
	case 1: err = ((vidix->fourcc.depth & VID_DEPTH_1BPP) != VID_DEPTH_1BPP); break;
	case 2: err = ((vidix->fourcc.depth & VID_DEPTH_2BPP) != VID_DEPTH_2BPP); break;
	case 4: err = ((vidix->fourcc.depth & VID_DEPTH_4BPP) != VID_DEPTH_4BPP); break;
	case 8: err = ((vidix->fourcc.depth & VID_DEPTH_8BPP) != VID_DEPTH_8BPP); break;
	case 12:err = ((vidix->fourcc.depth & VID_DEPTH_12BPP) != VID_DEPTH_12BPP); break;
	case 15:err = ((vidix->fourcc.depth & VID_DEPTH_15BPP) != VID_DEPTH_15BPP); break;
	case 16:err = ((vidix->fourcc.depth & VID_DEPTH_16BPP) != VID_DEPTH_16BPP); break;
	case 24:err = ((vidix->fourcc.depth & VID_DEPTH_24BPP) != VID_DEPTH_24BPP); break;
	case 32:err = ((vidix->fourcc.depth & VID_DEPTH_32BPP) != VID_DEPTH_32BPP); break;
	default: err=1; break;
    }
    if(err) {
	mpxp_fatal<<"video server has unsupported color depth by vidix ("<<vidix->fourcc.depth<<")"<<std::endl;
	return MPXP_False;
    }
    if((dst_width > src_width || dst_height > src_height) && (vidix->cap.flags & FLAG_UPSCALER) != FLAG_UPSCALER) {
	mpxp_fatal<<"vidix driver can't upscale image ("<<src_width<<"x"<<src_height<<" -> "<<dst_width<<"x"<<dst_height<<")"<<std::endl;
	return MPXP_False;
    }
    if((dst_width > src_width || dst_height > src_height) && (vidix->cap.flags & FLAG_DOWNSCALER) != FLAG_DOWNSCALER) {
	mpxp_fatal<<"vidix driver can't downscale image ("<<src_width<<"x"<<src_height<<" -> "<<dst_width<<"x"<<dst_height<<")"<<std::endl;
	return MPXP_False;
    }
    image_width = src_width;
    image_height = src_height;
    src_format = format;
    if(forced_fourcc) format = forced_fourcc;
    vidix->playback.fourcc = format;
    vidix->playback.capability = vidix->cap.flags; /* every ;) */
    vidix->playback.blend_factor = 0; /* for now */
    /* display the full picture.
	Nick: we could implement here zooming to a specified area -- alex */
    vidix->playback.src.x = vidix->playback.src.y = 0;
    vidix->playback.src.w = src_width;
    vidix->playback.src.h = src_height;
    vidix->playback.dest.x = x_org;
    vidix->playback.dest.y = y_org;
    vidix->playback.dest.w = dst_width;
    vidix->playback.dest.h = dst_height;
    vidix->playback.num_frames=(vo_conf.use_bm!=1)?NUM_FRAMES-1:1;
    if(vidix->playback.num_frames > vo_conf.xp_buffs) vidix->playback.num_frames = vo_conf.xp_buffs;
    vidix->playback.src.pitch.y = vidix->playback.src.pitch.u = vidix->playback.src.pitch.v = 0;
    if((err=vidix->config_playback())!=0) {
	mpxp_fatal<<"Can't configure playback: "<<strerror(err)<<std::endl;
	return MPXP_False;
    }
    mpxp_v<<"using "<<vidix->playback.num_frames<<" buffers"<<std::endl;
    /* configure busmastering */
    if(vo_conf.use_bm) {
	if(vidix->cap.flags & FLAG_DMA) {
	    int psize = getpagesize();
	    bm_locked=1;
	    for(i=0;i<vo_conf.xp_buffs;i++) {
		if(!bm_buffs[i]) bm_buffs[i] = new(alignmem,psize) uint8_t[vidix->playback.frame_size];
		if(!(bm_buffs[i])) {
		    mpxp_err<<"Can't allocate memory for busmastering"<<std::endl;
		    return MPXP_False;
		}
		if(mlock(bm_buffs[i],vidix->playback.frame_size) != 0) {
		    unsigned j;
		    mpxp_warn<<"Can't lock memory for busmastering"<<std::endl;
		    for(j=0;j<i;j++) munlock(bm_buffs[i],vidix->playback.frame_size);
		    bm_locked=0;
		}
	    }
	    memset(&vidix->dma,0,sizeof(vidix_dma_t));
	    bm_total_frames=bm_slow_frames=0;
	}
	else
	mpxp_err<<"Can not configure bus mastering: your driver is not DMA capable"<<std::endl;
	vo_conf.use_bm = 0;
    }
    if(vo_conf.use_bm) mpxp_ok<<"using BUSMASTERING"<<std::endl;
    mem = static_cast<uint8_t*>(vidix->playback.dga_addr);

    if(!video_clean) {
	/*  clear every frame with correct address and frame_size
	    only once per session */
	for (i = 0; i < vidix->playback.num_frames; i++)
	    memset(mem + vidix->playback.offsets[i], 0x80, vidix->playback.frame_size);
	video_clean=1;
    }
    mpxp_dbg2<<"vidix returns pitches "<<vidix->playback.dest.pitch.y<<" "<<vidix->playback.dest.pitch.u<<" "<<vidix->playback.dest.pitch.v<<std::endl;
    switch(format) {
	case IMGFMT_Y800:
	case IMGFMT_YVU9:
	case IMGFMT_IF09:
	case IMGFMT_I420:
	case IMGFMT_IYUV:
	case IMGFMT_YV12:
	    apitch = vidix->playback.dest.pitch.y-1;
	    vidix->playback.offset.y = (image_width + apitch) & ~apitch;
	    apitch = vidix->playback.dest.pitch.v-1;
	    vidix->playback.offset.v = (image_width + apitch) & ~apitch;
	    apitch = vidix->playback.dest.pitch.u-1;
	    vidix->playback.offset.u = (image_width + apitch) & ~apitch;
	    image_Bpp=1;
	    break;
	case IMGFMT_RGB32:
	case IMGFMT_BGR32:
	    apitch = vidix->playback.dest.pitch.y-1;
	    vidix->playback.offset.y = (image_width*4 + apitch) & ~apitch;
	    vidix->playback.offset.u = vidix->playback.offset.v = 0;
	    image_Bpp=4;
	    break;
	case IMGFMT_RGB24:
	case IMGFMT_BGR24:
	    apitch = vidix->playback.dest.pitch.y-1;
	    vidix->playback.offset.y = (image_width*3 + apitch) & ~apitch;
	    vidix->playback.offset.u = vidix->playback.offset.v = 0;
	    image_Bpp=3;
	    break;
	default:
	    apitch = vidix->playback.dest.pitch.y-1;
	    vidix->playback.offset.y = (image_width*2 + apitch) & ~apitch;
	    vidix->playback.offset.u = vidix->playback.offset.v = 0;
	    image_Bpp=2;
	    break;
    }
    switch(format) {
	case IMGFMT_YVU9:
	case IMGFMT_IF09:
	    vidix->playback.offset.u /= 4;
	    vidix->playback.offset.v /= 4;
	    break;
	case IMGFMT_I420:
	case IMGFMT_IYUV:
	case IMGFMT_YV12:
	    vidix->playback.offset.u /= 2;
	    vidix->playback.offset.v /= 2;
	    break;
    }
    return MPXP_Ok;
}

void Vidix_System::get_surface_caps(dri_surface_cap_t *caps) const
{
    caps->caps = vo_conf.use_bm ? DRI_CAP_TEMP_VIDEO|DRI_CAP_BUSMASTERING : DRI_CAP_VIDEO_MMAPED;
    caps->caps |= DRI_CAP_HORZSCALER | DRI_CAP_VERTSCALER;
    if((vidix->cap.flags & FLAG_DOWNSCALER) == FLAG_DOWNSCALER)
	    caps->caps |= DRI_CAP_DOWNSCALER;
    if((vidix->cap.flags & FLAG_UPSCALER) == FLAG_UPSCALER)
	    caps->caps |= DRI_CAP_UPSCALER;
    caps->fourcc = vidix->playback.fourcc;
    caps->width=vidix->playback.src.w;
    caps->height=vidix->playback.src.h;
    /* in case of vidix movie fit surface */
    caps->x = caps->y=0;
    caps->w=caps->width;
    caps->h=caps->height;
    caps->strides[0] = vidix->playback.offset.y;
    caps->strides[1] = vidix->playback.offset.v;
    caps->strides[2] = vidix->playback.offset.u;
    caps->strides[3] = 0;
}

void Vidix_System::get_surface(dri_surface_t *surf) const
{
    if(vo_conf.use_bm) {
	surf->planes[0] = bm_buffs[surf->idx] + vidix->playback.offset.y;
	surf->planes[1] = bm_buffs[surf->idx] + vidix->playback.offset.v;
	surf->planes[2] = bm_buffs[surf->idx] + vidix->playback.offset.u;
    } else {
	surf->planes[0] = mem + vidix->playback.offsets[surf->idx] + vidix->playback.offset.y;
	surf->planes[1] = mem + vidix->playback.offsets[surf->idx] + vidix->playback.offset.v;
	surf->planes[2] = mem + vidix->playback.offsets[surf->idx] + vidix->playback.offset.u;
    }
    surf->planes[3] = 0;
}

unsigned Vidix_System::get_num_frames() const { return (vo_conf.use_bm==1)?vo_conf.xp_buffs:vidix->playback.num_frames; }

MPXP_Rc Vidix_System::flush_page(unsigned idx) {
    if(vo_conf.use_bm > 1) copy_dma(idx,1);
    return MPXP_Ok;
}
