#ifndef X11_COMMON_H
#define X11_COMMON_H

#ifdef HAVE_X11

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "video_out.h"
#include "dri_vo.h"

int __FASTCALL__ vo_x11_init(vo_data_t*vo);
int __FASTCALL__ vo_x11_uninit(vo_data_t*vo,Display *display, Window window);
/* local data */
int __FASTCALL__ vo_x11_Shmem_Flag(vo_data_t*vo);
XImage* __FASTCALL__ vo_x11_Image(vo_data_t*vo,unsigned idx);
uint8_t* __FASTCALL__ vo_x11_ImageData(vo_data_t*vo,unsigned idx);
void __FASTCALL__ vo_x11_getMyXImage(vo_data_t*vo,unsigned idx,Visual *visual,unsigned depth,unsigned w,unsigned h);
void __FASTCALL__ vo_x11_freeMyXImage(vo_data_t*vo,unsigned idx);
void __FASTCALL__ vo_x11_hidecursor ( Display* , Window );
void __FASTCALL__ vo_x11_decoration(vo_data_t*vo,Display * vo_Display,Window w,int d );
void __FASTCALL__ vo_x11_classhint( Display * display,Window window,char *name );
void __FASTCALL__ vo_x11_sizehint(vo_data_t*vo,int x, int y, int width, int height );
void __FASTCALL__ vo_x11_calcpos(vo_data_t*vo,XSizeHints* hint, unsigned d_width, unsigned d_height, unsigned flags );
uint32_t __FASTCALL__ vo_x11_check_events(vo_data_t*vo,Display *mydisplay,int (* __FASTCALL__ adjust_size)(unsigned cw,unsigned ch,unsigned *nw,unsigned *nh));
void __FASTCALL__ vo_x11_fullscreen(vo_data_t*vo);
#endif

void __FASTCALL__ saver_off(vo_data_t*vo, Display *);
void __FASTCALL__ saver_on(vo_data_t*,Display *);

#ifdef HAVE_XINERAMA
void __FASTCALL__ vo_x11_xinerama_move(vo_data_t*vo,Display *dsp, Window w,const XSizeHints*hint);
#endif

#ifdef HAVE_XF86VM
void __FASTCALL__ vo_vm_switch(vo_data_t*vo,uint32_t, uint32_t, int*, int*);
void __FASTCALL__ vo_vm_close(vo_data_t*vo,Display*);
#endif

#endif
