/*
 * This file is part of MPlayer.
 *
 * MPlayer is mp_free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mp_config.h"
#include "pp_msg.h"

#include "xmpcore/mp_image.h"
#include "vf.h"

#include "libvo/video_out.h"

//===========================================================================//

static int __FASTCALL__ config(struct vf_instance_s *vf,
        int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt){
    flags&=~VOFLAG_FLIPPING; // remove the FLIP flag
    return vf_next_config(vf,width,height,d_width,d_height,flags,outfmt);
}

static int __FASTCALL__ put_slice(struct vf_instance_s *vf, mp_image_t *mpi){
    if(mpi->flags&MP_IMGFLAG_DIRECT){
	// we've used DR, so we're ready...
	if(!(mpi->flags&MP_IMGFLAG_PLANAR))
	    ((mp_image_t*)mpi->priv)->planes[1] = mpi->planes[1]; // passthrough rgb8 palette
	return vf_next_put_slice(vf,(mp_image_t*)mpi->priv);
    }

    vf->dmpi=vf_get_new_genome(vf->next,MP_IMGTYPE_EXPORT, MP_IMGFLAG_ACCEPT_STRIDE,mpi);

    // set up mpi as a upside-down image of dmpi:
    vf->dmpi->planes[0]=mpi->planes[0]+
		    mpi->stride[0]*(mpi->height-1);
    vf->dmpi->stride[0]=-mpi->stride[0];
    if(vf->dmpi->flags&MP_IMGFLAG_PLANAR){
        vf->dmpi->planes[1]=mpi->planes[1]+
	    mpi->stride[1]*((mpi->height>>mpi->chroma_y_shift)-1);
	vf->dmpi->stride[1]=-mpi->stride[1];
	vf->dmpi->planes[2]=mpi->planes[2]+
	    mpi->stride[2]*((mpi->height>>mpi->chroma_y_shift)-1);
	vf->dmpi->stride[2]=-mpi->stride[2];
    } else
	vf->dmpi->planes[1]=mpi->planes[1]; // passthru bgr8 palette!!!

    return vf_next_put_slice(vf,vf->dmpi);
}

static int __FASTCALL__ query_format(struct vf_instance_s* vf, unsigned int fmt,unsigned w,unsigned h){
    return vf_next_query_format(vf,fmt,w,h);
}

//===========================================================================//

static MPXP_Rc __FASTCALL__ vf_open(vf_instance_t *vf, char *args){
    vf->config=config;
//    vf->get_image=get_image;
    vf->put_slice=put_slice;
    vf->query_format=query_format;
//    vf->default_reqs=VFCAP_ACCEPT_STRIDE;
    return MPXP_Ok;
}

const vf_info_t vf_info_flip = {
    "flip image upside-down",
    "flip",
    "A'rpi",
    "",
    VF_FLAGS_THREADS,
    vf_open,
    NULL
};

//===========================================================================//
