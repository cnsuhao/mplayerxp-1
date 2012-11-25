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

LIBVO_EXTERN( x11 )

static vo_info_t vo_info =
{
	"X11 ( XImage/Shm )"
#ifdef CONFIG_VIDIX
	" (with x11:vidix subdevice)"
#endif
	,
	"x11",
	"Aaron Holtzman <aholtzma@ess.engr.uvic.ca>",
	""
};

#ifdef HAVE_SHM
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#endif
/* private prototypes */
static void __FASTCALL__ Display_Image (vo_data_t*vo, XImage * myximage );

/*** X11 related variables ***/
struct x11_priv_t : public video_private {
    x11_priv_t();
    virtual ~x11_priv_t() {}

    uint32_t		image_width;
    uint32_t		image_height;
    uint32_t		in_format;
    uint32_t		out_format;

    unsigned		depth,bpp,mode;

    XVisualInfo		vinfo;

    int			baseAspect; // 1<<16 based fixed point aspect, so that the aspect stays correct during resizing
/* xp related variables */
    unsigned		num_buffers; // 1 - default
#ifdef CONFIG_VIDIX
    const char *	vidix_name;
    vidix_server_t*	vidix_server;
#endif
    uint32_t		subdev_flags;
    X11_System*		x11;
};

x11_priv_t::x11_priv_t() {
    num_buffers=1;
    subdev_flags = 0xFFFFFFFEUL;
}

static uint32_t __FASTCALL__ parseSubDevice(vo_data_t*vo,const char *sd)
{
    x11_priv_t& priv = *static_cast<x11_priv_t*>(vo->priv);
    uint32_t flags;
    flags = 0;
#ifdef CONFIG_VIDIX
    if(memcmp(sd,"vidix",5) == 0) priv.vidix_name = &sd[5]; /* priv.vidix_name will be valid within init() */
    else
#endif
    { MSG_ERR("vo_vesa: Unknown subdevice: '%s'\n", sd); return 0xFFFFFFFFUL; }
    return flags;
}

static MPXP_Rc __FASTCALL__ preinit(vo_data_t*vo,const char *arg)
{
    MPXP_Rc vidix_err=MPXP_Ok;
    x11_priv_t* priv;
    priv=new(zeromem) x11_priv_t;
    vo->priv=priv;
    if(arg) priv->subdev_flags = parseSubDevice(vo,arg);
#ifdef CONFIG_VIDIX
    if(priv->vidix_name) {
	if(!(priv->vidix_server=vidix_preinit(vo,priv->vidix_name,&video_out_x11)))
	    vidix_err=MPXP_False;
    }
#endif
    priv->x11=new(zeromem) X11_System(vo_conf.mDisplayName);
    priv->x11->saver_off();
    return vidix_err;
}


static void uninit(vo_data_t*vo)
{
    unsigned i;
    x11_priv_t& priv = *static_cast<x11_priv_t*>(vo->priv);
    X11_System& x11 = *priv.x11;
#ifdef CONFIG_VIDIX
    if(priv.vidix_name) vidix_term(vo);
    delete priv.vidix_server;
#endif
    for(i=0;i<priv.num_buffers;i++)  x11.freeMyXImage(i);
    x11.saver_on(); // screen saver back on

#ifdef HAVE_XF86VM
    x11.vm_close();
#endif
    delete &x11;
    delete vo->priv;
}

#ifdef CONFIG_VIDIX
static void resize_vidix(vo_data_t* vo) {
    x11_priv_t& priv = *static_cast<x11_priv_t*>(vo->priv);
    X11_System& x11 = *priv.x11;
    vo_rect_t winc;
    x11.get_win_coord(&winc);
    vidix_stop(vo);
    if (vidix_init(vo,priv.image_width, priv.image_height, winc.x, winc.y,
	    winc.w, winc.h, priv.in_format, x11.depth(),
	    vo_conf.screenwidth, vo_conf.screenheight) != MPXP_Ok)
    {
	MSG_FATAL( "Can't initialize VIDIX driver: %s: %s\n",
	    priv.vidix_name, strerror(errno));
	vidix_term(vo);
	uninit(vo);
	exit(1); /* !!! */
    }
    if(vidix_start(vo)!=0) { uninit(vo); exit(1); }
}
#endif

static uint32_t __FASTCALL__ check_events(vo_data_t*vo,vo_adjust_size_t adjust_size)
{
    x11_priv_t& priv = *static_cast<x11_priv_t*>(vo->priv);
    X11_System& x11 = *priv.x11;
    uint32_t ret = x11.check_events(vo,adjust_size);

    /* clear the old window */
    if (ret & VO_EVENT_RESIZE) {
	unsigned idx;
	unsigned newW= vo->dest.w;
	unsigned newH= vo->dest.h;
	int newAspect= (newW*(1<<16) + (newH>>1))/newH;
	if(newAspect>priv.baseAspect) newW= (newH*priv.baseAspect + (1<<15))>>16;
	else                 newH= ((newW<<16) + (priv.baseAspect>>1)) /priv.baseAspect;
	priv.image_width= (newW+7)&(~7);
	priv.image_height= newH;
#ifdef CONFIG_VIDIX
	if(priv.vidix_name) resize_vidix(vo);
	else
#endif
	{
	    vo_lock_surfaces(vo);
	    for(idx=0;idx<priv.num_buffers;idx++) {
		x11.freeMyXImage(idx);
		x11.getMyXImage(idx,priv.vinfo.visual,priv.depth,priv.image_width,priv.image_height);
	    }
	    vo_unlock_surfaces(vo);
	}
   }
   return ret;
}

static MPXP_Rc __FASTCALL__ config(vo_data_t*vo,uint32_t width,uint32_t height,uint32_t d_width,uint32_t d_height,uint32_t flags,char *title,uint32_t format)
{
    x11_priv_t& priv = *static_cast<x11_priv_t*>(vo->priv);
    X11_System& x11 = *priv.x11;
    // int interval, prefer_blank, allow_exp, nothing;
    XSizeHints hint;
    unsigned i;

    priv.num_buffers=vo_conf.xp_buffs;

    if (!title)
	title = mp_strdup("MPlayerXP X11 (XImage/Shm) render");

    priv.in_format=format;

    priv.depth=x11.depth();
    if ( priv.depth != 15 && priv.depth != 16 && priv.depth != 24 && priv.depth != 32 )
	priv.depth=24;
    x11.match_visual( &priv.vinfo );

    priv.baseAspect= ((1<<16)*d_width + d_height/2)/d_height;

    aspect_save_orig(width,height);
    aspect_save_prescale(d_width,d_height);
    aspect_save_screenres(vo_conf.screenwidth,vo_conf.screenheight);

    aspect(&d_width,&d_height,vo_FS(vo)?A_ZOOM:A_NOZOOM);

    x11.calcpos(vo,&hint,d_width,d_height,flags);
    hint.flags=PPosition | PSize;
    vo->dest.w=hint.width;
    vo->dest.h=hint.height;

    priv.image_width=d_width;
    priv.image_height=d_height;

    x11.create_window(hint,priv.vinfo.visual,vo_VM(vo),priv.depth,title);

    x11.classhint("vo_x11");
    x11.hidecursor();
    if ( vo_FS(vo) ) x11.decoration(0);

    /* we cannot grab mouse events on root window :( */
    x11.select_input(StructureNotifyMask | KeyPressMask |
		    ButtonPressMask | ButtonReleaseMask | PointerMotionMask);

#ifdef CONFIG_VIDIX
    if(!priv.vidix_name)
#endif
    for(i=0;i<priv.num_buffers;i++) x11.getMyXImage(i,priv.vinfo.visual,priv.depth,priv.image_width,priv.image_height);

#ifdef CONFIG_VIDIX
    if(!priv.vidix_name) {
#endif
    XImage* ximg=x11.Image(0);
    switch ((priv.bpp=ximg->bits_per_pixel)){
	case 24: priv.out_format= IMGFMT_BGR24; break;
	case 32: priv.out_format= IMGFMT_BGR32; break;
	case 15: priv.out_format= IMGFMT_BGR15; break;
	case 16: priv.out_format= IMGFMT_BGR16; break;
	default: break;
    }

    /* If we have blue in the lowest bit then obviously RGB */
    priv.mode=( ( ximg->blue_mask & 0x01 ) != 0 ) ? MODE_RGB : MODE_BGR;
#ifdef WORDS_BIGENDIAN
    if ( ximg->byte_order != MSBFirst )
#else
    if ( ximg->byte_order != LSBFirst )
#endif
    {
	priv.mode=( ( ximg->blue_mask & 0x01 ) != 0 ) ? MODE_BGR : MODE_RGB;
    }

#ifdef WORDS_BIGENDIAN
    if(priv.mode==MODE_BGR && priv.bpp!=32) {
	MSG_ERR("BGR%d not supported, please contact the developers\n", priv.bpp);
	return MPXP_False;
    }
    if(priv.mode==MODE_RGB && priv.bpp==32) {
	MSG_ERR("RGB32 not supported on big-endian systems, please contact the developers\n");
	return MPXP_False;
    }
#else
    if(priv.mode==MODE_BGR) {
	MSG_ERR("BGR not supported, please contact the developers\n");
	return MPXP_False;
    }
#endif
#ifdef CONFIG_VIDIX
    }
#endif
#ifdef CONFIG_VIDIX
    if(priv.vidix_name) {
	vo_rect_t winc;
	x11.get_win_coord(&winc);
	if(vidix_init(vo,priv.image_width,priv.image_height,winc.x,winc.y,
			winc.w,winc.h,
			priv.in_format,x11.depth(),
			vo_conf.screenwidth,vo_conf.screenheight) != MPXP_Ok) {
	    MSG_ERR("vo_vesa: Can't initialize VIDIX driver\n");
	    priv.vidix_name = NULL;
	    return MPXP_False;
	} else MSG_V("vo_vesa: Using VIDIX\n");
	if(vidix_start(vo)!=0) return MPXP_False;
	if (vidix_grkey_support(vo)) {
	    vidix_grkey_t gr_key;
	    vidix_grkey_get(vo,&gr_key);
	    gr_key.key_op = KEYS_PUT;
	    gr_key.ckey.op = CKEY_TRUE;
	    gr_key.ckey.red = 255;
	    gr_key.ckey.green = 0;
	    gr_key.ckey.blue = 255;
	    vidix_grkey_set(vo,&gr_key);
	}
    }
#endif
    return MPXP_Ok;
}

static const vo_info_t* get_info(const vo_data_t*vo )
{
    UNUSED(vo);
    return &vo_info;
}

static void __FASTCALL__ Display_Image(vo_data_t*vo,XImage *myximage )
{
    x11_priv_t& priv = *static_cast<x11_priv_t*>(vo->priv);
    X11_System& x11 = *priv.x11;
    vo_rect_t r;
    r.x=r.y=0;
    r.w=(vo->dest.w-myximage->width)/2;
    r.h=(vo->dest.h-myximage->height)/2;
    x11.put_image(myximage,r);
}

static void __FASTCALL__ select_frame(vo_data_t*vo, unsigned idx ){
    x11_priv_t& priv = *static_cast<x11_priv_t*>(vo->priv);
    X11_System& x11 = *priv.x11;
#ifdef CONFIG_VIDIX
    if(priv.vidix_server) {
	priv.vidix_server->select_frame(vo,idx);
	return;
    }
#endif
    Display_Image(vo,x11.Image(idx));
    if (priv.num_buffers>1) x11.flush();
    else x11.sync(False);
    return;
}

static MPXP_Rc __FASTCALL__ query_format(vo_query_fourcc_t* format )
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

static void __FASTCALL__ x11_dri_get_surface_caps(const vo_data_t*vo,dri_surface_cap_t *caps)
{
    x11_priv_t& priv = *static_cast<x11_priv_t*>(vo->priv);
    caps->caps = DRI_CAP_TEMP_VIDEO;
    caps->fourcc = priv.out_format;
    caps->width=priv.image_width;
    caps->height=priv.image_height;
    caps->x=0;
    caps->y=0;
    caps->w=priv.image_width;
    caps->h=priv.image_height;
    caps->strides[0] = priv.image_width*((priv.bpp+7)/8);
    caps->strides[1] = 0;
    caps->strides[2] = 0;
    caps->strides[3] = 0;
}

static void __FASTCALL__ x11_dri_get_surface(const vo_data_t*vo,dri_surface_t *surf)
{
    x11_priv_t& priv = *static_cast<x11_priv_t*>(vo->priv);
    X11_System& x11 = *priv.x11;
    surf->planes[0] = x11.ImageData(surf->idx);
    surf->planes[1] = 0;
    surf->planes[2] = 0;
    surf->planes[3] = 0;
}

static MPXP_Rc __FASTCALL__ control(vo_data_t*vo,uint32_t request, any_t*data)
{
    x11_priv_t& priv = *static_cast<x11_priv_t*>(vo->priv);
    X11_System& x11 = *priv.x11;
#ifdef CONFIG_VIDIX
    if(priv.vidix_server)
	if(priv.vidix_server->control(vo,request,data)==MPXP_Ok) return MPXP_Ok;
#endif
    switch (request) {
	case VOCTRL_CHECK_EVENTS: {
	    vo_resize_t* vrest = reinterpret_cast<vo_resize_t*>(data);
	    vrest->event_type = check_events(vo,vrest->adjust_size);
#ifdef CONFIG_VIDIX
	    if(priv.vidix_name) resize_vidix(vo);
#endif
	    return MPXP_True;
	}
	case VOCTRL_FULLSCREEN:
	    x11.fullscreen(vo);
#ifdef CONFIG_VIDIX
	    if(priv.vidix_name) resize_vidix(vo);
#endif
	    return MPXP_True;
	// all cases below are substituted by vidix
	case VOCTRL_QUERY_FORMAT:
	    return query_format(reinterpret_cast<vo_query_fourcc_t*>(data));
	case VOCTRL_GET_NUM_FRAMES:
	    *(uint32_t *)data = priv.num_buffers;
	    return MPXP_True;
	case DRI_GET_SURFACE_CAPS:
	    x11_dri_get_surface_caps(vo,reinterpret_cast<dri_surface_cap_t*>(data));
	    return MPXP_True;
	case DRI_GET_SURFACE:
	    x11_dri_get_surface(vo,reinterpret_cast<dri_surface_t*>(data));
	    return MPXP_True;
    }
    return MPXP_NA;
}
