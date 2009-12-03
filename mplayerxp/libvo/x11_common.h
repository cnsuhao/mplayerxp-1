
#ifndef X11_COMMON_H
#define X11_COMMON_H

#ifdef X11_FULLSCREEN

#include <X11/Xlib.h>
#include <X11/Xutil.h>

extern int vo_fs;

extern char *mDisplayName;
extern Display *mDisplay;
extern Window mRootWin;
extern int mScreen;
extern int mLocalDisplay;
extern int WinID;

int vo_x11_init( void );
void __FASTCALL__ vo_hidecursor ( Display* , Window );
void __FASTCALL__ vo_x11_decoration( Display * vo_Display,Window w,int d );
void __FASTCALL__ vo_x11_classhint( Display * display,Window window,char *name );
int __FASTCALL__ vo_x11_uninit(Display *display, Window window);
void __FASTCALL__ vo_x11_sizehint( int x, int y, int width, int height );
void __FASTCALL__ vo_x11_calcpos( XSizeHints* hint, unsigned d_width, unsigned d_height, unsigned flags );
uint32_t __FASTCALL__ vo_x11_check_events(Display *mydisplay,int (* __FASTCALL__ adjust_size)(unsigned cw,unsigned ch,unsigned *nw,unsigned *nh));
void vo_x11_fullscreen( void );
#endif

extern Window     vo_window;
extern GC         vo_gc;
extern XSizeHints vo_hint;

void __FASTCALL__ saver_off( Display * );
void __FASTCALL__ saver_on( Display * );

#ifdef HAVE_XINERAMA
void __FASTCALL__ vo_x11_xinerama_move(Display *dsp, Window w);
#endif

#ifdef HAVE_XF86VM
void __FASTCALL__ vo_vm_switch(uint32_t, uint32_t, int*, int*);
void __FASTCALL__ vo_vm_close(Display*);
#endif

#endif
