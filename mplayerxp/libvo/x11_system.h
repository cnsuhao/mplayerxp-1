#ifndef X11_COMMON_H
#define X11_COMMON_H

#ifdef HAVE_X11

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#ifdef HAVE_SHM
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#endif
#ifdef HAVE_XINERAMA
#include <X11/extensions/Xinerama.h>
#endif
#ifdef HAVE_XF86VM
#include <X11/extensions/xf86vmode.h>
#endif
#ifdef HAVE_XDPMS
#include <X11/extensions/dpms.h>
#endif
#include "video_out.h"
#include "dri_vo.h"

typedef struct {
    long flags;
    long functions;
    long decorations;
    long input_mode;
    long state;
}MotifWmHints_t;

class X11_System {
    public:
	X11_System(const char* DisplayName);
	virtual ~X11_System();

	void		match_visual(XVisualInfo*) const;
	virtual XVisualInfo* get_visual() const;
	virtual void	create_window(const XSizeHints& hint,XVisualInfo* visual,int is_vm,unsigned depth,const char*title);
	void		get_win_coord(vo_rect_t&) const;
	void		select_input(long mask) const;
	unsigned	depth() const { return _depth; }
	void		flush() const;
	void		sync(int method) const;

	int		Shmem_Flag() const;
	::XImage*	Image(unsigned idx) const;
	virtual uint8_t*ImageData(unsigned idx) const;
	virtual void	getMyXImage(unsigned idx,Visual *visual,unsigned depth,unsigned w,unsigned h);
	virtual void	freeMyXImage(unsigned idx);
	void		put_image(XImage*,const vo_rect_t&) const;

	void		hidecursor () const;
	void		decoration(int d);
	void		classhint(const char *name) const;
	void		sizehint(int x, int y, int width, int height) const;
	void		calcpos(XSizeHints* hint, unsigned d_width, unsigned d_height, unsigned flags);
	uint32_t	check_events(vo_adjust_size_t adjust_size,vo_data_t* opaque);
	int		fullscreen(unsigned& flags);

	void		saver_off();
	void		saver_on();
#ifdef HAVE_XINERAMA
	void		xinerama_move(const XSizeHints*hint) const;
#endif
#ifdef HAVE_XF86VM
	void		vm_switch(uint32_t, uint32_t, int*, int*);
	void		vm_close();
#endif
    protected:
	::Display*	get_display() const { return mDisplay; }
	int		get_screen() const { return mScreen; }
	::GC		get_gc() const {return gc; }
	void		update_win_coord();

	::Window	window;
	vo_rect_t	prev,curr;
#ifdef HAVE_SHM
	::XShmSegmentInfo Shminfo[MAX_DRI_BUFFERS];
	int		gXErrorFlag;
	int		CompletionType;
#endif
    private:
	int		find_depth_from_visuals(Visual **visual_return) const;

	::Display*	mDisplay;
	int		mScreen,mLocalDisplay;
	::Window	mRootWin;
	::GC		gc;

	unsigned	_depth;

	int		dpms_disabled;
	int		timeout_save;

	int		_Shmem_Flag;
	::XImage*	myximage[MAX_DRI_BUFFERS];
#ifdef HAVE_XINERAMA
	int		xinerama_x;
	int		xinerama_y;
#endif
#ifdef HAVE_XF86VM
	::XF86VidModeModeInfo**	vidmodes;
	::XF86VidModeModeLine	modeline;
#endif
	MotifWmHints_t	MotifWmHints;
	Atom		MotifHints;
};

#ifdef HAVE_XV
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
class Xv_System : public X11_System {
    public:
	Xv_System(const char* DisplayName);
	virtual ~Xv_System();

	unsigned	query_port(uint32_t fourcc);

	::XvImage*	ImageXv(unsigned idx) const;
	virtual uint8_t*ImageData(unsigned idx) const;
	virtual void	getMyXImage(unsigned idx,Visual *visual,unsigned format,unsigned w,unsigned h);
	virtual void	freeMyXImage(unsigned idx);
	void		put_image(XvImage*,const vo_rect_t&) const;

	int		get_video_eq(vo_videq_t *info) const;
	int		set_video_eq(const vo_videq_t *info) const;
	int		reset_video_eq() const;
    private:
	XvPortID		port;
	unsigned		adaptors;
	XvAdaptorInfo*		ai;
	XvImageFormatValues*	fo;
	::XvImage*		myxvimage[MAX_DRI_BUFFERS];
};
#endif

#ifdef HAVE_OPENGL
#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glut.h>
class GLX_System : public X11_System {
    public:
	GLX_System(const char* DisplayName);
	virtual ~GLX_System();

	virtual void	create_window(const XSizeHints& hint,XVisualInfo* visual,int is_vm,unsigned depth,const char*title);

	void	swap_buffers() const;
	virtual XVisualInfo* get_visual() const { return vis; }
    private:
	GLXContext	ctx;
	XVisualInfo*	vis;
};
#endif
#endif
#endif