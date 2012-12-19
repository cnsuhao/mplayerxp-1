#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "libvo2/img_format.h"
#include "xmpcore/xmp_image.h"
#include "vf.h"
#include "vf_internal.h"
#include "pp_msg.h"

struct vf_priv_t {
    int csp;
};

//===========================================================================//

static int __FASTCALL__ vf_config(vf_instance_t* vf,
	int width, int height, int d_width, int d_height,
	vo_flags_e flags, unsigned int outfmt){
    return vf_next_config(vf, width, height, d_width, d_height, flags, outfmt);
}

static inline int clamp_y(int x){
    return (x > 235) ? 235 : (x < 16) ? 16 : x;
}

static inline int clamp_c(int x){
    return (x > 240) ? 240 : (x < 16) ? 16 : x;
}

static int __FASTCALL__ put_slice(vf_instance_t* vf, mp_image_t *mpi){
    unsigned i,j,y,x;
    uint8_t *y_in, *cb_in, *cr_in;
    uint8_t *y_out, *cb_out, *cr_out;

    vf->dmpi=vf_get_new_temp_genome(vf->next,mpi);

    y_in = mpi->planes[0];
    cb_in = mpi->planes[1];
    cr_in = mpi->planes[2];

    y_out = vf->dmpi->planes[0];
    cb_out = vf->dmpi->planes[1];
    cr_out = vf->dmpi->planes[2];

    for (i = 0; i < mpi->h; i++)
	for (j = mpi->x; j < mpi->w; j++) {
	    y = i+mpi->y;
	    x = j+mpi->x;
	    y_out[y*vf->dmpi->stride[0]+x] = clamp_y(y_in[y*mpi->stride[0]+x]);
	}
    for (i = 0; i < mpi->chroma_height; i++)
	for (j = (mpi->x)>>(mpi->chroma_x_shift); j < mpi->chroma_width; j++)
	{
	    y=i+(mpi->y>>mpi->chroma_y_shift);
	    x=j+(mpi->y>>mpi->chroma_x_shift);
	    cb_out[y*vf->dmpi->stride[1]+x] = clamp_c(cb_in[y*mpi->stride[1]+x]);
	    cr_out[y*vf->dmpi->stride[2]+x] = clamp_c(cr_in[y*mpi->stride[2]+x]);
	}

    return vf_next_put_slice(vf,vf->dmpi);
}

//===========================================================================//

static void __FASTCALL__ uninit(vf_instance_t* vf){
	delete vf->priv;
}

static int __FASTCALL__ query_format(vf_instance_t* vf, unsigned int fmt,unsigned w,unsigned h){
    switch(fmt){
	case IMGFMT_YV12:
	case IMGFMT_I420:
	case IMGFMT_IYUV:
	    return 1;
    }
    return 0;
}

static MPXP_Rc __FASTCALL__ vf_open(vf_instance_t *vf,const char* args){
    vf->config_vf=vf_config;
    vf->uninit=uninit;
    vf->put_slice=put_slice;
//    vf->uninit=uninit;
    vf->query_format=query_format;
//    vf->priv=mp_calloc(1, sizeof(vf_priv_t));
//    if (args)
//	vf->priv->csp = atoi(args);
    check_pin("vfilter",vf->pin,VF_PIN);
    return MPXP_Ok;
}

extern const vf_info_t vf_info_yuvcsp = {
    "yuv colorspace converter",
    "yuvcsp",
    "Alex Beregszaszi",
    "",
    VF_FLAGS_THREADS|VF_FLAGS_SLICES,
    vf_open
};

//===========================================================================//
