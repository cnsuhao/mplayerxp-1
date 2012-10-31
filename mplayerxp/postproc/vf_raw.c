/*
  This video filter exports the incoming signal to raw file
  TODO: add length + pts to export into sockets
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "mp_config.h"

#include "libvo/img_format.h"
#include "mp_image.h"
#include "vf.h"

#include "osdep/fastmemcpy.h"
#include "osdep/mplib.h"
#include "postproc/swscale.h"
#include "pp_msg.h"

struct vf_priv_s {
    FILE *out;
};

//===========================================================================//
static int __FASTCALL__ config(struct vf_instance_s* vf,
        int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt,any_t*tune){
    return vf_next_config(vf,width,height,d_width,d_height,flags,outfmt,tune);
}

static int __FASTCALL__ put_slice(struct vf_instance_s* vf, mp_image_t *mpi){
    mp_image_t *dmpi;

    // hope we'll get DR buffer:
    dmpi=vf_get_image(vf->next,mpi->imgfmt,
	MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE,
	mpi->w,mpi->h,mpi->xp_idx);
    if(mpi->flags&MP_IMGFLAG_PLANAR){
	    fwrite(mpi->planes[0],mpi->stride[0],mpi->h,vf->priv->out);
	    fwrite(mpi->planes[1],mpi->stride[1],mpi->h>>mpi->chroma_y_shift,vf->priv->out);
	    fwrite(mpi->planes[2],mpi->stride[2],mpi->h>>mpi->chroma_y_shift,vf->priv->out);
	    MSG_V("[vf_raw] dumping %lu bytes\n"
	    ,mpi->stride[0]*mpi->h+
	    (((mpi->stride[1]+mpi->stride[2])*mpi->h)>>mpi->chroma_y_shift));
    } else {
	    fwrite(mpi->planes[0],mpi->stride[0],mpi->h,vf->priv->out);
	    MSG_V("[vf_raw] dumping %lu bytes\n",mpi->stride[0]*mpi->h);
    }
    return vf_next_put_slice(vf,dmpi);
}

//===========================================================================//
static void __FASTCALL__ uninit(struct vf_instance_s* vf)
{
    fclose(vf->priv->out);
    mp_free(vf->priv);
}
static ControlCodes __FASTCALL__ vf_open(vf_instance_t *vf,const char* args){
    vf->config=config;
    vf->put_slice=put_slice;
    vf->uninit=uninit;
    vf->priv=mp_malloc(sizeof(struct vf_priv_s));
    if(!(vf->priv->out=fopen(args?args:"1.raw","wb"))) { mp_free(vf->priv); return CONTROL_FALSE; }
    return CONTROL_OK;
}

const vf_info_t vf_info_raw = {
    "raw data output filter",
    "raw",
    "Nickols_K",
    "",
    VF_FLAGS_THREADS,
    vf_open
};

//===========================================================================//
