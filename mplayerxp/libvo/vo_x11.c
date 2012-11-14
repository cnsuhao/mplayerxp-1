#define DISP

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
#ifdef HAVE_XF86VM
#include <X11/extensions/xf86vmode.h>
#endif
#include <errno.h>

#include "mp_config.h"
#include "mplayerxp.h"
#include "xmpcore/xmp_core.h"
#include "aspect.h"
#include "video_out.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>


#include "x11_common.h"

#include "osdep/fastmemcpy.h"
#include "osdep/mplib.h"
#include "sub.h"

#include "postproc/swscale.h" /* for MODE_RGB(BGR) definitions */
#include "video_out_internal.h"
#include "dri_vo.h"
#include "xmpcore/mp_image.h"
#include "vo_msg.h"
LIBVO_EXTERN( x11 )

static vo_info_t vo_info =
{
	"X11 ( XImage/Shm )",
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
typedef struct priv_s {
    uint32_t		image_width;
    uint32_t		image_height;
    uint32_t		in_format;
    uint32_t		out_format;

    unsigned		depth,bpp,mode;

    XWindowAttributes	attribs;
    XVisualInfo		vinfo;

    int			baseAspect; // 1<<16 based fixed point aspect, so that the aspect stays correct during resizing
/* xp related variables */
    unsigned		num_buffers; // 1 - default
}priv_t;

static uint32_t __FASTCALL__ check_events(vo_data_t*vo,int (* __FASTCALL__ adjust_size)(unsigned cw,unsigned ch,unsigned *w,unsigned *h))
{
    priv_t* priv=(priv_t*)vo->priv;
    uint32_t ret = vo_x11_check_events(vo,vo->mDisplay,adjust_size);

   /* clear the old window */
  if (ret & VO_EVENT_RESIZE)
  {
	unsigned idx;
	unsigned newW= vo->dest.w;
	unsigned newH= vo->dest.h;
	int newAspect= (newW*(1<<16) + (newH>>1))/newH;
	if(newAspect>priv->baseAspect) newW= (newH*priv->baseAspect + (1<<15))>>16;
	else                 newH= ((newW<<16) + (priv->baseAspect>>1)) /priv->baseAspect;
	XSetBackground(vo->mDisplay, vo->gc, 0);
	XClearWindow(vo->mDisplay, vo->window);
	priv->image_width= (newW+7)&(~7);
	priv->image_height= newH;
	vo_lock_surfaces(vo);
	for(idx=0;idx<priv->num_buffers;idx++)
	{
	    vo_x11_freeMyXImage(vo,idx);
	    vo_x11_getMyXImage(vo,idx,priv->vinfo.visual,priv->depth,priv->image_width,priv->image_height);
	}
	vo_unlock_surfaces(vo);
   }
   return ret;
}

static MPXP_Rc __FASTCALL__ config(vo_data_t*vo,uint32_t width,uint32_t height,uint32_t d_width,uint32_t d_height,uint32_t flags,char *title,uint32_t format)
{
    priv_t* priv=(priv_t*)vo->priv;
    // int interval, prefer_blank, allow_exp, nothing;
    unsigned int fg,bg;
    XSizeHints hint;
    XEvent xev;
    XGCValues xgcv;
    Colormap theCmap;
    XSetWindowAttributes xswa;
    unsigned long xswamask;
    unsigned i;

    priv->num_buffers=vo_conf.xp_buffs;

    if (!title)
	title = mp_strdup("MPlayerXP X11 (XImage/Shm) render");

    priv->in_format=format;

    XGetWindowAttributes( vo->mDisplay,DefaultRootWindow( vo->mDisplay ),&priv->attribs );
    priv->depth=priv->attribs.depth;

    if ( priv->depth != 15 && priv->depth != 16 && priv->depth != 24 && priv->depth != 32 ) priv->depth=24;
    XMatchVisualInfo( vo->mDisplay,vo->mScreen,priv->depth,TrueColor,&priv->vinfo );

    priv->baseAspect= ((1<<16)*d_width + d_height/2)/d_height;

    aspect_save_orig(width,height);
    aspect_save_prescale(d_width,d_height);
    aspect_save_screenres(vo_conf.screenwidth,vo_conf.screenheight);

    aspect(&d_width,&d_height,vo_FS(vo)?A_ZOOM:A_NOZOOM);

    vo_x11_calcpos(vo,&hint,d_width,d_height,flags);
    hint.flags=PPosition | PSize;

    bg=WhitePixel( vo->mDisplay,vo->mScreen );
    fg=BlackPixel( vo->mDisplay,vo->mScreen );
    vo->dest.w=hint.width;
    vo->dest.h=hint.height;

    priv->image_width=d_width;
    priv->image_height=d_height;

    theCmap  =XCreateColormap( vo->mDisplay,RootWindow( vo->mDisplay,vo->mScreen ),
    priv->vinfo.visual,AllocNone );

    xswa.background_pixel=0;
    xswa.border_pixel=0;
    xswa.colormap=theCmap;
    xswamask=CWBackPixel | CWBorderPixel | CWColormap;

#ifdef HAVE_XF86VM
    if ( vo_VM(vo) ) {
	xswa.override_redirect=True;
	xswamask|=CWOverrideRedirect;
    }
#endif

    if ( vo_conf.WinID>=0 ){
	vo->window = vo_conf.WinID ? ((Window)vo_conf.WinID) : RootWindow( vo->mDisplay,vo->mScreen );
	XUnmapWindow( vo->mDisplay,vo->window );
	XChangeWindowAttributes( vo->mDisplay,vo->window,xswamask,&xswa );
    } else {
	vo->window=XCreateWindow( vo->mDisplay,RootWindow( vo->mDisplay,vo->mScreen ),
				hint.x,hint.y,
				hint.width,hint.height,
				xswa.border_pixel,priv->depth,CopyFromParent,priv->vinfo.visual,xswamask,&xswa );
    }
    vo_x11_classhint( vo->mDisplay,vo->window,"vo_x11" );
    vo_x11_hidecursor(vo->mDisplay,vo->window);
    if ( vo_FS(vo) ) vo_x11_decoration(vo,vo->mDisplay,vo->window,0 );
    XSelectInput( vo->mDisplay,vo->window,StructureNotifyMask );
    XSetStandardProperties( vo->mDisplay,vo->window,title,title,None,NULL,0,&hint );
    XMapWindow( vo->mDisplay,vo->window );
#ifdef HAVE_XINERAMA
    vo_x11_xinerama_move(vo,vo->mDisplay,vo->window,&hint);
#endif
    if(vo_conf.WinID!=0)
    do { XNextEvent( vo->mDisplay,&xev ); } while ( xev.type != MapNotify || xev.xmap.event != vo->window );
    XSelectInput( vo->mDisplay,vo->window,NoEventMask );

    XFlush( vo->mDisplay );
    XSync( vo->mDisplay,False );
    vo->gc=XCreateGC( vo->mDisplay,vo->window,0L,&xgcv );

    /* we cannot grab mouse events on root window :( */
    XSelectInput( vo->mDisplay,vo->window,StructureNotifyMask | KeyPressMask |
	((vo_conf.WinID==0)?0:(ButtonPressMask | ButtonReleaseMask | PointerMotionMask)) );

#ifdef HAVE_XF86VM
    if ( vo_VM(vo) ) {
	/* Grab the mouse pointer in our window */
	XGrabPointer(vo->mDisplay, vo->window, True, 0,
		   GrabModeAsync, GrabModeAsync,
		   vo->window, None, CurrentTime);
	XSetInputFocus(vo->mDisplay, vo->window, RevertToNone, CurrentTime);
    }
#endif
    for(i=0;i<priv->num_buffers;i++) vo_x11_getMyXImage(vo,i,priv->vinfo.visual,priv->depth,priv->image_width,priv->image_height);

    XImage* ximg=vo_x11_Image(vo,0);
    switch ((priv->bpp=ximg->bits_per_pixel)){
	case 24: priv->out_format= IMGFMT_BGR24; break;
	case 32: priv->out_format= IMGFMT_BGR32; break;
	case 15: priv->out_format= IMGFMT_BGR15; break;
	case 16: priv->out_format= IMGFMT_BGR16; break;
	default: break;
    }

    /* If we have blue in the lowest bit then obviously RGB */
    priv->mode=( ( ximg->blue_mask & 0x01 ) != 0 ) ? MODE_RGB : MODE_BGR;
#ifdef WORDS_BIGENDIAN
    if ( ximg->byte_order != MSBFirst )
#else
    if ( ximg->byte_order != LSBFirst )
#endif
    {
	priv->mode=( ( ximg->blue_mask & 0x01 ) != 0 ) ? MODE_BGR : MODE_RGB;
    }

#ifdef WORDS_BIGENDIAN
    if(priv->mode==MODE_BGR && priv->bpp!=32) {
	MSG_ERR("BGR%d not supported, please contact the developers\n", priv->bpp);
	return MPXP_False;
    }
    if(priv->mode==MODE_RGB && priv->bpp==32) {
	MSG_ERR("RGB32 not supported on big-endian systems, please contact the developers\n");
	return MPXP_False;
    }
#else
    if(priv->mode==MODE_BGR) {
	MSG_ERR("BGR not supported, please contact the developers\n");
	return MPXP_False;
    }
#endif
    saver_off(vo,vo->mDisplay);
    return MPXP_Ok;
}

static const vo_info_t* get_info(const vo_data_t*vo )
{
    UNUSED(vo);
    return &vo_info;
}

static void __FASTCALL__ Display_Image(vo_data_t*vo,XImage *myximage )
{
#ifdef DISP
#ifdef HAVE_SHM
    if( vo_x11_Shmem_Flag(vo)) {
	XShmPutImage(	vo->mDisplay,vo->window,vo->gc,myximage,
			0,0,
			( vo->dest.w - myximage->width ) / 2,( vo->dest.h - myximage->height ) / 2,
			myximage->width,myximage->height,True );
    }
    else
#endif
    {
	XPutImage(	vo->mDisplay,vo->window,vo->gc,myximage,
			0,0,
			( vo->dest.w - myximage->width ) / 2,( vo->dest.h - myximage->height ) / 2,
			myximage->width,myximage->height);
    }
#endif
}

static void __FASTCALL__ select_frame(vo_data_t*vo, unsigned idx ){
    priv_t* priv=(priv_t*)vo->priv;
    Display_Image(vo,vo_x11_Image(vo,idx));
    if (priv->num_buffers>1) XFlush(vo->mDisplay);
    else XSync(vo->mDisplay, False);
    return;
}

static uint32_t __FASTCALL__ query_format(vo_data_t*vo, vo_query_fourcc_t* format )
{
    MSG_DBG2("vo_x11: query_format was called: %x (%s)\n",format->fourcc,vo_format_name(format->fourcc));
#ifdef WORDS_BIGENDIAN
    if (IMGFMT_IS_BGR(format->fourcc) && rgbfmt_depth(format->fourcc)<48)
#else
    if (IMGFMT_IS_RGB(format->fourcc) && rgbfmt_depth(format->fourcc)<48)
#endif
    {
	if (rgbfmt_depth(format->fourcc) == (unsigned)vo->depthonscreen)
	    return 0x1|0x2|0x4;
	else
	    return 0x1|0x4;
    }
// just for tests:
//if(format->fourcc==IMGFMT_YUY2) return 0x1|0x2|0x4;
    return 0;
}


static void uninit(vo_data_t*vo)
{
    unsigned i;
    priv_t* priv=(priv_t*)vo->priv;
    for(i=0;i<priv->num_buffers;i++)  vo_x11_freeMyXImage(vo,i);
    saver_on(vo,vo->mDisplay); // screen saver back on

#ifdef HAVE_XF86VM
    vo_vm_close(vo,vo->mDisplay);
#endif
    vo_x11_uninit(vo,vo->mDisplay, vo->window);
    mp_free(vo->priv);
}

static MPXP_Rc __FASTCALL__ preinit(vo_data_t*vo,const char *arg)
{
    vo->priv=mp_mallocz(sizeof(priv_t));
    priv_t* priv=(priv_t*)vo->priv;
    priv->num_buffers=1;
    if(arg) {
	MSG_ERR("vo_x11: Unknown subdevice: %s\n",arg);
	return MPXP_False;
    }
    if(vo_x11_init(vo)!=MPXP_Ok) return MPXP_False; // Can't open X11
    return MPXP_Ok;
}

static void __FASTCALL__ x11_dri_get_surface_caps(const vo_data_t*vo,dri_surface_cap_t *caps)
{
    priv_t* priv=(priv_t*)vo->priv;
    caps->caps = DRI_CAP_TEMP_VIDEO;
    caps->fourcc = priv->out_format;
    caps->width=priv->image_width;
    caps->height=priv->image_height;
    caps->x=0;
    caps->y=0;
    caps->w=priv->image_width;
    caps->h=priv->image_height;
    caps->strides[0] = priv->image_width*((priv->bpp+7)/8);
    caps->strides[1] = 0;
    caps->strides[2] = 0;
    caps->strides[3] = 0;
}

static void __FASTCALL__ x11_dri_get_surface(const vo_data_t*vo,dri_surface_t *surf)
{
    UNUSED(vo);
    surf->planes[0] = vo_x11_ImageData(vo,surf->idx);
    surf->planes[1] = 0;
    surf->planes[2] = 0;
    surf->planes[3] = 0;
}

static MPXP_Rc __FASTCALL__ control(vo_data_t*vo,uint32_t request, any_t*data)
{
    priv_t* priv=(priv_t*)vo->priv;
  switch (request) {
  case VOCTRL_QUERY_FORMAT:
    return query_format(vo,(vo_query_fourcc_t*)data);
  case VOCTRL_CHECK_EVENTS:
    {
     vo_resize_t * vrest = (vo_resize_t *)data;
     vrest->event_type = check_events(vo,vrest->adjust_size);
     return MPXP_True;
    }
  case VOCTRL_FULLSCREEN:
    vo_x11_fullscreen(vo);
    return MPXP_True;
  case VOCTRL_GET_NUM_FRAMES:
	*(uint32_t *)data = priv->num_buffers;
	return MPXP_True;
  case DRI_GET_SURFACE_CAPS:
	x11_dri_get_surface_caps(vo,data);
	return MPXP_True;
  case DRI_GET_SURFACE:
	x11_dri_get_surface(vo,data);
	return MPXP_True;
  }
  return MPXP_NA;
}
