#include <stdio.h>
#include "img_format.h"
#include "libavutil/avutil.h"

const char * __FASTCALL__ vo_format_name(int format)
{
    static char unknow_format[20];
    switch(format)
    {
	case IMGFMT_RGB1: return("RGB 1-bit");
	case IMGFMT_RGB4: return("RGB 4-bit");
	case IMGFMT_RG4B: return("RGB 4-bit per byte");
	case IMGFMT_RGB8: return("RGB 8-bit");
	case IMGFMT_RGB15: return("RGB 15-bit");
	case IMGFMT_RGB16: return("RGB 16-bit");
	case IMGFMT_RGB24: return("RGB 24-bit");
	case IMGFMT_RGB48NE: return("RGB 48-bit");
//	case IMGFMT_RGB32: return("RGB 32-bit");
	case IMGFMT_BGR1: return("BGR 1-bit");
	case IMGFMT_BGR4: return("BGR 4-bit");
	case IMGFMT_BG4B: return("BGR 4-bit per byte");
	case IMGFMT_BGR8: return("BGR 8-bit");
	case IMGFMT_BGR15: return("BGR 15-bit");
	case IMGFMT_BGR16: return("BGR 16-bit");
	case IMGFMT_BGR24: return("BGR 24-bit");
	case IMGFMT_BGR48NE: return("BGR 48-bit");
//	case IMGFMT_BGR32: return("BGR 32-bit");
	case IMGFMT_ABGR: return("ABGR");
	case IMGFMT_BGRA: return("BGRA");
	case IMGFMT_ARGB: return("ARGB");
	case IMGFMT_RGBA: return("RGBA");
	case IMGFMT_YVU9: return("Planar YVU9");
	case IMGFMT_IF09: return("Planar IF09");
	case IMGFMT_YV12: return("Planar YV12");
	case IMGFMT_I420: return("Planar I420");
	case IMGFMT_IYUV: return("Planar IYUV");
	case IMGFMT_CLPL: return("Planar CLPL");
	case IMGFMT_Y800: return("Planar Y800");
	case IMGFMT_Y8: return("Planar Y8");
	case IMGFMT_444P: return("Planar 444P");
	case IMGFMT_422P: return("Planar 422P");
	case IMGFMT_411P: return("Planar 411P");
	case IMGFMT_NV12: return("Planar NV12");
	case IMGFMT_NV21: return("Planar NV21");
        case IMGFMT_HM12: return("Planar NV12 Macroblock");
	case IMGFMT_IUYV: return("Packed IUYV");
	case IMGFMT_IY41: return("Packed IY41");
	case IMGFMT_IYU1: return("Packed IYU1");
	case IMGFMT_IYU2: return("Packed IYU2");
	case IMGFMT_UYVY: return("Packed UYVY");
	case IMGFMT_UYNV: return("Packed UYNV");
	case IMGFMT_cyuv: return("Packed CYUV");
	case IMGFMT_Y422: return("Packed Y422");
	case IMGFMT_YUY2: return("Packed YUY2");
	case IMGFMT_YUNV: return("Packed YUNV");
	case IMGFMT_YVYU: return("Packed YVYU");
	case IMGFMT_Y41P: return("Packed Y41P");
	case IMGFMT_Y211: return("Packed Y211");
	case IMGFMT_Y41T: return("Packed Y41T");
	case IMGFMT_Y42T: return("Packed Y42T");
	case IMGFMT_V422: return("Packed V422");
	case IMGFMT_V655: return("Packed V655");
	case IMGFMT_CLJR: return("Packed CLJR");
	case IMGFMT_YUVP: return("Packed YUVP");
	case IMGFMT_UYVP: return("Packed UYVP");
	case IMGFMT_420A: return "Planar YV12 with alpha";
	case IMGFMT_420P16_LE: return "Planar 420P 16-bit little-endian";
	case IMGFMT_420P16_BE: return "Planar 420P 16-bit big-endian";
	case IMGFMT_422P16_LE: return "Planar 422P 16-bit little-endian";
	case IMGFMT_422P16_BE: return "Planar 422P 16-bit big-endian";
	case IMGFMT_444P16_LE: return "Planar 444P 16-bit little-endian";
	case IMGFMT_444P16_BE: return "Planar 444P 16-bit big-endian";
	case IMGFMT_MPEGPES: return("Mpeg PES");
	case IMGFMT_ZRMJPEGNI: return("Zoran MJPEG non-interlaced");
	case IMGFMT_ZRMJPEGIT: return("Zoran MJPEG top field first");
	case IMGFMT_ZRMJPEGIB: return("Zoran MJPEG bottom field first");
	case IMGFMT_XVMC_MOCO_MPEG2: return("MPEG1/2 Motion Compensation");
	case IMGFMT_XVMC_IDCT_MPEG2: return("MPEG1/2 Motion Compensation and IDCT");
    }
    snprintf(unknow_format,20,"Unknown 0x%04x",format);
    return unknow_format;
}

typedef struct s_pix_fourcc{
    int pix_fmt;
    uint32_t fourcc;
}pix_fourcc;
static pix_fourcc pfcc[] =
{
    { PIX_FMT_YUV420P, IMGFMT_YV12 },  ///< Planar YUV 4:2:0, 12bpp, (1 Cr & Cb sample per 2x2 Y samples)
    { PIX_FMT_YUYV422, IMGFMT_YUY2 },  ///< Packed YUV 4:2:2, 16bpp, Y0 Cb Y1 Cr
    { PIX_FMT_RGB24,   IMGFMT_RGB24},  ///< Packed RGB 8:8:8, 24bpp, RGBRGB...
    { PIX_FMT_BGR24,   IMGFMT_BGR24},  ///< Packed RGB 8:8:8, 24bpp, BGRBGR...
    { PIX_FMT_YUV422P, IMGFMT_422P },  ///< Planar YUV 4:2:2, 16bpp, (1 Cr & Cb sample per 2x1 Y samples)
    { PIX_FMT_YUV444P, IMGFMT_444P },  ///< Planar YUV 4:4:4, 24bpp, (1 Cr & Cb sample per 1x1 Y samples)
    { PIX_FMT_RGB32,   IMGFMT_RGB32},  ///< Packed RGB 8:8:8, 32bpp, (msb)8A 8R 8G 8B(lsb), in cpu endianness
    { PIX_FMT_YUV410P, IMGFMT_YVU9 },  ///< Planar YUV 4:1:0,  9bpp, (1 Cr & Cb sample per 4x4 Y samples)
    { PIX_FMT_YUV410P, IMGFMT_IF09 },  ///< rough alias
    { PIX_FMT_YUV411P, IMGFMT_411P },  ///< Planar YUV 4:1:1, 12bpp, (1 Cr & Cb sample per 4x1 Y samples)
    { PIX_FMT_RGB48BE, IMGFMT_RGB48BE },  ///< packed RGB 16:16:16, 48bpp, 16R, 16G, 16B, big-endian
    { PIX_FMT_RGB48LE, IMGFMT_RGB48LE },  ///< packed RGB 16:16:16, 48bpp, 16R, 16G, 16B, little-endian
    { PIX_FMT_RGB565,  IMGFMT_RGB16},  ///< Packed RGB 5:6:5, 16bpp, (msb)   5R 6G 5B(lsb), in cpu endianness
    { PIX_FMT_RGB555,  IMGFMT_RGB15},  ///< Packed RGB 5:5:5, 16bpp, (msb)1A 5R 5G 5B(lsb), in cpu endianness most significant bit to 1
    { PIX_FMT_GRAY8,   IMGFMT_Y800 },  ///<        Y        ,  8bpp
    { PIX_FMT_GRAY8,   IMGFMT_Y8   },  ///< alias
    { PIX_FMT_MONOWHITE, IMGFMT_RGB1}, ///<        Y        ,  1bpp, 1 is white
    { PIX_FMT_MONOBLACK, IMGFMT_RGB1}, ///<        Y        ,  1bpp, 0 is black
    { PIX_FMT_PAL8,    IMGFMT_RGB8 },  ///< 8 bit with PIX_FMT_RGB32 palette
    { PIX_FMT_YUVJ420P,IMGFMT_YV12 },  ///< Planar YUV 4:2:0, 12bpp, full scale (jpeg)
    { PIX_FMT_YUVJ420P,IMGFMT_I420 },  ///< alias
    { PIX_FMT_YUVJ422P,IMGFMT_422P },  ///< Planar YUV 4:2:2, 16bpp, full scale (jpeg)
    { PIX_FMT_YUVJ444P,IMGFMT_444P },  ///< Planar YUV 4:4:4, 24bpp, full scale (jpeg)
    { PIX_FMT_XVMC_MPEG2_MC, IMGFMT_XVMC},///< XVideo Motion Acceleration via common packet passing(xvmc_render.h)
    { PIX_FMT_XVMC_MPEG2_IDCT, IMGFMT_XVMC_IDCT_MPEG2},
    { PIX_FMT_UYVY422, IMGFMT_UYVY },  ///< Packed YUV 4:2:2, 16bpp, Cb Y0 Cr Y1
    { PIX_FMT_UYYVYY411, IMGFMT_Y41P },///< Packed YUV 4:1:1, 12bpp, Cb Y0 Y1 Cr Y2 Y3
    { PIX_FMT_BGR32,   IMGFMT_BGR32 }, ///< Packed RGB 8:8:8, 32bpp, (msb)8A 8B 8G 8R(lsb), in cpu endianness
    { PIX_FMT_BGR565,  IMGFMT_BGR16 }, ///< Packed RGB 5:6:5, 16bpp, (msb)   5B 6G 5R(lsb), in cpu endianness
    { PIX_FMT_BGR555,  IMGFMT_BGR15 }, ///< Packed RGB 5:5:5, 16bpp, (msb)1A 5B 5G 5R(lsb), in cpu endianness most significant bit to 1
    { PIX_FMT_BGR8,    IMGFMT_BGR8  }, ///< Packed RGB 3:3:2,  8bpp, (msb)2B 3G 3R(lsb)
    { PIX_FMT_BGR4,    IMGFMT_BGR4  }, ///< Packed RGB 1:2:1,  4bpp, (msb)1B 2G 1R(lsb)
    { PIX_FMT_BGR4_BYTE,IMGFMT_BGR4_CHAR}, ///< Packed RGB 1:2:1,  8bpp, (msb)1B 2G 1R(lsb)
    { PIX_FMT_RGB8,    IMGFMT_RGB8  }, ///< Packed RGB 3:3:2,  8bpp, (msb)2R 3G 3B(lsb)
    { PIX_FMT_RGB4,    IMGFMT_RGB4  },  ///< Packed RGB 1:2:1,  4bpp, (msb)2R 3G 3B(lsb)
    { PIX_FMT_RGB4_BYTE,IMGFMT_RGB4_CHAR }, ///< Packed RGB 1:2:1,  8bpp, (msb)2R 3G 3B(lsb)
    { PIX_FMT_NV12,    IMGFMT_NV12 },   ///< Planar YUV 4:2:0, 12bpp, 1 plane for Y and 1 for UV
    { PIX_FMT_NV21,    IMGFMT_NV21 },  ///< as above, but U and V bytes are swapped

    { PIX_FMT_YUVA420P,  IMGFMT_420A },///< planar YUV 4:2:0, 20bpp, (1 Cr & Cb sample per 2x2 Y & A samples)
    { PIX_FMT_YUV420P16LE, IMGFMT_420P16_LE }, ///< planar YUV 4:2:0, 24bpp, (1 Cr & Cb sample per 2x2 Y samples), little-endian
    { PIX_FMT_YUV420P16BE, IMGFMT_420P16_BE },///< planar YUV 4:2:0, 24bpp, (1 Cr & Cb sample per 2x2 Y samples), big-endian
    { PIX_FMT_YUV422P16LE, IMGFMT_422P16_LE },///< planar YUV 4:2:2, 32bpp, (1 Cr & Cb sample per 2x1 Y samples), little-endian
    { PIX_FMT_YUV422P16BE, IMGFMT_422P16_BE },///< planar YUV 4:2:2, 32bpp, (1 Cr & Cb sample per 2x1 Y samples), big-endian
    { PIX_FMT_YUV444P16LE, IMGFMT_444P16_LE },///< planar YUV 4:4:4, 48bpp, (1 Cr & Cb sample per 1x1 Y samples), little-endian
    { PIX_FMT_YUV444P16BE, IMGFMT_444P16_BE },///< planar YUV 4:4:4, 48bpp, (1 Cr & Cb sample per 1x1 Y samples), big-endian

    { PIX_FMT_RGB32_1, IMGFMT_RGBA },  ///< Packed RGB 8:8:8, 32bpp, (msb)8R 8G 8B 8A(lsb), in cpu endianness
    { PIX_FMT_BGR32_1, IMGFMT_BGRA } ///< Packed RGB 8:8:8, 32bpp, (msb)8B 8G 8R 8A(lsb), in cpu endianness
};

int		pixfmt_from_fourcc(uint32_t fourcc)
{
    unsigned i;
    for(i=0;i<sizeof(pfcc)/sizeof(pix_fourcc);i++)
    {
	if(fourcc==pfcc[i].fourcc) return pfcc[i].pix_fmt;
    }
    return -1;
}

uint32_t	fourcc_from_pixfmt(int pixfmt)
{
    unsigned i;
    for(i=0;i<sizeof(pfcc)/sizeof(pix_fourcc);i++)
    {
	if(pixfmt==pfcc[i].pix_fmt) return pfcc[i].fourcc;
    }
    return -1;
}

unsigned rgbfmt_depth(unsigned fmt)
{
    switch(pixfmt_from_fourcc(fmt)) {
	case PIX_FMT_RGB48BE:
	case PIX_FMT_RGB48LE:
//	case PIX_FMT_BGR48BE:
//	case PIX_FMT_BGR48LE:
	    return 48;
        case PIX_FMT_BGRA:
        case PIX_FMT_ABGR:
        case PIX_FMT_RGBA:
        case PIX_FMT_ARGB:
            return 32;
        case PIX_FMT_BGR24:
        case PIX_FMT_RGB24:
            return 24;
        case PIX_FMT_BGR565:
        case PIX_FMT_RGB565:
        case PIX_FMT_GRAY16BE:
        case PIX_FMT_GRAY16LE:
            return 16;
        case PIX_FMT_BGR555:
        case PIX_FMT_RGB555:
            return 15;
        case PIX_FMT_BGR8:
        case PIX_FMT_RGB8:
            return 8;
        case PIX_FMT_BGR4:
        case PIX_FMT_RGB4:
        case PIX_FMT_BGR4_BYTE:
        case PIX_FMT_RGB4_BYTE:
            return 4;
        case PIX_FMT_MONOBLACK:
            return 1;
        default:
            return 256;
    }
}
