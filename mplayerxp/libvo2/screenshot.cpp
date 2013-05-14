#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
/*
 * screenshot.c, Portable Network Graphics Renderer for Mplayer
 *
 * Based on vo_png.c (Copyright 2001 by Felix Buenemann <atmosfear@users.sourceforge.net>)
 *
 * Uses libpng (which uses zlib), so see according licenses.
 *
 */
#include <iostream>
#include <fstream>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>

#include "mplayerxp.h"
#ifdef HAVE_PNG
#ifdef HAVE_LIBPNG_PNG
#include <libpng/png.h>
#else
#include <png.h>
#endif
#endif

#include "screenshot.h"
#include "img_format.h"
#include "mpxp_conf_lavc.h"
#include "postproc/vf_scale.h"
#include "vo_msg.h"

static const int RGB=0;
static const int BGR=1;
typedef struct sshot_priv_s {
    int cspace;
    unsigned image_width,image_height;
}sshot_priv_t;
static sshot_priv_t sshot = { RGB, 0, 0 };
#ifdef HAVE_PNG
struct pngdata {
	FILE * fp;
	png_structp png_ptr;
	png_infop info_ptr;
	enum {OK,ERROR} status;
};

static struct pngdata create_png (char * fname)
{
    struct pngdata png;

    png.png_ptr = png_create_write_struct
       (PNG_LIBPNG_VER_STRING, NULL,
	NULL, NULL);
    png.info_ptr = png_create_info_struct(png.png_ptr);

    if (!png.png_ptr) {
	mpxp_v<<"PNG Failed to init png pointer"<<std::endl;
	png.status = pngdata::ERROR;
	return png;
    }

    if (!png.info_ptr) {
	mpxp_v<<"PNG Failed to init png infopointer"<<std::endl;
	png_destroy_write_struct(&png.png_ptr,(png_infopp)NULL);
	png.status = pngdata::ERROR;
	return png;
    }

    if (setjmp(png.png_ptr->jmpbuf)) {
	mpxp_v<<"PNG Internal error!"<<std::endl;
	png_destroy_write_struct(&png.png_ptr, &png.info_ptr);
	fclose(png.fp);
	png.status = pngdata::ERROR;
	return png;
    }

    png.fp = fopen (fname, "wb");
    if (png.fp == NULL) {
	mpxp_err<<"PNG Error opening "<<strerror(errno)<<" for writing!"<<std::endl;
	png.status = pngdata::ERROR;
	return png;
    }

    mpxp_v<<"PNG Init IO"<<std::endl;
    png_init_io(png.png_ptr, png.fp);

    /* set the zlib compression level */
    png_set_compression_level(png.png_ptr, mp_conf.z_compression);

    png_set_IHDR(png.png_ptr, png.info_ptr, sshot.image_width, sshot.image_height,
       8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
       PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    mpxp_v<<"PNG Write Info"<<std::endl;
    png_write_info(png.png_ptr, png.info_ptr);

    if(sshot.cspace) {
	mpxp_v<<"PNG Set BGR Conversion"<<std::endl;
	png_set_bgr(png.png_ptr);
    }

    png.status = pngdata::OK;
    return png;

}

static uint8_t destroy_png(struct pngdata png) {

    mpxp_v<<"PNG Write End"<<std::endl;
    png_write_end(png.png_ptr, png.info_ptr);

    mpxp_v<<"PNG Destroy Write Struct"<<std::endl;
    png_destroy_write_struct(&png.png_ptr, &png.info_ptr);

    fclose (png.fp);

    return 0;
}
#else

/* Note: this is LE version */
static void write_bmp(const char *fname,unsigned w,unsigned h,uint8_t *data)
{
    std::ofstream f;
    char c[4];
    uint32_t udata;
    unsigned i;
    unsigned long fsize_off,data_off,fsize_val,data_val;
    f.open(fname,std::ios_base::out|std::ios_base::binary);
    if(!f.is_open()) return;
    c[0]='B';
    c[1]='M';
    f.write(c,2);
    fsize_off = f.tellp();
    f.seekp(4,std::ios_base::cur);
    memset(c,0,4);
    f.write(c,4);
    data_off=f.tellp();
    f.seekp(4,std::ios_base::cur);

    udata=40;
    f.write((char*)&udata,4); /* sizeof BITMAPINFOHEADER == biSize */
    udata=w;
    f.write((char*)&udata,4); /* sizeof biWidth */
    udata=h;
    f.write((char*)&udata,4); /* sizeof biHeight */
    udata=1;
    f.write((char*)&udata,2); /* sizeof biPlanes */
    udata=24;
    f.write((char*)&udata,2); /* sizeof biBitCount */
    udata=0;
    f.write((char*)&udata,4); /* sizeof biCompression */
    udata=w*h*3;
    f.write((char*)&udata,4); /* sizeof biSizeImage */
    udata=0;
    f.write((char*)&udata,4); /* sizeof biXPelsPerMeter */
    udata=0;
    f.write((char*)&udata,4); /* sizeof biYPelsPerMeter */
    udata=0;
    f.write((char*)&udata,4); /* sizeof biClrUsed */
    udata=0;
    f.write((char*)&udata,4); /* sizeof biClrImportant */
    data_val=f.tellp();
    for(i=0;i<h;i++) {/* flip picture here */
	f.write((char*)(data+(w*3)*(h-i-1)),w*3);
    }
    fsize_val=f.tellp();
    f.seekp(fsize_off,std::ios_base::beg);
    f.write((char*)&fsize_val,4);
    f.seekp(data_off,std::ios_base::beg);
    f.write((char*)&data_val,2);
    f.seekp(fsize_val,std::ios_base::beg);
    f.close();
}
#endif

MPXP_Rc gr_screenshot(const char *fname,const uint8_t *planes[],const unsigned *strides,uint32_t fourcc,unsigned w,unsigned h)
{
    char buf[256];
#ifdef HAVE_PNG
    struct pngdata png;
#endif
    uint8_t *image_data=NULL;
    uint8_t *dst[3];
    int dstStride[3];
    struct SwsContext * sws = NULL;


    sws = sws_getContextFromCmdLine(w,h,pixfmt_from_fourcc(fourcc),w,h,
#ifdef HAVE_PNG
    pixfmt_from_fourcc(IMGFMT_BGR24)
#else
    pixfmt_from_fourcc(IMGFMT_RGB24)
#endif
    );
    if(!sws) {
	mpxp_err<<"vo_png: Can't initialize SwScaler"<<std::endl;
	return MPXP_False;
    }
    sshot.image_width = w;
    sshot.image_height = h;
    if(!(image_data = new uint8_t[sshot.image_width*sshot.image_height*3]))
    {
	mpxp_err<<"vo_png: Can't allocate temporary buffer"<<std::endl;
	return MPXP_False;
    }
#ifdef HAVE_PNG
    if((mp_conf.z_compression >= 0) && (mp_conf.z_compression <= 9)) {
	    if(mp_conf.z_compression == 0) {
		mpxp_hint<<"PNG Warning: compression level set to 0, compression disabled!"<<std::endl;
		mpxp_hint<<"PNG Info: Use the -z <n> switch to set compression level from 0 to 9."<<std::endl;
		mpxp_hint<<"PNG Info: (0 = no compression, 1 = fastest, lowest - 9 best, slowest compression)"<<std::endl;
	    }
    }
    else {
	mpxp_warn<<"PNG Warning: compression level out of range setting to 1!"<<std::endl;
	mpxp_warn<<"PNG Info: Use the -z <n> switch to set compression level from 0 to 9."<<std::endl;
	mpxp_warn<<"PNG Info: (0 = no compression, 1 = fastest, lowest - 9 best, slowest compression)"<<std::endl;
	    mp_conf.z_compression = Z_BEST_SPEED;
    }
    mpxp_v<<"PNG Compression level "<<mp_conf.z_compression<<std::endl;
#endif
    dstStride[0]=sshot.image_width*3;
    dstStride[1]=
    dstStride[2]=0;
    dst[0]=image_data;
    dst[1]=
    dst[2]=0;
    sws_scale(sws,planes,reinterpret_cast<const int*>(strides),0,h,dst,dstStride);
#ifdef HAVE_PNG
    snprintf (buf, 100, "%s.png", fname);
#else
    snprintf (buf, 100, "%s.bmp", fname);
#endif

#ifdef HAVE_PNG
    png = create_png(buf);

    if(png.status) mpxp_err<<"PNG Error in create_png"<<std::endl;

    png_byte *row_pointers[sshot.image_height];
    unsigned bppmul = (bpp+7)/8;
    for ( k = 0; k < sshot.image_height; k++ ) row_pointers[k] = &image_data[sshot.image_width*k*bppmul];
    png_write_image(png.png_ptr, row_pointers);

    destroy_png(png);
#else
    write_bmp(buf,w,h,image_data);
#endif
    if(image_data){ delete image_data;image_data=NULL;}
    if(sws) sws_freeContext(sws);
    return MPXP_Ok;
}
