#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
/*
 * vo_xv.c, X11 Xv interface
 *
 * Copyright (C) 1996, MPEG Software Simulation Group. All Rights Reserved.
 *
 * Hacked into mpeg2dec by
 *
 * Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *
 * Xv image suuport by Gerd Knorr <kraxel@goldbach.in-berlin.de>
 * fullscreen support by Pontscho
 * double buffering support by A'rpi
 */
#include <algorithm>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mplayerxp.h"
#include "xmpcore/xmp_core.h"
#include "video_out.h"
#include "video_out_internal.h"

LIBVO_EXTERN(xv)

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <errno.h>

#include "x11_system.h"

#include "osdep/fastmemcpy.h"
#include "sub.h"
#include "aspect.h"
#include "dri_vo.h"
#include "xmpcore/mp_image.h"

#include "vo_msg.h"

static vo_info_t vo_info =
{
	"X11/Xv",
	"xv",
	"Gerd Knorr <kraxel@goldbach.in-berlin.de>",
	""
};

/* since it doesn't seem to be defined on some platforms */
int XShmGetEventBase(Display*);
// FIXME: dynamically allocate this stuff
static void __FASTCALL__ allocate_xvimage(vo_data_t*,int);

/* Xv related variables */
struct xv_priv_t : public video_private {
    xv_priv_t();
    virtual ~xv_priv_t() {}

    uint32_t		image_width;
    uint32_t		image_height;
    uint32_t		image_format;
    unsigned		depth;

    unsigned int	formats, adaptors, port, format, bpp;

    unsigned		expose_idx,num_buffers; // 1 - default
    unsigned		dwidth,dheight;

    Xv_System*		xv;
};

xv_priv_t::xv_priv_t() {
    num_buffers=1;
}

static void set_gamma_correction( vo_data_t*vo )
{
    xv_priv_t& priv = *static_cast<xv_priv_t*>(vo->priv);
    Xv_System& xv = *priv.xv;
    vo_videq_t info;
    /* try all */
    xv.reset_video_eq();
    info.name=VO_EC_BRIGHTNESS;
    info.value=vo_conf.gamma.brightness;
    xv.set_video_eq(&info);
    info.name=VO_EC_CONTRAST;
    info.value=vo_conf.gamma.contrast;
    xv.set_video_eq(&info);
    info.name=VO_EC_SATURATION;
    info.value=vo_conf.gamma.saturation;
    xv.set_video_eq(&info);
    info.name=VO_EC_HUE;
    info.value=vo_conf.gamma.hue;
    xv.set_video_eq(&info);
    info.name=VO_EC_RED_INTENSITY;
    info.value=vo_conf.gamma.red_intensity;
    xv.set_video_eq(&info);
    info.name=VO_EC_GREEN_INTENSITY;
    info.value=vo_conf.gamma.green_intensity;
    xv.set_video_eq(&info);
    info.name=VO_EC_BLUE_INTENSITY;
    info.value=vo_conf.gamma.blue_intensity;
    xv.set_video_eq(&info);
}

/* unofficial gatos extensions */
#define FOURCC_RGBA32	0x41424752
#define FOURCC_RGB24	0x00000000
#define FOURCC_RGBT16	0x54424752
#define FOURCC_RGB16	0x32424752

/*
 * connect to server, create and map window,
 * allocate colors and (shared) memory
 */
static MPXP_Rc __FASTCALL__ config_vo(vo_data_t*vo,uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height,const char *title, uint32_t format)
{
    xv_priv_t& priv = *static_cast<xv_priv_t*>(vo->priv);
    Xv_System& xv = *priv.xv;

    XVisualInfo vinfo;
    XSizeHints hint;

    unsigned i;

    aspect_save_orig(width,height);
    aspect_save_prescale(d_width,d_height);

    priv.image_height = height;
    priv.image_width = width;
    priv.image_format=format;

    priv.num_buffers=vo_conf.xp_buffs;

    priv.depth=xv.depth();
    if ( priv.depth != 15 && priv.depth != 16 && priv.depth != 24 && priv.depth != 32 )
	priv.depth=24;
    xv.match_visual( &vinfo );

    aspect_save_screenres(vo_conf.screenwidth,vo_conf.screenheight);
    aspect(&d_width,&d_height,vo_ZOOM(vo)?A_ZOOM:A_NOZOOM);

    xv.calcpos(&hint,d_width,d_height,vo->flags);
    hint.flags = PPosition | PSize;

    priv.dwidth=d_width; priv.dheight=d_height; //XXX: what are the copy vars used for?

    xv.create_window(hint,&vinfo,vo_VM(vo),priv.depth,title);
    xv.classhint("vo_x11");
    xv.hidecursor();
    if ( vo_FS(vo) ) xv.decoration(0);

    /* we cannot grab mouse events on root window :( */
    xv.select_input(StructureNotifyMask | KeyPressMask |
		    ButtonPressMask | ButtonReleaseMask | PointerMotionMask);

    priv.format = xv.query_port(format);
    if(priv.format) {
	switch (priv.format){
	    case IMGFMT_IF09:
	    case IMGFMT_YVU9:
		priv.bpp=9;
		break;
	    case IMGFMT_YV12:
	    case IMGFMT_I420:
	    case IMGFMT_IYUV:
		priv.bpp=12;
		break;
	    case IMGFMT_YUY2:
	    case IMGFMT_YVYU:
		priv.bpp=16;
		break;
	    case IMGFMT_UYVY:
		priv.bpp=16;
		break;
	    case IMGFMT_RGB32:
		priv.bpp=32;
		break;
	    case IMGFMT_RGB16:
		priv.bpp=16;
		break;
	    default:
		priv.bpp = 16;
	}

	for(i=0;i<priv.num_buffers;++i) allocate_xvimage(vo,i);

	set_gamma_correction(vo);

	aspect(&priv.dwidth,&priv.dheight,vo_ZOOM(vo)?A_ZOOM:A_NOZOOM);
	return MPXP_Ok;
    }

    MSG_FATAL("Sorry, Xv not supported by this X11 version/driver\n");
    MSG_FATAL("******** Try with  -vo x11  or  -vo sdl  *********\n");
    return MPXP_False;
}

static const vo_info_t * get_info(const vo_data_t*vo)
{
    UNUSED(vo);
    return &vo_info;
}

static void __FASTCALL__ allocate_xvimage(vo_data_t*vo,int idx)
{
    xv_priv_t& priv = *static_cast<xv_priv_t*>(vo->priv);
    Xv_System& xv = *priv.xv;
    xv.getMyXImage(idx,NULL,priv.format,priv.image_width,priv.image_height);
    return;
}

static void __FASTCALL__ deallocate_xvimage(vo_data_t*vo,int idx)
{
    xv_priv_t& priv = *static_cast<xv_priv_t*>(vo->priv);
    Xv_System& xv = *priv.xv;
    xv.freeMyXImage(idx);
    xv.flush();
    xv.sync(False);
    return;
}

static uint32_t __FASTCALL__ check_events(vo_data_t*vo,vo_adjust_size_t adjust_size)
{
    xv_priv_t& priv = *static_cast<xv_priv_t*>(vo->priv);
    Xv_System& xv = *priv.xv;
    uint32_t e=xv.check_events(adjust_size,vo);
    if(e&VO_EVENT_RESIZE) {
	vo_rect_t winc;
	xv.get_win_coord(&winc);
	MSG_V( "[xv-resize] dx: %d dy: %d dw: %d dh: %d\n",
		winc.x,winc.y,winc.w,winc.h);

	aspect(&priv.dwidth,&priv.dheight,vo_ZOOM(vo)?A_ZOOM:A_NOZOOM);
    }
    if ( e & VO_EVENT_EXPOSE ) {
	vo_rect_t r,r2;
	xv.get_win_coord(&r);
	r2=r;
	r.w=r.h=1;
	xv.put_image(xv.ImageXv(priv.expose_idx),r);
	if(vo_FS(vo)) r2.h--;
	xv.put_image(xv.ImageXv(priv.expose_idx),r2);
    }
    return e|VO_EVENT_FORCE_UPDATE;
}

static void __FASTCALL__ select_frame(vo_data_t*vo,unsigned idx)
{
    xv_priv_t& priv = *static_cast<xv_priv_t*>(vo->priv);
    Xv_System& xv = *priv.xv;
    vo_rect_t r;
    xv.get_win_coord(&r);
    if(vo_FS(vo)) r.h--;
    xv.put_image(xv.ImageXv(idx),r);
    priv.expose_idx=idx;
    if (priv.num_buffers>1) xv.flush();
    else xv.sync(False);
    return;
}

static MPXP_Rc __FASTCALL__ query_format(vo_data_t*vo,vo_query_fourcc_t* format)
{
    xv_priv_t& priv = *static_cast<xv_priv_t*>(vo->priv);
    Xv_System& xv = *priv.xv;
    priv.format=xv.query_port(format->fourcc);
    if(priv.format) { format->flags=VOCAP_SUPPORTED|VOCAP_HWSCALER; return MPXP_Ok; }
    return MPXP_False;
}

static void uninit(vo_data_t*vo)
{
    xv_priv_t& priv = *static_cast<xv_priv_t*>(vo->priv);
    Xv_System& xv = *priv.xv;
    unsigned i;
    xv.saver_on(); // screen saver back on
    for( i=0;i<priv.num_buffers;i++ ) deallocate_xvimage(vo,i);
#ifdef HAVE_XF86VM
    xv.vm_close();
#endif
    delete &xv;
    delete vo->priv;
}

static MPXP_Rc __FASTCALL__ preinit(vo_data_t*vo,const char *arg)
{
    xv_priv_t*priv;
    priv=new(zeromem) xv_priv_t;
    vo->priv=priv;
    if(arg) {
	MSG_ERR("vo_xv: Unknown subdevice: %s\n",arg);
	return MPXP_False;
    }
    priv->xv=new(zeromem) Xv_System(vo_conf.mDisplayName);
    priv->xv->saver_off();
    return MPXP_Ok;
}

static void __FASTCALL__ xv_dri_get_surface_caps(vo_data_t*vo,dri_surface_cap_t *caps)
{
    xv_priv_t& priv = *static_cast<xv_priv_t*>(vo->priv);
    Xv_System& xv = *priv.xv;
    unsigned i,n;
    caps->caps = DRI_CAP_TEMP_VIDEO | DRI_CAP_UPSCALER | DRI_CAP_DOWNSCALER |
		DRI_CAP_HORZSCALER | DRI_CAP_VERTSCALER;
    caps->fourcc = priv.image_format;
    caps->width=priv.image_width;
    caps->height=priv.image_height;
    caps->x=0;
    caps->y=0;
    caps->w=priv.image_width;
    caps->h=priv.image_height;
    n=std::min(4,xv.ImageXv(0)->num_planes);
    for(i=0;i<n;i++)
	caps->strides[i] = xv.ImageXv(0)->pitches[i];
    unsigned ts;
    ts = caps->strides[2];
    caps->strides[2] = caps->strides[1];
    caps->strides[1] = ts;
}

static void __FASTCALL__ xv_dri_get_surface(vo_data_t*vo,dri_surface_t *surf)
{
    xv_priv_t& priv = *static_cast<xv_priv_t*>(vo->priv);
    Xv_System& xv = *priv.xv;
    unsigned i,n;
    n=std::min(4,xv.ImageXv(0)->num_planes);
    for(i=0;i<n;i++)
	surf->planes[i] = xv.ImageData(surf->idx) + xv.ImageXv(surf->idx)->offsets[i];
    for(;i<4;i++)
	surf->planes[i] = 0;
    {
	uint8_t* tp;
	tp = surf->planes[2];
	surf->planes[2] = surf->planes[1];
	surf->planes[1] = tp;
    }
}

static MPXP_Rc __FASTCALL__ control_vo(vo_data_t*vo,uint32_t request, any_t*data)
{
    xv_priv_t& priv = *static_cast<xv_priv_t*>(vo->priv);
    Xv_System& xv = *priv.xv;
  switch (request) {
  case VOCTRL_QUERY_FORMAT:
    return query_format(vo,(vo_query_fourcc_t*)data);
  case VOCTRL_FULLSCREEN:
    if(xv.fullscreen()) vo_FS_SET(vo);
    else		vo_FS_UNSET(vo);
    return MPXP_True;
  case VOCTRL_CHECK_EVENTS:
    {
     vo_resize_t * vrest = (vo_resize_t *)data;
     vrest->event_type = check_events(vo,vrest->adjust_size);
     return MPXP_True;
    }
  case VOCTRL_GET_NUM_FRAMES:
	*(uint32_t *)data = priv.num_buffers;
	return MPXP_True;
  case DRI_GET_SURFACE_CAPS:
	xv_dri_get_surface_caps(vo,reinterpret_cast<dri_surface_cap_t*>(data));
	return MPXP_True;
  case DRI_GET_SURFACE:
	xv_dri_get_surface(vo,reinterpret_cast<dri_surface_t*>(data));
	return MPXP_True;
  case VOCTRL_SET_EQUALIZER:
	if(!xv.set_video_eq(reinterpret_cast<vo_videq_t*>(data))) return MPXP_True;
	return MPXP_False;
  case VOCTRL_GET_EQUALIZER:
	if(xv.get_video_eq(reinterpret_cast<vo_videq_t*>(data))) return MPXP_True;
	return MPXP_False;
  }
  return MPXP_NA;
}
