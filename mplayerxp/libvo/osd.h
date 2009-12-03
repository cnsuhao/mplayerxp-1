#ifndef __MPLAYER_OSD_H
#define __MPLAYER_OSD_H

/* Generic alpha renderers for all YUV modes and RGB depths. */
/* These are "reference implementations", should be optimized later (MMX, etc) */

extern void vo_draw_alpha_init( void ); /* build tables */

typedef void (* __FASTCALL__ draw_alpha_f)(int w,int h, const unsigned char* src, const unsigned char *srca, int srcstride, unsigned char* dstbase,int dststride);

extern draw_alpha_f vo_draw_alpha_yv12_ptr;
extern draw_alpha_f vo_draw_alpha_yuy2_ptr;
extern draw_alpha_f vo_draw_alpha_uyvy_ptr;
extern draw_alpha_f vo_draw_alpha_rgb24_ptr;
extern draw_alpha_f vo_draw_alpha_rgb32_ptr;
extern draw_alpha_f vo_draw_alpha_rgb15_ptr;
extern draw_alpha_f vo_draw_alpha_rgb16_ptr;
#define vo_draw_alpha_yv12(w,h,src,srca,srcstride,dstbase,dstrstride) (*vo_draw_alpha_yv12_ptr)(w,h,src,srca,srcstride,dstbase,dstrstride)
#define vo_draw_alpha_yuy2(w,h,src,srca,srcstride,dstbase,dstrstride) (*vo_draw_alpha_yuy2_ptr)(w,h,src,srca,srcstride,dstbase,dstrstride)
#define vo_draw_alpha_uyvy(w,h,src,srca,srcstride,dstbase,dstrstride) (*vo_draw_alpha_uyvy_ptr)(w,h,src,srca,srcstride,dstbase,dstrstride)
#define vo_draw_alpha_rgb24(w,h,src,srca,srcstride,dstbase,dstrstride) (*vo_draw_alpha_rgb24_ptr)(w,h,src,srca,srcstride,dstbase,dstrstride)
#define vo_draw_alpha_rgb32(w,h,src,srca,srcstride,dstbase,dstrstride) (*vo_draw_alpha_rgb32_ptr)(w,h,src,srca,srcstride,dstbase,dstrstride)
#define vo_draw_alpha_rgb15(w,h,src,srca,srcstride,dstbase,dstrstride) (*vo_draw_alpha_rgb15_ptr)(w,h,src,srca,srcstride,dstbase,dstrstride)
#define vo_draw_alpha_rgb16(w,h,src,srca,srcstride,dstbase,dstrstride) (*vo_draw_alpha_rgb16_ptr)(w,h,src,srca,srcstride,dstbase,dstrstride)
#endif
