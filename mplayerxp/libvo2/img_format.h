
#ifndef __IMG_FORMAT_H
#define __IMG_FORMAT_H
#include <inttypes.h>
#include <stdint.h>
#include "mp_config.h"
#include "mp_conf_lavc.h"

/* RGB/BGR Formats */
enum {
    IMGFMT_RGB_MASK	=0xFFFFFF00,
    IMGFMT_RGB		=(('R'<<24)|('G'<<16)|('B'<<8)),
    IMGFMT_RGB1		=(IMGFMT_RGB|1),
    IMGFMT_RGB4		=(IMGFMT_RGB|4),
    IMGFMT_RGB4_CHAR	=(IMGFMT_RGB|4|128), // RGB4 with 1 pixel per byte
    IMGFMT_RGB8		=(IMGFMT_RGB|8),
    IMGFMT_RGB15	=(IMGFMT_RGB|15),
    IMGFMT_RGB16	=(IMGFMT_RGB|16),
    IMGFMT_RGB24	=(IMGFMT_RGB|24),
    IMGFMT_RGB32	=(IMGFMT_RGB|32),
    IMGFMT_RGB48LE	=(IMGFMT_RGB|48),
    IMGFMT_RGB48BE	=(IMGFMT_RGB|48|128),

    IMGFMT_BGR_MASK	=0xFFFFFF00,
    IMGFMT_BGR		=(('B'<<24)|('G'<<16)|('R'<<8)),
    IMGFMT_BGR1		=(IMGFMT_BGR|1),
    IMGFMT_BGR4		=(IMGFMT_BGR|4),
    IMGFMT_BGR4_CHAR	=(IMGFMT_BGR|4|128), // BGR4 with 1 pixel per byte
    IMGFMT_BGR8		=(IMGFMT_BGR|8),
    IMGFMT_BGR15	=(IMGFMT_BGR|15),
    IMGFMT_BGR16	=(IMGFMT_BGR|16),
    IMGFMT_BGR24	=(IMGFMT_BGR|24),
    IMGFMT_BGR32	=(IMGFMT_BGR|32),
    IMGFMT_BGR48LE	=(IMGFMT_BGR|48),
    IMGFMT_BGR48BE	=(IMGFMT_BGR|48|128),

#ifdef WORDS_BIGENDIAN
    IMGFMT_ABGR		=IMGFMT_RGB32,
    IMGFMT_BGRA		=(IMGFMT_RGB32|64),
    IMGFMT_ARGB		=IMGFMT_BGR32,
    IMGFMT_RGBA		=(IMGFMT_BGR32|64),
    IMGFMT_RGB48NE	=IMGFMT_RGB48BE,
    IMGFMT_BGR48NE	=IMGFMT_RGB48LE,
#else
    IMGFMT_ABGR		=(IMGFMT_BGR32|64),
    IMGFMT_BGRA		=IMGFMT_BGR32,
    IMGFMT_ARGB		=(IMGFMT_RGB32|64),
    IMGFMT_RGBA		=IMGFMT_RGB32,
    IMGFMT_RGB48NE	=IMGFMT_RGB48LE,
    IMGFMT_BGR48NE	=IMGFMT_RGB48BE,
#endif
/* old names for compatibility */
    IMGFMT_RG4B		=IMGFMT_RGB4_CHAR,
    IMGFMT_BG4B		=IMGFMT_BGR4_CHAR,
/* Planar YUV Formats */
    IMGFMT_YVU9		=0x39555659,
    IMGFMT_IF09		=0x39304649,
    IMGFMT_YV12		=0x32315659,
    IMGFMT_I420		=0x30323449,
    IMGFMT_IYUV		=0x56555949,
    IMGFMT_CLPL		=0x4C504C43,
    IMGFMT_Y800		=0x30303859,
    IMGFMT_Y8		=0x20203859,
    IMGFMT_NV12		=0x3231564E,
    IMGFMT_NV21		=0x3132564E,
/* unofficial Planar Formats, FIXME if official 4CC exists */
    IMGFMT_444P		=0x50343434,
    IMGFMT_422P		=0x50323234,
    IMGFMT_411P		=0x50313134,
    IMGFMT_HM12		=0x32314D48,
/* Packed YUV Formats */
    IMGFMT_IUYV		=0x56595549,
    IMGFMT_IY41		=0x31435949,
    IMGFMT_IYU1		=0x31555949,
    IMGFMT_IYU2		=0x32555949,
    IMGFMT_UYVY		=0x59565955,
    IMGFMT_UYNV		=0x564E5955,
    IMGFMT_cyuv		=0x76757963,
    IMGFMT_Y422		=0x32323459,
    IMGFMT_YUY2		=0x32595559,
    IMGFMT_YUNV		=0x564E5559,
    IMGFMT_YVYU		=0x55595659,
    IMGFMT_Y41P		=0x50313459,
    IMGFMT_Y211		=0x31313259,
    IMGFMT_Y41T		=0x54313459,
    IMGFMT_Y42T		=0x54323459,
    IMGFMT_V422		=0x32323456,
    IMGFMT_V655		=0x35353656,
    IMGFMT_CLJR		=0x524A4C43,
    IMGFMT_YUVP		=0x50565559,
    IMGFMT_UYVP		=0x50565955,
// 4:2:0 planar with alpha
    IMGFMT_420A		=0x41303234,

    IMGFMT_444P16_LE	=0x51343434,
    IMGFMT_444P16_BE	=0x34343451,
    IMGFMT_422P16_LE	=0x51323234,
    IMGFMT_422P16_BE	=0x34323251,
    IMGFMT_420P16_LE	=0x51303234,
    IMGFMT_420P16_BE	=0x34323051,
#if HAVE_BIGENDIAN
    IMGFMT_444P16	=IMGFMT_444P16_BE,
    IMGFMT_422P16	=IMGFMT_422P16_BE,
    IMGFMT_420P16	=IMGFMT_420P16_BE,
#else
    IMGFMT_444P16	=IMGFMT_444P16_LE,
    IMGFMT_422P16	=IMGFMT_422P16_LE,
    IMGFMT_420P16	=IMGFMT_420P16_LE,
#endif
/* Compressed Formats */
    IMGFMT_MPEGPES	=(('M'<<24)|('P'<<16)|('E'<<8)|('S')),
/* Formats that are understood by zoran chips, we include
 * non-interlaced, interlaced top-first, interlaced bottom-first */
    IMGFMT_ZRMJPEGNI	=(('Z'<<24)|('R'<<16)|('N'<<8)|('I')),
    IMGFMT_ZRMJPEGIT	=(('Z'<<24)|('R'<<16)|('I'<<8)|('T')),
    IMGFMT_ZRMJPEGIB	=(('Z'<<24)|('R'<<16)|('I'<<8)|('B')),
// I think that this code could not be used by any other codec/format
    IMGFMT_XVMC		=0x1DC70000,
    IMGFMT_XVMC_MASK	=0xFFFF0000,
//these are chroma420
    IMGFMT_XVMC_MOCO_MPEG2=(IMGFMT_XVMC|0x02),
    IMGFMT_XVMC_IDCT_MPEG2=(IMGFMT_XVMC|0x82),
// VDPAU specific format.
    IMGFMT_VDPAU	=0x1DC80000,
    IMGFMT_VDPAU_MASK	=0xFFFF0000,
    IMGFMT_VDPAU_MPEG1	=(IMGFMT_VDPAU|0x01),
    IMGFMT_VDPAU_MPEG2	=(IMGFMT_VDPAU|0x02),
    IMGFMT_VDPAU_H264	=(IMGFMT_VDPAU|0x03),
    IMGFMT_VDPAU_WMV3	=(IMGFMT_VDPAU|0x04),
    IMGFMT_VDPAU_VC1	=(IMGFMT_VDPAU|0x05),
    IMGFMT_VDPAU_MPEG4	=(IMGFMT_VDPAU|0x06)
};

static inline int IMGFMT_IS_RGB(uint32_t fmt) { return (fmt&IMGFMT_RGB_MASK)==IMGFMT_RGB; }
static inline int IMGFMT_IS_BGR(uint32_t fmt) { return (fmt&IMGFMT_BGR_MASK)==IMGFMT_BGR; }
static inline int IMGFMT_RGB_DEPTH(uint32_t fmt) { return (fmt&0x3F); }
static inline int IMGFMT_BGR_DEPTH(uint32_t fmt) { return (fmt&0x3F); }
static inline int IMGFMT_IS_YUVP16_LE(uint32_t fmt) { return ((fmt^IMGFMT_420P16_LE) & 0xff0000ff) == 0; }
static inline int IMGFMT_IS_YUVP16_BE(uint32_t fmt) { return ((fmt^IMGFMT_420P16_BE) & 0xff0000ff) == 0; }
static inline int IMGFMT_IS_YUVP16_NE(uint32_t fmt) { return ((fmt^IMGFMT_420P16   ) & 0xff0000ff) == 0; }
static inline int IMGFMT_IS_YUVP16(uint32_t fmt)    { return (IMGFMT_IS_YUVP16_LE(fmt)||IMGFMT_IS_YUVP16_BE(fmt)); }
static inline int IMGFMT_IS_XVMC(uint32_t fmt)  { return ((fmt)&IMGFMT_XVMC_MASK)==IMGFMT_XVMC; }
static inline int IMGFMT_IS_VDPAU(uint32_t fmt) { return ((fmt)&IMGFMT_VDPAU_MASK)==IMGFMT_VDPAU; }

struct vo_mpegpes_t {
    any_t* data;
    int size;
    int id;        // stream id. usually 0x1E0
    int timestamp; // pts, 90000 Hz counter based
};

/** Returns human-readable fourcc description
 * @param format	fourcc of image
 * @return		string of format name
**/
const char * __FASTCALL__ vo_format_name(int format);
enum PixelFormat pixfmt_from_fourcc(uint32_t fourcc);
uint32_t	fourcc_from_pixfmt(enum PixelFormat pixfmt);
extern unsigned rgbfmt_depth(unsigned fmt);

#endif
