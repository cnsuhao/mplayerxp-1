#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mp_config.h"

#include "libvo/img_format.h"
#include "xmpcore/mp_image.h"
#include "vf.h"

#include "osdep/fastmemcpy.h"
#include "osdep/mplib.h"
#include "libvo/sub.h"
#include "pp_msg.h"

struct vf_priv_s {
	int state;
	long long in;
	long long out;
};

static inline any_t*my_memcpy_pic(any_t* dst, any_t* src, int bytesPerLine, int height, int dstStride, int srcStride, int finalize)
{
	int i;
	any_t*retval=dst;

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

static int __FASTCALL__ put_slice(struct vf_instance_s* vf, mp_image_t *mpi)
{
	mp_image_t *dmpi;
	int ret = 0,finalize;
	int flags = mpi->fields;
	int state = vf->priv->state;

	dmpi = vf_get_new_exportable_genome(vf->next, MP_IMGTYPE_STATIC, MP_IMGFLAG_ACCEPT_STRIDE | MP_IMGFLAG_PRESERVE, mpi);

	finalize = dmpi->flags&MP_IMGFLAG_FINALIZED;
	vf->priv->in++;

	if ((state == 0 &&
	     !(flags & MP_IMGFIELD_TOP_FIRST)) ||
	    (state == 1 &&
	     flags & MP_IMGFIELD_TOP_FIRST)) {
		MSG_WARN(
		       "softpulldown: Unexpected field flags: state=%d top_field_first=%d repeat_first_field=%d\n",
		       state,
		       (flags & MP_IMGFIELD_TOP_FIRST) != 0,
		       (flags & MP_IMGFIELD_REPEAT_FIRST) != 0);
		state ^= 1;
	}

	if (state == 0) {
		ret = vf_next_put_slice(vf, mpi);
		vf->priv->out++;
		if (flags & MP_IMGFIELD_REPEAT_FIRST) {
			my_memcpy_pic(dmpi->planes[0],
				   mpi->planes[0], mpi->w, mpi->h/2,
				   dmpi->stride[0]*2, mpi->stride[0]*2,finalize);
			if (mpi->flags & MP_IMGFLAG_PLANAR) {
				my_memcpy_pic(dmpi->planes[1],
					      mpi->planes[1],
					      mpi->chroma_width,
					      mpi->chroma_height/2,
					      dmpi->stride[1]*2,
					      mpi->stride[1]*2,finalize);
				my_memcpy_pic(dmpi->planes[2],
					      mpi->planes[2],
					      mpi->chroma_width,
					      mpi->chroma_height/2,
					      dmpi->stride[2]*2,
					      mpi->stride[2]*2,finalize);
			}
			state=1;
		}
	} else {
		my_memcpy_pic(dmpi->planes[0]+dmpi->stride[0],
			      mpi->planes[0]+mpi->stride[0], mpi->w, mpi->h/2,
			      dmpi->stride[0]*2, mpi->stride[0]*2,finalize);
		if (mpi->flags & MP_IMGFLAG_PLANAR) {
			my_memcpy_pic(dmpi->planes[1]+dmpi->stride[1],
				      mpi->planes[1]+mpi->stride[1],
				      mpi->chroma_width, mpi->chroma_height/2,
				      dmpi->stride[1]*2, mpi->stride[1]*2,finalize);
			my_memcpy_pic(dmpi->planes[2]+dmpi->stride[2],
				      mpi->planes[2]+mpi->stride[2],
				      mpi->chroma_width, mpi->chroma_height/2,
				      dmpi->stride[2]*2, mpi->stride[2]*2,finalize);
		}
		ret = vf_next_put_slice(vf, dmpi);
		vf->priv->out++;
		if (flags & MP_IMGFIELD_REPEAT_FIRST) {
			ret |= vf_next_put_slice(vf, mpi);
			vf->priv->out++;
			state=0;
		} else {
			my_memcpy_pic(dmpi->planes[0],
				      mpi->planes[0], mpi->w, mpi->h/2,
				      dmpi->stride[0]*2, mpi->stride[0]*2,finalize);
			if (mpi->flags & MP_IMGFLAG_PLANAR) {
				my_memcpy_pic(dmpi->planes[1],
					      mpi->planes[1],
					      mpi->chroma_width,
					      mpi->chroma_height/2,
					      dmpi->stride[1]*2,
					      mpi->stride[1]*2,finalize);
				my_memcpy_pic(dmpi->planes[2],
					      mpi->planes[2],
					      mpi->chroma_width,
					      mpi->chroma_height/2,
					      dmpi->stride[2]*2,
					      mpi->stride[2]*2,finalize);
			}
		}
	}

	vf->priv->state = state;

	return ret;
}

static int __FASTCALL__ config(struct vf_instance_s* vf,
	int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt)
{
	return vf_next_config(vf,width,height,d_width,d_height,flags,outfmt);
}

static void __FASTCALL__ uninit(struct vf_instance_s* vf)
{
	MSG_INFO( "softpulldown: %lld frames in, %lld frames out\n", vf->priv->in, vf->priv->out);
	mp_free(vf->priv);
}

static MPXP_Rc __FASTCALL__ vf_open(vf_instance_t *vf,const char* args)
{
    struct vf_priv_s *p;
    vf->config = config;
    vf->put_slice = put_slice;
    vf->uninit = uninit;
    vf->default_reqs = VFCAP_ACCEPT_STRIDE;
    vf->priv = p = mp_calloc(1, sizeof(struct vf_priv_s));
    vf->priv->state = 0;
    return MPXP_Ok;
}

const vf_info_t vf_info_softpulldown = {
    "mpeg2 soft 3:2 pulldown",
    "softpulldown",
    "Tobias Diedrich",
    "",
    VF_FLAGS_THREADS,
    vf_open
};
