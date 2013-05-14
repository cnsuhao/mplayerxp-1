#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
/*
  This video filter exports the incoming signal to raw file
  TODO: add length + pts to export into sockets
*/
#include <iostream>
#include <fstream>

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
    std::ofstream out;
};

//===========================================================================//
static int __FASTCALL__ vf_config(vf_instance_t* vf,
	int width, int height, int d_width, int d_height,
	vo_flags_e flags, unsigned int outfmt){
    return vf_next_config(vf,width,height,d_width,d_height,flags,outfmt);
}

static int __FASTCALL__ put_slice(vf_instance_t* vf, mp_image_t *mpi){
    mp_image_t *dmpi;

    // hope we'll get DR buffer:
    dmpi=vf_get_new_temp_genome(vf->next,mpi);
    if(mpi->flags&MP_IMGFLAG_PLANAR){
	    vf->priv->out.write((char*)(mpi->planes[0]),mpi->stride[0]*mpi->h);
	    vf->priv->out.write((char*)(mpi->planes[1]),mpi->stride[1]*mpi->h>>mpi->chroma_y_shift);
	    vf->priv->out.write((char*)(mpi->planes[2]),mpi->stride[2]*mpi->h>>mpi->chroma_y_shift);
	    MSG_V("[vf_raw] dumping %lu bytes\n"
	    ,mpi->stride[0]*mpi->h+
	    (((mpi->stride[1]+mpi->stride[2])*mpi->h)>>mpi->chroma_y_shift));
    } else {
	    vf->priv->out.write((char*)(mpi->planes[0]),mpi->stride[0]*mpi->h);
	    MSG_V("[vf_raw] dumping %lu bytes\n",mpi->stride[0]*mpi->h);
    }
    return vf_next_put_slice(vf,dmpi);
}

//===========================================================================//
static void __FASTCALL__ uninit(vf_instance_t* vf)
{
    vf_priv_t* s = vf->priv;
    s->out.close();
    delete s;
}
static MPXP_Rc __FASTCALL__ vf_open(vf_instance_t *vf,const char* args){
    vf->config_vf=vf_config;
    vf->put_slice=put_slice;
    vf->uninit=uninit;
    vf_priv_t* priv=new(zeromem) vf_priv_t;
    vf->priv=priv;
    priv->out.open(args?args:"1.raw",std::ios_base::out|std::ios_base::binary);
    if(!priv->out.is_open()) { delete priv; return MPXP_False; }
    check_pin("vfilter",vf->pin,VF_PIN);
    return MPXP_Ok;
}

extern const vf_info_t vf_info_raw = {
    "raw data output filter",
    "raw",
    "Nickols_K",
    "",
    VF_FLAGS_THREADS,
    vf_open
};

//===========================================================================//
