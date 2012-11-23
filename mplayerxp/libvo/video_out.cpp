#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
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
#include <algorithm>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/mman.h>

#include "video_out.h"

#include "osdep/shmem.h"
#include "mp_conf_lavc.h"
#include "postproc/vf.h"
#include "xmpcore/xmp_core.h"
#include "mplayerxp.h"
#include "osdep/fastmemcpy.h"
#include "img_format.h"
#include "screenshot.h"
#include "osdep/bswap.h"
#include "dri_vo.h"
#include "osd.h"
#include "sub.h"
#include "vo_msg.h"

vo_conf_t vo_conf;

//
// Externally visible list of all vo drivers
//
extern const vo_functions_t video_out_x11;
extern const vo_functions_t video_out_xv;
extern const vo_functions_t video_out_dga;
extern const vo_functions_t video_out_sdl;
extern const vo_functions_t video_out_null;
extern const vo_functions_t video_out_pgm;
extern const vo_functions_t video_out_md5;
extern const vo_functions_t video_out_fbdev;
extern const vo_functions_t video_out_png;
extern const vo_functions_t video_out_opengl;
#ifdef HAVE_VESA
extern const vo_functions_t video_out_vesa;
#endif

static const vo_functions_t* video_out_drivers[] =
{
#ifdef HAVE_XV
	&video_out_xv,
#endif
#ifdef HAVE_OPENGL
	&video_out_opengl,
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

typedef struct vo_priv_s {
    char			antiviral_hole[RND_CHAR8];
    uint32_t			srcFourcc,image_format,image_width,image_height;
    uint32_t			org_width,org_height;
    unsigned			ps_off[4]; /* offsets for y,u,v in panscan mode */
    unsigned long long int	frame_counter;
    pthread_mutex_t		surfaces_mutex;
    vo_format_desc		vod;
    dri_priv_t			dri;
    const vo_functions_t *	video_out;
    draw_alpha_f		draw_alpha;
}vo_priv_t;

void vo_print_help(vo_data_t*vo)
{
    unsigned i;
    MSG_INFO("Available video output drivers:\n");
    i=0;
    while (video_out_drivers[i]) {
	const vo_info_t *info = video_out_drivers[i++]->get_info (vo);
	MSG_INFO("\t%s\t%s\n", info->short_name, info->name);
    }
    MSG_INFO("\n");
}

const vo_functions_t* vo_register(vo_data_t*vo,const char *driver_name)
{
    vo_priv_t* priv=(vo_priv_t*)vo->vo_priv;
    unsigned i;
    if(!driver_name) priv->video_out=video_out_drivers[0];
    else
    for (i=0; video_out_drivers[i] != &video_out_null; i++){
	const vo_info_t *info = video_out_drivers[i]->get_info (vo);
	if(strcmp(info->short_name,driver_name) == 0){
	    priv->video_out = video_out_drivers[i];break;
	}
    }
    return priv->video_out;
}

const vo_info_t* vo_get_info(vo_data_t*vo)
{
    vo_priv_t* priv=(vo_priv_t*)vo->vo_priv;
    return priv->video_out->get_info(vo);
}

vo_data_t* __FASTCALL__ vo_preinit_structs( void )
{
    vo_data_t* vo;
    pthread_mutexattr_t attr;

    memset(&vo_conf,0,sizeof(vo_conf_t));
    vo_conf.movie_aspect=-1.0;
    vo_conf.flip=-1;
    vo_conf.xp_buffs=64;

    vo=new(zeromem) vo_data_t;
    vo->window = None;
    vo->osd_progbar_type=-1;
    vo->osd_progbar_value=100;   // 0..256

    vo->vo_priv=new(zeromem) vo_priv_t;
    vo_priv_t* priv=(vo_priv_t*)vo->vo_priv;
    rnd_fill(priv->antiviral_hole,offsetof(vo_priv_t,srcFourcc)-offsetof(vo_priv_t,antiviral_hole));
    pthread_mutexattr_init(&attr);
    pthread_mutex_init(&priv->surfaces_mutex,&attr);
    priv->dri.num_xp_frames=1;
    rnd_fill(vo->antiviral_hole,offsetof(vo_data_t,mScreen)-offsetof(vo_data_t,antiviral_hole));
    return vo;
}

MPXP_Rc __FASTCALL__ vo_init(vo_data_t*vo,const char *subdevice)
{
    vo_priv_t* priv=(vo_priv_t*)vo->vo_priv;
    MSG_DBG3("dri_vo_dbg: vo_init(%s)\n",subdevice);
    priv->frame_counter=0;
    return priv->video_out->preinit(vo,subdevice);
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

static void __FASTCALL__ dri_config(vo_data_t*vo,uint32_t fourcc)
{
    vo_priv_t* priv=(vo_priv_t*)vo->vo_priv;
    unsigned i;
    priv->dri.is_planar = vo_describe_fourcc(fourcc,&priv->vod);
    priv->dri.bpp=priv->vod.bpp;
    if(!priv->dri.bpp) priv->dri.has_dri=0; /*unknown fourcc*/
    if(priv->dri.has_dri)
    {
	priv->video_out->control(vo,VOCTRL_GET_NUM_FRAMES,&priv->dri.num_xp_frames);
	priv->dri.num_xp_frames=std::min(priv->dri.num_xp_frames,unsigned(MAX_DRI_BUFFERS));
	for(i=0;i<priv->dri.num_xp_frames;i++)
	{
	    priv->dri.surf[i].idx=i;
	    priv->video_out->control(vo,DRI_GET_SURFACE,&priv->dri.surf[i]);
	}
    }
}

static void __FASTCALL__ ps_tune(vo_data_t*vo,unsigned width,unsigned height)
{
    vo_priv_t* priv=(vo_priv_t*)vo->vo_priv;
    int src_is_planar;
    unsigned src_stride,ps_x,ps_y;
    vo_format_desc vd;
    ps_x = (priv->org_width - width)/2;
    ps_y = (priv->org_height - height)/2;
    src_is_planar = vo_describe_fourcc(priv->srcFourcc,&vd);
    src_stride=src_is_planar?priv->org_width:priv->org_width*((vd.bpp+7)/8);
    priv->ps_off[0] = priv->ps_off[1] = priv->ps_off[2] = priv->ps_off[3] = 0;
    if(!src_is_planar)
	priv->ps_off[0] = ps_y*src_stride+ps_x*((vd.bpp+7)/8);
    else
    {
	priv->ps_off[0] = ps_y*src_stride+ps_x;
	if(vd.bpp==12) /*YV12 series*/
	{
		priv->ps_off[1] = (ps_y/2)*(src_stride/2)+ps_x/2;
		priv->ps_off[2] = (ps_y/2)*(src_stride/2)+ps_x/2;
	}
	else
	if(vd.bpp==9) /*YVU9 series*/
	{
		priv->ps_off[1] = (ps_y/4)*(src_stride/4)+ps_x/4;
		priv->ps_off[2] = (ps_y/4)*(src_stride/4)+ps_x/4;
	}
    }
}

static void __FASTCALL__ dri_tune(vo_data_t*vo,unsigned width,unsigned height)
{
    vo_priv_t* priv=(vo_priv_t*)vo->vo_priv;
    priv->dri.sstride=priv->dri.is_planar?width:width*((priv->dri.bpp+7)/8);
    priv->dri.off[0] = priv->dri.off[1] = priv->dri.off[2] = priv->dri.off[3] = 0;
    if(!priv->dri.is_planar)
    {
	priv->dri.planes_eq = priv->dri.sstride == priv->dri.cap.strides[0];
	priv->dri.off[0] = priv->dri.cap.y*priv->dri.cap.strides[0]+priv->dri.cap.x*((priv->dri.bpp+7)/8);
    }
    else
    {
	unsigned long y_off,u_off,v_off;
	y_off = (unsigned long)priv->dri.surf[0].planes[0];
	u_off = (unsigned long)std::min(priv->dri.surf[0].planes[1],priv->dri.surf[0].planes[2]);
	v_off = (unsigned long)std::max(priv->dri.surf[0].planes[1],priv->dri.surf[0].planes[2]);
	priv->dri.off[0] = priv->dri.cap.y*priv->dri.cap.strides[0]+priv->dri.cap.x;
	if(priv->dri.bpp==12) /*YV12 series*/
	{
		priv->dri.planes_eq = width == priv->dri.cap.strides[0] &&
			width*height == u_off - y_off &&
			width*height*5/4 == v_off - y_off &&
			priv->dri.cap.strides[0]/2 == priv->dri.cap.strides[1] &&
			priv->dri.cap.strides[0]/2 == priv->dri.cap.strides[2];
		priv->dri.off[1] = (priv->dri.cap.y/2)*priv->dri.cap.strides[1]+priv->dri.cap.x/2;
		priv->dri.off[2] = (priv->dri.cap.y/2)*priv->dri.cap.strides[2]+priv->dri.cap.x/2;
	}
	else
	if(priv->dri.bpp==9) /*YVU9 series*/
	{
		priv->dri.planes_eq = width == priv->dri.cap.strides[0] &&
			width*height == u_off - y_off &&
			width*height*17/16 == v_off - y_off &&
			priv->dri.cap.strides[0]/4 == priv->dri.cap.strides[1] &&
			priv->dri.cap.strides[0]/4 == priv->dri.cap.strides[2];
		priv->dri.off[1] = (priv->dri.cap.y/4)*priv->dri.cap.strides[1]+priv->dri.cap.x/4;
		priv->dri.off[2] = (priv->dri.cap.y/4)*priv->dri.cap.strides[2]+priv->dri.cap.x/4;
	}
	else
	if(priv->dri.bpp==8) /*Y800 series*/
		priv->dri.planes_eq = width == priv->dri.cap.strides[0];
    }
    priv->dri.accel=(priv->dri.cap.caps&(DRI_CAP_DOWNSCALER|DRI_CAP_HORZSCALER|
			    DRI_CAP_UPSCALER|DRI_CAP_VERTSCALER))==
			    (DRI_CAP_DOWNSCALER|DRI_CAP_HORZSCALER|
			    DRI_CAP_UPSCALER|DRI_CAP_VERTSCALER);
    priv->dri.dr = priv->srcFourcc == priv->dri.cap.fourcc && !(priv->dri.flags & VOFLG_FLIP) &&
			    !priv->ps_off[0] && !priv->ps_off[1] && !priv->ps_off[2] && !priv->ps_off[3];
    if(priv->dri.dr && priv->dri.cap.w < width)
	priv->dri.dr = priv->dri.cap.caps&(DRI_CAP_DOWNSCALER|DRI_CAP_HORZSCALER)?1:0;
    if(priv->dri.dr && priv->dri.cap.w > width)
	priv->dri.dr = priv->dri.cap.caps&(DRI_CAP_UPSCALER|DRI_CAP_HORZSCALER)?1:0;
    if(priv->dri.dr && priv->dri.cap.h < height)
	priv->dri.dr = priv->dri.cap.caps&(DRI_CAP_DOWNSCALER|DRI_CAP_VERTSCALER)?1:0;
    if(priv->dri.dr && priv->dri.cap.h > height)
	priv->dri.dr = priv->dri.cap.caps&(DRI_CAP_UPSCALER|DRI_CAP_VERTSCALER)?1:0;
}

static void __FASTCALL__ dri_reconfig(vo_data_t*vo,uint32_t event )
{
    vo_priv_t* priv=(vo_priv_t*)vo->vo_priv;
	priv->dri.has_dri = 1;
	priv->video_out->control(vo,DRI_GET_SURFACE_CAPS,&priv->dri.cap);
	dri_config(vo,priv->dri.cap.fourcc);
	/* ugly workaround of swapped BGR-fourccs. Should be removed in the future */
	if(!priv->dri.has_dri)
	{
		priv->dri.has_dri=1;
		priv->dri.cap.fourcc = bswap_32(priv->dri.cap.fourcc);
		dri_config(vo,priv->dri.cap.fourcc);
	}
	dri_tune(vo,priv->image_width,priv->image_height);
	/* TODO: smart analizer of scaling possibilities of vo_driver */
	if((event & VO_EVENT_RESIZE) == VO_EVENT_RESIZE)
	{
	    xp_core->in_resize=1;
	    vf_reinit_vo(priv->dri.cap.w,priv->dri.cap.h,priv->dri.cap.fourcc,1);
	}
	vf_reinit_vo(priv->dri.cap.w,priv->dri.cap.h,priv->dri.cap.fourcc,0);
}

static int vo_inited=0;
MPXP_Rc __FASTCALL__ vo_config(vo_data_t*vo,uint32_t width, uint32_t height, uint32_t d_width,
		   uint32_t d_height, uint32_t fullscreen, char *title,
		   uint32_t format)
{
    vo_priv_t* priv=(vo_priv_t*)vo->vo_priv;
    MPXP_Rc retval;
    unsigned dest_fourcc,w,d_w,h,d_h;
    MSG_DBG3("dri_vo_dbg: vo_config\n");
    if(vo_inited) {
	MSG_FATAL("!!!priv->video_out internal fatal error: priv->video_out is initialized more than once!!!\n");
	return MPXP_False;
    }
    vo_inited++;
    dest_fourcc = format;
    priv->org_width = width;
    priv->org_height = height;

    w = width;
    d_w = d_width;
    h = height;
    d_h = d_height;

    priv->dri.d_width = d_w;
    priv->dri.d_height = d_h;
    MSG_V("priv->video_out->config(%u,%u,%u,%u,0x%x,'%s',%s)\n"
	,w,h,d_w,d_h,fullscreen,title,vo_format_name(dest_fourcc));
    retval = priv->video_out->config(vo,w,h,d_w,d_h,fullscreen,title,dest_fourcc);
    priv->srcFourcc=format;
    if(retval == MPXP_Ok) {
	int dri_retv;
	dri_retv = priv->video_out->control(vo,DRI_GET_SURFACE_CAPS,&priv->dri.cap);
	priv->image_format = format;
	priv->image_width = w;
	priv->image_height = h;
	ps_tune(vo,priv->image_width,priv->org_height);
	if(dri_retv == MPXP_True) dri_reconfig(vo,0);
	MSG_V("dri_vo_caps: driver does %s support DRI\n",priv->dri.has_dri?"":"not");
	MSG_V("dri_vo_caps: caps=%08X fourcc=%08X(%s) x,y,w,h(%u %u %u %u)\n"
	      "dri_vo_caps: width_height(%u %u) strides(%u %u %u %u) priv->dri.bpp=%u\n"
		,priv->dri.cap.caps
		,priv->dri.cap.fourcc
		,vo_format_name(priv->dri.cap.fourcc)
		,priv->dri.cap.x,priv->dri.cap.y,priv->dri.cap.w,priv->dri.cap.h
		,priv->dri.cap.width,priv->dri.cap.height
		,priv->dri.cap.strides[0],priv->dri.cap.strides[1]
		,priv->dri.cap.strides[2],priv->dri.cap.strides[3]
		,priv->dri.bpp);
	MSG_V("dri_vo_src: w,h(%u %u) d_w,d_h(%u %u)\n"
	      "dri_vo_src: flags=%08X fourcc=%08X(%s)\n"
		,width,height
		,d_width,d_height
		,fullscreen
		,format
		,vo_format_name(format));
	priv->dri.flags = fullscreen;
    }
    return retval;
}

/* if vo_driver doesn't support dri then it won't work with this logic */
uint32_t __FASTCALL__ vo_query_format(vo_data_t*vo,uint32_t* fourcc, unsigned src_w, unsigned src_h)
{
    vo_priv_t* priv=(vo_priv_t*)vo->vo_priv;
    uint32_t dri_forced_fourcc;
    MPXP_Rc retval;
    vo_query_fourcc_t qfourcc;
    MSG_DBG3("dri_vo_dbg: vo_query_format(%08lX)\n",*fourcc);
    qfourcc.fourcc = *fourcc;
    qfourcc.w = src_w;
    qfourcc.h = src_h;
    if(priv->video_out->control(vo,VOCTRL_QUERY_FORMAT,&qfourcc)==MPXP_False)
	qfourcc.flags=VOCAP_NA;
    MSG_V("dri_vo: request for %s fourcc: %i\n",vo_format_name(*fourcc),qfourcc.flags);
    dri_forced_fourcc = *fourcc;
    return qfourcc.flags;
}

MPXP_Rc vo_reset(vo_data_t*vo)
{
    vo_priv_t* priv=(vo_priv_t*)vo->vo_priv;
    MSG_DBG3("dri_vo_dbg: vo_reset\n");
    return priv->video_out->control(vo,VOCTRL_RESET,NULL);
}

MPXP_Rc vo_screenshot(vo_data_t*vo,unsigned idx )
{
    vo_priv_t* priv=(vo_priv_t*)vo->vo_priv;
    char buf[256];
    MSG_DBG3("dri_vo_dbg: vo_screenshot\n");
    sprintf(buf,"%llu",priv->frame_counter);
    return gr_screenshot(buf,const_cast<const uint8_t**>(priv->dri.surf[idx].planes),priv->dri.cap.strides,priv->dri.cap.fourcc,priv->dri.cap.width,priv->dri.cap.height);
}

MPXP_Rc vo_pause(vo_data_t*vo)
{
    vo_priv_t* priv=(vo_priv_t*)vo->vo_priv;
    MSG_DBG3("dri_vo_dbg: vo_pause\n");
    return priv->video_out->control(vo,VOCTRL_PAUSE,0);
}

MPXP_Rc vo_resume(vo_data_t*vo)
{
    vo_priv_t* priv=(vo_priv_t*)vo->vo_priv;
    MSG_DBG3("dri_vo_dbg: vo_resume\n");
    return priv->video_out->control(vo,VOCTRL_RESUME,0);
}

void vo_lock_surfaces(vo_data_t*vo) {
    vo_priv_t* priv=(vo_priv_t*)vo->vo_priv;
    pthread_mutex_lock(&priv->surfaces_mutex);
}
void vo_unlock_surfaces(vo_data_t*vo) {
    vo_priv_t* priv=(vo_priv_t*)vo->vo_priv;
    pthread_mutex_unlock(&priv->surfaces_mutex);
}

MPXP_Rc __FASTCALL__ vo_get_surface(vo_data_t*vo,mp_image_t* mpi)
{
    vo_priv_t* priv=(vo_priv_t*)vo->vo_priv;
    int width_less_stride;
    MSG_DBG2("dri_vo_dbg: vo_get_surface type=%X flg=%X\n",mpi->type,mpi->flags);
    width_less_stride = 0;
    if(mpi->flags & MP_IMGFLAG_PLANAR)
    {
	width_less_stride = mpi->w <= priv->dri.cap.strides[0] &&
			    (mpi->w>>mpi->chroma_x_shift) <= priv->dri.cap.strides[1] &&
			    (mpi->w>>mpi->chroma_x_shift) <= priv->dri.cap.strides[2];
    }
    else width_less_stride = mpi->w*mpi->bpp <= priv->dri.cap.strides[0];
    if(priv->dri.has_dri)
    {
	/* static is singlebuffered decoding */
	if(mpi->type==MP_IMGTYPE_STATIC && priv->dri.num_xp_frames>1)
	{
	    MSG_DBG2("dri_vo_dbg: vo_get_surface FAIL mpi->type==MP_IMGTYPE_STATIC && priv->dri.num_xp_frames>1\n");
	    return MPXP_False;
	}
	/*I+P requires 2+ static buffers for R/W */
	if(mpi->type==MP_IMGTYPE_IP && (priv->dri.num_xp_frames < 2 || (priv->dri.cap.caps&DRI_CAP_VIDEO_MMAPED)==DRI_CAP_VIDEO_MMAPED))
	{
	    MSG_DBG2("dri_vo_dbg: vo_get_surface FAIL (mpi->type==MP_IMGTYPE_IP && priv->dri.num_xp_frames < 2) || (priv->dri.cap.caps&DRI_CAP_VIDEO_MMAPED)==DRI_CAP_VIDEO_MMAPED\n");
	    return MPXP_False;
	}
	/*I+P+B requires 3+ static buffers for R/W */
	if(mpi->type==MP_IMGTYPE_IPB && (priv->dri.num_xp_frames != 3 || (priv->dri.cap.caps&DRI_CAP_VIDEO_MMAPED)==DRI_CAP_VIDEO_MMAPED))
	{
	    MSG_DBG2("dri_vo_dbg: vo_get_surface FAIL (mpi->type==MP_IMGTYPE_IPB && priv->dri.num_xp_frames != 3) || (priv->dri.cap.caps&DRI_CAP_VIDEO_MMAPED)==DRI_CAP_VIDEO_MMAPED\n");
	    return MPXP_False;
	}
	/* video surface is bad thing for reading */
	if(((mpi->flags&MP_IMGFLAG_READABLE)||(mpi->type==MP_IMGTYPE_TEMP)) && (priv->dri.cap.caps&DRI_CAP_VIDEO_MMAPED)==DRI_CAP_VIDEO_MMAPED)
	{
	    MSG_DBG2("dri_vo_dbg: vo_get_surface FAIL mpi->flags&MP_IMGFLAG_READABLE && (priv->dri.cap.caps&DRI_CAP_VIDEO_MMAPED)==DRI_CAP_VIDEO_MMAPED\n");
	    return MPXP_False;
	}
	/* it seems that surfaces are equal */
	if((((mpi->flags&MP_IMGFLAG_ACCEPT_STRIDE) && width_less_stride) || priv->dri.planes_eq) && priv->dri.dr)
	{
	    vo_lock_surfaces(vo);
	    mpi->planes[0]=priv->dri.surf[mpi->xp_idx].planes[0]+priv->dri.off[0];
	    mpi->planes[1]=priv->dri.surf[mpi->xp_idx].planes[1]+priv->dri.off[1];
	    mpi->planes[2]=priv->dri.surf[mpi->xp_idx].planes[2]+priv->dri.off[2];
	    mpi->stride[0]=priv->dri.cap.strides[0];
	    mpi->stride[1]=priv->dri.cap.strides[1];
	    mpi->stride[2]=priv->dri.cap.strides[2];
	    mpi->flags|=MP_IMGFLAG_DIRECT;
	    vo_unlock_surfaces(vo);
	    MSG_DBG2("dri_vo_dbg: vo_get_surface OK\n");
	    return MPXP_True;
	}
	MSG_DBG2("dri_vo_dbg: vo_get_surface FAIL (mpi->flags&MP_IMGFLAG_ACCEPT_STRIDE && width_less_stride) || priv->dri.planes_eq) && priv->dri.dr\n");
	return MPXP_False;
    }
    else return MPXP_False;
}

static int __FASTCALL__ adjust_size(any_t*vo,unsigned cw,unsigned ch,unsigned *nw,unsigned *nh)
{
    vo_priv_t* priv=(vo_priv_t*)((vo_data_t*)vo)->vo_priv;
    MSG_DBG3("dri_vo_dbg: adjust_size was called %u %u %u %u\n",cw,ch,*nw,*nh);
    if((priv->dri.flags & VOFLG_ZOOM) && (cw != *nw || ch != *nh) && !(priv->dri.flags & VOFLG_FS))
    {
	float aspect,newv;
	aspect = (float)priv->dri.d_width / (float)priv->dri.d_height;
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

int vo_check_events(vo_data_t*vo)
{
    vo_priv_t* priv=(vo_priv_t*)vo->vo_priv;
    uint32_t retval;
    int need_repaint;
    vo_resize_t vrest;
    MSG_DBG3("dri_vo_dbg: vo_check_events\n");
    vrest.event_type = 0;
    vrest.adjust_size = adjust_size;
    retval = priv->video_out->control(vo,VOCTRL_CHECK_EVENTS,&vrest);
    /* it's ok since accelerated drivers doesn't touch surfaces
       but there is only one driver (vo_x11) which changes surfaces
       on 'fullscreen' key */
    need_repaint=0;
    if(priv->dri.has_dri && retval == MPXP_True && (vrest.event_type & VO_EVENT_RESIZE) == VO_EVENT_RESIZE)
    {
	need_repaint=1;
	dri_reconfig(vo,vrest.event_type);
    }
    return (need_repaint && !priv->dri.accel) || (vrest.event_type&VO_EVENT_FORCE_UPDATE);
}

MPXP_Rc vo_fullscreen(vo_data_t*vo)
{
    vo_priv_t* priv=(vo_priv_t*)vo->vo_priv;
    uint32_t etype;
    MPXP_Rc retval;
    MSG_DBG3("dri_vo_dbg: vo_fullscreen\n");
    etype = 0;
    retval = priv->video_out->control(vo,VOCTRL_FULLSCREEN,&etype);
    if(priv->dri.has_dri && retval == MPXP_True && (etype & VO_EVENT_RESIZE) == VO_EVENT_RESIZE)
	dri_reconfig(vo,etype);
    if(retval == MPXP_True) priv->dri.flags ^= VOFLG_FS;
    return retval;
}

unsigned __FASTCALL__ vo_get_num_frames(vo_data_t*vo) {
    vo_priv_t* priv=(vo_priv_t*)vo->vo_priv;
    return priv->dri.num_xp_frames;
}

MPXP_Rc __FASTCALL__ vo_draw_slice(vo_data_t*vo,const mp_image_t *mpi)
{
    vo_priv_t* priv=(vo_priv_t*)vo->vo_priv;
    unsigned i,_w[4],_h[4],x,y;
    MSG_DBG3("dri_vo_dbg: vo_draw_slice xywh=%i %i %i %i\n",mpi->x,mpi->y,mpi->w,mpi->h);
    if(priv->dri.has_dri) {
	uint8_t *dst[4];
	const uint8_t *ps_src[4];
	int dstStride[4];
	int finalize=vo_is_final(vo);
	unsigned idx = mpi->xp_idx;
	for(i=0;i<4;i++) {
	    dst[i]=priv->dri.surf[idx].planes[i]+priv->dri.off[i];
	    dstStride[i]=priv->dri.cap.strides[i];
	    dst[i]+=((mpi->y*dstStride[i])*priv->vod.y_mul[i])/priv->vod.y_div[i];
	    dst[i]+=(mpi->x*priv->vod.x_mul[i])/priv->vod.x_div[i];
	    _w[i]=(mpi->w*priv->vod.x_mul[i])/priv->vod.x_div[i];
	    _h[i]=(mpi->h*priv->vod.y_mul[i])/priv->vod.y_div[i];
	    y = i?(mpi->y>>mpi->chroma_y_shift):mpi->y;
	    x = i?(mpi->x>>mpi->chroma_x_shift):mpi->x;
	    ps_src[i] = mpi->planes[i]+(y*mpi->stride[i])+x+priv->ps_off[i];
	}
	for(i=0;i<4;i++) {
	    if(mpi->stride[i]) {
		if(finalize)
		    stream_copy_pic(dst[i],ps_src[i],_w[i],_h[i],dstStride[i],mpi->stride[i]);
		else
		    memcpy_pic(dst[i],ps_src[i],_w[i],_h[i],dstStride[i],mpi->stride[i]);
	    }
	}
	return MPXP_Ok;
    }
    return MPXP_False;
}

void vo_select_frame(vo_data_t*vo,unsigned play_idx)
{
    vo_priv_t* priv=(vo_priv_t*)vo->vo_priv;
    MSG_DBG2("dri_vo_dbg: vo_select_frame(play_idx=%u)\n",play_idx);
    priv->video_out->select_frame(vo,play_idx);
}

void vo_flush_page(vo_data_t*vo,unsigned decoder_idx)
{
    vo_priv_t* priv=(vo_priv_t*)vo->vo_priv;
    MSG_DBG3("dri_vo_dbg: vo_flush_pages [idx=%u]\n",decoder_idx);
    priv->frame_counter++;
    if((priv->dri.cap.caps & DRI_CAP_VIDEO_MMAPED)!=DRI_CAP_VIDEO_MMAPED)
					priv->video_out->control(vo,VOCTRL_FLUSH_PAGES,&decoder_idx);
}

/* DRAW OSD */
static void __FASTCALL__ clear_rect(vo_data_t*vo,unsigned _y0,unsigned h,uint8_t *dest,unsigned stride,unsigned dstride,uint8_t filler)
{
  vo_priv_t* priv=(vo_priv_t*)vo->vo_priv;
  unsigned i;
  for(i=0;i<h;i++)
  {
      if(_y0+i<priv->dri.cap.y||_y0+i>=priv->dri.cap.y+priv->dri.cap.h) memset(dest,filler,stride);
      dest += dstride;
  }
}

static void __FASTCALL__ clear_rect2(vo_data_t*vo,unsigned _y0,unsigned h,uint8_t *dest,unsigned stride,unsigned dstride,uint8_t filler)
{
  vo_priv_t* priv=(vo_priv_t*)vo->vo_priv;
  unsigned i;
  unsigned _y1 = priv->dri.cap.y/2;
  unsigned _y2 = (priv->dri.cap.y+priv->dri.cap.h)/2;
  for(i=0;i<h;i++)
  {
      if(_y0+i<_y1||_y0+i>=_y2) memset(dest,filler,stride);
      dest += dstride;
  }
}

static void __FASTCALL__ clear_rect4(vo_data_t*vo,unsigned _y0,unsigned h,uint8_t *dest,unsigned stride,unsigned dstride,uint8_t filler)
{
  vo_priv_t* priv=(vo_priv_t*)vo->vo_priv;
  unsigned i;
  unsigned _y1 = priv->dri.cap.y/4;
  unsigned _y2 = (priv->dri.cap.y+priv->dri.cap.h)/4;
  for(i=0;i<h;i++)
  {
      if(_y0+i<_y1||_y0+i>=_y2) memset(dest,filler,stride);
      dest += dstride;
  }
}

static void __FASTCALL__ clear_rect_rgb(vo_data_t*vo,unsigned _y0,unsigned h,uint8_t *dest,unsigned stride,unsigned dstride)
{
  vo_priv_t* priv=(vo_priv_t*)vo->vo_priv;
  unsigned i;
  for(i=0;i<h;i++)
  {
      if(_y0+i<priv->dri.cap.y||_y0+i>=priv->dri.cap.y+priv->dri.cap.h) memset(dest,0,stride);
      dest += dstride;
  }
}

static void __FASTCALL__ clear_rect_yuy2(vo_data_t*vo,unsigned _y0,unsigned h,uint8_t *dest,unsigned stride,unsigned dstride)
{
  vo_priv_t* priv=(vo_priv_t*)vo->vo_priv;
  unsigned i;
  for(i=0;i<h;i++)
  {
	if(_y0+i<priv->dri.cap.y||_y0+i>=priv->dri.cap.y+priv->dri.cap.h)
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

static void __FASTCALL__ dri_remove_osd(any_t*_vo,unsigned idx,int x0,int _y0, int w,int h)
{
    vo_data_t* vo=reinterpret_cast<vo_data_t*>(_vo);
    vo_priv_t* priv=reinterpret_cast<vo_priv_t*>(vo->vo_priv);
    if(x0+w<=priv->dri.cap.width&&_y0+h<=priv->dri.cap.height)
    switch(priv->dri.cap.fourcc)
    {
	case IMGFMT_RGB15:
	case IMGFMT_BGR15:
	case IMGFMT_RGB16:
	case IMGFMT_BGR16:
	case IMGFMT_RGB24:
	case IMGFMT_BGR24:
	case IMGFMT_RGB32:
	case IMGFMT_BGR32:
		clear_rect_rgb(vo,_y0,h,priv->dri.surf[idx].planes[0]+_y0*priv->dri.cap.strides[0]+x0*((priv->dri.bpp+7)/8),
			    w*(priv->dri.bpp+7)/8,priv->dri.cap.strides[0]);
		break;
	case IMGFMT_YVYU:
	case IMGFMT_YUY2:
		clear_rect_yuy2(vo,_y0,h,priv->dri.surf[idx].planes[0]+_y0*priv->dri.cap.strides[0]+x0*2,
			    w*2,priv->dri.cap.strides[0]);
		break;
	case IMGFMT_UYVY:
		clear_rect_yuy2(vo,_y0,h,priv->dri.surf[idx].planes[0]+_y0*priv->dri.cap.strides[0]+x0*2+1,
			    w*2,priv->dri.cap.strides[0]);
		break;
	case IMGFMT_Y800:
		clear_rect(vo,_y0,h,priv->dri.surf[idx].planes[0]+_y0*priv->dri.cap.strides[0]+x0,
			    w,priv->dri.cap.strides[0],0x10);
		break;
	case IMGFMT_YV12:
	case IMGFMT_I420:
	case IMGFMT_IYUV:
		clear_rect(vo,_y0,h,priv->dri.surf[idx].planes[0]+_y0*priv->dri.cap.strides[0]+x0,
			    w,priv->dri.cap.strides[0],0x10);
		clear_rect2(vo,_y0/2,h/2,priv->dri.surf[idx].planes[1]+_y0/2*priv->dri.cap.strides[1]+x0/2,
			    w/2,priv->dri.cap.strides[1],0x80);
		clear_rect2(vo,_y0/2,h/2,priv->dri.surf[idx].planes[2]+_y0/2*priv->dri.cap.strides[2]+x0/2,
			    w/2,priv->dri.cap.strides[2],0x80);
		break;
	case IMGFMT_YVU9:
	case IMGFMT_IF09:
		clear_rect(vo,_y0,h,priv->dri.surf[idx].planes[0]+_y0*priv->dri.cap.strides[0]+x0,
			    w,priv->dri.cap.strides[0],0x10);
		clear_rect4(vo,_y0/4,h/4,priv->dri.surf[idx].planes[1]+_y0/4*priv->dri.cap.strides[1]+x0/4,
			    w/4,priv->dri.cap.strides[1],0x80);
		clear_rect4(vo,_y0/4,h/4,priv->dri.surf[idx].planes[2]+_y0/4*priv->dri.cap.strides[2]+x0/4,
			    w/4,priv->dri.cap.strides[2],0x80);
		break;
    }
}

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

static void __FASTCALL__ dri_draw_osd(any_t*vo,unsigned idx,int x0,int _y0, int w,int h,const unsigned char* src,const unsigned char *srca, int stride)
{
    vo_priv_t* priv=reinterpret_cast<vo_priv_t*>(reinterpret_cast<vo_data_t*>(vo)->vo_priv);
    int finalize=vo_is_final(reinterpret_cast<vo_data_t*>(vo));
    if(x0+w<=priv->dri.cap.width&&_y0+h<=priv->dri.cap.height)
    {
	if(!priv->draw_alpha) priv->draw_alpha=get_draw_alpha(priv->dri.cap.fourcc);
	if(priv->draw_alpha)
	    (*priv->draw_alpha)(w,h,src,srca,stride,
			    priv->dri.surf[idx].planes[0]+priv->dri.cap.strides[0]*_y0+x0*((priv->dri.bpp+7)/8),
			    priv->dri.cap.strides[0],finalize);
    }
}

void vo_draw_osd(vo_data_t*vo,unsigned idx)
{
    vo_priv_t* priv=(vo_priv_t*)vo->vo_priv;
    MSG_DBG3("dri_vo_dbg: vo_draw_osd\n");
    if(priv->dri.has_dri && !(priv->dri.cap.caps & DRI_CAP_HWOSD))
    {
	if( priv->dri.cap.x || priv->dri.cap.y ||
	    priv->dri.cap.w != priv->dri.cap.width || priv->dri.cap.h != priv->dri.cap.height)
		    vo_remove_text(vo,idx,priv->dri.cap.width,priv->dri.cap.height,dri_remove_osd);
	vo_draw_text(vo,idx,priv->dri.cap.width,priv->dri.cap.height,dri_draw_osd);
    }
}

void vo_draw_spudec_direct(vo_data_t*vo,unsigned idx)
{
    vo_priv_t* priv=(vo_priv_t*)vo->vo_priv;
    MSG_DBG3("dri_vo_dbg: vo_draw_osd\n");
    if(priv->dri.has_dri && !(priv->dri.cap.caps & DRI_CAP_HWOSD))
    {
//	if( priv->dri.cap.x || priv->dri.cap.y ||
//	    priv->dri.cap.w != priv->dri.cap.width || priv->dri.cap.h != priv->dri.cap.height)
//		    vo_remove_text(idx,priv->dri.cap.width,priv->dri.cap.height,dri_remove_osd);
	vo_draw_spudec(vo,idx,priv->dri.cap.width,priv->dri.cap.height,dri_draw_osd);
    }
}

void vo_uninit(vo_data_t*vo)
{
    vo_priv_t* priv=(vo_priv_t*)vo->vo_priv;
    MSG_DBG3("dri_vo_dbg: vo_uninit\n");
    vo_inited--;
    priv->video_out->uninit(vo);
    pthread_mutex_destroy(&priv->surfaces_mutex);
    delete priv;
}

MPXP_Rc __FASTCALL__ vo_control(vo_data_t*vo,uint32_t request, any_t*data)
{
    MPXP_Rc rval;
    vo_priv_t* priv=(vo_priv_t*)vo->vo_priv;
    rval=priv->video_out->control(vo,request,data);
    MSG_DBG3("dri_vo_dbg: %u=vo_control( %u, %p )\n",rval,request,data);
    return rval;
}

int __FASTCALL__ vo_is_final(vo_data_t*vo) {
    vo_priv_t* priv=(vo_priv_t*)vo->vo_priv;
    int mmaped=priv->dri.cap.caps&DRI_CAP_VIDEO_MMAPED;
    int busmaster=priv->dri.cap.caps&DRI_CAP_BUSMASTERING;
    return mmaped||busmaster||(priv->dri.num_xp_frames>1);
}

