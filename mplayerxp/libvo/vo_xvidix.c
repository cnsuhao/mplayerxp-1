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

/* X11 related variables */
static int X_already_started = 0;

/* Colorkey handling */
static XGCValues mGCV;
static uint32_t	fgColor;

/* VIDIX related */
static char *vidix_name;
static vo_tune_info_t vtune;

/* Image parameters */
static uint32_t image_width;
static uint32_t image_height;
static uint32_t image_format;
static uint32_t image_depth;

/* Window parameters */
static uint32_t window_x, window_y;
static uint32_t window_width, window_height;

/* used by XGetGeometry & XTranslateCoordinates for moving/resizing window */
static Window mRoot;
static uint32_t drwX, drwY, drwWidth, drwHeight, drwBorderWidth,
    drwDepth, drwcX, drwcY, dwidth, dheight;

static uint32_t __FASTCALL__ set_window(int force_update,const vo_tune_info_t *info)
{
    uint32_t retval=0;
    XGetGeometry(mDisplay, vo_window, &mRoot, &drwX, &drwY, &drwWidth,
	&drwHeight, &drwBorderWidth, &drwDepth);
    drwX = drwY = 0;
    XTranslateCoordinates(mDisplay, vo_window, mRoot, 0, 0,
	&drwcX, &drwcY, &mRoot);

    if (!vo_fs)
	MSG_V( "[xvidix] dcx: %d dcy: %d dx: %d dy: %d dw: %d dh: %d\n",
	    drwcX, drwcY, drwX, drwY, drwWidth, drwHeight);

    /* following stuff copied from vo_xmga.c */
#if X11_FULLSCREEN
    if (vo_fs)
    {
	drwX = (vo_screenwidth - (dwidth > vo_screenwidth ? vo_screenwidth : dwidth)) / 2;
	drwcX += drwX;
	drwY = (vo_screenheight - (dheight > vo_screenheight ? vo_screenheight : dheight)) / 2;
	drwcY += drwY;
	drwWidth = (dwidth > vo_screenwidth ? vo_screenwidth : dwidth);
	drwHeight = (dheight > vo_screenheight ? vo_screenheight : dheight);
	MSG_V( "[xvidix-fs] dcx: %d dcy: %d dx: %d dy: %d dw: %d dh: %d\n",
	    drwcX, drwcY, drwX, drwY, drwWidth, drwHeight);
    }
#endif

#ifdef HAVE_XINERAMA
    if (XineramaIsActive(mDisplay))
    {
	XineramaScreenInfo *screens;
	int num_screens;
	int i = 0;
	
	screens = XineramaQueryScreens(mDisplay, &num_screens);
	
	/* find the screen we are on */
	while (((unsigned)screens[i].x_org <= drwcX) || ((unsigned)screens[i].y_org <= drwcY) ||
		((unsigned)screens[i].x_org + (unsigned)screens[i].width >= drwcX) ||
		((unsigned)screens[i].y_org + (unsigned)screens[i].height >= drwcY))
	    i++;

	/* set drwcX and drwcY to the right values */
	drwcX = drwcX - screens[i].x_org;
	drwcY = drwcY - screens[i].y_org;
	XFree(screens);
    }
#endif

    /* set new values in VIDIX */
    if (force_update || (window_x != drwcX) || (window_y != drwcY) ||
	(window_width != drwWidth) || (window_height != drwHeight))
    {
	retval = VO_EVENT_RESIZE;
	if(enable_xp && !force_update) LOCK_VDECODING();
	window_x = drwcX;
	window_y = drwcY;
	window_width = drwWidth;
	window_height = drwHeight;

	/* FIXME: implement runtime resize/move if possible, this way is very ugly! */
	vidix_stop();
	if (vidix_init(image_width, image_height, window_x, window_y,
	    window_width, window_height, image_format, vo_depthonscreen,
	    vo_screenwidth, vo_screenheight,info) != 0)
        {
	    MSG_FATAL( "Can't initialize VIDIX driver: %s: %s\n",
		vidix_name, strerror(errno));
	    vidix_term();
	    uninit();
    	    exit(1); /* !!! */
	}
	if(vidix_start()!=0) { uninit(); exit(1); }
    }
    
    MSG_V( "[xvidix] window properties: pos: %dx%d, size: %dx%d\n",
	window_x, window_y, window_width, window_height);

    /* mDrawColorKey: */

    /* fill drawable with specified color */
    XSetBackground( mDisplay,vo_gc,0 );
    XClearWindow( mDisplay,vo_window );
    XSetForeground(mDisplay, vo_gc, fgColor);
    XFillRectangle(mDisplay, vo_window, vo_gc, drwX, drwY, drwWidth,
	(vo_fs ? drwHeight - 1 : drwHeight));
    /* flush, update drawable */
    XFlush(mDisplay);

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

    image_height = height;
    image_width = width;
    image_format = format;

    if (IMGFMT_IS_RGB(format) || IMGFMT_IS_BGR(format))
    {
	image_depth = rgbfmt_depth(format);
    }
    else
    switch(format)
    {
	case IMGFMT_YVU9:
	case IMGFMT_IF09:
	    image_depth = 9;
	    break;
	case IMGFMT_IYUV:
	case IMGFMT_I420:
	case IMGFMT_YV12:
	    image_depth = 12;
	    break;
	case IMGFMT_UYVY:
	case IMGFMT_YUY2:
	    image_depth = 16;
	    break;
	default:
	    image_depth = 16;
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
    aspect_save_screenres(vo_screenwidth, vo_screenheight);

    window_x = 0;
    window_y = 0;
    window_width = d_width;
    window_height = d_height;

    vo_fs = flags&0x01;
    if (vo_fs)
     { vo_old_width=d_width; vo_old_height=d_height; }

    X_already_started++;
    
    /* from xmga.c */
    switch(vo_depthonscreen)
    {
	case 32:
	case 24:
	    fgColor = 0x00ff00ffL;
	    break;
	case 16:
	    fgColor = 0xf81fL;
	    break;
	case 15:
	    fgColor = 0x7c1fL;
	    break;
	default:
	    MSG_ERR( "Sorry, this (%d) color depth is not supported\n",
		vo_depthonscreen);
    }

    aspect(&d_width, &d_height,flags & VOFLAG_SWSCALE?A_ZOOM:A_NOZOOM);

#ifdef X11_FULLSCREEN
    if (vo_fs) /* fullscreen */
    {
        if (flags & VOFLAG_SWSCALE)
        {
    	    aspect(&d_width, &d_height, A_ZOOM);
        }
    	else
    	{
	    d_width = vo_screenwidth;
	    d_height = vo_screenheight;
    	}
	window_width = vo_screenwidth;
	window_height = vo_screenheight;
    }
#endif
    dwidth = d_width;
    dheight = d_height;
    /* Make the window */
    XGetWindowAttributes(mDisplay, DefaultRootWindow(mDisplay), &attribs);

    /* from vo_x11 */
    window_depth = attribs.depth;
    if ((window_depth != 15) && (window_depth != 16) && (window_depth != 24)
	&& (window_depth != 32))
        window_depth = 24;
    XMatchVisualInfo(mDisplay, mScreen, window_depth, TrueColor, &vinfo);

    xswa.background_pixel = vo_fs ? BlackPixel(mDisplay, mScreen) : fgColor;
    xswa.border_pixel     = 0;
    xswa.colormap         = XCreateColormap(mDisplay, RootWindow(mDisplay, mScreen),
					    vinfo.visual, AllocNone);
    xswa.event_mask = StructureNotifyMask | ExposureMask | KeyPressMask | ButtonPressMask | ButtonReleaseMask;
    xswamask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;

    if (WinID >= 0)
    {
	vo_window = WinID ? ((Window)WinID) : RootWindow(mDisplay, mScreen);
	XUnmapWindow(mDisplay, vo_window);
	XChangeWindowAttributes(mDisplay, vo_window, xswamask, &xswa);
    }
    else
	vo_window = XCreateWindow(mDisplay, RootWindow(mDisplay, mScreen),
	    window_x, window_y, window_width, window_height, xswa.border_pixel,
	    vinfo.depth, InputOutput, vinfo.visual, xswamask, &xswa);

    vo_x11_classhint(mDisplay, vo_window, "xvidix");
    vo_hidecursor(mDisplay, vo_window);

#ifdef X11_FULLSCREEN
    if (vo_fs) /* fullscreen */
	vo_x11_decoration(mDisplay, vo_window, 0);
#endif

    XGetNormalHints(mDisplay, vo_window, &hint);
    hint.x = window_x;
    hint.y = window_y;
    hint.base_width = hint.width = window_width;
    hint.base_height = hint.height = window_height;
    hint.flags = USPosition | USSize;
    XSetNormalHints(mDisplay, vo_window, &hint);

    XStoreName(mDisplay, vo_window, title);
    /* Map window. */

    XMapWindow(mDisplay, vo_window);
#ifdef HAVE_XINERAMA
    vo_x11_xinerama_move(mDisplay, vo_window);
#endif

    vo_gc = XCreateGC(mDisplay, vo_window, GCForeground, &mGCV);

    XSelectInput( mDisplay,vo_window,StructureNotifyMask | KeyPressMask | ButtonPressMask | ButtonReleaseMask );

    MSG_V( "[xvidix] image properties: %dx%d depth: %d\n",
	image_width, image_height, image_depth);
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
    if(info) memcpy(&vtune,info,sizeof(vo_tune_info_t));
    else     memset(&vtune,0,sizeof(vo_tune_info_t));
    XFlush(mDisplay);
    XSync(mDisplay, False);

    saver_off(mDisplay); /* turning off screen saver */

    return(0);
}

static const vo_info_t *get_info(void)
{
    return(&vo_info);
}

static uint32_t __FASTCALL__ check_events(int (* __FASTCALL__ adjust_size)(unsigned cw,unsigned ch,unsigned *w,unsigned *h))
{
    uint32_t event = vo_x11_check_events(mDisplay,adjust_size);
    if((event & VO_EVENT_RESIZE))
	event = set_window(0,&vtune);
    return event;
}

/* flip_page should be overwritten with vidix functions (vosub_vidix.c) */

static void __FASTCALL__ flip_page(unsigned idx)
{
    UNUSED(idx);
    MSG_FATAL( "[xvidix] error: didn't used vidix flip_page!\n");
    return;
}

static uint32_t __FASTCALL__ query_format(vo_query_fourcc_t* format)
{
  return(vidix_query_fourcc(format));
}

static void uninit(void)
{
    vidix_term();

    saver_on(mDisplay); /* screen saver back on */
    vo_x11_uninit(mDisplay, vo_window);
    if(vidix_name) { free(vidix_name); vidix_name = NULL; }
    X_already_started --;
}

static uint32_t __FASTCALL__ preinit(const char *arg)
{
    if (arg)
        vidix_name = strdup(arg);
    else
    {
	MSG_V( "No vidix driver name provided, probing available ones!\n");
	vidix_name = NULL;
    }

    if (vidix_preinit(vidix_name, &video_out_xvidix) != 0)
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
