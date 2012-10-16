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

#include "../mp_config.h"
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

#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>

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
static void __FASTCALL__ allocate_xvimage(int);

/* X11 related variables */
typedef struct voxv_priv_s {
    uint32_t		image_width;
    uint32_t		image_height;
    uint32_t		image_format;
    int			flip_flag;
    unsigned		depth;

    XWindowAttributes	attribs;
    XvAdaptorInfo*	ai;
    XvImageFormatValues*fo;

    unsigned int	ver,rel,req,ev,err;
    unsigned int	formats, adaptors, port, format, bpp;

    unsigned		expose_idx,num_buffers; // 1 - default
    XvImage*		image[MAX_DRI_BUFFERS];
    XShmSegmentInfo	Shminfo[MAX_DRI_BUFFERS];

    Window		mRoot;
    uint32_t		drwX,drwY,drwWidth,drwHeight,drwBorderWidth,drwDepth;
    uint32_t		drwcX,drwcY,dwidth,dheight;
}voxv_priv_t;
static voxv_priv_t voxv;

static int __FASTCALL__ xv_reset_video_eq(void)
{
 unsigned i;
 XvAttribute *attributes;
 int howmany,xv_atomka;
 static int was_reset = 0;
 /* get available attributes */
    attributes = XvQueryPortAttributes(vo.mDisplay, voxv.port, &howmany);
    /* first pass try reset */
    for (i = 0; (int)i < howmany && attributes; i++)
    {
	if (attributes[i].flags & XvSettable && !strcmp(attributes[i].name,"XV_SET_DEFAULTS"))
	{
		was_reset = 1;
		MSG_DBG2("vo_xv: reset gamma correction\n");
		xv_atomka = XInternAtom(vo.mDisplay, attributes[i].name, True);
		XvSetPortAttribute(vo.mDisplay, voxv.port, xv_atomka, attributes[i].max_value);
	}
    }
    return was_reset;
}

static int __FASTCALL__ xv_set_video_eq(const vo_videq_t *info)
{
 unsigned i;
 XvAttribute *attributes;
 int howmany, xv_min,xv_max,xv_atomka;
 /* get available attributes */
    attributes = XvQueryPortAttributes(vo.mDisplay, voxv.port, &howmany);
    for (i = 0; (int)i < howmany && attributes; i++)
    {
            if (attributes[i].flags & XvSettable)
            {
                xv_min = attributes[i].min_value;
                xv_max = attributes[i].max_value;
                xv_atomka = XInternAtom(vo.mDisplay, attributes[i].name, True);
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
			XvSetPortAttribute(vo.mDisplay, voxv.port, xv_atomka, port_value);
			return 0;
		    }
                }
        }
    }
    return 1;
}

static int __FASTCALL__ xv_get_video_eq( vo_videq_t *info)
{
 unsigned i;
 XvAttribute *attributes;
 int howmany, xv_min,xv_max,xv_atomka;
/* get available attributes */
     attributes = XvQueryPortAttributes(vo.mDisplay, voxv.port, &howmany);
     for (i = 0; (int)i < howmany && attributes; i++)
     {
            if (attributes[i].flags & XvGettable)
            {
                xv_min = attributes[i].min_value;
                xv_max = attributes[i].max_value;
                xv_atomka = XInternAtom(vo.mDisplay, attributes[i].name, True);
/* since we have SET_DEFAULTS first in our list, we can check if it's available
   then trigger it if it's ok so that the other values are at default upon query */
                if (xv_atomka != None)
                {
		    int port_value,port_min,port_max,port_mid,has_value=0;;
                    XvGetPortAttribute(vo.mDisplay, voxv.port, xv_atomka, &port_value);
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
  info.value=vo.gamma.brightness;
  xv_set_video_eq(&info);
  info.name=VO_EC_CONTRAST;
  info.value=vo.gamma.contrast;
  xv_set_video_eq(&info);
  info.name=VO_EC_SATURATION;
  info.value=vo.gamma.saturation;
  xv_set_video_eq(&info);
  info.name=VO_EC_HUE;
  info.value=vo.gamma.hue;
  xv_set_video_eq(&info);
  info.name=VO_EC_RED_INTENSITY;
  info.value=vo.gamma.red_intensity;
  xv_set_video_eq(&info);
  info.name=VO_EC_GREEN_INTENSITY;
  info.value=vo.gamma.green_intensity;
  xv_set_video_eq(&info);
  info.name=VO_EC_BLUE_INTENSITY;
  info.value=vo.gamma.blue_intensity;
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

 voxv.image_height = height;
 voxv.image_width = width;
 voxv.image_format=format;

 vo.fs=flags&VOFLAG_FULLSCREEN;
 vo.softzoom=flags&VOFLAG_SWSCALE;
 if ( vo.fs )
  { vo.prev.w=d_width; vo.prev.h=d_height; }

#ifdef HAVE_XF86VM
 if( flags&0x02 ) vm = 1;
#endif
 voxv.flip_flag=flags&VOFLAG_FLIPPING;
 voxv.num_buffers=vo.doublebuffering?vo.da_buffs:1;


 aspect_save_screenres(vo.screenwidth,vo.screenheight);

 aspect(&d_width,&d_height,vo.softzoom?A_ZOOM:A_NOZOOM);
 if( vo.fs ) aspect(&d_width,&d_height,A_ZOOM);

   vo_x11_calcpos(&hint,d_width,d_height,flags);
   hint.flags = PPosition | PSize;

   voxv.dwidth=d_width; voxv.dheight=d_height; //XXX: what are the copy vars used for?
   XGetWindowAttributes(vo.mDisplay, DefaultRootWindow(vo.mDisplay), &voxv.attribs);
   voxv.depth=voxv.attribs.depth;
   if (voxv.depth != 15 && voxv.depth != 16 && voxv.depth != 24 && voxv.depth != 32) voxv.depth = 24;
   XMatchVisualInfo(vo.mDisplay, vo.mScreen, voxv.depth, TrueColor, &vinfo);

   xswa.background_pixel = 0;
   xswa.border_pixel     = 0;
   xswamask = CWBackPixel | CWBorderPixel;

    if ( vo.WinID>=0 ){
      vo.window = vo.WinID ? ((Window)vo.WinID) : RootWindow(vo.mDisplay,vo.mScreen);
      XUnmapWindow( vo.mDisplay,vo.window );
      XChangeWindowAttributes( vo.mDisplay,vo.window,xswamask,&xswa );
    } else

   vo.window = XCreateWindow(vo.mDisplay, RootWindow(vo.mDisplay,vo.mScreen),
       hint.x, hint.y, hint.width, hint.height,
       0, voxv.depth,CopyFromParent,vinfo.visual,xswamask,&xswa);

   vo_x11_classhint( vo.mDisplay,vo.window,"xv" );
   vo_x11_hidecursor(vo.mDisplay,vo.window);

   XSelectInput(vo.mDisplay, vo.window, StructureNotifyMask | KeyPressMask | 
	((vo.WinID==0) ? 0 : (PointerMotionMask
		| ButtonPressMask | ButtonReleaseMask
   )));
   XSetStandardProperties(vo.mDisplay, vo.window, hello, hello, None, NULL, 0, &hint);
   if ( vo.fs ) vo_x11_decoration( vo.mDisplay,vo.window,0 );
   XMapWindow(vo.mDisplay, vo.window);
#ifdef HAVE_XINERAMA
   vo_x11_xinerama_move(vo.mDisplay,vo.window,&hint);
#endif
   vo.gc = XCreateGC(vo.mDisplay, vo.window, 0L, &xgcv);
   XFlush(vo.mDisplay);
   XSync(vo.mDisplay, False);
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

 voxv.port = 0;
 if (Success == XvQueryExtension(vo.mDisplay,&voxv.ver,&voxv.rel,&voxv.req,&voxv.ev,&voxv.err))
  {
   /* check for Xvideo support */
   if (Success != XvQueryAdaptors(vo.mDisplay,DefaultRootWindow(vo.mDisplay), &voxv.adaptors,&voxv.ai))
    {
     MSG_ERR("Xv: XvQueryAdaptors failed");
     return -1;
    }
   /* check voxv.adaptors */
   for (i = 0; i < voxv.adaptors && voxv.port == 0; i++)
    {
     if ((voxv.ai[i].type & XvInputMask) && (voxv.ai[i].type & XvImageMask))
	 for (xv_p = voxv.ai[i].base_id; xv_p < voxv.ai[i].base_id+voxv.ai[i].num_ports; ++xv_p)
	 {
	     if (!XvGrabPort(vo.mDisplay, xv_p, CurrentTime)) {
		 voxv.port = xv_p;
		 break;
	     } else {
		 MSG_ERR("Xv: could not grab port %i\n", (int)xv_p);
	     }
	 }
    }
   /* check image voxv.formats */
   if (voxv.port != 0)
    {
     voxv.fo = XvListImageFormats(vo.mDisplay, voxv.port, (int*)&voxv.formats);
     voxv.format=0;
     if(format==IMGFMT_BGR32) format=FOURCC_RGBA32;
     if(format==IMGFMT_BGR16) format=FOURCC_RGB16;
     for(i = 0; i < voxv.formats; i++){
       MSG_V("Xvideo image format: 0x%x (%4.4s) %s\n", voxv.fo[i].id,(char*)&voxv.fo[i].id, (voxv.fo[i].format == XvPacked) ? "packed" : "planar");
       if (voxv.fo[i].id == (int)format) voxv.format = voxv.fo[i].id;
     }
     if (!voxv.format) voxv.port = 0;
    }

   if (voxv.port != 0)
    {
     MSG_V( "using Xvideo port %d for hw scaling\n",voxv.port );
       
       switch (voxv.format){
	case IMGFMT_IF09:
	case IMGFMT_YVU9:
			  voxv.bpp=9;
			  break;
	case IMGFMT_YV12:
	case IMGFMT_I420:
        case IMGFMT_IYUV:
			  voxv.bpp=12;
			  break;
	case IMGFMT_YUY2:
	case IMGFMT_YVYU:
			  voxv.bpp=16;
			  break;	
	case IMGFMT_UYVY:
			  voxv.bpp=16;
			  break;
	case FOURCC_RGBA32:
			  voxv.bpp=32;
			  break;
	case FOURCC_RGB16:
			  voxv.bpp=16;
			  break;
	default:
			  voxv.bpp = 16;
       }

      for(i=0;i<voxv.num_buffers;++i) allocate_xvimage(i);

     set_gamma_correction();

     XGetGeometry( vo.mDisplay,vo.window,&voxv.mRoot,&voxv.drwX,&voxv.drwY,&voxv.drwWidth,&voxv.drwHeight,&voxv.drwBorderWidth,&voxv.drwDepth );
     voxv.drwX=0; voxv.drwY=0;
     XTranslateCoordinates( vo.mDisplay,vo.window,voxv.mRoot,0,0,&voxv.drwcX,&voxv.drwcY,&voxv.mRoot );
     MSG_V( "[xv] dcx: %d dcy: %d dx: %d dy: %d dw: %d dh: %d\n",voxv.drwcX,voxv.drwcY,voxv.drwX,voxv.drwY,voxv.drwWidth,voxv.drwHeight );

     aspect(&voxv.dwidth,&voxv.dheight,vo.softzoom?A_ZOOM:A_NOZOOM);
     if ( vo.fs )
      {
       aspect(&voxv.dwidth,&voxv.dheight,A_ZOOM);
       voxv.drwX=( vo.screenwidth - (voxv.dwidth > vo.screenwidth?vo.screenwidth:voxv.dwidth) ) / 2;
       voxv.drwcX+=voxv.drwX;
       voxv.drwY=( vo.screenheight - (voxv.dheight > vo.screenheight?vo.screenheight:voxv.dheight) ) / 2;
       voxv.drwcY+=voxv.drwY;
       voxv.drwWidth=(voxv.dwidth > vo.screenwidth?vo.screenwidth:voxv.dwidth);
       voxv.drwHeight=(voxv.dheight > vo.screenheight?vo.screenheight:voxv.dheight);
       MSG_V( "[xv-fs] dcx: %d dcy: %d dx: %d dy: %d dw: %d dh: %d\n",voxv.drwcX,voxv.drwcY,voxv.drwX,voxv.drwY,voxv.drwWidth,voxv.drwHeight );
      }
     saver_off(vo.mDisplay);  // turning off screen saver
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
 voxv.image[foo] = XvShmCreateImage(vo.mDisplay, voxv.port, voxv.format, 0, voxv.image_width, voxv.image_height, &voxv.Shminfo[foo]);

 voxv.Shminfo[foo].shmid    = shmget(IPC_PRIVATE, voxv.image[foo]->data_size, IPC_CREAT | 0777);
 voxv.Shminfo[foo].shmaddr  = (char *) shmat(voxv.Shminfo[foo].shmid, 0, 0);
 voxv.Shminfo[foo].readOnly = False;

 voxv.image[foo]->data = voxv.Shminfo[foo].shmaddr;
 XShmAttach(vo.mDisplay, &voxv.Shminfo[foo]);
 XSync(vo.mDisplay, False);
 shmctl(voxv.Shminfo[foo].shmid, IPC_RMID, 0);
 memset(voxv.image[foo]->data,128,voxv.image[foo]->data_size);
 return;
}

static void __FASTCALL__ deallocate_xvimage(int foo)
{
 XShmDetach( vo.mDisplay,&voxv.Shminfo[foo] );
 shmdt( voxv.Shminfo[foo].shmaddr );
 XFlush( vo.mDisplay );
 XSync(vo.mDisplay, False);
 return;
}

static uint32_t __FASTCALL__ check_events(int (* __FASTCALL__ adjust_size)(unsigned cw,unsigned ch,unsigned *w,unsigned *h))
{
 uint32_t e=vo_x11_check_events(vo.mDisplay,adjust_size);
 if(e&VO_EVENT_RESIZE)
  {
   XGetGeometry( vo.mDisplay,vo.window,&voxv.mRoot,&voxv.drwX,&voxv.drwY,&voxv.drwWidth,&voxv.drwHeight,&voxv.drwBorderWidth,&voxv.drwDepth );
   voxv.drwX=0; voxv.drwY=0;
   XTranslateCoordinates( vo.mDisplay,vo.window,voxv.mRoot,0,0,&voxv.drwcX,&voxv.drwcY,&voxv.mRoot );
   MSG_V( "[xv-resize] dcx: %d dcy: %d dx: %d dy: %d dw: %d dh: %d\n",voxv.drwcX,voxv.drwcY,voxv.drwX,voxv.drwY,voxv.drwWidth,voxv.drwHeight );

   aspect(&voxv.dwidth,&voxv.dheight,vo.softzoom?A_ZOOM:A_NOZOOM);
   if ( vo.fs )
    {
     aspect(&voxv.dwidth,&voxv.dheight,A_ZOOM);
     voxv.drwX=( vo.screenwidth - (voxv.dwidth > vo.screenwidth?vo.screenwidth:voxv.dwidth) ) / 2;
     voxv.drwcX+=voxv.drwX;
     voxv.drwY=( vo.screenheight - (voxv.dheight > vo.screenheight?vo.screenheight:voxv.dheight) ) / 2;
     voxv.drwcY+=voxv.drwY;
     voxv.drwWidth=(voxv.dwidth > vo.screenwidth?vo.screenwidth:voxv.dwidth);
     voxv.drwHeight=(voxv.dheight > vo.screenheight?vo.screenheight:voxv.dheight);
     MSG_V( "[xv-fs] dcx: %d dcy: %d dx: %d dy: %d dw: %d dh: %d\n",voxv.drwcX,voxv.drwcY,voxv.drwX,voxv.drwY,voxv.drwWidth,voxv.drwHeight );
    }
  }
 if ( e & VO_EVENT_EXPOSE )
  {
   XvShmPutImage(vo.mDisplay, voxv.port, vo.window, vo.gc, voxv.image[voxv.expose_idx], 0, 0,  voxv.image_width, voxv.image_height, voxv.drwX, voxv.drwY, 1, 1, False);
   XvShmPutImage(vo.mDisplay, voxv.port, vo.window, vo.gc, voxv.image[voxv.expose_idx], 0, 0,  voxv.image_width, voxv.image_height, voxv.drwX,voxv.drwY,voxv.drwWidth,(vo.fs?voxv.drwHeight - 1:voxv.drwHeight), False);
  }
  return e|VO_EVENT_FORCE_UPDATE;
}

static void __FASTCALL__ change_frame(unsigned idx)
{
 voxv.expose_idx=idx;
 XvShmPutImage(vo.mDisplay, voxv.port, vo.window, vo.gc, voxv.image[idx],
         0, 0,  voxv.image_width, voxv.image_height,
         voxv.drwX,voxv.drwY,voxv.drwWidth,(vo.fs?voxv.drwHeight - 1:voxv.drwHeight),
         False);
 if (voxv.num_buffers>1) XFlush(vo.mDisplay);
 else XSync(vo.mDisplay, False);
 return;
}

static uint32_t __FASTCALL__ query_format(vo_query_fourcc_t* format)
{
 unsigned i;
 XvPortID xv_p;
 if (!vo_x11_init()) return 0;
 voxv.port = 0;
 if (Success == XvQueryExtension(vo.mDisplay,&voxv.ver,&voxv.rel,&voxv.req,&voxv.ev,&voxv.err))
  {
   /* check for Xvideo support */
   if (Success != XvQueryAdaptors(vo.mDisplay,DefaultRootWindow(vo.mDisplay), &voxv.adaptors,&voxv.ai))
    {
     MSG_ERR("Xv: XvQueryAdaptors failed");
     return -1;
    }
   /* check voxv.adaptors */
   for (i = 0; i < voxv.adaptors && voxv.port == 0; i++)
    {
     if ((voxv.ai[i].type & XvInputMask) && (voxv.ai[i].type & XvImageMask))
	 for (xv_p = voxv.ai[i].base_id; xv_p < voxv.ai[i].base_id+voxv.ai[i].num_ports; ++xv_p)
	 {
	     if (!XvGrabPort(vo.mDisplay, xv_p, CurrentTime)) {
		 voxv.port = xv_p;
		 break;
	     } else {
		 MSG_ERR("Xv: could not grab port %i\n", (int)xv_p);
	     }
	 }
    }
   /* check image voxv.formats */
   if (voxv.port != 0)
    {
     voxv.fo = XvListImageFormats(vo.mDisplay, voxv.port, (int*)&voxv.formats);
     voxv.format=0;
     for(i = 0; i < voxv.formats; i++){
       if(voxv.fo[i].id == (int)format->fourcc) return 1;
       if(voxv.fo[i].id == FOURCC_RGBA32 && format->fourcc == IMGFMT_BGR32) return 1;
       if(voxv.fo[i].id == FOURCC_RGB16 && format->fourcc == IMGFMT_BGR16) return 1;
     }
     if (!voxv.format) voxv.port = 0;
    }
  }
return 0;
}

static void uninit(void)
{
 unsigned i;
 saver_on(vo.mDisplay); // screen saver back on
 for( i=0;i<voxv.num_buffers;i++ ) deallocate_xvimage( i );
#ifdef HAVE_XF86VM
 vo_vm_close(vo.mDisplay);
#endif
 vo_x11_uninit(vo.mDisplay, vo.window);
}

static uint32_t __FASTCALL__ preinit(const char *arg)
{
    memset(&voxv,0,sizeof(voxv_priv_t));
    voxv.num_buffers=1;
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
    caps->fourcc = voxv.image_format;
    caps->width=voxv.image_width;
    caps->height=voxv.image_height;
    caps->x=0;
    caps->y=0;
    caps->w=voxv.image_width;
    caps->h=voxv.image_height;
    n=voxv.image[0]?min(4,voxv.image[0]->num_planes):1;
    if(voxv.image[0]) {
	for(i=0;i<n;i++)
	    caps->strides[i] = voxv.image[0]->pitches[i];
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
    n=min(4,voxv.image[0]->num_planes);
    for(i=0;i<n;i++)
	surf->planes[i] = voxv.image[surf->idx]->data + voxv.image[surf->idx]->offsets[i];
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
	*(uint32_t *)data = voxv.num_buffers;
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
