#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
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

#include "pp_msg.h"

#include "xmpcore/xmp_image.h"
#include "vf.h"
#include "vf_internal.h"

#include "libvo2/video_out.h"

//===========================================================================//

static int __FASTCALL__ vf_config(vf_instance_t *vf,
	int width, int height, int d_width, int d_height,
	vo_flags_e flags, unsigned int outfmt){
    flags&=~VOFLAG_FLIPPING; // remove the FLIP flag
    return vf_next_config(vf,width,height,d_width,d_height,flags,outfmt);
}

static int __FASTCALL__ put_slice(vf_instance_t *vf,const mp_image_t& smpi){
    if(smpi.flags&MP_IMGFLAG_DIRECT){
	// we've used DR, so we're ready...
	if(!(smpi.flags&MP_IMGFLAG_PLANAR))
	    ((mp_image_t*)smpi.priv)->planes[1] = smpi.planes[1]; // passthrough rgb8 palette
	return vf_next_put_slice(vf,*reinterpret_cast<const mp_image_t*>(smpi.priv));
    }

    vf->dmpi=vf_get_new_exportable_genome(vf->next,MP_IMGTYPE_EXPORT, MP_IMGFLAG_ACCEPT_STRIDE,smpi);

    // set up smpi as a upside-down image of dmpi:
    vf->dmpi->planes[0]=smpi.planes[0]+
		    smpi.stride[0]*(smpi.height-1);
    vf->dmpi->stride[0]=-smpi.stride[0];
    if(vf->dmpi->flags&MP_IMGFLAG_PLANAR){
	vf->dmpi->planes[1]=smpi.planes[1]+
	    smpi.stride[1]*((smpi.height>>smpi.chroma_y_shift)-1);
	vf->dmpi->stride[1]=-smpi.stride[1];
	vf->dmpi->planes[2]=smpi.planes[2]+
	    smpi.stride[2]*((smpi.height>>smpi.chroma_y_shift)-1);
	vf->dmpi->stride[2]=-smpi.stride[2];
    } else
	vf->dmpi->planes[1]=smpi.planes[1]; // passthru bgr8 palette!!!

    return vf_next_put_slice(vf,*vf->dmpi);
}

static int __FASTCALL__ query_format(vf_instance_t* vf, unsigned int fmt,unsigned w,unsigned h){
    return vf_next_query_format(vf,fmt,w,h);
}

//===========================================================================//

static MPXP_Rc __FASTCALL__ vf_open(vf_instance_t *vf,const char *args){
    vf->config_vf=vf_config;
//    vf->get_image=get_image;
    vf->put_slice=put_slice;
    vf->query_format=query_format;
//    vf->default_reqs=VFCAP_ACCEPT_STRIDE;
    check_pin("vfilter",vf->pin,VF_PIN);
    return MPXP_Ok;
}

extern const vf_info_t vf_info_flip = {
    "flip image upside-down",
    "flip",
    "A'rpi",
    "",
    VF_FLAGS_THREADS,
    vf_open
};

//===========================================================================//
