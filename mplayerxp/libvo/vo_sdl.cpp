#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
/*  vo_sdl.c
 *
 *  (was video_out_sdl.c from OMS project/mpeg2dec -> http://linuxvideo.org)
 *
 *  Copyright (C) Ryan C. Gordon <icculus@lokigames.com> - April 22, 2000.
 *
 *  Copyright (C) Felix Buenemann <atmosfear@users.sourceforge.net> - 2001
 *
 *  (for extensive code enhancements)
 *
 *  Current maintainer for MPlayer project (report bugs to that address):
 *    Felix Buenemann <atmosfear@users.sourceforge.net>
 *
 *  This file is a video out driver using the SDL library (http://libsdl.org/),
 *  to be used with MPlayer [The Movie Player for Linux] project, further info
 *  from http://mplayer.sourceforge.net.
 *
 *  Current license is not decided yet, but we're heading for GPL.
 *
 *  -- old disclaimer --
 *
 *  A mpeg2dec display driver that does output through the
 *  Simple DirectMedia Layer (SDL) library. This effectively gives us all
 *  sorts of output options: X11, SVGAlib, fbcon, AAlib, GGI. Win32, MacOS
 *  and BeOS support, too. Yay. SDL info, source, and binaries can be found
 *  at http://slouken.devolution.com/SDL/
 *
 *  This file is part of mpeg2dec, a mp_free MPEG-2 video stream decoder.
 *
 *  mpeg2dec is mp_free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  mpeg2dec is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation.
 *
 *  -- end old disclaimer --
 *
 *  Changes:
 *    Dominik Schnitzer <dominik@schnitzer.at> - November 08, 2000.
 *    - Added resizing support, fullscreen: changed the sdlmodes selection
 *       routine.
 *    - SDL bugfixes: removed the atexit(SLD_Quit), SDL_Quit now resides in
 *       the plugin_exit routine.
 *    - Commented the source :)
 *    - Shortcuts: for switching between Fullscreen/Windowed mode and for
 *       cycling between the different Fullscreen modes.
 *    - Small bugfixes: proper width/height of movie
 *    Dominik Schnitzer <dominik@schnitzer.at> - November 11, 2000.
 *    - Cleanup code, more comments
 *    - Better error handling
 *    Bruno Barreyra <barreyra@ufl.edu> - December 10, 2000.
 *    - Eliminated memcpy's for entire frames
 *    Felix Buenemann <Atmosfear@users.sourceforge.net> - March 11, 2001
 *    - Added aspect-ratio awareness for fullscreen
 *    Felix Buenemann <Atmosfear@users.sourceforge.net> - March 11, 2001
 *    - Fixed aspect-ratio awareness, did only vertical scaling (black bars above
 *       and below), now also does horizontal scaling (black bars left and right),
 *       so you get the biggest possible picture with correct aspect-ratio.
 *    Felix Buenemann <Atmosfear@users.sourceforge.net> - March 12, 2001
 *    - Minor bugfix to aspect-ratio for non-4:3-resolutions (like 1280x1024)
 *    - Bugfix to check_events() to reveal mouse cursor after 'q'-quit in
 *       fullscreen-mode
 *    Felix Buenemann <Atmosfear@users.sourceforge.net> - April 10, 2001
 *    - Changed keypress-detection from keydown to keyup, seems to fix keyrepeat
 *       bug (key had to be pressed twice to be detected)
 *    - Changed key-handling: 'f' cycles fullscreen/windowed, ESC/RETURN/'q' quits
 *    - Bugfix which avoids exit, because return is passed to sdl-output on startup,
 *       which caused the player to exit (keyboard-buffer problem? better solution
 *       recommed)
 *    Felix Buenemann <Atmosfear@users.sourceforge.net> - April 11, 2001
 *    - OSD and subtitle support added
 *    - some minor code-changes
 *    - added code to comply with new fullscreen meaning
 *    - changed fullscreen-mode-cycling from '+' to 'c' (interferred with audiosync
 *       adjustment)
 *    Felix Buenemann <Atmosfear@users.sourceforge.net> - April 13, 2001
 *    - added keymapping to toggle OSD ('o' key)
 *    - added some defines to modify some sdl-out internas (see comments)
 *
 *    Felix Buenemann: further changes will be visible through cvs log, don't want
 *     to update this all the time (CVS info on http://mplayer.sourceforge.net)
 *
 */
#include <algorithm>
/* define to force software-surface (video surface stored in system memory)*/
#undef SDL_NOHWSURFACE

/* define to enable surface locks, this might be needed on SMP machines */
#undef SDL_ENABLE_LOCKS

//#define BUGGY_SDL //defined by configure

/* MONITOR_ASPECT MUST BE FLOAT */
#define MONITOR_ASPECT 4.0/3.0

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>

#include "mplayerxp.h"
#include "xmpcore/xmp_core.h"

#include "video_out.h"

#include "osdep/fastmemcpy.h"
#include "sub.h"
#include "aspect.h"

#ifdef HAVE_X11
#include <X11/Xlib.h>
#include "x11_common.h"
#endif

#include "input2/input.h"
#include "input2/mouse.h"
#include "osdep/keycodes.h"
#include "dri_vo.h"
#include "video_out_internal.h"
#include "xmpcore/mp_image.h"
#ifdef CONFIG_VIDIX
#include "vosub_vidix.h"
#endif
#include "vo_msg.h"

LIBVO_EXTERN(sdl)

int sdl_noxv;
int sdl_forcexv;
int sdl_forcegl;

static vo_info_t vo_info =
{
	"SDL YUV/RGB/BGR renderer (SDL v1.1.7+ !)"
#ifdef CONFIG_VIDIX
	" (with sdl:vidix subdevice)"
#endif
	,
	"sdl",
	"Ryan C. Gordon <icculus@lokigames.com>, Felix Buenemann <atmosfear@users.sourceforge.net>",
	""
};

#include <SDL/SDL.h>


#if	defined(sun) && defined(__svr4__)
/* setenv is missing on solaris */
static void setenv(const char *name, const char *val, int _xx)
{
    int len  = strlen(name) + strlen(val) + 2;
    char *env = mp_malloc(len);

    if (env != NULL) {
	strcpy(env, name);
	strcat(env, "=");
	strcat(env, val);
	putenv(env);
    }
}
#endif


#define FS 0x01
#define VM 0x02
#define ZOOM 0x04
#define FLIP 0x08

#ifdef SDL_ENABLE_LOCKS
#define	SDL_OVR_LOCK(x)        if (SDL_LockYUVOverlay (priv->overlay[x])) { \
				MSG_V("SDL: Couldn't lock YUV overlay\n"); \
				return x; \
			    }
#define SDL_OVR_UNLOCK(x)      SDL_UnlockYUVOverlay (priv->overlay[x]);

#define SDL_SRF_LOCK(srf, x)   if(SDL_MUSTLOCK(srf)) { \
				if(SDL_LockSurface (srf)) { \
					MSG_V("SDL: Couldn't lock RGB surface\n"); \
					return x; \
				} \
			    }

#define SDL_SRF_UNLOCK(srf) if(SDL_MUSTLOCK(srf)) \
				SDL_UnlockSurface (srf);
#else
#define SDL_OVR_LOCK(x)
#define SDL_OVR_UNLOCK(x)
#define SDL_SRF_LOCK(srf, x)
#define SDL_SRF_UNLOCK(srf)
#endif

typedef enum {
    YUV=0,
    RGB,
    BGR,
    GL
}sdl_mode_e;

/** Private SDL Data structure **/
typedef struct priv_s {
    char	driver[8]; /* output driver used by sdl */
    SDL_Surface*surface; /* SDL display surface */
    SDL_Surface*rgbsurface[MAX_DRI_BUFFERS]; /* SDL RGB surface */
    SDL_Overlay*overlay[MAX_DRI_BUFFERS]; /* SDL YUV overlay */
    unsigned	num_buffs; /* XP related indexes */
    SDL_Rect**	fullmodes; /* available fullscreen modes */
    Uint32	sdlflags, sdlfullflags; /* surface attributes for fullscreen and windowed mode */
    SDL_Rect	windowsize; /* save the windowed output extents */
    Uint8	bpp; /* Bits per Pixel */
    sdl_mode_e	mode; /* RGB or YUV? */
    int		fullmode; /* current fullscreen mode, 0 = highest available fullscreen mode */
    int		flip; /* Flip image */
    int		fulltype; /* fullscreen behaviour; see init */
    int		X; /* is X running (0/1) */
    int		XWidth, XHeight; /* X11 Resolution */
    int		width, height; /* original image dimensions */
    unsigned	dstwidth, dstheight; /* destination dimensions */
    int		y; /* Draw image at coordinate y on the SDL surfaces */
    int		y_screen_top, y_screen_bottom; /* The image is displayed between those y coordinates in priv->surface */
    int		osd_has_changed; /* 1 if the OSD has changed otherwise 0 */
    uint32_t	format; /* source image format (YUV/RGB/...) */
    /* dirty_off_frame[0] contains a bounding box around the osd contents drawn above the image
       dirty_off_frame[1] is the corresponding thing for OSD contents drawn below the image
    */
    SDL_Rect	dirty_off_frame[2];
#ifdef CONFIG_VIDIX
    const char *	vidix_name;
    vidix_server_t*	vidix_server;
#endif
}priv_t;

static void __FASTCALL__ erase_area_4(int x_start, int width, int height, int pitch, uint32_t color, uint8_t* pixels);
static void __FASTCALL__ erase_area_1(int x_start, int width, int height, int pitch, uint8_t color, uint8_t* pixels);
static MPXP_Rc __FASTCALL__ setup_surfaces(vo_data_t*);
static MPXP_Rc __FASTCALL__ setup_surface(vo_data_t*vo,unsigned idx);
static MPXP_Rc __FASTCALL__ set_video_mode(vo_data_t*vo,int width, int height, int bpp, uint32_t sdlflags);
static void __FASTCALL__ erase_rectangle(vo_data_t*vo,unsigned idx,int x, int y, int w, int h);

static char sdl_subdevice[100]="";

/** libvo Plugin functions **/

/**
 * Take a null-terminated array of pointers, and find the last element.
 *
 *    params : array == array of which we want to find the last element.
 *   returns : index of last NON-NULL element.
 **/

static inline int findArrayEnd (SDL_Rect **array)
{
    int i = 0;
    while ( array[i++] );	/* keep loopin' ... */

    /* return the index of the last array element */
    return i - 1;
}

/**
 * Open and prepare SDL output.
 *
 *    params : *plugin ==
 *             *name ==
 *   returns : 0 on success, -1 on failure
 **/

static MPXP_Rc sdl_open ( vo_data_t*vo )
{
    priv_t *priv = reinterpret_cast<priv_t*>(vo->priv);
    const SDL_VideoInfo *vidInfo = NULL;
    /*static int opened = 0;
	if (opened)
	    return 0;
	opened = 1;*/
    MSG_DBG3("SDL: Opening Plugin\n");
#ifdef CONFIG_VIDIX
    if(memcmp(sdl_subdevice,"vidix",5) == 0) {
	priv->vidix_name = &sdl_subdevice[5]; /* vidix_name will be valid within init() */
	if(!(priv->vidix_server=vidix_preinit(vo,priv->vidix_name,&video_out_sdl)))
	    return MPXP_False;
	strcpy(priv->driver,"vidix");
    } else {
#endif
	if(sdl_subdevice[0]) setenv("SDL_VIDEODRIVER", sdl_subdevice, 1);

	/* does the user want SDL to try and force Xv */
	if(sdl_forcexv)	setenv("SDL_VIDEO_X11_NODIRECTCOLOR", "1", 1);

	/* does the user want to disable Xv and use software scaling instead */
	if(sdl_noxv) setenv("SDL_VIDEO_YUV_HWACCEL", "0", 1);
#ifdef CONFIG_VIDIX
    }
#endif

    /* default to no fullscreen mode, we'll set this as soon we have the avail. modes */
    priv->fullmode = -2;

    priv->fullmodes = NULL;
    priv->bpp = 0;

    /* initialize the SDL Video system */
    if (!SDL_WasInit(SDL_INIT_VIDEO)) {
	if (SDL_Init (SDL_INIT_VIDEO|SDL_INIT_NOPARACHUTE)) {
	    MSG_ERR("SDL: Initializing of SDL failed: %s.\n", SDL_GetError());
	    return MPXP_False;
	}
    }

#ifdef CONFIG_VIDIX
    if(memcmp(sdl_subdevice,"vidix",5) != 0)
#endif
    SDL_VideoDriverName(priv->driver, 8);
    MSG_OK("SDL: Using driver: %s\n", priv->driver);
    /* other default values */
#ifdef SDL_NOHWSURFACE
    MSG_V("SDL: using software-surface\n");
    priv->sdlflags = SDL_SWSURFACE|SDL_RESIZABLE|SDL_ASYNCBLIT|SDL_ANYFORMAT;
    priv->sdlfullflags = SDL_SWSURFACE|SDL_FULLSCREEN|SDL_DOUBLEBUF|SDL_ASYNCBLIT|SDL_ANYFORMAT;
#else
    MSG_V("SDL: using hardware-surface\n");
    priv->sdlflags = SDL_HWSURFACE|SDL_RESIZABLE|SDL_ASYNCBLIT|SDL_HWACCEL/*|SDL_ANYFORMAT*/;
    priv->sdlfullflags = SDL_HWSURFACE|SDL_FULLSCREEN|SDL_DOUBLEBUF|SDL_ASYNCBLIT|SDL_HWACCEL/*|SDL_ANYFORMAT*/;
#endif
    if(sdl_forcegl) {
	priv->sdlflags |= SDL_OPENGL|SDL_OPENGLBLIT|SDL_ANYFORMAT;
	priv->sdlfullflags |= SDL_OPENGL|SDL_OPENGLBLIT|SDL_ANYFORMAT;
    }
    /* Setup Keyrepeats (500/30 are defaults) */
    SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, 100 /*SDL_DEFAULT_REPEAT_INTERVAL*/);

    /* get information about the graphics adapter */
    vidInfo = SDL_GetVideoInfo ();

    /* collect all fullscreen & hardware modes available */
    if (!(priv->fullmodes = SDL_ListModes (vidInfo->vfmt, priv->sdlfullflags))) {
	/* non hardware accelerated fullscreen modes */
	priv->sdlfullflags &= ~SDL_HWSURFACE;
	priv->fullmodes = SDL_ListModes (vidInfo->vfmt, priv->sdlfullflags);
    }

    /* test for normal resizeable & windowed hardware accellerated surfaces */
    if (!SDL_ListModes (vidInfo->vfmt, priv->sdlflags)) {
	/* test for NON hardware accelerated resizeable surfaces - poor you.
	 * That's all we have. If this fails there's nothing left.
	 * Theoretically there could be Fullscreenmodes left - we ignore this for now.
	 */
	priv->sdlflags &= ~SDL_HWSURFACE;
	if ((!SDL_ListModes (vidInfo->vfmt, priv->sdlflags)) && (!priv->fullmodes)) {
	    MSG_ERR("SDL: Couldn't get any acceptable SDL Mode for output.\n");
	    return MPXP_False;
	}
    }
   /* YUV overlays need at least 16-bit color depth, but the
    * display might less. The SDL AAlib target says it can only do
    * 8-bits, for example. So, if the display is less than 16-bits,
    * we'll force the BPP to 16, and pray that SDL can emulate for us.
    */
    priv->bpp = vidInfo->vfmt->BitsPerPixel;
    if (priv->mode == YUV && priv->bpp < 16) {
	MSG_V("SDL: Your SDL display target wants to be at a color "
		"depth of (%d), but we need it to be at least 16 "
		"bits, so we need to emulate 16-bit color. This is "
		"going to slow things down; you might want to "
		"increase your display's color depth, if possible.\n",
		priv->bpp);
	priv->bpp = 16;
    }

    /* We don't want those in our event queue.
     * We use SDL_KEYUP cause SDL_KEYDOWN seems to cause problems
     * with keys need to be pressed twice, to be recognized.
     */
#ifndef BUGGY_SDL
    SDL_EventState(SDL_ACTIVEEVENT, SDL_IGNORE);
    SDL_EventState(SDL_MOUSEMOTION, SDL_IGNORE);
//  SDL_EventState(SDL_QUIT, SDL_IGNORE);
    SDL_EventState(SDL_SYSWMEVENT, SDL_IGNORE);
    SDL_EventState(SDL_USEREVENT, SDL_IGNORE);
#endif

    /* Success! */
    return MPXP_Ok;
}


/**
 * Close SDL, Cleanups, Free Memory
 *
 *    params : *plugin
 *   returns : non-zero on success, zero on error.
 **/

static int sdl_close (vo_data_t*vo)
{
    priv_t *priv = reinterpret_cast<priv_t*>(vo->priv);
    unsigned i,n;
#ifdef CONFIG_VIDIX
    if(priv->vidix_name) vidix_term(vo);
#endif
    n=priv->num_buffs;
    for(i=0;i<n;i++) {
	/* Cleanup YUV Overlay structure */
	if (priv->overlay[i])
	    SDL_FreeYUVOverlay(priv->overlay[i]);
	/* Free RGB Surface */
	if (priv->rgbsurface[i])
	    SDL_FreeSurface(priv->rgbsurface[i]);
    }
    /* Free our blitting surface */
    if (priv->surface) SDL_FreeSurface(priv->surface);

    /* DONT attempt to mp_free the fullscreen modes array. SDL_Quit* does this for us */
    /* Cleanup SDL */
    if(SDL_WasInit(SDL_INIT_VIDEO))
	SDL_QuitSubSystem(SDL_INIT_VIDEO);

    MSG_DBG3("SDL: Closed Plugin\n");

    return 0;
}

/* Set video mode. Not fullscreen */
static MPXP_Rc __FASTCALL__ set_video_mode(vo_data_t*vo,int width, int height, int bpp, uint32_t sdlflags)
{
    priv_t *priv = reinterpret_cast<priv_t*>(vo->priv);
    SDL_Surface* newsurface;
    MPXP_Rc retval=MPXP_False;
    newsurface = SDL_SetVideoMode(width, height, bpp, sdlflags);

    if(newsurface) {
	vo_lock_surfaces(vo);
	/* priv->surface will be NULL the first time this function is called. */
	if(priv->surface)
	    SDL_FreeSurface(priv->surface);

	priv->surface = newsurface;
	priv->dstwidth = width;
	priv->dstheight = height;

	retval = setup_surfaces(vo);
	vo_unlock_surfaces(vo);
    }
    else
	MSG_ERR("set_video_mode: SDL_SetVideoMode failed: %s\n", SDL_GetError());
    return retval;
}

static MPXP_Rc __FASTCALL__ set_fullmode (vo_data_t*vo,int mode) {
    priv_t *priv = reinterpret_cast<priv_t*>(vo->priv);
    SDL_Surface *newsurface = NULL;
    int screen_surface_w, screen_surface_h;
    MPXP_Rc retval=MPXP_False;

    /* if we haven't set a fullmode yet, default to the lowest res fullmode first */
    /* But select a mode where the full video enter */
    if(priv->X && priv->fulltype & FS) {
	screen_surface_w = priv->XWidth;
	screen_surface_h = priv->XHeight;
    }
    else if (mode < 0) {
	int i;
	mode = 0; // Default to the biggest mode avaible
	for(i = findArrayEnd(priv->fullmodes) - 1; i >=0; i--) {
	    if( (priv->fullmodes[i]->w >= priv->dstwidth) &&
		      (priv->fullmodes[i]->h >= priv->dstheight) ) {
		    mode = i;
		    break;
	    }
	}
	priv->fullmode = mode;
	screen_surface_h = priv->fullmodes[mode]->h;
	screen_surface_w = priv->fullmodes[mode]->w;
    } else {
	screen_surface_h = priv->fullmodes[mode]->h;
	screen_surface_w = priv->fullmodes[mode]->w;
    }
    aspect_save_screenres(screen_surface_w, screen_surface_h);

    /* calculate new video size/aspect */
    if(priv->mode == YUV) {
	if(priv->fulltype&FS) aspect_save_screenres(priv->XWidth, priv->XHeight);
	aspect(&priv->dstwidth, &priv->dstheight, A_ZOOM);
    }

    /* try to change to given fullscreenmode */
    newsurface = SDL_SetVideoMode(  priv->dstwidth, screen_surface_h, priv->bpp,
					priv->sdlfullflags);

    /* if creation of new surface was successfull, save it and hide mouse cursor */
    if(newsurface) {
	vo_lock_surfaces(vo);
	if (priv->surface) SDL_FreeSurface(priv->surface);
	priv->surface = newsurface;
	SDL_ShowCursor(0);
	SDL_SRF_LOCK(priv->surface, -1)
	SDL_FillRect(priv->surface, NULL, 0);
	SDL_SRF_UNLOCK(priv->surface)
	retval = setup_surfaces(vo);
	vo_unlock_surfaces(vo);
    } else
	MSG_ERR("set_fullmode: SDL_SetVideoMode failed: %s\n", SDL_GetError());
    return retval;
}


/**
 * Initialize an SDL surface and an SDL YUV overlay.
 *
 *    params : width  == width of video we'll be displaying.
 *             height == height of video we'll be displaying.
 *             fullscreen == want to be fullscreen?
 *             title == Title for window titlebar.
 *   returns : non-zero on success, zero on error.
 **/

static MPXP_Rc __FASTCALL__ config(vo_data_t*vo,uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t flags, char *title, uint32_t format)
//static int sdl_setup (int width, int height)
{
    priv_t *priv = reinterpret_cast<priv_t*>(vo->priv);
    MPXP_Rc retval;

    if(sdl_forcegl) priv->mode = GL;
    else
    switch(format){
	case IMGFMT_I420:
	case IMGFMT_YV12:
	case IMGFMT_IYUV:
	case IMGFMT_YUY2:
	case IMGFMT_UYVY:
	case IMGFMT_YVYU:
	    priv->mode = YUV;
	    break;
	case IMGFMT_BGR15:
	case IMGFMT_BGR16:
	case IMGFMT_BGR24:
	case IMGFMT_BGR32:
	    priv->mode = BGR;
	    break;
	case IMGFMT_RGB15:
	case IMGFMT_RGB16:
	case IMGFMT_RGB24:
	case IMGFMT_RGB32:
	    priv->mode = RGB;
	    break;
	default:
	    MSG_ERR("SDL: Unsupported image format (0x%X)\n",format);
	    return MPXP_False;
    }

    MSG_V("SDL: Using 0x%X (%s) image format\n", format,
	vo_format_name(format));

    if(priv->mode != YUV) {
	priv->sdlflags |= SDL_ANYFORMAT;
	priv->sdlfullflags |= SDL_ANYFORMAT;
    }
    /* SDL can only scale YUV data */
    if(priv->mode == RGB || priv->mode == BGR) {
	d_width = width;
	d_height = height;
    }
    aspect_save_orig(width,height);
    aspect_save_prescale(d_width ? d_width : width, d_height ? d_height : height);

    /* Save the original Image size */
    priv->X = 0;
    priv->width  = width;
    priv->height = height;
    priv->dstwidth  = d_width ? d_width : width;
    priv->dstheight = d_height ? d_height : height;

    priv->format = format;

    /* Set output window title */
    SDL_WM_SetCaption (".: MPlayerXP : F = Fullscreen/Windowed : C = Cycle Fullscreen Resolutions :.", title);

    if(priv->mode == GL) {
	switch(format){
	    case IMGFMT_BGR15:
	    case IMGFMT_RGB15:
		SDL_GL_SetAttribute(SDL_GL_RED_SIZE,5);
		SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE,5);
		SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,5);
		SDL_GL_SetAttribute(SDL_GL_ACCUM_RED_SIZE,5);
		SDL_GL_SetAttribute(SDL_GL_ACCUM_GREEN_SIZE,5);
		SDL_GL_SetAttribute(SDL_GL_ACCUM_BLUE_SIZE,5);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,16);
		break;
	    case IMGFMT_BGR16:
	    case IMGFMT_RGB16:
		SDL_GL_SetAttribute(SDL_GL_RED_SIZE,5);
		SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE,6);
		SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,5);
		SDL_GL_SetAttribute(SDL_GL_ACCUM_RED_SIZE,5);
		SDL_GL_SetAttribute(SDL_GL_ACCUM_GREEN_SIZE,6);
		SDL_GL_SetAttribute(SDL_GL_ACCUM_BLUE_SIZE,5);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,16);
		break;
	    case IMGFMT_BGR24:
	    case IMGFMT_RGB24:
		SDL_GL_SetAttribute(SDL_GL_RED_SIZE,8);
		SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE,8);
		SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,8);
		SDL_GL_SetAttribute(SDL_GL_ACCUM_RED_SIZE,8);
		SDL_GL_SetAttribute(SDL_GL_ACCUM_GREEN_SIZE,8);
		SDL_GL_SetAttribute(SDL_GL_ACCUM_BLUE_SIZE,8);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,24);
		break;
	    case IMGFMT_BGR32:
	    case IMGFMT_RGB32:
		SDL_GL_SetAttribute(SDL_GL_RED_SIZE,8);
		SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE,8);
		SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,8);
		SDL_GL_SetAttribute(SDL_GL_ACCUM_RED_SIZE,8);
		SDL_GL_SetAttribute(SDL_GL_ACCUM_GREEN_SIZE,8);
		SDL_GL_SetAttribute(SDL_GL_ACCUM_BLUE_SIZE,8);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,32);
		break;
	    default:
		MSG_ERR("SDL: Unsupported image format in GL mode (0x%X)\n",format);
		return MPXP_False;
	}
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
    }
    if(priv->X) {
	aspect_save_screenres(priv->XWidth,priv->XHeight);
	aspect(&priv->dstwidth,&priv->dstheight,flags&VOFLAG_SWSCALE?A_ZOOM:A_NOZOOM);
    }
    priv->windowsize.w = priv->dstwidth;
    priv->windowsize.h = priv->dstheight;

    /* bit 0 (0x01) means fullscreen (-fs)
     * bit 1 (0x02) means mode switching (-vm)
     * bit 2 (0x04) enables software scaling (-zoom)
     * bit 3 (0x08) enables flipping (-flip)
     */
    if(flags&FLIP) {
	MSG_V("SDL: using flipped video (only with RGB/BGR/packed YUV)\n");
	priv->flip = 1;
    }
    if(flags&FS) {
	MSG_V("SDL: setting zoomed fullscreen without modeswitching\n");
	MSG_V("SDL: Info - please use -vm or -zoom to switch to best resolution.\n");
	priv->fulltype = FS;
	retval = set_fullmode(vo,priv->fullmode);
	if(retval!=MPXP_Ok) return retval;
    } else if(flags&VM) {
	MSG_V("SDL: setting zoomed fullscreen with modeswitching\n");
	priv->fulltype = VM;
	set_fullmode(vo,priv->fullmode);
    } else if(flags&ZOOM) {
	MSG_V("SDL: setting zoomed fullscreen with modeswitching\n");
	priv->fulltype = ZOOM;
	retval = set_fullmode(vo,priv->fullmode);
	if(retval!=MPXP_Ok) return retval;
    } else {
	if((strcmp(priv->driver, "x11") == 0)
	    ||(strcmp(priv->driver, "windib") == 0)
	    ||(strcmp(priv->driver, "directx") == 0)
	    ||((strcmp(priv->driver, "aalib") == 0)
	    && priv->X)) {
		MSG_V("SDL: setting windowed mode\n");
		retval = set_video_mode(vo,priv->dstwidth, priv->dstheight, priv->bpp, priv->sdlflags);
		if(retval!=MPXP_Ok) return retval;
	    } else {
		MSG_V("SDL: setting zoomed fullscreen with modeswitching\n");
		priv->fulltype = ZOOM;
		retval = set_fullmode(vo,priv->fullmode);
		if(retval!=MPXP_Ok) return retval;
	    }
    }

    if(!priv->surface) { // cannot SetVideoMode
	MSG_ERR("SDL: failed to set video mode: %s\n", SDL_GetError());
	return MPXP_False;
    }
    return MPXP_Ok;
}

static MPXP_Rc setup_surfaces( vo_data_t*vo )
{
    priv_t *priv = reinterpret_cast<priv_t*>(vo->priv);
    unsigned i;
    MPXP_Rc retval;
    priv->num_buffs=vo_conf.xp_buffs;
#ifdef CONFIG_VIDIX
    if(!priv->vidix_name) {
#endif
    for(i=0;i<priv->num_buffs;i++) {
	retval = setup_surface(vo,i);
	if(retval!=MPXP_Ok) return retval;
    }
#ifdef CONFIG_VIDIX
    }
    else {
	if(vidix_init(vo,priv->width,priv->height,0,priv->y,
			priv->dstwidth,priv->dstheight,priv->format,priv->bpp,
			priv->XWidth,priv->XHeight) != MPXP_Ok) {
	    MSG_ERR("vo_sdl: Can't initialize VIDIX driver\n");
	    priv->vidix_name = NULL;
	    return MPXP_False;
	} else MSG_V("vo_sdl: Using VIDIX\n");
	if(vidix_start(vo)!=0) return MPXP_False;
    }
#endif
    return MPXP_Ok;
}

/* Free priv->rgbsurface or priv->overlay if they are != NULL.
 * Setup priv->rgbsurface or priv->overlay depending on source format.
 * The size of the created surface or overlay depends on the size of
 * priv->surface, priv->width, priv->height, priv->dstwidth and priv->dstheight.
 */
static MPXP_Rc __FASTCALL__ setup_surface(vo_data_t*vo,unsigned idx)
{
    priv_t *priv = reinterpret_cast<priv_t*>(vo->priv);
    float v_scale = ((float) priv->dstheight) / priv->height;
    int surfwidth, surfheight;
    surfwidth = priv->width;
    surfheight = priv->height + (priv->surface->h - priv->dstheight) / v_scale;
    surfheight&= ~1;
    /* Place the image in the middle of the screen */
    priv->y = (surfheight - priv->height) / 2;
    priv->y_screen_top = priv->y * v_scale;
    priv->y_screen_bottom = priv->y_screen_top + priv->dstheight;

    priv->dirty_off_frame[0].x = -1;
    priv->dirty_off_frame[0].y = -1;
    priv->dirty_off_frame[1].x = -1;
    priv->dirty_off_frame[1].y = -1;

    /* Make sure the entire screen is updated */
    vo_osd_changed(1);

    if(priv->rgbsurface[idx])
	SDL_FreeSurface(priv->rgbsurface[idx]);
    else if(priv->overlay[idx])
	SDL_FreeYUVOverlay(priv->overlay[idx]);

    priv->rgbsurface[idx] = NULL;
    priv->overlay[idx] = NULL;

    switch(priv->format) {
	/* Initialize and create the RGB Surface used for video out in BGR/RGB mode */
	//SDL_Surface *SDL_CreateRGBSurface(Uint32 flags, int width, int height, int depth, Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask);
	//	SDL_SWSURFACE,SDL_HWSURFACE,SDL_SRCCOLORKEY, priv->flags?	guess: exchange Rmask and Bmask for BGR<->RGB
	// 32 bit: a:ff000000 r:ff000 g:ff00 b:ff
	// 24 bit: r:ff0000 g:ff00 b:ff
	// 16 bit: r:1111100000000000b g:0000011111100000b b:0000000000011111b
	// 15 bit: r:111110000000000b g:000001111100000b b:000000000011111b
	// FIXME: colorkey detect based on bpp, FIXME static bpp value, FIXME alpha value correct?
	case IMGFMT_RGB15:
	    priv->rgbsurface[idx] = SDL_CreateRGBSurface (SDL_SRCCOLORKEY, surfwidth, surfheight, 15, 31, 992, 31744, 0);
	    break;
	case IMGFMT_BGR15:
	    priv->rgbsurface[idx] = SDL_CreateRGBSurface (SDL_SRCCOLORKEY, surfwidth, surfheight, 15, 31744, 992, 31, 0);
	    break;
	case IMGFMT_RGB16:
	    priv->rgbsurface[idx] = SDL_CreateRGBSurface (SDL_SRCCOLORKEY, surfwidth, surfheight, 16, 31, 2016, 63488, 0);
	    break;
	case IMGFMT_BGR16:
	    priv->rgbsurface[idx] = SDL_CreateRGBSurface (SDL_SRCCOLORKEY, surfwidth, surfheight, 16, 63488, 2016, 31, 0);
	    break;
	case IMGFMT_RGB24:
	    priv->rgbsurface[idx] = SDL_CreateRGBSurface (SDL_SRCCOLORKEY, surfwidth, surfheight, 24, 0x0000FF, 0x00FF00, 0xFF0000, 0);
	    break;
	case IMGFMT_BGR24:
	    priv->rgbsurface[idx] = SDL_CreateRGBSurface (SDL_SRCCOLORKEY, surfwidth, surfheight, 24, 0xFF0000, 0x00FF00, 0x0000FF, 0);
	    break;
	case IMGFMT_RGB32:
	    priv->rgbsurface[idx] = SDL_CreateRGBSurface (SDL_SRCCOLORKEY, surfwidth, surfheight, 32, 0x000000FF, 0x0000FF00, 0x00FF0000, 0/*0xFF000000*/);
	    break;
	case IMGFMT_BGR32:
	    priv->rgbsurface[idx] = SDL_CreateRGBSurface (SDL_SRCCOLORKEY, surfwidth, surfheight, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0/*0xFF000000*/);
	    break;
	default:
	    /* Initialize and create the YUV Overlay used for video out */
	    if (!(priv->overlay[idx] = SDL_CreateYUVOverlay (surfwidth, surfheight, priv->format==IMGFMT_I420?IMGFMT_IYUV:priv->format, priv->surface))) {
		MSG_ERR ("SDL: Couldn't create a YUV overlay: %s\n", SDL_GetError());
		return MPXP_False;
	    }
    }
    if(priv->mode != YUV && priv->mode != GL) {
	if(!priv->rgbsurface[idx]) {
	    MSG_ERR ("SDL: Couldn't create a RGB surface: %s\n", SDL_GetError());
	    return MPXP_False;
	}

	if((priv->format&0xFF) != priv->bpp)
	    MSG_WARN("SDL: using depth/colorspace conversion, this will slow things"
		   "down (%ibpp -> %ibpp).\n", priv->format&0xFF, priv->bpp);
    }
    erase_rectangle(vo,idx,0, 0, surfwidth, surfheight);

    return MPXP_Ok;
}

/**
 * Checks for SDL keypress and window resize events
 *
 *   params : none
 *  returns : doesn't return
 **/

#define shift_key (event.key.keysym.mod==(KMOD_LSHIFT||KMOD_RSHIFT))
static uint32_t __FASTCALL__ check_events (vo_data_t*vo,vo_adjust_size_t adjust_size)
{
    priv_t *priv = reinterpret_cast<priv_t*>(vo->priv);
    SDL_Event event;
    SDLKey keypressed = SDLKey(0);
    static int firstcheck = 0;
    uint32_t retval;

    retval = 0;
    /* Poll the waiting SDL Events */
    while ( SDL_PollEvent(&event) ) {
	switch (event.type) {
	    /* capture window resize events */
	    case SDL_VIDEORESIZE:
		(*adjust_size)(vo,priv->windowsize.w,priv->windowsize.h,reinterpret_cast<unsigned*>(&event.resize.w), reinterpret_cast<unsigned*>(&event.resize.h));
		if(set_video_mode(vo,event.resize.w, event.resize.h,
				  priv->bpp, priv->sdlflags)!=0)
				  exit(EXIT_FAILURE);

		/* save video extents, to restore them after going fullscreen */
		priv->windowsize.w = priv->surface->w;
		priv->windowsize.h = priv->surface->h;
		MSG_DBG3("SDL: Window resize\n");
		retval = VO_EVENT_RESIZE;
		break;

	    case SDL_MOUSEBUTTONDOWN:
		if(event.button.button == 4 || event.button.button == 5)
		    mplayer_put_key(MOUSE_BASE+event.button.button-1);
		else
		    mplayer_put_key((MOUSE_BASE+event.button.button-1) | MP_KEY_DOWN);
		break;

	    case SDL_MOUSEBUTTONUP:
		mplayer_put_key(MOUSE_BASE+event.button.button-1);
		break;

	/* graphics mode selection shortcuts */
#ifdef BUGGY_SDL
	    case SDL_KEYDOWN:
		switch(event.key.keysym.sym) {
		    case SDLK_UP: mplayer_put_key(KEY_UP); break;
		    case SDLK_DOWN: mplayer_put_key(KEY_DOWN); break;
		    case SDLK_LEFT: mplayer_put_key(KEY_LEFT); break;
		    case SDLK_RIGHT: mplayer_put_key(KEY_RIGHT); break;
		    case SDLK_LESS: mplayer_put_key(shift_key?'>':'<'); break;
		    case SDLK_GREATER: mplayer_put_key('>'); break;
		    case SDLK_ASTERISK:
		    case SDLK_KP_MULTIPLY:
		    case SDLK_SLASH:
		    case SDLK_KP_DIVIDE:
		    default: break;
		}
		break;
	    case SDL_KEYUP:
#else
	    case SDL_KEYDOWN:
#endif
		keypressed = event.key.keysym.sym;
		MSG_V("SDL: Key pressed: '%i'\n", keypressed);

		/* c key pressed. c cycles through available fullscreenmodes, if we have some */
		if ( ((keypressed == SDLK_c)) && (priv->fullmodes) ) {
		    /* select next fullscreen mode */
		    priv->fullmode++;
		    if (priv->fullmode > (findArrayEnd(priv->fullmodes) - 1)) priv->fullmode = 0;
		    if(set_fullmode(vo,priv->fullmode)!=0) exit(EXIT_FAILURE);
		    MSG_V("SDL: Set next available fullscreen mode.\n");
		    retval = VO_EVENT_RESIZE;
		} else if ( keypressed == SDLK_n ) {
#ifdef HAVE_X11
		    aspect(&priv->dstwidth, &priv->dstheight,vo_ZOOM(vo)?A_ZOOM:A_NOZOOM);
#endif
		    if (unsigned(priv->surface->w) != priv->dstwidth || unsigned(priv->surface->h) != priv->dstheight) {
			if(set_video_mode(vo,priv->dstwidth, priv->dstheight, priv->bpp, priv->sdlflags)!=0) exit(EXIT_FAILURE);
			priv->windowsize.w = priv->surface->w;
			priv->windowsize.h = priv->surface->h;
			MSG_V("SDL: Normal size\n");
			retval |= VO_EVENT_RESIZE;
		    } else if (unsigned(priv->surface->w) != priv->dstwidth * 2 || unsigned(priv->surface->h) != priv->dstheight * 2) {
			if(set_video_mode(vo,priv->dstwidth * 2, priv->dstheight * 2, priv->bpp, priv->sdlflags)!=0) exit(EXIT_FAILURE);
			priv->windowsize.w = priv->surface->w;
			priv->windowsize.h = priv->surface->h;
			MSG_V("SDL: Double size\n");
			retval |= VO_EVENT_RESIZE;
		    }
		} else switch(keypressed) {
			case SDLK_RETURN:
			    if (!firstcheck) { firstcheck = 1; break; }
			case SDLK_ESCAPE:
			case SDLK_q:
			    SDL_ShowCursor(1);
			    mplayer_put_key('q');
			    break;
			/*case SDLK_o: mplayer_put_key('o');break;
			case SDLK_SPACE: mplayer_put_key(' ');break;
			case SDLK_p: mplayer_put_key('p');break;*/
			case SDLK_7: mplayer_put_key(shift_key?'/':'7');
			case SDLK_PLUS: mplayer_put_key(shift_key?'*':'+');
			case SDLK_KP_PLUS: mplayer_put_key('+');break;
			case SDLK_MINUS:
			case SDLK_KP_MINUS: mplayer_put_key('-');break;
			case SDLK_TAB: mplayer_put_key('\t');break;
			case SDLK_PAGEUP: mplayer_put_key(KEY_PAGE_UP);break;
			case SDLK_PAGEDOWN: mplayer_put_key(KEY_PAGE_DOWN);break;
#ifdef BUGGY_SDL
			case SDLK_UP:
			case SDLK_DOWN:
			case SDLK_LEFT:
			case SDLK_RIGHT:
			case SDLK_ASTERISK:
			case SDLK_KP_MULTIPLY:
			case SDLK_SLASH:
			case SDLK_KP_DIVIDE:
			    break;
#else
			case SDLK_UP: mplayer_put_key(KEY_UP);break;
			case SDLK_DOWN: mplayer_put_key(KEY_DOWN);break;
			case SDLK_LEFT: mplayer_put_key(KEY_LEFT);break;
			case SDLK_RIGHT: mplayer_put_key(KEY_RIGHT);break;
			case SDLK_LESS: mplayer_put_key(shift_key?'>':'<'); break;
			case SDLK_GREATER: mplayer_put_key('>'); break;
			case SDLK_ASTERISK:
			case SDLK_KP_MULTIPLY: mplayer_put_key('*'); break;
			case SDLK_SLASH:
			case SDLK_KP_DIVIDE: mplayer_put_key('/'); break;
#endif
			default:
			    mplayer_put_key(keypressed);
		}
		break;
	    case SDL_QUIT:
		SDL_ShowCursor(1);
		mplayer_put_key('q');
		break;
	}
    }
    return retval;
}
#undef shift_key

/* Erase (paint it black) the rectangle specified by x, y, w and h in the surface
   or overlay which is used for OSD
*/
static void __FASTCALL__ erase_rectangle(vo_data_t*vo,unsigned idx,int x, int y, int w, int h)
{
    priv_t *priv = reinterpret_cast<priv_t*>(vo->priv);

    switch(priv->format) {
	case IMGFMT_YV12:
	case IMGFMT_I420:
	case IMGFMT_IYUV:
	{
	    SDL_OVR_LOCK(idx)

	    /* Erase Y plane */
	    erase_area_1(x, w, h,
			priv->overlay[idx]->pitches[0], 0,
			priv->overlay[idx]->pixels[0] +
			priv->overlay[idx]->pitches[0]*y);
	    /* Erase U and V planes */
	    w /= 2;
	    x /= 2;
	    h /= 2;
	    y /= 2;

	    erase_area_1(x, w, h,
			priv->overlay[idx]->pitches[1], 128,
			priv->overlay[idx]->pixels[1] +
			priv->overlay[idx]->pitches[1]*y);

	    erase_area_1(x, w, h,
			priv->overlay[idx]->pitches[2], 128,
			priv->overlay[idx]->pixels[2] +
			priv->overlay[idx]->pitches[2]*y);
	    SDL_OVR_UNLOCK(idx)
	    break;
	}
	case IMGFMT_YUY2:
	case IMGFMT_YVYU:
	case IMGFMT_UYVY:
	{
	    /* yuy2 and yvyu represent black the same way */
	    uint8_t yuy2_black[] = {16, 128, 16, 128};
	    uint8_t uyvy_black[] = {128, 16, 128, 16};

	    SDL_OVR_LOCK(idx)
	    erase_area_4(x*2, w*2, h,
			priv->overlay[idx]->pitches[0],
			priv->format == IMGFMT_UYVY ? *((uint32_t*) uyvy_black):
			(*(uint32_t*) yuy2_black),
			priv->overlay[idx]->pixels[0] +
			priv->overlay[idx]->pitches[0]*y);
	    SDL_OVR_UNLOCK(idx)
	    break;
	}
	case IMGFMT_RGB15:
	case IMGFMT_BGR15:
	case IMGFMT_RGB16:
	case IMGFMT_BGR16:
	case IMGFMT_RGB24:
	case IMGFMT_BGR24:
	case IMGFMT_RGB32:
	case IMGFMT_BGR32:
	{
	    SDL_Rect rect;
	    rect.w = w; rect.h = h;
	    rect.x = x; rect.y = y;

	    SDL_SRF_LOCK(priv->rgbsurface[idx], (void) 0)
	    SDL_FillRect(priv->rgbsurface[idx], &rect, 0);
	    SDL_SRF_UNLOCK(priv->rgbsurface[idx])
	    break;
	}
    }
}

/* Fill area beginning at 'pixels' with 'color'. 'x_start', 'width' and 'pitch'
 * are given in bytes. 4 bytes at a time.
 */
static void __FASTCALL__ erase_area_4(int x_start, int width, int height, int pitch, uint32_t color, uint8_t* pixels)
{
    int x_end = x_start/4 + width/4;
    int x, y;
    uint32_t* data = (uint32_t*) pixels;

    x_start /= 4;
    pitch /= 4;

    for(y = 0; y < height; y++) {
	for(x = x_start; x < x_end; x++)
	    data[y*pitch + x] = color;
    }
}

/* Fill area beginning at 'pixels' with 'color'. 'x_start', 'width' and 'pitch'
 * are given in bytes. 1 byte at a time.
 */
static void __FASTCALL__ erase_area_1(int x_start, int width, int height, int pitch, uint8_t color, uint8_t* pixels)
{
    int y;
    for(y = 0; y < height; y++) memset(&pixels[y*pitch + x_start], color, width);
}

/**
 * Display the surface we have written our data to
 *
 *   params : mode == index of the desired fullscreen mode
 *  returns : doesn't return
 **/

static void __FASTCALL__ select_frame(vo_data_t*vo,unsigned idx)
{
    priv_t *priv = reinterpret_cast<priv_t*>(vo->priv);
#ifdef CONFIG_VIDIX
    if(priv->vidix_server) {
	priv->vidix_server->select_frame(vo,idx);
	return;
    }
#endif

	if(priv->mode == YUV) {
		/* blit to the YUV overlay */
		SDL_DisplayYUVOverlay (priv->overlay[idx], &priv->surface->clip_rect);

		/* check if we have a double buffered surface and flip() if we do. */
		if ( priv->surface->flags & SDL_DOUBLEBUF )
			SDL_Flip(priv->surface);

		//SDL_LockYUVOverlay (priv->overlay); // removed because unused!?
	} else {
	    /* blit to the RGB surface */
	    if(SDL_BlitSurface (priv->rgbsurface[idx], NULL, priv->surface, NULL))
			MSG_ERR("SDL: Blit failed: %s\n", SDL_GetError());

	    /* update screen */
	    if(sdl_forcegl)
		SDL_UpdateRects(priv->surface, 1, &priv->surface->clip_rect);
	    else
	    {
		if(priv->osd_has_changed) {
		    priv->osd_has_changed = 0;
		    SDL_UpdateRects(priv->surface, 1, &priv->surface->clip_rect);
		}
		else
		    SDL_UpdateRect(priv->surface, 0, priv->y_screen_top,
				priv->surface->clip_rect.w, priv->y_screen_bottom);
	    }
	    /* check if we have a double buffered surface and flip() if we do. */
	    if(sdl_forcegl) SDL_GL_SwapBuffers();
	    else
	    if(priv->surface->flags & SDL_DOUBLEBUF ) SDL_Flip(priv->surface);
	}
}

static MPXP_Rc __FASTCALL__ query_format(const vo_data_t*vo,vo_query_fourcc_t* format)
{
    priv_t *priv = reinterpret_cast<priv_t*>(vo->priv);
    if(sdl_forcegl) {
	if (IMGFMT_IS_BGR(format->fourcc)) {
	    if  (rgbfmt_depth(format->fourcc) == (unsigned)priv->bpp &&
		((unsigned)priv->bpp==16 || (unsigned)priv->bpp == 32))
			format->flags=VOCAP_SUPPORTED|VOCAP_HWSCALER;
			return MPXP_Ok;
	}
	return MPXP_False;
    }
    else
    switch(format->fourcc){
    case IMGFMT_YV12:
    case IMGFMT_I420:
    case IMGFMT_IYUV:
    case IMGFMT_YUY2:
    case IMGFMT_UYVY:
    case IMGFMT_YVYU:
    case IMGFMT_RGB15:
    case IMGFMT_BGR15:
    case IMGFMT_RGB16:
    case IMGFMT_BGR16:
    case IMGFMT_RGB24:
    case IMGFMT_BGR24:
    case IMGFMT_RGB32:
    case IMGFMT_BGR32:
	format->flags=VOCAP_SUPPORTED|VOCAP_HWSCALER;
	return MPXP_Ok; // hw supported w/conversion & osd
    }
    return MPXP_False;
}

static const vo_info_t* get_info(const vo_data_t*vo)
{
    UNUSED(vo);
    return &vo_info;
}


static void uninit(vo_data_t*vo)
{
#ifdef HAVE_X11
    saver_on(vo,vo->mDisplay);
    vo_x11_uninit(vo,vo->mDisplay, vo->window);
#endif
    sdl_close(vo);
#ifdef CONFIG_VIDIX
    vidix_term(vo);
    priv_t *priv = reinterpret_cast<priv_t*>(vo->priv);
    delete priv->vidix_server;
#endif
    delete vo->priv;
}

static MPXP_Rc __FASTCALL__ preinit(vo_data_t*vo,const char *arg)
{
    priv_t *priv = new(zeromem) priv_t;
    vo->priv=priv;
    priv->num_buffs = 1;
    priv->surface = NULL;
    if(arg) strcpy(sdl_subdevice,arg);
#ifdef HAVE_X11
    if(vo_x11_init(vo)!=MPXP_Ok) return MPXP_False; // Can't open X11
    saver_off(vo,vo->mDisplay);
#endif
    return sdl_open(vo);
}

static void __FASTCALL__ sdl_dri_get_surface_caps(const vo_data_t*vo,dri_surface_cap_t *caps)
{
    priv_t *priv = reinterpret_cast<priv_t*>(vo->priv);
    caps->caps = DRI_CAP_TEMP_VIDEO | DRI_CAP_UPSCALER | DRI_CAP_DOWNSCALER |
		 DRI_CAP_HORZSCALER | DRI_CAP_VERTSCALER;
    caps->fourcc = priv->format;
    caps->x=0;
    caps->y=priv->y;
    caps->w=priv->width;
    caps->h=priv->height;
    if(priv->mode == YUV) {
	if(priv->overlay[0]) {
	    int i,n;
	    caps->width=priv->overlay[0]->w;
	    caps->height=priv->overlay[0]->h;
	    n = std::min(4,priv->overlay[0]->planes);
	    for(i=0;i<n;i++)
		caps->strides[i] = priv->overlay[0]->pitches[i];
	    for(;i<4;i++)
		caps->strides[i] = 0;
	    if(priv->format == IMGFMT_YV12) {
		unsigned ts;
		ts = caps->strides[2];
		caps->strides[2] = caps->strides[1];
		caps->strides[1] = ts;
	    }
	}
    }
    else {
	if(priv->rgbsurface[0]) {
	    caps->width=priv->rgbsurface[0]->w;
	    caps->height=priv->rgbsurface[0]->h;
	    caps->strides[0] = priv->rgbsurface[0]->pitch;
	    caps->strides[1] = 0;
	    caps->strides[2] = 0;
	    caps->strides[3] = 0;
	}
    }
}

static void __FASTCALL__ sdl_dri_get_surface(const vo_data_t*vo,dri_surface_t *surf)
{
    priv_t *priv = reinterpret_cast<priv_t*>(vo->priv);
    if(priv->mode == YUV) {
	int i,n;
	n = std::min(4,priv->overlay[surf->idx]->planes);
	for(i=0;i<n;i++)
		surf->planes[i] = priv->overlay[surf->idx]->pixels[i];
	for(;i<4;i++)
		surf->planes[i] = 0;
	if(priv->format == IMGFMT_YV12) {
	    uint8_t* tp;
	    tp = surf->planes[2];
	    surf->planes[2] = surf->planes[1];
	    surf->planes[1] = tp;
	}
    } else {
	surf->planes[0] = reinterpret_cast<uint8_t*>(priv->rgbsurface[surf->idx]->pixels);
	surf->planes[1] = 0;
	surf->planes[2] = 0;
	surf->planes[3] = 0;
    }
}

static MPXP_Rc __FASTCALL__ control(vo_data_t*vo,uint32_t request, any_t*data)
{
    priv_t *priv = reinterpret_cast<priv_t*>(vo->priv);
#ifdef CONFIG_VIDIX
    if(priv->vidix_server)
	if(priv->vidix_server->control(vo,request,data)==MPXP_Ok) return MPXP_Ok;
#endif
  switch (request) {
    case VOCTRL_QUERY_FORMAT:
	return query_format(vo,(vo_query_fourcc_t*)data);
    case VOCTRL_GET_NUM_FRAMES:
	*(uint32_t *)data = priv->num_buffs;
	return MPXP_True;
    case DRI_GET_SURFACE_CAPS:
	sdl_dri_get_surface_caps(vo,reinterpret_cast<dri_surface_cap_t*>(data));
	return MPXP_True;
    case DRI_GET_SURFACE:
	sdl_dri_get_surface(vo,reinterpret_cast<dri_surface_t*>(data));
	return MPXP_True;
    case VOCTRL_CHECK_EVENTS: {
	vo_resize_t * vrest = (vo_resize_t *)data;
	vrest->event_type = check_events(vo,vrest->adjust_size);
	return MPXP_True;
    }
    case VOCTRL_FULLSCREEN:
	if (priv->surface->flags & SDL_FULLSCREEN) {
	    if(set_video_mode(vo,priv->windowsize.w, priv->windowsize.h, priv->bpp, priv->sdlflags)!=0) exit(EXIT_FAILURE);
	    SDL_ShowCursor(1);
	    MSG_V("SDL: Windowed mode\n");
	} else if (priv->fullmodes) {
	    if(set_fullmode(vo,priv->fullmode)!=0) exit(EXIT_FAILURE);
	    MSG_V("SDL: Set fullscreen mode\n");
	}
	*(uint32_t *)data = VO_EVENT_RESIZE;
	return MPXP_True;
  }
  return MPXP_NA;
}
