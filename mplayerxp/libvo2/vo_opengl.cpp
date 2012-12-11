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
namespace mpxp {
class OpenGL_VO_Interface : public VO_Interface {
    public:
	OpenGL_VO_Interface(const std::string& args);
	virtual ~OpenGL_VO_Interface();

	virtual MPXP_Rc	configure(uint32_t width,
				uint32_t height,
				uint32_t d_width,
				uint32_t d_height,
				unsigned flags,
				const std::string& title,
				uint32_t format);
	virtual MPXP_Rc	select_frame(unsigned idx);
	virtual void	get_surface_caps(dri_surface_cap_t *caps) const;
	virtual void	get_surface(dri_surface_t *surf);
	virtual MPXP_Rc	query_format(vo_query_fourcc_t* format) const;
	virtual unsigned get_num_frames() const;

	virtual MPXP_Rc	toggle_fullscreen();
	virtual uint32_t check_events(const vo_resize_t*);
	virtual MPXP_Rc	ctrl(uint32_t request, any_t*data);
    private:
	void		gl_init_fb(unsigned x,unsigned y,unsigned d_width,unsigned d_height) const;
	void		resize(int x,int y) const;
	void		gl_display_Image(XImage *myximage) const;

	LocalPtr<Aspect>	aspect;
	uint32_t		image_width;
	uint32_t		image_height;
	uint32_t		image_format;
	uint32_t		dwidth,dheight;

	XWindowAttributes	attribs;
	XVisualInfo		vinfo;
	unsigned		depth,bpp,out_mode,flags;

	unsigned		num_buffers; // 1 - default
	uint32_t		gl_out_format,out_format;
	LocalPtr<GLX_System>	glx;
};

OpenGL_VO_Interface::OpenGL_VO_Interface(const std::string& arg)
			    :VO_Interface(arg),
			    aspect(new(zeromem) Aspect(mp_conf.monitor_pixel_aspect)),
			    glx(new(zeromem) GLX_System(vo_conf.mDisplayName,vo_conf.xinerama_screen))
{
    num_buffers=1;
    glx->saver_off();
}

OpenGL_VO_Interface::~OpenGL_VO_Interface() {
    unsigned i;
    glFinish();
    for(i=0;i<num_buffers;i++) glx->freeMyXImage(i);
    glx->saver_on(); // screen saver back on
#ifdef HAVE_XF86VM
    glx->vm_close();
#endif
}

void OpenGL_VO_Interface::gl_init_fb(unsigned x,unsigned y,unsigned d_width,unsigned d_height) const
{
    float sx = (GLfloat) (d_width-x) / (GLfloat)image_width;
    float sy = (GLfloat) (d_height-y) / (GLfloat)image_height;

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
    glPixelZoom(sx,(flags&VOFLAG_FLIPPING)?sy:-sy);
}

void OpenGL_VO_Interface::resize(int x,int y) const {
    MSG_V("[gl] Resize: %dx%d\n",x,y);
    gl_init_fb(0, 0, x, y);
    glClear(GL_COLOR_BUFFER_BIT);
}

/* connect to server, create and map window,
 * allocate colors and (shared) memory
 */
MPXP_Rc OpenGL_VO_Interface::configure(uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height,unsigned _flags, const std::string& title, uint32_t format)
{
    int is_bgr;
    XSizeHints hint;
    unsigned i;

    flags=_flags;

    aspect->save(width,height,d_width,d_height,glx->screen_width(),glx->screen_height());
    aspect->calc(d_width,d_height,flags&VOFLAG_FULLSCREEN?Aspect::ZOOM:Aspect::NOZOOM);

    image_height= height;
    image_width = width;
    image_format= format;

    num_buffers=vo_conf.xp_buffs;

    glx->calcpos(&hint,d_width,d_height,flags);

    hint.flags = PPosition | PSize;
    dwidth=d_width; dheight=d_height; //XXX: what are the copy vars used for?

    depth=glx->depth();

    XVisualInfo* vis=glx->get_visual();
    vinfo=*vis;

    glx->create_window(hint,&vinfo,flags,depth,title);

    gl_init_fb(0,0,d_width,d_height);

    /* allocate multibuffers */
    for(i=0;i<num_buffers;i++) glx->getMyXImage(i,vinfo.visual,depth,image_width,image_height);

    out_mode=GL_RGB;
    XImage *ximg=glx->Image(0);
    is_bgr=(ximg->blue_mask&0x01)!=0;
    switch ((bpp=ximg->bits_per_pixel)){
	case 32:out_mode=GL_RGBA;
		gl_out_format=is_bgr?GL_UNSIGNED_INT_8_8_8_8_REV:GL_UNSIGNED_INT_8_8_8_8;
		out_format = IMGFMT_RGB32;
		break;
	case 24:gl_out_format=is_bgr?GL_UNSIGNED_INT_8_8_8_8_REV:GL_UNSIGNED_INT_8_8_8_8;
		out_format = IMGFMT_RGB24;
		break;
	case 15:gl_out_format=is_bgr?GL_UNSIGNED_SHORT_1_5_5_5_REV:GL_UNSIGNED_SHORT_5_5_5_1;
		out_format = IMGFMT_RGB15;
		break;
	case 16:gl_out_format=is_bgr?GL_UNSIGNED_SHORT_5_6_5_REV:GL_UNSIGNED_SHORT_5_6_5;
		out_format = IMGFMT_RGB16;
		break;
	default: break;
    }
    return MPXP_Ok;
}

uint32_t OpenGL_VO_Interface::check_events(const vo_resize_t* vrest)
{
    int e=glx->check_events(vrest->adjust_size,vrest->vo);
    vo_rect_t r;
    glx->get_win_coord(r);
    if(e&VO_EVENT_RESIZE) resize(r.w,r.h);
    return e|VO_EVENT_FORCE_UPDATE;
}

void OpenGL_VO_Interface::gl_display_Image(XImage *myximage) const
{
    glDrawPixels(image_width,
		image_height,
		out_mode,
		gl_out_format,
		myximage->data);
}

MPXP_Rc OpenGL_VO_Interface::select_frame(unsigned idx) {
    gl_display_Image(glx->Image(idx));
    if (num_buffers>1) glx->swap_buffers();
    glFlush();
    return MPXP_Ok;
}

MPXP_Rc OpenGL_VO_Interface::query_format( vo_query_fourcc_t* format ) const
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


void OpenGL_VO_Interface::get_surface_caps(dri_surface_cap_t *caps) const
{
    caps->caps =DRI_CAP_TEMP_VIDEO|
		DRI_CAP_HORZSCALER|DRI_CAP_VERTSCALER|
		DRI_CAP_DOWNSCALER|DRI_CAP_UPSCALER;
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

void OpenGL_VO_Interface::get_surface(dri_surface_t *surf)
{
    surf->planes[0] = glx->ImageData(surf->idx);
    surf->planes[1] = 0;
    surf->planes[2] = 0;
    surf->planes[3] = 0;
}

unsigned OpenGL_VO_Interface::get_num_frames() const { return num_buffers; }

MPXP_Rc OpenGL_VO_Interface::toggle_fullscreen() {
    glx->fullscreen(flags);
    vo_rect_t r;
    glx->get_win_coord(r);
    resize(r.w,r.h);
    return MPXP_True;
}

MPXP_Rc OpenGL_VO_Interface::ctrl(uint32_t request, any_t*data) {
    UNUSED(request);
    UNUSED(data);
    return MPXP_NA;
}

static VO_Interface* query_interface(const std::string& args) { return new(zeromem) OpenGL_VO_Interface(args); }
extern const vo_info_t opengl_vo_info =
{
  "X11 (OpenGL)",
  "opengl",
  "Nickols_K <nickols_k@mail.ru>",
  "",
  query_interface
};
} //namespace
