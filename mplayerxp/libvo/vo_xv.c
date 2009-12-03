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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "../mplayer.h"
#include "../dec_ahead.h"
#include "video_out.h"
#include "video_out_internal.h"

LIBVO_EXTERN(xv)

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>
#include <errno.h>

#include "x11_common.h"

#include "fastmemcpy.h"
#include "sub.h"
#include "aspect.h"
#include "dri_vo.h"
#include "../mp_image.h"

static vo_info_t vo_info =
{
        "X11/Xv",
        "xv",
        "Gerd Knorr <kraxel@goldbach.in-berlin.de>",
        ""
};

/* since it doesn't seem to be defined on some platforms */
int XShmGetEventBase(Display*);

/* X11 related variables */
static unsigned depth;
static XWindowAttributes attribs;

#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvlib.h>
// FIXME: dynamically allocate this stuff
static void __FASTCALL__ allocate_xvimage(int);
static unsigned int ver,rel,req,ev,err;
static unsigned int formats, adaptors,i,xv_port,xv_format,xv_bpp;
static XvAdaptorInfo        *ai;
static XvImageFormatValues  *fo;

static unsigned expose_idx=0,num_buffers=1; // default
static XvImage* xvimage[MAX_DRI_BUFFERS];
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>

static XShmSegmentInfo Shminfo[MAX_DRI_BUFFERS];

static uint32_t image_width;
static uint32_t image_height;
static uint32_t image_format;
static int flip_flag;

static Window                 mRoot;
static uint32_t               drwX,drwY,drwWidth,drwHeight,drwBorderWidth,drwDepth;
static uint32_t               drwcX,drwcY,dwidth,dheight;

static int __FASTCALL__ xv_reset_video_eq(void)
{
 XvAttribute *attributes;
 int howmany,xv_atomka;
 static int was_reset = 0;
 /* get available attributes */
    attributes = XvQueryPortAttributes(mDisplay, xv_port, &howmany);
    /* first pass try reset */
    for (i = 0; (int)i < howmany && attributes; i++)
    {
	if (attributes[i].flags & XvSettable && !strcmp(attributes[i].name,"XV_SET_DEFAULTS"))
	{
		was_reset = 1;
		MSG_DBG2("vo_xv: reset gamma correction\n");
		xv_atomka = XInternAtom(mDisplay, attributes[i].name, True);
		XvSetPortAttribute(mDisplay, xv_port, xv_atomka, attributes[i].max_value);
	}
    }
    return was_reset;
}

static int __FASTCALL__ xv_set_video_eq(const vo_videq_t *info)
{
 XvAttribute *attributes;
 int howmany, xv_min,xv_max,xv_atomka;
 /* get available attributes */
    attributes = XvQueryPortAttributes(mDisplay, xv_port, &howmany);
    for (i = 0; (int)i < howmany && attributes; i++)
    {
            if (attributes[i].flags & XvSettable)
            {
                xv_min = attributes[i].min_value;
                xv_max = attributes[i].max_value;
                xv_atomka = XInternAtom(mDisplay, attributes[i].name, True);
		/* since we have SET_DEFAULTS first in our list, we can check if it's available
		then trigger it if it's ok so that the other values are at default upon query */
                if (xv_atomka != None)
                {
		    int port_value,port_min,port_max,port_mid,has_value=0;
		    if(!strcmp(attributes[i].name,"XV_BRIGHTNESS") && !strcmp(info->name,VO_EC_BRIGHTNESS))
		    {
			has_value=1;
			port_value = info->value;
		    }
		    else
		    if(!strcmp(attributes[i].name,"XV_SATURATION") && !strcmp(info->name,VO_EC_SATURATION))
		    {
			has_value=1;
			port_value = info->value;
		    }
		    else
		    if(!strcmp(attributes[i].name,"XV_CONTRAST") && !strcmp(info->name,VO_EC_CONTRAST))
		    {
			has_value=1;
			port_value = info->value;
		    }
		    else
#if 0
/*  We may safely skip this parameter since NVidia driver has default == min
    for XV_HUE but not mid. IMHO it's meaningless against RGB. */
		    if(!strcmp(attributes[i].name,"XV_HUE") && !strcmp(info->name,VO_EC_HUE))
		    {
			has_value=1;
			port_value = info->value;
		    }
		    else
#endif
                    /* Note: since 22.01.2002 GATOS supports these attrs for radeons (NK) */
		    if(!strcmp(attributes[i].name,"XV_RED_INTENSITY") && !strcmp(info->name,VO_EC_RED_INTENSITY))
		    {
			has_value=1;
			port_value = info->value;
		    }
		    else
		    if(!strcmp(attributes[i].name,"XV_GREEN_INTENSITY") && !strcmp(info->name,VO_EC_GREEN_INTENSITY))
		    {
			has_value=1;
			port_value = info->value;
		    }
		    else
		    if(!strcmp(attributes[i].name,"XV_BLUE_INTENSITY") && !strcmp(info->name,VO_EC_BLUE_INTENSITY))
		    {
			has_value=1;
			port_value = info->value;
		    }
		    else continue;
                    if(has_value)
		    {
			port_min = xv_min;
			port_max = xv_max;
			port_mid = (port_min + port_max) / 2;
			port_value = port_mid + (port_value * (port_max - port_min)) / 2000;
			MSG_DBG2("vo_xv: set gamma %s to %i (min %i max %i mid %i)\n",attributes[i].name,port_value,port_min,port_max,port_mid);
			XvSetPortAttribute(mDisplay, xv_port, xv_atomka, port_value);
			return 0;
		    }
                }
        }
    }
    return 1;
}

static int __FASTCALL__ xv_get_video_eq( vo_videq_t *info)
{
 XvAttribute *attributes;
 int howmany, xv_min,xv_max,xv_atomka;
/* get available attributes */
     attributes = XvQueryPortAttributes(mDisplay, xv_port, &howmany);
     for (i = 0; (int)i < howmany && attributes; i++)
     {
            if (attributes[i].flags & XvGettable)
            {
                xv_min = attributes[i].min_value;
                xv_max = attributes[i].max_value;
                xv_atomka = XInternAtom(mDisplay, attributes[i].name, True);
/* since we have SET_DEFAULTS first in our list, we can check if it's available
   then trigger it if it's ok so that the other values are at default upon query */
                if (xv_atomka != None)
                {
		    int port_value,port_min,port_max,port_mid,has_value=0;;
                    XvGetPortAttribute(mDisplay, xv_port, xv_atomka, &port_value);
		    MSG_DBG2("vo_xv: get: %s = %i\n",attributes[i].name,port_value);

		    port_min = xv_min;
		    port_max = xv_max;
		    port_mid = (port_min + port_max) / 2;		    
		    port_value = ((port_value - port_mid)*2000)/(port_max-port_min);
		    
		    MSG_DBG2("vo_xv: assume: %s = %i\n",attributes[i].name,port_value);
		    
		    if(!strcmp(attributes[i].name,"XV_BRIGHTNESS") && !strcmp(info->name,VO_EC_BRIGHTNESS))
		    {
			has_value=1;
			port_value = info->value;
		    }
		    else
		    if(!strcmp(attributes[i].name,"XV_SATURATION") && !strcmp(info->name,VO_EC_SATURATION))
		    {
			has_value=1;
			port_value = info->value;
		    }
		    else
		    if(!strcmp(attributes[i].name,"XV_CONTRAST") && !strcmp(info->name,VO_EC_CONTRAST))
		    {
			has_value=1;
			port_value = info->value;
		    }
		    else
		    if(!strcmp(attributes[i].name,"XV_HUE") && !strcmp(info->name,VO_EC_HUE))
		    {
			has_value=1;
			port_value = info->value;
		    }
		    else
                    /* Note: since 22.01.2002 GATOS supports these attrs for radeons (NK) */
		    if(!strcmp(attributes[i].name,"XV_RED_INTENSITY") && !strcmp(info->name,VO_EC_RED_INTENSITY))
		    {
			has_value=1;
			port_value = info->value;
		    }
		    else
		    if(!strcmp(attributes[i].name,"XV_GREEN_INTENSITY") && !strcmp(info->name,VO_EC_GREEN_INTENSITY))
		    {
			has_value=1;
			port_value = info->value;
		    }
		    else
		    if(!strcmp(attributes[i].name,"XV_BLUE_INTENSITY") && !strcmp(info->name,VO_EC_BLUE_INTENSITY))
		    {
			has_value=1;
			port_value = info->value;
		    }
		    else continue;
		    if(has_value) return 1;
                }
        }
    }
    return 0;
}

static void set_gamma_correction( void )
{
  vo_videq_t info;
  /* try all */
  xv_reset_video_eq();
  info.name=VO_EC_BRIGHTNESS;
  info.value=vo_gamma_brightness;
  xv_set_video_eq(&info);
  info.name=VO_EC_CONTRAST;
  info.value=vo_gamma_contrast;
  xv_set_video_eq(&info);
  info.name=VO_EC_SATURATION;
  info.value=vo_gamma_saturation;
  xv_set_video_eq(&info);
  info.name=VO_EC_HUE;
  info.value=vo_gamma_hue;
  xv_set_video_eq(&info);
  info.name=VO_EC_RED_INTENSITY;
  info.value=vo_gamma_red_intensity;
  xv_set_video_eq(&info);
  info.name=VO_EC_GREEN_INTENSITY;
  info.value=vo_gamma_green_intensity;
  xv_set_video_eq(&info);
  info.name=VO_EC_BLUE_INTENSITY;
  info.value=vo_gamma_blue_intensity;
  xv_set_video_eq(&info);
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
static uint32_t __FASTCALL__ config(uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t flags, char *title, uint32_t format,const vo_tune_info_t *info)
{
// int screen;
 char *hello = (title == NULL) ? "Xv render" : title;
// char *name = ":0.0";
 XSizeHints hint;
 XVisualInfo vinfo;
 XvPortID xv_p;

 XGCValues xgcv;
 XSetWindowAttributes xswa;
 unsigned long xswamask,i;
#ifdef HAVE_XF86VM
 int vm=0;
#endif

 UNUSED(info);
 aspect_save_orig(width,height);
 aspect_save_prescale(d_width,d_height);

 image_height = height;
 image_width = width;
 image_format=format;

 vo_fs=flags&VOFLAG_FULLSCREEN;
 softzoom=flags&VOFLAG_SWSCALE;
 if ( vo_fs )
  { vo_old_width=d_width; vo_old_height=d_height; }
     
#ifdef HAVE_XF86VM
 if( flags&0x02 ) vm = 1;
#endif
 flip_flag=flags&VOFLAG_FLIPPING;
 num_buffers=vo_doublebuffering?vo_da_buffs:1;
 

 aspect_save_screenres(vo_screenwidth,vo_screenheight);

   aspect(&d_width,&d_height,softzoom?A_ZOOM:A_NOZOOM);
#ifdef X11_FULLSCREEN
     /* this code replaces X11_FULLSCREEN hack in mplayer.c
      * aspect() is available through aspect.h for all vos.
      * besides zooming should only be done with -zoom,
      * but I leave the old -fs behaviour so users don't get
      * irritated for now (and send lots o' mails ;) ::atmos
      */

     if( vo_fs ) aspect(&d_width,&d_height,A_ZOOM);
#endif
   vo_x11_calcpos(&hint,d_width,d_height,flags);
   hint.flags = PPosition | PSize;

   dwidth=d_width; dheight=d_height; //XXX: what are the copy vars used for?
   XGetWindowAttributes(mDisplay, DefaultRootWindow(mDisplay), &attribs);
   depth=attribs.depth;
   if (depth != 15 && depth != 16 && depth != 24 && depth != 32) depth = 24;
   XMatchVisualInfo(mDisplay, mScreen, depth, TrueColor, &vinfo);

   xswa.background_pixel = 0;
   xswa.border_pixel     = 0;
   xswamask = CWBackPixel | CWBorderPixel;

    if ( WinID>=0 ){
      vo_window = WinID ? ((Window)WinID) : RootWindow(mDisplay,mScreen);
      XUnmapWindow( mDisplay,vo_window );
      XChangeWindowAttributes( mDisplay,vo_window,xswamask,&xswa );
    } else 

   vo_window = XCreateWindow(mDisplay, RootWindow(mDisplay,mScreen),
       hint.x, hint.y, hint.width, hint.height,
       0, depth,CopyFromParent,vinfo.visual,xswamask,&xswa);

   vo_x11_classhint( mDisplay,vo_window,"xv" );
   vo_hidecursor(mDisplay,vo_window);

   XSelectInput(mDisplay, vo_window, StructureNotifyMask | KeyPressMask | 
	((WinID==0) ? 0 : (PointerMotionMask
		| ButtonPressMask | ButtonReleaseMask
   )));
   XSetStandardProperties(mDisplay, vo_window, hello, hello, None, NULL, 0, &hint);
   if ( vo_fs ) vo_x11_decoration( mDisplay,vo_window,0 );
   XMapWindow(mDisplay, vo_window);
#ifdef HAVE_XINERAMA
   vo_x11_xinerama_move(mDisplay,vo_window);
#endif
   vo_gc = XCreateGC(mDisplay, vo_window, 0L, &xgcv);
   XFlush(mDisplay);
   XSync(mDisplay, False);
#ifdef HAVE_XF86VM
    if ( vm )
     {
      /* Grab the mouse pointer in our window */
      XGrabPointer(mDisplay, vo_window, True, 0,
                   GrabModeAsync, GrabModeAsync,
                   vo_window, None, CurrentTime);
      XSetInputFocus(mDisplay, vo_window, RevertToNone, CurrentTime);
     }
#endif

 xv_port = 0;
 if (Success == XvQueryExtension(mDisplay,&ver,&rel,&req,&ev,&err))
  {
   /* check for Xvideo support */
   if (Success != XvQueryAdaptors(mDisplay,DefaultRootWindow(mDisplay), &adaptors,&ai))
    {
     MSG_ERR("Xv: XvQueryAdaptors failed");
     return -1;
    }
   /* check adaptors */
   for (i = 0; i < adaptors && xv_port == 0; i++)
    {
     if ((ai[i].type & XvInputMask) && (ai[i].type & XvImageMask))
	 for (xv_p = ai[i].base_id; xv_p < ai[i].base_id+ai[i].num_ports; ++xv_p)
	 {
	     if (!XvGrabPort(mDisplay, xv_p, CurrentTime)) {
		 xv_port = xv_p;
		 break;
	     } else {
		 MSG_ERR("Xv: could not grab port %i\n", (int)xv_p);
	     }
	 }
    }
   /* check image formats */
   if (xv_port != 0)
    {
     fo = XvListImageFormats(mDisplay, xv_port, (int*)&formats);
     xv_format=0;
     if(format==IMGFMT_BGR32) format=FOURCC_RGBA32;
     if(format==IMGFMT_BGR16) format=FOURCC_RGB16;
     for(i = 0; i < formats; i++){
       MSG_V("Xvideo image format: 0x%x (%4.4s) %s\n", fo[i].id,(char*)&fo[i].id, (fo[i].format == XvPacked) ? "packed" : "planar");
       if (fo[i].id == (int)format) xv_format = fo[i].id;
     }
     if (!xv_format) xv_port = 0;
    }

   if (xv_port != 0)
    {
     MSG_V( "using Xvideo port %d for hw scaling\n",xv_port );
       
       switch (xv_format){
	case IMGFMT_IF09:
	case IMGFMT_YVU9:
			  xv_bpp=9;
			  break;
	case IMGFMT_YV12:
	case IMGFMT_I420:
        case IMGFMT_IYUV:
			  xv_bpp=12;
			  break;
	case IMGFMT_YUY2:
	case IMGFMT_YVYU:
			  xv_bpp=16;
			  break;	
	case IMGFMT_UYVY:
			  xv_bpp=16;
			  break;
	case FOURCC_RGBA32:
			  xv_bpp=32;
			  break;
	case FOURCC_RGB16:
			  xv_bpp=16;
			  break;
	default:
			  xv_bpp = 16;
       }

      for(i=0;i<num_buffers;++i) allocate_xvimage(i);

     set_gamma_correction();

     XGetGeometry( mDisplay,vo_window,&mRoot,&drwX,&drwY,&drwWidth,&drwHeight,&drwBorderWidth,&drwDepth );
     drwX=0; drwY=0;
     XTranslateCoordinates( mDisplay,vo_window,mRoot,0,0,&drwcX,&drwcY,&mRoot );
     MSG_V( "[xv] dcx: %d dcy: %d dx: %d dy: %d dw: %d dh: %d\n",drwcX,drwcY,drwX,drwY,drwWidth,drwHeight );

     aspect(&dwidth,&dheight,softzoom?A_ZOOM:A_NOZOOM);
     if ( vo_fs )
      {
       aspect(&dwidth,&dheight,A_ZOOM);
       drwX=( vo_screenwidth - (dwidth > vo_screenwidth?vo_screenwidth:dwidth) ) / 2;
       drwcX+=drwX;
       drwY=( vo_screenheight - (dheight > vo_screenheight?vo_screenheight:dheight) ) / 2;
       drwcY+=drwY;
       drwWidth=(dwidth > vo_screenwidth?vo_screenwidth:dwidth);
       drwHeight=(dheight > vo_screenheight?vo_screenheight:dheight);
       MSG_V( "[xv-fs] dcx: %d dcy: %d dx: %d dy: %d dw: %d dh: %d\n",drwcX,drwcY,drwX,drwY,drwWidth,drwHeight );
      }
     saver_off(mDisplay);  // turning off screen saver
     return 0;
    }
  }

 MSG_FATAL("Sorry, Xv not supported by this X11 version/driver\n");
 MSG_FATAL("******** Try with  -vo x11  or  -vo sdl  *********\n");
 return 1;
}

static const vo_info_t * get_info(void)
{ return &vo_info; }

static void __FASTCALL__ allocate_xvimage(int foo)
{
 /*
  * allocate XvImages.  FIXME: no error checking, without
  * mit-shm this will bomb...
  */
 xvimage[foo] = XvShmCreateImage(mDisplay, xv_port, xv_format, 0, image_width, image_height, &Shminfo[foo]);

 Shminfo[foo].shmid    = shmget(IPC_PRIVATE, xvimage[foo]->data_size, IPC_CREAT | 0777);
 Shminfo[foo].shmaddr  = (char *) shmat(Shminfo[foo].shmid, 0, 0);
 Shminfo[foo].readOnly = False;

 xvimage[foo]->data = Shminfo[foo].shmaddr;
 XShmAttach(mDisplay, &Shminfo[foo]);
 XSync(mDisplay, False);
 shmctl(Shminfo[foo].shmid, IPC_RMID, 0);
 memset(xvimage[foo]->data,128,xvimage[foo]->data_size);
 return;
}

static void __FASTCALL__ deallocate_xvimage(int foo)
{
 XShmDetach( mDisplay,&Shminfo[foo] );
 shmdt( Shminfo[foo].shmaddr );
 XFlush( mDisplay );
 XSync(mDisplay, False);
 return;
}

static uint32_t __FASTCALL__ check_events(int (* __FASTCALL__ adjust_size)(unsigned cw,unsigned ch,unsigned *w,unsigned *h))
{
 uint32_t e=vo_x11_check_events(mDisplay,adjust_size);
 if(e&VO_EVENT_RESIZE)
  {
   XGetGeometry( mDisplay,vo_window,&mRoot,&drwX,&drwY,&drwWidth,&drwHeight,&drwBorderWidth,&drwDepth );
   drwX=0; drwY=0;
   XTranslateCoordinates( mDisplay,vo_window,mRoot,0,0,&drwcX,&drwcY,&mRoot );
   MSG_V( "[xv-resize] dcx: %d dcy: %d dx: %d dy: %d dw: %d dh: %d\n",drwcX,drwcY,drwX,drwY,drwWidth,drwHeight );

   aspect(&dwidth,&dheight,softzoom?A_ZOOM:A_NOZOOM);
   if ( vo_fs )
    {
     aspect(&dwidth,&dheight,A_ZOOM);
     drwX=( vo_screenwidth - (dwidth > vo_screenwidth?vo_screenwidth:dwidth) ) / 2;
     drwcX+=drwX;
     drwY=( vo_screenheight - (dheight > vo_screenheight?vo_screenheight:dheight) ) / 2;
     drwcY+=drwY;
     drwWidth=(dwidth > vo_screenwidth?vo_screenwidth:dwidth);
     drwHeight=(dheight > vo_screenheight?vo_screenheight:dheight);
     MSG_V( "[xv-fs] dcx: %d dcy: %d dx: %d dy: %d dw: %d dh: %d\n",drwcX,drwcY,drwX,drwY,drwWidth,drwHeight );
    }
  }
 if ( e & VO_EVENT_EXPOSE )
  {
   XvShmPutImage(mDisplay, xv_port, vo_window, vo_gc, xvimage[expose_idx], 0, 0,  image_width, image_height, drwX, drwY, 1, 1, False);
   XvShmPutImage(mDisplay, xv_port, vo_window, vo_gc, xvimage[expose_idx], 0, 0,  image_width, image_height, drwX,drwY,drwWidth,(vo_fs?drwHeight - 1:drwHeight), False);
  }
  return e|VO_EVENT_FORCE_UPDATE;
}

static void __FASTCALL__ flip_page(unsigned idx)
{
 expose_idx=idx;
 XvShmPutImage(mDisplay, xv_port, vo_window, vo_gc, xvimage[idx],
         0, 0,  image_width, image_height,
         drwX,drwY,drwWidth,(vo_fs?drwHeight - 1:drwHeight),
         False);
 if (num_buffers>1) XFlush(mDisplay);
 else XSync(mDisplay, False);
 return;
}

static uint32_t __FASTCALL__ query_format(vo_query_fourcc_t* format)
{
 XvPortID xv_p;
 if (!vo_x11_init()) return 0;
 xv_port = 0;
 if (Success == XvQueryExtension(mDisplay,&ver,&rel,&req,&ev,&err))
  {
   /* check for Xvideo support */
   if (Success != XvQueryAdaptors(mDisplay,DefaultRootWindow(mDisplay), &adaptors,&ai))
    {
     MSG_ERR("Xv: XvQueryAdaptors failed");
     return -1;
    }
   /* check adaptors */
   for (i = 0; i < adaptors && xv_port == 0; i++)
    {
     if ((ai[i].type & XvInputMask) && (ai[i].type & XvImageMask))
	 for (xv_p = ai[i].base_id; xv_p < ai[i].base_id+ai[i].num_ports; ++xv_p)
	 {
	     if (!XvGrabPort(mDisplay, xv_p, CurrentTime)) {
		 xv_port = xv_p;
		 break;
	     } else {
		 MSG_ERR("Xv: could not grab port %i\n", (int)xv_p);
	     }
	 }
    }
   /* check image formats */
   if (xv_port != 0)
    {
     fo = XvListImageFormats(mDisplay, xv_port, (int*)&formats);
     xv_format=0;
     for(i = 0; i < formats; i++){
       if(fo[i].id == (int)format->fourcc) return 1;
       if(fo[i].id == FOURCC_RGBA32 && format->fourcc == IMGFMT_BGR32) return 1;
       if(fo[i].id == FOURCC_RGB16 && format->fourcc == IMGFMT_BGR16) return 1;
     }
     if (!xv_format) xv_port = 0;
    }
  }
return 0;
}

static void uninit(void) 
{
 unsigned i;
 saver_on(mDisplay); // screen saver back on
 for( i=0;i<num_buffers;i++ ) deallocate_xvimage( i );
#ifdef HAVE_XF86VM
 vo_vm_close(mDisplay);
#endif
 vo_x11_uninit(mDisplay, vo_window);
}

static uint32_t __FASTCALL__ preinit(const char *arg)
{
    if(arg) 
    {
	MSG_ERR("vo_xv: Unknown subdevice: %s\n",arg);
	return ENOSYS;
    }
    return 0;
}

#ifndef max
#define max(a,b) ((a)>(b)?(a):(b))
#endif
#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif

static void __FASTCALL__ xv_dri_get_surface_caps(dri_surface_cap_t *caps)
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
    n=xvimage[0]?min(4,xvimage[0]->num_planes):1;
    if(xvimage[0]) {
	for(i=0;i<n;i++)
	    caps->strides[i] = xvimage[0]->pitches[i];
    }
    for(;i<4;i++)
	caps->strides[i] = 0;
    {
	unsigned ts;
	ts = caps->strides[2];
	caps->strides[2] = caps->strides[1];
	caps->strides[1] = ts;
    }
}

static void __FASTCALL__ xv_dri_get_surface(dri_surface_t *surf)
{
    unsigned i,n;
    n=min(4,xvimage[0]->num_planes);
    for(i=0;i<n;i++)
	surf->planes[i] = xvimage[surf->idx]->data + xvimage[surf->idx]->offsets[i];
    for(;i<4;i++)
	surf->planes[i] = 0;
    {
	void * tp;
	tp = surf->planes[2];
	surf->planes[2] = surf->planes[1];
	surf->planes[1] = tp;
    }
}

static uint32_t __FASTCALL__ control(uint32_t request, void *data)
{
  switch (request) {
  case VOCTRL_QUERY_FORMAT:
    return query_format((vo_query_fourcc_t*)data);
  case VOCTRL_FULLSCREEN:
    vo_x11_fullscreen();
    return VO_TRUE;
  case VOCTRL_CHECK_EVENTS:
    {
     vo_resize_t * vrest = (vo_resize_t *)data;
     vrest->event_type = check_events(vrest->adjust_size);
     if(enable_xp && (vrest->event_type & VO_EVENT_RESIZE)==VO_EVENT_RESIZE)
		    LOCK_VDECODING(); /* just for compatibility with other vo */
     return VO_TRUE;
    }
  case VOCTRL_GET_NUM_FRAMES:
	*(uint32_t *)data = num_buffers;
	return VO_TRUE;
  case DRI_GET_SURFACE_CAPS:
	xv_dri_get_surface_caps(data);
	return VO_TRUE;
  case DRI_GET_SURFACE: 
	xv_dri_get_surface(data);
	return VO_TRUE;
  case VOCTRL_SET_EQUALIZER:
	if(!xv_set_video_eq(data)) return VO_TRUE;
	return VO_FALSE;
  case VOCTRL_GET_EQUALIZER:
	if(xv_get_video_eq(data)) return VO_TRUE;
	return VO_FALSE;
  }
  return VO_NOTIMPL;
}
