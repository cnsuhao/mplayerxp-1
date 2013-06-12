#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
/*
    Copyright (C) 2002 Michael Niedermayer <michaelni@gmx.at>

    This program is mp_free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>

#include "libvo2/img_format.h"
#include "xmpcore/xmp_image.h"
#include "vf.h"
#include "vf_internal.h"
#include "osdep/fastmemcpy.h"
#include "pp_msg.h"
//===========================================================================//

static void __FASTCALL__ get_image(vf_instance_t* vf, mp_image_t *smpi){
    mp_image_t *dmpi= vf_get_new_genome(vf->next,*smpi);

    smpi->planes[0]=dmpi->planes[0];
    smpi->planes[1]=dmpi->planes[2];
    smpi->planes[2]=dmpi->planes[1];
    smpi->stride[0]=dmpi->stride[0];
    smpi->stride[1]=dmpi->stride[2];
    smpi->stride[2]=dmpi->stride[1];
    smpi->width=dmpi->width;
#if 0
    smpi->flags|=MP_IMGFLAG_DIRECT;
#endif
    smpi->priv=dmpi;
}

static int __FASTCALL__ put_slice(vf_instance_t* vf,const mp_image_t& smpi){
    mp_image_t *dmpi;

    if(smpi.flags&MP_IMGFLAG_DIRECT){
	dmpi=(mp_image_t*)smpi.priv;
    } else {
	dmpi=vf_get_new_exportable_genome(vf->next, MP_IMGTYPE_EXPORT, 0, smpi);
	assert(smpi.flags&MP_IMGFLAG_PLANAR);
	dmpi->planes[0]=smpi.planes[0];
	dmpi->planes[1]=smpi.planes[2];
	dmpi->planes[2]=smpi.planes[1];
	dmpi->stride[0]=smpi.stride[0];
	dmpi->stride[1]=smpi.stride[2];
	dmpi->stride[2]=smpi.stride[1];
	dmpi->width=smpi.width;
    }

    vf_clone_mpi_attributes(dmpi,smpi);
    return vf_next_put_slice(vf,*dmpi);
}

//===========================================================================//

static int __FASTCALL__ query_format(vf_instance_t* vf, unsigned int fmt,unsigned w,unsigned h){
	switch(fmt)
	{
	case IMGFMT_YV12:
	case IMGFMT_I420:
	case IMGFMT_IYUV:
	case IMGFMT_YVU9:
	case IMGFMT_444P:
	case IMGFMT_422P:
	case IMGFMT_411P:
		return vf_next_query_format(vf, fmt,w,h);
	}
	return 0;
}

static MPXP_Rc __FASTCALL__ vf_open(vf_instance_t *vf,const char* args){
    vf->put_slice=put_slice;
    vf->get_image=get_image;
    vf->query_format=query_format;
    check_pin("vfilter",vf->pin,VF_PIN);
    return MPXP_Ok;
}

extern const vf_info_t vf_info_swapuv = {
    "UV swaper",
    "swapuv",
    "Michael Niedermayer",
    "",
    VF_FLAGS_THREADS|VF_FLAGS_SLICES,
    vf_open
};

//===========================================================================//
