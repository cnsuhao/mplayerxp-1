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
#include "../mplayer.h"
#include "../dec_ahead.h"
#include "aspect.h"
#include "video_out.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>


#include "x11_common.h"

#include "fastmemcpy.h"
#include "sub.h"


/* special case for vo_x11 driver. We need mutexes */
//#include "../dec_ahead.h"
#define MSG_D(args...)
#define LOCK_VDECODING() { MSG_D(DA_PREFIX"LOCK_VDECODING\n"); pthread_mutex_lock(&vdecoding_mutex); }

#include "../postproc/swscale.h" /* for MODE_RGB(BGR) definitions */
#include "video_out_internal.h"
#include "dri_vo.h"
#include "../mp_image.h"
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
static void __FASTCALL__ Display_Image ( XImage * myximage );

/*** X11 related variables ***/
typedef struct vox11_priv_s {
    uint32_t		image_width;
    uint32_t		image_height;
    uint32_t		in_format;
    uint32_t		out_format;

    unsigned		depth,bpp,mode;

    XWindowAttributes	attribs;
    XVisualInfo		vinfo;

    int			baseAspect; // 1<<16 based fixed point aspect, so that the aspect stays correct during resizing
    int			Flip_Flag;
    int			zoomFlag;
/* xp related variables */
    unsigned		num_buffers; // 1 - default
}vox11_priv_t;
static vox11_priv_t vox11;

static uint32_t __FASTCALL__ check_events(int (* __FASTCALL__ adjust_size)(unsigned cw,unsigned ch,unsigned *w,unsigned *h))
{
  uint32_t ret = vo_x11_check_events(vo.mDisplay,adjust_size);

   /* clear the old window */
  if (ret & VO_EVENT_RESIZE)
  {
	unsigned idx;
	unsigned newW= vo.dest.w;
	unsigned newH= vo.dest.h;
	int newAspect= (newW*(1<<16) + (newH>>1))/newH;
	if(newAspect>vox11.baseAspect) newW= (newH*vox11.baseAspect + (1<<15))>>16;
	else                 newH= ((newW<<16) + (vox11.baseAspect>>1)) /vox11.baseAspect;
	XSetBackground(vo.mDisplay, vo.gc, 0);
	XClearWindow(vo.mDisplay, vo.window);
	vox11.image_width= (newW+7)&(~7);
	vox11.image_height= newH;
	if(enable_xp) LOCK_VDECODING();
	for(idx=0;idx<vox11.num_buffers;idx++)
	{
	    vo_x11_freeMyXImage(idx);
	    vo_x11_getMyXImage(idx,vox11.vinfo.visual,vox11.depth,vox11.image_width,vox11.image_height);
	}
   }
   return ret;
}

static uint32_t __FASTCALL__ config( uint32_t width,uint32_t height,uint32_t d_width,uint32_t d_height,uint32_t flags,char *title,uint32_t format,const vo_tune_info_t *info)
{
// int screen;
 int fullscreen=0;
 int vm=0;
// int interval, prefer_blank, allow_exp, nothing;
 unsigned int fg,bg;
 XSizeHints hint;
 XEvent xev;
 XGCValues xgcv;
 Colormap theCmap;
 XSetWindowAttributes xswa;
 unsigned long xswamask;
 unsigned i;

 UNUSED(info);

 vox11.num_buffers=vo.doublebuffering?vo.da_buffs:1;

 if (!title)
    title = strdup("MPlayerXP X11 (XImage/Shm) render");

 vox11.in_format=format;

 if( flags&0x03 ) fullscreen = 1;
 if( flags&0x02 ) vm = 1;
 if( flags&0x08 ) vox11.Flip_Flag = 1;
 vox11.zoomFlag = flags&0x04;
// if(!fullscreen) vox11.zoomFlag=1; //it makes no sense to avoid zooming on windowd vox11.mode

 XGetWindowAttributes( vo.mDisplay,DefaultRootWindow( vo.mDisplay ),&vox11.attribs );
 vox11.depth=vox11.attribs.depth;

 if ( vox11.depth != 15 && vox11.depth != 16 && vox11.depth != 24 && vox11.depth != 32 ) vox11.depth=24;
 XMatchVisualInfo( vo.mDisplay,vo.mScreen,vox11.depth,TrueColor,&vox11.vinfo );


 vox11.baseAspect= ((1<<16)*d_width + d_height/2)/d_height;

 aspect_save_orig(width,height);
 aspect_save_prescale(d_width,d_height);
 aspect_save_screenres(vo.screenwidth,vo.screenheight);

 vo.softzoom=flags&VOFLAG_SWSCALE;

 aspect(&d_width,&d_height,A_NOZOOM);
 if( fullscreen ) aspect(&d_width,&d_height,A_ZOOM);

    vo_x11_calcpos(&hint,d_width,d_height,flags);
    hint.flags=PPosition | PSize;

    bg=WhitePixel( vo.mDisplay,vo.mScreen );
    fg=BlackPixel( vo.mDisplay,vo.mScreen );
    vo.dest.w=hint.width;
    vo.dest.h=hint.height;

    vox11.image_width=d_width;
    vox11.image_height=d_height;

    theCmap  =XCreateColormap( vo.mDisplay,RootWindow( vo.mDisplay,vo.mScreen ),
    vox11.vinfo.visual,AllocNone );

    xswa.background_pixel=0;
    xswa.border_pixel=0;
    xswa.colormap=theCmap;
    xswamask=CWBackPixel | CWBorderPixel | CWColormap;

#ifdef HAVE_XF86VM
    if ( vm )
     {
      xswa.override_redirect=True;
      xswamask|=CWOverrideRedirect;
     }
#endif

    if ( vo.WinID>=0 ){
      vo.window = vo.WinID ? ((Window)vo.WinID) : RootWindow( vo.mDisplay,vo.mScreen );
      XUnmapWindow( vo.mDisplay,vo.window );
      XChangeWindowAttributes( vo.mDisplay,vo.window,xswamask,&xswa );
    }
    else {
      vo.window=XCreateWindow( vo.mDisplay,RootWindow( vo.mDisplay,vo.mScreen ),
                         hint.x,hint.y,
                         hint.width,hint.height,
                         xswa.border_pixel,vox11.depth,CopyFromParent,vox11.vinfo.visual,xswamask,&xswa );
    }
    vo_x11_classhint( vo.mDisplay,vo.window,"vo_x11" );
    vo_x11_hidecursor(vo.mDisplay,vo.window);
    if ( fullscreen ) vo_x11_decoration( vo.mDisplay,vo.window,0 );
    XSelectInput( vo.mDisplay,vo.window,StructureNotifyMask );
    XSetStandardProperties( vo.mDisplay,vo.window,title,title,None,NULL,0,&hint );
    XMapWindow( vo.mDisplay,vo.window );
#ifdef HAVE_XINERAMA
    vo_x11_xinerama_move(vo.mDisplay,vo.window,&hint);
#endif
    if(vo.WinID!=0)
    do { XNextEvent( vo.mDisplay,&xev ); } while ( xev.type != MapNotify || xev.xmap.event != vo.window );
    XSelectInput( vo.mDisplay,vo.window,NoEventMask );

    XFlush( vo.mDisplay );
    XSync( vo.mDisplay,False );
    vo.gc=XCreateGC( vo.mDisplay,vo.window,0L,&xgcv );

    /* we cannot grab mouse events on root window :( */
    XSelectInput( vo.mDisplay,vo.window,StructureNotifyMask | KeyPressMask | 
	((vo.WinID==0)?0:(ButtonPressMask | ButtonReleaseMask | PointerMotionMask)) );

#ifdef HAVE_XF86VM
    if ( vm )
     {
      /* Grab the mouse pointer in our window */
      XGrabPointer(vo.mDisplay, vo.window, True, 0,
                   GrabModeAsync, GrabModeAsync,
                   vo.window, None, CurrentTime);
      XSetInputFocus(vo.mDisplay, vo.window, RevertToNone, CurrentTime);
     }
#endif
  for(i=0;i<vox11.num_buffers;i++) vo_x11_getMyXImage(i,vox11.vinfo.visual,vox11.depth,vox11.image_width,vox11.image_height);

  switch ((vox11.bpp=vo_x11_myximage[0]->bits_per_pixel)){
	case 24: vox11.out_format= IMGFMT_BGR24; break;
	case 32: vox11.out_format= IMGFMT_BGR32; break;
	case 15: vox11.out_format= IMGFMT_BGR15; break;
	case 16: vox11.out_format= IMGFMT_BGR16; break;
	default: break;
  }

  /* If we have blue in the lowest bit then obviously RGB */
  vox11.mode=( ( vo_x11_myximage[0]->blue_mask & 0x01 ) != 0 ) ? MODE_RGB : MODE_BGR;
#ifdef WORDS_BIGENDIAN
  if ( vo_x11_myximage[0]->byte_order != MSBFirst )
#else
  if ( vo_x11_myximage[0]->byte_order != LSBFirst )
#endif
  {
    vox11.mode=( ( vo_x11_myximage[0]->blue_mask & 0x01 ) != 0 ) ? MODE_BGR : MODE_RGB;
  }

#ifdef WORDS_BIGENDIAN
  if(vox11.mode==MODE_BGR && vox11.bpp!=32){
    MSG_ERR("BGR%d not supported, please contact the developers\n", vox11.bpp);
    return -1;
  }
  if(vox11.mode==MODE_RGB && vox11.bpp==32){
    MSG_ERR("RGB32 not supported on big-endian systems, please contact the developers\n");
    return -1;
  }
#else
  if(vox11.mode==MODE_BGR){
    MSG_ERR("BGR not supported, please contact the developers\n");
    return -1;
  }
#endif
 saver_off(vo.mDisplay);
 return 0;
}

static const vo_info_t* get_info( void )
{ return &vo_info; }

static void __FASTCALL__ Display_Image( XImage *myximage )
{
#ifdef DISP
#ifdef HAVE_SHM
 if ( vo_x11_Shmem_Flag )
  {
   XShmPutImage( vo.mDisplay,vo.window,vo.gc,myximage,
                 0,0,
                 ( vo.dest.w - myximage->width ) / 2,( vo.dest.h - myximage->height ) / 2,
                 myximage->width,myximage->height,True );
  }
  else
#endif
   {
    XPutImage( vo.mDisplay,vo.window,vo.gc,myximage,
               0,0,
               ( vo.dest.w - myximage->width ) / 2,( vo.dest.h - myximage->height ) / 2,
               myximage->width,myximage->height);
  }
#endif
}

static void __FASTCALL__ change_frame( unsigned idx ){
 Display_Image( vo_x11_myximage[idx] );
 if (vox11.num_buffers>1) XFlush(vo.mDisplay);
 else XSync(vo.mDisplay, False);
 return;
}

static uint32_t __FASTCALL__ query_format( vo_query_fourcc_t* format )
{
    MSG_DBG2("vo_x11: query_format was called: %x (%s)\n",format->fourcc,vo_format_name(format->fourcc));
#ifdef WORDS_BIGENDIAN
    if (IMGFMT_IS_BGR(format->fourcc) && rgbfmt_depth(format->fourcc)<48)
#else
    if (IMGFMT_IS_RGB(format->fourcc) && rgbfmt_depth(format->fourcc)<48)
#endif
    {
	if (rgbfmt_depth(format->fourcc) == (unsigned)vo.depthonscreen)
	    return 0x1|0x2|0x4;
	else
	    return 0x1|0x4;
    }
    return 0;
}


static void uninit(void)
{
 unsigned i;
 for(i=0;i<vox11.num_buffers;i++)  vo_x11_freeMyXImage(i);
 saver_on(vo.mDisplay); // screen saver back on

#ifdef HAVE_XF86VM
 vo_vm_close(vo.mDisplay);
#endif

 vo_x11_uninit(vo.mDisplay, vo.window);
}

static uint32_t __FASTCALL__ preinit(const char *arg)
{
    memset(&vox11,0,sizeof(vox11_priv_t));
    vox11.num_buffers=1;
    if(arg)
    {
	MSG_ERR("vo_x11: Unknown subdevice: %s\n",arg);
	return ENOSYS;
    }

    if( !vo_x11_init() ) return -1; // Can't open X11

    return 0;
}

static void __FASTCALL__ x11_dri_get_surface_caps(dri_surface_cap_t *caps)
{
    caps->caps = DRI_CAP_TEMP_VIDEO;
    caps->fourcc = vox11.out_format;
    caps->width=vox11.image_width;
    caps->height=vox11.image_height;
    caps->x=0;
    caps->y=0;
    caps->w=vox11.image_width;
    caps->h=vox11.image_height;
    caps->strides[0] = vox11.image_width*((vox11.bpp+7)/8);
    caps->strides[1] = 0;
    caps->strides[2] = 0;
    caps->strides[3] = 0;
}

static void __FASTCALL__ x11_dri_get_surface(dri_surface_t *surf)
{
    surf->planes[0] = vo_x11_ImageData(surf->idx);
    surf->planes[1] = 0;
    surf->planes[2] = 0;
    surf->planes[3] = 0;
}

static uint32_t __FASTCALL__ control(uint32_t request, void *data)
{
  switch (request) {
  case VOCTRL_QUERY_FORMAT:
    return query_format((vo_query_fourcc_t*)data);
  case VOCTRL_CHECK_EVENTS:
    {
     vo_resize_t * vrest = (vo_resize_t *)data;
     vrest->event_type = check_events(vrest->adjust_size);
     return VO_TRUE;
    }
  case VOCTRL_FULLSCREEN:
    vo_x11_fullscreen();
    return VO_TRUE;
  case VOCTRL_GET_NUM_FRAMES:
	*(uint32_t *)data = vox11.num_buffers;
	return VO_TRUE;
  case DRI_GET_SURFACE_CAPS:
	x11_dri_get_surface_caps(data);
	return VO_TRUE;
  case DRI_GET_SURFACE:
	x11_dri_get_surface(data);
	return VO_TRUE;
  }
  return VO_NOTIMPL;
}
