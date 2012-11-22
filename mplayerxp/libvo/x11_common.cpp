#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "mp_config.h"
#include "mplayerxp.h"
#include "osdep/mplib.h"
#ifdef HAVE_X11

#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#include "video_out.h"
#include "x11_common.h"
#include <X11/Xmd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#ifdef HAVE_XDPMS
#include <X11/extensions/dpms.h>
#endif

#ifdef HAVE_XINERAMA
#include <X11/extensions/Xinerama.h>
#endif

#ifdef HAVE_XF86VM
#include <X11/extensions/xf86vmode.h>
#include <X11/XF86keysym.h>
#endif

#include "input2/input.h"
#include "input2/mouse.h"
#include "vo_msg.h"

#ifdef HAVE_SHM
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>

/* since it doesn't seem to be defined on some platforms */
extern int XShmGetEventBase( Display* );
#endif

using namespace mpxp;

/*
 * If SCAN_VISUALS is defined, vo_x11_init() scans all available TrueColor
 * visuals for the 'best' visual for MPlayer video display.  Note that
 * the 'best' visual might be different from the default visual that
 * is in use on the root window of the display/screen.
 */
#define	SCAN_VISUALS

typedef struct priv_s {
    int			mLocalDisplay;
    Window		mRootWin;

    int			dpms_disabled;
    int			timeout_save;

    vo_rect_t		prev;

    XImage *		myximage[MAX_DRI_BUFFERS];
    int			Shmem_Flag;
#ifdef HAVE_XINERAMA
    int			xinerama_x;
    int			xinerama_y;
#endif
#ifdef HAVE_SHM
    XShmSegmentInfo	Shminfo[MAX_DRI_BUFFERS];
    int			gXErrorFlag;
    int			CompletionType;
#endif
#ifdef HAVE_XF86VM
    XF86VidModeModeInfo **vidmodes;
    XF86VidModeModeLine	modeline;
#endif
}priv_t;

int __FASTCALL__ vo_x11_Shmem_Flag(const vo_data_t*vo)
{
    priv_t*priv=(priv_t*)vo->priv2;
    return priv->Shmem_Flag;
}

XImage* __FASTCALL__ vo_x11_Image(const vo_data_t*vo,unsigned idx)
{
    priv_t*priv=(priv_t*)vo->priv2;
    return priv->myximage[idx];
}

uint8_t* __FASTCALL__ vo_x11_ImageData(const vo_data_t*vo,unsigned idx)
{
    return (uint8_t*)vo_x11_Image(vo,idx)->data;
}

void __FASTCALL__ vo_x11_getMyXImage(const vo_data_t*vo,unsigned idx,Visual *visual,unsigned depth,unsigned w,unsigned h)
{
    priv_t*priv=(priv_t*)vo->priv2;
#ifdef HAVE_SHM
    if ( priv->mLocalDisplay && XShmQueryExtension( vo->mDisplay )) priv->Shmem_Flag=1;
    else {
	priv->Shmem_Flag=0;
	MSG_V( "Shared memory not supported\nReverting to normal Xlib\n" );
    }
    if ( priv->Shmem_Flag ) {
	priv->CompletionType=XShmGetEventBase( vo->mDisplay ) + ShmCompletion;
	priv->myximage[idx]=XShmCreateImage( vo->mDisplay,visual,depth,ZPixmap,NULL,&priv->Shminfo[idx],w,h);
	if ( priv->myximage[idx] == NULL ) {
	    if ( priv->myximage[idx] != NULL ) XDestroyImage( priv->myximage[idx] );
	    MSG_V( "Shared memory error,disabling ( Ximage error )\n" );
	    goto shmemerror;
	}
	priv->Shminfo[idx].shmid=shmget( IPC_PRIVATE,
		priv->myximage[idx]->bytes_per_line * priv->myximage[idx]->height,IPC_CREAT|0777);
	if ( priv->Shminfo[idx].shmid < 0 ) {
	    XDestroyImage( priv->myximage[idx] );
	    MSG_V( "%s\n",strerror( errno ) );
	    MSG_V( "Shared memory error,disabling ( seg id error )\n" );
	    goto shmemerror;
	}
	priv->Shminfo[idx].shmaddr=( char * ) shmat( priv->Shminfo[idx].shmid,0,0 );

	if ( priv->Shminfo[idx].shmaddr == ( ( char * ) -1 ) ) {
	    XDestroyImage( priv->myximage[idx] );
	    if ( priv->Shminfo[idx].shmaddr != ( ( char * ) -1 ) ) shmdt( priv->Shminfo[idx].shmaddr );
	    MSG_V( "Shared memory error,disabling ( address error )\n" );
	    goto shmemerror;
	}
	priv->myximage[idx]->data=priv->Shminfo[idx].shmaddr;
	priv->Shminfo[idx].readOnly=False;
	XShmAttach( vo->mDisplay,&priv->Shminfo[idx] );

	XSync( vo->mDisplay,False );

	if ( priv->gXErrorFlag ) {
	    XDestroyImage( priv->myximage[idx] );
	    shmdt( priv->Shminfo[idx].shmaddr );
	    MSG_V( "Shared memory error,disabling.\n" );
	    priv->gXErrorFlag=0;
	    goto shmemerror;
	} else shmctl( priv->Shminfo[idx].shmid,IPC_RMID,0 );
	static int firstTime=1;
	if (firstTime) {
	    MSG_V( "Sharing memory.\n" );
	    firstTime=0;
	}
    } else {
	shmemerror:
	priv->Shmem_Flag=0;
#endif
	priv->myximage[idx]=XGetImage( vo->mDisplay,vo->window,0,0,
				w,h,AllPlanes,ZPixmap );
#ifdef HAVE_SHM
    }
#endif
}

void __FASTCALL__ vo_x11_freeMyXImage(vo_data_t*vo,unsigned idx)
{
    priv_t*priv=(priv_t*)vo->priv2;
#ifdef HAVE_SHM
    if ( priv->Shmem_Flag ) {
	XShmDetach( vo->mDisplay,&priv->Shminfo[idx] );
	XDestroyImage( priv->myximage[idx] );
	shmdt( priv->Shminfo[idx].shmaddr );
    } else
#endif
    {
	XDestroyImage( priv->myximage[idx] );
    }
    priv->myximage[idx]=NULL;
}


void __FASTCALL__ vo_x11_hidecursor ( Display *disp, Window win )
{
	Cursor no_ptr;
	Pixmap bm_no;
	XColor black,dummy;
	Colormap colormap;
	static char bm_no_data[] = { 0,0,0,0, 0,0,0,0  };

	if(vo_conf.WinID==0) return;	// do not hide, if we're playing at rootwin

	colormap = DefaultColormap(disp,DefaultScreen(disp));
	XAllocNamedColor(disp,colormap,"black",&black,&dummy);
	bm_no = XCreateBitmapFromData(disp, win, bm_no_data, 8,8);
	no_ptr=XCreatePixmapCursor(disp, bm_no, bm_no,&black, &black,0, 0);
	XDefineCursor(disp,win,no_ptr);
}


#ifdef	SCAN_VISUALS
/*
 * Scan the available visuals on this Display/Screen.  Try to find
 * the 'best' available TrueColor visual that has a decent color
 * depth (at least 15bit).  If there are multiple visuals with depth
 * >= 15bit, we prefer visuals with a smaller color depth.
 */
static int __FASTCALL__ vo_find_depth_from_visuals(Display *dpy, int screen, Visual **visual_return)
{
  XVisualInfo visual_tmpl;
  XVisualInfo *visuals;
  int nvisuals, i;
  int bestvisual = -1;
  int bestvisual_depth = -1;

  visual_tmpl.screen = screen;
  visual_tmpl.c_class = TrueColor;
  visuals = XGetVisualInfo(dpy,
			   VisualScreenMask | VisualClassMask, &visual_tmpl,
			   &nvisuals);
  if (visuals != NULL) {
    for (i = 0; i < nvisuals; i++) {
	MSG_V("vo: X11 truecolor visual %#x, depth %d, R:%lX G:%lX B:%lX\n",
	       visuals[i].visualid, visuals[i].depth,
	       visuals[i].red_mask, visuals[i].green_mask,
	       visuals[i].blue_mask);
      /*
       * save the visual index and it's depth, if this is the first
       * truecolor visul, or a visual that is 'preferred' over the
       * previous 'best' visual
       */
      if (bestvisual_depth == -1
	  || (visuals[i].depth >= 15
	      && (   visuals[i].depth < bestvisual_depth
		  || bestvisual_depth < 15))) {
	bestvisual = i;
	bestvisual_depth = visuals[i].depth;
      }
    }

    if (bestvisual != -1 && visual_return != NULL)
      *visual_return = visuals[bestvisual].visual;

    XFree(visuals);
  }
  return bestvisual_depth;
}
#endif

static int x11_errorhandler(Display *display, XErrorEvent *event)
{
#define MSGLEN 60
    char msg[MSGLEN];

    XGetErrorText(display, event->error_code, (char *)&msg, MSGLEN);

    MSG_ERR("X11 error: %s\n", msg);

    MSG_V("Type: %x, display: %x, resourceid: %x, serial: %lx\n",
	    event->type, event->display, event->resourceid, event->serial);
    MSG_V("Error code: %x, request code: %x, minor code: %x\n",
	    event->error_code, event->request_code, event->minor_code);

    exit_player("X11 error");
#undef MSGLEN
    return 0;
}

MPXP_Rc vo_x11_init(vo_data_t*vo)
{
    unsigned depth, bpp;
    unsigned int mask;
// char    * DisplayName = ":0.0";
    XImage  * mXImage = NULL;
    XWindowAttributes attribs;
    char* dispName;

    if(vo->depthonscreen) return MPXP_Ok; // already called

    vo->priv2=mp_mallocz(sizeof(priv_t));
    priv_t*priv=(priv_t*)vo->priv2;
    priv->CompletionType=-1;

    XSetErrorHandler(x11_errorhandler);

#if 0
    if (!vo->mDisplayName)
	if (!(vo->mDisplayName=getenv("DISPLAY")))
	    vo->mDisplayName=mp_strdup(":0.0");
#else
    dispName = XDisplayName(vo_conf.mDisplayName);
#endif

    MSG_V("X11 opening display: %s\n", dispName);

    vo->mDisplay=XOpenDisplay(dispName);
    if ( !vo->mDisplay ) {
	MSG_ERR( "vo: couldn't open the X11 display (%s)!\n",dispName );
	return MPXP_False;
    }
    vo->mScreen=DefaultScreen( vo->mDisplay );     // Screen ID.
    priv->mRootWin=RootWindow( vo->mDisplay,vo->mScreen );// Root window ID.

#ifdef HAVE_XINERAMA
    if(XineramaIsActive(vo->mDisplay)) {
	XineramaScreenInfo *screens;
	int num_screens;

	screens = XineramaQueryScreens(vo->mDisplay, &num_screens);
	if(mp_conf.xinerama_screen >= num_screens) mp_conf.xinerama_screen = 0;
	if (! vo_conf.screenwidth)
	    vo_conf.screenwidth=screens[mp_conf.xinerama_screen].width;
	if (! vo_conf.screenheight)
	    vo_conf.screenheight=screens[mp_conf.xinerama_screen].height;
	priv->xinerama_x = screens[mp_conf.xinerama_screen].x_org;
	priv->xinerama_y = screens[mp_conf.xinerama_screen].y_org;

	XFree(screens);
    } else
#endif
#ifdef HAVE_XF86VM
    {
	int clock;
	XF86VidModeGetModeLine( vo->mDisplay,vo->mScreen,&clock ,&priv->modeline );
	if ( !vo_conf.screenwidth )  vo_conf.screenwidth=priv->modeline.hdisplay;
	if ( !vo_conf.screenheight ) vo_conf.screenheight=priv->modeline.vdisplay;
    }
#endif
    if (! vo_conf.screenwidth)
	vo_conf.screenwidth=DisplayWidth( vo->mDisplay,vo->mScreen );
    if (! vo_conf.screenheight)
	vo_conf.screenheight=DisplayHeight( vo->mDisplay,vo->mScreen );
    // get color depth (from root window, or the best visual):
    XGetWindowAttributes(vo->mDisplay, priv->mRootWin, &attribs);
    depth=attribs.depth;

#ifdef	SCAN_VISUALS
    if (depth != 15 && depth != 16 && depth != 24 && depth != 32) {
	Visual *visual;

	depth = vo_find_depth_from_visuals(vo->mDisplay, vo->mScreen, &visual);
	if ((int)depth != -1)
	    mXImage=XCreateImage(vo->mDisplay, visual, depth, ZPixmap,
			  0, NULL, 1, 1, 8, 1);
    } else
#endif
	mXImage=XGetImage( vo->mDisplay,priv->mRootWin,0,0,1,1,AllPlanes,ZPixmap );

    vo->depthonscreen = depth;	// display depth on screen

    // get bits/pixel from XImage structure:
    if (mXImage == NULL) mask = 0;
    else {
   /*
    * for the depth==24 case, the XImage structures might use
    * 24 or 32 bits of data per pixel.  The global variable
    * vo_depthonscreen stores the amount of data per pixel in the
    * XImage structure!
    *
    * Maybe we should rename vo_depthonscreen to (or add) vo_bpp?
    */
	bpp=mXImage->bits_per_pixel;
	if((vo->depthonscreen+7)/8 != (bpp+7)/8) vo->depthonscreen=bpp; // by A'rpi
	mask=mXImage->red_mask|mXImage->green_mask|mXImage->blue_mask;
	MSG_V("vo: X11 color mask:  %X  (R:%lX G:%lX B:%lX)\n",
	    mask,mXImage->red_mask,mXImage->green_mask,mXImage->blue_mask);
	XDestroyImage( mXImage );
    }
    if(((vo->depthonscreen+7)/8)==2){
	if(mask==0x7FFF) vo->depthonscreen=15; else
	if(mask==0xFFFF) vo->depthonscreen=16;
    }
// XCloseDisplay( vo->mDisplay );
/* slightly improved local display detection AST */
    if ( strncmp(dispName, "unix:", 5) == 0)
		dispName += 4;
    else if ( strncmp(dispName, "localhost:", 10) == 0)
		dispName += 9;
    if (*dispName==':') priv->mLocalDisplay=1; else priv->mLocalDisplay=0;
    MSG_V("vo: X11 running at %dx%d with depth %d and %d bits/pixel (\"%s\" => %s display)\n",
	vo_conf.screenwidth,vo_conf.screenheight,
	depth, vo->depthonscreen,
	dispName,priv->mLocalDisplay?"local":"remote");

    return MPXP_Ok;
}


#include "../osdep/keycodes.h"
#include "wskeys.h"

#ifdef XF86XK_AudioPause
static void __FASTCALL__ vo_x11_putkey_ext(int keysym){
 switch ( keysym )
  {
#ifdef XF86XK_Standby
   case XF86XK_Standby:       mplayer_put_key(KEY_XF86_STANDBY); break;
#endif
#ifdef XF86XK_PowerOff
   case XF86XK_PowerOff:
#endif
#ifdef XF86XK_PowerDown
   case XF86XK_PowerDown:     mplayer_put_key(KEY_XF86_POWER); break;
#endif
#ifdef XF86XK_AudioPausue
   case XF86XK_AudioPause:    mplayer_put_key(KEY_XF86_PAUSE); break;
#endif
#ifdef XF86XK_Stop
   case XF86XK_Stop:
#endif
#ifdef XF86XK_AudioStop
   case XF86XK_AudioStop:     mplayer_put_key(KEY_XF86_STOP); break;
#endif
#ifdef XF86XK_AudioPlay
   case XF86XK_AudioPlay:     mplayer_put_key(KEY_XF86_PLAY); break;
#endif
#ifdef XF86XK_AudioPrev
   case XF86XK_AudioPrev:     mplayer_put_key(KEY_XF86_PREV); break;
#endif
#ifdef XF86XK_AudioNext
   case XF86XK_AudioNext:     mplayer_put_key(KEY_XF86_NEXT); break;
#endif
#ifdef XF86XK_AudioRaiseVolume
   case XF86XK_AudioRaiseVolume: mplayer_put_key(KEY_XF86_VOLUME_UP); break;
#endif
#ifdef XF86XK_AudioLowerVolume
   case XF86XK_AudioLowerVolume: mplayer_put_key(KEY_XF86_VOLUME_DN); break;
#endif
#ifdef XF86XK_AudioMute
   case XF86XK_AudioMute:     mplayer_put_key(KEY_XF86_MUTE); break;
#endif
#ifdef XF86XK_AudioMedia
   case XF86XK_AudioMedia:    mplayer_put_key(KEY_XF86_MENU); break;
#endif
#ifdef XF86XK_Eject
   case XF86XK_Eject:     mplayer_put_key(KEY_XF86_EJECT); break;
#endif
#ifdef XF86XK_Back
   case XF86XK_Back:     mplayer_put_key(KEY_XF86_REWIND); break;
#endif
#ifdef XF86XK_Forward
   case XF86XK_Forward:     mplayer_put_key(KEY_XF86_FORWARD); break;
#endif
#ifdef XF86XK_BrightnessAdjust
   case XF86XK_BrightnessAdjust:     mplayer_put_key(KEY_XF86_BRIGHTNESS); break;
#endif
#ifdef XF86XK_ContrastAdjust
   case XF86XK_ContrastAdjust:     mplayer_put_key(KEY_XF86_CONTRAST); break;
#endif
#ifdef XF86XK_ScreenSaver
   case XF86XK_ScreenSaver:     mplayer_put_key(KEY_XF86_SCREENSAVE); break;
#endif
#ifdef XF86XK_Refresh
   case XF86XK_Refresh:     mplayer_put_key(KEY_XF86_REFRESH); break;
#endif
   default: break;
  }
}
#endif

static void __FASTCALL__ vo_x11_putkey(int key){
 switch ( key )
  {
   case wsLeft:      mplayer_put_key(KEY_LEFT); break;
   case wsRight:     mplayer_put_key(KEY_RIGHT); break;
   case wsUp:        mplayer_put_key(KEY_UP); break;
   case wsDown:      mplayer_put_key(KEY_DOWN); break;
   case wsSpace:     mplayer_put_key(' '); break;
   case wsEscape:    mplayer_put_key(KEY_ESC); break;
   case wsEnter:     mplayer_put_key(KEY_ENTER); break;
   case wsBackSpace: mplayer_put_key(KEY_BS); break;
   case wsDelete:    mplayer_put_key(KEY_DELETE); break;
   case wsInsert:    mplayer_put_key(KEY_INSERT); break;
   case wsHome:      mplayer_put_key(KEY_HOME); break;
   case wsEnd:       mplayer_put_key(KEY_END); break;
   case wsPageUp:    mplayer_put_key(KEY_PAGE_UP); break;
   case wsPageDown:  mplayer_put_key(KEY_PAGE_DOWN); break;
   case wsF1:        mplayer_put_key(KEY_F+1); break;
   case wsF2:        mplayer_put_key(KEY_F+2); break;
   case wsF3:        mplayer_put_key(KEY_F+3); break;
   case wsF4:        mplayer_put_key(KEY_F+4); break;
   case wsF5:        mplayer_put_key(KEY_F+5); break;
   case wsF6:        mplayer_put_key(KEY_F+6); break;
   case wsF7:        mplayer_put_key(KEY_F+7); break;
   case wsF8:        mplayer_put_key(KEY_F+8); break;
   case wsF9:        mplayer_put_key(KEY_F+9); break;
   case wsF10:       mplayer_put_key(KEY_F+10); break;
   case wsq:
   case wsQ:         mplayer_put_key('q'); break;
   case wsp:
   case wsP:         mplayer_put_key('p'); break;
   case wsMinus:
   case wsGrayMinus: mplayer_put_key('-'); break;
   case wsPlus:
   case wsGrayPlus:  mplayer_put_key('+'); break;
   case wsGrayMul:
   case wsMul:       mplayer_put_key('*'); break;
   case wsGrayDiv:
   case wsDiv:       mplayer_put_key('/'); break;
   case wsLess:      mplayer_put_key('<'); break;
   case wsMore:      mplayer_put_key('>'); break;
   case wsGray0:     mplayer_put_key(KEY_KP0); break;
   case wsGrayEnd:
   case wsGray1:     mplayer_put_key(KEY_KP1); break;
   case wsGrayDown:
   case wsGray2:     mplayer_put_key(KEY_KP2); break;
   case wsGrayPgDn:
   case wsGray3:     mplayer_put_key(KEY_KP3); break;
   case wsGrayLeft:
   case wsGray4:     mplayer_put_key(KEY_KP4); break;
   case wsGray5Dup:
   case wsGray5:     mplayer_put_key(KEY_KP5); break;
   case wsGrayRight:
   case wsGray6:     mplayer_put_key(KEY_KP6); break;
   case wsGrayHome:
   case wsGray7:     mplayer_put_key(KEY_KP7); break;
   case wsGrayUp:
   case wsGray8:     mplayer_put_key(KEY_KP8); break;
   case wsGrayPgUp:
   case wsGray9:     mplayer_put_key(KEY_KP9); break;
   case wsGrayDecimal: mplayer_put_key(KEY_KPDEC); break;
   case wsGrayInsert: mplayer_put_key(KEY_KPINS); break;
   case wsGrayDelete: mplayer_put_key(KEY_KPDEL); break;
   case wsGrayEnter: mplayer_put_key(KEY_KPENTER); break;
   case wsm:
   case wsM:	     mplayer_put_key('m'); break;
   case wso:
   case wsO:         mplayer_put_key('o'); break;
   default: if((key>='a' && key<='z')||(key>='A' && key<='Z')||
	       (key>='0' && key<='9')) mplayer_put_key(key);
  }

}


// ----- Motif header: -------

#define MWM_HINTS_FUNCTIONS     (1L << 0)
#define MWM_HINTS_DECORATIONS   (1L << 1)
#define MWM_HINTS_INPUT_MODE    (1L << 2)
#define MWM_HINTS_STATUS        (1L << 3)

#define MWM_FUNC_ALL            (1L << 0)
#define MWM_FUNC_RESIZE         (1L << 1)
#define MWM_FUNC_MOVE           (1L << 2)
#define MWM_FUNC_MINIMIZE       (1L << 3)
#define MWM_FUNC_MAXIMIZE       (1L << 4)
#define MWM_FUNC_CLOSE          (1L << 5)

#define MWM_DECOR_ALL           (1L << 0)
#define MWM_DECOR_BORDER        (1L << 1)
#define MWM_DECOR_RESIZEH       (1L << 2)
#define MWM_DECOR_TITLE         (1L << 3)
#define MWM_DECOR_MENU          (1L << 4)
#define MWM_DECOR_MINIMIZE      (1L << 5)
#define MWM_DECOR_MAXIMIZE      (1L << 6)

#define MWM_INPUT_MODELESS 0
#define MWM_INPUT_PRIMARY_APPLICATION_MODAL 1
#define MWM_INPUT_SYSTEM_MODAL 2
#define MWM_INPUT_FULL_APPLICATION_MODAL 3
#define MWM_INPUT_APPLICATION_MODAL MWM_INPUT_PRIMARY_APPLICATION_MODAL

#define MWM_TEAROFF_WINDOW      (1L<<0)

typedef struct
{
  long flags;
  long functions;
  long decorations;
  long input_mode;
  long state;
} MotifWmHints;

static MotifWmHints   vo_MotifWmHints;
static Atom           vo_MotifHints  = None;

// Note: always d==0 !
void __FASTCALL__ vo_x11_decoration(vo_data_t*vo,Display * vo_Display,Window w,int d )
{
    if(vo_conf.fsmode&1){
	XSetWindowAttributes attr;
	attr.override_redirect = True;
	XChangeWindowAttributes(vo_Display, w, CWOverrideRedirect, &attr);
//    XMapWindow(vo_Display, w);
    }

    if(vo_conf.fsmode&8){
	XSetTransientForHint (vo_Display, w, RootWindow(vo_Display,vo->mScreen));
    }

    vo_MotifHints=XInternAtom( vo_Display,"_MOTIF_WM_HINTS",0 );
    if ( vo_MotifHints != None ) {
	memset( &vo_MotifWmHints,0,sizeof( MotifWmHints ) );
	vo_MotifWmHints.flags=MWM_HINTS_FUNCTIONS | MWM_HINTS_DECORATIONS;
	if ( d ) {
	    vo_MotifWmHints.functions=MWM_FUNC_MOVE | MWM_FUNC_CLOSE | MWM_FUNC_MINIMIZE | MWM_FUNC_MAXIMIZE;
	    d=MWM_DECOR_ALL;
	}
#if 0
	vo_MotifWmHints.decorations=d|((vo_conf.fsmode&2)?0:MWM_DECOR_MENU);
#else
	vo_MotifWmHints.decorations=d|((vo_conf.fsmode&2)?MWM_DECOR_MENU:0);
#endif
	XChangeProperty( vo_Display,w,vo_MotifHints,vo_MotifHints,32,
			PropModeReplace,(unsigned char *)&vo_MotifWmHints,(vo_conf.fsmode&4)?4:5 );
    }
}

void __FASTCALL__ vo_x11_classhint( Display * display,Window window,const char *name ){
	    XClassHint wmClass;
	    wmClass.res_name = const_cast<char*>(name);
	    wmClass.res_class = const_cast<char*>("MPlayerXP");
	    XSetClassHint(display,window,&wmClass);
}

MPXP_Rc __FASTCALL__ vo_x11_uninit(vo_data_t*vo,Display *display, Window window)
{
    XSetErrorHandler(NULL);
    /* and -wid is set */
    if (!(vo_conf.WinID > 0))
	XDestroyWindow(display, window);
    XCloseDisplay(display);
    vo->depthonscreen = 0;
    delete vo->priv2;
    return MPXP_Ok;
}


const char * evt_names[] = {
"KeyPress",
"KeyRelease",
"ButtonPress",
"ButtonRelease",
"MotionNotify",
"EnterNotify",
"LeaveNotify",
"FocusIn",
"FocusOut",
"KeymapNotify",
"Expose",
"GraphicsExpose",
"NoExpose",
"VisibilityNotify",
"CreateNotify",
"DestroyNotify",
"UnmapNotify",
"MapNotify",
"MapRequest",
"ReparentNotify",
"ConfigureNotify",
"ConfigureRequest",
"GravityNotify",
"ResizeRequest",
"CirculateNotify",
"CirculateRequest",
"PropertyNotify",
"SelectionClear",
"SelectionRequest",
"SelectionNotify",
"ColormapNotify",
"ClientMessage",
"MappingNotify",
"GenericEvent",
"LASTEvent"
};

static const char * __FASTCALL__ evt_name(unsigned num)
{
    if(num >=2 && num <= 36)	return evt_names[num-2];
    else			return "Unknown";
}

uint32_t __FASTCALL__ vo_x11_check_events(vo_data_t*vo,Display *mydisplay,vo_adjust_size_t adjust_size)
{
    uint32_t ret=0;
    XEvent         Event;
    char           buf[100];
    KeySym         keySym;
    static XComposeStatus stat;
    int adj_ret=0;
    unsigned ow,oh,nw,nh;
    while ( XPending( mydisplay ) ) {
	XNextEvent( mydisplay,&Event );
	MSG_V("X11_common: event_type = %lX (%s)\n",Event.type,evt_name(Event.type));
	switch( Event.type ) {
	    case Expose:
		ret|=VO_EVENT_EXPOSE;
		break;
	    case ConfigureNotify:
		nw = Event.xconfigure.width;
		nh = Event.xconfigure.height;
		if(adjust_size) adj_ret = (*adjust_size)(vo,vo->dest.w,vo->dest.h,&nw,&nh);
		ow = vo->dest.w;
		oh = vo->dest.h;
		vo->dest.w=nw;
		vo->dest.h=nh;
		Window root;
		int ifoo;
		unsigned foo;
		Window win;
		XGetGeometry(mydisplay, vo->window, &root, &ifoo, &ifoo,
			&foo/*width*/, &foo/*height*/, &foo, &foo);
		XTranslateCoordinates(mydisplay, vo->window, root, 0, 0,
			reinterpret_cast<int*>(&vo->dest.x),
			reinterpret_cast<int*>(&vo->dest.y), &win);
		if(adjust_size && ow != vo->dest.w && oh != vo->dest.h && adj_ret) {
		    XResizeWindow( vo->mDisplay,vo->window,vo->dest.w,vo->dest.h );
		    XSync( vo->mDisplay,True);
		}
		MSG_V("X11 Window %dx%d-%dx%d\n", vo->dest.x, vo->dest.y, vo->dest.w, vo->dest.h);
		ret|=VO_EVENT_RESIZE;
		break;
	    case KeyPress: {
		int key;
		XLookupString( &Event.xkey,buf,sizeof(buf),&keySym,&stat );
#ifdef XF86XK_AudioPause
		vo_x11_putkey_ext( keySym );
#endif
		key=( (keySym&0xff00) != 0?( (keySym&0x00ff) + 256 ):( keySym ) );
		vo_x11_putkey( key );
		ret|=VO_EVENT_KEYPRESS;
		break;
		}
	    case ButtonPress:
		// Ignore mouse whell press event
		if(Event.xbutton.button > 3) {
		    mplayer_put_key(MOUSE_BTN0+Event.xbutton.button-1);
		    break;
		}
	    case ButtonRelease:
		mplayer_put_key(MOUSE_BTN0+Event.xbutton.button-1);
	    default:
		break;
	}
    }
    return ret;
}

void __FASTCALL__ vo_x11_sizehint(vo_data_t*vo, int x, int y, int width, int height )
{
    XSizeHints hint;
    hint.flags=PPosition | PSize | PWinGravity;
    hint.x=x; hint.y=y; hint.width=width; hint.height=height;
    hint.win_gravity=StaticGravity;
    XSetWMNormalHints( vo->mDisplay,vo->window,&hint );
}

void __FASTCALL__ vo_x11_calcpos(vo_data_t*vo,XSizeHints* hint, unsigned d_width, unsigned d_height, unsigned flags )
{
    UNUSED(flags);
#ifdef HAVE_XF86VM
    int modeline_width, modeline_height;
    static uint32_t vm_width;
    static uint32_t vm_height;
#endif
    hint->x=(vo_conf.screenwidth-d_width)/2;
    hint->y=(vo_conf.screenheight-d_height)/2;
    hint->width=d_width;
    hint->height=d_height;
#ifdef HAVE_XF86VM
    if ( vo_VM(vo) ) {
	vm_width=d_width; vm_height=d_height;
	vo_vm_switch(vo,vm_width, vm_height,&modeline_width, &modeline_height);
	hint->x=(vo_conf.screenwidth-modeline_width)/2;
	hint->y=(vo_conf.screenheight-modeline_height)/2;
	hint->width=modeline_width;
	hint->height=modeline_height;
    }
    else
#endif
    if ( vo_FS(vo) ) {
      hint->width=vo_conf.screenwidth;
      hint->height=vo_conf.screenheight;
      hint->x=0;
      hint->y=0;
    }
}

void vo_x11_fullscreen(vo_data_t*vo )
{
    priv_t*priv=(priv_t*)vo->priv2;
    XUnmapWindow( vo->mDisplay,vo->window );
    if ( !vo_FS(vo) ) {
	vo_FS_SET(vo);
	priv->prev=vo->dest;
	vo->dest.x=0;  vo->dest.y=0; vo->dest.w=vo_conf.screenwidth; vo->dest.h=vo_conf.screenheight;
	vo_x11_decoration(vo,vo->mDisplay,vo->window,0 );
    } else {
	vo_FS_UNSET(vo);
	vo->dest=priv->prev;
	vo_x11_decoration(vo,vo->mDisplay,vo->window,1 );
    }
    vo_x11_sizehint(vo,vo->dest.x,vo->dest.y,vo->dest.w,vo->dest.h );
    XMoveResizeWindow( vo->mDisplay,vo->window,vo->dest.x,vo->dest.y,vo->dest.w,vo->dest.h );
    XMapWindow( vo->mDisplay,vo->window );
    XSync( vo->mDisplay,False );
}

void __FASTCALL__ saver_on(vo_data_t*vo,Display *mDisplay) {

    priv_t*priv=(priv_t*)vo->priv2;
#ifdef HAVE_XDPMS
    int nothing;
    if (priv->dpms_disabled) {
	if (DPMSQueryExtension(mDisplay, &nothing, &nothing)) {
	    if (!DPMSEnable(mDisplay)) {  // restoring power saving settings
		MSG_WARN("DPMS not available?\n");
	    } else {
		// DPMS does not seem to be enabled unless we call DPMSInfo
		BOOL onoff;
		CARD16 state;
		DPMSInfo(mDisplay, &state, &onoff);
		if (onoff) {
		    MSG_V ("Successfully enabled DPMS\n");
		} else {
		    MSG_ERR ("Could not enable DPMS\n");
		}
	    }
	}
    }
#endif

    if (priv->timeout_save) {
	int dummy, interval, prefer_blank, allow_exp;
	XGetScreenSaver(mDisplay, &dummy, &interval, &prefer_blank, &allow_exp);
	XSetScreenSaver(mDisplay, priv->timeout_save, interval, prefer_blank, allow_exp);
	XGetScreenSaver(mDisplay, &priv->timeout_save, &interval, &prefer_blank, &allow_exp);
    }
}

void __FASTCALL__ saver_off(vo_data_t*vo,Display *mDisplay) {

    priv_t*priv=(priv_t*)vo->priv2;
    int interval, prefer_blank, allow_exp;
#ifdef HAVE_XDPMS
    int nothing;

    if (DPMSQueryExtension(mDisplay, &nothing, &nothing)) {
	BOOL onoff;
	CARD16 state;
	DPMSInfo(mDisplay, &state, &onoff);
	if (onoff) {
	    Status stat;
	    MSG_V ("Disabling DPMS\n");
	    priv->dpms_disabled=1;
	    stat = DPMSDisable(mDisplay);  // monitor powersave off
	    MSG_V ("stat: %d\n", stat);
	}
    }
#endif
    XGetScreenSaver(mDisplay, &priv->timeout_save, &interval, &prefer_blank, &allow_exp);
    if (priv->timeout_save) // turning off screensaver
	XSetScreenSaver(mDisplay, 0, interval, prefer_blank, allow_exp);
}



#ifdef HAVE_XINERAMA
void __FASTCALL__ vo_x11_xinerama_move(vo_data_t*vo,Display *dsp, Window w,const XSizeHints*hint)
{
    priv_t*priv=(priv_t*)vo->priv2;
    if(XineramaIsActive(dsp))
	XMoveWindow(dsp,w,priv->xinerama_x+hint->x,priv->xinerama_y+hint->y);
}
#endif

#ifdef HAVE_XF86VM
void __FASTCALL__ vo_vm_switch(vo_data_t*vo,uint32_t X, uint32_t Y, int* modeline_width, int* modeline_height)
{
    priv_t*priv=(priv_t*)vo->priv2;
    int vm_event, vm_error;
    int vm_ver, vm_rev;
    int i,j,have_vm=0;

    int modecount;

    if (XF86VidModeQueryExtension(vo->mDisplay, &vm_event, &vm_error)) {
      XF86VidModeQueryVersion(vo->mDisplay, &vm_ver, &vm_rev);
      MSG_V("XF86VidMode Extension v%i.%i\n", vm_ver, vm_rev);
      have_vm=1;
    } else
      MSG_WARN("XF86VidMode Extenstion not available.\n");

    if (have_vm) {
      if (priv->vidmodes==NULL)
	XF86VidModeGetAllModeLines(vo->mDisplay,vo->mScreen,&modecount,&priv->vidmodes);
      j=0;
      *modeline_width=priv->vidmodes[0]->hdisplay;
      *modeline_height=priv->vidmodes[0]->vdisplay;

      for (i=1; i<modecount; i++)
	if ((priv->vidmodes[i]->hdisplay >= X) && (priv->vidmodes[i]->vdisplay >= Y))
	  if ( (priv->vidmodes[i]->hdisplay <= *modeline_width ) && (priv->vidmodes[i]->vdisplay <= *modeline_height) )
	    {
	      *modeline_width=priv->vidmodes[i]->hdisplay;
	      *modeline_height=priv->vidmodes[i]->vdisplay;
	      j=i;
	    }

      MSG_V("XF86VM: Selected video mode %dx%d for image size %dx%d.\n",*modeline_width, *modeline_height, X, Y);
      XF86VidModeLockModeSwitch(vo->mDisplay,vo->mScreen,0);
      XF86VidModeSwitchToMode(vo->mDisplay,vo->mScreen,priv->vidmodes[j]);
      XF86VidModeSwitchToMode(vo->mDisplay,vo->mScreen,priv->vidmodes[j]);
      X=(vo_conf.screenwidth-*modeline_width)/2;
      Y=(vo_conf.screenheight-*modeline_height)/2;
      XF86VidModeSetViewPort(vo->mDisplay,vo->mScreen,X,Y);
    }
}

void __FASTCALL__ vo_vm_close(vo_data_t*vo,Display *dpy)
{
    priv_t*priv=(priv_t*)vo->priv2;
    if (priv->vidmodes!=NULL) {
	int i, modecount;
	int screen; screen=DefaultScreen( dpy );

	delete priv->vidmodes; priv->vidmodes=NULL;
	XF86VidModeGetAllModeLines(vo->mDisplay,vo->mScreen,&modecount,&priv->vidmodes);
	for (i=0; i<modecount; i++)
	    if ((priv->vidmodes[i]->hdisplay == vo_conf.screenwidth) && (priv->vidmodes[i]->vdisplay == vo_conf.screenheight)) {
		MSG_V("\nReturning to original mode %dx%d\n", vo_conf.screenwidth, vo_conf.screenheight);
		break;
	    }
	XF86VidModeSwitchToMode(dpy,screen,priv->vidmodes[i]);
	XF86VidModeSwitchToMode(dpy,screen,priv->vidmodes[i]);
	delete priv->vidmodes;
    }
}
#endif

#endif
