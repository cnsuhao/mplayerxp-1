#define OSD_SUPPORT

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mp_config.h"

#include "libvo/img_format.h"
#include "xmpcore/mp_image.h"
#include "vf.h"

#include "osdep/fastmemcpy.h"
#include "osdep/mplib.h"

#ifdef OSD_SUPPORT
#include "libvo/video_out.h"
#include "libvo/sub.h"
#include "libvo/osd.h"
#endif
#include "pp_msg.h"

#define MAX(a,b) ((a) > (b) ? (a) : (b))

extern vo_data_t* vo_data;

struct vf_priv_s {
    int up_h,dn_h;
    unsigned w,h;
    int new_frame;
};

//===========================================================================//

static int __FASTCALL__ vf_config(struct vf_instance_s* vf,
	int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt){
    unsigned h,dh;
    vf->priv->w=width;
    vf->priv->h=height;
    if(vf->priv->dn_h==-1 || vf->priv->up_h==-1)
	vf->priv->dn_h=vf->priv->up_h=(get_osd_height(vo_data,OSD_PB_START,0)*4)/3;
    h=height+vf->priv->up_h+vf->priv->dn_h;
    dh=d_height+vf->priv->up_h+vf->priv->dn_h;
    return vf_next_config(vf,width,h,d_width,dh,flags,outfmt);
}

static int __FASTCALL__ put_slice(struct vf_instance_s* vf, mp_image_t *mpi){
    int finalize;
    mp_image_t *dmpi;
    if(vf->priv->new_frame)
    {
	vf->priv->new_frame = 0;
	dmpi=vf_get_new_temp_genome(vf->next,mpi);
	vf_mpi_clear(dmpi,0,0,vf->priv->w,vf->priv->up_h);
	dmpi=vf_get_new_image(vf->next,mpi->imgfmt,
	    MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE,
	    vf->priv->w,vf->priv->up_h+vf->priv->h+vf->priv->dn_h,mpi->xp_idx);
	vf_mpi_clear(dmpi,0,0,vf->priv->w,vf->priv->dn_h);
    }
    dmpi=vf_get_new_image(vf->next,mpi->imgfmt,
	    MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE,
	    mpi->w,mpi->h+vf->priv->up_h,mpi->xp_idx);
    finalize = dmpi->flags&MP_IMGFLAG_FINALIZED;
    // copy mpi->dmpi...
    if(mpi->flags&MP_IMGFLAG_PLANAR){
	if(finalize) {
	stream_copy_pic(dmpi->planes[0],
		mpi->planes[0], mpi->w, mpi->h,
		dmpi->stride[0],mpi->stride[0]);
	stream_copy_pic(dmpi->planes[1],
		mpi->planes[1], mpi->chroma_width, mpi->chroma_height,
		dmpi->stride[1],mpi->stride[1]);
	stream_copy_pic(dmpi->planes[2],
		mpi->planes[2], mpi->chroma_width, mpi->chroma_height,
		dmpi->stride[2],mpi->stride[2]);
	} else {
	memcpy_pic(dmpi->planes[0],
		mpi->planes[0], mpi->w, mpi->h,
		dmpi->stride[0],mpi->stride[0]);
	memcpy_pic(dmpi->planes[1],
		mpi->planes[1], mpi->chroma_width, mpi->chroma_height,
		dmpi->stride[1],mpi->stride[1]);
	memcpy_pic(dmpi->planes[2],
		mpi->planes[2], mpi->chroma_width, mpi->chroma_height,
		dmpi->stride[2],mpi->stride[2]);
	}
    } else {
	if(finalize) {
	stream_copy_pic(dmpi->planes[0],
		mpi->planes[0], mpi->w*(dmpi->bpp/8), mpi->h,
		dmpi->stride[0],mpi->stride[0]);
	} else {
	memcpy_pic(dmpi->planes[0],
		mpi->planes[0], mpi->w*(dmpi->bpp/8), mpi->h,
		dmpi->stride[0],mpi->stride[0]);
	}
	dmpi->planes[1] = mpi->planes[1]; // passthrough rgb8 palette
    }
    return vf_next_put_slice(vf,dmpi);
}

//===========================================================================//

static MPXP_Rc __FASTCALL__ control(struct vf_instance_s* vf, int request, any_t* data){
    switch(request){
    case VFCTRL_START_FRAME:
	vf->priv->new_frame=1;
	break;
    }
    return vf_next_control(vf,request,data);
}

static int __FASTCALL__ query_format(struct vf_instance_s* vf, unsigned int fmt,unsigned w,unsigned h){
    return vf_next_query_format(vf->next,fmt,w,h);
}

static MPXP_Rc __FASTCALL__ vf_open(vf_instance_t *vf,const char* args){
    vf->config=vf_config;
    vf->control=control;
    vf->query_format=query_format;
    vf->put_slice=put_slice;
    if(!vf->priv) vf->priv=new(zeromem) struct vf_priv_s;
    if(args) sscanf(args,"%d:%d",&vf->priv->up_h,&vf->priv->dn_h);
    else     vf->priv->up_h=vf->priv->dn_h=-1;
    check_pin("vfilter",vf->pin,VF_PIN);
    return MPXP_Ok;
}

extern const vf_info_t vf_info_expand = {
    "expanding for osd",
    "expand",
    "A'rpi",
    "",
    VF_FLAGS_THREADS,
    vf_open
};

//===========================================================================//
