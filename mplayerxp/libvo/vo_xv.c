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

#include "mp_config.h"
#include "mplayerxp.h"
#include "xmpcore/xmp_core.h"
#include "osdep/mplib.h"
#include "video_out.h"
#include "video_out_internal.h"

LIBVO_EXTERN(xv)

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>
#include <errno.h>

#include "x11_common.h"

#include "osdep/fastmemcpy.h"
#include "sub.h"
#include "aspect.h"
#include "dri_vo.h"
#include "xmpcore/mp_image.h"

#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
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
typedef struct priv_s {
    uint32_t		image_width;
    uint32_t		image_height;
    uint32_t		image_format;
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
}priv_t;

static int __FASTCALL__ xv_reset_video_eq(vo_data_t*vo)
{
    priv_t*priv=(priv_t*)vo->priv;
    unsigned i;
    XvAttribute *attributes;
    int howmany,xv_atomka;
    static int was_reset = 0;
 /* get available attributes */
    attributes = XvQueryPortAttributes(vo->mDisplay, priv->port, &howmany);
    /* first pass try reset */
    for (i = 0; (int)i < howmany && attributes; i++) {
	if (attributes[i].flags & XvSettable && !strcmp(attributes[i].name,"XV_SET_DEFAULTS")) {
	    was_reset = 1;
	    MSG_DBG2("vo_xv: reset gamma correction\n");
	    xv_atomka = XInternAtom(vo->mDisplay, attributes[i].name, True);
	    XvSetPortAttribute(vo->mDisplay, priv->port, xv_atomka, attributes[i].max_value);
	}
    }
    return was_reset;
}

static int __FASTCALL__ xv_set_video_eq(vo_data_t*vo,const vo_videq_t *info)
{
    priv_t*priv=(priv_t*)vo->priv;
    unsigned i;
    XvAttribute *attributes;
    int howmany, xv_min,xv_max,xv_atomka;
 /* get available attributes */
    attributes = XvQueryPortAttributes(vo->mDisplay, priv->port, &howmany);
    for (i = 0; (int)i < howmany && attributes; i++) {
        if (attributes[i].flags & XvSettable) {
	    xv_min = attributes[i].min_value;
	    xv_max = attributes[i].max_value;
	    xv_atomka = XInternAtom(vo->mDisplay, attributes[i].name, True);
	    /* since we have SET_DEFAULTS first in our list, we can check if it's available
	    then trigger it if it's ok so that the other values are at default upon query */
	    if (xv_atomka != None) {
		int port_value,port_min,port_max,port_mid,has_value=0;
		if(!strcmp(attributes[i].name,"XV_BRIGHTNESS") && !strcmp(info->name,VO_EC_BRIGHTNESS)) {
		    has_value=1;
		    port_value = info->value;
		} else if(!strcmp(attributes[i].name,"XV_SATURATION") && !strcmp(info->name,VO_EC_SATURATION)) {
		    has_value=1;
		    port_value = info->value;
		} else if(!strcmp(attributes[i].name,"XV_CONTRAST") && !strcmp(info->name,VO_EC_CONTRAST)) {
		    has_value=1;
		    port_value = info->value;
		} else
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
		    if(!strcmp(attributes[i].name,"XV_RED_INTENSITY") && !strcmp(info->name,VO_EC_RED_INTENSITY)) {
		    has_value=1;
		    port_value = info->value;
		} else if(!strcmp(attributes[i].name,"XV_GREEN_INTENSITY") && !strcmp(info->name,VO_EC_GREEN_INTENSITY)) {
		    has_value=1;
		    port_value = info->value;
		} else if(!strcmp(attributes[i].name,"XV_BLUE_INTENSITY") && !strcmp(info->name,VO_EC_BLUE_INTENSITY)) {
		    has_value=1;
		    port_value = info->value;
		} else continue;
		if(has_value) {
		    port_min = xv_min;
		    port_max = xv_max;
		    port_mid = (port_min + port_max) / 2;
		    port_value = port_mid + (port_value * (port_max - port_min)) / 2000;
		    MSG_DBG2("vo_xv: set gamma %s to %i (min %i max %i mid %i)\n",attributes[i].name,port_value,port_min,port_max,port_mid);
		    XvSetPortAttribute(vo->mDisplay, priv->port, xv_atomka, port_value);
		    return 0;
		}
	    }
	}
    }
    return 1;
}

static int __FASTCALL__ xv_get_video_eq(vo_data_t*vo, vo_videq_t *info)
{
    priv_t*priv=(priv_t*)vo->priv;
    unsigned i;
    XvAttribute *attributes;
    int howmany, xv_min,xv_max,xv_atomka;
/* get available attributes */
    attributes = XvQueryPortAttributes(vo->mDisplay, priv->port, &howmany);
    for (i = 0; (int)i < howmany && attributes; i++) {
	if (attributes[i].flags & XvGettable) {
	    xv_min = attributes[i].min_value;
	    xv_max = attributes[i].max_value;
	    xv_atomka = XInternAtom(vo->mDisplay, attributes[i].name, True);
/* since we have SET_DEFAULTS first in our list, we can check if it's available
   then trigger it if it's ok so that the other values are at default upon query */
	    if (xv_atomka != None) {
		int port_value,port_min,port_max,port_mid,has_value=0;;
		XvGetPortAttribute(vo->mDisplay, priv->port, xv_atomka, &port_value);
		MSG_DBG2("vo_xv: get: %s = %i\n",attributes[i].name,port_value);

		port_min = xv_min;
		port_max = xv_max;
		port_mid = (port_min + port_max) / 2;
		port_value = ((port_value - port_mid)*2000)/(port_max-port_min);

		MSG_DBG2("vo_xv: assume: %s = %i\n",attributes[i].name,port_value);

		if(!strcmp(attributes[i].name,"XV_BRIGHTNESS") && !strcmp(info->name,VO_EC_BRIGHTNESS)) {
		    has_value=1;
		    port_value = info->value;
		} else if(!strcmp(attributes[i].name,"XV_SATURATION") && !strcmp(info->name,VO_EC_SATURATION)) {
		    has_value=1;
		    port_value = info->value;
		} else if(!strcmp(attributes[i].name,"XV_CONTRAST") && !strcmp(info->name,VO_EC_CONTRAST)) {
		    has_value=1;
		    port_value = info->value;
		} else if(!strcmp(attributes[i].name,"XV_HUE") && !strcmp(info->name,VO_EC_HUE)) {
		    has_value=1;
		    port_value = info->value;
		} else if(!strcmp(attributes[i].name,"XV_RED_INTENSITY") && !strcmp(info->name,VO_EC_RED_INTENSITY)) {
                    /* Note: since 22.01.2002 GATOS supports these attrs for radeons (NK) */
		    has_value=1;
		    port_value = info->value;
		} else if(!strcmp(attributes[i].name,"XV_GREEN_INTENSITY") && !strcmp(info->name,VO_EC_GREEN_INTENSITY)) {
		    has_value=1;
		    port_value = info->value;
		} else if(!strcmp(attributes[i].name,"XV_BLUE_INTENSITY") && !strcmp(info->name,VO_EC_BLUE_INTENSITY)) {
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

static void set_gamma_correction( vo_data_t*vo )
{
  vo_videq_t info;
  /* try all */
  xv_reset_video_eq(vo);
  info.name=VO_EC_BRIGHTNESS;
  info.value=vo_conf.gamma.brightness;
  xv_set_video_eq(vo,&info);
  info.name=VO_EC_CONTRAST;
  info.value=vo_conf.gamma.contrast;
  xv_set_video_eq(vo,&info);
  info.name=VO_EC_SATURATION;
  info.value=vo_conf.gamma.saturation;
  xv_set_video_eq(vo,&info);
  info.name=VO_EC_HUE;
  info.value=vo_conf.gamma.hue;
  xv_set_video_eq(vo,&info);
  info.name=VO_EC_RED_INTENSITY;
  info.value=vo_conf.gamma.red_intensity;
  xv_set_video_eq(vo,&info);
  info.name=VO_EC_GREEN_INTENSITY;
  info.value=vo_conf.gamma.green_intensity;
  xv_set_video_eq(vo,&info);
  info.name=VO_EC_BLUE_INTENSITY;
  info.value=vo_conf.gamma.blue_intensity;
  xv_set_video_eq(vo,&info);
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
static MPXP_Rc __FASTCALL__ config(vo_data_t*vo,uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t flags, char *title, uint32_t format)
{
    priv_t*priv=(priv_t*)vo->priv;
// int screen;
    char *hello = (title == NULL) ? "Xv render" : title;
// char *name = ":0.0";
    XSizeHints hint;
    XVisualInfo vinfo;
    XvPortID xv_p;

    XGCValues xgcv;
    XSetWindowAttributes xswa;
    unsigned long xswamask,i;

    aspect_save_orig(width,height);
    aspect_save_prescale(d_width,d_height);

    priv->image_height = height;
    priv->image_width = width;
    priv->image_format=format;

    if ( vo_FS(vo) ) { vo->dest.w=d_width; vo->dest.h=d_height; }

    priv->num_buffers=vo_conf.xp_buffs;

    aspect_save_screenres(vo_conf.screenwidth,vo_conf.screenheight);

    aspect(&d_width,&d_height,vo_ZOOM(vo)?A_ZOOM:A_NOZOOM);

    vo_x11_calcpos(vo,&hint,d_width,d_height,flags);
    hint.flags = PPosition | PSize;

    priv->dwidth=d_width; priv->dheight=d_height; //XXX: what are the copy vars used for?
    XGetWindowAttributes(vo->mDisplay, DefaultRootWindow(vo->mDisplay), &priv->attribs);
    priv->depth=priv->attribs.depth;
    if (priv->depth != 15 && priv->depth != 16 && priv->depth != 24 && priv->depth != 32) priv->depth = 24;
    XMatchVisualInfo(vo->mDisplay, vo->mScreen, priv->depth, TrueColor, &vinfo);

    xswa.background_pixel = 0;
    xswa.border_pixel     = 0;
    xswamask = CWBackPixel | CWBorderPixel;

    if ( vo_conf.WinID>=0 ){
	vo->window = vo_conf.WinID ? ((Window)vo_conf.WinID) : RootWindow(vo->mDisplay,vo->mScreen);
	XUnmapWindow( vo->mDisplay,vo->window );
	XChangeWindowAttributes( vo->mDisplay,vo->window,xswamask,&xswa );
    } else
	vo->window = XCreateWindow( vo->mDisplay, RootWindow(vo->mDisplay,vo->mScreen),
				    hint.x, hint.y, hint.width, hint.height,
				    0, priv->depth,CopyFromParent,vinfo.visual,xswamask,&xswa);

    vo_x11_classhint( vo->mDisplay,vo->window,"xv" );
    vo_x11_hidecursor(vo->mDisplay,vo->window);

    XSelectInput(vo->mDisplay, vo->window, StructureNotifyMask | KeyPressMask | 
	((vo_conf.WinID==0) ? 0 : (PointerMotionMask
		| ButtonPressMask | ButtonReleaseMask)));
    XSetStandardProperties(vo->mDisplay, vo->window, hello, hello, None, NULL, 0, &hint);
    if ( vo_FS(vo) ) vo_x11_decoration(vo,vo->mDisplay,vo->window,0 );
    XMapWindow(vo->mDisplay, vo->window);
#ifdef HAVE_XINERAMA
    vo_x11_xinerama_move(vo,vo->mDisplay,vo->window,&hint);
#endif
    vo->gc = XCreateGC(vo->mDisplay, vo->window, 0L, &xgcv);
    XFlush(vo->mDisplay);
    XSync(vo->mDisplay, False);
#ifdef HAVE_XF86VM
    if ( vo_VM(vo) ) {
	/* Grab the mouse pointer in our window */
	XGrabPointer(vo->mDisplay, vo->window, True, 0,
			GrabModeAsync, GrabModeAsync,
			vo->window, None, CurrentTime);
	XSetInputFocus(vo->mDisplay, vo->window, RevertToNone, CurrentTime);
    }
#endif

    priv->port = 0;
    if (Success == XvQueryExtension(vo->mDisplay,&priv->ver,&priv->rel,&priv->req,&priv->ev,&priv->err)) {
	/* check for Xvideo support */
	if (Success != XvQueryAdaptors(vo->mDisplay,DefaultRootWindow(vo->mDisplay), &priv->adaptors,&priv->ai)) {
	    MSG_ERR("Xv: XvQueryAdaptors failed");
	    return MPXP_False;
	}
	/* check priv->adaptors */
	for (i = 0; i < priv->adaptors && priv->port == 0; i++) {
	    if ((priv->ai[i].type & XvInputMask) && (priv->ai[i].type & XvImageMask))
	    for (xv_p = priv->ai[i].base_id; xv_p < priv->ai[i].base_id+priv->ai[i].num_ports; ++xv_p) {
		if (!XvGrabPort(vo->mDisplay, xv_p, CurrentTime)) {
		    priv->port = xv_p;
		    break;
		} else {
		    MSG_ERR("Xv: could not grab port %i\n", (int)xv_p);
		}
	    }
	}
	/* check image priv->formats */
	if (priv->port != 0) {
	    priv->fo = XvListImageFormats(vo->mDisplay, priv->port, (int*)&priv->formats);
	    priv->format=0;
	    if(format==IMGFMT_BGR32) format=FOURCC_RGBA32;
	    if(format==IMGFMT_BGR16) format=FOURCC_RGB16;
	    for(i = 0; i < priv->formats; i++) {
		MSG_V("Xvideo image format: 0x%x (%4.4s) %s\n", priv->fo[i].id,(char*)&priv->fo[i].id, (priv->fo[i].format == XvPacked) ? "packed" : "planar");
		if (priv->fo[i].id == (int)format) priv->format = priv->fo[i].id;
	    }
	    if (!priv->format) priv->port = 0;
	}

	if (priv->port != 0) {
	    MSG_V( "using Xvideo port %d for hw scaling\n",priv->port );

	    switch (priv->format){
		case IMGFMT_IF09:
		case IMGFMT_YVU9:
			priv->bpp=9;
			break;
		case IMGFMT_YV12:
		case IMGFMT_I420:
		case IMGFMT_IYUV:
			priv->bpp=12;
			break;
		case IMGFMT_YUY2:
		case IMGFMT_YVYU:
			priv->bpp=16;
			break;
		case IMGFMT_UYVY:
			priv->bpp=16;
			break;
		case FOURCC_RGBA32:
			priv->bpp=32;
			break;
		case FOURCC_RGB16:
			priv->bpp=16;
			break;
		default:
			priv->bpp = 16;
	    }

	    for(i=0;i<priv->num_buffers;++i) allocate_xvimage(vo,i);

	    set_gamma_correction(vo);

	    XGetGeometry( vo->mDisplay,vo->window,&priv->mRoot,&priv->drwX,&priv->drwY,&priv->drwWidth,&priv->drwHeight,&priv->drwBorderWidth,&priv->drwDepth );
	    priv->drwX=0; priv->drwY=0;
	    XTranslateCoordinates( vo->mDisplay,vo->window,priv->mRoot,0,0,&priv->drwcX,&priv->drwcY,&priv->mRoot );
	    MSG_V( "[xv] dcx: %d dcy: %d dx: %d dy: %d dw: %d dh: %d\n",priv->drwcX,priv->drwcY,priv->drwX,priv->drwY,priv->drwWidth,priv->drwHeight );

	    aspect(&priv->dwidth,&priv->dheight,vo_ZOOM(vo)?A_ZOOM:A_NOZOOM);
	    if ( vo_FS(vo) ) {
		aspect(&priv->dwidth,&priv->dheight,A_ZOOM);
		priv->drwX=( vo_conf.screenwidth - (priv->dwidth > vo_conf.screenwidth?vo_conf.screenwidth:priv->dwidth) ) / 2;
		priv->drwcX+=priv->drwX;
		priv->drwY=( vo_conf.screenheight - (priv->dheight > vo_conf.screenheight?vo_conf.screenheight:priv->dheight) ) / 2;
		priv->drwcY+=priv->drwY;
		priv->drwWidth=(priv->dwidth > vo_conf.screenwidth?vo_conf.screenwidth:priv->dwidth);
		priv->drwHeight=(priv->dheight > vo_conf.screenheight?vo_conf.screenheight:priv->dheight);
		MSG_V( "[xv-fs] dcx: %d dcy: %d dx: %d dy: %d dw: %d dh: %d\n",priv->drwcX,priv->drwcY,priv->drwX,priv->drwY,priv->drwWidth,priv->drwHeight );
	    }
	    saver_off(vo,vo->mDisplay);  // turning off screen saver
	    return MPXP_Ok;
	}
    }

    MSG_FATAL("Sorry, Xv not supported by this X11 version/driver\n");
    MSG_FATAL("******** Try with  -vo x11  or  -vo sdl  *********\n");
    return MPXP_False;
}

static const vo_info_t * get_info(vo_data_t*vo)
{
    UNUSED(vo);
    return &vo_info;
}

static void __FASTCALL__ allocate_xvimage(vo_data_t*vo,int foo)
{
    priv_t*priv=(priv_t*)vo->priv;
 /*
  * allocate XvImages.  FIXME: no error checking, without
  * mit-shm this will bomb...
  */
    priv->image[foo] = XvShmCreateImage(vo->mDisplay, priv->port, priv->format, 0, priv->image_width, priv->image_height, &priv->Shminfo[foo]);

    priv->Shminfo[foo].shmid    = shmget(IPC_PRIVATE, priv->image[foo]->data_size, IPC_CREAT | 0777);
    priv->Shminfo[foo].shmaddr  = (char *) shmat(priv->Shminfo[foo].shmid, 0, 0);
    priv->Shminfo[foo].readOnly = False;

    priv->image[foo]->data = priv->Shminfo[foo].shmaddr;
    XShmAttach(vo->mDisplay, &priv->Shminfo[foo]);
    XSync(vo->mDisplay, False);
    shmctl(priv->Shminfo[foo].shmid, IPC_RMID, 0);
    memset(priv->image[foo]->data,128,priv->image[foo]->data_size);
    return;
}

static void __FASTCALL__ deallocate_xvimage(vo_data_t*vo,int foo)
{
    priv_t*priv=(priv_t*)vo->priv;
    XShmDetach( vo->mDisplay,&priv->Shminfo[foo] );
    shmdt( priv->Shminfo[foo].shmaddr );
    XFlush( vo->mDisplay );
    XSync(vo->mDisplay, False);
    return;
}

static uint32_t __FASTCALL__ check_events(vo_data_t*vo,int (* __FASTCALL__ adjust_size)(unsigned cw,unsigned ch,unsigned *w,unsigned *h))
{
    priv_t*priv=(priv_t*)vo->priv;
    uint32_t e=vo_x11_check_events(vo,vo->mDisplay,adjust_size);
    if(e&VO_EVENT_RESIZE) {
	XGetGeometry( vo->mDisplay,vo->window,&priv->mRoot,&priv->drwX,&priv->drwY,&priv->drwWidth,&priv->drwHeight,&priv->drwBorderWidth,&priv->drwDepth );
	priv->drwX=0; priv->drwY=0;
	XTranslateCoordinates( vo->mDisplay,vo->window,priv->mRoot,0,0,&priv->drwcX,&priv->drwcY,&priv->mRoot );
	MSG_V( "[xv-resize] dcx: %d dcy: %d dx: %d dy: %d dw: %d dh: %d\n",priv->drwcX,priv->drwcY,priv->drwX,priv->drwY,priv->drwWidth,priv->drwHeight );

	aspect(&priv->dwidth,&priv->dheight,vo_ZOOM(vo)?A_ZOOM:A_NOZOOM);
	if ( vo_FS(vo) ) {
	    aspect(&priv->dwidth,&priv->dheight,A_ZOOM);
	    priv->drwX=( vo_conf.screenwidth - (priv->dwidth > vo_conf.screenwidth?vo_conf.screenwidth:priv->dwidth) ) / 2;
	    priv->drwcX+=priv->drwX;
	    priv->drwY=( vo_conf.screenheight - (priv->dheight > vo_conf.screenheight?vo_conf.screenheight:priv->dheight) ) / 2;
	    priv->drwcY+=priv->drwY;
	    priv->drwWidth=(priv->dwidth > vo_conf.screenwidth?vo_conf.screenwidth:priv->dwidth);
	    priv->drwHeight=(priv->dheight > vo_conf.screenheight?vo_conf.screenheight:priv->dheight);
	    MSG_V( "[xv-fs] dcx: %d dcy: %d dx: %d dy: %d dw: %d dh: %d\n",priv->drwcX,priv->drwcY,priv->drwX,priv->drwY,priv->drwWidth,priv->drwHeight );
	}
    }
    if ( e & VO_EVENT_EXPOSE ) {
	XvShmPutImage(vo->mDisplay, priv->port, vo->window, vo->gc, priv->image[priv->expose_idx], 0, 0,  priv->image_width, priv->image_height, priv->drwX, priv->drwY, 1, 1, False);
	XvShmPutImage(vo->mDisplay, priv->port, vo->window, vo->gc, priv->image[priv->expose_idx], 0, 0,  priv->image_width, priv->image_height, priv->drwX,priv->drwY,priv->drwWidth,(vo_FS(vo)?priv->drwHeight - 1:priv->drwHeight), False);
    }
    return e|VO_EVENT_FORCE_UPDATE;
}

static void __FASTCALL__ select_frame(vo_data_t*vo,unsigned idx)
{
    priv_t*priv=(priv_t*)vo->priv;
    priv->expose_idx=idx;
    XvShmPutImage(vo->mDisplay, priv->port, vo->window, vo->gc, priv->image[idx],
	0, 0,  priv->image_width, priv->image_height,
	priv->drwX,priv->drwY,priv->drwWidth,(vo_FS(vo)?priv->drwHeight - 1:priv->drwHeight),
	False);
    if (priv->num_buffers>1) XFlush(vo->mDisplay);
    else XSync(vo->mDisplay, False);
    return;
}

static uint32_t __FASTCALL__ query_format(vo_data_t*vo,vo_query_fourcc_t* format)
{
    priv_t*priv=(priv_t*)vo->priv;
    unsigned i;
    XvPortID xv_p;
    if (vo_x11_init(vo)!=MPXP_Ok) return 0;
    priv->port = 0;
    if (Success == XvQueryExtension(vo->mDisplay,&priv->ver,&priv->rel,&priv->req,&priv->ev,&priv->err)) {
	/* check for Xvideo support */
	if (Success != XvQueryAdaptors(vo->mDisplay,DefaultRootWindow(vo->mDisplay), &priv->adaptors,&priv->ai)) {
	    MSG_ERR("Xv: XvQueryAdaptors failed");
	    return -1;
	}
	/* check priv->adaptors */
	for (i = 0; i < priv->adaptors && priv->port == 0; i++) {
	    if ((priv->ai[i].type & XvInputMask) && (priv->ai[i].type & XvImageMask))
		for (xv_p = priv->ai[i].base_id; xv_p < priv->ai[i].base_id+priv->ai[i].num_ports; ++xv_p) {
		    if (!XvGrabPort(vo->mDisplay, xv_p, CurrentTime)) {
			priv->port = xv_p;
			break;
		    } else {
			MSG_ERR("Xv: could not grab port %i\n", (int)xv_p);
		    }
		}
	}
	/* check image priv->formats */
	if (priv->port != 0) {
	    priv->fo = XvListImageFormats(vo->mDisplay, priv->port, (int*)&priv->formats);
	    priv->format=0;
	    for(i = 0; i < priv->formats; i++) {
		if(priv->fo[i].id == (int)format->fourcc) return 1;
		if(priv->fo[i].id == FOURCC_RGBA32 && format->fourcc == IMGFMT_BGR32) return 1;
		if(priv->fo[i].id == FOURCC_RGB16 && format->fourcc == IMGFMT_BGR16) return 1;
	    }
	    if (!priv->format) priv->port = 0;
	}
    }
    return 0;
}

static void uninit(vo_data_t*vo)
{
    priv_t*priv=(priv_t*)vo->priv;
    unsigned i;
    saver_on(vo,vo->mDisplay); // screen saver back on
    for( i=0;i<priv->num_buffers;i++ ) deallocate_xvimage(vo,i);
#ifdef HAVE_XF86VM
    vo_vm_close(vo,vo->mDisplay);
#endif
    vo_x11_uninit(vo,vo->mDisplay, vo->window);
    mp_free(vo->priv);
}

static MPXP_Rc __FASTCALL__ preinit(vo_data_t*vo,const char *arg)
{
    vo->priv=mp_mallocz(sizeof(priv_t));
    priv_t*priv=(priv_t*)vo->priv;
    priv->num_buffers=1;
    if(arg) {
	MSG_ERR("vo_xv: Unknown subdevice: %s\n",arg);
	return MPXP_False;
    }
    return MPXP_Ok;
}

#ifndef max
#define max(a,b) ((a)>(b)?(a):(b))
#endif
#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif

static void __FASTCALL__ xv_dri_get_surface_caps(vo_data_t*vo,dri_surface_cap_t *caps)
{
    priv_t*priv=(priv_t*)vo->priv;
    unsigned i,n;
    caps->caps = DRI_CAP_TEMP_VIDEO | DRI_CAP_UPSCALER | DRI_CAP_DOWNSCALER |
		DRI_CAP_HORZSCALER | DRI_CAP_VERTSCALER;
    caps->fourcc = priv->image_format;
    caps->width=priv->image_width;
    caps->height=priv->image_height;
    caps->x=0;
    caps->y=0;
    caps->w=priv->image_width;
    caps->h=priv->image_height;
    n=priv->image[0]?min(4,priv->image[0]->num_planes):1;
    if(priv->image[0]) {
	for(i=0;i<n;i++)
	    caps->strides[i] = priv->image[0]->pitches[i];
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

static void __FASTCALL__ xv_dri_get_surface(vo_data_t*vo,dri_surface_t *surf)
{
    priv_t*priv=(priv_t*)vo->priv;
    unsigned i,n;
    n=min(4,priv->image[0]->num_planes);
    for(i=0;i<n;i++)
	surf->planes[i] = priv->image[surf->idx]->data + priv->image[surf->idx]->offsets[i];
    for(;i<4;i++)
	surf->planes[i] = 0;
    {
	any_t* tp;
	tp = surf->planes[2];
	surf->planes[2] = surf->planes[1];
	surf->planes[1] = tp;
    }
}

static MPXP_Rc __FASTCALL__ control(vo_data_t*vo,uint32_t request, any_t*data)
{
    priv_t*priv=(priv_t*)vo->priv;
  switch (request) {
  case VOCTRL_QUERY_FORMAT:
    return query_format(vo,(vo_query_fourcc_t*)data);
  case VOCTRL_FULLSCREEN:
    vo_x11_fullscreen(vo);
    return MPXP_True;
  case VOCTRL_CHECK_EVENTS:
    {
     vo_resize_t * vrest = (vo_resize_t *)data;
     vrest->event_type = check_events(vo,vrest->adjust_size);
     return MPXP_True;
    }
  case VOCTRL_GET_NUM_FRAMES:
	*(uint32_t *)data = priv->num_buffers;
	return MPXP_True;
  case DRI_GET_SURFACE_CAPS:
	xv_dri_get_surface_caps(vo,data);
	return MPXP_True;
  case DRI_GET_SURFACE:
	xv_dri_get_surface(vo,data);
	return MPXP_True;
  case VOCTRL_SET_EQUALIZER:
	if(!xv_set_video_eq(vo,data)) return MPXP_True;
	return MPXP_False;
  case VOCTRL_GET_EQUALIZER:
	if(xv_get_video_eq(vo,data)) return MPXP_True;
	return MPXP_False;
  }
  return MPXP_NA;
}
