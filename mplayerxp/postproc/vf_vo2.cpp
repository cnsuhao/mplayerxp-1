#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xmpcore/xmp_image.h"
#include "vf.h"
#include "vf_internal.h"
#include "vfcap.h"
#include "libvo2/video_out.h"
#include "libvo2/dri_vo.h"
#include "pp_msg.h"
#include "mplayerxp.h" // mpxp_context().video().output

//===========================================================================//
struct vf_priv_t {
    int is_planar;
    int sw,sh,dw,dh,sflg;
    int ofmt;
    vo_format_desc vd;
};
static int vo_config_count;
static int __FASTCALL__ query_format(vf_instance_t* vf, unsigned int fmt,unsigned w,unsigned h); /* forward declaration */

static void __FASTCALL__ print_conf(vf_instance_t* vf)
{
    const vo_info_t *info = mpxp_context().video().output->get_info();
    mpxp_info<<"VO-CONF: ["<<info->short_name<<"] "
	    <<vf->priv->sw<<"x"<<vf->priv->sh
	    <<" => "<<vf->priv->dw<<"x"<<vf->priv->dh
	    <<" "<<vo_format_name(vf->priv->ofmt)
	    <<" "
	    <<((vf->priv->sflg&1)?" [fs]":"")
	    <<((vf->priv->sflg&2)?" [vm]":"")
	    <<((vf->priv->sflg&4)?" [zoom]":"")
	    <<((vf->priv->sflg&8)?" [flip]":"")<<std::endl;
    mpxp_v<<"VO: Description: "<<info->name<<std::endl;
    mpxp_v<<"VO: Author: "<<info->author<<std::endl;
    if(info->comment && strlen(info->comment) > 0)
	mpxp_v<<"VO: Comment: "<<info->comment<<std::endl;
}

static int __FASTCALL__ vf_config(vf_instance_t* vf,
	int width, int height, int d_width, int d_height,
	vo_flags_e flags, unsigned int outfmt){

    if ((width <= 0) || (height <= 0) || (d_width <= 0) || (d_height <= 0))
    {
	mpxp_err<<"VO: invalid dimensions!"<<std::endl;
	return 0;
    }

    vf->conf.w=width;
    vf->conf.h=height;
    vf->conf.fourcc=outfmt;
    vf->priv->ofmt=outfmt;
    vf->priv->sw=width;
    vf->priv->sh=height;
    vf->priv->dw=d_width;
    vf->priv->dh=d_height;
    vf->priv->sflg=flags;
    // save vo's stride capability for the wanted colorspace:
    vf->default_caps=query_format(vf,outfmt,d_width,d_height);// & VFCAP_ACCEPT_STRIDE;

    mpxp_dbg2<<"vf_vo2->config("<<width<<","<<height<<","<<d_width<<","<<d_height
	  <<","<<flags<<","<<vo_format_name(outfmt)<<")"<<std::endl;
    if(MPXP_Ok!=mpxp_context().video().output->configure(vf->parent,width,height,d_width,d_height,flags,"MPlayerXP",outfmt))
	return 0;
    vf->priv->is_planar=vo_describe_fourcc(outfmt,&vf->priv->vd);
    vf->conf.w=d_width;
    vf->conf.h=d_height;
    vf->conf.fourcc=outfmt;
    ++vo_config_count;
    return 1;
}

static MPXP_Rc __FASTCALL__ control_vf(vf_instance_t* vf, int request,any_t* data)
{
    UNUSED(vf);
    mpxp_dbg2<<"vf_control: "<<request<<std::endl;
    switch(request){
    case VFCTRL_SET_EQUALIZER: {
	vf_equalizer_t *eq=reinterpret_cast<vf_equalizer_t*>(data);
	if(!vo_config_count) return MPXP_False; // vo not configured?
	return mpxp_context().video().output->ctrl(VOCTRL_SET_EQUALIZER, eq);
    }
    case VFCTRL_GET_EQUALIZER: {
	vf_equalizer_t *eq=reinterpret_cast<vf_equalizer_t*>(data);
	if(!vo_config_count) return MPXP_False; // vo not configured?
	return mpxp_context().video().output->ctrl(VOCTRL_GET_EQUALIZER, eq);
    }
    }
    // return video_out->control_vf(request,data);
    return MPXP_Unknown;
}

static int __FASTCALL__ query_format(vf_instance_t* vf, unsigned int fmt,unsigned w,unsigned h){
    dri_surface_cap_t dcaps;
    int rflags;
    uint32_t flags=mpxp_context().video().output->query_format(&fmt,w,h);
    mpxp_dbg2<<"[vf_vo2] "<<flags<<"=query_format("<<vo_format_name(fmt)<<")"<<std::endl;
    rflags=0;
    UNUSED(vf);
    if(flags) {
	mpxp_context().video().output->get_surface_caps(&dcaps);
	if(dcaps.caps&DRI_CAP_UPSCALER) rflags |=VFCAP_HWSCALE_UP;
	if(dcaps.caps&DRI_CAP_DOWNSCALER) rflags |=VFCAP_HWSCALE_DOWN;
	if(rflags&(VFCAP_HWSCALE_UP|VFCAP_HWSCALE_DOWN)) rflags |= VFCAP_SWSCALE;
	if(dcaps.caps&DRI_CAP_HWOSD) rflags |=VFCAP_OSD;
	if(flags&0x1) rflags|= VFCAP_CSP_SUPPORTED;
	if(flags&0x2) rflags|= VFCAP_CSP_SUPPORTED_BY_HW;
    }
    return rflags;
}

static void __FASTCALL__ get_image(vf_instance_t* vf,
	mp_image_t *smpi){
    MPXP_Rc retval;
    UNUSED(vf);
    int finalize = mpxp_context().video().output->is_final();
    retval=mpxp_context().video().output->get_surface(smpi);
    if(retval==MPXP_Ok) {
	smpi->flags |= MP_IMGFLAG_FINAL|MP_IMGFLAG_DIRECT;
	if(finalize) smpi->flags |= MP_IMGFLAG_FINALIZED;
	mpxp_dbg2<<"vf_vo_get_image was called successfully"<<std::endl;
    } else mpxp_dbg2<<"vf_vo_get_image was called failed"<<std::endl;
}

static int __FASTCALL__ put_slice(vf_instance_t* vf,const mp_image_t& smpi){
    if(!vo_config_count) return 0; // vo not configured?
    if(!(smpi.flags & MP_IMGFLAG_FINAL) || (vf_first(vf)==vf && !(smpi.flags & MP_IMGFLAG_RENDERED))) {
	mpxp_dbg2<<"vf_vo_put_slice was called("
		<<smpi.xp_idx<<"): "<<smpi.x<<" "<<smpi.y<<" "<<smpi.w<<" "<<smpi.h<<std::endl;
	mpxp_context().video().output->draw_slice(smpi);
    }
    return 1;
}

static void __FASTCALL__ uninit( vf_instance_t* vf ) {
    delete vf->priv ;
    vf->priv = NULL;
}

//===========================================================================//

static MPXP_Rc __FASTCALL__ vf_open(vf_instance_t *vf,const char* args){
    UNUSED(args);
    vf->config_vf=vf_config;
    vf->control_vf=control_vf;
    vf->uninit=uninit;
    vf->print_conf=print_conf;
    vf->query_format=query_format;
    vf->get_image=get_image;
    vf->put_slice=put_slice;
    vf->priv = new(zeromem) vf_priv_t;
    check_pin("vfilter",vf->pin,VF_PIN);
    return MPXP_Ok;
}

extern const vf_info_t vf_info_vo2 = {
    "libvo2 wrapper",
    "vo2",
    "A'rpi",
    "for internal use",
    VF_FLAGS_THREADS|VF_FLAGS_SLICES,
    vf_open
};

//===========================================================================//
