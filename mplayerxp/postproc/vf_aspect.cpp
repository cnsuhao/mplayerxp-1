#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "libvo2/img_format.h"
#include "xmpcore/mp_image.h"
#include "vf.h"
#include "vf_internal.h"
#include "pp_msg.h"

struct vf_priv_t {
	int w, h;
	float aspect;
};

static int __FASTCALL__ vf_config(vf_instance_t* vf,
	int width, int height, int d_width, int d_height,
	vo_flags_e flags, unsigned int outfmt)
{
	if(vf->priv->aspect==768.)
	{
	    d_height=vf->priv->aspect*d_height/d_width;
	    d_width=width; // do X-scaling by hardware
	}
	else if (vf->priv->w && vf->priv->h) {
		d_width = vf->priv->w;
		d_height = vf->priv->h;
	} else {
		if (vf->priv->aspect * height > width) {
			d_width = height * vf->priv->aspect;
			d_height = height;
		} else {
			d_height = width / vf->priv->aspect;
			d_width = width;
		}
	}
	return vf_next_config(vf, width, height, d_width, d_height, flags, outfmt);
}

static MPXP_Rc __FASTCALL__ vf_open(vf_instance_t *vf,const char* args)
{
	vf->config_vf = vf_config;
	vf->put_slice = vf_next_put_slice;
	//vf->default_caps = 0;
	vf->priv = new(zeromem) vf_priv_t;
	vf->priv->aspect = 4.0/3.0;
	if (args) {
		if (strcmp(args,"dvb") == 0) vf->priv->aspect = 768.;
		else if (strchr(args, '/')) {
			int w, h;
			sscanf(args, "%d/%d", &w, &h);
			vf->priv->aspect = (float)w/h;
		} else if (strchr(args, '.')) {
			sscanf(args, "%f", &vf->priv->aspect);
		} else {
			sscanf(args, "%d:%d", &vf->priv->w, &vf->priv->h);
		}
	}
    check_pin("vfilter",vf->pin,VF_PIN);
    return MPXP_Ok;
}

extern const vf_info_t vf_info_aspect = {
    "reset displaysize/aspect",
    "aspect",
    "Rich Felker",
    "",
    VF_FLAGS_THREADS|VF_FLAGS_SLICES,
    vf_open
};

