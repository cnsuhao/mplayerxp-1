#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
/*
 *
 * HuffYUV Decoder for Mplayer
 * (c) 2002 Roberto Togni
 *
 * Fourcc: HFYU
 *
 * Original Win32 codec copyright:
 *
 *** Huffyuv v2.1.1, by Ben Rudiak-Gould.
 *** http://www.math.berkeley.edu/~benrg/huffyuv.html
 ***
 *** This file is copyright 2000 Ben Rudiak-Gould, and distributed under
 *** the terms of the GNU General Public License, v2 or later.  See
 *** http://www.gnu.org/copyleft/gpl.html.
 *
 */
#include <algorithm>

#include <stdio.h>
#include <stdlib.h>

#include "mpxp_help.h"
#include "osdep/bswap.h"

#include "libvo2/img_format.h"
#include "vd.h"
#include "vd_msg.h"

namespace	usr {
/*
 * Bitmap types
 */
    enum {
	BMPTYPE_YUV=-1,
	BMPTYPE_RGB=-2,
	BMPTYPE_RGBA=-3
    };
/*
 * Compression methods
 */
    enum {
	METHOD_LEFT=0,
	METHOD_GRAD=1,
	METHOD_MEDIAN=2,
	DECORR_FLAG=64,
	METHOD_LEFT_DECORR=(METHOD_LEFT | DECORR_FLAG),
	METHOD_GRAD_DECORR=(METHOD_GRAD | DECORR_FLAG),
	METHOD_OLD=-2,
    };

/*
 * Huffman table
 */
    struct DecodeTable {
	unsigned char* table_pointers[32];
	unsigned char table_data[129*25];
    };

/*
 * Decoder context
 */
    class huffyuv_decoder : public Video_Decoder {
	public:
	    huffyuv_decoder(VD_Interface&,sh_video_t&,put_slice_info_t&,uint32_t fourcc);
	    virtual ~huffyuv_decoder();

	    virtual MPXP_Rc		ctrl(int cmd,any_t* arg,long arg2=0);
	    virtual mp_image_t*		run(const enc_frame_t& frame);
	    virtual video_probe_t	get_probe_information() const;
	private:
	    const unsigned char*	InitializeDecodeTable(const unsigned char* hufftable,
						unsigned char* shift, DecodeTable* decode_table) const;
	    const unsigned char*	InitializeShiftAddTables(const unsigned char* hufftable,
						unsigned char* shift, unsigned* add_shifted) const;
	    const unsigned char*	DecompressHuffmanTable(const unsigned char* hufftable,
						unsigned char* dst) const;
	    unsigned char		huff_decompress(const unsigned int* in, unsigned int *pos,
						const DecodeTable *decode_table,const unsigned char *decode_shift);

	    VD_Interface&		parent;
	    sh_video_t&			sh;
	    const video_probe_t*	probe;
	    // Real image depth
	    int				bitcount;
	    // Prediction method
	    int				method;
	    // Bitmap color type
	    int				bitmaptype;
	    // Huffman tables
	    unsigned char		decode1_shift[256];
	    unsigned char		decode2_shift[256];
	    unsigned char		decode3_shift[256];
	    DecodeTable			decode1, decode2, decode3;
	    // Above line buffers
	    unsigned char*		abovebuf1;
	    unsigned char*		abovebuf2;

	    static const uint32_t	FOURCC_HFYU;
	    static const unsigned char*	HUFFTABLE_CLASSIC_YUV;
	    static const unsigned char*	HUFFTABLE_CLASSIC_RGB;
	    static const unsigned char*	HUFFTABLE_CLASSIC_YUV_CHROMA;
	    static const unsigned char classic_shift_luma[];
	    static const unsigned char classic_shift_chroma[];
	    static const unsigned char classic_add_luma[256];
	    static const unsigned char classic_add_chroma[256];
    };
const uint32_t		huffyuv_decoder::FOURCC_HFYU=mmioFOURCC('H','F','Y','U');
const unsigned char*	huffyuv_decoder::HUFFTABLE_CLASSIC_YUV=(const unsigned char*)(-1);
const unsigned char*	huffyuv_decoder::HUFFTABLE_CLASSIC_RGB=(const unsigned char*)(-2);
const unsigned char*	huffyuv_decoder::HUFFTABLE_CLASSIC_YUV_CHROMA=(const unsigned char*)(-3);

video_probe_t huffyuv_decoder::get_probe_information() const { return *probe; }

static const video_probe_t probes[] = {
    { "huffyuv", "huffyuv", FOURCC_TAG('H','F','Y','U'), VCodecStatus_Working, {IMGFMT_YUY2,IMGFMT_BGR32,IMGFMT_BGR24}, {VideoFlag_None, VideoFlag_None } },
    { NULL, NULL, 0x0, VCodecStatus_NotWorking, {0x0}, { VideoFlag_None }}
};

/*
 * Classic Huffman tables
 */
const unsigned char huffyuv_decoder::classic_shift_luma[] = {
  34,36,35,69,135,232,9,16,10,24,11,23,12,16,13,10,14,8,15,8,
  16,8,17,20,16,10,207,206,205,236,11,8,10,21,9,23,8,8,199,70,
  69,68, 0
};

const unsigned char huffyuv_decoder::classic_shift_chroma[] = {
  66,36,37,38,39,40,41,75,76,77,110,239,144,81,82,83,84,85,118,183,
  56,57,88,89,56,89,154,57,58,57,26,141,57,56,58,57,58,57,184,119,
  214,245,116,83,82,49,80,79,78,77,44,75,41,40,39,38,37,36,34, 0
};

const unsigned char huffyuv_decoder::classic_add_luma[256] = {
    3,  9,  5, 12, 10, 35, 32, 29, 27, 50, 48, 45, 44, 41, 39, 37,
   73, 70, 68, 65, 64, 61, 58, 56, 53, 50, 49, 46, 44, 41, 38, 36,
   68, 65, 63, 61, 58, 55, 53, 51, 48, 46, 45, 43, 41, 39, 38, 36,
   35, 33, 32, 30, 29, 27, 26, 25, 48, 47, 46, 44, 43, 41, 40, 39,
   37, 36, 35, 34, 32, 31, 30, 28, 27, 26, 24, 23, 22, 20, 19, 37,
   35, 34, 33, 31, 30, 29, 27, 26, 24, 23, 21, 20, 18, 17, 15, 29,
   27, 26, 24, 22, 21, 19, 17, 16, 14, 26, 25, 23, 21, 19, 18, 16,
   15, 27, 25, 23, 21, 19, 17, 16, 14, 26, 25, 23, 21, 18, 17, 14,
   12, 17, 19, 13,  4,  9,  2, 11,  1,  7,  8,  0, 16,  3, 14,  6,
   12, 10,  5, 15, 18, 11, 10, 13, 15, 16, 19, 20, 22, 24, 27, 15,
   18, 20, 22, 24, 26, 14, 17, 20, 22, 24, 27, 15, 18, 20, 23, 25,
   28, 16, 19, 22, 25, 28, 32, 36, 21, 25, 29, 33, 38, 42, 45, 49,
   28, 31, 34, 37, 40, 42, 44, 47, 49, 50, 52, 54, 56, 57, 59, 60,
   62, 64, 66, 67, 69, 35, 37, 39, 40, 42, 43, 45, 47, 48, 51, 52,
   54, 55, 57, 59, 60, 62, 63, 66, 67, 69, 71, 72, 38, 40, 42, 43,
   46, 47, 49, 51, 26, 28, 30, 31, 33, 34, 18, 19, 11, 13,  7,  8,
};

const unsigned char huffyuv_decoder::classic_add_chroma[256] = {
    3,  1,  2,  2,  2,  2,  3,  3,  7,  5,  7,  5,  8,  6, 11,  9,
    7, 13, 11, 10,  9,  8,  7,  5,  9,  7,  6,  4,  7,  5,  8,  7,
   11,  8, 13, 11, 19, 15, 22, 23, 20, 33, 32, 28, 27, 29, 51, 77,
   43, 45, 76, 81, 46, 82, 75, 55, 56,144, 58, 80, 60, 74,147, 63,
  143, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
   80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 27, 30, 21, 22,
   17, 14,  5,  6,100, 54, 47, 50, 51, 53,106,107,108,109,110,111,
  112,113,114,115,  4,117,118, 92, 94,121,122,  3,124,103,  2,  1,
    0,129,130,131,120,119,126,125,136,137,138,139,140,141,142,134,
  135,132,133,104, 64,101, 62, 57,102, 95, 93, 59, 61, 28, 97, 96,
   52, 49, 48, 29, 32, 25, 24, 46, 23, 98, 45, 44, 43, 20, 42, 41,
   19, 18, 99, 40, 15, 39, 38, 16, 13, 12, 11, 37, 10,  9,  8, 36,
    7,128,127,105,123,116, 35, 34, 33,145, 31, 79, 42,146, 78, 26,
   83, 48, 49, 50, 44, 47, 26, 31, 30, 18, 17, 19, 21, 24, 25, 13,
   14, 16, 17, 18, 20, 21, 12, 14, 15,  9, 10,  6,  9,  6,  5,  8,
    6, 12,  8, 10,  7,  9,  6,  4,  6,  2,  2,  3,  3,  3,  3,  2,
};

/*
 *
 * Init HuffYUV decoder
 *
 */
huffyuv_decoder::huffyuv_decoder(VD_Interface& p,sh_video_t& _sh,put_slice_info_t& psi,uint32_t fourcc)
	    :Video_Decoder(p,_sh,psi,fourcc)
	    ,parent(p)
	    ,sh(_sh)
{
    unsigned i;
    for(i=0;probes[i].driver;i++)
	if(fourcc==probes[i].fourcc)
	    probe=&probes[i];
    if(!probe) throw bad_format_exception();

    MPXP_Rc vo_ret; // Video output init ret value
    const unsigned char *hufftable; // Compressed huffman tables
    BITMAPINFOHEADER *bih = sh.bih;

    mpxp_v<<"[HuffYUV] Allocating above line buffer"<<std::endl;
    abovebuf1 = new unsigned char [4*bih->biWidth];
    abovebuf2 = new unsigned char [4*bih->biWidth];

    if (bih->biCompression != FOURCC_HFYU) {
	mpxp_v<<"[HuffYUV] BITMAPHEADER fourcc != HFYU"<<std::endl;
	throw bad_format_exception();
    }

    /* Get bitcount */
    bitcount = 0;
    if (bih->biSize > sizeof(BITMAPINFOHEADER)+1)
	bitcount = *((char*)bih + sizeof(BITMAPINFOHEADER) + 1);
    if (bitcount == 0)
	bitcount = bih->biBitCount;

    /* Get bitmap type */
    switch (bitcount & ~7) {
	case 16:
	    bitmaptype = BMPTYPE_YUV; // -1
	    mpxp_v<<"[HuffYUV] Image type is YUV"<<std::endl;
	    break;
	case 24:
	    bitmaptype = BMPTYPE_RGB; // -2
	    mpxp_v<<"[HuffYUV] Image type is RGB"<<std::endl;
	    break;
	case 32:
	    bitmaptype = BMPTYPE_RGBA; //-3
	    mpxp_v<<"[HuffYUV] Image type is RGBA"<<std::endl;
	    break;
	default:
	    bitmaptype = 0; // ERR
	    mpxp_v<<"[HuffYUV] Image type is unknown"<<std::endl;
    }

    /* Get method */
    switch (bih->biBitCount & 7) {
	case 0:
	    if (bih->biSize > sizeof(BITMAPINFOHEADER)) {
		method = *((unsigned char*)bih + sizeof(BITMAPINFOHEADER));
		mpxp_v<<"[HuffYUV] Method stored in extra data"<<std::endl;
	    } else
		method = METHOD_OLD;	// Is it really needed?
	    break;
	case 1:
	    method = METHOD_LEFT;
	    break;
	case 2:
	    method = METHOD_LEFT_DECORR;
	    break;
	case 3:
	    if (bitmaptype == BMPTYPE_YUV) {
		method = METHOD_GRAD;
	    } else {
		method = METHOD_GRAD_DECORR;
	    }
	    break;
	case 4:
	    method = METHOD_MEDIAN;
	    break;
	default:
	    mpxp_v<<"[HuffYUV] Method: fallback to METHOD_OLD"<<std::endl;
	    method = METHOD_OLD;
    }

    /* Print method info */
    switch (method) {
	case METHOD_LEFT:
	    mpxp_v<<"[HuffYUV] Method: Predict Left"<<std::endl;
	    break;
	case METHOD_GRAD:
	    mpxp_v<<"[HuffYUV] Method: Predict Gradient"<<std::endl;
	    break;
	case METHOD_MEDIAN:
	    mpxp_v<<"[HuffYUV] Method: Predict Median"<<std::endl;
	    break;
	case METHOD_LEFT_DECORR:
	    mpxp_v<<"[HuffYUV] Method: Predict Left with decorrelation"<<std::endl;
	    break;
	case METHOD_GRAD_DECORR:
	    mpxp_v<<"[HuffYUV] Method: Predict Gradient with decorrelation"<<std::endl;
	    break;
	case METHOD_OLD:
	    mpxp_v<<"[HuffYUV] Method Old"<<std::endl;
	    break;
	default:
	    mpxp_v<<"[HuffYUV] Method unknown"<<std::endl;
    }

    /* Get compressed Huffman tables */
    if (bih->biSize == sizeof(BITMAPINFOHEADER) /*&& !(bih->biBitCount&7)*/) {
	hufftable = (bitmaptype == BMPTYPE_YUV) ? HUFFTABLE_CLASSIC_YUV : HUFFTABLE_CLASSIC_RGB;
	mpxp_v<<"[HuffYUV] Using classic static Huffman tables"<<std::endl;
    } else {
	hufftable = (unsigned char*)bih + sizeof(BITMAPINFOHEADER) + ((bih->biBitCount&7) ? 0 : 4);
	mpxp_v<<"[HuffYUV] Using Huffman tables stored in file"<<std::endl;
    }

    /* Initialize decoder Huffman tables */
    hufftable = InitializeDecodeTable(hufftable, decode1_shift, &(decode1));
    hufftable = InitializeDecodeTable(hufftable, decode2_shift, &(decode2));
    InitializeDecodeTable(hufftable, decode3_shift, &(decode3));

    /*
     * Initialize video output device
     */
    switch (bitmaptype) {
	case BMPTYPE_RGB:
	case BMPTYPE_YUV:
	    vo_ret = parent.config_vf(sh.src_w,sh.src_h);
	    break;
	case BMPTYPE_RGBA:
	    mpxp_v<<"[HuffYUV] RGBA not supported yet."<<std::endl;
	    throw bad_format_exception();
	default:
	    mpxp_v<<"[HuffYUV] BUG! Unknown bitmaptype in vo config."<<std::endl;
	    throw bad_format_exception();
    }
    if(vo_ret!=MPXP_Ok) throw bad_format_exception();
}

/*
 *
 * Uninit HuffYUV decoder
 *
 */
huffyuv_decoder::~huffyuv_decoder() {
    if (abovebuf1)
	delete abovebuf1;
    if (abovebuf2)
	delete abovebuf2;
}

// to set/get/query special features/parameters
MPXP_Rc huffyuv_decoder::ctrl(int cmd,any_t* arg,long arg2)
{
    UNUSED(arg2);
    switch(cmd) {
	case VDCTRL_QUERY_FORMAT:
	    if (bitmaptype == BMPTYPE_YUV) {
		if (*((int*)arg) == IMGFMT_YUY2)
		    return MPXP_True;
		else
		    return MPXP_False;
	    } else {
		if ((*((int*)arg) == IMGFMT_BGR32) || (*((int*)arg) == IMGFMT_BGR24))
		    return MPXP_True;
		else
		    return MPXP_False;
	    }
    }
    return MPXP_Unknown;
}

#define HUFF_DECOMPRESS_YUYV() \
{ \
    y1 = huff_decompress((unsigned int *)encoded, &pos, &(decode1), decode1_shift); \
    u = huff_decompress((unsigned int *)encoded, &pos, &(decode2), decode2_shift); \
    y2 = huff_decompress((unsigned int *)encoded, &pos, &(decode1), decode1_shift); \
    v = huff_decompress((unsigned int *)encoded, &pos, &(decode3), decode3_shift); \
}

#define HUFF_DECOMPRESS_RGB_DECORR() \
{ \
    g = huff_decompress((unsigned int *)encoded, &pos, &(decode2), decode2_shift); \
    b = huff_decompress((unsigned int *)encoded, &pos, &(decode1), decode1_shift); \
    r = huff_decompress((unsigned int *)encoded, &pos, &(decode3), decode3_shift); \
}

#define HUFF_DECOMPRESS_RGB() \
{ \
    b = huff_decompress((unsigned int *)encoded, &pos, &(decode1), decode1_shift); \
    g = huff_decompress((unsigned int *)encoded, &pos, &(decode2), decode2_shift); \
    r = huff_decompress((unsigned int *)encoded, &pos, &(decode3), decode3_shift); \
}

#define MEDIAN(left, above, aboveleft) \
{ \
    if ((mi = (above)) > (left)) { \
	mx = mi; \
	mi = (left); \
    } else \
	mx = (left); \
    tmp = (above) + (left) - (aboveleft); \
    if (tmp < mi) \
	med = mi; \
    else if (tmp > mx) \
	med = mx; \
    else \
	med = tmp; \
}

#define YUV_STORE1ST_ABOVEBUF() \
{ \
    abovebuf[0] = outptr[0] = encoded[0]; \
    abovebuf[1] = left_u = outptr[1] = encoded[1]; \
    abovebuf[2] = left_y = outptr[2] = encoded[2]; \
    abovebuf[3] = left_v = outptr[3] = encoded[3]; \
    pixel_ptr = 4; \
}

#define YUV_STORE1ST() \
{ \
    outptr[0] = encoded[0]; \
    left_u = outptr[1] = encoded[1]; \
    left_y = outptr[2] = encoded[2]; \
    left_v = outptr[3] = encoded[3]; \
    pixel_ptr = 4; \
}

#define RGB_STORE1ST() \
{ \
    pixel_ptr = (height-1)*mpi->stride[0]; \
    left_b = outptr[pixel_ptr++] = encoded[1]; \
    left_g = outptr[pixel_ptr++] = encoded[2]; \
    left_r = outptr[pixel_ptr++] = encoded[3]; \
    pixel_ptr += bgr32; \
}

#define RGB_STORE1ST_ABOVEBUF() \
{ \
    pixel_ptr = (height-1)*mpi->stride[0]; \
    abovebuf[0] = left_b = outptr[pixel_ptr++] = encoded[1]; \
    abovebuf[1] = left_g = outptr[pixel_ptr++] = encoded[2]; \
    abovebuf[2] = left_r = outptr[pixel_ptr++] = encoded[3]; \
    pixel_ptr += bgr32; \
}

#define YUV_PREDLEFT() \
{ \
    outptr[pixel_ptr++] = left_y += y1; \
    outptr[pixel_ptr++] = left_u += u; \
    outptr[pixel_ptr++] = left_y += y2; \
    outptr[pixel_ptr++] = left_v += v; \
}

#define YUV_PREDLEFT_BUF(buf, offs) \
{ \
    (buf)[(offs)] = outptr[pixel_ptr++] = left_y += y1; \
    (buf)[(offs)+1] = outptr[pixel_ptr++] = left_u += u; \
    (buf)[(offs)+2] = outptr[pixel_ptr++] = left_y += y2; \
    (buf)[(offs)+3] = outptr[pixel_ptr++] = left_v += v; \
}

#define YUV_PREDMED() \
{ \
    MEDIAN (left_y, abovebuf[col], abovebuf[col-2]); \
    curbuf[col] = outptr[pixel_ptr++] = left_y = med + y1; \
    MEDIAN (left_u, abovebuf[col+1], abovebuf[col+1-4]); \
    curbuf[col+1] = outptr[pixel_ptr++] = left_u = med + u; \
    MEDIAN (left_y, abovebuf[col+2], abovebuf[col+2-2]); \
    curbuf[col+2] = outptr[pixel_ptr++] = left_y = med + y2; \
    MEDIAN (left_v, abovebuf[col+3], abovebuf[col+3-4]); \
    curbuf[col+3] = outptr[pixel_ptr++] = left_v = med + v; \
}

#define RGB_PREDLEFT_DECORR() { \
    outptr[pixel_ptr++] = left_b += b + g; \
    outptr[pixel_ptr++] = left_g += g; \
    outptr[pixel_ptr++] = left_r += r + g; \
    pixel_ptr += bgr32; \
}

#define RGB_PREDLEFT() { \
    outptr[pixel_ptr++] = left_b += b; \
    outptr[pixel_ptr++] = left_g += g; \
    outptr[pixel_ptr++] = left_r += r; \
    pixel_ptr += bgr32; \
}

/*
 *
 * Decode a HuffYUV frame
 *
 */
mp_image_t* huffyuv_decoder::run(const enc_frame_t& frame)
{
    mp_image_t* mpi;
    int pixel_ptr;
    unsigned char y1, y2, u, v, r, g, b;
    unsigned char left_y, left_u, left_v, left_r, left_g, left_b;
    unsigned char tmp, mi, mx, med;
    unsigned char *swap;
    int row, col;
    unsigned int pos = 32;
    const unsigned char *encoded = (const unsigned char *)frame.data;
    unsigned char *abovebuf = abovebuf1;
    unsigned char *curbuf = abovebuf2;
    unsigned char *outptr;
    int width = sh.src_w; // Real image width
    int height = sh.src_h; // Real image height
    int bgr32;

    // skipped frame
    if(frame.len <= 0) return NULL;

    /* Do not accept stride for rgb, it gives me wrong output :-( */
    if (bitmaptype == BMPTYPE_YUV)
	mpi=parent.get_image(MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE,sh.src_w, sh.src_h);
    else
	mpi=parent.get_image(MP_IMGTYPE_TEMP, 0,sh.src_w, sh.src_h);
    if(mpi->flags&MP_IMGFLAG_DIRECT) mpi->flags|=MP_IMGFLAG_RENDERED;

    outptr = mpi->planes[0]; // Output image pointer

    if (bitmaptype == BMPTYPE_YUV) {
	width >>= 1; // Each cycle stores two pixels
	if (method == METHOD_GRAD) {
	    /*
	     * YUV predict gradient
	     */
	    /* Store 1st pixel */
	    YUV_STORE1ST_ABOVEBUF();
	    // Decompress 1st row (always stored with left prediction)
	    for (col = 1*4; col < width*4; col += 4) {
		HUFF_DECOMPRESS_YUYV();
		YUV_PREDLEFT_BUF(abovebuf, col);
	    }
	    curbuf[width*4-1] = curbuf[width*4-2] = curbuf[width*4-3] = 0;
	    for (row = 1; row < height; row++) {
		pixel_ptr = row * mpi->stride[0];
		HUFF_DECOMPRESS_YUYV();
		curbuf[0] = outptr[pixel_ptr++] = left_y += y1 + abovebuf[0] - curbuf[width*4-2];
		curbuf[1] = outptr[pixel_ptr++] = left_u += u + abovebuf[1] - curbuf[width*4+1-4];
		curbuf[2] = outptr[pixel_ptr++] = left_y += y2 + abovebuf[2] - abovebuf[0];
		curbuf[3] = outptr[pixel_ptr++] = left_v += v + abovebuf[3] - curbuf[width*4+3-4];
		for (col = 1*4; col < width*4; col += 4) {
		    HUFF_DECOMPRESS_YUYV();
		    curbuf[col] = outptr[pixel_ptr++] = left_y += y1 + abovebuf[col]-abovebuf[col-2];
		    curbuf[col+1] = outptr[pixel_ptr++] = left_u += u + abovebuf[col+1]-abovebuf[col+1-4];
		    curbuf[col+2] = outptr[pixel_ptr++] = left_y += y2 + abovebuf[col+2]-abovebuf[col+2-2];
		    curbuf[col+3] = outptr[pixel_ptr++] = left_v += v + abovebuf[col+3]-abovebuf[col+3-4];
		}
		swap = abovebuf;
		abovebuf = curbuf;
		curbuf = swap;
	    }
	} else if (method == METHOD_MEDIAN) {
	    /*
	     * YUV predict median
	     */
	    /* Store 1st pixel */
	    YUV_STORE1ST_ABOVEBUF();
	    // Decompress 1st row (always stored with left prediction)
	    for (col = 1*4; col < width*4; col += 4) {
		HUFF_DECOMPRESS_YUYV();
		YUV_PREDLEFT_BUF (abovebuf, col);
	    }
	    // Decompress 1st two pixels of 2nd row
	    pixel_ptr = mpi->stride[0];
	    HUFF_DECOMPRESS_YUYV();
	    YUV_PREDLEFT_BUF (curbuf, 0);
	    HUFF_DECOMPRESS_YUYV();
	    YUV_PREDLEFT_BUF (curbuf, 4);
	    // Complete 2nd row
	    for (col = 2*4; col < width*4; col += 4) {
		HUFF_DECOMPRESS_YUYV();
		YUV_PREDMED();
	    }
	    swap = abovebuf;
	    abovebuf = curbuf;
	    curbuf = swap;
	    for (row = 2; row < height; row++) {
		pixel_ptr = row * mpi->stride[0];
		HUFF_DECOMPRESS_YUYV();
		MEDIAN (left_y, abovebuf[0], curbuf[width*4-2]);
		curbuf[0] = outptr[pixel_ptr++] = left_y = med + y1;
		MEDIAN (left_u, abovebuf[1], curbuf[width*4+1-4]);
		curbuf[1] = outptr[pixel_ptr++] = left_u = med + u;
		MEDIAN (left_y, abovebuf[2], abovebuf[0]);
		curbuf[2] = outptr[pixel_ptr++] = left_y = med + y2;
		MEDIAN (left_v, abovebuf[3], curbuf[width*4+3-4]);
		curbuf[3] = outptr[pixel_ptr++] = left_v = med + v;
		for (col = 1*4; col < width*4; col += 4) {
		    HUFF_DECOMPRESS_YUYV();
		    YUV_PREDMED();
		}
		swap = abovebuf;
		abovebuf = curbuf;
		curbuf = swap;
	    }
	} else {
	    /*
	     * YUV predict left and predict old
	     */
	    /* Store 1st pixel */
	    YUV_STORE1ST();
	    // Decompress 1st row (always stored with left prediction)
	    for (col = 1*4; col < width*4; col += 4) {
			HUFF_DECOMPRESS_YUYV();
			YUV_PREDLEFT();
	    }
	    for (row = 1; row < height; row++) {
		pixel_ptr = row * mpi->stride[0];
		for (col = 0; col < width*4; col += 4) {
		    HUFF_DECOMPRESS_YUYV();
		    YUV_PREDLEFT();
		}
	    }
	}
    } else {
	bgr32 = (mpi->bpp) >> 5; // 1 if bpp = 32, 0 if bpp = 24
	if (method == METHOD_LEFT_DECORR) {
	    /*
	     * RGB predict left with decorrelation
	     */
	    /* Store 1st pixel */
	    RGB_STORE1ST();
	    // Decompress 1st row
	    for (col = 1; col < width; col ++) {
		HUFF_DECOMPRESS_RGB_DECORR();
		RGB_PREDLEFT_DECORR();
	    }
	    for (row = 1; row < height; row++) {
		pixel_ptr = (height - row - 1) * mpi->stride[0];
		for (col = 0; col < width; col++) {
		    HUFF_DECOMPRESS_RGB_DECORR();
		    RGB_PREDLEFT_DECORR();
		}
	    }
	} else if (method == METHOD_GRAD_DECORR) {
	    /*
	     * RGB predict gradient with decorrelation
	     */
	    /* Store 1st pixel */
	    RGB_STORE1ST_ABOVEBUF();
	    // Decompress 1st row (always stored with left prediction)
	    for (col = 1*3; col < width*3; col += 3) {
		HUFF_DECOMPRESS_RGB_DECORR();
		abovebuf[col] = outptr[pixel_ptr++] = left_b += b + g;
		abovebuf[col+1] = outptr[pixel_ptr++] = left_g += g;
		abovebuf[col+2] = outptr[pixel_ptr++] = left_r += r + g;
		pixel_ptr += bgr32;
	    }
	    curbuf[width*3-1] = curbuf[width*3-2] = curbuf[width*3-3] = 0;
	    for (row = 1; row < height; row++) {
		pixel_ptr = (height - row - 1) * mpi->stride[0];
		HUFF_DECOMPRESS_RGB_DECORR();
		curbuf[0] = outptr[pixel_ptr++] = left_b += b + g + abovebuf[0] - curbuf[width*3-3];
		curbuf[1] = outptr[pixel_ptr++] = left_g += g + abovebuf[1] - curbuf[width*3+1-3];
		curbuf[2] = outptr[pixel_ptr++] = left_r += r + g + abovebuf[2] - curbuf[width*3+2-3];
		pixel_ptr += bgr32;
		for (col = 1*3; col < width*3; col += 3) {
		    HUFF_DECOMPRESS_RGB_DECORR();
		    curbuf[col] = outptr[pixel_ptr++] = left_b += b + g + abovebuf[col]-abovebuf[col-3];
		    curbuf[col+1] = outptr[pixel_ptr++] = left_g += g + abovebuf[col+1]-abovebuf[col+1-3];
		    curbuf[col+2] = outptr[pixel_ptr++] = left_r += r + g + abovebuf[col+2]-abovebuf[col+2-3];
		    pixel_ptr += bgr32;
		}
		swap = abovebuf;
		abovebuf = curbuf;
		curbuf = swap;
	    }
	} else {
	    /*
	     * RGB predict left (no decorrelation) and predict old
	     */
	    /* Store 1st pixel */
	    RGB_STORE1ST();
	    // Decompress 1st row
	    for (col = 1; col < width; col++) {
		HUFF_DECOMPRESS_RGB();
		RGB_PREDLEFT();
	    }
	    for (row = 1; row < height; row++) {
		pixel_ptr = (height - row - 1) * mpi->stride[0];
		for (col = 0; col < width; col++) {
		    HUFF_DECOMPRESS_RGB();
		    RGB_PREDLEFT();
		}
	    }
	}
    }
    return mpi;
}

const unsigned char* huffyuv_decoder::InitializeDecodeTable(const unsigned char* hufftable,
					unsigned char* shift, DecodeTable* decode_table) const
{
	unsigned int add_shifted[256];
	char code_lengths[256];
	char code_firstbits[256];
	char table_lengths[32];
	int all_zero_code=-1;
	int i, j, k;
	int firstbit, length, val;
	unsigned char* p;
	unsigned char * table;

	/* Initialize shift[] and add_shifted[] */
	hufftable = InitializeShiftAddTables(hufftable, shift, add_shifted);

	memset(table_lengths, -1, 32);

	/* Fill code_firstbits[], code_legths[] and table_lengths[] */
	for (i = 0; i < 256; ++i) {
		if (add_shifted[i]) {
			for (firstbit = 31; firstbit >= 0; firstbit--) {
				if (add_shifted[i] & (1 << firstbit)) {
					code_firstbits[i] = firstbit;
					length = shift[i] - (32 - firstbit);
					code_lengths[i] = length;
					table_lengths[firstbit] = std::max(int(table_lengths[firstbit]), length);
					break;
				}
			}
		} else {
			all_zero_code = i;
		}
	}

	p = decode_table->table_data;
	*p++ = 31;
	*p++ = all_zero_code;
	for (j = 0; j < 32; ++j) {
		if (table_lengths[j] == -1) {
			decode_table->table_pointers[j] = decode_table->table_data;
		} else {
			decode_table->table_pointers[j] = p;
			*p++ = j - table_lengths[j];
			p += 1 << table_lengths[j];
		}
	}

	for (k=0; k<256; ++k) {
		if (add_shifted[k]) {
			firstbit = code_firstbits[k];
			val = add_shifted[k] - (1 << firstbit);
			table = decode_table->table_pointers[firstbit];
			memset(&table[1 + (val >> table[0])], k,
						 1 << (table_lengths[firstbit] - code_lengths[k]));
		}
	}
	return hufftable;
}

const unsigned char* huffyuv_decoder::InitializeShiftAddTables(const unsigned char* hufftable,
					unsigned char* shift, unsigned* add_shifted) const
{
	int i, j;
	unsigned int bits; // must be 32bit unsigned
	int min_already_processed;
	int max_not_processed;
	int bit;

	// special-case the old tables, since they don't fit the new rules
	if (hufftable == HUFFTABLE_CLASSIC_YUV || hufftable == HUFFTABLE_CLASSIC_RGB) {
		DecompressHuffmanTable(classic_shift_luma, shift);
		for (i = 0; i < 256; ++i)
			add_shifted[i] = classic_add_luma[i] << (32 - shift[i]);
		return (hufftable == HUFFTABLE_CLASSIC_YUV) ? HUFFTABLE_CLASSIC_YUV_CHROMA : hufftable;
	} else if (hufftable == HUFFTABLE_CLASSIC_YUV_CHROMA) {
		DecompressHuffmanTable(classic_shift_chroma, shift);
		for (i = 0; i < 256; ++i)
			add_shifted[i] = classic_add_chroma[i] << (32 - shift[i]);
		return hufftable;
	}

	hufftable = DecompressHuffmanTable(hufftable, shift);

	// derive the actual bit patterns from the code lengths
	min_already_processed = 32;
	bits = 0;
	do {
		max_not_processed = 0;
		for (i = 0; i < 256; ++i) {
			if (shift[i] < min_already_processed && shift[i] > max_not_processed)
				max_not_processed = shift[i];
		}
		bit = 1 << (32 - max_not_processed);
//		assert (!(bits & (bit - 1)));
		for (j = 0; j < 256; ++j) {
			if (shift[j] == max_not_processed) {
				add_shifted[j] = bits;
				bits += bit;
			}
		}
		min_already_processed = max_not_processed;
	} while (bits & 0xFFFFFFFF);

	return hufftable;
}

const unsigned char* huffyuv_decoder::DecompressHuffmanTable(const unsigned char* hufftable,
						    unsigned char* dst) const
{
	int val;
	int repeat;
	int i = 0;

	do {
		val = *hufftable & 31;
		repeat = *hufftable++ >> 5;
		if (!repeat)
			repeat = *hufftable++;
		while (repeat--)
			dst[i++] = val;
	} while (i < 256);

	return hufftable;
}

unsigned char huffyuv_decoder::huff_decompress(const unsigned int* in, unsigned int *pos,const DecodeTable *decode_table,
						const unsigned char *decode_shift)
{
	unsigned int word = *pos >> 5;
	unsigned int bit = *pos & 31;
	unsigned int val = in[word];
	unsigned char outbyte;
	unsigned char *tableptr;
	int i;

	if (bit)
		val = (val << bit) | (in[word + 1] >> (32 - bit));
  // figure out the appropriate lookup table based on the number of leading zeros
	i = 31;
	val |= 1;
	while ((val & (1 << i--)) == 0);
	val &= ~(1 << (i+1));
	tableptr = decode_table->table_pointers[i+1];
	val >>= *tableptr;

	outbyte = tableptr[val+1];
	*pos += decode_shift[outbyte];

	return outbyte;
}

static const mpxp_option_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

static Video_Decoder* query_interface(VD_Interface& p,sh_video_t& sh,put_slice_info_t& psi,uint32_t fourcc) { return new(zeromem) huffyuv_decoder(p,sh,psi,fourcc); }

extern const vd_info_t vd_huffyuv_info = {
    "HuffYUV Video decoder",
    "huffyuv",
    "Roberto Togni (original win32 by Ben Rudiak-Gould http://www.math.berkeley.edu/~benrg/huffyuv.html)",
    "build-in",
    query_interface,
    options
};
} // namespace	usr