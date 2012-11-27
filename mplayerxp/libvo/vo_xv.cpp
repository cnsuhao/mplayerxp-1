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


class Xv_VO_Interface : public VO_Interface {
    public:
	Xv_VO_Interface(const char* args);
	virtual ~Xv_VO_Interface();

	virtual MPXP_Rc	configure(uint32_t width,
				uint32_t height,
				uint32_t d_width,
				uint32_t d_height,
				unsigned flags,
				const char *title,
				uint32_t format);
	virtual MPXP_Rc	select_frame(unsigned idx);
	virtual void	get_surface_caps(dri_surface_cap_t *caps) const;
	virtual void	get_surface(dri_surface_t *surf);
	virtual MPXP_Rc	query_format(vo_query_fourcc_t* format) const;
	virtual unsigned get_num_frames() const;

	virtual MPXP_Rc	toggle_fullscreen();
	virtual uint32_t check_events(const vo_resize_t*);
	virtual MPXP_Rc	ctrl(uint32_t request, any_t*data);
    private:
	void		allocate_xvimage(int idx) const;
	void		deallocate_xvimage(int idx) const;
	void		set_gamma_correction( ) const;

	uint32_t	image_width;
	uint32_t	image_height;
	uint32_t	image_format;
	unsigned	depth,flags;

	unsigned int	formats, adaptors, port, format, bpp;

	unsigned	expose_idx,num_buffers; // 1 - default
	unsigned	dwidth,dheight;

	Xv_System&	xv;
};

/* since it doesn't seem to be defined on some platforms */
int XShmGetEventBase(Display*);
// FIXME: dynamically allocate this stuff

Xv_VO_Interface::Xv_VO_Interface(const char *arg)
		:VO_Interface(arg),
		xv(*new(zeromem) Xv_System(vo_conf.mDisplayName))
{
    num_buffers=1;
    if(arg) {
	MSG_ERR("vo_xv: Unknown subdevice: %s\n",arg);
	exit_player("Xv error");
    }
}

Xv_VO_Interface::~Xv_VO_Interface()
{
    unsigned i;
    xv.saver_on(); // screen saver back on
    for(i=0;i<num_buffers;i++ ) deallocate_xvimage(i);
#ifdef HAVE_XF86VM
    xv.vm_close();
#endif
}

void Xv_VO_Interface::set_gamma_correction( ) const
{
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
MPXP_Rc Xv_VO_Interface::configure(uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height,unsigned _flags,const char *title, uint32_t _format)
{
    XVisualInfo vinfo;
    XSizeHints hint;
    unsigned i;

    flags=_flags;

    aspect_save_orig(width,height);
    aspect_save_prescale(d_width,d_height);

    image_height = height;
    image_width = width;
    image_format=_format;

    num_buffers=vo_conf.xp_buffs;

    depth=xv.depth();
    if ( depth != 15 && depth != 16 && depth != 24 && depth != 32 )
	depth=24;
    xv.match_visual( &vinfo );

    aspect_save_screenres(vo_conf.screenwidth,vo_conf.screenheight);
    aspect(&d_width,&d_height,flags&VOFLAG_FULLSCREEN?A_ZOOM:A_NOZOOM);

    xv.calcpos(&hint,d_width,d_height,flags);
    hint.flags = PPosition | PSize;

    dwidth=d_width; dheight=d_height; //XXX: what are the copy vars used for?

    xv.create_window(hint,&vinfo,flags&VOFLAG_MODESWITCHING,depth,title);
    xv.classhint("vo_x11");
    xv.hidecursor();
    if ( flags&VOFLAG_FULLSCREEN ) xv.decoration(0);

    /* we cannot grab mouse events on root window :( */
    xv.select_input(StructureNotifyMask | KeyPressMask |
		    ButtonPressMask | ButtonReleaseMask | PointerMotionMask);

    format = xv.query_port(_format);
    if(format) {
	switch (format){
	    case IMGFMT_IF09:
	    case IMGFMT_YVU9:
		bpp=9;
		break;
	    case IMGFMT_YV12:
	    case IMGFMT_I420:
	    case IMGFMT_IYUV:
		bpp=12;
		break;
	    case IMGFMT_YUY2:
	    case IMGFMT_YVYU:
		bpp=16;
		break;
	    case IMGFMT_UYVY:
		bpp=16;
		break;
	    case IMGFMT_RGB32:
		bpp=32;
		break;
	    case IMGFMT_RGB16:
		bpp=16;
		break;
	    default:
		bpp = 16;
	}

	for(i=0;i<num_buffers;++i) allocate_xvimage(i);

	set_gamma_correction();

	aspect(&dwidth,&dheight,flags&VOFLAG_FULLSCREEN?A_ZOOM:A_NOZOOM);
	return MPXP_Ok;
    }

    MSG_FATAL("Sorry, Xv not supported by this X11 version/driver\n");
    MSG_FATAL("******** Try with  -vo x11  or  -vo sdl  *********\n");
    return MPXP_False;
}

void Xv_VO_Interface::allocate_xvimage(int idx) const
{
    xv.getMyXImage(idx,NULL,format,image_width,image_height);
    return;
}

void Xv_VO_Interface::deallocate_xvimage(int idx) const
{
    xv.freeMyXImage(idx);
    xv.flush();
    xv.sync(False);
    return;
}

uint32_t Xv_VO_Interface::check_events(const vo_resize_t*vrest)
{
    uint32_t e=xv.check_events(vrest->adjust_size,vrest->vo);
    if(e&VO_EVENT_RESIZE) {
	vo_rect_t winc;
	xv.get_win_coord(winc);
	MSG_V( "[xv-resize] dx: %d dy: %d dw: %d dh: %d\n",
		winc.x,winc.y,winc.w,winc.h);

	aspect(&dwidth,&dheight,flags&VOFLAG_FULLSCREEN?A_ZOOM:A_NOZOOM);
    }
    if ( e & VO_EVENT_EXPOSE ) {
	vo_rect_t r,r2;
	xv.get_win_coord(r);
	r2=r;
	r.w=r.h=1;
	xv.put_image(xv.ImageXv(expose_idx),r);
	if(flags&VOFLAG_FULLSCREEN) r2.h--;
	xv.put_image(xv.ImageXv(expose_idx),r2);
    }
    return e|VO_EVENT_FORCE_UPDATE;
}

MPXP_Rc Xv_VO_Interface::select_frame(unsigned idx)
{
    vo_rect_t r;
    xv.get_win_coord(r);
    if(flags&VOFLAG_FULLSCREEN) r.h--;
    xv.put_image(xv.ImageXv(idx),r);
    expose_idx=idx;
    if (num_buffers>1) xv.flush();
    else xv.sync(False);
    return MPXP_Ok;
}

MPXP_Rc Xv_VO_Interface::query_format(vo_query_fourcc_t* _format) const
{
    if(xv.query_port(_format->fourcc)) {
	_format->flags=VOCAP_SUPPORTED|VOCAP_HWSCALER;
	return MPXP_Ok;
    }
    return MPXP_False;
}

void Xv_VO_Interface::get_surface_caps(dri_surface_cap_t *caps) const
{
    unsigned i,n;
    caps->caps = DRI_CAP_TEMP_VIDEO | DRI_CAP_UPSCALER | DRI_CAP_DOWNSCALER |
		DRI_CAP_HORZSCALER | DRI_CAP_VERTSCALER;
    caps->fourcc = image_format;
    caps->width=image_width;
    caps->height=image_height;
    caps->x=0;
    caps->y=0;
    caps->w=image_width;
    caps->h=image_height;
    n=std::min(4,xv.ImageXv(0)->num_planes);
    for(i=0;i<n;i++)
	caps->strides[i] = xv.ImageXv(0)->pitches[i];
    unsigned ts;
    ts = caps->strides[2];
    caps->strides[2] = caps->strides[1];
    caps->strides[1] = ts;
}

void Xv_VO_Interface::get_surface(dri_surface_t *surf)
{
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

unsigned Xv_VO_Interface::get_num_frames() const { return num_buffers; }

MPXP_Rc Xv_VO_Interface::toggle_fullscreen() {
    xv.fullscreen(flags);
    return MPXP_True;
}

MPXP_Rc Xv_VO_Interface::ctrl(uint32_t request, any_t*data)
{
  switch (request) {
    case VOCTRL_SET_EQUALIZER:
	if(!xv.set_video_eq(reinterpret_cast<vo_videq_t*>(data))) return MPXP_True;
	return MPXP_False;
    case VOCTRL_GET_EQUALIZER:
	if(xv.get_video_eq(reinterpret_cast<vo_videq_t*>(data))) return MPXP_True;
	return MPXP_False;
  }
  return MPXP_NA;
}

static VO_Interface* query_interface(const char* args) { return new(zeromem) Xv_VO_Interface(args); }
extern const vo_info_t xv_vo_info =
{
	"X11/Xv",
	"xv",
	"Gerd Knorr <kraxel@goldbach.in-berlin.de>",
	"",
	query_interface
};
