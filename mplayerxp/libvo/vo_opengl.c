/*
 * This file is part of MPlayerXP.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <dlfcn.h>


#include "../mp_config.h"
#include "../mplayer.h"
#include "../dec_ahead.h"
#include "dri_vo.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "font_load.h"
#include "sub.h"
#include "subopt-helper.h"
#include "../postproc/swscale.h" /* for MODE_RGB(BGR) definitions */

#ifdef GL_WIN32
#include <windows.h>
#include <GL/gl.h>
#include "w32_common.h"
#else
#include <GL/gl.h>
#include <X11/Xlib.h>
#include <GL/glx.h>
#include <GL/glut.h>
#include "x11_common.h"
#endif

#include "aspect.h"
#ifdef CONFIG_GUI
#include "gui/interface.h"
#endif
#include "fastmemcpy.h"
#include "../postproc/vfcap.h"
#include "vo_msg.h"

static const vo_info_t vo_info =
{
  "X11 (OpenGL)",
  "opengl",
  "Nickols_K <nickols_k@mail.ru>",
  ""
};

const LIBVO_EXTERN(opengl)

typedef struct vogl_priv_s {
    uint32_t		image_width;
    uint32_t		image_height;
    uint32_t		image_format;
    uint32_t		dwidth,dheight;

    XWindowAttributes	attribs;
    XVisualInfo		vinfo;
    unsigned		depth,bpp,out_mode;

    unsigned		num_buffers; // 1 - default
    any_t*		glx_context;
    uint32_t		gl_out_format,out_format;
}vogl_priv_t;
static vogl_priv_t vogl;


static const vo_info_t *get_info(void)
{
    return(&vo_info);
}

static XVisualInfo *get_visual_info(Display *dpy, Window win)
{
    XWindowAttributes wattr;
    XVisualInfo vi_template;
    int dummy;

    XGetWindowAttributes(dpy, win, &wattr);
    vi_template.visualid = XVisualIDFromVisual(wattr.visual);
    return XGetVisualInfo(dpy, VisualIDMask, &vi_template, &dummy);
}

static void gl_init_fb(unsigned x,unsigned y,unsigned d_width,unsigned d_height)
{
    float sx = (GLfloat) (d_width-x) / (GLfloat)vogl.image_width;
    float sy = (GLfloat) (d_height-y) / (GLfloat)vogl.image_height;

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

#define FOVY     60.0f
#define ASPECT   1.0f
#define Z_NEAR   0.1f
#define Z_FAR    100.0f
#define Z_CAMERA 0.869f

    glViewport(x, y, d_width, d_height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(FOVY, ASPECT, Z_NEAR, Z_FAR);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glTranslatef(-0.5f, -0.5f, -Z_CAMERA);
    glScalef(1.0f / (GLfloat)d_width,
             -1.0f / (GLfloat)d_height,
             1.0f / (GLfloat)d_width);
    glTranslatef(0.0f, -1.0f * (GLfloat)d_height, 0.0f);

    glClearColor(0.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
    glRasterPos2i(x, y);
    glPixelZoom(sx,vo.flip?sy:-sy);
}

static void resize(int x,int y){
  MSG_V("[gl] Resize: %dx%d\n",x,y);
  if (vo.WinID >= 0) {
    unsigned top = 0, left = 0, w = x, h = y;
    aspect(&w,&h,A_ZOOM);
    left=( vo.screenwidth - (vogl.dwidth > vo.screenwidth?vo.screenwidth:vogl.dwidth) ) / 2;
    top=( vo.screenheight - (vogl.dheight > vo.screenheight?vo.screenheight:vogl.dheight) ) / 2;
    w=(vogl.dwidth > vo.screenwidth?vo.screenwidth:vogl.dwidth);
    h=(vogl.dheight > vo.screenheight?vo.screenheight:vogl.dheight);
    gl_init_fb(left,top,w,h);
  } else
  gl_init_fb( 0, 0, x, y );
  glClear(GL_COLOR_BUFFER_BIT);
}

/* connect to server, create and map window,
 * allocate colors and (shared) memory
 */
static uint32_t __FASTCALL__ config(uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t flags, char *title, uint32_t format,const vo_tune_info_t *info)
{
 int is_bgr;
 char *hello = (title == NULL) ? "Glx render" : title;
 XSizeHints hint;

 XGCValues xgcv;
 XSetWindowAttributes xswa;
 unsigned long xswamask,i;
#ifdef HAVE_XF86VM
 int vm=0;
#endif

 UNUSED(info);
 aspect_save_orig(width,height);
 aspect_save_prescale(d_width,d_height);

 vogl.image_height = height;
 vogl.image_width = width;
 vogl.image_format=format;

 vo.fs=flags&VOFLAG_FULLSCREEN;
 vo.softzoom=flags&VOFLAG_SWSCALE;
 if ( vo.fs )
  { vo.dest.w=d_width; vo.dest.h=d_height; }

#ifdef HAVE_XF86VM
 if( flags&0x02 ) vm = 1;
#endif
 vo.flip=flags&VOFLAG_FLIPPING;
 vogl.num_buffers=vo.da_buffs;


 aspect_save_screenres(vo.screenwidth,vo.screenheight);
 aspect(&d_width,&d_height,vo.softzoom?A_ZOOM:A_NOZOOM);
  if( vo.fs ) aspect(&d_width,&d_height,A_ZOOM);
   vo_x11_calcpos(&hint,d_width,d_height,flags);
   hint.flags = PPosition | PSize;

   vogl.dwidth=d_width; vogl.dheight=d_height; //XXX: what are the copy vars used for?

   XGetWindowAttributes(vo.mDisplay, DefaultRootWindow(vo.mDisplay), &vogl.attribs);
   vogl.depth=vogl.attribs.depth;
   if (vogl.depth != 15 && vogl.depth != 16 && vogl.depth != 24 && vogl.depth != 32) vogl.depth = 24;
   XMatchVisualInfo(vo.mDisplay, vo.mScreen, vogl.depth, TrueColor, &vogl.vinfo);

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
       0, vogl.depth,CopyFromParent,vogl.vinfo.visual,xswamask,&xswa);

   vo_x11_classhint( vo.mDisplay,vo.window,"opengl" );
   vo_x11_hidecursor(vo.mDisplay,vo.window);

   XSelectInput(vo.mDisplay, vo.window, StructureNotifyMask | KeyPressMask |
	((vo.WinID==0) ? 0 : (PointerMotionMask
		| ButtonPressMask | ButtonReleaseMask )));
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

#ifdef GL_WIN32
  if (!vo_w32_config(d_width, d_height, flags))
    return -1;
#else
  {
    XVisualInfo *vi;
    vi = get_visual_info(vo.mDisplay, vo.window);
    if (vi == NULL) {
	MSG_ERR("[vo_oengl]: Can't get XVisualInfo\n");
	return -1;
    }
    vogl.glx_context = glXCreateContext(vo.mDisplay, vi, NULL, True);
    XFree(vi);
    if (vogl.glx_context == NULL) {
	MSG_ERR("[vo_oengl]: Can't create GLX context\n");
	return -1;
    }
    if (!glXMakeCurrent(vo.mDisplay, vo.window, vogl.glx_context)) {
	MSG_ERR("[vo_oengl]: Can't make GLX context current\n");
	return -1;
    }
  }
#endif
    gl_init_fb(0,0,d_width,d_height);
    /* allocate multibuffers */
    for(i=0;i<vogl.num_buffers;i++) vo_x11_getMyXImage(i,vogl.vinfo.visual,vogl.depth,vogl.image_width,vogl.image_height);

    vogl.out_mode=GL_RGB;
    is_bgr=(vo_x11_myximage[0]->blue_mask&0x01)!=0;
    switch ((vogl.bpp=vo_x11_myximage[0]->bits_per_pixel)){
	case 32:vogl.out_mode=GL_RGBA;
		vogl.gl_out_format=is_bgr?GL_UNSIGNED_INT_8_8_8_8_REV:GL_UNSIGNED_INT_8_8_8_8;
		vogl.out_format = IMGFMT_RGB32;
		break;
	case 24:vogl.gl_out_format=is_bgr?GL_UNSIGNED_INT_8_8_8_8_REV:GL_UNSIGNED_INT_8_8_8_8;
		vogl.out_format = IMGFMT_RGB24;
		break;
	case 15:vogl.gl_out_format=is_bgr?GL_UNSIGNED_SHORT_1_5_5_5_REV:GL_UNSIGNED_SHORT_5_5_5_1;
		vogl.out_format = IMGFMT_RGB15;
		break;
	case 16:vogl.gl_out_format=is_bgr?GL_UNSIGNED_SHORT_5_6_5_REV:GL_UNSIGNED_SHORT_5_6_5;
		vogl.out_format = IMGFMT_RGB16;
		break;
	default: break;
    }
    saver_off(vo.mDisplay);
  return 0;
}

static uint32_t __FASTCALL__ check_events(int (* __FASTCALL__ adjust_size)(unsigned cw,unsigned ch,unsigned *w,unsigned *h))
{
    int e=vo_x11_check_events(vo.mDisplay,adjust_size);
    if(e&VO_EVENT_RESIZE) resize(vo.dest.w,vo.dest.h);
    return e|VO_EVENT_FORCE_UPDATE;
}

static void __FASTCALL__ gl_display_Image( XImage *myximage )
{
    glDrawPixels(vogl.image_width,
		vogl.image_height,
		vogl.out_mode,
		vogl.gl_out_format,
		myximage->data);
}

static void select_frame(unsigned idx) {
 gl_display_Image( vo_x11_myximage[idx] );
 if (vogl.num_buffers>1) glXSwapBuffers(vo.mDisplay, vo.window);
 glFlush();
 return;
}

static uint32_t __FASTCALL__ query_format( vo_query_fourcc_t* format )
{
    MSG_DBG2("vo_opengl: query_format was called: %x (%s)\n",format->fourcc,vo_format_name(format->fourcc));
    if((IMGFMT_IS_BGR(format->fourcc)||IMGFMT_IS_RGB(format->fourcc))&&rgbfmt_depth(format->fourcc)<48)
    {
	MSG_DBG2("vo_opengl: OK\n");
	return  VFCAP_CSP_SUPPORTED | VFCAP_CSP_SUPPORTED_BY_HW | VFCAP_FLIP |
		VFCAP_HWSCALE_UP | VFCAP_HWSCALE_DOWN;
    }
    MSG_DBG2("vo_opengl: FALSE\n");
    return 0;
}


static void
uninit(void)
{
  unsigned i;
//  if (!vo_config_count) return;
  glFinish();
  glXMakeCurrent(vo.mDisplay, None, NULL);
  glXDestroyContext(vo.mDisplay, vogl.glx_context);
  for(i=0;i<vogl.num_buffers;i++)  vo_x11_freeMyXImage(i);
  saver_on(vo.mDisplay); // screen saver back on
#ifdef HAVE_XF86VM
  vo_vm_close(vo.mDisplay);
#endif
  vo_x11_uninit(vo.mDisplay, vo.window);
}

static uint32_t __FASTCALL__ preinit(const char *arg)
{
    memset(&vogl,0,sizeof(vogl_priv_t));
    vogl.num_buffers=1;
    UNUSED(arg);
    if (!vo_x11_init()) return -1;
    return 0;
}

static void __FASTCALL__ gl_dri_get_surface_caps(dri_surface_cap_t *caps)
{
    caps->caps =DRI_CAP_TEMP_VIDEO|
		DRI_CAP_HORZSCALER|DRI_CAP_VERTSCALER|
		DRI_CAP_DOWNSCALER|DRI_CAP_UPSCALER;
    caps->fourcc = vogl.out_format;
    caps->width=vogl.image_width;
    caps->height=vogl.image_height;
    caps->x=0;
    caps->y=0;
    caps->w=vogl.image_width;
    caps->h=vogl.image_height;
    caps->strides[0] = vogl.image_width*((vogl.bpp+7)/8);
    caps->strides[1] = 0;
    caps->strides[2] = 0;
    caps->strides[3] = 0;
}

static void __FASTCALL__ gl_dri_get_surface(dri_surface_t *surf)
{
    surf->planes[0] = vo_x11_ImageData(surf->idx);
    surf->planes[1] = 0;
    surf->planes[2] = 0;
    surf->planes[3] = 0;
}

static uint32_t control(uint32_t request, any_t*data)
{
  switch (request) {
  case VOCTRL_QUERY_FORMAT:
    return query_format((vo_query_fourcc_t*)data);
  case VOCTRL_FULLSCREEN:
    vo_fullscreen();
    resize(vo.dest.w, vo.dest.h);
    return VO_TRUE;
  case VOCTRL_GET_NUM_FRAMES:
	*(uint32_t *)data = vogl.num_buffers;
	return VO_TRUE;
  case DRI_GET_SURFACE_CAPS:
	gl_dri_get_surface_caps(data);
	return VO_TRUE;
  case DRI_GET_SURFACE:
	gl_dri_get_surface(data);
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
