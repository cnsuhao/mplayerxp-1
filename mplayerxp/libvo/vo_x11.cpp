#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;

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

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

#include "mplayerxp.h"
#include "xmpcore/xmp_core.h"
#include "aspect.h"
#include "video_out.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#ifdef HAVE_XF86VM
#include <X11/extensions/xf86vmode.h>
#endif

#include "x11_system.h"

#include "osdep/fastmemcpy.h"
#include "sub.h"

#include "postproc/swscale.h" /* for MODE_RGB(BGR) definitions */
#include "video_out_internal.h"
#include "video_out.h"
#ifdef CONFIG_VIDIX
#include "vidix_system.h"
#endif
#include "dri_vo.h"
#include "xmpcore/mp_image.h"
#include "vo_msg.h"

class X11_VO_Interface : public VO_Interface {
    public:
	X11_VO_Interface(const char* args);
	virtual ~X11_VO_Interface();

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
	MPXP_Rc		query_format(vo_query_fourcc_t* format) const;
	virtual unsigned get_num_frames() const;

	virtual MPXP_Rc	toggle_fullscreen();
	virtual uint32_t check_events(const vo_resize_t*);
	virtual MPXP_Rc	ctrl(uint32_t request, any_t*data);
    private:
	const char*	parse_sub_device(const char *sd);
	void		resize(int x,int y) const;
	void		display_image(XImage * myximage) const;
	void		lock_surfaces();
	void		unlock_surfaces();

	Aspect&		aspect;
	uint32_t	image_width;
	uint32_t	image_height;
	uint32_t	in_format;
	uint32_t	out_format;

	unsigned	depth,bpp,mode,flags;

	XVisualInfo	vinfo;

	int		baseAspect; // 1<<16 based fixed point aspect, so that the aspect stays correct during resizing
/* xp related variables */
	unsigned	num_buffers; // 1 - default
#ifdef CONFIG_VIDIX
	Vidix_System*	vidix;
	void		resize_vidix() const;
#endif
	uint32_t	subdev_flags;
	X11_System&	x11;

	pthread_mutex_t	surfaces_mutex;
};

void X11_VO_Interface::lock_surfaces() {
    pthread_mutex_lock(&surfaces_mutex);
}

void X11_VO_Interface::unlock_surfaces() {
    pthread_mutex_unlock(&surfaces_mutex);
}

#ifdef HAVE_SHM
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#endif

const char* X11_VO_Interface::parse_sub_device(const char *sd)
{
#ifdef CONFIG_VIDIX
    if(memcmp(sd,"vidix",5) == 0) return &sd[5]; /* vidix_name will be valid within init() */
#endif
    MSG_ERR("vo_x11: Unknown subdevice: '%s'\n", sd);
    return NULL;
}

X11_VO_Interface::X11_VO_Interface(const char *arg)
		:VO_Interface(arg),
		aspect(*new(zeromem) Aspect(mp_conf.monitor_pixel_aspect)),
		x11(*new(zeromem) X11_System(vo_conf.mDisplayName,vo_conf.xinerama_screen))
{
    const char* vidix_name=NULL;
    num_buffers=1;
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutex_init(&surfaces_mutex,&attr);

    if(arg) vidix_name = parse_sub_device(arg);
#ifdef CONFIG_VIDIX
    if(vidix_name) {
MSG_INFO("args=%s vidix-name=%s\n",arg,vidix_name);
	if(!(vidix=new(zeromem) Vidix_System(vidix_name))) {
	    MSG_ERR("Cannot initialze vidix with '%s' argument\n",vidix_name);
	    exit_player("Vidix error");
	}
    }
#endif
    x11.saver_off();
}

X11_VO_Interface::~X11_VO_Interface()
{
    unsigned i;
#ifdef CONFIG_VIDIX
    if(vidix) delete vidix;
#endif
    for(i=0;i<num_buffers;i++)  x11.freeMyXImage(i);
    x11.saver_on(); // screen saver back on
#ifdef HAVE_XF86VM
    x11.vm_close();
#endif
    pthread_mutex_destroy(&surfaces_mutex);
}

#ifdef CONFIG_VIDIX
void X11_VO_Interface::resize_vidix() const {
    vo_rect_t winc;
    x11.get_win_coord(winc);
    vidix->stop();
    if (vidix->configure(image_width, image_height, winc.x, winc.y,
	    winc.w, winc.h, in_format, x11.depth(),
	    x11.screen_width(), x11.screen_height()) != MPXP_Ok)
    {
	MSG_FATAL( "Can't initialize VIDIX: %s\n",strerror(errno));
	delete vidix;
	exit_player("Vidix init"); /* !!! */
    }
    if(vidix->start()!=0) { delete vidix; exit_player("Vidix start"); }
}
#endif

uint32_t X11_VO_Interface::check_events(const vo_resize_t*vrest)
{
    uint32_t ret = x11.check_events(vrest->adjust_size,vrest->vo);

    /* clear the old window */
    if (ret & VO_EVENT_RESIZE) {
	unsigned idx;
	vo_rect_t r;
	x11.get_win_coord(r);
	unsigned newW= r.w;
	unsigned newH= r.h;
	int newAspect=		(newW*(1<<16) + (newH>>1))/newH;
	if(newAspect>baseAspect)newW= (newH*baseAspect + (1<<15))>>16;
	else			newH= ((newW<<16) + (baseAspect>>1)) /baseAspect;
	image_width= (newW+7)&(~7);
	image_height= newH;
#ifdef CONFIG_VIDIX
	if(vidix) resize_vidix();
	else
#endif
	{
	    lock_surfaces();
	    for(idx=0;idx<num_buffers;idx++) {
		x11.freeMyXImage(idx);
		x11.getMyXImage(idx,vinfo.visual,depth,image_width,image_height);
	    }
	    unlock_surfaces();
	}
   }
   return ret;
}

MPXP_Rc X11_VO_Interface::configure(uint32_t width,uint32_t height,uint32_t d_width,uint32_t d_height,unsigned _flags,const char *title,uint32_t format)
{
    XSizeHints hint;
    unsigned i;

    flags=_flags;
    num_buffers=vo_conf.xp_buffs;

    in_format=format;

    depth=x11.depth();
    if ( depth != 15 && depth != 16 && depth != 24 && depth != 32 )
	depth=24;
    x11.match_visual( &vinfo );

    baseAspect= ((1<<16)*d_width + d_height/2)/d_height;

    aspect.save(width,height,d_width,d_height,x11.screen_width(),x11.screen_height());
    aspect.calc(d_width,d_height,flags&VOFLAG_FULLSCREEN?Aspect::ZOOM:Aspect::NOZOOM);

    x11.calcpos(&hint,d_width,d_height,flags);
    hint.flags=PPosition | PSize;

    image_width=d_width;
    image_height=d_height;

    x11.create_window(hint,&vinfo,flags,depth,title);

#ifdef CONFIG_VIDIX
    if(!vidix)
#endif
    for(i=0;i<num_buffers;i++) x11.getMyXImage(i,vinfo.visual,depth,image_width,image_height);

#ifdef CONFIG_VIDIX
    if(!vidix) {
#endif
    XImage* ximg=x11.Image(0);
    switch ((bpp=ximg->bits_per_pixel)){
	case 24: out_format= IMGFMT_BGR24; break;
	case 32: out_format= IMGFMT_BGR32; break;
	case 15: out_format= IMGFMT_BGR15; break;
	case 16: out_format= IMGFMT_BGR16; break;
	default: break;
    }

    /* If we have blue in the lowest bit then obviously RGB */
    mode=( ( ximg->blue_mask & 0x01 ) != 0 ) ? MODE_RGB : MODE_BGR;
#ifdef WORDS_BIGENDIAN
    if ( ximg->byte_order != MSBFirst )
#else
    if ( ximg->byte_order != LSBFirst )
#endif
    {
	mode=( ( ximg->blue_mask & 0x01 ) != 0 ) ? MODE_BGR : MODE_RGB;
    }

#ifdef WORDS_BIGENDIAN
    if(mode==MODE_BGR && bpp!=32) {
	MSG_ERR("BGR%d not supported, please contact the developers\n", priv.bpp);
	return MPXP_False;
    }
    if(mode==MODE_RGB && bpp==32) {
	MSG_ERR("RGB32 not supported on big-endian systems, please contact the developers\n");
	return MPXP_False;
    }
#else
    if(mode==MODE_BGR) {
	MSG_ERR("BGR not supported, please contact the developers\n");
	return MPXP_False;
    }
#endif
#ifdef CONFIG_VIDIX
    }
#endif
#ifdef CONFIG_VIDIX
    if(vidix) {
	vo_rect_t winc;
	x11.get_win_coord(winc);
	if(vidix->configure(image_width,image_height,winc.x,winc.y,
			winc.w,winc.h,
			in_format,x11.depth(),
			x11.screen_width(),x11.screen_height()) != MPXP_Ok) {
	    MSG_ERR("vo_vesa: Can't initialize VIDIX driver\n");
	    return MPXP_False;
	} else MSG_V("vo_vesa: Using VIDIX\n");
	if(vidix->start()!=0) return MPXP_False;
	if (vidix->grkey_support()) {
	    vidix_grkey_t gr_key;
	    vidix->grkey_get(&gr_key);
	    gr_key.key_op = KEYS_PUT;
	    gr_key.ckey.op = CKEY_TRUE;
	    gr_key.ckey.red = 255;
	    gr_key.ckey.green = 0;
	    gr_key.ckey.blue = 255;
	    vidix->grkey_set(&gr_key);
	}
    }
#endif
    return MPXP_Ok;
}

void X11_VO_Interface::display_image(XImage *myximage ) const
{
    vo_rect_t r;
    x11.get_win_coord(r);
    r.x=r.y=0;
    r.w=(r.w-myximage->width)/2;
    r.h=(r.h-myximage->height)/2;
    x11.put_image(myximage,r);
}

MPXP_Rc X11_VO_Interface::select_frame( unsigned idx ){
#ifdef CONFIG_VIDIX
    if(vidix) return vidix->select_frame(idx);
#endif
    lock_surfaces();
    display_image(x11.Image(idx));
    if (num_buffers>1) x11.flush();
    else x11.sync(False);
    unlock_surfaces();
    return MPXP_Ok;
}

MPXP_Rc X11_VO_Interface::query_format(vo_query_fourcc_t* format) const
{
#ifdef CONFIG_VIDIX
    if(vidix) return vidix->query_fourcc(format);
#endif
    MSG_DBG2("vo_x11: query_format was called: %x (%s)\n",format->fourcc,vo_format_name(format->fourcc));
#ifdef WORDS_BIGENDIAN
    if (IMGFMT_IS_BGR(format->fourcc) && rgbfmt_depth(format->fourcc)<48)
#else
    if (IMGFMT_IS_RGB(format->fourcc) && rgbfmt_depth(format->fourcc)<48)
#endif
    {
	format->flags=VOCAP_SUPPORTED;
	return MPXP_Ok;
    }
// just for tests:
//if(format->fourcc==IMGFMT_YUY2) return 0x1|0x2|0x4;
    return MPXP_False;
}

void X11_VO_Interface::get_surface_caps(dri_surface_cap_t *caps) const
{
#ifdef CONFIG_VIDIX
    if(vidix) return vidix->get_surface_caps(caps);
#endif
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

void X11_VO_Interface::get_surface(dri_surface_t *surf)
{
#ifdef CONFIG_VIDIX
    if(vidix) return vidix->get_surface(surf);
#endif
    lock_surfaces();
    surf->planes[0] = x11.ImageData(surf->idx);
    surf->planes[1] = 0;
    surf->planes[2] = 0;
    surf->planes[3] = 0;
    unlock_surfaces();
}

MPXP_Rc X11_VO_Interface::flush_page(unsigned idx) {
#ifdef CONFIG_VIDIX
    if(vidix) return vidix->flush_page(idx);
#endif
    return MPXP_False;
}

unsigned X11_VO_Interface::get_num_frames() const {
#ifdef CONFIG_VIDIX
    if(vidix) return vidix->get_num_frames();
#endif
    return num_buffers;
}

MPXP_Rc X11_VO_Interface::toggle_fullscreen() {
    x11.fullscreen(flags);
#ifdef CONFIG_VIDIX
    if(vidix) resize_vidix();
#endif
    return MPXP_True;
}

MPXP_Rc X11_VO_Interface::ctrl(uint32_t request, any_t*data) {
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

static VO_Interface* query_interface(const char* args) { return new(zeromem) X11_VO_Interface(args); }
extern const vo_info_t x11_vo_info =
{
	"X11 ( XImage/Shm )"
#ifdef CONFIG_VIDIX
	" (with x11:vidix subdevice)"
#endif
	,
	"x11",
	"Aaron Holtzman <aholtzma@ess.engr.uvic.ca>",
	"",
	query_interface
};
