#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;

/*
 * video_out_x11.c,X11 interface
 *
 *
 * Copyright ( C ) 1996,MPEG Software Simulation Group. All Rights Reserved.
 *
 * Hacked into mpeg2dec by
 *
 * Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *
 * 15 & 16 bpp support added by Franck Sicard <Franck.Sicard@solsoft.fr>
 * use swScaler instead of lots of tricky converters by Michael Niedermayer <michaelni@gmx.at>
 * runtime fullscreen switching by alex
 *
 */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

#include "mplayerxp.h"
#include "xmpcore/xmp_core.h"
#include "aspect.h"
#include "video_out.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#ifdef HAVE_XF86VM
#include <X11/extensions/xf86vmode.h>
#endif

#include "x11_system.h"

#include "osdep/fastmemcpy.h"
#include "sub.h"

#include "postproc/swscale.h" /* for MODE_RGB(BGR) definitions */
#include "video_out_internal.h"
#include "video_out.h"
#ifdef CONFIG_VIDIX
#include "vosub_vidix.h"
#include <vidix/vidix.h>
#endif
#include "dri_vo.h"
#include "xmpcore/mp_image.h"
#include "vo_msg.h"

class X11_VO_Interface : public VO_Interface {
    public:
	X11_VO_Interface(const char* args);
	virtual ~X11_VO_Interface();

	virtual MPXP_Rc	configure(uint32_t width,
				uint32_t height,
				uint32_t d_width,
				uint32_t d_height,
				unsigned flags,
				const char *title,
				uint32_t format);
	virtual void	select_frame(unsigned idx);
	virtual MPXP_Rc	ctrl(uint32_t request, any_t*data);
    private:
	uint32_t	parse_sub_device(const char *sd);
	void		resize(int x,int y) const;
	uint32_t	check_events(const vo_resize_t*);
	void		display_image(XImage * myximage) const;

	void		dri_get_surface_caps(dri_surface_cap_t *caps) const;
	void		dri_get_surface(dri_surface_t *surf) const;
	MPXP_Rc		query_format(vo_query_fourcc_t* format) const;

	uint32_t	image_width;
	uint32_t	image_height;
	uint32_t	in_format;
	uint32_t	out_format;

	unsigned	depth,bpp,mode,flags;

	XVisualInfo	vinfo;

	int		baseAspect; // 1<<16 based fixed point aspect, so that the aspect stays correct during resizing
/* xp related variables */
	unsigned	num_buffers; // 1 - default
#ifdef CONFIG_VIDIX
	const char *	vidix_name;
	vidix_server_t*	vidix_server;
	vidix_priv_t*	vidix;
	void		resize_vidix() const;
#endif
	uint32_t	subdev_flags;
	X11_System&	x11;
};

#ifdef HAVE_SHM
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#endif

uint32_t X11_VO_Interface::parse_sub_device(const char *sd)
{
    uint32_t _flags;
    _flags = 0;
#ifdef CONFIG_VIDIX
    if(memcmp(sd,"vidix",5) == 0) vidix_name = &sd[5]; /* priv.vidix_name will be valid within init() */
    else
#endif
    { MSG_ERR("vo_x11: Unknown subdevice: '%s'\n", sd); return 0xFFFFFFFFUL; }
    return _flags;
}

X11_VO_Interface::X11_VO_Interface(const char *arg)
		:VO_Interface(arg),
		x11(*new(zeromem) X11_System(vo_conf.mDisplayName))
{
    num_buffers=1;
    subdev_flags = 0xFFFFFFFEUL;

    if(arg) subdev_flags = parse_sub_device(arg);
#ifdef CONFIG_VIDIX
    if(vidix_name) {
	vidix=vidix_preinit(vidix_name);
	if(!(vidix_server=vidix_get_server(vidix))) {
	    MSG_ERR("Cannot initialze vidix with '%s' argument\n",vidix_name);
	    exit_player("Vidix error");
	}
    }
#endif
    x11.saver_off();
}

X11_VO_Interface::~X11_VO_Interface()
{
    unsigned i;
#ifdef CONFIG_VIDIX
    if(vidix) vidix_term(vidix);
    delete vidix_server;
#endif
    for(i=0;i<num_buffers;i++)  x11.freeMyXImage(i);
    x11.saver_on(); // screen saver back on

#ifdef HAVE_XF86VM
    x11.vm_close();
#endif
}

#ifdef CONFIG_VIDIX
void X11_VO_Interface::resize_vidix() const {
    vo_rect_t winc;
    x11.get_win_coord(&winc);
    vidix_stop(vidix);
    if (vidix_init(vidix,image_width, image_height, winc.x, winc.y,
	    winc.w, winc.h, in_format, x11.depth(),
	    vo_conf.screenwidth, vo_conf.screenheight) != MPXP_Ok)
    {
	MSG_FATAL( "Can't initialize VIDIX driver: %s: %s\n",
	    vidix_name, strerror(errno));
	vidix_term(vidix);
	exit_player("Vidix init"); /* !!! */
    }
    if(vidix_start(vidix)!=0) exit_player("Vidix start");
}
#endif

uint32_t X11_VO_Interface::check_events(const vo_resize_t*vrest)
{
    uint32_t ret = x11.check_events(vrest->adjust_size,vrest->vo);

    /* clear the old window */
    if (ret & VO_EVENT_RESIZE) {
	unsigned idx;
	vo_rect_t r;
	x11.get_win_coord(&r);
	unsigned newW= r.w;
	unsigned newH= r.h;
	int newAspect=		(newW*(1<<16) + (newH>>1))/newH;
	if(newAspect>baseAspect)newW= (newH*baseAspect + (1<<15))>>16;
	else			newH= ((newW<<16) + (baseAspect>>1)) /baseAspect;
	image_width= (newW+7)&(~7);
	image_height= newH;
#ifdef CONFIG_VIDIX
	if(vidix_name) resize_vidix();
	else
#endif
	{
	    vo_lock_surfaces(vrest->vo);
	    for(idx=0;idx<num_buffers;idx++) {
		x11.freeMyXImage(idx);
		x11.getMyXImage(idx,vinfo.visual,depth,image_width,image_height);
	    }
	    vo_unlock_surfaces(vrest->vo);
	}
   }
   return ret;
}

MPXP_Rc X11_VO_Interface::configure(uint32_t width,uint32_t height,uint32_t d_width,uint32_t d_height,unsigned _flags,const char *title,uint32_t format)
{
    XSizeHints hint;
    unsigned i;

    flags=_flags;
    num_buffers=vo_conf.xp_buffs;

    if (!title)
	title = mp_strdup("MPlayerXP X11 (XImage/Shm) render");

    in_format=format;

    depth=x11.depth();
    if ( depth != 15 && depth != 16 && depth != 24 && depth != 32 )
	depth=24;
    x11.match_visual( &vinfo );

    baseAspect= ((1<<16)*d_width + d_height/2)/d_height;

    aspect_save_orig(width,height);
    aspect_save_prescale(d_width,d_height);
    aspect_save_screenres(vo_conf.screenwidth,vo_conf.screenheight);

    aspect(&d_width,&d_height,flags&VOFLAG_SWSCALE?A_ZOOM:A_NOZOOM);

    x11.calcpos(&hint,d_width,d_height,flags);
    hint.flags=PPosition | PSize;

    image_width=d_width;
    image_height=d_height;

    x11.create_window(hint,&vinfo,flags&VOFLAG_MODESWITCHING,depth,title);

    x11.classhint("vo_x11");
    x11.hidecursor();
    if ( flags&VOFLAG_FULLSCREEN ) x11.decoration(0);

    /* we cannot grab mouse events on root window :( */
    x11.select_input(StructureNotifyMask | KeyPressMask |
		    ButtonPressMask | ButtonReleaseMask | PointerMotionMask);

#ifdef CONFIG_VIDIX
    if(!vidix_name)
#endif
    for(i=0;i<num_buffers;i++) x11.getMyXImage(i,vinfo.visual,depth,image_width,image_height);

#ifdef CONFIG_VIDIX
    if(!vidix_name) {
#endif
    XImage* ximg=x11.Image(0);
    switch ((bpp=ximg->bits_per_pixel)){
	case 24: out_format= IMGFMT_BGR24; break;
	case 32: out_format= IMGFMT_BGR32; break;
	case 15: out_format= IMGFMT_BGR15; break;
	case 16: out_format= IMGFMT_BGR16; break;
	default: break;
    }

    /* If we have blue in the lowest bit then obviously RGB */
    mode=( ( ximg->blue_mask & 0x01 ) != 0 ) ? MODE_RGB : MODE_BGR;
#ifdef WORDS_BIGENDIAN
    if ( ximg->byte_order != MSBFirst )
#else
    if ( ximg->byte_order != LSBFirst )
#endif
    {
	mode=( ( ximg->blue_mask & 0x01 ) != 0 ) ? MODE_BGR : MODE_RGB;
    }

#ifdef WORDS_BIGENDIAN
    if(mode==MODE_BGR && bpp!=32) {
	MSG_ERR("BGR%d not supported, please contact the developers\n", priv.bpp);
	return MPXP_False;
    }
    if(mode==MODE_RGB && bpp==32) {
	MSG_ERR("RGB32 not supported on big-endian systems, please contact the developers\n");
	return MPXP_False;
    }
#else
    if(mode==MODE_BGR) {
	MSG_ERR("BGR not supported, please contact the developers\n");
	return MPXP_False;
    }
#endif
#ifdef CONFIG_VIDIX
    }
#endif
#ifdef CONFIG_VIDIX
    if(vidix_name) {
	vo_rect_t winc;
	x11.get_win_coord(&winc);
	if(vidix_init(vidix,image_width,image_height,winc.x,winc.y,
			winc.w,winc.h,
			in_format,x11.depth(),
			vo_conf.screenwidth,vo_conf.screenheight) != MPXP_Ok) {
	    MSG_ERR("vo_vesa: Can't initialize VIDIX driver\n");
	    vidix_name = NULL;
	    return MPXP_False;
	} else MSG_V("vo_vesa: Using VIDIX\n");
	if(vidix_start(vidix)!=0) return MPXP_False;
	if (vidix_grkey_support(vidix)) {
	    vidix_grkey_t gr_key;
	    vidix_grkey_get(vidix,&gr_key);
	    gr_key.key_op = KEYS_PUT;
	    gr_key.ckey.op = CKEY_TRUE;
	    gr_key.ckey.red = 255;
	    gr_key.ckey.green = 0;
	    gr_key.ckey.blue = 255;
	    vidix_grkey_set(vidix,&gr_key);
	}
    }
#endif
    return MPXP_Ok;
}

void X11_VO_Interface::display_image(XImage *myximage ) const
{
    vo_rect_t r;
    x11.get_win_coord(&r);
    r.x=r.y=0;
    r.w=(r.w-myximage->width)/2;
    r.h=(r.h-myximage->height)/2;
    x11.put_image(myximage,r);
}

void X11_VO_Interface::select_frame( unsigned idx ){
#ifdef CONFIG_VIDIX
    if(vidix_server) {
	vidix_server->select_frame(vidix,idx);
	return;
    }
#endif
    display_image(x11.Image(idx));
    if (num_buffers>1) x11.flush();
    else x11.sync(False);
    return;
}

MPXP_Rc X11_VO_Interface::query_format(vo_query_fourcc_t* format) const
{
    MSG_DBG2("vo_x11: query_format was called: %x (%s)\n",format->fourcc,vo_format_name(format->fourcc));
#ifdef WORDS_BIGENDIAN
    if (IMGFMT_IS_BGR(format->fourcc) && rgbfmt_depth(format->fourcc)<48)
#else
    if (IMGFMT_IS_RGB(format->fourcc) && rgbfmt_depth(format->fourcc)<48)
#endif
    {
	format->flags=VOCAP_SUPPORTED;
	return MPXP_Ok;
    }
// just for tests:
//if(format->fourcc==IMGFMT_YUY2) return 0x1|0x2|0x4;
    return MPXP_False;
}

void X11_VO_Interface::dri_get_surface_caps(dri_surface_cap_t *caps) const
{
    caps->caps = DRI_CAP_TEMP_VIDEO;
    caps->fourcc = out_format;
    caps->width=image_width;
    caps->height=image_height;
    caps->x=0;
    caps->y=0;
    caps->w=image_width;
    caps->h=image_height;
    caps->strides[0] = image_width*((bpp+7)/8);
    caps->strides[1] = 0;
    caps->strides[2] = 0;
    caps->strides[3] = 0;
}

void X11_VO_Interface::dri_get_surface(dri_surface_t *surf) const
{
    surf->planes[0] = x11.ImageData(surf->idx);
    surf->planes[1] = 0;
    surf->planes[2] = 0;
    surf->planes[3] = 0;
}

MPXP_Rc X11_VO_Interface::ctrl(uint32_t request, any_t*data)
{
#ifdef CONFIG_VIDIX
    if(vidix_server)
	if(vidix_server->control(vidix,request,data)==MPXP_Ok) return MPXP_Ok;
#endif
    switch (request) {
	case VOCTRL_CHECK_EVENTS: {
	    vo_resize_t* vrest = reinterpret_cast<vo_resize_t*>(data);
	    vrest->event_type = check_events(vrest);
#ifdef CONFIG_VIDIX
	    if(vidix_name) resize_vidix();
#endif
	    return MPXP_True;
	}
	case VOCTRL_FULLSCREEN:
	    x11.fullscreen();
#ifdef CONFIG_VIDIX
	    if(vidix_name) resize_vidix();
#endif
	    return MPXP_True;
	// all cases below are substituted by vidix
	case VOCTRL_QUERY_FORMAT:
	    return query_format(reinterpret_cast<vo_query_fourcc_t*>(data));
	case VOCTRL_GET_NUM_FRAMES:
	    *(uint32_t *)data = num_buffers;
	    return MPXP_True;
	case DRI_GET_SURFACE_CAPS:
	    dri_get_surface_caps(reinterpret_cast<dri_surface_cap_t*>(data));
	    return MPXP_True;
	case DRI_GET_SURFACE:
	    dri_get_surface(reinterpret_cast<dri_surface_t*>(data));
	    return MPXP_True;
    }
    return MPXP_NA;
}

static VO_Interface* query_interface(const char* args) { return new(zeromem) X11_VO_Interface(args); }
extern const vo_info_t x11_info =
{
	"X11 ( XImage/Shm )"
#ifdef CONFIG_VIDIX
	" (with x11:vidix subdevice)"
#endif
	,
	"x11",
	"Aaron Holtzman <aholtzma@ess.engr.uvic.ca>",
	"",
	query_interface
};
