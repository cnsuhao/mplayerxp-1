#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
/*
 * This file is part of MPlayerXP.
 *
 * MPlayer is mp_free software; you can redistribute it and/or modify
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

#include "mplayerxp.h"
#include "xmpcore/xmp_core.h"

#include "dri_vo.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "font_load.h"
#include "sub.h"
#include "libmpconf/subopt-helper.h"
#include "postproc/swscale.h" /* for MODE_RGB(BGR) definitions */

#ifdef GL_WIN32
#include <windows.h>
#include <GL/gl.h>
#include "w32_common.h"
#else
#include <X11/Xlib.h>
#include "x11_system.h"
#endif

#include "aspect.h"
#ifdef CONFIG_GUI
#include "gui/interface.h"
#endif
#include "osdep/fastmemcpy.h"
#include "postproc/vfcap.h"
#include "vo_msg.h"

static const vo_info_t vo_info =
{
  "X11 (OpenGL)",
  "opengl",
  "Nickols_K <nickols_k@mail.ru>",
  ""
};

LIBVO_EXTERN(opengl)

struct ogl_priv_t : public video_private {
    ogl_priv_t();
    virtual ~ogl_priv_t() {}

    uint32_t		image_width;
    uint32_t		image_height;
    uint32_t		image_format;
    uint32_t		dwidth,dheight;

    XWindowAttributes	attribs;
    XVisualInfo		vinfo;
    unsigned		depth,bpp,out_mode;

    unsigned		num_buffers; // 1 - default
    uint32_t		gl_out_format,out_format;
#ifdef HAVE_X11
    GLX_System*		glx;
#endif
};

ogl_priv_t::ogl_priv_t() {
    num_buffers=1;
}

static const vo_info_t *get_info(const vo_data_t*vo)
{
    UNUSED(vo);
    return(&vo_info);
}

static void gl_init_fb(vo_data_t*vo,unsigned x,unsigned y,unsigned d_width,unsigned d_height)
{
    ogl_priv_t& priv = *static_cast<ogl_priv_t*>(vo->priv);
    float sx = (GLfloat) (d_width-x) / (GLfloat)priv.image_width;
    float sy = (GLfloat) (d_height-y) / (GLfloat)priv.image_height;

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
    glPixelZoom(sx,vo_FLIP(vo)?sy:-sy);
}

static void resize(vo_data_t*vo,int x,int y){
    MSG_V("[gl] Resize: %dx%d\n",x,y);
    gl_init_fb(vo, 0, 0, x, y );
    glClear(GL_COLOR_BUFFER_BIT);
}

/* connect to server, create and map window,
 * allocate colors and (shared) memory
 */
static MPXP_Rc __FASTCALL__ config_vo(vo_data_t*vo,uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t flags, char *title, uint32_t format)
{
    ogl_priv_t& priv = *static_cast<ogl_priv_t*>(vo->priv);
    GLX_System& glx = *priv.glx;
    int is_bgr;
    XSizeHints hint;
    unsigned i;

    aspect_save_orig(width,height);
    aspect_save_prescale(d_width,d_height);

    priv.image_height = height;
    priv.image_width = width;
    priv.image_format=format;

    if ( vo_FS(vo) ) { vo->dest.w=d_width; vo->dest.h=d_height; }

    priv.num_buffers=vo_conf.xp_buffs;

    aspect_save_screenres(vo_conf.screenwidth,vo_conf.screenheight);
    aspect(&d_width,&d_height,vo_ZOOM(vo)?A_ZOOM:A_NOZOOM);
    glx.calcpos(vo,&hint,d_width,d_height,flags);

    hint.flags = PPosition | PSize;
    priv.dwidth=d_width; priv.dheight=d_height; //XXX: what are the copy vars used for?

    priv.depth=glx.depth();

    if (priv.depth != 15 && priv.depth != 16 && priv.depth != 24 && priv.depth != 32)
	priv.depth = 24;
    glx.match_visual( &priv.vinfo);

    glx.create_window(hint,priv.vinfo.visual,vo_VM(vo),priv.depth,title);

    glx.classhint("opengl");
    glx.hidecursor();

    if ( vo_FS(vo) ) glx.decoration(0);

    /* we cannot grab mouse events on root window :( */
    glx.select_input(StructureNotifyMask | KeyPressMask |
		    ButtonPressMask | ButtonReleaseMask | PointerMotionMask);
#ifdef GL_WIN32
    if (!vo_w32_config(d_width, d_height, flags)) return MPXP_False;
#else
    {
	XVisualInfo *vi;
	vi = glx.get_visual();
	if (vi == NULL) {
	    MSG_ERR("[vo_oengl]: Can't get XVisualInfo\n");
	    return MPXP_False;
	}
	glx.create_context(vi);
	::XFree(vi);
    }
#endif
    gl_init_fb(vo,0,0,d_width,d_height);
    /* allocate multibuffers */
    for(i=0;i<priv.num_buffers;i++) glx.getMyXImage(i,priv.vinfo.visual,priv.depth,priv.image_width,priv.image_height);

    priv.out_mode=GL_RGB;
    XImage *ximg=glx.Image(0);
    is_bgr=(ximg->blue_mask&0x01)!=0;
    switch ((priv.bpp=ximg->bits_per_pixel)){
	case 32:priv.out_mode=GL_RGBA;
		priv.gl_out_format=is_bgr?GL_UNSIGNED_INT_8_8_8_8_REV:GL_UNSIGNED_INT_8_8_8_8;
		priv.out_format = IMGFMT_RGB32;
		break;
	case 24:priv.gl_out_format=is_bgr?GL_UNSIGNED_INT_8_8_8_8_REV:GL_UNSIGNED_INT_8_8_8_8;
		priv.out_format = IMGFMT_RGB24;
		break;
	case 15:priv.gl_out_format=is_bgr?GL_UNSIGNED_SHORT_1_5_5_5_REV:GL_UNSIGNED_SHORT_5_5_5_1;
		priv.out_format = IMGFMT_RGB15;
		break;
	case 16:priv.gl_out_format=is_bgr?GL_UNSIGNED_SHORT_5_6_5_REV:GL_UNSIGNED_SHORT_5_6_5;
		priv.out_format = IMGFMT_RGB16;
		break;
	default: break;
    }
    return MPXP_Ok;
}

static uint32_t __FASTCALL__ check_events(vo_data_t*vo,vo_adjust_size_t adjust_size)
{
    ogl_priv_t& priv = *static_cast<ogl_priv_t*>(vo->priv);
    GLX_System& glx = *priv.glx;
    int e=glx.check_events(vo,adjust_size);
    if(e&VO_EVENT_RESIZE) resize(vo,vo->dest.w,vo->dest.h);
    return e|VO_EVENT_FORCE_UPDATE;
}

static void __FASTCALL__ gl_display_Image(vo_data_t*vo,XImage *myximage )
{
    ogl_priv_t& priv = *static_cast<ogl_priv_t*>(vo->priv);
    glDrawPixels(priv.image_width,
		priv.image_height,
		priv.out_mode,
		priv.gl_out_format,
		myximage->data);
}

static void select_frame(vo_data_t*vo,unsigned idx) {
    ogl_priv_t& priv = *static_cast<ogl_priv_t*>(vo->priv);
    GLX_System& glx = *priv.glx;

    gl_display_Image(vo,glx.Image(idx));
    if (priv.num_buffers>1) glx.swap_buffers();
    glFlush();
    return;
}

static MPXP_Rc __FASTCALL__ query_format( vo_query_fourcc_t* format )
{
    MSG_DBG2("vo_opengl: query_format was called: %x (%s)\n",format->fourcc,vo_format_name(format->fourcc));
    if((IMGFMT_IS_BGR(format->fourcc)||IMGFMT_IS_RGB(format->fourcc))&&rgbfmt_depth(format->fourcc)<48) {
	MSG_DBG2("vo_opengl: OK\n");
	format->flags=VOCAP_SUPPORTED | VOCAP_HWSCALER | VOCAP_FLIP;
	return MPXP_Ok;
    }
    MSG_DBG2("vo_opengl: FALSE\n");
    return MPXP_False;
}


static void uninit(vo_data_t*vo)
{
    ogl_priv_t& priv = *static_cast<ogl_priv_t*>(vo->priv);
    GLX_System& glx = *priv.glx;
    unsigned i;
//  if (!vo_config_count) return;
    glFinish();
    glx.destroy_context();
    for(i=0;i<priv.num_buffers;i++)  glx.freeMyXImage(i);
    glx.saver_on(); // screen saver back on
#ifdef HAVE_XF86VM
    glx.vm_close();
#endif
    delete &glx;
    delete vo->priv;
}

static MPXP_Rc __FASTCALL__ preinit(vo_data_t*vo,const char *arg)
{
    ogl_priv_t*priv;
    priv=new(zeromem) ogl_priv_t;
    vo->priv=priv;
    UNUSED(arg);
    priv->glx=new(zeromem) GLX_System(vo_conf.mDisplayName);
    priv->glx->saver_off();
    return MPXP_Ok;
}

static void __FASTCALL__ gl_dri_get_surface_caps(const vo_data_t*vo,dri_surface_cap_t *caps)
{
    ogl_priv_t& priv = *static_cast<ogl_priv_t*>(vo->priv);
    caps->caps =DRI_CAP_TEMP_VIDEO|
		DRI_CAP_HORZSCALER|DRI_CAP_VERTSCALER|
		DRI_CAP_DOWNSCALER|DRI_CAP_UPSCALER;
    caps->fourcc = priv.out_format;
    caps->width=priv.image_width;
    caps->height=priv.image_height;
    caps->x=0;
    caps->y=0;
    caps->w=priv.image_width;
    caps->h=priv.image_height;
    caps->strides[0] = priv.image_width*((priv.bpp+7)/8);
    caps->strides[1] = 0;
    caps->strides[2] = 0;
    caps->strides[3] = 0;
}

static void __FASTCALL__ gl_dri_get_surface(const vo_data_t*vo,dri_surface_t *surf)
{
    ogl_priv_t& priv = *static_cast<ogl_priv_t*>(vo->priv);
    GLX_System& glx = *priv.glx;
    surf->planes[0] = glx.ImageData(surf->idx);
    surf->planes[1] = 0;
    surf->planes[2] = 0;
    surf->planes[3] = 0;
}

static MPXP_Rc control_vo(vo_data_t*vo,uint32_t request, any_t*data)
{
    ogl_priv_t& priv = *static_cast<ogl_priv_t*>(vo->priv);
    switch (request) {
	case VOCTRL_QUERY_FORMAT:
	    return query_format((vo_query_fourcc_t*)data);
	case VOCTRL_FULLSCREEN:
	    vo_fullscreen(vo);
	    resize(vo,vo->dest.w, vo->dest.h);
	    return MPXP_True;
	case VOCTRL_GET_NUM_FRAMES:
	    *(uint32_t *)data = priv.num_buffers;
	    return MPXP_True;
	case DRI_GET_SURFACE_CAPS:
	    gl_dri_get_surface_caps(vo,reinterpret_cast<dri_surface_cap_t*>(data));
	    return MPXP_True;
	case DRI_GET_SURFACE:
	    gl_dri_get_surface(vo,reinterpret_cast<dri_surface_t*>(data));
	    return MPXP_True;
	case VOCTRL_CHECK_EVENTS: {
	    vo_resize_t * vrest = (vo_resize_t *)data;
	    vrest->event_type = check_events(vo,vrest->adjust_size);
	    return MPXP_True;
	}
    }
    return MPXP_NA;
}
