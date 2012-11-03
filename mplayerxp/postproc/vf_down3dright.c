#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "mp_config.h"
#include "osdep/cpudetect.h"

#include "libvo/img_format.h"
#include "xmpcore/mp_image.h"
#include "vf.h"

#include "osdep/fastmemcpy.h"
#include "osdep/mplib.h"
#include "postproc/swscale.h"
#include "pp_msg.h"

struct vf_priv_s {
	int skipline;
	int scalew;
	int scaleh;
};

static void __FASTCALL__ toright(unsigned char *dst[3], unsigned char *src[3],
		    unsigned int dststride[3], unsigned int srcstride[3],
		    int w, int h, struct vf_priv_s* p,int finalize)
{
	int k;

#ifdef _OPENMP
#pragma omp parallel for
#endif
	for (k = 0; k < 3; k++) {
		unsigned char* fromL = src[k];
		unsigned char* fromR = src[k];
		unsigned char* to = dst[k];
		unsigned int src = srcstride[k];
                unsigned int dst = dststride[k];
		unsigned int ss;
		unsigned int dd;
		int i;

		if (k > 0) {
			i = h / 4 - p->skipline / 2;
			ss = src * (h / 4 + p->skipline / 2);
			dd = w / 4;
		} else {
			i = h / 2 - p->skipline;
                        ss = src * (h / 2 + p->skipline);
			dd = w / 2;
		}
		fromR += ss;
		for ( ; i > 0; i--) {
                        int j;
			unsigned char* t = to;
			unsigned char* sL = fromL;
			unsigned char* sR = fromR;

			if (p->scalew == 1) {
				for (j = dd; j > 0; j--) {
					*t++ = (sL[0] + sL[1]) / 2;
					sL+=2;
				}
				for (j = dd ; j > 0; j--) {
					*t++ = (sR[0] + sR[1]) / 2;
					sR+=2;
				}
			} else {
				for (j = dd * 2 ; j > 0; j--)
					*t++ = *sL++;
				for (j = dd * 2 ; j > 0; j--)
					*t++ = *sR++;
			}
			if (p->scaleh == 1) {
			    if(finalize)
				stream_copy(to + dst, to, dst);
			    else
				memcpy(to + dst, to, dst);
                                to += dst;
			}
			to += dst;
			fromL += src;
			fromR += src;
		}
		//printf("K %d  %d   %d   %d  %d \n", k, w, h,  src, dst);
	}
}

static int __FASTCALL__ put_slice(struct vf_instance_s* vf, mp_image_t *mpi)
{
	mp_image_t *dmpi;
	int finalize;
	// hope we'll get DR buffer:
	dmpi=vf_get_image(vf->next, IMGFMT_YV12,
			  MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE,
			  mpi->w * vf->priv->scalew,
			  mpi->h / vf->priv->scaleh - vf->priv->skipline,mpi->xp_idx);
	finalize = dmpi->flags&MP_IMGFLAG_FINALIZED;

	toright(dmpi->planes, mpi->planes, dmpi->stride,
		mpi->stride, mpi->w, mpi->h, vf->priv,finalize);

	return vf_next_put_slice(vf,dmpi);
}

static int __FASTCALL__ config(struct vf_instance_s* vf,
		  int width, int height, int d_width, int d_height,
		  unsigned int flags, unsigned int outfmt,any_t*tune)
{
	/* FIXME - also support UYVY output? */
	return vf_next_config(vf, width * vf->priv->scalew,
			      height / vf->priv->scaleh - vf->priv->skipline, d_width, d_height, flags, IMGFMT_YV12, tune);
}


static int __FASTCALL__ query_format(struct vf_instance_s* vf, unsigned int fmt,unsigned w,unsigned h)
{
    /* FIXME - really any YUV 4:2:0 input format should work */
    switch (fmt) {
	case IMGFMT_YV12:
	case IMGFMT_IYUV:
	case IMGFMT_I420:
	    return vf_next_query_format(vf, IMGFMT_YV12,w,h);
    }
    return 0;
}

static void __FASTCALL__ uninit(struct vf_instance_s* vf)
{
    mp_free(vf->priv);
}

static ControlCodes __FASTCALL__ vf_open(vf_instance_t *vf,const char* args)
{
    vf->config=config;
    vf->query_format=query_format;
    vf->put_slice=put_slice;
    vf->uninit=uninit;

    vf->priv = mp_calloc(1, sizeof (struct vf_priv_s));
    vf->priv->skipline = 0;
    vf->priv->scalew = 1;
    vf->priv->scaleh = 2;
    if (args) sscanf(args, "%d:%d:%d", &vf->priv->skipline, &vf->priv->scalew, &vf->priv->scaleh);

    return CONTROL_OK;
}

const vf_info_t vf_info_down3dright = {
    "convert stereo movie from top-bottom to left-right field",
    "down3dright",
    "Zdenek Kabelac",
    "",
    VF_FLAGS_THREADS,
    vf_open
};

