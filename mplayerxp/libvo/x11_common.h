
#ifndef X11_COMMON_H
#define X11_COMMON_H

#ifdef HAVE_X11

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "dri_vo.h"

extern XImage *vo_x11_myximage[MAX_DRI_BUFFERS];
extern int vo_x11_Shmem_Flag;

int vo_x11_init( void );
/* local data */
#define vo_x11_ImageData(idx) ( uint8_t * ) vo_x11_myximage[idx]->data
void __FASTCALL__ vo_x11_getMyXImage(unsigned idx,Visual *visual,unsigned depth,unsigned w,unsigned h);
void __FASTCALL__ vo_x11_freeMyXImage(unsigned idx);
void __FASTCALL__ vo_x11_hidecursor ( Display* , Window );
void __FASTCALL__ vo_x11_decoration( Display * vo_Display,Window w,int d );
void __FASTCALL__ vo_x11_classhint( Display * display,Window window,char *name );
int __FASTCALL__ vo_x11_uninit(Display *display, Window window);
void __FASTCALL__ vo_x11_sizehint( int x, int y, int width, int height );
void __FASTCALL__ vo_x11_calcpos( XSizeHints* hint, unsigned d_width, unsigned d_height, unsigned flags );
uint32_t __FASTCALL__ vo_x11_check_events(Display *mydisplay,int (* __FASTCALL__ adjust_size)(unsigned cw,unsigned ch,unsigned *nw,unsigned *nh));
void vo_x11_fullscreen( void );
#endif

void __FASTCALL__ saver_off( Display * );
void __FASTCALL__ saver_on( Display * );

#ifdef HAVE_XINERAMA
void __FASTCALL__ vo_x11_xinerama_move(Display *dsp, Window w,const XSizeHints*hint);
#endif

#ifdef HAVE_XF86VM
void __FASTCALL__ vo_vm_switch(uint32_t, uint32_t, int*, int*);
void __FASTCALL__ vo_vm_close(Display*);
#endif

#endif
