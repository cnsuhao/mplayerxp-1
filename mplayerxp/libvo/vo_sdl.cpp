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
#include <pthread.h>

#include "mplayerxp.h"
#include "xmpcore/xmp_core.h"

#include "video_out.h"

#include "osdep/fastmemcpy.h"
#include "sub.h"
#include "aspect.h"

#ifdef HAVE_X11
#include <X11/Xlib.h>
#include "x11_system.h"
#endif

#include "input2/input.h"
#include "input2/mouse.h"
#include "osdep/keycodes.h"
#include "dri_vo.h"
#include "video_out_internal.h"
#include "xmpcore/mp_image.h"
#ifdef CONFIG_VIDIX
#include "vidix_system.h"
#endif
#include "vo_msg.h"

int sdl_noxv;
int sdl_forcexv;
int sdl_forcegl;

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

typedef enum {
    YUV=0,
    RGB,
    BGR,
    GL
}sdl_mode_e;

class SDL_VO_Interface : public VO_Interface {
    public:
	SDL_VO_Interface(const char* args);
	virtual ~SDL_VO_Interface();

	virtual MPXP_Rc	configure(uint32_t width,
				uint32_t height,
				uint32_t d_width,
				uint32_t d_height,
				unsigned flags,
				const char *title,
				uint32_t format);
	virtual MPXP_Rc	select_frame(unsigned idx);
	virtual MPXP_Rc	flush_page(unsigned idx);
	virtual void	get_surface_caps(dri_surface_cap_t *caps) const;
	virtual void	get_surface(dri_surface_t *surf);
	virtual MPXP_Rc	query_format(vo_query_fourcc_t* format) const;
	virtual unsigned get_num_frames() const;

	virtual MPXP_Rc	toggle_fullscreen();
	virtual uint32_t check_events(const vo_resize_t*);
	virtual MPXP_Rc	ctrl(uint32_t request, any_t*data);
    private:
	void		sdl_open ( );
	void		sdl_close ( );
	MPXP_Rc		set_video_mode(int width, int height, int bpp, uint32_t sdlflags);
	MPXP_Rc		set_fullmode (int mode);
	MPXP_Rc		setup_surfaces();
	MPXP_Rc		setup_surface(unsigned idx);
	void		erase_rectangle(unsigned idx,int x, int y, int w, int h);
	void		lock_surfaces();
	void		unlock_surfaces();
	const char*	parse_sub_device(const char *sd) const;


	char		sdl_subdevice[100];
	char		driver[8]; /* output driver used by sdl */
	unsigned	flags;
	Aspect&		aspect;
	SDL_Surface*	surface; /* SDL display surface */
	SDL_Surface*	rgbsurface[MAX_DRI_BUFFERS]; /* SDL RGB surface */
	SDL_Overlay*	overlay[MAX_DRI_BUFFERS]; /* SDL YUV overlay */
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
	SDL_Rect	dirty_off_frame[2];
#ifdef CONFIG_VIDIX
	Vidix_System*	vidix;
#endif
#ifdef HAVE_X11
	X11_System&	x11;
#endif
	pthread_mutex_t	surfaces_mutex;
};

void SDL_VO_Interface::lock_surfaces() {
    pthread_mutex_lock(&surfaces_mutex);
}

void SDL_VO_Interface::unlock_surfaces() {
    pthread_mutex_unlock(&surfaces_mutex);
}

/** Private SDL Data structure **/
const char* SDL_VO_Interface::parse_sub_device(const char *sd) const
{
#ifdef CONFIG_VIDIX
    if(memcmp(sd,"vidix",5) == 0) return &sd[5]; /* vidix_name will be valid within init() */
#endif
    return NULL;
}

SDL_VO_Interface::~SDL_VO_Interface()
{
#ifdef HAVE_X11
    x11.saver_on();
#endif
    sdl_close();
    pthread_mutex_destroy(&surfaces_mutex);
#ifdef CONFIG_VIDIX
    if(vidix) delete vidix;
#endif
}

SDL_VO_Interface::SDL_VO_Interface(const char *arg)
		:VO_Interface(arg),
		aspect(*new(zeromem) Aspect(mp_conf.monitor_pixel_aspect))
#ifdef HAVE_X11
		,x11(*new(zeromem) X11_System(vo_conf.mDisplayName,vo_conf.xinerama_screen))
#endif
{
    const char* vidix_name=NULL;
    num_buffs = 1;
    surface = NULL;
    sdl_subdevice[0]='\0';
    if(arg) strcpy(sdl_subdevice,arg);
    if(arg) vidix_name = parse_sub_device(arg);
#ifdef CONFIG_VIDIX
    if(vidix_name) {
	if(!(vidix=new(zeromem) Vidix_System(vidix_name))) {
	    MSG_ERR("Cannot initialze vidix with '%s' argument\n",vidix_name);
	    exit_player("Vidix error");
	}
    }
#endif
#ifdef HAVE_X11
    x11.saver_off();
#endif
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutex_init(&surfaces_mutex,&attr);
    sdl_open();
}

static void __FASTCALL__ erase_area_4(int x_start, int width, int height, int pitch, uint32_t color, uint8_t* pixels);
static void __FASTCALL__ erase_area_1(int x_start, int width, int height, int pitch, uint8_t color, uint8_t* pixels);

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

void SDL_VO_Interface::sdl_open( )
{
    const SDL_VideoInfo *vidInfo = NULL;
    MSG_DBG3("SDL: Opening Plugin\n");
    if(sdl_subdevice[0]) setenv("SDL_VIDEODRIVER", sdl_subdevice, 1);

    /* does the user want SDL to try and force Xv */
    if(sdl_forcexv)	setenv("SDL_VIDEO_X11_NODIRECTCOLOR", "1", 1);

    /* does the user want to disable Xv and use software scaling instead */
    if(sdl_noxv) setenv("SDL_VIDEO_YUV_HWACCEL", "0", 1);

    /* default to no fullscreen mode, we'll set this as soon we have the avail. modes */
    fullmode = -2;

    fullmodes = NULL;
    bpp = 0;

    /* initialize the SDL Video system */
    if (!SDL_WasInit(SDL_INIT_VIDEO)) {
	if (SDL_Init (SDL_INIT_VIDEO|SDL_INIT_NOPARACHUTE)) {
	    MSG_ERR("SDL: Initializing of SDL failed: %s.\n", SDL_GetError());
	    exit_player("SDL error");
	}
    }

#ifdef CONFIG_VIDIX
    if(memcmp(sdl_subdevice,"vidix",5) != 0)
#endif
    SDL_VideoDriverName(driver, 8);
    MSG_OK("SDL: Using driver: %s\n", driver);
    /* other default values */
#ifdef SDL_NOHWSURFACE
    MSG_V("SDL: using software-surface\n");
    sdlflags = SDL_SWSURFACE|SDL_RESIZABLE|SDL_ASYNCBLIT|SDL_ANYFORMAT;
    sdlfullflags = SDL_SWSURFACE|SDL_FULLSCREEN|SDL_DOUBLEBUF|SDL_ASYNCBLIT|SDL_ANYFORMAT;
#else
    MSG_V("SDL: using hardware-surface\n");
    sdlflags = SDL_HWSURFACE|SDL_RESIZABLE|SDL_ASYNCBLIT|SDL_HWACCEL/*|SDL_ANYFORMAT*/;
    sdlfullflags = SDL_HWSURFACE|SDL_FULLSCREEN|SDL_DOUBLEBUF|SDL_ASYNCBLIT|SDL_HWACCEL/*|SDL_ANYFORMAT*/;
#endif
    if(sdl_forcegl) {
	sdlflags |= SDL_OPENGL|SDL_OPENGLBLIT|SDL_ANYFORMAT;
	sdlfullflags |= SDL_OPENGL|SDL_OPENGLBLIT|SDL_ANYFORMAT;
    }
    /* Setup Keyrepeats (500/30 are defaults) */
    SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, 100 /*SDL_DEFAULT_REPEAT_INTERVAL*/);

    /* get information about the graphics adapter */
    vidInfo = SDL_GetVideoInfo ();

    /* collect all fullscreen & hardware modes available */
    if (!(fullmodes = SDL_ListModes (vidInfo->vfmt, sdlfullflags))) {
	/* non hardware accelerated fullscreen modes */
	sdlfullflags &= ~SDL_HWSURFACE;
	fullmodes = SDL_ListModes (vidInfo->vfmt, sdlfullflags);
    }

    /* test for normal resizeable & windowed hardware accellerated surfaces */
    if (!SDL_ListModes (vidInfo->vfmt, sdlflags)) {
	/* test for NON hardware accelerated resizeable surfaces - poor you.
	 * That's all we have. If this fails there's nothing left.
	 * Theoretically there could be Fullscreenmodes left - we ignore this for now.
	 */
	sdlflags &= ~SDL_HWSURFACE;
	if ((!SDL_ListModes (vidInfo->vfmt, sdlflags)) && (!fullmodes)) {
	    MSG_ERR("SDL: Couldn't get any acceptable SDL Mode for output.\n");
	    exit_player("SDL error");
	}
    }
   /* YUV overlays need at least 16-bit color depth, but the
    * display might less. The SDL AAlib target says it can only do
    * 8-bits, for example. So, if the display is less than 16-bits,
    * we'll force the BPP to 16, and pray that SDL can emulate for us.
    */
    bpp = vidInfo->vfmt->BitsPerPixel;
    if (mode == YUV && bpp < 16) {
	MSG_V("SDL: Your SDL display target wants to be at a color "
		"depth of (%d), but we need it to be at least 16 "
		"bits, so we need to emulate 16-bit color. This is "
		"going to slow things down; you might want to "
		"increase your display's color depth, if possible.\n",
		bpp);
	bpp = 16;
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
}


/**
 * Close SDL, Cleanups, Free Memory
 *
 *    params : *plugin
 *   returns : non-zero on success, zero on error.
 **/

void SDL_VO_Interface::sdl_close ()
{
    unsigned i,n;
    n=num_buffs;
    for(i=0;i<n;i++) {
	/* Cleanup YUV Overlay structure */
	if (overlay[i])
	    SDL_FreeYUVOverlay(overlay[i]);
	/* Free RGB Surface */
	if (rgbsurface[i])
	    SDL_FreeSurface(rgbsurface[i]);
    }
    /* Free our blitting surface */
    if (surface) SDL_FreeSurface(surface);

    /* DONT attempt to mp_free the fullscreen modes array. SDL_Quit* does this for us */
    /* Cleanup SDL */
    if(SDL_WasInit(SDL_INIT_VIDEO))
	SDL_QuitSubSystem(SDL_INIT_VIDEO);

    MSG_DBG3("SDL: Closed Plugin\n");
}

/* Set video mode. Not fullscreen */
MPXP_Rc SDL_VO_Interface::set_video_mode(int _width, int _height, int _bpp, uint32_t _sdlflags)
{
    SDL_Surface* newsurface;
    MPXP_Rc retval=MPXP_False;
    newsurface = SDL_SetVideoMode(_width, _height, _bpp, _sdlflags);

    if(newsurface) {
	lock_surfaces();
	/* priv.surface will be NULL the first time this function is called. */
	if(surface)
	    SDL_FreeSurface(surface);

	surface = newsurface;
	dstwidth = _width;
	dstheight = _height;

	retval = setup_surfaces();
	unlock_surfaces();
    }
    else
	MSG_ERR("set_video_mode: SDL_SetVideoMode failed: %s\n", SDL_GetError());
    return retval;
}

MPXP_Rc SDL_VO_Interface::set_fullmode (int _mode) {
    SDL_Surface *newsurface = NULL;
    int screen_surface_w, screen_surface_h;
    MPXP_Rc retval=MPXP_False;

    /* if we haven't set a fullmode yet, default to the lowest res fullmode first */
    /* But select a mode where the full video enter */
    if(X && fulltype & VOFLAG_FULLSCREEN) {
	screen_surface_w = XWidth;
	screen_surface_h = XHeight;
    }
    else if (_mode < 0) {
	int i;
	_mode = 0; // Default to the biggest mode avaible
	for(i = findArrayEnd(fullmodes) - 1; i >=0; i--) {
	    if( (fullmodes[i]->w >= dstwidth) &&
		      (fullmodes[i]->h >= dstheight) ) {
		    _mode = i;
		    break;
	    }
	}
	fullmode = _mode;
	screen_surface_h = fullmodes[_mode]->h;
	screen_surface_w = fullmodes[_mode]->w;
    } else {
	screen_surface_h = fullmodes[_mode]->h;
	screen_surface_w = fullmodes[_mode]->w;
    }
    aspect.save_screen(screen_surface_w, screen_surface_h);

    /* calculate new video size/aspect */
    if(mode == YUV) {
	if(fulltype&VOFLAG_FULLSCREEN) aspect.save_screen(XWidth, XHeight);
	aspect.calc(dstwidth, dstheight, Aspect::NOZOOM);
    }

    /* try to change to given fullscreenmode */
    newsurface = SDL_SetVideoMode(dstwidth, screen_surface_h, bpp,
				sdlfullflags);

    /* if creation of new surface was successfull, save it and hide mouse cursor */
    if(newsurface) {
	lock_surfaces();
	if (surface) SDL_FreeSurface(surface);
	surface = newsurface;
	SDL_ShowCursor(0);
	SDL_FillRect(surface, NULL, 0);
	retval = setup_surfaces();
	unlock_surfaces();
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

MPXP_Rc SDL_VO_Interface::configure(uint32_t _width, uint32_t _height, uint32_t d_width, uint32_t d_height,unsigned _flags, const char *title, uint32_t _format)
//static int sdl_setup (int width, int height)
{
    MPXP_Rc retval;
    flags=_flags;

    if(sdl_forcegl) mode = GL;
    else
    switch(_format){
	case IMGFMT_I420:
	case IMGFMT_YV12:
	case IMGFMT_IYUV:
	case IMGFMT_YUY2:
	case IMGFMT_UYVY:
	case IMGFMT_YVYU:
	    mode = YUV;
	    break;
	case IMGFMT_BGR15:
	case IMGFMT_BGR16:
	case IMGFMT_BGR24:
	case IMGFMT_BGR32:
	    mode = BGR;
	    break;
	case IMGFMT_RGB15:
	case IMGFMT_RGB16:
	case IMGFMT_RGB24:
	case IMGFMT_RGB32:
	    mode = RGB;
	    break;
	default:
	    MSG_ERR("SDL: Unsupported image format (0x%X)\n",_format);
	    return MPXP_False;
    }

    MSG_V("SDL: Using 0x%X (%s) image format\n", _format,
	vo_format_name(_format));

    if(mode != YUV) {
	sdlflags |= SDL_ANYFORMAT;
	sdlfullflags |= SDL_ANYFORMAT;
    }
    /* SDL can only scale YUV data */
    if(mode == RGB || mode == BGR) {
	d_width = _width;
	d_height = _height;
    }
    aspect.save_image(_width,_height,d_width,d_height);

    /* Save the original Image size */
    X = 0;
    width  = _width;
    height = _height;
    dstwidth  = d_width ? d_width : _width;
    dstheight = d_height ? d_height : _height;

    format = _format;

    /* Set output window title */
    SDL_WM_SetCaption (".: MPlayerXP : F = Fullscreen/Windowed : C = Cycle Fullscreen Resolutions :.", title);

    if(mode == GL) {
	switch(_format){
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
		MSG_ERR("SDL: Unsupported image format in GL mode (0x%X)\n",_format);
		return MPXP_False;
	}
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
    }
    if(X) {
	aspect.save_screen(XWidth,XHeight);
	aspect.calc(dstwidth,dstheight,flags&VOFLAG_FULLSCREEN?Aspect::ZOOM:Aspect::NOZOOM);
    }
    windowsize.w = dstwidth;
    windowsize.h = dstheight;

    /* bit 0 (0x01) means fullscreen (-fs)
     * bit 1 (0x02) means mode switching (-vm)
     * bit 2 (0x04) enables software scaling (-zoom)
     * bit 3 (0x08) enables flipping (-flip)
     */
    if(flags&VOFLAG_FLIPPING) {
	MSG_V("SDL: using flipped video (only with RGB/BGR/packed YUV)\n");
	flip = 1;
    }
    if(flags&VOFLAG_FULLSCREEN) {
	MSG_V("SDL: setting zoomed fullscreen without modeswitching\n");
	MSG_V("SDL: Info - please use -vm or -zoom to switch to best resolution.\n");
	fulltype = VOFLAG_FULLSCREEN;
	retval = set_fullmode(fullmode);
	if(retval!=MPXP_Ok) return retval;
    } else if(flags&VOFLAG_MODESWITCHING) {
	MSG_V("SDL: setting zoomed fullscreen with modeswitching\n");
	fulltype = VOFLAG_MODESWITCHING;
	set_fullmode(fullmode);
    } else if(flags&VOFLAG_SWSCALE) {
	MSG_V("SDL: setting zoomed fullscreen with modeswitching\n");
	fulltype = VOFLAG_SWSCALE;
	retval = set_fullmode(fullmode);
	if(retval!=MPXP_Ok) return retval;
    } else {
	if((strcmp(driver, "x11") == 0)
	    ||(strcmp(driver, "windib") == 0)
	    ||(strcmp(driver, "directx") == 0)
	    ||((strcmp(driver, "aalib") == 0)
	    && X)) {
		MSG_V("SDL: setting windowed mode\n");
		retval = set_video_mode(dstwidth, dstheight, bpp, sdlflags);
		if(retval!=MPXP_Ok) return retval;
	    } else {
		MSG_V("SDL: setting zoomed fullscreen with modeswitching\n");
		fulltype = VOFLAG_SWSCALE;
		retval = set_fullmode(fullmode);
		if(retval!=MPXP_Ok) return retval;
	    }
    }

    if(!surface) { // cannot SetVideoMode
	MSG_ERR("SDL: failed to set video mode: %s\n", SDL_GetError());
	return MPXP_False;
    }
    return MPXP_Ok;
}

MPXP_Rc SDL_VO_Interface::setup_surfaces( )
{
    unsigned i;
    MPXP_Rc retval;
    num_buffs=vo_conf.xp_buffs;
#ifdef CONFIG_VIDIX
    if(vidix) {
	if(vidix->configure(width,height,0,y,
			dstwidth,dstheight,format,bpp,
			XWidth,XHeight) != MPXP_Ok) {
	    MSG_ERR("vo_sdl: Can't initialize VIDIX driver\n");
	    return MPXP_False;
	} else MSG_V("vo_sdl: Using VIDIX\n");
	if(vidix->start()!=0) return MPXP_False;
    } else
#endif
    for(i=0;i<num_buffs;i++) {
	retval = setup_surface(i);
	if(retval!=MPXP_Ok) return retval;
    }
    return MPXP_Ok;
}

/* Free priv.rgbsurface or priv.overlay if they are != NULL.
 * Setup priv.rgbsurface or priv.overlay depending on source format.
 * The size of the created surface or overlay depends on the size of
 * priv.surface, priv.width, priv.height, priv.dstwidth and priv.dstheight.
 */
MPXP_Rc SDL_VO_Interface::setup_surface(unsigned idx)
{
    float v_scale = ((float) dstheight) / height;
    int surfwidth, surfheight;
    surfwidth = width;
    surfheight = height + (surface->h - dstheight) / v_scale;
    surfheight&= ~1;
    /* Place the image in the middle of the screen */
    y = (surfheight - height) / 2;
    y_screen_top = y * v_scale;
    y_screen_bottom = y_screen_top + dstheight;

    dirty_off_frame[0].x = -1;
    dirty_off_frame[0].y = -1;
    dirty_off_frame[1].x = -1;
    dirty_off_frame[1].y = -1;

    /* Make sure the entire screen is updated */
    vo_osd_changed(1);

    if(rgbsurface[idx])
	SDL_FreeSurface(rgbsurface[idx]);
    else if(overlay[idx])
	SDL_FreeYUVOverlay(overlay[idx]);

    rgbsurface[idx] = NULL;
    overlay[idx] = NULL;

    switch(format) {
	/* Initialize and create the RGB Surface used for video out in BGR/RGB mode */
	//SDL_Surface *SDL_CreateRGBSurface(Uint32 flags, int width, int height, int depth, Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask);
	//	SDL_SWSURFACE,SDL_HWSURFACE,SDL_SRCCOLORKEY, priv.flags?	guess: exchange Rmask and Bmask for BGR<->RGB
	// 32 bit: a:ff000000 r:ff000 g:ff00 b:ff
	// 24 bit: r:ff0000 g:ff00 b:ff
	// 16 bit: r:1111100000000000b g:0000011111100000b b:0000000000011111b
	// 15 bit: r:111110000000000b g:000001111100000b b:000000000011111b
	// FIXME: colorkey detect based on bpp, FIXME static bpp value, FIXME alpha value correct?
	case IMGFMT_RGB15:
	    rgbsurface[idx] = SDL_CreateRGBSurface (SDL_SRCCOLORKEY, surfwidth, surfheight, 15, 31, 992, 31744, 0);
	    break;
	case IMGFMT_BGR15:
	    rgbsurface[idx] = SDL_CreateRGBSurface (SDL_SRCCOLORKEY, surfwidth, surfheight, 15, 31744, 992, 31, 0);
	    break;
	case IMGFMT_RGB16:
	    rgbsurface[idx] = SDL_CreateRGBSurface (SDL_SRCCOLORKEY, surfwidth, surfheight, 16, 31, 2016, 63488, 0);
	    break;
	case IMGFMT_BGR16:
	    rgbsurface[idx] = SDL_CreateRGBSurface (SDL_SRCCOLORKEY, surfwidth, surfheight, 16, 63488, 2016, 31, 0);
	    break;
	case IMGFMT_RGB24:
	    rgbsurface[idx] = SDL_CreateRGBSurface (SDL_SRCCOLORKEY, surfwidth, surfheight, 24, 0x0000FF, 0x00FF00, 0xFF0000, 0);
	    break;
	case IMGFMT_BGR24:
	    rgbsurface[idx] = SDL_CreateRGBSurface (SDL_SRCCOLORKEY, surfwidth, surfheight, 24, 0xFF0000, 0x00FF00, 0x0000FF, 0);
	    break;
	case IMGFMT_RGB32:
	    rgbsurface[idx] = SDL_CreateRGBSurface (SDL_SRCCOLORKEY, surfwidth, surfheight, 32, 0x000000FF, 0x0000FF00, 0x00FF0000, 0/*0xFF000000*/);
	    break;
	case IMGFMT_BGR32:
	    rgbsurface[idx] = SDL_CreateRGBSurface (SDL_SRCCOLORKEY, surfwidth, surfheight, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0/*0xFF000000*/);
	    break;
	default:
	    /* Initialize and create the YUV Overlay used for video out */
	    if (!(overlay[idx] = SDL_CreateYUVOverlay (surfwidth, surfheight, format==IMGFMT_I420?IMGFMT_IYUV:format, surface))) {
		MSG_ERR ("SDL: Couldn't create a YUV overlay: %s\n", SDL_GetError());
		return MPXP_False;
	    }
    }
    if(mode != YUV && mode != GL) {
	if(!rgbsurface[idx]) {
	    MSG_ERR ("SDL: Couldn't create a RGB surface: %s\n", SDL_GetError());
	    return MPXP_False;
	}

	if((format&0xFF) != bpp)
	    MSG_WARN("SDL: using depth/colorspace conversion, this will slow things"
		   "down (%ibpp -> %ibpp).\n", format&0xFF, bpp);
    }
    erase_rectangle(idx,0, 0, surfwidth, surfheight);

    return MPXP_Ok;
}

/**
 * Checks for SDL keypress and window resize events
 *
 *   params : none
 *  returns : doesn't return
 **/

#define shift_key (event.key.keysym.mod==(KMOD_LSHIFT||KMOD_RSHIFT))
uint32_t SDL_VO_Interface::check_events (const vo_resize_t* vrest){
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
		(*vrest->adjust_size)(vrest->vo,windowsize.w,windowsize.h,reinterpret_cast<unsigned*>(&event.resize.w), reinterpret_cast<unsigned*>(&event.resize.h));
		if(set_video_mode(event.resize.w, event.resize.h,
				  bpp, sdlflags)!=0)
				  exit_player("SDL set video mode");

		/* save video extents, to restore them after going fullscreen */
		windowsize.w = surface->w;
		windowsize.h = surface->h;
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
		if ( ((keypressed == SDLK_c)) && (fullmodes) ) {
		    /* select next fullscreen mode */
		    fullmode++;
		    if (fullmode > (findArrayEnd(fullmodes) - 1)) fullmode = 0;
		    if(set_fullmode(fullmode)!=0) exit_player("SDL set full mode");
		    MSG_V("SDL: Set next available fullscreen mode.\n");
		    retval = VO_EVENT_RESIZE;
		} else if ( keypressed == SDLK_n ) {
#ifdef HAVE_X11
		    aspect.calc(dstwidth, dstheight,flags&VOFLAG_FULLSCREEN?Aspect::ZOOM:Aspect::NOZOOM);
#endif
		    if (unsigned(surface->w) != dstwidth || unsigned(surface->h) != dstheight) {
			if(set_video_mode(dstwidth, dstheight, bpp, sdlflags)!=0) exit_player("SDL set video mode");
			windowsize.w = surface->w;
			windowsize.h = surface->h;
			MSG_V("SDL: Normal size\n");
			retval |= VO_EVENT_RESIZE;
		    } else if (unsigned(surface->w) != dstwidth * 2 || unsigned(surface->h) != dstheight * 2) {
			if(set_video_mode(dstwidth * 2, dstheight * 2, bpp, sdlflags)!=0) exit_player("SDL set video mode");
			windowsize.w = surface->w;
			windowsize.h = surface->h;
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
void SDL_VO_Interface::erase_rectangle(unsigned idx,int x, int _y, int w, int h)
{
    switch(format) {
	case IMGFMT_YV12:
	case IMGFMT_I420:
	case IMGFMT_IYUV:
	{
	    /* Erase Y plane */
	    erase_area_1(x, w, h,
			overlay[idx]->pitches[0], 0,
			overlay[idx]->pixels[0] +
			overlay[idx]->pitches[0]*_y);
	    /* Erase U and V planes */
	    w /= 2;
	    x /= 2;
	    h /= 2;
	    _y /= 2;

	    erase_area_1(x, w, h,
			overlay[idx]->pitches[1], 128,
			overlay[idx]->pixels[1] +
			overlay[idx]->pitches[1]*_y);

	    erase_area_1(x, w, h,
			overlay[idx]->pitches[2], 128,
			overlay[idx]->pixels[2] +
			overlay[idx]->pitches[2]*_y);
	    break;
	}
	case IMGFMT_YUY2:
	case IMGFMT_YVYU:
	case IMGFMT_UYVY:
	{
	    /* yuy2 and yvyu represent black the same way */
	    uint8_t yuy2_black[] = {16, 128, 16, 128};
	    uint8_t uyvy_black[] = {128, 16, 128, 16};
	    erase_area_4(x*2, w*2, h,
			overlay[idx]->pitches[0],
			format == IMGFMT_UYVY ? *((uint32_t*) uyvy_black):
			(*(uint32_t*) yuy2_black),
			overlay[idx]->pixels[0] +
			overlay[idx]->pitches[0]*_y);
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
	    rect.x = x; rect.y = _y;

	    SDL_FillRect(rgbsurface[idx], &rect, 0);
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

MPXP_Rc SDL_VO_Interface::select_frame(unsigned idx)
{
#ifdef CONFIG_VIDIX
    if(vidix) return vidix->select_frame(idx);
#endif

    if(mode == YUV) {
	/* blit to the YUV overlay */
	SDL_DisplayYUVOverlay (overlay[idx], &surface->clip_rect);

	/* check if we have a double buffered surface and flip() if we do. */
	if ( surface->flags & SDL_DOUBLEBUF )
		SDL_Flip(surface);

	//SDL_LockYUVOverlay (overlay); // removed because unused!?
    } else {
	/* blit to the RGB surface */
	if(SDL_BlitSurface (rgbsurface[idx], NULL, surface, NULL))
	    MSG_ERR("SDL: Blit failed: %s\n", SDL_GetError());

	/* update screen */
	if(sdl_forcegl) SDL_UpdateRects(surface, 1, &surface->clip_rect);
	else {
	    if(osd_has_changed) {
		osd_has_changed = 0;
		SDL_UpdateRects(surface, 1, &surface->clip_rect);
	    } else
		SDL_UpdateRect(surface, 0, y_screen_top,
				surface->clip_rect.w, y_screen_bottom);
	}
	/* check if we have a double buffered surface and flip() if we do. */
	if(sdl_forcegl) SDL_GL_SwapBuffers();
	else if(surface->flags & SDL_DOUBLEBUF ) SDL_Flip(surface);
    }
    return MPXP_Ok;
}

MPXP_Rc SDL_VO_Interface::query_format(vo_query_fourcc_t* fmt) const
{
#ifdef CONFIG_VIDIX
    if(vidix) return vidix->query_fourcc(fmt);
#endif
    if(sdl_forcegl) {
	if (IMGFMT_IS_BGR(fmt->fourcc)) {
	    if  (rgbfmt_depth(fmt->fourcc) == (unsigned)bpp &&
		((unsigned)bpp==16 || (unsigned)bpp == 32))
			fmt->flags=VOCAP_SUPPORTED|VOCAP_HWSCALER;
			return MPXP_Ok;
	}
	return MPXP_False;
    }
    else
    switch(fmt->fourcc){
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
	fmt->flags=VOCAP_SUPPORTED|VOCAP_HWSCALER;
	return MPXP_Ok; // hw supported w/conversion & osd
    }
    return MPXP_False;
}

void SDL_VO_Interface::get_surface_caps(dri_surface_cap_t *caps) const
{
#ifdef CONFIG_VIDIX
    if(vidix) return vidix->get_surface_caps(caps);
#endif
    caps->caps = DRI_CAP_TEMP_VIDEO | DRI_CAP_UPSCALER | DRI_CAP_DOWNSCALER |
		 DRI_CAP_HORZSCALER | DRI_CAP_VERTSCALER;
    caps->fourcc = format;
    caps->x=0;
    caps->y=y;
    caps->w=width;
    caps->h=height;
    if(mode == YUV) {
	if(overlay[0]) {
	    int i,n;
	    caps->width=overlay[0]->w;
	    caps->height=overlay[0]->h;
	    n = std::min(4,overlay[0]->planes);
	    for(i=0;i<n;i++)
		caps->strides[i] = overlay[0]->pitches[i];
	    for(;i<4;i++)
		caps->strides[i] = 0;
	    if(format == IMGFMT_YV12) {
		unsigned ts;
		ts = caps->strides[2];
		caps->strides[2] = caps->strides[1];
		caps->strides[1] = ts;
	    }
	}
    }
    else {
	if(rgbsurface[0]) {
	    caps->width=rgbsurface[0]->w;
	    caps->height=rgbsurface[0]->h;
	    caps->strides[0] = rgbsurface[0]->pitch;
	    caps->strides[1] = 0;
	    caps->strides[2] = 0;
	    caps->strides[3] = 0;
	}
    }
}

void SDL_VO_Interface::get_surface(dri_surface_t *surf)
{
#ifdef CONFIG_VIDIX
    if(vidix) return vidix->get_surface(surf);
#endif
    lock_surfaces();
    if(mode == YUV) {
	int i,n;
	n = std::min(4,overlay[surf->idx]->planes);
	for(i=0;i<n;i++)
		surf->planes[i] = overlay[surf->idx]->pixels[i];
	for(;i<4;i++)
		surf->planes[i] = 0;
	if(format == IMGFMT_YV12) {
	    uint8_t* tp;
	    tp = surf->planes[2];
	    surf->planes[2] = surf->planes[1];
	    surf->planes[1] = tp;
	}
    } else {
	surf->planes[0] = reinterpret_cast<uint8_t*>(rgbsurface[surf->idx]->pixels);
	surf->planes[1] = 0;
	surf->planes[2] = 0;
	surf->planes[3] = 0;
    }
    unlock_surfaces();
}

unsigned SDL_VO_Interface::get_num_frames() const {
#ifdef CONFIG_VIDIX
    if(vidix) return vidix->get_num_frames();
#endif
    return num_buffs;
}

MPXP_Rc SDL_VO_Interface::flush_page(unsigned idx) {
#ifdef CONFIG_VIDIX
    if(vidix) return vidix->flush_page(idx);
#endif
    return MPXP_False;
}

MPXP_Rc SDL_VO_Interface::toggle_fullscreen() {
    if (surface->flags & SDL_FULLSCREEN) {
	if(set_video_mode(windowsize.w, windowsize.h, bpp, sdlflags)!=0) exit_player("SDL set fullscreen");
	SDL_ShowCursor(1);
	MSG_V("SDL: Windowed mode\n");
    } else if (fullmodes) {
	if(set_fullmode(fullmode)!=0) exit_player("SDL set fullmode");
	MSG_V("SDL: Set fullscreen mode\n");
    }
    return MPXP_True;
}

MPXP_Rc SDL_VO_Interface::ctrl(uint32_t request, any_t*data)
{
#ifdef CONFIG_VIDIX
    switch (request) {
	case VOCTRL_SET_EQUALIZER:
	    if(!vidix->set_video_eq(reinterpret_cast<vo_videq_t*>(data))) return MPXP_True;
	    return MPXP_False;
	case VOCTRL_GET_EQUALIZER:
	    if(vidix->get_video_eq(reinterpret_cast<vo_videq_t*>(data))) return MPXP_True;
	    return MPXP_False;
    }
#endif
    return MPXP_NA;
}

static VO_Interface* query_interface(const char* args) { return new(zeromem) SDL_VO_Interface(args); }
extern const vo_info_t sdl_vo_info =
{
    "SDL YUV/RGB/BGR renderer (SDL v1.1.7+ !)"
#ifdef CONFIG_VIDIX
    " (with sdl:vidix subdevice)"
#endif
    ,
    "sdl",
    "Ryan C. Gordon <icculus@lokigames.com>, Felix Buenemann <atmosfear@users.sourceforge.net>",
    "",
    query_interface
};
