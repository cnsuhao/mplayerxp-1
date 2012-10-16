/*
    VIDIX accelerated overlay in a X window
    
    (C) Alex Beregszaszi & Zoltan Ponekker & Nickols_K
    
    WS window manager by Pontscho/Fresh!

    Based on vo_gl.c and vo_vesa.c and vo_xmga.c (.so mastah! ;))
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>

#include "mp_config.h"
#include "../mplayer.h"
#include "../dec_ahead.h"
#include "video_out.h"
#include "video_out_internal.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
//#include <X11/keysym.h>

#ifdef HAVE_XINERAMA
#include <X11/extensions/Xinerama.h>
#endif

#include "x11_common.h"
#include "aspect.h"

#include "vosub_vidix.h"
#include <vidix/vidixlib.h>
#include "vo_msg.h"

LIBVO_EXTERN(xvidix)

static vo_info_t vo_info =
{
    "X11 (VIDIX)",
    "xvidix",
    "Alex Beregszaszi",
    ""
};

#define UNUSED(x) ((void)(x)) /* Removes warning about unused arguments */

typedef struct xvidix_priv_s {
/* Image parameters */
    uint32_t		image_width;
    uint32_t		image_height;
    uint32_t		image_format;
    uint32_t		image_depth;
/* Window parameters */
    uint32_t		win_x, win_y, win_w, win_h;
/* X11 related variables */
    int			X_already_started;
    Window		mRoot;
    uint32_t		drwX, drwY, drwWidth, drwHeight, drwBorderWidth;
    uint32_t		drwDepth, drwcX, drwcY, dwidth, dheight;
/* Colorkey handling */
    XGCValues		mGCV;
    uint32_t		fgColor;
/* VIDIX related */
    char *		name;
    vo_tune_info_t	vtune;
}xvidix_priv_t;
static xvidix_priv_t xvidix;

static uint32_t __FASTCALL__ set_window(int force_update,const vo_tune_info_t *info)
{
    uint32_t retval=0;
    XGetGeometry(vo.mDisplay, vo.window, &xvidix.mRoot, &xvidix.drwX, &xvidix.drwY, &xvidix.drwWidth,
	&xvidix.drwHeight, &xvidix.drwBorderWidth, &xvidix.drwDepth);
    xvidix.drwX = xvidix.drwY = 0;
    XTranslateCoordinates(vo.mDisplay, vo.window, xvidix.mRoot, 0, 0,
	&xvidix.drwcX, &xvidix.drwcY, &xvidix.mRoot);

    if (!vo.fs)
	MSG_V( "[xvidix] dcx: %d dcy: %d dx: %d dy: %d dw: %d dh: %d\n",
	    xvidix.drwcX, xvidix.drwcY, xvidix.drwX, xvidix.drwY, xvidix.drwWidth, xvidix.drwHeight);

    /* following stuff copied from vo_xmga.c */
    if (vo.fs)
    {
	xvidix.drwX = (vo.screenwidth - (xvidix.dwidth > vo.screenwidth ? vo.screenwidth : xvidix.dwidth)) / 2;
	xvidix.drwcX += xvidix.drwX;
	xvidix.drwY = (vo.screenheight - (xvidix.dheight > vo.screenheight ? vo.screenheight : xvidix.dheight)) / 2;
	xvidix.drwcY += xvidix.drwY;
	xvidix.drwWidth = (xvidix.dwidth > vo.screenwidth ? vo.screenwidth : xvidix.dwidth);
	xvidix.drwHeight = (xvidix.dheight > vo.screenheight ? vo.screenheight : xvidix.dheight);
	MSG_V( "[xvidix-fs] dcx: %d dcy: %d dx: %d dy: %d dw: %d dh: %d\n",
	    xvidix.drwcX, xvidix.drwcY, xvidix.drwX, xvidix.drwY, xvidix.drwWidth, xvidix.drwHeight);
    }

#ifdef HAVE_XINERAMA
    if (XineramaIsActive(vo.mDisplay))
    {
	XineramaScreenInfo *screens;
	int num_screens;
	int i = 0;
	
	screens = XineramaQueryScreens(vo.mDisplay, &num_screens);
	
	/* find the screen we are on */
	while (((unsigned)screens[i].x_org <= xvidix.drwcX) || ((unsigned)screens[i].y_org <= xvidix.drwcY) ||
		((unsigned)screens[i].x_org + (unsigned)screens[i].width >= xvidix.drwcX) ||
		((unsigned)screens[i].y_org + (unsigned)screens[i].height >= xvidix.drwcY))
	    i++;

	/* set xvidix.drwcX and xvidix.drwcY to the right values */
	xvidix.drwcX = xvidix.drwcX - screens[i].x_org;
	xvidix.drwcY = xvidix.drwcY - screens[i].y_org;
	XFree(screens);
    }
#endif

    /* set new values in VIDIX */
    if (force_update || (xvidix.win_x != xvidix.drwcX) || (xvidix.win_y != xvidix.drwcY) ||
	(xvidix.win_w != xvidix.drwWidth) || (xvidix.win_h != xvidix.drwHeight))
    {
	retval = VO_EVENT_RESIZE;
	if(enable_xp && !force_update) LOCK_VDECODING();
	xvidix.win_x = xvidix.drwcX;
	xvidix.win_y = xvidix.drwcY;
	xvidix.win_w = xvidix.drwWidth;
	xvidix.win_h = xvidix.drwHeight;

	/* FIXME: implement runtime resize/move if possible, this way is very ugly! */
	vidix_stop();
	if (vidix_init(xvidix.image_width, xvidix.image_height, xvidix.win_x, xvidix.win_y,
	    xvidix.win_w, xvidix.win_h, xvidix.image_format, vo.depthonscreen,
	    vo.screenwidth, vo.screenheight,info) != 0)
        {
	    MSG_FATAL( "Can't initialize VIDIX driver: %s: %s\n",
		xvidix.name, strerror(errno));
	    vidix_term();
	    uninit();
    	    exit(1); /* !!! */
	}
	if(vidix_start()!=0) { uninit(); exit(1); }
    }
    
    MSG_V( "[xvidix] window properties: pos: %dx%d, size: %dx%d\n",
	xvidix.win_x, xvidix.win_y, xvidix.win_w, xvidix.win_h);

    /* mDrawColorKey: */

    /* fill drawable with specified color */
    XSetBackground( vo.mDisplay,vo.gc,0 );
    XClearWindow( vo.mDisplay,vo.window );
    XSetForeground(vo.mDisplay, vo.gc, xvidix.fgColor);
    XFillRectangle(vo.mDisplay, vo.window, vo.gc, xvidix.drwX, xvidix.drwY, xvidix.drwWidth,
	(vo.fs ? xvidix.drwHeight - 1 : xvidix.drwHeight));
    /* flush, update drawable */
    XFlush(vo.mDisplay);

    return retval;
}

/* connect to server, create and map window,
 * allocate colors and (shared) memory
 */
static uint32_t __FASTCALL__ config(uint32_t width, uint32_t height, uint32_t d_width,
    uint32_t d_height, uint32_t flags, char *title, uint32_t format,const vo_tune_info_t *info)
{
    XVisualInfo vinfo;
    XSizeHints hint;
    XSetWindowAttributes xswa;
    unsigned long xswamask;
    XWindowAttributes attribs;
    int window_depth;
    vo_query_fourcc_t qfourcc;
//    if (title)
//	free(title);
    title = strdup("MPlayerXP VIDIX X11 Overlay");

    xvidix.image_height = height;
    xvidix.image_width = width;
    xvidix.image_format = format;

    if ((IMGFMT_IS_RGB(format) || IMGFMT_IS_BGR(format)) && rgbfmt_depth(format)<48)
    {
	xvidix.image_depth = rgbfmt_depth(format);
    }
    else
    switch(format)
    {
	case IMGFMT_YVU9:
	case IMGFMT_IF09:
	    xvidix.image_depth = 9;
	    break;
	case IMGFMT_IYUV:
	case IMGFMT_I420:
	case IMGFMT_YV12:
	    xvidix.image_depth = 12;
	    break;
	case IMGFMT_UYVY:
	case IMGFMT_YUY2:
	    xvidix.image_depth = 16;
	    break;
	default:
	    xvidix.image_depth = 16;
	    MSG_FATAL( "Unknown image format: %s\n",
		vo_format_name(format));
	    break;
    }

    if (!vo_x11_init())
    {
	MSG_ERR("vo_x11_init failed\n");
        return -1;
    }
    aspect_save_orig(width, height);
    aspect_save_prescale(d_width, d_height);
    aspect_save_screenres(vo.screenwidth, vo.screenheight);

    xvidix.win_x = 0;
    xvidix.win_y = 0;
    xvidix.win_w = d_width;
    xvidix.win_h = d_height;

    vo.fs = flags&0x01;
    if (vo.fs)
     { vo.prev.w=d_width; vo.prev.h=d_height; }

    xvidix.X_already_started++;

    /* from xmga.c */
    switch(vo.depthonscreen)
    {
	case 32:
	case 24:
	    xvidix.fgColor = 0x00ff00ffL;
	    break;
	case 16:
	    xvidix.fgColor = 0xf81fL;
	    break;
	case 15:
	    xvidix.fgColor = 0x7c1fL;
	    break;
	default:
	    MSG_ERR( "Sorry, this (%d) color depth is not supported\n",
		vo.depthonscreen);
    }

    aspect(&d_width, &d_height,flags & VOFLAG_SWSCALE?A_ZOOM:A_NOZOOM);

    if (vo.fs) /* fullscreen */
    {
	if (flags & VOFLAG_SWSCALE)
	{
	    aspect(&d_width, &d_height, A_ZOOM);
	}
	else
	{
	    d_width = vo.screenwidth;
	    d_height = vo.screenheight;
	}
	xvidix.win_w = vo.screenwidth;
	xvidix.win_h = vo.screenheight;
    }

    xvidix.dwidth = d_width;
    xvidix.dheight = d_height;
    /* Make the window */
    XGetWindowAttributes(vo.mDisplay, DefaultRootWindow(vo.mDisplay), &attribs);

    /* from vo_x11 */
    window_depth = attribs.depth;
    if ((window_depth != 15) && (window_depth != 16) && (window_depth != 24)
	&& (window_depth != 32))
        window_depth = 24;
    XMatchVisualInfo(vo.mDisplay, vo.mScreen, window_depth, TrueColor, &vinfo);

    xswa.background_pixel = vo.fs ? BlackPixel(vo.mDisplay, vo.mScreen) : xvidix.fgColor;
    xswa.border_pixel     = 0;
    xswa.colormap         = XCreateColormap(vo.mDisplay, RootWindow(vo.mDisplay, vo.mScreen),
					    vinfo.visual, AllocNone);
    xswa.event_mask = StructureNotifyMask | ExposureMask | KeyPressMask | ButtonPressMask | ButtonReleaseMask;
    xswamask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;

    if (vo.WinID >= 0)
    {
	vo.window = vo.WinID ? ((Window)vo.WinID) : RootWindow(vo.mDisplay, vo.mScreen);
	XUnmapWindow(vo.mDisplay, vo.window);
	XChangeWindowAttributes(vo.mDisplay, vo.window, xswamask, &xswa);
    }
    else
	vo.window = XCreateWindow(vo.mDisplay, RootWindow(vo.mDisplay, vo.mScreen),
	    xvidix.win_x, xvidix.win_y, xvidix.win_w, xvidix.win_h, xswa.border_pixel,
	    vinfo.depth, InputOutput, vinfo.visual, xswamask, &xswa);

    vo_x11_classhint(vo.mDisplay, vo.window, "xvidix");
    vo_x11_hidecursor(vo.mDisplay, vo.window);

    if (vo.fs) vo_x11_decoration(vo.mDisplay, vo.window, 0);

    XGetNormalHints(vo.mDisplay, vo.window, &hint);
    hint.x = xvidix.win_x;
    hint.y = xvidix.win_y;
    hint.base_width = hint.width = xvidix.win_w;
    hint.base_height = hint.height = xvidix.win_h;
    hint.flags = USPosition | USSize;
    XSetNormalHints(vo.mDisplay, vo.window, &hint);

    XStoreName(vo.mDisplay, vo.window, title);
    /* Map window. */

    XMapWindow(vo.mDisplay, vo.window);
#ifdef HAVE_XINERAMA
    vo_x11_xinerama_move(vo.mDisplay, vo.window,&hint);
#endif

    vo.gc = XCreateGC(vo.mDisplay, vo.window, GCForeground, &xvidix.mGCV);

    XSelectInput( vo.mDisplay,vo.window,StructureNotifyMask | KeyPressMask | ButtonPressMask | ButtonReleaseMask );

    MSG_V( "[xvidix] image properties: %dx%d depth: %d\n",
	xvidix.image_width, xvidix.image_height, xvidix.image_depth);
    /* stupid call which requires to correct color key support */
    qfourcc.fourcc=format;
    qfourcc.w=width;
    qfourcc.h=height;
    vidix_query_fourcc(&qfourcc);
    if (vidix_grkey_support())
    {
#ifndef CONFIG_VIDIX
	vidix_grkey_t gr_key_l;
#endif
	vidix_grkey_t *gr_key;
#ifdef CONFIG_VIDIX
	gr_key = vdlAllocGrKeyS();
#else
	gr_key = &gr_key_l;
#endif
	vidix_grkey_get(gr_key);
	gr_key->key_op = KEYS_PUT;
	gr_key->ckey.op = CKEY_TRUE;
	gr_key->ckey.red = 255;
	gr_key->ckey.green = 0;
	gr_key->ckey.blue = 255;
	vidix_grkey_set(gr_key);
#ifdef CONFIG_VIDIX
	vdlFreeGrKeyS(gr_key);
#endif
    }

    set_window(1,info);
    if(info) memcpy(&xvidix.vtune,info,sizeof(vo_tune_info_t));
    else     memset(&xvidix.vtune,0,sizeof(vo_tune_info_t));
    XFlush(vo.mDisplay);
    XSync(vo.mDisplay, False);

    saver_off(vo.mDisplay); /* turning off screen saver */

    return(0);
}

static const vo_info_t *get_info(void)
{
    return(&vo_info);
}

static uint32_t __FASTCALL__ check_events(int (* __FASTCALL__ adjust_size)(unsigned cw,unsigned ch,unsigned *w,unsigned *h))
{
    uint32_t event = vo_x11_check_events(vo.mDisplay,adjust_size);
    if((event & VO_EVENT_RESIZE))
	event = set_window(0,&xvidix.vtune);
    return event;
}

/* change_frame should be overwritten with vidix functions (vosub_vidix.c) */

static void __FASTCALL__ change_frame(unsigned idx)
{
    UNUSED(idx);
    MSG_FATAL( "[xvidix] error: didn't used vidix change_frame!\n");
    return;
}

static uint32_t __FASTCALL__ query_format(vo_query_fourcc_t* format)
{
  return(vidix_query_fourcc(format));
}

static void uninit(void)
{
    vidix_term();

    saver_on(vo.mDisplay); /* screen saver back on */
    vo_x11_uninit(vo.mDisplay, vo.window);
    if(xvidix.name) { free(xvidix.name); xvidix.name = NULL; }
    xvidix.X_already_started --;
}

static uint32_t __FASTCALL__ preinit(const char *arg)
{
    memset(&xvidix,0,sizeof(xvidix_priv_t));
    if (arg)
        xvidix.name = strdup(arg);
    else
    {
	MSG_V( "No vidix driver name provided, probing available ones!\n");
	xvidix.name = NULL;
    }

    if (vidix_preinit(xvidix.name, &video_out_xvidix) != 0)
	return(1);

    return(0);
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
     return VO_TRUE;
    }
  }
  return VO_NOTIMPL;
}
