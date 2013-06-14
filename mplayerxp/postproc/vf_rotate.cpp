#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "libvo2/img_format.h"
#include "xmpcore/xmp_image.h"
#include "vf.h"
#include "vf_internal.h"

#include "osdep/fastmemcpy.h"
#include "postproc/swscale.h"
#include "pp_msg.h"

struct vf_priv_t {
    unsigned dw,dh;
    int direction;
};

static void __FASTCALL__ rotate_90(unsigned char* dst,const unsigned char* src,int dststride,int srcstride,unsigned w,unsigned h,unsigned bpp){
    int y;
    dst+=dststride*(h-1);
    dststride*=-1;

    for(y=0;y<h;y++){
	int x;
	switch(bpp){
	case 1:
	    for(x=0;x<w;x++) dst[x]=src[y+x*srcstride];
	    break;
	case 2:
	    for(x=0;x<w;x++) *((short*)(dst+x*2))=*((short*)(src+y*2+x*srcstride));
	    break;
	case 3:
	    for(x=0;x<w;x++){
		dst[x*3+0]=src[0+y*3+x*srcstride];
		dst[x*3+1]=src[1+y*3+x*srcstride];
		dst[x*3+2]=src[2+y*3+x*srcstride];
	    }
	    break;
	case 4:
	    for(x=0;x<w;x++) *((int*)(dst+x*4))=*((int*)(src+y*4+x*srcstride));
	}
	dst+=dststride;
    }
}

static void __FASTCALL__ rotate_270(unsigned char* dst,const unsigned char* src,int dststride,int srcstride,unsigned w,unsigned h,unsigned bpp){
    int y;
    dst+=dststride*(h-1);
    dststride*=-1;

    for(y=0;y<h;y++){
	int x;
	switch(bpp){
	case 1:
	    for(x=0;x<w;x++) dst[x]=src[(h-y-1)+(w-x-1)*srcstride];
	    break;
	case 2:
	    for(x=0;x<w;x++) *((short*)(dst+x*2))=*((short*)(src+y*2+x*srcstride));
	    break;
	case 3:
	    for(x=0;x<w;x++){
		dst[x*3+0]=src[0+y*3+x*srcstride];
		dst[x*3+1]=src[1+y*3+x*srcstride];
		dst[x*3+2]=src[2+y*3+x*srcstride];
	    }
	    break;
	case 4:
	    for(x=0;x<w;x++) *((int*)(dst+x*4))=*((int*)(src+y*4+x*srcstride));
	}
	dst+=dststride;
    }
}

static void __FASTCALL__ rotate_180(unsigned char* dst,const unsigned char* src,int dststride,int srcstride,unsigned w,unsigned h,unsigned bpp){
    unsigned y;
    src+=srcstride*(h-1);
    for(y=0;y<h;y++){
	int x;
	switch(bpp){
	case 1:
	    for(x=0;x<w;x++) dst[w-x-1]=src[x];
	    break;
	case 2:
	    for(x=0;x<w;x++) *((short*)(dst+(w-x-1)*2))=*((short*)(src+x*2));
	    break;
	case 3:
	    for(x=0;x<w;x++){
		dst[(w-x-1)*3+0]=src[0+x];
		dst[(w-x-1)*3+1]=src[1+x];
		dst[(w-x-1)*3+2]=src[2+x];
	    }
	    break;
	case 4:
	    for(x=0;x<w;x++) *((int*)(dst+(w-x-1)*4))=*((int*)(src+x*4));
	}
	dst+=dststride;
	src-=srcstride;
    }
}

static void __FASTCALL__ rotate(unsigned char* dst,const unsigned char* src,int dststride,int srcstride,int w,int h,int bpp,int dir)
{
    if(dir==90) rotate_90(dst,src,dststride,srcstride,w,h,bpp);
    else
    if(dir==180) rotate_180(dst,src,dststride,srcstride,w,h,bpp);
    else
    if(dir==270) rotate_270(dst,src,dststride,srcstride,w,h,bpp);
}

//===========================================================================//

static int __FASTCALL__ vf_config(vf_instance_t* vf,
	int width, int height, int d_width, int d_height,
	vo_flags_e flags, unsigned int outfmt){
    vf->priv->dw=width;
    vf->priv->dh=height;
    if(vf->priv->direction==180)
	return vf_next_config(vf,width,height,d_width,d_height,flags,outfmt);
    else
	return vf_next_config(vf,height,width,d_height,d_width,flags,outfmt);
}

static int __FASTCALL__ put_slice(vf_instance_t* vf,const mp_image_t& smpi){
    mp_image_t *dmpi;

    // hope we'll get DR buffer:
    dmpi=vf_get_new_temp_genome(vf->next,smpi);
    if(smpi.flags&MP_IMGFLAG_PLANAR){
	rotate(dmpi->planes[0],smpi.planes[0],
	       dmpi->stride[0],smpi.stride[0],
	       dmpi->w,dmpi->h,1,vf->priv->direction);
	rotate(dmpi->planes[1],smpi.planes[1],
	       dmpi->stride[1],smpi.stride[1],
	       dmpi->w>>smpi.chroma_x_shift,dmpi->h>>smpi.chroma_y_shift,1,vf->priv->direction);
	rotate(dmpi->planes[2],smpi.planes[2],
	       dmpi->stride[2],smpi.stride[2],
	       dmpi->w>>smpi.chroma_x_shift,dmpi->h>>smpi.chroma_y_shift,1,vf->priv->direction);
    } else {
	rotate(dmpi->planes[0],smpi.planes[0],
	       dmpi->stride[0],smpi.stride[0],
	       dmpi->w,dmpi->h,dmpi->bpp>>3,vf->priv->direction);
	dmpi->planes[1] = smpi.planes[1]; // passthrough rgb8 palette
    }
    return vf_next_put_slice(vf,*dmpi);
}

//===========================================================================//

static int __FASTCALL__ query_format(vf_instance_t* vf, unsigned int fmt,unsigned w,unsigned h){
    if(IMGFMT_IS_RGB(fmt) || IMGFMT_IS_BGR(fmt)) return vf_next_query_format(vf, fmt,w,h);
    // we can support only symmetric (chroma_x_shift==chroma_y_shift) YUV formats:
    switch(fmt) {
	case IMGFMT_YV12:
	case IMGFMT_I420:
	case IMGFMT_IYUV:
	case IMGFMT_YVU9:
//	case IMGFMT_IF09:
	case IMGFMT_Y8:
	case IMGFMT_Y800:
	case IMGFMT_444P:
	    return vf_next_query_format(vf, fmt,w,h);
    }
    return 0;
}

static MPXP_Rc __FASTCALL__ vf_open(vf_instance_t *vf,const char* args){
    vf->config_vf=vf_config;
    vf->put_slice=put_slice;
    vf->query_format=query_format;
    vf->priv=new(zeromem) vf_priv_t;
    vf->priv->direction=args?atoi(args):0;
    if(!(vf->priv->direction==0 || vf->priv->direction==90 ||
       vf->priv->direction==180 || vf->priv->direction==270)) {
	mpxp_err<<"[vf_rotate] can rotate on 0, 90, 180, 270 degrees only"<<std::endl;
	return MPXP_False;
    }
    if(vf->priv->direction==0) {
	/* passthrough mode */
	vf->put_slice=vf_next_put_slice;
	vf->query_format=vf_next_query_format;
	vf->config_vf=vf_next_config;
	mpxp_info<<"[vf_rotate] passthrough mode"<<std::endl;
    }
    check_pin("vfilter",vf->pin,VF_PIN);
    return MPXP_Ok;
}

extern const vf_info_t vf_info_rotate = {
    "rotate",
    "rotate",
    "A'rpi",
    "",
    VF_FLAGS_THREADS,
    vf_open
};

//===========================================================================//
