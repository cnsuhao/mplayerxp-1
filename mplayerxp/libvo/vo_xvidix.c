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
#include "mplayer.h"
#include "osdep/mplib.h"
#include "xmp_core.h"

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

typedef struct priv_s {
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
}priv_t;

static uint32_t __FASTCALL__ set_window(vo_data_t*vo,int force_update,const vo_tune_info_t *info)
{
    priv_t*priv=(priv_t*)vo->priv;
    uint32_t retval=0;
    XGetGeometry(vo->mDisplay, vo->window, &priv->mRoot, &priv->drwX, &priv->drwY, &priv->drwWidth,
	&priv->drwHeight, &priv->drwBorderWidth, &priv->drwDepth);
    priv->drwX = priv->drwY = 0;
    XTranslateCoordinates(vo->mDisplay, vo->window, priv->mRoot, 0, 0,
	&priv->drwcX, &priv->drwcY, &priv->mRoot);

    if (!vo_FS(vo))
	MSG_V( "[xvidix] dcx: %d dcy: %d dx: %d dy: %d dw: %d dh: %d\n",
	    priv->drwcX, priv->drwcY, priv->drwX, priv->drwY, priv->drwWidth, priv->drwHeight);

    /* following stuff copied from vo_xmga.c */
    if (vo_FS(vo)) {
	priv->drwX = (vo_conf.screenwidth - (priv->dwidth > vo_conf.screenwidth ? vo_conf.screenwidth : priv->dwidth)) / 2;
	priv->drwcX += priv->drwX;
	priv->drwY = (vo_conf.screenheight - (priv->dheight > vo_conf.screenheight ? vo_conf.screenheight : priv->dheight)) / 2;
	priv->drwcY += priv->drwY;
	priv->drwWidth = (priv->dwidth > vo_conf.screenwidth ? vo_conf.screenwidth : priv->dwidth);
	priv->drwHeight = (priv->dheight > vo_conf.screenheight ? vo_conf.screenheight : priv->dheight);
	MSG_V( "[xvidix-fs] dcx: %d dcy: %d dx: %d dy: %d dw: %d dh: %d\n",
	    priv->drwcX, priv->drwcY, priv->drwX, priv->drwY, priv->drwWidth, priv->drwHeight);
    }

#ifdef HAVE_XINERAMA
    if (XineramaIsActive(vo->mDisplay))
    {
	XineramaScreenInfo *screens;
	int num_screens;
	int i = 0;
	
	screens = XineramaQueryScreens(vo->mDisplay, &num_screens);
	
	/* find the screen we are on */
	while (((unsigned)screens[i].x_org <= priv->drwcX) || ((unsigned)screens[i].y_org <= priv->drwcY) ||
		((unsigned)screens[i].x_org + (unsigned)screens[i].width >= priv->drwcX) ||
		((unsigned)screens[i].y_org + (unsigned)screens[i].height >= priv->drwcY))
	    i++;

	/* set priv->drwcX and priv->drwcY to the right values */
	priv->drwcX = priv->drwcX - screens[i].x_org;
	priv->drwcY = priv->drwcY - screens[i].y_org;
	XFree(screens);
    }
#endif

    /* set new values in VIDIX */
    if (force_update || (priv->win_x != priv->drwcX) || (priv->win_y != priv->drwcY) ||
	(priv->win_w != priv->drwWidth) || (priv->win_h != priv->drwHeight))
    {
	retval = VO_EVENT_RESIZE;
	priv->win_x = priv->drwcX;
	priv->win_y = priv->drwcY;
	priv->win_w = priv->drwWidth;
	priv->win_h = priv->drwHeight;

	/* FIXME: implement runtime resize/move if possible, this way is very ugly! */
	vidix_stop(vo);
	if (vidix_init(vo,priv->image_width, priv->image_height, priv->win_x, priv->win_y,
	    priv->win_w, priv->win_h, priv->image_format, vo->depthonscreen,
	    vo_conf.screenwidth, vo_conf.screenheight,info) != 0)
        {
	    MSG_FATAL( "Can't initialize VIDIX driver: %s: %s\n",
		priv->name, strerror(errno));
	    vidix_term(vo);
	    uninit(vo);
	    exit(1); /* !!! */
	}
	if(vidix_start(vo)!=0) { uninit(vo); exit(1); }
    }

    MSG_V( "[xvidix] window properties: pos: %dx%d, size: %dx%d\n",
	priv->win_x, priv->win_y, priv->win_w, priv->win_h);

    /* mDrawColorKey: */

    /* fill drawable with specified color */
    XSetBackground( vo->mDisplay,vo->gc,0 );
    XClearWindow( vo->mDisplay,vo->window );
    XSetForeground(vo->mDisplay, vo->gc, priv->fgColor);
    XFillRectangle(vo->mDisplay, vo->window, vo->gc, priv->drwX, priv->drwY, priv->drwWidth,
	(vo_FS(vo) ? priv->drwHeight - 1 : priv->drwHeight));
    /* flush, update drawable */
    XFlush(vo->mDisplay);

    return retval;
}

/* connect to server, create and map window,
 * allocate colors and (shared) memory
 */
static uint32_t __FASTCALL__ config(vo_data_t*vo,uint32_t width, uint32_t height, uint32_t d_width,
    uint32_t d_height, uint32_t flags, char *title, uint32_t format,const vo_tune_info_t *info)
{
    priv_t*priv=(priv_t*)vo->priv;
    XVisualInfo vinfo;
    XSizeHints hint;
    XSetWindowAttributes xswa;
    unsigned long xswamask;
    XWindowAttributes attribs;
    int window_depth;
    vo_query_fourcc_t qfourcc;
//    if (title)
//	mp_free(title);
    title = mp_strdup("MPlayerXP VIDIX X11 Overlay");

    priv->image_height = height;
    priv->image_width = width;
    priv->image_format = format;

    if ((IMGFMT_IS_RGB(format) || IMGFMT_IS_BGR(format)) && rgbfmt_depth(format)<48)
    {
	priv->image_depth = rgbfmt_depth(format);
    }
    else
    switch(format)
    {
	case IMGFMT_YVU9:
	case IMGFMT_IF09:
	    priv->image_depth = 9;
	    break;
	case IMGFMT_IYUV:
	case IMGFMT_I420:
	case IMGFMT_YV12:
	    priv->image_depth = 12;
	    break;
	case IMGFMT_UYVY:
	case IMGFMT_YUY2:
	    priv->image_depth = 16;
	    break;
	default:
	    priv->image_depth = 16;
	    MSG_FATAL( "Unknown image format: %s\n",
		vo_format_name(format));
	    break;
    }

    if (!vo_x11_init(vo))
    {
	MSG_ERR("vo_x11_init failed\n");
        return -1;
    }
    aspect_save_orig(width, height);
    aspect_save_prescale(d_width, d_height);
    aspect_save_screenres(vo_conf.screenwidth, vo_conf.screenheight);

    priv->win_x = 0;
    priv->win_y = 0;
    priv->win_w = d_width;
    priv->win_h = d_height;

    if (vo_FS(vo)) { vo->dest.w=d_width; vo->dest.h=d_height; }

    priv->X_already_started++;

    /* from xmga.c */
    switch(vo->depthonscreen)
    {
	case 32:
	case 24:
	    priv->fgColor = 0x00ff00ffL;
	    break;
	case 16:
	    priv->fgColor = 0xf81fL;
	    break;
	case 15:
	    priv->fgColor = 0x7c1fL;
	    break;
	default:
	    MSG_ERR( "Sorry, this (%d) color depth is not supported\n",
		vo->depthonscreen);
    }

    aspect(&d_width, &d_height,flags & VOFLAG_SWSCALE?A_ZOOM:A_NOZOOM);

    if (vo_FS(vo)) { /* fullscreen */
	if (vo_ZOOM(vo)) aspect(&d_width, &d_height, A_ZOOM);
	else {
	    d_width = vo_conf.screenwidth;
	    d_height = vo_conf.screenheight;
	}
	priv->win_w = vo_conf.screenwidth;
	priv->win_h = vo_conf.screenheight;
    }

    priv->dwidth = d_width;
    priv->dheight = d_height;
    /* Make the window */
    XGetWindowAttributes(vo->mDisplay, DefaultRootWindow(vo->mDisplay), &attribs);

    /* from vo_x11 */
    window_depth = attribs.depth;
    if ((window_depth != 15) && (window_depth != 16) && (window_depth != 24)
	&& (window_depth != 32))
        window_depth = 24;
    XMatchVisualInfo(vo->mDisplay, vo->mScreen, window_depth, TrueColor, &vinfo);

    xswa.background_pixel = vo_FS(vo) ? BlackPixel(vo->mDisplay, vo->mScreen) : priv->fgColor;
    xswa.border_pixel     = 0;
    xswa.colormap         = XCreateColormap(vo->mDisplay, RootWindow(vo->mDisplay, vo->mScreen),
					    vinfo.visual, AllocNone);
    xswa.event_mask = StructureNotifyMask | ExposureMask | KeyPressMask | ButtonPressMask | ButtonReleaseMask;
    xswamask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;

    if (vo_conf.WinID >= 0)
    {
	vo->window = vo_conf.WinID ? ((Window)vo_conf.WinID) : RootWindow(vo->mDisplay, vo->mScreen);
	XUnmapWindow(vo->mDisplay, vo->window);
	XChangeWindowAttributes(vo->mDisplay, vo->window, xswamask, &xswa);
    }
    else
	vo->window = XCreateWindow(vo->mDisplay, RootWindow(vo->mDisplay, vo->mScreen),
	    priv->win_x, priv->win_y, priv->win_w, priv->win_h, xswa.border_pixel,
	    vinfo.depth, InputOutput, vinfo.visual, xswamask, &xswa);

    vo_x11_classhint(vo->mDisplay, vo->window, "xvidix");
    vo_x11_hidecursor(vo->mDisplay, vo->window);

    if (vo_FS(vo)) vo_x11_decoration(vo,vo->mDisplay, vo->window, 0);

    XGetNormalHints(vo->mDisplay, vo->window, &hint);
    hint.x = priv->win_x;
    hint.y = priv->win_y;
    hint.base_width = hint.width = priv->win_w;
    hint.base_height = hint.height = priv->win_h;
    hint.flags = USPosition | USSize;
    XSetNormalHints(vo->mDisplay, vo->window, &hint);

    XStoreName(vo->mDisplay, vo->window, title);
    /* Map window. */

    XMapWindow(vo->mDisplay, vo->window);
#ifdef HAVE_XINERAMA
    vo_x11_xinerama_move(vo,vo->mDisplay, vo->window,&hint);
#endif

    vo->gc = XCreateGC(vo->mDisplay, vo->window, GCForeground, &priv->mGCV);

    XSelectInput( vo->mDisplay,vo->window,StructureNotifyMask | KeyPressMask | ButtonPressMask | ButtonReleaseMask );

    MSG_V( "[xvidix] image properties: %dx%d depth: %d\n",
	priv->image_width, priv->image_height, priv->image_depth);
    /* stupid call which requires to correct color key support */
    qfourcc.fourcc=format;
    qfourcc.w=width;
    qfourcc.h=height;
    vidix_query_fourcc(vo,&qfourcc);
    if (vidix_grkey_support(vo))
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
	vidix_grkey_get(vo,gr_key);
	gr_key->key_op = KEYS_PUT;
	gr_key->ckey.op = CKEY_TRUE;
	gr_key->ckey.red = 255;
	gr_key->ckey.green = 0;
	gr_key->ckey.blue = 255;
	vidix_grkey_set(vo,gr_key);
#ifdef CONFIG_VIDIX
	vdlFreeGrKeyS(gr_key);
#endif
    }

    set_window(vo,1,info);
    if(info) memcpy(&priv->vtune,info,sizeof(vo_tune_info_t));
    else     memset(&priv->vtune,0,sizeof(vo_tune_info_t));
    XFlush(vo->mDisplay);
    XSync(vo->mDisplay, False);

    saver_off(vo,vo->mDisplay); /* turning off screen saver */

    return(0);
}

static const vo_info_t *get_info(vo_data_t*vo)
{
    UNUSED(vo);
    return &vo_info;
}

static uint32_t __FASTCALL__ check_events(vo_data_t*vo,int (* __FASTCALL__ adjust_size)(unsigned cw,unsigned ch,unsigned *w,unsigned *h))
{
    priv_t*priv=(priv_t*)vo->priv;
    uint32_t event = vo_x11_check_events(vo,vo->mDisplay,adjust_size);
    if((event & VO_EVENT_RESIZE))
	event = set_window(vo,0,&priv->vtune);
    return event;
}

/* select_frame should be overwritten with vidix functions (vosub_vidix.c) */
static void __FASTCALL__ select_frame(vo_data_t*vo,unsigned idx)
{
    UNUSED(vo);
    UNUSED(idx);
    MSG_FATAL( "[xvidix] error: didn't used vidix select_frame!\n");
    return;
}

static uint32_t __FASTCALL__ query_format(vo_data_t*vo,vo_query_fourcc_t* format)
{
    priv_t*priv=(priv_t*)vo->priv;
    return vidix_query_fourcc(vo,format);
}

static void uninit(vo_data_t*vo)
{
    priv_t*priv=(priv_t*)vo->priv;
    vidix_term(vo);

    saver_on(vo,vo->mDisplay); /* screen saver back on */
    vo_x11_uninit(vo,vo->mDisplay, vo->window);
    if(priv->name) { mp_free(priv->name); priv->name = NULL; }
    priv->X_already_started--;
    mp_free(priv);
}

static uint32_t __FASTCALL__ preinit(vo_data_t*vo,const char *arg)
{
    vo->priv=mp_mallocz(sizeof(priv_t));
    priv_t*priv=(priv_t*)vo->priv;
    if (arg)
        priv->name = mp_strdup(arg);
    else {
	MSG_V( "No vidix driver name provided, probing available ones!\n");
	priv->name = NULL;
    }
    if (vidix_preinit(vo,priv->name, &video_out_xvidix) != 0) return 1;
    return 0;
}

static ControlCodes __FASTCALL__ control(vo_data_t*vo,uint32_t request, any_t*data)
{
  switch (request) {
  case VOCTRL_QUERY_FORMAT:
    return query_format(vo,(vo_query_fourcc_t*)data);
  case VOCTRL_FULLSCREEN:
    vo_x11_fullscreen(vo);
    return CONTROL_TRUE;
  case VOCTRL_CHECK_EVENTS:
    {
     vo_resize_t * vrest = (vo_resize_t *)data;
     vrest->event_type = check_events(vo,vrest->adjust_size);
     return CONTROL_TRUE;
    }
  }
  return CONTROL_NA;
}
