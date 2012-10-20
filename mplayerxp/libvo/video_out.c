/*
 * video_out.c,
 *
 * Copyright (C) Aaron Holtzman - June 2000
 *
 *  mpeg2dec is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  mpeg2dec is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/mman.h>

#include "mp_config.h"
#include "video_out.h"

#include "../osdep/shmem.h"
#include "../postproc/swscale.h"
#include "../postproc/vf.h"
#include "../dec_ahead.h"
#include "../mplayer.h"
#include "fastmemcpy.h"
#include "img_format.h"
#include "screenshot.h"
#include "bswap.h"
#include "dri_vo.h"
#include "osd.h"
#include "sub.h"
#include "vo_msg.h"

vo_priv_t vo;
static vo_format_desc vod;

//
// Externally visible list of all vo drivers
//
extern vo_functions_t video_out_x11;
extern vo_functions_t video_out_xv;
extern vo_functions_t video_out_dga;
extern vo_functions_t video_out_sdl;
extern vo_functions_t video_out_null;
extern vo_functions_t video_out_pgm;
extern vo_functions_t video_out_md5;
extern vo_functions_t video_out_fbdev;
extern vo_functions_t video_out_png;
extern vo_functions_t video_out_opengl;
#ifdef HAVE_VESA
extern vo_functions_t video_out_vesa;
#endif
#ifdef CONFIG_VIDIX
extern vo_functions_t video_out_xvidix;
#endif

const vo_functions_t* video_out_drivers[] =
{
#ifdef HAVE_XV
        &video_out_xv,
#endif
#ifdef HAVE_OPENGL
        &video_out_opengl,
#endif
#if defined(CONFIG_VIDIX) && defined(HAVE_X11) 
	&video_out_xvidix,
#endif
#ifdef HAVE_DGA
        &video_out_dga,
#endif
#ifdef HAVE_X11
        &video_out_x11,
#endif
#ifdef HAVE_SDL
        &video_out_sdl,
#endif
#ifdef HAVE_VESA
	&video_out_vesa,
#endif
#ifdef HAVE_FBDEV
	&video_out_fbdev,
#endif
        &video_out_null,
        NULL
};

static const vo_functions_t *video_out=NULL;

#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif
#ifndef max
#define max(a,b) ((a)>(b)?(a):(b))
#endif

/* fullscreen:
 * bit 0 (0x01) means fullscreen (-fs)
 * bit 1 (0x02) means mode switching (-vm)
 * bit 2 (0x04) enables software scaling (-zoom)
 * bit 3 (0x08) enables flipping (-flip)
 */
#define VOFLG_FS	0x00000001UL
#define VOFLG_VM	0x00000002UL
#define VOFLG_ZOOM	0x00000004UL
#define VOFLG_FLIP	0x00000008UL
typedef struct dri_priv_s {
    unsigned		flags;
    int			has_dri;
    unsigned		bpp;
    dri_surface_cap_t	cap;
    dri_surface_t	surf[MAX_DRI_BUFFERS];
    unsigned		num_xp_frames;
    int			dr,planes_eq,is_planar,accel;
    unsigned		sstride;
    uint32_t		d_width,d_height;
    unsigned		off[4]; /* offsets for y,u,v if DR on non fully fitted surface */
}dri_priv_t;
static dri_priv_t dri;

typedef struct vo_priv_data_s {
    uint32_t			srcFourcc,image_format,image_width,image_height;
    uint32_t			org_width,org_height;
    unsigned			ps_off[4]; /* offsets for y,u,v in panscan mode */
    unsigned long long int	frame_counter;
    pthread_mutex_t		surfaces_mutex;
}vo_priv_data_t;
static vo_priv_data_t vo_data;

void vo_print_help( void )
{
    unsigned i;
    MSG_INFO("Available video output drivers:\n");
    i=0;
    while (video_out_drivers[i]) {
	const vo_info_t *info = video_out_drivers[i++]->get_info ();
	MSG_INFO("\t%s\t%s\n", info->short_name, info->name);
    }
    MSG_INFO("\n");
}

const vo_functions_t *vo_register(const char *driver_name)
{
  unsigned i;
  if(!driver_name)
    video_out=video_out_drivers[0];
  else
  for (i=0; video_out_drivers[i] != NULL; i++){
    const vo_info_t *info = video_out_drivers[i]->get_info ();
    if(strcmp(info->short_name,driver_name) == 0){
      video_out = video_out_drivers[i];break;
    }
  }
  return video_out;
}

const vo_info_t* vo_get_info( void )
{
    return video_out->get_info();
}

void __FASTCALL__ vo_preinit_structs( void )
{
    pthread_mutexattr_t attr;
    memset(&dri,0,sizeof(dri_priv_t));
    dri.num_xp_frames=1;
    memset(&vo,0,sizeof(vo_priv_t));
    vo.movie_aspect=-1.0;
    vo.flip=-1;
    vo.da_buffs=64;
    vo.window = None;
    vo.WinID=-1;
    vo.osd_progbar_type=-1;
    vo.osd_progbar_value=100;   // 0..256
    memset(&vo_data,0,sizeof(vo_priv_data_t));
    pthread_mutexattr_init(&attr);
    pthread_mutex_init(&vo_data.surfaces_mutex,&attr);
}

int __FASTCALL__ vo_init(const char *subdevice)
{
    MSG_DBG3("dri_vo_dbg: vo_init(%s)\n",subdevice);
    vo_data.frame_counter=0;
    return video_out->preinit(subdevice);
}

int __FASTCALL__ vo_describe_fourcc(uint32_t fourcc,vo_format_desc *vd)
{
    int is_planar;
    is_planar=0;
    vd->x_mul[0]=vd->x_mul[1]=vd->x_mul[2]=vd->x_mul[3]=1;
    vd->x_div[0]=vd->x_div[1]=vd->x_div[2]=vd->x_div[3]=1;
    vd->y_mul[0]=vd->y_mul[1]=vd->y_mul[2]=vd->y_mul[3]=1;
    vd->y_div[0]=vd->y_div[1]=vd->y_div[2]=vd->y_div[3]=1;
	switch(fourcc)
	{
		case IMGFMT_Y800:
		    is_planar=1;
		case IMGFMT_RGB8:
		case IMGFMT_BGR8:
		    vd->bpp = 8;
		    break;
		case IMGFMT_YVU9:
		case IMGFMT_IF09:
		    vd->bpp = 9;
		    vd->x_div[1]=vd->x_div[2]=4;
		    vd->y_div[1]=vd->y_div[2]=4;
		    is_planar=1;
		    break;
		case IMGFMT_YV12:
		case IMGFMT_I420:
		case IMGFMT_IYUV:
		    vd->bpp = 12;
		    vd->x_div[1]=vd->x_div[2]=2;
		    vd->y_div[1]=vd->y_div[2]=2;
		    is_planar=1;
		    break;
		case IMGFMT_YUY2:
		case IMGFMT_YVYU:
		case IMGFMT_UYVY:
		    vd->x_mul[0]=2;
		    vd->bpp = 16;
		    break;
		case IMGFMT_RGB15:
		case IMGFMT_BGR15:
		    vd->bpp = 15;
		    vd->x_mul[0]=2;
		    break;
		case IMGFMT_RGB16:
		case IMGFMT_BGR16:
		    vd->bpp = 16;
		    vd->x_mul[0]=2;
		    break;
		case IMGFMT_RGB24:
		case IMGFMT_BGR24:
		    vd->bpp = 24;
		    vd->x_mul[0]=3;
		    break;
		case IMGFMT_RGB32:
		case IMGFMT_BGR32:
		    vd->bpp = 32;
		    vd->x_mul[0]=4;
		    break;
		default:
		    /* unknown fourcc */
		    vd->bpp=0;
		    break;
	}
    return is_planar;
}

static void __FASTCALL__ dri_config(uint32_t fourcc)
{
    unsigned i;
    dri.is_planar = vo_describe_fourcc(fourcc,&vod);
    dri.bpp=vod.bpp;
    if(!dri.bpp) dri.has_dri=0; /*unknown fourcc*/
    if(dri.has_dri)
    {
	video_out->control(VOCTRL_GET_NUM_FRAMES,&dri.num_xp_frames);
	dri.num_xp_frames=min(dri.num_xp_frames,MAX_DRI_BUFFERS);
	for(i=0;i<dri.num_xp_frames;i++)
	{
	    dri.surf[i].idx=i;
	    video_out->control(DRI_GET_SURFACE,&dri.surf[i]);
	}
    }
}

static void __FASTCALL__ ps_tune(unsigned width,unsigned height)
{
    int src_is_planar;
    unsigned src_stride,ps_x,ps_y;
    vo_format_desc vd;
    ps_x = (vo_data.org_width - width)/2;
    ps_y = (vo_data.org_height - height)/2;
    src_is_planar = vo_describe_fourcc(vo_data.srcFourcc,&vd);
    src_stride=src_is_planar?vo_data.org_width:vo_data.org_width*((vd.bpp+7)/8);
    vo_data.ps_off[0] = vo_data.ps_off[1] = vo_data.ps_off[2] = vo_data.ps_off[3] = 0;
    if(!src_is_planar)
	vo_data.ps_off[0] = ps_y*src_stride+ps_x*((vd.bpp+7)/8);
    else
    {
	vo_data.ps_off[0] = ps_y*src_stride+ps_x;
	if(vd.bpp==12) /*YV12 series*/
	{
		vo_data.ps_off[1] = (ps_y/2)*(src_stride/2)+ps_x/2;
		vo_data.ps_off[2] = (ps_y/2)*(src_stride/2)+ps_x/2;
	}
	else
	if(vd.bpp==9) /*YVU9 series*/
	{
		vo_data.ps_off[1] = (ps_y/4)*(src_stride/4)+ps_x/4;
		vo_data.ps_off[2] = (ps_y/4)*(src_stride/4)+ps_x/4;
	}
    }
}

static void __FASTCALL__ dri_tune(unsigned width,unsigned height)
{
    dri.sstride=dri.is_planar?width:width*((dri.bpp+7)/8);
    dri.off[0] = dri.off[1] = dri.off[2] = dri.off[3] = 0;
    if(!dri.is_planar)
    {
	dri.planes_eq = dri.sstride == dri.cap.strides[0];
	dri.off[0] = dri.cap.y*dri.cap.strides[0]+dri.cap.x*((dri.bpp+7)/8);
    }
    else
    {
	unsigned long y_off,u_off,v_off;
	y_off = (unsigned long)dri.surf[0].planes[0];
	u_off = (unsigned long)min(dri.surf[0].planes[1],dri.surf[0].planes[2]);
	v_off = (unsigned long)max(dri.surf[0].planes[1],dri.surf[0].planes[2]);
	dri.off[0] = dri.cap.y*dri.cap.strides[0]+dri.cap.x;
	if(dri.bpp==12) /*YV12 series*/
	{
		dri.planes_eq = width == dri.cap.strides[0] &&
			width*height == u_off - y_off &&
			width*height*5/4 == v_off - y_off &&
			dri.cap.strides[0]/2 == dri.cap.strides[1] &&
			dri.cap.strides[0]/2 == dri.cap.strides[2];
		dri.off[1] = (dri.cap.y/2)*dri.cap.strides[1]+dri.cap.x/2;
		dri.off[2] = (dri.cap.y/2)*dri.cap.strides[2]+dri.cap.x/2;
	}
	else
	if(dri.bpp==9) /*YVU9 series*/
	{
		dri.planes_eq = width == dri.cap.strides[0] &&
			width*height == u_off - y_off &&
			width*height*17/16 == v_off - y_off &&
			dri.cap.strides[0]/4 == dri.cap.strides[1] &&
			dri.cap.strides[0]/4 == dri.cap.strides[2];
		dri.off[1] = (dri.cap.y/4)*dri.cap.strides[1]+dri.cap.x/4;
		dri.off[2] = (dri.cap.y/4)*dri.cap.strides[2]+dri.cap.x/4;
	}
	else
	if(dri.bpp==8) /*Y800 series*/
		dri.planes_eq = width == dri.cap.strides[0];
    }
    dri.accel=(dri.cap.caps&(DRI_CAP_DOWNSCALER|DRI_CAP_HORZSCALER|
			    DRI_CAP_UPSCALER|DRI_CAP_VERTSCALER))==
			    (DRI_CAP_DOWNSCALER|DRI_CAP_HORZSCALER|
			    DRI_CAP_UPSCALER|DRI_CAP_VERTSCALER);
    dri.dr = vo_data.srcFourcc == dri.cap.fourcc && !(dri.flags & VOFLG_FLIP) &&
			    !vo_data.ps_off[0] && !vo_data.ps_off[1] && !vo_data.ps_off[2] && !vo_data.ps_off[3];
    if(dri.dr && dri.cap.w < width)
	dri.dr = dri.cap.caps&(DRI_CAP_DOWNSCALER|DRI_CAP_HORZSCALER)?1:0;
    if(dri.dr && dri.cap.w > width)
	dri.dr = dri.cap.caps&(DRI_CAP_UPSCALER|DRI_CAP_HORZSCALER)?1:0;
    if(dri.dr && dri.cap.h < height)
	dri.dr = dri.cap.caps&(DRI_CAP_DOWNSCALER|DRI_CAP_VERTSCALER)?1:0;
    if(dri.dr && dri.cap.h > height)
	dri.dr = dri.cap.caps&(DRI_CAP_UPSCALER|DRI_CAP_VERTSCALER)?1:0;
}

static void __FASTCALL__ dri_reconfig( uint32_t event )
{
	dri.has_dri = 1;
	video_out->control(DRI_GET_SURFACE_CAPS,&dri.cap);
	dri_config(dri.cap.fourcc);
	/* ugly workaround of swapped BGR-fourccs. Should be removed in the future */
	if(!dri.has_dri)
	{
		dri.has_dri=1;
		dri.cap.fourcc = bswap_32(dri.cap.fourcc);
		dri_config(dri.cap.fourcc);
	}
	dri_tune(vo_data.image_width,vo_data.image_height);
	/* TODO: smart analizer of scaling possibilities of vo_driver */
	if((event & VO_EVENT_RESIZE) == VO_EVENT_RESIZE)
	{
	    xp_core.in_resize=1;
	    vf_reinit_vo(dri.cap.w,dri.cap.h,dri.cap.fourcc,1);
	}
	vf_reinit_vo(dri.cap.w,dri.cap.h,dri.cap.fourcc,0);
}

static int vo_inited=0;
uint32_t __FASTCALL__ vo_config(uint32_t width, uint32_t height, uint32_t d_width,
		   uint32_t d_height, uint32_t fullscreen, char *title,
		   uint32_t format,const vo_tune_info_t *vti)
{
    uint32_t retval;
    unsigned dest_fourcc,w,d_w,h,d_h;
    MSG_DBG3("dri_vo_dbg: vo_config\n");
    if(vo_inited)
    {
	MSG_FATAL("!!!video_out internal fatal error: video_out is initialized more than once!!!\n");
	return -1;
    }
    vo_inited++;
    dest_fourcc = format;
    vo_data.org_width = width;
    vo_data.org_height = height;

    w = width;
    d_w = d_width;
    h = height;
    d_h = d_height;

    dri.d_width = d_w;
    dri.d_height = d_h;
    MSG_V("video_out->config(%u,%u,%u,%u,%u,'%s',%s)\n"
	,w,h,d_w,d_h,fullscreen,title,vo_format_name(dest_fourcc));
    retval = video_out->config(w,h,d_w,d_h,fullscreen,title,dest_fourcc,vti);
    vo_data.srcFourcc=format;
    if(retval == 0)
    {
	int dri_retv;
	dri_retv = video_out->control(DRI_GET_SURFACE_CAPS,&dri.cap);
	vo_data.image_format = format;
	vo_data.image_width = w;
	vo_data.image_height = h;
	ps_tune(vo_data.image_width,vo_data.org_height);
	if(dri_retv == VO_TRUE) dri_reconfig(0);
	MSG_V("dri_vo_caps: driver does %s support DRI\n",dri.has_dri?"":"not");
	MSG_V("dri_vo_caps: caps=%08X fourcc=%08X(%s) x,y,w,h(%u %u %u %u)\n"
	      "dri_vo_caps: width_height(%u %u) strides(%u %u %u %u) dri.bpp=%u\n"
		,dri.cap.caps
		,dri.cap.fourcc
		,vo_format_name(dri.cap.fourcc)
		,dri.cap.x,dri.cap.y,dri.cap.w,dri.cap.h
		,dri.cap.width,dri.cap.height
		,dri.cap.strides[0],dri.cap.strides[1]
		,dri.cap.strides[2],dri.cap.strides[3]
		,dri.bpp);
	MSG_V("dri_vo_src: w,h(%u %u) d_w,d_h(%u %u)\n"
	      "dri_vo_src: flags=%08X fourcc=%08X(%s)\n"
		,width,height
		,d_width,d_height
		,fullscreen
		,format
		,vo_format_name(format));
	dri.flags = fullscreen;
    }
    return retval;
}

/* if vo_driver doesn't support dri then it won't work with this logic */
uint32_t __FASTCALL__ vo_query_format( uint32_t* fourcc, unsigned src_w, unsigned src_h)
{
    uint32_t retval,dri_forced_fourcc;
    vo_query_fourcc_t qfourcc;
    MSG_DBG3("dri_vo_dbg: vo_query_format(%08lX)\n",*fourcc);
    qfourcc.fourcc = *fourcc;
    qfourcc.w = src_w;
    qfourcc.h = src_h;
    retval = video_out->control(VOCTRL_QUERY_FORMAT,&qfourcc);
    MSG_V("dri_vo: request for %s fourcc: %s\n",vo_format_name(*fourcc),retval?"OK":"False");
    dri_forced_fourcc = *fourcc;
    if(retval) retval = 0x3; /* supported without convertion */
    return retval;
}

uint32_t vo_reset( void )
{
    MSG_DBG3("dri_vo_dbg: vo_reset\n");
    return video_out->control(VOCTRL_RESET,NULL);
}

uint32_t vo_screenshot( unsigned idx )
{
    char buf[256];
    MSG_DBG3("dri_vo_dbg: vo_screenshot\n");
    sprintf(buf,"%llu",vo_data.frame_counter);
    return gr_screenshot(buf,dri.surf[idx].planes,dri.cap.strides,dri.cap.fourcc,dri.cap.width,dri.cap.height);
}

uint32_t vo_pause( void )
{
    MSG_DBG3("dri_vo_dbg: vo_pause\n");
    return video_out->control(VOCTRL_PAUSE,0);
}

uint32_t vo_resume( void )
{
    MSG_DBG3("dri_vo_dbg: vo_resume\n");
    return video_out->control(VOCTRL_RESUME,0);
}

void vo_lock_surfaces(void) { pthread_mutex_lock(&vo_data.surfaces_mutex); }
void vo_unlock_surfaces(void) { pthread_mutex_unlock(&vo_data.surfaces_mutex); }

uint32_t __FASTCALL__ vo_get_surface( mp_image_t* mpi, unsigned decoder_idx)
{
    int width_less_stride;
    MSG_DBG2("dri_vo_dbg: vo_get_surface type=%X flg=%X\n",mpi->type,mpi->flags);
    width_less_stride = 0;
    mpi->xp_idx = decoder_idx;
    if(mpi->flags & MP_IMGFLAG_PLANAR)
    {
	width_less_stride = mpi->w <= dri.cap.strides[0] &&
			    (mpi->w>>mpi->chroma_x_shift) <= dri.cap.strides[1] &&
			    (mpi->w>>mpi->chroma_x_shift) <= dri.cap.strides[2];
    }
    else width_less_stride = mpi->w*mpi->bpp <= dri.cap.strides[0];
    if(dri.has_dri)
    {
	/* static is singlebuffered decoding */
	if(mpi->type==MP_IMGTYPE_STATIC && dri.num_xp_frames>1)
	{
	    MSG_DBG2("dri_vo_dbg: vo_get_surface FAIL mpi->type==MP_IMGTYPE_STATIC && dri.num_xp_frames>1\n");
	    return VO_FALSE;
	}
	/*I+P requires 2+ static buffers for R/W */
	if(mpi->type==MP_IMGTYPE_IP && (dri.num_xp_frames < 2 || (dri.cap.caps&DRI_CAP_VIDEO_MMAPED)==DRI_CAP_VIDEO_MMAPED))
	{
	    MSG_DBG2("dri_vo_dbg: vo_get_surface FAIL (mpi->type==MP_IMGTYPE_IP && dri.num_xp_frames < 2) || (dri.cap.caps&DRI_CAP_VIDEO_MMAPED)==DRI_CAP_VIDEO_MMAPED\n");
	    return VO_FALSE;
	}
	/*I+P+B requires 3+ static buffers for R/W */
	if(mpi->type==MP_IMGTYPE_IPB && (dri.num_xp_frames != 3 || (dri.cap.caps&DRI_CAP_VIDEO_MMAPED)==DRI_CAP_VIDEO_MMAPED))
	{
	    MSG_DBG2("dri_vo_dbg: vo_get_surface FAIL (mpi->type==MP_IMGTYPE_IPB && dri.num_xp_frames != 3) || (dri.cap.caps&DRI_CAP_VIDEO_MMAPED)==DRI_CAP_VIDEO_MMAPED\n");
	    return VO_FALSE;
	}
	/* video surface is bad thing for reading */
	if(((mpi->flags&MP_IMGFLAG_READABLE)||(mpi->type==MP_IMGTYPE_TEMP)) && (dri.cap.caps&DRI_CAP_VIDEO_MMAPED)==DRI_CAP_VIDEO_MMAPED)
	{
	    MSG_DBG2("dri_vo_dbg: vo_get_surface FAIL mpi->flags&MP_IMGFLAG_READABLE && (dri.cap.caps&DRI_CAP_VIDEO_MMAPED)==DRI_CAP_VIDEO_MMAPED\n");
	    return VO_FALSE;
	}
	/* it seems that surfaces are equal */
	if((((mpi->flags&MP_IMGFLAG_ACCEPT_STRIDE) && width_less_stride) || dri.planes_eq) && dri.dr)
	{
	    vo_lock_surfaces();
	    mpi->planes[0]=dri.surf[decoder_idx].planes[0]+dri.off[0];
	    mpi->planes[1]=dri.surf[decoder_idx].planes[1]+dri.off[1];
	    mpi->planes[2]=dri.surf[decoder_idx].planes[2]+dri.off[2];
	    mpi->stride[0]=dri.cap.strides[0];
	    mpi->stride[1]=dri.cap.strides[1];
	    mpi->stride[2]=dri.cap.strides[2];
	    mpi->flags|=MP_IMGFLAG_DIRECT;
	    vo_unlock_surfaces();
	    MSG_DBG2("dri_vo_dbg: vo_get_surface OK\n");
	    return VO_TRUE;
	}
	MSG_DBG2("dri_vo_dbg: vo_get_surface FAIL (mpi->flags&MP_IMGFLAG_ACCEPT_STRIDE && width_less_stride) || dri.planes_eq) && dri.dr\n");
	return VO_FALSE;
    }
    else return VO_FALSE;
}

static int __FASTCALL__ adjust_size(unsigned cw,unsigned ch,unsigned *nw,unsigned *nh)
{
    MSG_DBG3("dri_vo_dbg: adjust_size was called %u %u %u %u\n",cw,ch,*nw,*nh);
    if((dri.flags & VOFLG_ZOOM) && (cw != *nw || ch != *nh) && !(dri.flags & VOFLG_FS))
    {
	float aspect,newv;
	aspect = (float)dri.d_width / (float)dri.d_height;
	if(abs(cw-*nw) > abs(ch-*nh))
	{
	    newv = ((float)(*nw))/aspect;
	    *nh = newv;
	    if(newv-(float)(unsigned)newv > 0.5) (*nh)++;
	}
	else
	{
	    newv = ((float)(*nh))*aspect;
	    *nw = newv;
	    if(newv-(float)(unsigned)newv > 0.5) (*nw)++;
	}
	MSG_DBG3("dri_vo_dbg: adjust_size returns %u %u\n",*nw,*nh);
	return 1;
    }
    return 0;
}

int vo_check_events( void )
{
    uint32_t retval;
    int need_repaint;
    vo_resize_t vrest;
    MSG_DBG3("dri_vo_dbg: vo_check_events\n");
    vrest.event_type = 0;
    vrest.adjust_size = adjust_size;
    retval = video_out->control(VOCTRL_CHECK_EVENTS,&vrest);
    /* it's ok since accelerated drivers doesn't touch surfaces
       but there is only one driver (vo_x11) which changes surfaces
       on 'fullscreen' key */
    need_repaint=0;
    if(dri.has_dri && retval == VO_TRUE && (vrest.event_type & VO_EVENT_RESIZE) == VO_EVENT_RESIZE)
    {
	need_repaint=1;
	dri_reconfig(vrest.event_type);
    }
    return (need_repaint && !dri.accel) || (vrest.event_type&VO_EVENT_FORCE_UPDATE);
}

uint32_t vo_fullscreen( void )
{
    uint32_t retval,etype;
    MSG_DBG3("dri_vo_dbg: vo_fullscreen\n");
    etype = 0;
    retval = video_out->control(VOCTRL_FULLSCREEN,&etype);
    if(dri.has_dri && retval == VO_TRUE && (etype & VO_EVENT_RESIZE) == VO_EVENT_RESIZE)
	dri_reconfig(etype);
    if(retval == VO_TRUE) dri.flags ^= VOFLG_FS;
    return retval;
}

unsigned __FASTCALL__ vo_get_num_frames( void ) { return dri.num_xp_frames; }

uint32_t __FASTCALL__ vo_draw_slice(const mp_image_t *mpi)
{
    unsigned i,_w[4],_h[4],x,y;
    MSG_DBG3("dri_vo_dbg: vo_draw_slice xywh=%i %i %i %i\n",mpi->x,mpi->y,mpi->w,mpi->h);
    if(dri.has_dri)
    {
	uint8_t *dst[4];
	const uint8_t *ps_src[4];
	int dstStride[4];
	int finalize=vo_is_final();
	unsigned idx = mpi->xp_idx;
	for(i=0;i<4;i++)
	{
	    dst[i]=dri.surf[idx].planes[i]+dri.off[i];
	    dstStride[i]=dri.cap.strides[i];
	    dst[i]+=((mpi->y*dstStride[i])*vod.y_mul[i])/vod.y_div[i];
	    dst[i]+=(mpi->x*vod.x_mul[i])/vod.x_div[i];
	    _w[i]=(mpi->w*vod.x_mul[i])/vod.x_div[i];
	    _h[i]=(mpi->h*vod.y_mul[i])/vod.y_div[i];
	    y = i?(mpi->y>>mpi->chroma_y_shift):mpi->y;
	    x = i?(mpi->x>>mpi->chroma_x_shift):mpi->x;
	    ps_src[i] = mpi->planes[i]+(y*mpi->stride[i])+x+vo_data.ps_off[i];
	}
	for(i=0;i<4;i++) {
	    if(mpi->stride[i]) {
		if(finalize)
		    stream_copy_pic(dst[i],ps_src[i],_w[i],_h[i],dstStride[i],mpi->stride[i]);
		else
		    memcpy_pic(dst[i],ps_src[i],_w[i],_h[i],dstStride[i],mpi->stride[i]);
	    }
	}
	return 0;
    }
    return -1;
}

void vo_select_frame(unsigned play_idx)
{
    MSG_DBG2("dri_vo_dbg: vo_select_frame(play_idx=%u)\n",play_idx);
    video_out->select_frame(play_idx);
}

void vo_flush_page(unsigned decoder_idx)
{
    MSG_DBG3("dri_vo_dbg: vo_flush_pages [idx=%u]\n",decoder_idx);
    vo_data.frame_counter++;
    if((dri.cap.caps & DRI_CAP_VIDEO_MMAPED)!=DRI_CAP_VIDEO_MMAPED)
					video_out->control(VOCTRL_FLUSH_PAGES,&decoder_idx);
}

/* DRAW OSD */

static void __FASTCALL__ clear_rect(unsigned _y0,unsigned h,uint8_t *dest,unsigned stride,unsigned dstride,uint8_t filler)
{
  unsigned i;
  for(i=0;i<h;i++)
  {
      if(_y0+i<dri.cap.y||_y0+i>=dri.cap.y+dri.cap.h) memset(dest,filler,stride);
      dest += dstride;
  }
}

static void __FASTCALL__ clear_rect2(unsigned _y0,unsigned h,uint8_t *dest,unsigned stride,unsigned dstride,uint8_t filler)
{
  unsigned i;
  unsigned y1 = dri.cap.y/2;
  unsigned y2 = (dri.cap.y+dri.cap.h)/2;
  for(i=0;i<h;i++)
  {
      if(_y0+i<y1||_y0+i>=y2) memset(dest,filler,stride);
      dest += dstride;
  }
}

static void __FASTCALL__ clear_rect4(unsigned _y0,unsigned h,uint8_t *dest,unsigned stride,unsigned dstride,uint8_t filler)
{
  unsigned i;
  unsigned y1 = dri.cap.y/4;
  unsigned y2 = (dri.cap.y+dri.cap.h)/4;
  for(i=0;i<h;i++)
  {
      if(_y0+i<y1||_y0+i>=y2) memset(dest,filler,stride);
      dest += dstride;
  }
}

static void __FASTCALL__ clear_rect_rgb(unsigned _y0,unsigned h,uint8_t *dest,unsigned stride,unsigned dstride)
{
  unsigned i;
  for(i=0;i<h;i++)
  {
      if(_y0+i<dri.cap.y||_y0+i>=dri.cap.y+dri.cap.h) memset(dest,0,stride);
      dest += dstride;
  }
}

static void __FASTCALL__ clear_rect_yuy2(unsigned _y0,unsigned h,uint8_t *dest,unsigned stride,unsigned dstride)
{
  unsigned i;
  for(i=0;i<h;i++)
  {
	if(_y0+i<dri.cap.y||_y0+i>=dri.cap.y+dri.cap.h) 
	{
	    uint32_t *dst32;
	    unsigned j,size32;
	    size32=stride/4;
	    dst32=(uint32_t*)dest;
	    for(j=0;j<size32;j+=4) 
		dst32[j]=dst32[j+1]=dst32[j+2]=dst32[j+3]=0x80108010;
	    for(;j<size32;j+=4) 
		dst32[j]=0x80108010;
	}
	dest += dstride;
  }
}

static void __FASTCALL__ dri_remove_osd(unsigned idx,int x0,int _y0, int w,int h)
{
    if(x0+w<=dri.cap.width&&_y0+h<=dri.cap.height)
    switch(dri.cap.fourcc)
    {
	case IMGFMT_RGB15:
	case IMGFMT_BGR15:
	case IMGFMT_RGB16:
	case IMGFMT_BGR16:
	case IMGFMT_RGB24:
	case IMGFMT_BGR24:
	case IMGFMT_RGB32:
	case IMGFMT_BGR32:
		clear_rect_rgb( _y0,h,dri.surf[idx].planes[0]+_y0*dri.cap.strides[0]+x0*((dri.bpp+7)/8),
			    w*(dri.bpp+7)/8,dri.cap.strides[0]);
		break;
	case IMGFMT_YVYU:
	case IMGFMT_YUY2:
		clear_rect_yuy2( _y0,h,dri.surf[idx].planes[0]+_y0*dri.cap.strides[0]+x0*2,
			    w*2,dri.cap.strides[0]);
		break;
	case IMGFMT_UYVY:
		clear_rect_yuy2( _y0,h,dri.surf[idx].planes[0]+_y0*dri.cap.strides[0]+x0*2+1,
			    w*2,dri.cap.strides[0]);
		break;
	case IMGFMT_Y800:
		clear_rect( _y0,h,dri.surf[idx].planes[0]+_y0*dri.cap.strides[0]+x0,
			    w,dri.cap.strides[0],0x10);
		break;
	case IMGFMT_YV12:
	case IMGFMT_I420:
	case IMGFMT_IYUV:
		clear_rect( _y0,h,dri.surf[idx].planes[0]+_y0*dri.cap.strides[0]+x0,
			    w,dri.cap.strides[0],0x10);
		clear_rect2( _y0/2,h/2,dri.surf[idx].planes[1]+_y0/2*dri.cap.strides[1]+x0/2,
			    w/2,dri.cap.strides[1],0x80);
		clear_rect2( _y0/2,h/2,dri.surf[idx].planes[2]+_y0/2*dri.cap.strides[2]+x0/2,
			    w/2,dri.cap.strides[2],0x80);
		break;
	case IMGFMT_YVU9:
	case IMGFMT_IF09:
		clear_rect( _y0,h,dri.surf[idx].planes[0]+_y0*dri.cap.strides[0]+x0,
			    w,dri.cap.strides[0],0x10);
		clear_rect4( _y0/4,h/4,dri.surf[idx].planes[1]+_y0/4*dri.cap.strides[1]+x0/4,
			    w/4,dri.cap.strides[1],0x80);
		clear_rect4( _y0/4,h/4,dri.surf[idx].planes[2]+_y0/4*dri.cap.strides[2]+x0/4,
			    w/4,dri.cap.strides[2],0x80);
		break;
    }
}

static draw_alpha_f draw_alpha=NULL;
static draw_alpha_f __FASTCALL__ get_draw_alpha(uint32_t fmt) {
  MSG_DBG2("get_draw_alpha(%s)\n",vo_format_name(fmt));
  switch(fmt) {
  case IMGFMT_BGR15:
  case IMGFMT_RGB15:
    return vo_draw_alpha_rgb15_ptr;
  case IMGFMT_BGR16:
  case IMGFMT_RGB16:
    return vo_draw_alpha_rgb16_ptr;
  case IMGFMT_BGR24:
  case IMGFMT_RGB24:
    return vo_draw_alpha_rgb24_ptr;
  case IMGFMT_BGR32:
  case IMGFMT_RGB32:
    return vo_draw_alpha_rgb32_ptr;
  case IMGFMT_YV12:
  case IMGFMT_I420:
  case IMGFMT_IYUV:
  case IMGFMT_YVU9:
  case IMGFMT_IF09:
  case IMGFMT_Y800:
  case IMGFMT_Y8:
    return vo_draw_alpha_yv12_ptr;
  case IMGFMT_YUY2:
    return vo_draw_alpha_yuy2_ptr;
  case IMGFMT_UYVY:
    return vo_draw_alpha_uyvy_ptr;
  }

  return NULL;
}

static void __FASTCALL__ dri_draw_osd(unsigned idx,int x0,int _y0, int w,int h,const unsigned char* src,const unsigned char *srca, int stride)
{
    int finalize=vo_is_final();
    if(x0+w<=dri.cap.width&&_y0+h<=dri.cap.height)
    {
	if(!draw_alpha) draw_alpha=get_draw_alpha(dri.cap.fourcc);
	if(draw_alpha)
	    (*draw_alpha)(w,h,src,srca,stride,
			    dri.surf[idx].planes[0]+dri.cap.strides[0]*_y0+x0*((dri.bpp+7)/8),
			    dri.cap.strides[0],finalize);
    }
}

void vo_draw_osd(unsigned idx)
{
    MSG_DBG3("dri_vo_dbg: vo_draw_osd\n");
    if(dri.has_dri && !(dri.cap.caps & DRI_CAP_HWOSD))
    {
	if( dri.cap.x || dri.cap.y ||
	    dri.cap.w != dri.cap.width || dri.cap.h != dri.cap.height)
		    vo_remove_text(idx,dri.cap.width,dri.cap.height,dri_remove_osd);
	vo_draw_text(idx,dri.cap.width,dri.cap.height,dri_draw_osd);
    }
}

void vo_draw_spudec_direct(unsigned idx)
{
    MSG_DBG3("dri_vo_dbg: vo_draw_osd\n");
    if(dri.has_dri && !(dri.cap.caps & DRI_CAP_HWOSD))
    {
//	if( dri.cap.x || dri.cap.y ||
//	    dri.cap.w != dri.cap.width || dri.cap.h != dri.cap.height)
//		    vo_remove_text(idx,dri.cap.width,dri.cap.height,dri_remove_osd);
	vo_draw_spudec(idx,dri.cap.width,dri.cap.height,dri_draw_osd);
    }
}

void vo_uninit( void )
{
    MSG_DBG3("dri_vo_dbg: vo_uninit\n");
    vo_inited--;
    video_out->uninit();
}

uint32_t __FASTCALL__ vo_control(uint32_t request, any_t*data)
{
    uint32_t rval;
    rval=video_out->control(request,data);
    MSG_DBG3("dri_vo_dbg: %u=vo_control( %u, %p )\n",rval,request,data);
    return rval;
}

int __FASTCALL__ vo_is_final(void) {
    int mmaped=dri.cap.caps&DRI_CAP_VIDEO_MMAPED;
    int busmaster=dri.cap.caps&DRI_CAP_BUSMASTERING;
    return mmaped||busmaster||(dri.num_xp_frames>1);
}

