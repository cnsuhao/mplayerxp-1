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

struct FilterParam{
	int interleave;
	int swap;
};

struct vf_priv_t {
	FilterParam lumaParam;
	FilterParam chromaParam;
};

/***************************************************************************/

static void __FASTCALL__ interleave(uint8_t *dst,const uint8_t *src, int w, int h, int dstStride, int srcStride, int interleave, int swap,int finalize){
	const int a= swap;
	const int b= 1-a;
	const int m= h>>1;
	int y;

	switch(interleave){
	case -1:
		for(y=0; y < m; y++){
		    if(finalize) {
			stream_copy(dst + dstStride* y     , src + srcStride*(y*2 + a), w);
			stream_copy(dst + dstStride*(y + m), src + srcStride*(y*2 + b), w);
		    }
		    else {
			memcpy(dst + dstStride* y     , src + srcStride*(y*2 + a), w);
			memcpy(dst + dstStride*(y + m), src + srcStride*(y*2 + b), w);
		    }
		}
		break;
	case 0:
		for(y=0; y < m; y++){
		    if(finalize) {
			stream_copy(dst + dstStride* y*2   , src + srcStride*(y*2 + a), w);
			stream_copy(dst + dstStride*(y*2+1), src + srcStride*(y*2 + b), w);
		    } else {
			memcpy(dst + dstStride* y*2   , src + srcStride*(y*2 + a), w);
			memcpy(dst + dstStride*(y*2+1), src + srcStride*(y*2 + b), w);
		    }
		}
		break;
	case 1:
		for(y=0; y < m; y++){
		    if(finalize) {
			stream_copy(dst + dstStride*(y*2+a), src + srcStride* y     , w);
			stream_copy(dst + dstStride*(y*2+b), src + srcStride*(y + m), w);
		    } else {
			memcpy(dst + dstStride*(y*2+a), src + srcStride* y     , w);
			memcpy(dst + dstStride*(y*2+b), src + srcStride*(y + m), w);
		    }
		}
		break;
	}
}

static int __FASTCALL__ put_slice(vf_instance_t* vf,const mp_image_t& smpi){
	int w,finalize;
	FilterParam *luma  = &vf->priv->lumaParam;
	FilterParam *chroma= &vf->priv->chromaParam;

	mp_image_t *dmpi=vf_get_new_temp_genome(vf->next,smpi);

	if(smpi.flags&MP_IMGFLAG_PLANAR)
		w= smpi.w;
	else
		w= smpi.w * smpi.bpp/8;
	finalize = dmpi->flags&MP_IMGFLAG_FINALIZED;

	interleave(dmpi->planes[0], smpi.planes[0],
		w, smpi.h, dmpi->stride[0], smpi.stride[0], luma->interleave, luma->swap,finalize);

	if(smpi.flags&MP_IMGFLAG_PLANAR){
		int cw= smpi.w >> smpi.chroma_x_shift;
		int ch= smpi.h >> smpi.chroma_y_shift;

		interleave(dmpi->planes[1], smpi.planes[1], cw,ch,
			dmpi->stride[1], smpi.stride[1], chroma->interleave, luma->swap,finalize);
		interleave(dmpi->planes[2], smpi.planes[2], cw,ch,
			dmpi->stride[2], smpi.stride[2], chroma->interleave, luma->swap,finalize);
	}

	return vf_next_put_slice(vf,*dmpi);
}

//===========================================================================//

static void __FASTCALL__ parse(FilterParam *fp,const char* args){
	const char *pos;
	const char *max= strchr(args, ':');

	if(!max) max= args + strlen(args);

	pos= strchr(args, 's');
	if(pos && pos<max) fp->swap=1;
	pos= strchr(args, 'i');
	if(pos && pos<max) fp->interleave=1;
	pos= strchr(args, 'd');
	if(pos && pos<max) fp->interleave=-1;
}

static MPXP_Rc __FASTCALL__ vf_open(vf_instance_t *vf,const char* args){

    vf->put_slice=put_slice;
    vf->priv=new(zeromem) vf_priv_t;

    if(args) {
	const char *arg2= strchr(args,':');
	if(arg2) parse(&vf->priv->chromaParam, arg2+1);
	parse(&vf->priv->lumaParam, args);
    }
    check_pin("vfilter",vf->pin,VF_PIN);
    return MPXP_Ok;
}

extern const vf_info_t vf_info_il = {
    "(de)interleave",
    "il",
    "Michael Niedermayer",
    "",
    VF_FLAGS_THREADS,
    vf_open
};

//===========================================================================//
