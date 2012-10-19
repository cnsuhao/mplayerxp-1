/* 
 * screenshot.c, Portable Network Graphics Renderer for Mplayer
 *
 * Based on vo_png.c (Copyright 2001 by Felix Buenemann <atmosfear@users.sourceforge.net>)
 *
 * Uses libpng (which uses zlib), so see according licenses.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>

#include "../mp_config.h"
#include "../mplayer.h"
#ifdef HAVE_PNG
#ifdef HAVE_LIBPNG_PNG
#include <libpng/png.h>
#else
#include <png.h>
#endif
#endif

#include "screenshot.h"
#include "img_format.h"
#include "../postproc/swscale.h"
#include "../postproc/vf_scale.h"
#include "vo_msg.h"

#define RGB 0
#define BGR 1
typedef struct sshot_priv_s {
    int cspace;
    unsigned image_width,image_height;
}sshot_priv_t;
static sshot_priv_t sshot = { RGB, 0, 0 };
#ifdef HAVE_PNG
int z_compression = Z_NO_COMPRESSION;

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
       MSG_V("PNG Failed to init png pointer\n");
       png.status = ERROR;
       return png;
    }   
    
    if (!png.info_ptr) {
       MSG_V("PNG Failed to init png infopointer\n");
       png_destroy_write_struct(&png.png_ptr,
         (png_infopp)NULL);
       png.status = ERROR;
       return png;
    }

    if (setjmp(png.png_ptr->jmpbuf)) {
	MSG_V("PNG Internal error!\n");
        png_destroy_write_struct(&png.png_ptr, &png.info_ptr);
        fclose(png.fp);
        png.status = ERROR;
        return png;
    }

    png.fp = fopen (fname, "wb");
    if (png.fp == NULL) {
	MSG_ERR("\nPNG Error opening %s for writing!\n", strerror(errno));
	png.status = ERROR;
	return png;
    }

    MSG_V("PNG Init IO\n");
    png_init_io(png.png_ptr, png.fp);

    /* set the zlib compression level */
    png_set_compression_level(png.png_ptr, z_compression);

    png_set_IHDR(png.png_ptr, png.info_ptr, sshot.image_width, sshot.image_height,
       8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
       PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    MSG_V("PNG Write Info\n");
    png_write_info(png.png_ptr, png.info_ptr);

    if(sshot.cspace) {
    	MSG_V("PNG Set BGR Conversion\n");
    	png_set_bgr(png.png_ptr);
    }

    png.status = OK;
    return png;

}
   
static uint8_t destroy_png(struct pngdata png) {

    MSG_V("PNG Write End\n");
    png_write_end(png.png_ptr, png.info_ptr);

    MSG_V("PNG Destroy Write Struct\n");
    png_destroy_write_struct(&png.png_ptr, &png.info_ptr);

    fclose (png.fp);

    return 0;
}
#else

/* Note: this is LE version */
static void write_bmp(const char *fname,unsigned w,unsigned h,uint8_t *data)
{
    FILE* out;
    char c[4];
    uint32_t udata;
    unsigned i;
    unsigned long fsize_off,data_off,fsize_val,data_val;
    if(!(out=fopen(fname,"wb"))) return;
    c[0]='B';
    c[1]='M';
    fwrite(c,2,1,out);
    fsize_off = ftello(out);
    fseeko(out,4,SEEK_CUR);
    memset(c,0,4);
    fwrite(c,4,1,out);
    data_off=ftello(out);
    fseeko(out,4,SEEK_CUR);

    udata=40;
    fwrite(&udata,4,1,out); /* sizeof BITMAPINFOHEADER == biSize */
    udata=w;
    fwrite(&udata,4,1,out); /* sizeof biWidth */
    udata=h;
    fwrite(&udata,4,1,out); /* sizeof biHeight */
    udata=1;
    fwrite(&udata,2,1,out); /* sizeof biPlanes */
    udata=24;
    fwrite(&udata,2,1,out); /* sizeof biBitCount */
    udata=0;
    fwrite(&udata,4,1,out); /* sizeof biCompression */
    udata=w*h*3;
    fwrite(&udata,4,1,out); /* sizeof biSizeImage */
    udata=0;
    fwrite(&udata,4,1,out); /* sizeof biXPelsPerMeter */
    udata=0;
    fwrite(&udata,4,1,out); /* sizeof biYPelsPerMeter */
    udata=0;
    fwrite(&udata,4,1,out); /* sizeof biClrUsed */
    udata=0;
    fwrite(&udata,4,1,out); /* sizeof biClrImportant */
    data_val=ftello(out);
    for(i=0;i<h;i++) /* flip picture here */
    {
	fwrite(data+(w*3)*(h-i-1),w*3,1,out);
    }
    fsize_val=ftello(out);
    fseeko(out,fsize_off,SEEK_SET);
    fwrite(&fsize_val,4,1,out);
    fseeko(out,data_off,SEEK_SET);
    fwrite(&data_val,2,1,out);
    fseeko(out,fsize_val,SEEK_SET);
    fclose(out);
}
#endif

int gr_screenshot(const char *fname,uint8_t *planes[],unsigned *strides,uint32_t fourcc,unsigned w,unsigned h)
{
    unsigned k;
    char buf[256];
#ifdef HAVE_PNG
    struct pngdata png;
#endif
    uint8_t *image_data=NULL;
    uint8_t *dst[3];
    unsigned dstStride[3],bpp = 24;
    struct SwsContext * sws = NULL;


    sws = sws_getContextFromCmdLine(w,h,pixfmt_from_fourcc(fourcc),w,h,
#ifdef HAVE_PNG
    pixfmt_from_fourcc(IMGFMT_BGR24)
#else
    pixfmt_from_fourcc(IMGFMT_RGB24)
#endif
    );
    if(!sws)
    {
	MSG_ERR("vo_png: Can't initialize SwScaler\n");
	return -1;
    }
    sshot.image_width = w;
    sshot.image_height = h;
    if(!(image_data = malloc(sshot.image_width*sshot.image_height*3)))
    {
	MSG_ERR("vo_png: Can't allocate temporary buffer\n");
	return -1;
    }
#ifdef HAVE_PNG
    if((z_compression >= 0) && (z_compression <= 9)) {
	    if(z_compression == 0) {
		    MSG_HINT("PNG Warning: compression level set to 0, compression disabled!\n");
		    MSG_HINT("PNG Info: Use the -z <n> switch to set compression level from 0 to 9.\n");
		    MSG_HINT("PNG Info: (0 = no compression, 1 = fastest, lowest - 9 best, slowest compression)\n");
	    }
    }
    else {
	    MSG_WARN("PNG Warning: compression level out of range setting to 1!\n");
	    MSG_WARN("PNG Info: Use the -z <n> switch to set compression level from 0 to 9.\n");
	    MSG_WARN("PNG Info: (0 = no compression, 1 = fastest, lowest - 9 best, slowest compression)\n");
	    z_compression = Z_BEST_SPEED;
    }
    MSG_V("PNG Compression level %i\n", z_compression);
#endif
    dstStride[0]=sshot.image_width*3;
    dstStride[1]=
    dstStride[2]=0;
    dst[0]=image_data;
    dst[1]=
    dst[2]=0;
    sws_scale(sws,planes,strides,0,h,dst,dstStride);
#ifdef HAVE_PNG
    snprintf (buf, 100, "%s.png", fname);
#else
    snprintf (buf, 100, "%s.bmp", fname);
#endif

#ifdef HAVE_PNG
    png = create_png(buf);

    if(png.status){
	    MSG_ERR("PNG Error in create_png\n");
    }

    {
	png_byte *row_pointers[sshot.image_height];
	unsigned bppmul = (bpp+7)/8;
	for ( k = 0; k < sshot.image_height; k++ ) row_pointers[k] = &image_data[sshot.image_width*k*bppmul];
	png_write_image(png.png_ptr, row_pointers);
    }

    destroy_png(png);
#else
    write_bmp(buf,w,h,image_data);
#endif
    if(image_data){ free(image_data);image_data=NULL;}
    if(sws) sws_freeContext(sws);
    return 0;
}
