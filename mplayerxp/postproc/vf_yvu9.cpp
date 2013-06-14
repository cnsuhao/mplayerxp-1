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
#include "mpxp_conf_lavc.h"
#include "pp_msg.h"
//===========================================================================//

static int __FASTCALL__ vf_config(vf_instance_t* vf,
	int width, int height, int d_width, int d_height,
	vo_flags_e flags, unsigned int outfmt){

    if(vf_next_query_format(vf,IMGFMT_YV12,d_width,d_height)<=0){
	mpxp_err<<"yv12 isn't supported by next filter/vo :("<<std::endl;
	return 0;
    }

    return vf_next_config(vf,width,height,d_width,d_height,flags,IMGFMT_YV12);
}

static int __FASTCALL__ put_slice(vf_instance_t* vf,const mp_image_t& smpi){
    mp_image_t *dmpi;
    int y,w,h,finalize;

    // hope we'll get DR buffer:
    dmpi=vf_get_new_image(vf->next,IMGFMT_YV12,
	MP_IMGTYPE_TEMP, 0/*MP_IMGFLAG_ACCEPT_STRIDE*/,
	smpi.w, smpi.h,smpi.xp_idx);
    finalize = dmpi->flags&MP_IMGFLAG_FINALIZED;

#ifdef _OPENMP
#pragma omp parallel sections
{
#pragma omp section
#endif
    for(y=0;y<smpi.h;y++) {
	if(finalize)
	stream_copy(dmpi->planes[0]+dmpi->stride[0]*y,
	       smpi.planes[0]+smpi.stride[0]*y,
	       smpi.w);
	else
	memcpy(dmpi->planes[0]+dmpi->stride[0]*y,
	       smpi.planes[0]+smpi.stride[0]*y,
	       smpi.w);
    }
#ifdef _OPENMP
#pragma omp section
#endif
    (*vu9_to_vu12)( smpi.planes[1], smpi.planes[2],
		    dmpi->planes[1], dmpi->planes[2],
		    w,h,smpi.stride[1],smpi.stride[2],
		    dmpi->stride[1],dmpi->stride[2]);
#ifdef _OPENMP
}
#endif
    vf_clone_mpi_attributes(dmpi, smpi);

    return vf_next_put_slice(vf,*dmpi);
}

//===========================================================================//

static int __FASTCALL__ query_format(vf_instance_t* vf, unsigned int fmt,unsigned w,unsigned h){
    if (fmt == IMGFMT_YVU9 || fmt == IMGFMT_IF09)
	return vf_next_query_format(vf,IMGFMT_YV12,w,h) & (~VFCAP_CSP_SUPPORTED_BY_HW);
    return 0;
}

static MPXP_Rc __FASTCALL__ vf_open(vf_instance_t *vf,const char* args){
    vf->config_vf=vf_config;
    vf->put_slice=put_slice;
    vf->query_format=query_format;
    check_pin("vfilter",vf->pin,VF_PIN);
    return MPXP_Ok;
}

extern const vf_info_t vf_info_yvu9 = {
    "fast YVU9->YV12 conversion",
    "yvu9",
    "alex",
    "",
    VF_FLAGS_THREADS,
    vf_open
};

//===========================================================================//
