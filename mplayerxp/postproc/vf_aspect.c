#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "../mp_config.h"

#include "../libvo/img_format.h"
#include "mp_image.h"
#include "vf.h"
#include "pp_msg.h"

struct vf_priv_s {
	int w, h;
	float aspect;
};

static int __FASTCALL__ config(struct vf_instance_s* vf,
	int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt,void *tune)
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
	return vf_next_config(vf, width, height, d_width, d_height, flags, outfmt, tune);
}

static int __FASTCALL__ vf_open(vf_instance_t *vf,const char* args)
{
	vf->config = config;
	vf->put_slice = vf_next_put_slice;
	//vf->default_caps = 0;
	vf->priv = calloc(sizeof(struct vf_priv_s), 1);
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
	return 1;
}

const vf_info_t vf_info_aspect = {
    "reset displaysize/aspect",
    "aspect",
    "Rich Felker",
    "",
    VF_FLAGS_THREADS,
    vf_open
};

