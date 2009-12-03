
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_MALLOC
#include <malloc.h>
#endif

#define MSGT_CLASS MSGT_CPLAYER
#include "__mp_msg.h"
#include "libvo/img_format.h"
#include "mp_image.h"
#include "libvo/fastmemcpy.h"

void mp_image_setfmt(mp_image_t* mpi,unsigned int out_fmt){
    mpi->flags&=~(MP_IMGFLAG_PLANAR|MP_IMGFLAG_YUV|MP_IMGFLAG_SWAPPED);
    mpi->imgfmt=out_fmt;
    if(out_fmt == IMGFMT_MPEGPES){
	mpi->bpp=0;
	return;
    }
    if(out_fmt == IMGFMT_ZRMJPEGNI ||
	    out_fmt == IMGFMT_ZRMJPEGIT ||
	    out_fmt == IMGFMT_ZRMJPEGIB){
	mpi->bpp=0;
	return;
    }
    if(IMGFMT_IS_XVMC(out_fmt)){
	mpi->bpp=0;
	return;
    }
    mpi->num_planes=1;
    if (IMGFMT_IS_RGB(out_fmt) || IMGFMT_IS_BGR(out_fmt)) {
	mpi->bpp = rgbfmt_depth(out_fmt);
	if(IMGFMT_IS_BGR(out_fmt)) mpi->flags|=MP_IMGFLAG_SWAPPED;
	return;
    }
    mpi->flags|=MP_IMGFLAG_YUV;
    mpi->num_planes=3;
    switch(out_fmt){
    case IMGFMT_I420:
    case IMGFMT_IYUV:
	mpi->flags|=MP_IMGFLAG_SWAPPED;
    case IMGFMT_YV12:
	mpi->flags|=MP_IMGFLAG_PLANAR;
	mpi->bpp=12;
	mpi->chroma_width=(mpi->width>>1);
	mpi->chroma_height=(mpi->height>>1);
	mpi->chroma_x_shift=1;
	mpi->chroma_y_shift=1;
	return;
    case IMGFMT_IF09:
	mpi->num_planes=4;
    case IMGFMT_YVU9:
	mpi->flags|=MP_IMGFLAG_PLANAR;
	mpi->bpp=9;
	mpi->chroma_width=(mpi->width>>2);
	mpi->chroma_height=(mpi->height>>2);
	mpi->chroma_x_shift=2;
	mpi->chroma_y_shift=2;
	return;
    case IMGFMT_444P:
	mpi->flags|=MP_IMGFLAG_PLANAR;
	mpi->bpp=24;
	mpi->chroma_width=(mpi->width);
	mpi->chroma_height=(mpi->height);
	mpi->chroma_x_shift=0;
	mpi->chroma_y_shift=0;
	return;
    case IMGFMT_422P:
	mpi->flags|=MP_IMGFLAG_PLANAR;
	mpi->bpp=16;
	mpi->chroma_width=(mpi->width>>1);
	mpi->chroma_height=(mpi->height);
	mpi->chroma_x_shift=1;
	mpi->chroma_y_shift=0;
	return;
    case IMGFMT_411P:
	mpi->flags|=MP_IMGFLAG_PLANAR;
	mpi->bpp=12;
	mpi->chroma_width=(mpi->width>>2);
	mpi->chroma_height=(mpi->height);
	mpi->chroma_x_shift=2;
	mpi->chroma_y_shift=0;
	return;
    case IMGFMT_Y800:
    case IMGFMT_Y8:
	/* they're planar ones, but for easier handling use them as packed */
//	mpi->flags|=MP_IMGFLAG_PLANAR;
	mpi->bpp=8;
	mpi->num_planes=1;
	return;
    case IMGFMT_UYVY:
	mpi->flags|=MP_IMGFLAG_SWAPPED;
    case IMGFMT_YUY2:
	mpi->bpp=16;
	mpi->num_planes=1;
	return;
    case IMGFMT_NV12:
	mpi->flags|=MP_IMGFLAG_SWAPPED;
    case IMGFMT_NV21:
	mpi->flags|=MP_IMGFLAG_PLANAR;
	mpi->bpp=12;
	mpi->num_planes=2;
	mpi->chroma_width=(mpi->width>>0);
	mpi->chroma_height=(mpi->height>>1);
	mpi->chroma_x_shift=0;
	mpi->chroma_y_shift=1;
	return;
    }
    MSG_WARN("mp_image: Unknown out_fmt: 0x%X\n",out_fmt);
    mpi->bpp=0;
}

mp_image_t* new_mp_image(int w,int h){
    mp_image_t* mpi=(mp_image_t*)malloc(sizeof(mp_image_t));
    if(!mpi) return NULL; // error!
    memset(mpi,0,sizeof(mp_image_t));
    mpi->width=mpi->w=w;
    mpi->height=mpi->h=h;
    return mpi;
}

void free_mp_image(mp_image_t* mpi){
    if(!mpi) return;
    if(mpi->flags&MP_IMGFLAG_ALLOCATED){
	/* becouse we allocate the whole image in once */
	if(mpi->planes[0]) free(mpi->planes[0]);
    }
    free(mpi);
}

mp_image_t* alloc_mpi(int w, int h, unsigned int fmt) {
  mp_image_t* mpi = new_mp_image(w,h);

  mp_image_setfmt(mpi,fmt);
  // IF09 - allocate space for 4. plane delta info - unused
  if (mpi->imgfmt == IMGFMT_IF09)
    {
      mpi->planes[0]=memalign(64, mpi->bpp*mpi->width*(mpi->height+2)/8+
			      mpi->chroma_width*mpi->chroma_height);
      /* delta table, just for fun ;) */
      mpi->planes[3]=mpi->planes[0]+2*(mpi->chroma_width*mpi->chroma_height);
    }
  else
    mpi->planes[0]=memalign(64, mpi->bpp*mpi->width*(mpi->height+2)/8);
  if(mpi->flags&MP_IMGFLAG_PLANAR){
    // YV12/I420/YVU9/IF09. feel free to add other planar formats here...
    if(!mpi->stride[0]) mpi->stride[0]=mpi->width;
    if(!mpi->stride[1]) mpi->stride[1]=mpi->stride[2]=mpi->chroma_width;
    if(mpi->flags&MP_IMGFLAG_SWAPPED){
      // I420/IYUV  (Y,U,V)
      mpi->planes[1]=mpi->planes[0]+mpi->width*mpi->height;
      mpi->planes[2]=mpi->planes[1]+mpi->chroma_width*mpi->chroma_height;
    } else {
      // YV12,YVU9,IF09  (Y,V,U)
      mpi->planes[2]=mpi->planes[0]+mpi->width*mpi->height;
      mpi->planes[1]=mpi->planes[2]+mpi->chroma_width*mpi->chroma_height;
    }
  } else {
    if(!mpi->stride[0]) mpi->stride[0]=mpi->width*mpi->bpp/8;
  }
  mpi->flags|=MP_IMGFLAG_ALLOCATED;
  
  return mpi;
}

void copy_mpi(mp_image_t *dmpi,const mp_image_t *mpi) {
  if(mpi->flags&MP_IMGFLAG_PLANAR){
    memcpy_pic(dmpi->planes[0],mpi->planes[0], mpi->w, mpi->h,
	       dmpi->stride[0],mpi->stride[0]);
    memcpy_pic(dmpi->planes[1],mpi->planes[1], mpi->chroma_width, mpi->chroma_height,
	       dmpi->stride[1],mpi->stride[1]);
    memcpy_pic(dmpi->planes[2], mpi->planes[2], mpi->chroma_width, mpi->chroma_height,
	       dmpi->stride[2],mpi->stride[2]);
  } else {
    memcpy_pic(dmpi->planes[0],mpi->planes[0], 
	       mpi->w*(dmpi->bpp/8), mpi->h,
	       dmpi->stride[0],mpi->stride[0]);
  }
}
