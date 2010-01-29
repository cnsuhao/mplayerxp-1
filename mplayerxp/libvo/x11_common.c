#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "../mp_config.h"
#include "../mplayer.h"
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

#include "../input/input.h"
#include "../input/mouse.h"
#include "vo_msg.h"

/*
 * If SCAN_VISUALS is defined, vo_x11_init() scans all available TrueColor
 * visuals for the 'best' visual for MPlayer video display.  Note that
 * the 'best' visual might be different from the default visual that
 * is in use on the root window of the display/screen.
 */
#define	SCAN_VISUALS

static int dpms_disabled=0;
static int timeout_save=0;

char* mDisplayName=NULL;
Display* mDisplay;
Window   mRootWin;
int mScreen;
int mLocalDisplay;

/* output window id */
int WinID=-1;

#ifdef HAVE_XINERAMA
int xinerama_screen = 0;
int xinerama_x = 0;
int xinerama_y = 0;
#endif
#ifdef HAVE_XF86VM
XF86VidModeModeInfo **vidmodes=NULL;
XF86VidModeModeLine modeline;
#endif

#ifdef HAVE_SHM
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>

int vo_x11_Shmem_Flag=0;
static XShmSegmentInfo Shminfo[MAX_DRI_BUFFERS];
static int gXErrorFlag;
static int CompletionType=-1;

/* since it doesn't seem to be defined on some platforms */
extern int XShmGetEventBase( Display* );
#endif

XImage *vo_x11_myximage[MAX_DRI_BUFFERS];

void __FASTCALL__ vo_x11_getMyXImage(unsigned idx,Visual *visual,unsigned depth,unsigned w,unsigned h)
{
#ifdef HAVE_SHM
 if ( mLocalDisplay && XShmQueryExtension( mDisplay )) vo_x11_Shmem_Flag=1;
 else {
    vo_x11_Shmem_Flag=0;
    MSG_V( "Shared memory not supported\nReverting to normal Xlib\n" );
 }
 if ( vo_x11_Shmem_Flag ) CompletionType=XShmGetEventBase( mDisplay ) + ShmCompletion;
 if ( vo_x11_Shmem_Flag ) {
   vo_x11_myximage[idx]=XShmCreateImage( mDisplay,visual,depth,ZPixmap,NULL,&Shminfo[idx],w,h);
   if ( vo_x11_myximage[idx] == NULL ) {
     if ( vo_x11_myximage[idx] != NULL ) XDestroyImage( vo_x11_myximage[idx] );
     MSG_V( "Shared memory error,disabling ( Ximage error )\n" );
     goto shmemerror;
   }
   Shminfo[idx].shmid=shmget( IPC_PRIVATE,
   vo_x11_myximage[idx]->bytes_per_line * vo_x11_myximage[idx]->height ,
   IPC_CREAT | 0777 );
   if ( Shminfo[idx].shmid < 0 ) {
    XDestroyImage( vo_x11_myximage[idx] );
    MSG_V( "%s\n",strerror( errno ) );
    MSG_V( "Shared memory error,disabling ( seg id error )\n" );
    goto shmemerror;
   }
   Shminfo[idx].shmaddr=( char * ) shmat( Shminfo[idx].shmid,0,0 );

   if ( Shminfo[idx].shmaddr == ( ( char * ) -1 ) )
   {
    XDestroyImage( vo_x11_myximage[idx] );
    if ( Shminfo[idx].shmaddr != ( ( char * ) -1 ) ) shmdt( Shminfo[idx].shmaddr );
    MSG_V( "Shared memory error,disabling ( address error )\n" );
    goto shmemerror;
   }
   vo_x11_myximage[idx]->data=Shminfo[idx].shmaddr;
   Shminfo[idx].readOnly=False;
   XShmAttach( mDisplay,&Shminfo[idx] );

   XSync( mDisplay,False );

   if ( gXErrorFlag )
   {
    XDestroyImage( vo_x11_myximage[idx] );
    shmdt( Shminfo[idx].shmaddr );
    MSG_V( "Shared memory error,disabling.\n" );
    gXErrorFlag=0;
    goto shmemerror;
   }
   else
    shmctl( Shminfo[idx].shmid,IPC_RMID,0 );

   {
     static int firstTime=1;
     if (firstTime){
       MSG_V( "Sharing memory.\n" );
       firstTime=0;
     }
   }
 }
 else
  {
   shmemerror:
   vo_x11_Shmem_Flag=0;
#endif
   vo_x11_myximage[idx]=XGetImage( mDisplay,vo_window,0,0,
				w,h,AllPlanes,ZPixmap );
#ifdef HAVE_SHM
  }
#endif
}

void __FASTCALL__ vo_x11_freeMyXImage(unsigned idx)
{
#ifdef HAVE_SHM
 if ( vo_x11_Shmem_Flag )
  {
   XShmDetach( mDisplay,&Shminfo[idx] );
   XDestroyImage( vo_x11_myximage[idx] );
   shmdt( Shminfo[idx].shmaddr );
  }
  else
#endif
  {
   XDestroyImage( vo_x11_myximage[idx] );
  }
  vo_x11_myximage[idx]=NULL;
}


void __FASTCALL__ vo_x11_hidecursor ( Display *disp , Window win )
{
	Cursor no_ptr;
	Pixmap bm_no;
	XColor black,dummy;
	Colormap colormap;
	static char bm_no_data[] = { 0,0,0,0, 0,0,0,0  };

	if(WinID==0) return;	// do not hide, if we're playing at rootwin
	
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
  visual_tmpl.class = TrueColor;
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

int vo_x11_init( void )
{
// int       mScreen;
 unsigned depth, bpp;
 unsigned int mask;
// char    * DisplayName = ":0.0";
// Display * mDisplay;
 XImage  * mXImage = NULL;
// Window    mRootWin;
 XWindowAttributes attribs;
 char* dispName;

 if(vo_depthonscreen) return 1; // already called

 XSetErrorHandler(x11_errorhandler);

#if 0
 if (!mDisplayName)
   if (!(mDisplayName=getenv("DISPLAY")))
     mDisplayName=strdup(":0.0");
#else
  dispName = XDisplayName(mDisplayName);
#endif

 MSG_V("X11 opening display: %s\n", dispName);

 mDisplay=XOpenDisplay(dispName);
 if ( !mDisplay )
  {
   MSG_ERR( "vo: couldn't open the X11 display (%s)!\n",dispName );
   return 0;
  }
 mScreen=DefaultScreen( mDisplay );     // Screen ID.
 mRootWin=RootWindow( mDisplay,mScreen );// Root window ID.

#ifdef HAVE_XINERAMA
 if(XineramaIsActive(mDisplay))
  {
  XineramaScreenInfo *screens;
  int num_screens;

  screens = XineramaQueryScreens(mDisplay, &num_screens);
  if(xinerama_screen >= num_screens) xinerama_screen = 0;
  if (! vo_screenwidth)
    vo_screenwidth=screens[xinerama_screen].width;
  if (! vo_screenheight)
    vo_screenheight=screens[xinerama_screen].height;
  xinerama_x = screens[xinerama_screen].x_org;
  xinerama_y = screens[xinerama_screen].y_org;

  XFree(screens);
  }
 else
#endif
#ifdef HAVE_XF86VM
 {
  int clock;
  XF86VidModeGetModeLine( mDisplay,mScreen,&clock ,&modeline );
  if ( !vo_screenwidth )  vo_screenwidth=modeline.hdisplay;
  if ( !vo_screenheight ) vo_screenheight=modeline.vdisplay;
 }
#endif
 {
 if (! vo_screenwidth)
   vo_screenwidth=DisplayWidth( mDisplay,mScreen );
 if (! vo_screenheight)
   vo_screenheight=DisplayHeight( mDisplay,mScreen );
 }
 // get color depth (from root window, or the best visual):
 XGetWindowAttributes(mDisplay, mRootWin, &attribs);
 depth=attribs.depth;

#ifdef	SCAN_VISUALS
 if (depth != 15 && depth != 16 && depth != 24 && depth != 32) {
   Visual *visual;

   depth = vo_find_depth_from_visuals(mDisplay, mScreen, &visual);
   if ((int)depth != -1)
     mXImage=XCreateImage(mDisplay, visual, depth, ZPixmap,
			  0, NULL, 1, 1, 8, 1);
 } else
#endif
 mXImage=XGetImage( mDisplay,mRootWin,0,0,1,1,AllPlanes,ZPixmap );

 vo_depthonscreen = depth;	// display depth on screen

 // get bits/pixel from XImage structure:
 if (mXImage == NULL) {
   mask = 0;
 } else {
   /*
    * for the depth==24 case, the XImage structures might use
    * 24 or 32 bits of data per pixel.  The global variable
    * vo_depthonscreen stores the amount of data per pixel in the
    * XImage structure!
    *
    * Maybe we should rename vo_depthonscreen to (or add) vo_bpp?
    */
   bpp=mXImage->bits_per_pixel;
   if((vo_depthonscreen+7)/8 != (bpp+7)/8) vo_depthonscreen=bpp; // by A'rpi
   mask=mXImage->red_mask|mXImage->green_mask|mXImage->blue_mask;
   MSG_V("vo: X11 color mask:  %X  (R:%lX G:%lX B:%lX)\n",
	    mask,mXImage->red_mask,mXImage->green_mask,mXImage->blue_mask);
   XDestroyImage( mXImage );
 }
 if(((vo_depthonscreen+7)/8)==2){
   if(mask==0x7FFF) vo_depthonscreen=15; else
   if(mask==0xFFFF) vo_depthonscreen=16;
 }
// XCloseDisplay( mDisplay );
/* slightly improved local display detection AST */
 if ( strncmp(dispName, "unix:", 5) == 0)
		dispName += 4;
 else if ( strncmp(dispName, "localhost:", 10) == 0)
		dispName += 9;
 if (*dispName==':') mLocalDisplay=1; else mLocalDisplay=0;
 MSG_V("vo: X11 running at %dx%d with depth %d and %d bits/pixel (\"%s\" => %s display)\n",
	vo_screenwidth,vo_screenheight,
	depth, vo_depthonscreen,
	dispName,mLocalDisplay?"local":"remote");

 return 1;
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
void __FASTCALL__ vo_x11_decoration( Display * vo_Display,Window w,int d )
{

  if(vo_fsmode&1){
    XSetWindowAttributes attr;
    attr.override_redirect = True;
    XChangeWindowAttributes(vo_Display, w, CWOverrideRedirect, &attr);
//    XMapWindow(vo_Display, w);
  }

  if(vo_fsmode&8){
    XSetTransientForHint (vo_Display, w, RootWindow(vo_Display,mScreen));
  }

 vo_MotifHints=XInternAtom( vo_Display,"_MOTIF_WM_HINTS",0 );
 if ( vo_MotifHints != None )
  {
   memset( &vo_MotifWmHints,0,sizeof( MotifWmHints ) );
   vo_MotifWmHints.flags=MWM_HINTS_FUNCTIONS | MWM_HINTS_DECORATIONS;
   if ( d )
    {
     vo_MotifWmHints.functions=MWM_FUNC_MOVE | MWM_FUNC_CLOSE | MWM_FUNC_MINIMIZE | MWM_FUNC_MAXIMIZE;
     d=MWM_DECOR_ALL;
    }
#if 0
   vo_MotifWmHints.decorations=d|((vo_fsmode&2)?0:MWM_DECOR_MENU);
#else
   vo_MotifWmHints.decorations=d|((vo_fsmode&2)?MWM_DECOR_MENU:0);
#endif
   XChangeProperty( vo_Display,w,vo_MotifHints,vo_MotifHints,32,
                    PropModeReplace,(unsigned char *)&vo_MotifWmHints,(vo_fsmode&4)?4:5 );
  }
}

void __FASTCALL__ vo_x11_classhint( Display * display,Window window,char *name ){
	    XClassHint wmClass;
	    wmClass.res_name = name;
	    wmClass.res_class = "MPlayerXP";
	    XSetClassHint(display,window,&wmClass);
}

Window     vo_window = None;
GC         vo_gc;
XSizeHints vo_hint;

int __FASTCALL__ vo_x11_uninit(Display *display, Window window)
{
    XSetErrorHandler(NULL);

    {
	/* and -wid is set */
	if (!(WinID > 0))
	    XDestroyWindow(display, window);
	XCloseDisplay(display);
	vo_depthonscreen = 0;
    }
    return(1);
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

uint32_t __FASTCALL__ vo_x11_check_events(Display *mydisplay,int (* __FASTCALL__ adjust_size)(unsigned cw,unsigned ch,unsigned *nw,unsigned *nh))
{
 uint32_t ret=0;
 XEvent         Event;
 char           buf[100];
 KeySym         keySym;
 static XComposeStatus stat;
 int adj_ret=0;
 unsigned ow,oh,nw,nh;
 while ( XPending( mydisplay ) )
  {
   XNextEvent( mydisplay,&Event );
    MSG_V("X11_common: event_type = %lX (%s)\n",Event.type,evt_name(Event.type));
    switch( Event.type )
     {
      case Expose:
           ret|=VO_EVENT_EXPOSE;
           break;
      case ConfigureNotify:
	   nw = Event.xconfigure.width;
	   nh = Event.xconfigure.height;
	   if(adjust_size) adj_ret = (*adjust_size)(vo_dwidth,vo_dheight,&nw,&nh);
	   ow = vo_dwidth;
	   oh = vo_dheight;
           vo_dwidth=nw;
           vo_dheight=nh;
	   {
	    Window root;
	    unsigned foo;
	    Window win;
	    XGetGeometry(mydisplay, vo_window, &root, &foo, &foo,
		&foo/*width*/, &foo/*height*/, &foo, &foo);
	    XTranslateCoordinates(mydisplay, vo_window, root, 0, 0,
		&vo_dx, &vo_dy, &win);
	    }
	   if(adjust_size && ow != vo_dwidth && oh != vo_dheight && adj_ret)
	   {
		XResizeWindow( mDisplay,vo_window,vo_dwidth,vo_dheight );
		XSync( mDisplay,True);
	   }
	   MSG_V("X11 Window %dx%d-%dx%d\n", vo_dx, vo_dy, vo_dwidth, vo_dheight);
           ret|=VO_EVENT_RESIZE;
           break;
      case KeyPress:
           {
	    int key;
            XLookupString( &Event.xkey,buf,sizeof(buf),&keySym,&stat );
#ifdef XF86XK_AudioPause
             vo_x11_putkey_ext( keySym );
#endif
	    key=( (keySym&0xff00) != 0?( (keySym&0x00ff) + 256 ):( keySym ) );
            vo_x11_putkey( key );
            ret|=VO_EVENT_KEYPRESS;
	   }
           break;
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

void __FASTCALL__ vo_x11_sizehint( int x, int y, int width, int height )
{
 vo_hint.flags=PPosition | PSize | PWinGravity;
 vo_hint.x=x; vo_hint.y=y; vo_hint.width=width; vo_hint.height=height;
 vo_hint.win_gravity=StaticGravity;
 XSetWMNormalHints( mDisplay,vo_window,&vo_hint );
}

void __FASTCALL__ vo_x11_calcpos( XSizeHints* hint, unsigned d_width, unsigned d_height, unsigned flags )
{
 int fullscreen=0;
#ifdef HAVE_XF86VM
 unsigned int modeline_width, modeline_height;
 static uint32_t vm_width;
 static uint32_t vm_height;
#endif
 int vm=0;
    if( flags&0x03 ) fullscreen = 1;
    if( flags&0x02 ) vm = 1;
    hint->x=(vo_screenwidth-d_width)/2;
    hint->y=(vo_screenheight-d_height)/2;
    hint->width=d_width;
    hint->height=d_height;
#ifdef HAVE_XF86VM
    if ( vm )
    {
	vm_width=d_width; vm_height=d_height;
	vo_vm_switch(vm_width, vm_height,&modeline_width, &modeline_height);
	hint->x=(vo_screenwidth-modeline_width)/2;
	hint->y=(vo_screenheight-modeline_height)/2;
	hint->width=modeline_width;
	hint->height=modeline_height;
    }
    else
#endif
    if ( fullscreen )
    {
      hint->width=vo_screenwidth;
      hint->height=vo_screenheight;
      hint->x=0;
      hint->y=0;
    }
}

void vo_x11_fullscreen( void )
{
 XUnmapWindow( mDisplay,vo_window );
 if ( !vo_fs )
  {
   vo_fs=VO_TRUE;
   vo_old_x=vo_dx; vo_old_y=vo_dy; vo_old_width=vo_dwidth;   vo_old_height=vo_dheight;
   vo_dx=0;        vo_dy=0;        vo_dwidth=vo_screenwidth; vo_dheight=vo_screenheight;
   vo_x11_decoration( mDisplay,vo_window,0 );
  }
  else
   {
    vo_fs=VO_FALSE;
    vo_dx=vo_old_x; vo_dy=vo_old_y; vo_dwidth=vo_old_width; vo_dheight=vo_old_height;
    vo_x11_decoration( mDisplay,vo_window,1 );
   }
 vo_x11_sizehint( vo_dx,vo_dy,vo_dwidth,vo_dheight );
 XMoveResizeWindow( mDisplay,vo_window,vo_dx,vo_dy,vo_dwidth,vo_dheight );
 XMapWindow( mDisplay,vo_window );
 XSync( mDisplay,False );
}

void __FASTCALL__ saver_on(Display *mDisplay) {

#ifdef HAVE_XDPMS
    int nothing;
    if (dpms_disabled)
    {
	if (DPMSQueryExtension(mDisplay, &nothing, &nothing))
	{
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

    if (timeout_save)
    {
	int dummy, interval, prefer_blank, allow_exp;
	XGetScreenSaver(mDisplay, &dummy, &interval, &prefer_blank, &allow_exp);
	XSetScreenSaver(mDisplay, timeout_save, interval, prefer_blank, allow_exp);
	XGetScreenSaver(mDisplay, &timeout_save, &interval, &prefer_blank, &allow_exp);
    }

}

void __FASTCALL__ saver_off(Display *mDisplay) {

    int interval, prefer_blank, allow_exp;
#ifdef HAVE_XDPMS
    int nothing;

    if (DPMSQueryExtension(mDisplay, &nothing, &nothing))
    {
	BOOL onoff;
	CARD16 state;
	DPMSInfo(mDisplay, &state, &onoff);
	if (onoff)
	{
           Status stat;
	    MSG_V ("Disabling DPMS\n");
	    dpms_disabled=1;
	    stat = DPMSDisable(mDisplay);  // monitor powersave off
            MSG_V ("stat: %d\n", stat);
	}
    }
#endif
    XGetScreenSaver(mDisplay, &timeout_save, &interval, &prefer_blank, &allow_exp);
    if (timeout_save)
	XSetScreenSaver(mDisplay, 0, interval, prefer_blank, allow_exp);
		    // turning off screensaver
}



#ifdef HAVE_XINERAMA
void __FASTCALL__ vo_x11_xinerama_move(Display *dsp, Window w,const XSizeHints*hint)
{
	if(XineramaIsActive(dsp))
		XMoveWindow(dsp,w,xinerama_x+hint->x,xinerama_y+hint->y);
}
#endif

#ifdef HAVE_XF86VM
void __FASTCALL__ vo_vm_switch(uint32_t X, uint32_t Y, int* modeline_width, int* modeline_height)
{
    int vm_event, vm_error;
    int vm_ver, vm_rev;
    int i,j,have_vm=0;

    int modecount;

    if (XF86VidModeQueryExtension(mDisplay, &vm_event, &vm_error)) {
      XF86VidModeQueryVersion(mDisplay, &vm_ver, &vm_rev);
      MSG_V("XF86VidMode Extension v%i.%i\n", vm_ver, vm_rev);
      have_vm=1;
    } else
      MSG_WARN("XF86VidMode Extenstion not available.\n");

    if (have_vm) {
      if (vidmodes==NULL)
        XF86VidModeGetAllModeLines(mDisplay,mScreen,&modecount,&vidmodes);
      j=0;
      *modeline_width=vidmodes[0]->hdisplay;
      *modeline_height=vidmodes[0]->vdisplay;

      for (i=1; i<modecount; i++)
        if ((vidmodes[i]->hdisplay >= X) && (vidmodes[i]->vdisplay >= Y))
          if ( (vidmodes[i]->hdisplay <= *modeline_width ) && (vidmodes[i]->vdisplay <= *modeline_height) )
	    {
	      *modeline_width=vidmodes[i]->hdisplay;
	      *modeline_height=vidmodes[i]->vdisplay;
	      j=i;
	    }

      MSG_V("XF86VM: Selected video mode %dx%d for image size %dx%d.\n",*modeline_width, *modeline_height, X, Y);
      XF86VidModeLockModeSwitch(mDisplay,mScreen,0);
      XF86VidModeSwitchToMode(mDisplay,mScreen,vidmodes[j]);
      XF86VidModeSwitchToMode(mDisplay,mScreen,vidmodes[j]);
      X=(vo_screenwidth-*modeline_width)/2;
      Y=(vo_screenheight-*modeline_height)/2;
      XF86VidModeSetViewPort(mDisplay,mScreen,X,Y);
    }
}

void __FASTCALL__ vo_vm_close(Display *dpy)
{
        if (vidmodes!=NULL)
         {
           int i, modecount;
           int screen; screen=DefaultScreen( dpy );

           free(vidmodes); vidmodes=NULL;
           XF86VidModeGetAllModeLines(mDisplay,mScreen,&modecount,&vidmodes);
           for (i=0; i<modecount; i++)
             if ((vidmodes[i]->hdisplay == vo_screenwidth) && (vidmodes[i]->vdisplay == vo_screenheight)) 
               { 
                 MSG_V("\nReturning to original mode %dx%d\n", vo_screenwidth, vo_screenheight);
                 break;
               }

           XF86VidModeSwitchToMode(dpy,screen,vidmodes[i]);
           XF86VidModeSwitchToMode(dpy,screen,vidmodes[i]);
           free(vidmodes);
         }
}
#endif

#endif
