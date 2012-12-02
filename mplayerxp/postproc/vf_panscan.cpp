#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
#include <algorithm>

#define OSD_SUPPORT

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libvo/img_format.h"
#include "xmpcore/mp_image.h"
#include "vf.h"
#include "vf_internal.h"

#include "osdep/fastmemcpy.h"

#ifdef OSD_SUPPORT
#include "libvo/sub.h"
#endif
#include "pp_msg.h"

struct vf_priv_t {
    unsigned org_w;
    unsigned org_h;
    unsigned ps_x,ps_y,ps_w,ps_h;
    int fmt;
    float panscan;
};

//===========================================================================//

static int __FASTCALL__ vf_config(vf_instance_t* vf,
	int width, int height, int d_width, int d_height,
	vo_flags_e flags, unsigned int outfmt){
    unsigned w,h,d_w,d_h;
    vf->priv->org_w=width;
    vf->priv->org_h=height;
    if(vf->priv->panscan>=1.)
    {
	w=(float)width/vf->priv->panscan;
	d_w = (float)d_width/vf->priv->panscan;
	if(outfmt==IMGFMT_YV12 || outfmt==IMGFMT_I420)
	{
		w&=~3;
		d_w&=~3;
	}
	else
	if(outfmt==IMGFMT_YVU9 || outfmt==IMGFMT_IF09)
	{
		w&=~7;
		d_w&=~7;
	}
	vf->priv->ps_w=(float)width/vf->priv->panscan;
	vf->priv->ps_x=(vf->priv->org_w-vf->priv->ps_w)>>1;
	vf->priv->ps_h=h=height;
	vf->priv->ps_y=0;
	d_h = d_height;
    }
    else
    {
	h=(float)height*vf->priv->panscan;
	d_h = (float)d_height*vf->priv->panscan;
	if(outfmt==IMGFMT_YV12 || outfmt==IMGFMT_I420)
	{
		h&=~3;
		d_h&=~3;
	}
	else
	if(outfmt==IMGFMT_YVU9 || outfmt==IMGFMT_IF09)
	{
		h&=~7;
		d_h&=~7;
	}
	d_w = d_width;
	vf->priv->ps_w=w=width;
	vf->priv->ps_x=0;
	vf->priv->ps_h=h;
	vf->priv->ps_y=(vf->priv->org_h-vf->priv->ps_h)>>1;
    }
    vf->priv->fmt=outfmt;
    MSG_DBG2("w,h=%i %i org_w,h=%i %i\n",w,h,vf->priv->org_w,vf->priv->org_h);
    return vf_next_config(vf,w,h,d_w,d_h,flags,outfmt);
}


static int __FASTCALL__ put_slice(vf_instance_t* vf, mp_image_t *mpi){

    mp_image_t *dmpi;
    unsigned sx,sy,sw,sh;
    unsigned dx,dy,dw,dh;
    unsigned cw,ch;
    int finalize;
    dx=mpi->x; dy=mpi->y; dw=mpi->w; dh=mpi->h;
    sx=vf->priv->ps_x; sy=vf->priv->ps_y; sw=vf->priv->ps_w; sh=vf->priv->ps_h;
    MSG_DBG2("[vf_panscan] src: %i %i %i %i dst: %i %i %i %i\n",sx,sy,sw,sh,dx,dy,dw,dh);
    if(	(sy+sh < dy) || (sy>dy+dh) ||
	(sx+sw < dx) || (sx>dx+dw)) return 1; /* nothing todo */
    /* In hope that mpi->x === 0 */
    cw=std::min(sw,dw);
    ch=std::min(sh,dh);
    dmpi=vf_get_new_image(vf->next,mpi->imgfmt,
	    MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE,
	    cw,ch,mpi->xp_idx);
    finalize = dmpi->flags&MP_IMGFLAG_FINALIZED;
    if(sy < dy)		{ ch-=dy-sy; sy=dy; }
    if(sh > dh)		ch=sh-dh;
    if(sx < dx)		{ cw-=dx-sx; sx=dx; }
    if(sw > dw)		cw=sw-dw;

    if(mpi->flags&MP_IMGFLAG_PLANAR){
	if(finalize) {
	stream_copy_pic(
		dmpi->planes[0]+dx+dy*dmpi->stride[0],
		mpi->planes[0]+sx+sy*mpi->stride[0],
		cw, ch,
		dmpi->stride[0],mpi->stride[0]);
	stream_copy_pic(
		dmpi->planes[1]+(dx>>dmpi->chroma_x_shift)+(dy>>dmpi->chroma_y_shift)*dmpi->stride[1],
		mpi->planes[1]+(sx>>mpi->chroma_x_shift)+(sy>>mpi->chroma_y_shift)*mpi->stride[1],
		cw>>mpi->chroma_x_shift,
		ch>>mpi->chroma_y_shift,
		dmpi->stride[1],mpi->stride[1]);
	stream_copy_pic(
		dmpi->planes[2]+(dx>>dmpi->chroma_x_shift)+(dy>>dmpi->chroma_y_shift)*dmpi->stride[2],
		mpi->planes[2]+(sx>>mpi->chroma_x_shift)+(sy>>mpi->chroma_y_shift)*mpi->stride[2],
		cw>>mpi->chroma_x_shift,
		ch>>mpi->chroma_y_shift,
		dmpi->stride[2],mpi->stride[2]);
	} else {
	memcpy_pic(
		dmpi->planes[0]+dx+dy*dmpi->stride[0],
		mpi->planes[0]+sx+sy*mpi->stride[0],
		cw, ch,
		dmpi->stride[0],mpi->stride[0]);
	memcpy_pic(
		dmpi->planes[1]+(dx>>dmpi->chroma_x_shift)+(dy>>dmpi->chroma_y_shift)*dmpi->stride[1],
		mpi->planes[1]+(sx>>mpi->chroma_x_shift)+(sy>>mpi->chroma_y_shift)*mpi->stride[1],
		cw>>mpi->chroma_x_shift,
		ch>>mpi->chroma_y_shift,
		dmpi->stride[1],mpi->stride[1]);
	memcpy_pic(
		dmpi->planes[2]+(dx>>dmpi->chroma_x_shift)+(dy>>dmpi->chroma_y_shift)*dmpi->stride[2],
		mpi->planes[2]+(sx>>mpi->chroma_x_shift)+(sy>>mpi->chroma_y_shift)*mpi->stride[2],
		cw>>mpi->chroma_x_shift,
		ch>>mpi->chroma_y_shift,
		dmpi->stride[2],mpi->stride[2]);
	}
	memcpy_pic(
		dmpi->planes[1]+(dx>>dmpi->chroma_x_shift)+(dy>>dmpi->chroma_y_shift)*dmpi->stride[1],
		mpi->planes[1]+(sx>>mpi->chroma_x_shift)+(sy>>mpi->chroma_y_shift)*mpi->stride[1],
		cw>>mpi->chroma_x_shift,
		ch>>mpi->chroma_y_shift,
		dmpi->stride[1],mpi->stride[1]);
	memcpy_pic(
		dmpi->planes[2]+(dx>>dmpi->chroma_x_shift)+(dy>>dmpi->chroma_y_shift)*dmpi->stride[2],
		mpi->planes[2]+(sx>>mpi->chroma_x_shift)+(sy>>mpi->chroma_y_shift)*mpi->stride[2],
		cw>>mpi->chroma_x_shift,
		ch>>mpi->chroma_y_shift,
		dmpi->stride[2],mpi->stride[2]);

    } else {
	if(finalize)
	stream_copy_pic(dmpi->planes[0]+dx*((dmpi->bpp+7)/8)+dy*dmpi->stride[0],
		mpi->planes[0]+sx*((mpi->bpp+7)/8)+sy*mpi->stride[0],
		cw*((mpi->bpp+7)/8), ch,
		dmpi->stride[0],mpi->stride[0]);
	else
	memcpy_pic(dmpi->planes[0]+dx*((dmpi->bpp+7)/8)+dy*dmpi->stride[0],
		mpi->planes[0]+sx*((mpi->bpp+7)/8)+sy*mpi->stride[0],
		cw*((mpi->bpp+7)/8), ch,
		dmpi->stride[0],mpi->stride[0]);
	dmpi->planes[1] = mpi->planes[1]; // passthrough rgb8 palette
    }
    return vf_next_put_slice(vf,dmpi);
}

static void __FASTCALL__ print_conf(vf_instance_t* vf)
{
    MSG_INFO("[vf_panscan]: cropping [%dx%d] -> [%dx%d] [%s]\n",
	vf->priv->org_w,vf->priv->org_h,
	vf->priv->ps_w,vf->priv->ps_h,
	vo_format_name(vf->priv->fmt));
}


static MPXP_Rc __FASTCALL__ control_vf(vf_instance_t* vf, int request, any_t* data){
    return vf_next_control(vf,request,data);
}

static int __FASTCALL__ query_format(vf_instance_t* vf, unsigned int fmt,unsigned w,unsigned h){
    return vf_next_query_format(vf->next,fmt,w,h);
}

static MPXP_Rc __FASTCALL__ vf_open(vf_instance_t *vf,const char* args){
    vf->config_vf=vf_config;
    vf->control_vf=control_vf;
    vf->query_format=query_format;
    vf->put_slice=put_slice;
    vf->print_conf=print_conf;
    if(!vf->priv) vf->priv=new(zeromem) vf_priv_t;
    vf->priv->panscan=0;
    if(args) sscanf(args,"%f",&vf->priv->panscan);
    if(vf->priv->panscan<=0.) return MPXP_False;
    check_pin("vfilter",vf->pin,VF_PIN);
    return MPXP_Ok;
}

extern const vf_info_t vf_info_panscan = {
    "Panning and scan",
    "panscan",
    "Nickols_K",
    "",
    VF_FLAGS_THREADS,
    vf_open
};

//===========================================================================//
static int __FASTCALL__ crop_config(vf_instance_t* vf,
	int width, int height, int d_width, int d_height,
	vo_flags_e flags, unsigned int outfmt){
    unsigned w,h,d_w,d_h;
    vf->priv->org_w=width;
    vf->priv->org_h=height;
    if(vf->priv->ps_w<=0 || vf->priv->ps_w>width) vf->priv->ps_w=width;
    if(vf->priv->ps_h<=0 || vf->priv->ps_h>height) vf->priv->ps_h=height;
    if(vf->priv->ps_x<=0 || vf->priv->ps_x>width) vf->priv->ps_x=width;
    if(vf->priv->ps_y<=0 || vf->priv->ps_y>height) vf->priv->ps_y=height;
    w=vf->priv->ps_w;
    h=vf->priv->ps_h;
    if(outfmt==IMGFMT_YV12 || outfmt==IMGFMT_I420) {
	w&=~3;
	d_w&=~3;
    }
    else if(outfmt==IMGFMT_YVU9 || outfmt==IMGFMT_IF09) {
	w&=~7;
	d_w&=~7;
    }
    vf->priv->ps_w=w;
    vf->priv->ps_h=h;
    vf->priv->fmt=outfmt;
    // check:
    if(vf->priv->ps_w+vf->priv->ps_x>width ||
       vf->priv->ps_h+vf->priv->ps_y>height){
	MSG_WARN("Cropping position exceed image size\n");
	return 0;
    }
    d_w=(float)d_width*vf->priv->ps_w/width;
    d_h=(float)d_height*vf->priv->ps_h/height;
    return vf_next_config(vf,w,h,d_w,d_h,flags,outfmt);
}

static MPXP_Rc __FASTCALL__ vf_crop_open(vf_instance_t *vf,const char* args){
    vf->config_vf=crop_config;
    vf->control_vf=control_vf;
    vf->query_format=query_format;
    vf->put_slice=put_slice;
    vf->print_conf=print_conf;
    if(!vf->priv) vf->priv=new(zeromem) vf_priv_t;
    vf->priv->ps_x=
    vf->priv->ps_y=
    vf->priv->ps_w=
    vf->priv->ps_h=-1;
    if(args) sscanf(args,"%i,%i,%i,%i",&vf->priv->ps_x,&vf->priv->ps_y,&vf->priv->ps_w,&vf->priv->ps_h);
    check_pin("vfilter",vf->pin,VF_PIN);
    return MPXP_Ok;
}

extern const vf_info_t vf_info_crop = {
    "cropping (panscan like behaviour)",
    "crop",
    "Nickols_K",
    "",
    VF_FLAGS_THREADS,
    vf_crop_open
};

//===========================================================================//
