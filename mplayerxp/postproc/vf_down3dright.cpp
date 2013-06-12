#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "osdep/cpudetect.h"

#include "libvo2/img_format.h"
#include "xmpcore/xmp_image.h"
#include "vf.h"
#include "vf_internal.h"

#include "osdep/fastmemcpy.h"
#include "postproc/swscale.h"
#include "pp_msg.h"

struct vf_priv_t {
	int skipline;
	int scalew;
	int scaleh;
};

static void __FASTCALL__ toright(unsigned char *dst[3],const unsigned char* const src[3],
		    unsigned int dststride[3], unsigned int const srcstride[3],
		    int w, int h, vf_priv_t* p,int finalize)
{
	int k;

#ifdef _OPENMP
#pragma omp parallel for
#endif
	for (k = 0; k < 3; k++) {
		const unsigned char* fromL = src[k];
		const unsigned char* fromR = src[k];
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
			const unsigned char* sL = fromL;
			const unsigned char* sR = fromR;

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

static int __FASTCALL__ put_slice(vf_instance_t* vf,const mp_image_t& smpi)
{
	mp_image_t *dmpi;
	int finalize;
	// hope we'll get DR buffer:
	dmpi=vf_get_new_image(vf->next, IMGFMT_YV12,
			  MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE,
			  smpi.w * vf->priv->scalew,
			  smpi.h / vf->priv->scaleh - vf->priv->skipline,smpi.xp_idx);
	finalize = dmpi->flags&MP_IMGFLAG_FINALIZED;

	toright(dmpi->planes, smpi.planes, dmpi->stride,
		smpi.stride, smpi.w, smpi.h, vf->priv,finalize);

	return vf_next_put_slice(vf,*dmpi);
}

static int __FASTCALL__ vf_config(vf_instance_t* vf,
		  int width, int height, int d_width, int d_height,
		  vo_flags_e flags, unsigned int outfmt)
{
	/* FIXME - also support UYVY output? */
	return vf_next_config(vf, width * vf->priv->scalew,
			      height / vf->priv->scaleh - vf->priv->skipline, d_width, d_height, flags, IMGFMT_YV12);
}


static int __FASTCALL__ query_format(vf_instance_t* vf, unsigned int fmt,unsigned w,unsigned h)
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

static void __FASTCALL__ uninit(vf_instance_t* vf)
{
    delete vf->priv;
}

static MPXP_Rc __FASTCALL__ vf_open(vf_instance_t *vf,const char* args)
{
    vf->config_vf=vf_config;
    vf->query_format=query_format;
    vf->put_slice=put_slice;
    vf->uninit=uninit;

    vf->priv = new(zeromem) vf_priv_t;
    vf->priv->skipline = 0;
    vf->priv->scalew = 1;
    vf->priv->scaleh = 2;
    if (args) sscanf(args, "%d:%d:%d", &vf->priv->skipline, &vf->priv->scalew, &vf->priv->scaleh);
    check_pin("vfilter",vf->pin,VF_PIN);
    return MPXP_Ok;
}

extern const vf_info_t vf_info_down3dright = {
    "convert stereo movie from top-bottom to left-right field",
    "down3dright",
    "Zdenek Kabelac",
    "",
    VF_FLAGS_THREADS,
    vf_open
};

