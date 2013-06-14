#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xmpcore/xmp_image.h"
#include "vf.h"
#include "vf_internal.h"

#include "osdep/fastmemcpy.h"
#include "pp_msg.h"

struct vf_priv_t {
    int x, y, w, h;
};

static int __FASTCALL__ vf_config(vf_instance_t* vf,
       int width, int height, int d_width, int d_height,
       vo_flags_e flags, unsigned int outfmt)
{
    if (vf->priv->w < 0 || width < vf->priv->w)
	vf->priv->w = width;
    if (vf->priv->h < 0 || height < vf->priv->h)
	vf->priv->h = height;
    if (vf->priv->x < 0)
	vf->priv->x = (width - vf->priv->w) / 2;
    if (vf->priv->y < 0)
	vf->priv->y = (height - vf->priv->h) / 2;
    if (vf->priv->w + vf->priv->x > width
	|| vf->priv->h + vf->priv->y > height) {
	mpxp_warn<<"rectangle: bad position/width/height - rectangle area is out of the original!"<<std::endl;
	return 0;
    }
    return vf_next_config(vf, width, height, d_width, d_height, flags, outfmt);
}

static MPXP_Rc __FASTCALL__ control_vf(vf_instance_t* vf, int request, any_t*data)
{
    const int *const tmp = reinterpret_cast<int*>(data);
    switch(request){
    case VFCTRL_CHANGE_RECTANGLE:
	switch (tmp[0]){
	case 0:
	    vf->priv->w += tmp[1];
	    return MPXP_Ok;
	    break;
	case 1:
	    vf->priv->h += tmp[1];
	    return MPXP_Ok;
	    break;
	case 2:
	    vf->priv->x += tmp[1];
	    return MPXP_Ok;
	    break;
	case 3:
	    vf->priv->y += tmp[1];
	    return MPXP_Ok;
	    break;
	default:
	    mpxp_fatal<<"Unknown param "<<tmp[0]<<std::endl;
	    return MPXP_False;
	}
    }
    return vf_next_control(vf, request, data);
}

static int __FASTCALL__ put_slice(vf_instance_t* vf,const mp_image_t& smpi){
    mp_image_t* dmpi;
    int finalize;
    unsigned int bpp = smpi.bpp / 8;
    unsigned int x, y, w, h;
    dmpi = vf_get_new_exportable_genome(vf->next, MP_IMGTYPE_TEMP,MP_IMGFLAG_ACCEPT_STRIDE | MP_IMGFLAG_PREFER_ALIGNED_STRIDE, smpi);
    finalize = dmpi->flags&MP_IMGFLAG_FINALIZED;

    if(finalize)
    stream_copy_pic(dmpi->planes[0],smpi.planes[0],smpi.w*bpp, smpi.h,
		    dmpi->stride[0],smpi.stride[0]);
    else
    memcpy_pic(dmpi->planes[0],smpi.planes[0],smpi.w*bpp, smpi.h,
	       dmpi->stride[0],smpi.stride[0]);
    if(smpi.flags&MP_IMGFLAG_PLANAR && smpi.flags&MP_IMGFLAG_YUV){
	if(finalize) {
	stream_copy_pic(dmpi->planes[1],smpi.planes[1],
		   smpi.w>>smpi.chroma_x_shift, smpi.h>>smpi.chroma_y_shift,
		   dmpi->stride[1],smpi.stride[1]);
	stream_copy_pic(dmpi->planes[2],smpi.planes[2],
		   smpi.w>>smpi.chroma_x_shift, smpi.h>>smpi.chroma_y_shift,
		   dmpi->stride[2],smpi.stride[2]);
	} else {
	memcpy_pic(dmpi->planes[1],smpi.planes[1],
		   smpi.w>>smpi.chroma_x_shift, smpi.h>>smpi.chroma_y_shift,
		   dmpi->stride[1],smpi.stride[1]);
	memcpy_pic(dmpi->planes[2],smpi.planes[2],
		   smpi.w>>smpi.chroma_x_shift, smpi.h>>smpi.chroma_y_shift,
		   dmpi->stride[2],smpi.stride[2]);
	}
    }

    /* Draw the rectangle */

    mpxp_dbg2<<"rectangle: -vf rectangle="<<vf->priv->w<<":"<<vf->priv->h<<":"<<vf->priv->x<<":"<<vf->priv->y<<std::endl;

    if (vf->priv->x < 0)
	x = 0;
    else if (dmpi->width < vf->priv->x)
	x = dmpi->width;
    else
	x = vf->priv->x;
    if (vf->priv->x + vf->priv->w - 1 < 0)
	w = vf->priv->x + vf->priv->w - 1 - x;
    else if (dmpi->width < vf->priv->x + vf->priv->w - 1)
	w = dmpi->width - x;
    else
	w = vf->priv->x + vf->priv->w - 1 - x;
    if (vf->priv->y < 0)
	y = 0;
    else if (dmpi->height < vf->priv->y)
	y = dmpi->height;
    else
	y = vf->priv->y;
    if (vf->priv->y + vf->priv->h - 1 < 0)
	h = vf->priv->y + vf->priv->h - 1 - y;
    else if (dmpi->height < vf->priv->y + vf->priv->h - 1)
	h = dmpi->height - y;
    else
	h = vf->priv->y + vf->priv->h - 1 - y;

    if (0 <= vf->priv->y && vf->priv->y <= dmpi->height) {
	unsigned char *p = dmpi->planes[0] + y * dmpi->stride[0] + x * bpp;
	unsigned int count = w * bpp;
	while (count--)
	    p[count] = 0xff - p[count];
    }
    if (h != 1 && vf->priv->y + vf->priv->h - 1 <= smpi.height) {
	unsigned char *p = dmpi->planes[0] + (vf->priv->y + vf->priv->h - 1) * dmpi->stride[0] + x * bpp;
	unsigned int count = w * bpp;
	while (count--)
	    p[count] = 0xff - p[count];
    }
    if (0 <= vf->priv->x  && vf->priv->x <= dmpi->width) {
	unsigned char *p = dmpi->planes[0] + y * dmpi->stride[0] + x * bpp;
	unsigned int count = h;
	while (count--) {
	    unsigned int i = bpp;
	    while (i--)
		p[i] = 0xff - p[i];
	    p += dmpi->stride[0];
	}
    }
    if (w != 1 && vf->priv->x + vf->priv->w - 1 <= smpi.width) {
	unsigned char *p = dmpi->planes[0] + y * dmpi->stride[0] + (vf->priv->x + vf->priv->w - 1) * bpp;
	unsigned int count = h;
	while (count--) {
	    unsigned int i = bpp;
	    while (i--)
		p[i] = 0xff - p[i];
	    p += dmpi->stride[0];
	}
    }
    return vf_next_put_slice(vf,*dmpi);
}

static MPXP_Rc __FASTCALL__ vf_open(vf_instance_t* vf,const char* args) {
    vf->config_vf = vf_config;
    vf->control_vf = control_vf;
    vf->put_slice = put_slice;
    vf->priv = new(zeromem) vf_priv_t;
    vf->priv->x = -1;
    vf->priv->y = -1;
    vf->priv->w = -1;
    vf->priv->h = -1;
    if (args)
	sscanf(args, "%d:%d:%d:%d",
	       &vf->priv->w, &vf->priv->h, &vf->priv->x, &vf->priv->y);
    check_pin("vfilter",vf->pin,VF_PIN);
    return MPXP_Ok;
}

extern const vf_info_t vf_info_rectangle = {
    "draw rectangle",
    "rectangle",
    "Kim Minh Kaplan",
    "",
    VF_FLAGS_THREADS,
    vf_open
};
