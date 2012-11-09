#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>

#include "mp_config.h"
#include "osdep/cpudetect.h"
#include "osdep/mplib.h"

#include "libvo/img_format.h"
#include "xmpcore/mp_image.h"
#include "vf.h"

#include "postprocess.h"
#include "pp_msg.h"

struct vf_priv_s {
    int pp;
    pp_mode *ppMode[PP_QUALITY_MAX+1];
    any_t*context;
    unsigned int outfmt;
};

//===========================================================================//

static int __FASTCALL__ config(struct vf_instance_s* vf,
        int width, int height, int d_width, int d_height,
	unsigned int voflags, unsigned int outfmt,any_t*tune){
    int flags=
          (gCpuCaps.hasMMX   ? PP_CPU_CAPS_MMX   : 0)
	| (gCpuCaps.hasMMX2  ? PP_CPU_CAPS_MMX2  : 0)
	| (gCpuCaps.has3DNow ? PP_CPU_CAPS_3DNOW : 0);

    switch(outfmt){
    case IMGFMT_444P: flags|= PP_FORMAT_444; break;
    case IMGFMT_422P: flags|= PP_FORMAT_422; break;
    case IMGFMT_411P: flags|= PP_FORMAT_411; break;
    default:          flags|= PP_FORMAT_420; break;
    }

    if(vf->priv->context) pp_free_context(vf->priv->context);
    vf->priv->context= pp2_get_context(width, height, flags);

    return vf_next_config(vf,width,height,d_width,d_height,voflags,outfmt,tune);
}

static void __FASTCALL__ uninit(struct vf_instance_s* vf){
    int i;
    for(i=0; i<=PP_QUALITY_MAX; i++){
        if(vf->priv->ppMode[i])
	    pp_free_mode(vf->priv->ppMode[i]);
    }
    if(vf->priv->context) pp_free_context(vf->priv->context);
}

static int __FASTCALL__ query_format(struct vf_instance_s* vf, unsigned int fmt,unsigned w,unsigned h){
    switch(fmt){
    case IMGFMT_YV12:
    case IMGFMT_I420:
    case IMGFMT_IYUV:
    case IMGFMT_444P:
    case IMGFMT_422P:
    case IMGFMT_411P:
	return vf_next_query_format(vf,fmt,w,h);
    }
    return 0;
}

static MPXP_Rc __FASTCALL__ control(struct vf_instance_s* vf, int request, any_t* data){
    switch(request){
    case VFCTRL_QUERY_MAX_PP_LEVEL:
	return PP_QUALITY_MAX;
    case VFCTRL_SET_PP_LEVEL:
	vf->priv->pp= *((unsigned int*)data);
	return MPXP_True;
    }
    return vf_next_control(vf,request,data);
}

static void __FASTCALL__ get_image(struct vf_instance_s* vf, mp_image_t *mpi){
    if(vf->priv->pp&0xFFFF) return; // non-local filters enabled
    if((mpi->type==MP_IMGTYPE_IPB || vf->priv->pp) && 
	mpi->flags&MP_IMGFLAG_PRESERVE) return; // don't change
    if(!(mpi->flags&MP_IMGFLAG_ACCEPT_STRIDE) && mpi->imgfmt!=vf->priv->outfmt)
	return; // colorspace differ
    // ok, we can do pp in-place (or pp disabled):
    vf->dmpi=vf_get_image(vf->next,mpi->imgfmt,
        mpi->type, mpi->flags, mpi->w, mpi->h,mpi->xp_idx);
    mpi->planes[0]=vf->dmpi->planes[0];
    mpi->stride[0]=vf->dmpi->stride[0];
    mpi->width=vf->dmpi->width;
    if(mpi->flags&MP_IMGFLAG_PLANAR){
        mpi->planes[1]=vf->dmpi->planes[1];
        mpi->planes[2]=vf->dmpi->planes[2];
	mpi->stride[1]=vf->dmpi->stride[1];
	mpi->stride[2]=vf->dmpi->stride[2];
    }
    mpi->flags|=MP_IMGFLAG_DIRECT;
}

static int __FASTCALL__ put_slice(struct vf_instance_s* vf, mp_image_t *mpi){
    if(!(mpi->flags&MP_IMGFLAG_DIRECT)){
	// no DR, so get a new image! hope we'll get DR buffer:
	vf->dmpi=vf_get_image(vf->next,mpi->imgfmt,
	    MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE|MP_IMGFLAG_PREFER_ALIGNED_STRIDE,
	    (mpi->w+7)&(~7),(mpi->h+7)&(~7),mpi->xp_idx);
    }

    if(vf->priv->pp || !(mpi->flags&MP_IMGFLAG_DIRECT)){
	// do the postprocessing! (or copy if no DR)
	pp_postprocess(mpi->planes           ,mpi->stride,
		    vf->dmpi->planes,vf->dmpi->stride,
		    (mpi->w+7)&(~7),mpi->h,
		    mpi->qscale, mpi->qstride,
		    vf->priv->ppMode[ vf->priv->pp ], vf->priv->context,
#ifdef PP_PICT_TYPE_QP2
		    mpi->pict_type | (mpi->qscale_type ? PP_PICT_TYPE_QP2 : 0));
#else
		    mpi->pict_type);
#endif
    }
    return vf_next_put_slice(vf,vf->dmpi);
}

//===========================================================================//

extern int divx_quality;

static unsigned int fmt_list[]={
    IMGFMT_YV12,
    IMGFMT_I420,
    IMGFMT_IYUV,
    IMGFMT_444P,
    IMGFMT_422P,
    IMGFMT_411P,
    0
};

static MPXP_Rc __FASTCALL__ vf_open(vf_instance_t *vf,const char* args){
    char *endptr;
    const char* name;
    int i;
    int hex_mode=0;

    vf->query_format=query_format;
    vf->control=control;
    vf->config=config;
    vf->get_image=get_image;
    vf->put_slice=put_slice;
    vf->uninit=uninit;
    vf->default_caps=VFCAP_ACCEPT_STRIDE|VFCAP_POSTPROC;
    vf->priv=mp_malloc(sizeof(struct vf_priv_s));
    vf->priv->context=NULL;

    // check csp:
    vf->priv->outfmt=vf_match_csp(&vf->next,fmt_list,IMGFMT_YV12,1,1);
    if(!vf->priv->outfmt) return 0; // no csp match :(

    if(args){
	hex_mode= strtol(args, &endptr, 0);
	if(*endptr){
	    name= args;
	}else
	    name= NULL;
    }else{
	name="de";
    }

    for(i=0; i<=PP_QUALITY_MAX; i++){
	vf->priv->ppMode[i]= pp_get_mode_by_name_and_quality(name, i);
	if(vf->priv->ppMode[i]==NULL) return MPXP_Error;
    }

    vf->priv->pp=PP_QUALITY_MAX;
    return MPXP_Ok;
}

const vf_info_t vf_info_pp = {
    "postprocessing",
    "pp",
    "A'rpi",
    "",
    VF_FLAGS_THREADS,
    vf_open
};

//===========================================================================//
