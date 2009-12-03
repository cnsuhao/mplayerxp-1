/*
 *
 *  rgb2rgb.h, Software RGB to RGB convertor
 *  pluralize by Software PAL8 to RGB convertor
 *               Software YUV to YUV convertor
 *               Software YUV to RGB convertor
 */

#ifndef RGB2RGB_INCLUDED
#define RGB2RGB_INCLUDED

// Note: do not fix the dependence on stdio.h

/* A full collection of rgb to rgb(bgr) convertors */
typedef void (* __FASTCALL__ rgb2rgb_f)(const uint8_t *src,uint8_t *dst,unsigned src_size);

extern rgb2rgb_f rgb24to32;
extern rgb2rgb_f rgb24to16;
extern rgb2rgb_f rgb24to15;
extern rgb2rgb_f rgb32to24;
extern rgb2rgb_f rgb32to16;
extern rgb2rgb_f rgb32to15;
extern rgb2rgb_f rgb15to16;
extern rgb2rgb_f rgb15to24;
extern rgb2rgb_f rgb15to32;
extern rgb2rgb_f rgb16to15;
extern rgb2rgb_f rgb16to24;
extern rgb2rgb_f rgb16to32;
extern rgb2rgb_f rgb24tobgr24;
extern rgb2rgb_f rgb24tobgr16;
extern rgb2rgb_f rgb24tobgr15;
extern rgb2rgb_f rgb32tobgr32;
extern rgb2rgb_f rgb32tobgr16;
extern rgb2rgb_f rgb32tobgr15;

extern void __FASTCALL__ rgb24tobgr32(const uint8_t *src, uint8_t *dst, unsigned src_size);
extern void __FASTCALL__ rgb32tobgr24(const uint8_t *src, uint8_t *dst, unsigned src_size);
extern void __FASTCALL__ rgb16tobgr32(const uint8_t *src, uint8_t *dst, unsigned src_size);
extern void __FASTCALL__ rgb16tobgr24(const uint8_t *src, uint8_t *dst, unsigned src_size);
extern void __FASTCALL__ rgb16tobgr16(const uint8_t *src, uint8_t *dst, unsigned src_size);
extern void __FASTCALL__ rgb16tobgr15(const uint8_t *src, uint8_t *dst, unsigned src_size);
extern void __FASTCALL__ rgb15tobgr32(const uint8_t *src, uint8_t *dst, unsigned src_size);
extern void __FASTCALL__ rgb15tobgr24(const uint8_t *src, uint8_t *dst, unsigned src_size);
extern void __FASTCALL__ rgb15tobgr16(const uint8_t *src, uint8_t *dst, unsigned src_size);
extern void __FASTCALL__ rgb15tobgr15(const uint8_t *src, uint8_t *dst, unsigned src_size);
extern void __FASTCALL__ rgb8tobgr8(const uint8_t *src, uint8_t *dst, unsigned src_size);


extern void __FASTCALL__ palette8torgb32(const uint8_t *src, uint8_t *dst, unsigned num_pixels, const uint8_t *palette);
extern void __FASTCALL__ palette8tobgr32(const uint8_t *src, uint8_t *dst, unsigned num_pixels, const uint8_t *palette);
extern void __FASTCALL__ palette8torgb24(const uint8_t *src, uint8_t *dst, unsigned num_pixels, const uint8_t *palette);
extern void __FASTCALL__ palette8tobgr24(const uint8_t *src, uint8_t *dst, unsigned num_pixels, const uint8_t *palette);
extern void __FASTCALL__ palette8torgb16(const uint8_t *src, uint8_t *dst, unsigned num_pixels, const uint8_t *palette);
extern void __FASTCALL__ palette8tobgr16(const uint8_t *src, uint8_t *dst, unsigned num_pixels, const uint8_t *palette);
extern void __FASTCALL__ palette8torgb15(const uint8_t *src, uint8_t *dst, unsigned num_pixels, const uint8_t *palette);
extern void __FASTCALL__ palette8tobgr15(const uint8_t *src, uint8_t *dst, unsigned num_pixels, const uint8_t *palette);

/**
 *
 * height should be a multiple of 2 and width should be a multiple of 16 (if this is a
 * problem for anyone then tell me, and ill fix it)
 * chrominance data is only taken from every secound line others are ignored FIXME write HQ version
 */
//void uyvytoyv12(const uint8_t *src, uint8_t *ydst, uint8_t *udst, uint8_t *vdst,

/**
 *
 * height should be a multiple of 2 and width should be a multiple of 16 (if this is a
 * problem for anyone then tell me, and ill fix it)
 */
 
typedef void (* __FASTCALL__ planar2packet_f)(const uint8_t *ysrc, const uint8_t *usrc,
	const uint8_t *vsrc, uint8_t *dst,
	unsigned int width, unsigned int height,
	int lumStride, int chromStride, int dstStride);
typedef void (* __FASTCALL__ packet2planar_f)(const uint8_t *src, uint8_t *ydst, uint8_t *udst, uint8_t *vdst,
	unsigned int width, unsigned int height,
	int lumStride, int chromStride, int srcStride);
/**
 *
 * width should be a multiple of 16
 */
extern planar2packet_f yv12toyuy2;
extern planar2packet_f yuv422ptoyuy2;

/**
 *
 * height should be a multiple of 2 and width should be a multiple of 16 (if this is a
 * problem for anyone then tell me, and ill fix it)
 */
extern packet2planar_f yuy2toyv12;

/**
 *
 * height should be a multiple of 2 and width should be a multiple of 16 (if this is a
 * problem for anyone then tell me, and ill fix it)
 */
extern planar2packet_f yv12touyvy;

/**
 *
 * height should be a multiple of 2 and width should be a multiple of 2 (if this is a
 * problem for anyone then tell me, and ill fix it)
 * chrominance data is only taken from every secound line others are ignored FIXME write HQ version
 */
extern packet2planar_f rgb24toyv12;
extern void (* __FASTCALL__ planar2x)(const uint8_t *src, uint8_t *dst, int width, int height, int srcStride, int dstStride);

extern void (* __FASTCALL__ interleaveBytes)(uint8_t *src1, uint8_t *src2, uint8_t *dst,
			    unsigned width, unsigned height, int src1Stride,
			    int src2Stride, int dstStride);

extern void (* __FASTCALL__ yvu9toyv12)(const uint8_t *ysrc, const uint8_t *usrc, const uint8_t *vsrc,
			    uint8_t *ydst, uint8_t *udst, uint8_t *vdst,
			    unsigned int width, unsigned int height, int lumStride, int chromStride);

extern void (* __FASTCALL__ vu9_to_vu12)(const uint8_t *src1, const uint8_t *src2,
			uint8_t *dst1, uint8_t *dst2,
			unsigned width, unsigned height,
			int srcStride1, int srcStride2,
			int dstStride1, int dstStride2);

extern void (* __FASTCALL__ yvu9_to_yuy2)(const uint8_t *src1, const uint8_t *src2, const uint8_t *src3,
			uint8_t *dst,
			unsigned width, unsigned height,
			int srcStride1, int srcStride2,
			int srcStride3, int dstStride);
	
void sws_rgb2rgb_init(int flags);

#define MODE_RGB  0x1
#define MODE_BGR  0x2

#endif
