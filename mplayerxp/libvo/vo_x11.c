#define DISP

/*
 * video_out_x11.c,X11 interface
 *
 *
 * Copyright ( C ) 1996,MPEG Software Simulation Group. All Rights Reserved.
 *
 * Hacked into mpeg2dec by
 *
 * Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *
 * 15 & 16 bpp support added by Franck Sicard <Franck.Sicard@solsoft.fr>
 * use swScaler instead of lots of tricky converters by Michael Niedermayer <michaelni@gmx.at>
 * runtime fullscreen switching by alex
 *
 */

#include <pthread.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#ifdef HAVE_XF86VM
#include <X11/extensions/xf86vmode.h>
#endif
#include <errno.h>

#include "mp_config.h"
#include "../mplayer.h"
#include "../dec_ahead.h"
#include "aspect.h"
#include "video_out.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>


#include "x11_common.h"

#include "fastmemcpy.h"
#include "sub.h"


/* special case for vo_x11 driver. We need mutexes */
//#include "../dec_ahead.h"
#define MSG_D(args...)
#define LOCK_VDECODING() { MSG_D(DA_PREFIX"LOCK_VDECODING\n"); pthread_mutex_lock(&vdecoding_mutex); }

#include "../postproc/swscale.h" /* for MODE_RGB(BGR) definitions */
#include "video_out_internal.h"
#include "dri_vo.h"
#include "../mp_image.h"
LIBVO_EXTERN( x11 )

static vo_info_t vo_info =
{
        "X11 ( XImage/Shm )",
        "x11",
        "Aaron Holtzman <aholtzma@ess.engr.uvic.ca>",
        ""
};

/* private prototypes */
static void __FASTCALL__ Display_Image ( XImage * myximage );

/* local data */
#define ImageData(idx) ( uint8_t * ) myximage[idx]->data

/*** X11 related variables ***/
/* xp related variables */
static unsigned num_buffers=1; // default

static XImage *myximage[MAX_DRI_BUFFERS];
static int depth,bpp,mode;
static XWindowAttributes attribs;

static int Flip_Flag;
static int zoomFlag;

#ifdef HAVE_SHM
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>

static int Shmem_Flag;
static XShmSegmentInfo Shminfo[MAX_DRI_BUFFERS];
static int gXErrorFlag;
static int CompletionType=-1;

/* since it doesn't seem to be defined on some platforms */
extern int XShmGetEventBase( Display* );
#endif

static uint32_t image_width;
static uint32_t image_height;
static uint32_t in_format;
static uint32_t out_format=0;
static int baseAspect; // 1<<16 based fixed point aspect, so that the aspect stays correct during resizing
static XVisualInfo vinfo;

static void __FASTCALL__ getMyXImage(unsigned idx)
{
#ifdef HAVE_SHM
 if ( mLocalDisplay && XShmQueryExtension( mDisplay ) ) Shmem_Flag=1;
  else
   {
    Shmem_Flag=0;
    MSG_V( "Shared memory not supported\nReverting to normal Xlib\n" );
   }
 if ( Shmem_Flag ) CompletionType=XShmGetEventBase( mDisplay ) + ShmCompletion;

 if ( Shmem_Flag )
  {
   myximage[idx]=XShmCreateImage( mDisplay,vinfo.visual,depth,ZPixmap,NULL,&Shminfo[idx],image_width,image_height );
   if ( myximage[idx] == NULL )
    {
     if ( myximage[idx] != NULL ) XDestroyImage( myximage[idx] );
     MSG_V( "Shared memory error,disabling ( Ximage error )\n" );
     goto shmemerror;
    }
   Shminfo[idx].shmid=shmget( IPC_PRIVATE,
   myximage[idx]->bytes_per_line * myximage[idx]->height ,
   IPC_CREAT | 0777 );
   if ( Shminfo[idx].shmid < 0 )
   {
    XDestroyImage( myximage[idx] );
    MSG_V( "%s\n",strerror( errno ) );
    MSG_V( "Shared memory error,disabling ( seg id error )\n" );
    goto shmemerror;
   }
   Shminfo[idx].shmaddr=( char * ) shmat( Shminfo[idx].shmid,0,0 );

   if ( Shminfo[idx].shmaddr == ( ( char * ) -1 ) )
   {
    XDestroyImage( myximage[idx] );
    if ( Shminfo[idx].shmaddr != ( ( char * ) -1 ) ) shmdt( Shminfo[idx].shmaddr );
    MSG_V( "Shared memory error,disabling ( address error )\n" );
    goto shmemerror;
   }
   myximage[idx]->data=Shminfo[idx].shmaddr;
   Shminfo[idx].readOnly=False;
   XShmAttach( mDisplay,&Shminfo[idx] );

   XSync( mDisplay,False );

   if ( gXErrorFlag )
   {
    XDestroyImage( myximage[idx] );
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
   Shmem_Flag=0;
#endif
   myximage[idx]=XGetImage( mDisplay,vo_window,0,0,
   image_width,image_height,AllPlanes,ZPixmap );
#ifdef HAVE_SHM
  }
#endif
}

static void __FASTCALL__ freeMyXImage(unsigned idx)
{
#ifdef HAVE_SHM
 if ( Shmem_Flag )
  {
   XShmDetach( mDisplay,&Shminfo[idx] );
   XDestroyImage( myximage[idx] );
   shmdt( Shminfo[idx].shmaddr );
  }
  else
#endif
  {
   XDestroyImage( myximage[idx] );
  }
  myximage[idx]=NULL;
}


static uint32_t __FASTCALL__ check_events(int (* __FASTCALL__ adjust_size)(unsigned cw,unsigned ch,unsigned *w,unsigned *h))
{
  uint32_t ret = vo_x11_check_events(mDisplay,adjust_size);

   /* clear the old window */
  if (ret & VO_EVENT_RESIZE)
  {
	unsigned idx;
	unsigned newW= vo_dwidth;
	unsigned newH= vo_dheight;
	int newAspect= (newW*(1<<16) + (newH>>1))/newH;
	if(newAspect>baseAspect) newW= (newH*baseAspect + (1<<15))>>16;
	else                 newH= ((newW<<16) + (baseAspect>>1)) /baseAspect;
	XSetBackground(mDisplay, vo_gc, 0);
	XClearWindow(mDisplay, vo_window);
	image_width= (newW+7)&(~7);
	image_height= newH;
	if(enable_xp) LOCK_VDECODING();
	for(idx=0;idx<num_buffers;idx++)
	{
	    freeMyXImage(idx);
	    getMyXImage(idx);
	}
   }
   return ret;
}

static uint32_t __FASTCALL__ config( uint32_t width,uint32_t height,uint32_t d_width,uint32_t d_height,uint32_t flags,char *title,uint32_t format,const vo_tune_info_t *info)
{
// int screen;
 int fullscreen=0;
 int vm=0;
// int interval, prefer_blank, allow_exp, nothing;
 unsigned int fg,bg;
 XSizeHints hint;
 XEvent xev;
 XGCValues xgcv;
 Colormap theCmap;
 XSetWindowAttributes xswa;
 unsigned long xswamask;
 unsigned i;

 UNUSED(info);

 num_buffers=vo_doublebuffering?vo_da_buffs:1;

 if (!title)
    title = strdup("MPlayerXP X11 (XImage/Shm) render");

 in_format=format;
 
 if( flags&0x03 ) fullscreen = 1;
 if( flags&0x02 ) vm = 1;
 if( flags&0x08 ) Flip_Flag = 1;
 zoomFlag = flags&0x04;
// if(!fullscreen) zoomFlag=1; //it makes no sense to avoid zooming on windowd mode
 
 XGetWindowAttributes( mDisplay,DefaultRootWindow( mDisplay ),&attribs );
 depth=attribs.depth;

 if ( depth != 15 && depth != 16 && depth != 24 && depth != 32 ) depth=24;
 XMatchVisualInfo( mDisplay,mScreen,depth,TrueColor,&vinfo );

 /* set image size (which is indeed neither the input nor output size), 
    if zoom is on it will be changed during draw_slice anyway so we dont dupplicate the aspect code here 
 */
 image_width=(d_width + 7) & (~7);
 image_height=d_height;

 baseAspect= ((1<<16)*d_width + d_height/2)/d_height;

 aspect_save_orig(width,height);
 aspect_save_prescale(d_width,d_height);
 aspect_save_screenres(vo_screenwidth,vo_screenheight);

 softzoom=flags&VOFLAG_SWSCALE;

 aspect(&d_width,&d_height,A_NOZOOM);
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
    hint.flags=PPosition | PSize;

    bg=WhitePixel( mDisplay,mScreen );
    fg=BlackPixel( mDisplay,mScreen );
    vo_dwidth=hint.width;
    vo_dheight=hint.height;

    theCmap  =XCreateColormap( mDisplay,RootWindow( mDisplay,mScreen ),
    vinfo.visual,AllocNone );

    xswa.background_pixel=0;
    xswa.border_pixel=0;
    xswa.colormap=theCmap;
    xswamask=CWBackPixel | CWBorderPixel | CWColormap;

#ifdef HAVE_XF86VM
    if ( vm )
     {
      xswa.override_redirect=True;
      xswamask|=CWOverrideRedirect;
     }
#endif

    if ( WinID>=0 ){
      vo_window = WinID ? ((Window)WinID) : RootWindow( mDisplay,mScreen );
      XUnmapWindow( mDisplay,vo_window );
      XChangeWindowAttributes( mDisplay,vo_window,xswamask,&xswa );
    }
    else
      vo_window=XCreateWindow( mDisplay,RootWindow( mDisplay,mScreen ),
                         hint.x,hint.y,
                         hint.width,hint.height,
                         xswa.border_pixel,depth,CopyFromParent,vinfo.visual,xswamask,&xswa );

    vo_x11_classhint( mDisplay,vo_window,"x11" );
    vo_hidecursor(mDisplay,vo_window);
    if ( fullscreen ) vo_x11_decoration( mDisplay,vo_window,0 );
    XSelectInput( mDisplay,vo_window,StructureNotifyMask );
    XSetStandardProperties( mDisplay,vo_window,title,title,None,NULL,0,&hint );
    XMapWindow( mDisplay,vo_window );
#ifdef HAVE_XINERAMA
   vo_x11_xinerama_move(mDisplay,vo_window);
#endif
    if(WinID!=0)
    do { XNextEvent( mDisplay,&xev ); } while ( xev.type != MapNotify || xev.xmap.event != vo_window );
    XSelectInput( mDisplay,vo_window,NoEventMask );

    XFlush( mDisplay );
    XSync( mDisplay,False );
    vo_gc=XCreateGC( mDisplay,vo_window,0L,&xgcv );

    /* we cannot grab mouse events on root window :( */
    XSelectInput( mDisplay,vo_window,StructureNotifyMask | KeyPressMask | 
	((WinID==0)?0:(ButtonPressMask | ButtonReleaseMask | PointerMotionMask)) );

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
  for(i=0;i<num_buffers;i++) getMyXImage(i);

  switch ((bpp=myximage[0]->bits_per_pixel)){
	case 24: out_format= IMGFMT_BGR24; break;
	case 32: out_format= IMGFMT_BGR32; break;
	case 15: out_format= IMGFMT_BGR15; break;
	case 16: out_format= IMGFMT_BGR16; break;
	default: break;
  }

  /* If we have blue in the lowest bit then obviously RGB */
  mode=( ( myximage[0]->blue_mask & 0x01 ) != 0 ) ? MODE_RGB : MODE_BGR;
#ifdef WORDS_BIGENDIAN
  if ( myximage[0]->byte_order != MSBFirst )
#else
  if ( myximage[0]->byte_order != LSBFirst )
#endif
  {
    mode=( ( myximage[0]->blue_mask & 0x01 ) != 0 ) ? MODE_BGR : MODE_RGB;
  }

#ifdef WORDS_BIGENDIAN
  if(mode==MODE_BGR && bpp!=32){
    MSG_ERR("BGR%d not supported, please contact the developers\n", bpp);
    return -1;
  }
  if(mode==MODE_RGB && bpp==32){
    MSG_ERR("RGB32 not supported on big-endian systems, please contact the developers\n");
    return -1;
  }
#else
  if(mode==MODE_BGR){
    MSG_ERR("BGR not supported, please contact the developers\n");
    return -1;
  }
#endif  
 saver_off(mDisplay);
 return 0;
}

static const vo_info_t* get_info( void )
{ return &vo_info; }

static void __FASTCALL__ Display_Image( XImage *myximage )
{
#ifdef DISP
#ifdef HAVE_SHM
 if ( Shmem_Flag )
  {
   XShmPutImage( mDisplay,vo_window,vo_gc,myximage,
                 0,0,
                 ( vo_dwidth - myximage->width ) / 2,( vo_dheight - myximage->height ) / 2,
                 myximage->width,myximage->height,True );
  }
  else
#endif
   {
    XPutImage( mDisplay,vo_window,vo_gc,myximage,
               0,0,
               ( vo_dwidth - myximage->width ) / 2,( vo_dheight - myximage->height ) / 2,
               myximage->width,myximage->height);
  }
#endif
}

static void __FASTCALL__ flip_page( unsigned idx ){
 Display_Image( myximage[idx] );
 if (num_buffers>1) XFlush(mDisplay);
 else XSync(mDisplay, False);
 return;
}

static uint32_t __FASTCALL__ query_format( vo_query_fourcc_t* format )
{
    MSG_DBG2("vo_x11: query_format was called: %x (%s)\n",format->fourcc,vo_format_name(format->fourcc));
#ifdef WORDS_BIGENDIAN
    if (IMGFMT_IS_BGR(format->fourcc) && rgbfmt_depth(format->fourcc)<48)
#else
    if (IMGFMT_IS_RGB(format->fourcc) && rgbfmt_depth(format->fourcc)<48)
#endif
    {
	if (rgbfmt_depth(format->fourcc) == (unsigned)vo_depthonscreen)
	    return 0x1|0x2|0x4;
	else
	    return 0x1|0x4;
    }
    return 0;
}


static void uninit(void)
{
 unsigned i;
 for(i=0;i<num_buffers;i++)  freeMyXImage(i);
 saver_on(mDisplay); // screen saver back on

#ifdef HAVE_XF86VM
 vo_vm_close(mDisplay);
#endif

 vo_x11_uninit(mDisplay, vo_window);
}

static uint32_t __FASTCALL__ preinit(const char *arg)
{
    if(arg)
    {
	MSG_ERR("vo_x11: Unknown subdevice: %s\n",arg);
	return ENOSYS;
    }

    if( !vo_x11_init() ) return -1; // Can't open X11

    return 0;
}

#if 0
/* for runtime fullscreen switching */
static int vo_fs_oldx = -1;
static int vo_fs_oldy = -1;
static int vo_fs_oldwidth = -1;
static int vo_fs_oldheight = -1;
#endif

static void __FASTCALL__ x11_dri_get_surface_caps(dri_surface_cap_t *caps)
{
    caps->caps = DRI_CAP_TEMP_VIDEO;
    caps->fourcc = out_format;
    caps->width=image_width;
    caps->height=image_height;
    caps->x=0;
    caps->y=0;
    caps->w=image_width;
    caps->h=image_height;
    caps->strides[0] = image_width*((bpp+7)/8);
    caps->strides[1] = 0;
    caps->strides[2] = 0;
    caps->strides[3] = 0;
}

static void __FASTCALL__ x11_dri_get_surface(dri_surface_t *surf)
{
    surf->planes[0] = ImageData(surf->idx);
    surf->planes[1] = 0;
    surf->planes[2] = 0;
    surf->planes[3] = 0;
}

static uint32_t __FASTCALL__ control(uint32_t request, void *data)
{
  switch (request) {
  case VOCTRL_QUERY_FORMAT:
    return query_format((vo_query_fourcc_t*)data);
  case VOCTRL_CHECK_EVENTS:
    {
     vo_resize_t * vrest = (vo_resize_t *)data;
     vrest->event_type = check_events(vrest->adjust_size);
     return VO_TRUE;
    }
  case VOCTRL_FULLSCREEN:
    vo_x11_fullscreen();
    return VO_TRUE;
  case VOCTRL_GET_NUM_FRAMES:
	*(uint32_t *)data = num_buffers;
	return VO_TRUE;
  case DRI_GET_SURFACE_CAPS:
	x11_dri_get_surface_caps(data);
	return VO_TRUE;
  case DRI_GET_SURFACE:
	x11_dri_get_surface(data);
	return VO_TRUE;
  }
  return VO_NOTIMPL;
}
