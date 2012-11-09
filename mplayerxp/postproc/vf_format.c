#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "mp_config.h"

#include "libvo/img_format.h"
#include "xmpcore/mp_image.h"
#include "vf.h"
#include "pp_msg.h"
#include "osdep/mplib.h"

struct vf_priv_s {
    unsigned int fmt;
};
//===========================================================================//

static int __FASTCALL__ query_format(struct vf_instance_s* vf, unsigned int fmt,unsigned w,unsigned h){
    MSG_DBG2("[vf_format] request for %s limited by %s\n",vo_format_name(fmt),vo_format_name(vf->priv->fmt));
    if(fmt==vf->priv->fmt)
	return vf_next_query_format(vf,fmt,w,h);
    return 0;
}

static int __FASTCALL__ query_noformat(struct vf_instance_s* vf, unsigned int fmt,unsigned w,unsigned h){
    MSG_DBG2("[vf_noformat] request for %s limited by %s\n",vo_format_name(fmt),vo_format_name(vf->priv->fmt));
    if(fmt!=vf->priv->fmt)
	return vf_next_query_format(vf,fmt,w,h);
    return 0;
}

static int __FASTCALL__ config(struct vf_instance_s* vf,
        int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt,any_t*tune){
    return vf_next_config(vf,width,height,d_width,d_height,flags,outfmt,tune);
}

static uint32_t get_format(const char *args)
{
	if(!strcasecmp(args,"444p")) return IMGFMT_444P; else
	if(!strcasecmp(args,"422p")) return IMGFMT_422P; else
	if(!strcasecmp(args,"411p")) return IMGFMT_411P; else
	if(!strcasecmp(args,"yuy2")) return IMGFMT_YUY2; else
	if(!strcasecmp(args,"yv12")) return IMGFMT_YV12; else
	if(!strcasecmp(args,"i420")) return IMGFMT_I420; else
	if(!strcasecmp(args,"yvu9")) return IMGFMT_YVU9; else
	if(!strcasecmp(args,"if09")) return IMGFMT_IF09; else
	if(!strcasecmp(args,"iyuv")) return IMGFMT_IYUV; else
	if(!strcasecmp(args,"uyvy")) return IMGFMT_UYVY; else
	if(!strcasecmp(args,"bgr24")) return IMGFMT_BGR24; else
	if(!strcasecmp(args,"bgr32")) return IMGFMT_BGR32; else
	if(!strcasecmp(args,"bgr16")) return IMGFMT_BGR16; else
	if(!strcasecmp(args,"bgr15")) return IMGFMT_BGR15; else
	if(!strcasecmp(args,"bgr8")) return IMGFMT_BGR8; else
	if(!strcasecmp(args,"bgr4")) return IMGFMT_BGR4; else
	if(!strcasecmp(args,"bg4b")) return IMGFMT_BG4B; else
	if(!strcasecmp(args,"bgr1")) return IMGFMT_BGR1; else
	if(!strcasecmp(args,"rgb24")) return IMGFMT_RGB24; else
	if(!strcasecmp(args,"rgb32")) return IMGFMT_RGB32; else
	if(!strcasecmp(args,"rgb16")) return IMGFMT_RGB16; else
	if(!strcasecmp(args,"rgb15")) return IMGFMT_RGB15; else
	if(!strcasecmp(args,"rgb8")) return IMGFMT_RGB8; else
	if(!strcasecmp(args,"rgb4")) return IMGFMT_RGB4; else
	if(!strcasecmp(args,"rg4b")) return IMGFMT_RG4B; else
	if(!strcasecmp(args,"rgb1")) return IMGFMT_RGB1; else
	if(!strcasecmp(args,"rgba")) return IMGFMT_RGBA; else
	if(!strcasecmp(args,"argb")) return IMGFMT_ARGB; else
	if(!strcasecmp(args,"bgra")) return IMGFMT_BGRA; else
	if(!strcasecmp(args,"abgr")) return IMGFMT_ABGR; else
	return 0;
}

static MPXP_Rc __FASTCALL__ vf_open(vf_instance_t *vf,const char* args){
    vf->query_format=query_format;
    vf->config=config;
    vf->default_caps=0;
    if(!vf->priv) {
      vf->priv=mp_malloc(sizeof(struct vf_priv_s));
      vf->priv->fmt=IMGFMT_YUY2;
    }
    if(args){
	if(!(vf->priv->fmt=get_format(args))) {
	    printf("Unknown format name: '%s'\n",args);
	    return MPXP_False;
	}
    }
    return MPXP_Ok;
}

static MPXP_Rc __FASTCALL__ vf_no_open(vf_instance_t *vf,const char* args){
    vf->query_format=query_noformat;
    vf->config=config;
    vf->default_caps=0;
    if(!vf->priv) {
      vf->priv=mp_malloc(sizeof(struct vf_priv_s));
      vf->priv->fmt=IMGFMT_YUY2;
    }
    if(args){
	if(!(vf->priv->fmt=get_format(args))) {
	    printf("Unknown format name: '%s'\n",args);
	    return MPXP_False;
	}
    }
    return MPXP_Ok;
}

const vf_info_t vf_info_format = {
    "force output format",
    "format",
    "Nickols_K",
    "FIXME! get_image()/put_image()",
    VF_FLAGS_THREADS|VF_FLAGS_SLICES,
    vf_open
};

const vf_info_t vf_info_noformat = {
    "disable output format",
    "noformat",
    "A'rpi",
    "FIXME! get_image()/put_image()",
    VF_FLAGS_THREADS|VF_FLAGS_SLICES,
    vf_no_open
};

//===========================================================================//
