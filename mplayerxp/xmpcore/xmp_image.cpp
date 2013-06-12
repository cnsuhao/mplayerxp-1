#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mplayerxp.h"
#include "libvo2/img_format.h"
#include "xmp_image.h"
#include "osdep/fastmemcpy.h"
#include "player_msg.h"

namespace	usr {
void mp_image_t::setfmt(unsigned int out_fmt) {
    flags&=~(MP_IMGFLAG_PLANAR|MP_IMGFLAG_YUV|MP_IMGFLAG_SWAPPED);
    imgfmt=out_fmt;
    if(out_fmt == IMGFMT_MPEGPES){
	bpp=0;
	return;
    }
    if(out_fmt == IMGFMT_ZRMJPEGNI ||
	    out_fmt == IMGFMT_ZRMJPEGIT ||
	    out_fmt == IMGFMT_ZRMJPEGIB){
	bpp=0;
	return;
    }
    if(IMGFMT_IS_XVMC(out_fmt)){
	bpp=0;
	return;
    }
    num_planes=1;
    if (IMGFMT_IS_RGB(out_fmt) || IMGFMT_IS_BGR(out_fmt)) {
	bpp = rgbfmt_depth(out_fmt);
	if(IMGFMT_IS_BGR(out_fmt)) flags|=MP_IMGFLAG_SWAPPED;
	return;
    }
    flags|=MP_IMGFLAG_YUV;
    num_planes=3;
    switch(out_fmt){
    case IMGFMT_I420:
    case IMGFMT_IYUV:
	flags|=MP_IMGFLAG_SWAPPED;
    case IMGFMT_YV12:
	flags|=MP_IMGFLAG_PLANAR;
	bpp=12;
	chroma_width=(width>>1);
	chroma_height=(height>>1);
	chroma_x_shift=1;
	chroma_y_shift=1;
	return;
    case IMGFMT_420A:
    case IMGFMT_IF09:
	num_planes=4;
    case IMGFMT_YVU9:
	flags|=MP_IMGFLAG_PLANAR;
	bpp=9;
	chroma_width=(width>>2);
	chroma_height=(height>>2);
	chroma_x_shift=2;
	chroma_y_shift=2;
	return;
    case IMGFMT_444P16_LE:
    case IMGFMT_444P16_BE:
	flags|=MP_IMGFLAG_PLANAR;
	bpp=48;
	chroma_width=(width);
	chroma_height=(height);
	chroma_x_shift=0;
	chroma_y_shift=0;
	return;
    case IMGFMT_444P:
	flags|=MP_IMGFLAG_PLANAR;
	bpp=24;
	chroma_width=(width);
	chroma_height=(height);
	chroma_x_shift=0;
	chroma_y_shift=0;
	return;
    case IMGFMT_422P16_LE:
    case IMGFMT_422P16_BE:
	flags|=MP_IMGFLAG_PLANAR;
	bpp=32;
	chroma_width=(width>>1);
	chroma_height=(height);
	chroma_x_shift=1;
	chroma_y_shift=0;
	return;
    case IMGFMT_422P:
	flags|=MP_IMGFLAG_PLANAR;
	bpp=16;
	chroma_width=(width>>1);
	chroma_height=(height);
	chroma_x_shift=1;
	chroma_y_shift=0;
	return;
    case IMGFMT_420P16_LE:
    case IMGFMT_420P16_BE:
	flags|=MP_IMGFLAG_PLANAR;
	bpp=24;
	chroma_width=(width>>2);
	chroma_height=(height);
	chroma_x_shift=2;
	chroma_y_shift=0;
	return;
    case IMGFMT_411P:
	flags|=MP_IMGFLAG_PLANAR;
	bpp=12;
	chroma_width=(width>>2);
	chroma_height=(height);
	chroma_x_shift=2;
	chroma_y_shift=0;
	return;
    case IMGFMT_Y800:
    case IMGFMT_Y8:
	/* they're planar ones, but for easier handling use them as packed */
//	flags|=MP_IMGFLAG_PLANAR;
	bpp=8;
	num_planes=1;
	return;
    case IMGFMT_UYVY:
	flags|=MP_IMGFLAG_SWAPPED;
    case IMGFMT_YUY2:
	bpp=16;
	num_planes=1;
	return;
    case IMGFMT_NV12:
	flags|=MP_IMGFLAG_SWAPPED;
    case IMGFMT_NV21:
	flags|=MP_IMGFLAG_PLANAR;
	bpp=12;
	num_planes=2;
	chroma_width=(width>>0);
	chroma_height=(height>>1);
	chroma_x_shift=0;
	chroma_y_shift=1;
	return;
    }
    mpxp_warn<<"mp_image: Unknown out_fmt: 0x"<<std::hex<<out_fmt<<std::endl;
    show_backtrace("outfmt",10);
    bpp=0;
}

mp_image_t::mp_image_t(unsigned _w,unsigned _h,unsigned _xp_idx)
	    :xp_idx(_xp_idx)
	    ,width(_w)
	    ,height(_h)
	    ,w(_w)
	    ,h(_h)
{
    planes[0]=NULL;
    qscale=NULL;
    priv=NULL;
}

mp_image_t::mp_image_t(const mp_image_t& in)
	:xp_idx(in.xp_idx)
	,flags(in.flags)
	,type(in.type)
	,bpp(in.bpp)
	,imgfmt(in.imgfmt)
	,width(in.width)
	,height(in.height)
	,x(in.x)
	,y(in.y)
	,w(in.w)
	,h(in.h)
	,num_planes(in.num_planes)
	,qscale(in.qscale)
	,qstride(in.qstride)
	,qscale_type(in.qscale_type)
	,pict_type(in.pict_type)
	,fields(in.fields)
	,chroma_width(in.chroma_width)
	,chroma_height(in.chroma_height)
	,chroma_x_shift(in.chroma_x_shift)
	,chroma_y_shift(in.chroma_y_shift)
	,priv(in.priv)
{
    memcpy(stride,in.stride,sizeof(stride));
    alloc();
    copy_planes(in);
}

mp_image_t& mp_image_t::operator=(const mp_image_t& in) {
    copy_genome(in);
    alloc();
    copy_planes(in);
    return *this;
}

mp_image_t::~mp_image_t(){
    if(flags&MP_IMGFLAG_ALLOCATED){
	/* becouse we allocate the whole image in once */
	if(planes[0]) delete planes[0];
    }
}

void mp_image_t::alloc() {
    unsigned size,delta;
    size=bpp*width*(height+2)/8;
    delta=0;
    // IF09 - allocate space for 4. plane delta info - unused
    if (imgfmt == IMGFMT_IF09) delta=chroma_width*chroma_height;
    planes[0]=new(alignmem,64) unsigned char[size+delta];
    if(delta) /* delta table, just for fun ;) */
	planes[3]=planes[0]+2*(chroma_width*chroma_height);
    if(flags&MP_IMGFLAG_PLANAR){
	// YV12/I420/YVU9/IF09. feel mp_free to add other planar formats here...
	if(!stride[0]) stride[0]=width;
	if(!stride[1]) stride[1]=stride[2]=chroma_width;
	if(flags&MP_IMGFLAG_SWAPPED){
	    // I420/IYUV  (Y,U,V)
	    planes[1]=planes[0]+width*height;
	    planes[2]=planes[1]+chroma_width*chroma_height;
	} else {
	    // YV12,YVU9,IF09  (Y,V,U)
	    planes[2]=planes[0]+width*height;
	    planes[1]=planes[2]+chroma_width*chroma_height;
	}
    } else {
	if(!stride[0]) stride[0]=width*bpp/8;
    }
    flags|=MP_IMGFLAG_ALLOCATED;
}

void mp_image_t::copy_genome(const mp_image_t& smpi) {
    xp_idx=smpi.xp_idx;
    flags=smpi.flags;
    type=smpi.type;
    bpp=smpi.bpp;
    imgfmt=smpi.imgfmt;
    width=smpi.width;
    height=smpi.height;
    x=smpi.x;
    y=smpi.y;
    w=smpi.w;
    h=smpi.h;
    num_planes=smpi.num_planes;
    memcpy(stride,smpi.stride,sizeof(stride));
    qscale=smpi.qscale;
    qstride=smpi.qstride;
    qscale_type=smpi.qscale_type;
    pict_type=smpi.pict_type;
    fields=smpi.fields;
    chroma_width=smpi.chroma_width;
    chroma_height=smpi.chroma_height;
    chroma_x_shift=smpi.chroma_x_shift;
    chroma_y_shift=smpi.chroma_y_shift;
    priv=smpi.priv;
}

void mp_image_t::copy_planes(const mp_image_t& smpi) {
  if(smpi.flags&MP_IMGFLAG_PLANAR){
    memcpy_pic(planes[0],smpi.planes[0], smpi.w, smpi.h,
		stride[0],smpi.stride[0]);
    memcpy_pic(planes[1],smpi.planes[1], smpi.chroma_width, smpi.chroma_height,
		stride[1],smpi.stride[1]);
    memcpy_pic(planes[2], smpi.planes[2], smpi.chroma_width, smpi.chroma_height,
		stride[2],smpi.stride[2]);
  } else {
    memcpy_pic(planes[0],smpi.planes[0],
		smpi.w*(bpp/8), smpi.h,
		stride[0],smpi.stride[0]);
  }
}

void mp_image_t::fake_slice(const mp_image_t& mpi,unsigned _y,unsigned _h)
{
    copy_genome(mpi);
    memcpy(planes,mpi.planes,sizeof(planes));
    y = _y;
    h = _h;
    chroma_height = _h >> mpi.chroma_y_shift;
    xp_idx = mpi.xp_idx;
    flags&=~MP_IMGFLAG_ALLOCATED;
}

} // namespace	usr
