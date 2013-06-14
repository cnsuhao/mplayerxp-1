#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libvo2/img_format.h"
#include "xmpcore/xmp_image.h"
#include "vf.h"
#include "vf_internal.h"

#include "osdep/fastmemcpy.h"
#include "libvo2/sub.h"
#include "pp_msg.h"

struct vf_priv_t {
	int state;
	long long in;
	long long out;
};

static inline any_t*my_memcpy_pic(any_t* _dst,const any_t* _src, int bytesPerLine, int height, int dstStride, int srcStride, int finalize)
{
	int i;
	uint8_t* dst=(uint8_t*)_dst;
	const uint8_t* src = (const uint8_t*)_src;
	any_t* retval=dst;

	for(i=0; i<height; i++)
	{
	    if(finalize)
		stream_copy(dst, src, bytesPerLine);
	    else
		memcpy(dst, src, bytesPerLine);
		src+= srcStride;
		dst+= dstStride;
	}

	return retval;
}

static int __FASTCALL__ put_slice(vf_instance_t* vf,const mp_image_t& smpi)
{
	mp_image_t *dmpi;
	int ret = 0,finalize;
	int flags = smpi.fields;
	int state = vf->priv->state;

	dmpi = vf_get_new_exportable_genome(vf->next, MP_IMGTYPE_STATIC, MP_IMGFLAG_ACCEPT_STRIDE | MP_IMGFLAG_PRESERVE, smpi);

	finalize = dmpi->flags&MP_IMGFLAG_FINALIZED;
	vf->priv->in++;

	if ((state == 0 &&
	     !(flags & MP_IMGFIELD_TOP_FIRST)) ||
	    (state == 1 &&
	     flags & MP_IMGFIELD_TOP_FIRST)) {
		mpxp_warn<<"softpulldown: Unexpected field flags: state="<<state<<" top_field_first="
		    <<((flags & MP_IMGFIELD_TOP_FIRST) != 0)<<" repeat_first_field="<<((flags & MP_IMGFIELD_REPEAT_FIRST) != 0)<<std::endl;
		state ^= 1;
	}

	if (state == 0) {
		ret = vf_next_put_slice(vf, smpi);
		vf->priv->out++;
		if (flags & MP_IMGFIELD_REPEAT_FIRST) {
			my_memcpy_pic(dmpi->planes[0],
				   smpi.planes[0], smpi.w, smpi.h/2,
				   dmpi->stride[0]*2, smpi.stride[0]*2,finalize);
			if (smpi.flags & MP_IMGFLAG_PLANAR) {
				my_memcpy_pic(dmpi->planes[1],
					      smpi.planes[1],
					      smpi.chroma_width,
					      smpi.chroma_height/2,
					      dmpi->stride[1]*2,
					      smpi.stride[1]*2,finalize);
				my_memcpy_pic(dmpi->planes[2],
					      smpi.planes[2],
					      smpi.chroma_width,
					      smpi.chroma_height/2,
					      dmpi->stride[2]*2,
					      smpi.stride[2]*2,finalize);
			}
			state=1;
		}
	} else {
		my_memcpy_pic(dmpi->planes[0]+dmpi->stride[0],
			      smpi.planes[0]+smpi.stride[0], smpi.w, smpi.h/2,
			      dmpi->stride[0]*2, smpi.stride[0]*2,finalize);
		if (smpi.flags & MP_IMGFLAG_PLANAR) {
			my_memcpy_pic(dmpi->planes[1]+dmpi->stride[1],
				      smpi.planes[1]+smpi.stride[1],
				      smpi.chroma_width, smpi.chroma_height/2,
				      dmpi->stride[1]*2, smpi.stride[1]*2,finalize);
			my_memcpy_pic(dmpi->planes[2]+dmpi->stride[2],
				      smpi.planes[2]+smpi.stride[2],
				      smpi.chroma_width, smpi.chroma_height/2,
				      dmpi->stride[2]*2, smpi.stride[2]*2,finalize);
		}
		ret = vf_next_put_slice(vf,*dmpi);
		vf->priv->out++;
		if (flags & MP_IMGFIELD_REPEAT_FIRST) {
			ret |= vf_next_put_slice(vf, smpi);
			vf->priv->out++;
			state=0;
		} else {
			my_memcpy_pic(dmpi->planes[0],
				      smpi.planes[0], smpi.w, smpi.h/2,
				      dmpi->stride[0]*2, smpi.stride[0]*2,finalize);
			if (smpi.flags & MP_IMGFLAG_PLANAR) {
				my_memcpy_pic(dmpi->planes[1],
					      smpi.planes[1],
					      smpi.chroma_width,
					      smpi.chroma_height/2,
					      dmpi->stride[1]*2,
					      smpi.stride[1]*2,finalize);
				my_memcpy_pic(dmpi->planes[2],
					      smpi.planes[2],
					      smpi.chroma_width,
					      smpi.chroma_height/2,
					      dmpi->stride[2]*2,
					      smpi.stride[2]*2,finalize);
			}
		}
	}

	vf->priv->state = state;

	return ret;
}

static int __FASTCALL__ vf_config(vf_instance_t* vf,
	int width, int height, int d_width, int d_height,
	vo_flags_e flags, unsigned int outfmt)
{
    return vf_next_config(vf,width,height,d_width,d_height,flags,outfmt);
}

static void __FASTCALL__ uninit(vf_instance_t* vf)
{
    mpxp_info<<"softpulldown: "<<vf->priv->in<<" frames in, "<<vf->priv->out<<" frames out"<<std::endl;
    delete vf->priv;
}

static MPXP_Rc __FASTCALL__ vf_open(vf_instance_t *vf,const char* args)
{
    vf_priv_t *p;
    vf->config_vf = vf_config;
    vf->put_slice = put_slice;
    vf->uninit = uninit;
    vf->default_reqs = VFCAP_ACCEPT_STRIDE;
    vf->priv = p = new(zeromem) vf_priv_t;
    vf->priv->state = 0;
    check_pin("vfilter",vf->pin,VF_PIN);
    return MPXP_Ok;
}

extern const vf_info_t vf_info_softpulldown = {
    "mpeg2 soft 3:2 pulldown",
    "softpulldown",
    "Tobias Diedrich",
    "",
    VF_FLAGS_THREADS,
    vf_open
};
