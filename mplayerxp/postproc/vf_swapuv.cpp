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

#include "mp_config.h"

#include "libvo/img_format.h"
#include "xmpcore/mp_image.h"
#include "vf.h"
#include "osdep/fastmemcpy.h"
#include "osdep/mplib.h"
#include "pp_msg.h"

using namespace mpxp;
//===========================================================================//

static void __FASTCALL__ get_image(struct vf_instance_s* vf, mp_image_t *mpi){
    mp_image_t *dmpi= vf_get_new_genome(vf->next,mpi);

    mpi->planes[0]=dmpi->planes[0];
    mpi->planes[1]=dmpi->planes[2];
    mpi->planes[2]=dmpi->planes[1];
    mpi->stride[0]=dmpi->stride[0];
    mpi->stride[1]=dmpi->stride[2];
    mpi->stride[2]=dmpi->stride[1];
    mpi->width=dmpi->width;
#if 0
    mpi->flags|=MP_IMGFLAG_DIRECT;
#endif
    mpi->priv=(any_t*)dmpi;
}

static int __FASTCALL__ put_slice(struct vf_instance_s* vf, mp_image_t *mpi){
    mp_image_t *dmpi;

    if(mpi->flags&MP_IMGFLAG_DIRECT){
	dmpi=(mp_image_t*)mpi->priv;
    } else {
	dmpi=vf_get_new_exportable_genome(vf->next, MP_IMGTYPE_EXPORT, 0, mpi);
	assert(mpi->flags&MP_IMGFLAG_PLANAR);
	dmpi->planes[0]=mpi->planes[0];
	dmpi->planes[1]=mpi->planes[2];
	dmpi->planes[2]=mpi->planes[1];
	dmpi->stride[0]=mpi->stride[0];
	dmpi->stride[1]=mpi->stride[2];
	dmpi->stride[2]=mpi->stride[1];
	dmpi->width=mpi->width;
    }

    vf_clone_mpi_attributes(dmpi, mpi);
    return vf_next_put_slice(vf,dmpi);
}

//===========================================================================//

static int __FASTCALL__ query_format(struct vf_instance_s* vf, unsigned int fmt,unsigned w,unsigned h){
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
