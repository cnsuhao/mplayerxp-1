#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xmpcore/xmp_core.h"
#include "mplayerxp.h"
#include "help_mp.h"

#include "libvo/img_format.h"
#include "libvo/video_out.h"
#include "xmpcore/mp_image.h"
#include "vf.h"
#include "vf_internal.h"

#include "osdep/fastmemcpy.h"
#include "libmpconf/codec-cfg.h"
#include "pp_msg.h"
#include "mplayerxp.h" // mpxp_context().video().output

extern const vf_info_t vf_info_1bpp;
extern const vf_info_t vf_info_2xsai;
extern const vf_info_t vf_info_aspect;
extern const vf_info_t vf_info_crop;
extern const vf_info_t vf_info_delogo;
extern const vf_info_t vf_info_denoise3d;
extern const vf_info_t vf_info_dint;
extern const vf_info_t vf_info_down3dright;
extern const vf_info_t vf_info_eq;
extern const vf_info_t vf_info_expand;
extern const vf_info_t vf_info_flip;
extern const vf_info_t vf_info_fmtcvt;
extern const vf_info_t vf_info_framestep;
extern const vf_info_t vf_info_format;
extern const vf_info_t vf_info_il;
extern const vf_info_t vf_info_menu;
extern const vf_info_t vf_info_mirror;
extern const vf_info_t vf_info_noformat;
extern const vf_info_t vf_info_noise;
extern const vf_info_t vf_info_ow;
extern const vf_info_t vf_info_palette;
extern const vf_info_t vf_info_panscan;
extern const vf_info_t vf_info_perspective;
extern const vf_info_t vf_info_pp;
extern const vf_info_t vf_info_raw;
extern const vf_info_t vf_info_rectangle;
extern const vf_info_t vf_info_rgb2bgr;
extern const vf_info_t vf_info_rotate;
extern const vf_info_t vf_info_smartblur;
extern const vf_info_t vf_info_scale;
extern const vf_info_t vf_info_softpulldown;
extern const vf_info_t vf_info_swapuv;
extern const vf_info_t vf_info_test;
extern const vf_info_t vf_info_unsharp;
extern const vf_info_t vf_info_vo;
extern const vf_info_t vf_info_yuvcsp;
extern const vf_info_t vf_info_yuy2;
extern const vf_info_t vf_info_yvu9;
extern const vf_info_t vf_info_null;

// list of available filters:
static const vf_info_t* filter_list[]={
    &vf_info_1bpp,
    &vf_info_2xsai,
    &vf_info_aspect,
    &vf_info_crop,
    &vf_info_delogo,
    &vf_info_denoise3d,
    &vf_info_dint,
    &vf_info_down3dright,
    &vf_info_eq,
    &vf_info_expand,
    &vf_info_flip,
    &vf_info_fmtcvt,
    &vf_info_format,
    &vf_info_framestep,
    &vf_info_il,
    &vf_info_menu,
    &vf_info_mirror,
    &vf_info_noformat,
    &vf_info_noise,
    &vf_info_ow,
    &vf_info_palette,
    &vf_info_panscan,
    &vf_info_perspective,
    &vf_info_pp,
    &vf_info_raw,
    &vf_info_rectangle,
    &vf_info_rgb2bgr,
    &vf_info_rotate,
    &vf_info_smartblur,
    &vf_info_scale,
    &vf_info_softpulldown,
    &vf_info_swapuv,
    &vf_info_test,
    &vf_info_unsharp,
    &vf_info_vo,
    &vf_info_yuvcsp,
    &vf_info_yuy2,
    &vf_info_yvu9,
    &vf_info_null,
    NULL
};

//============================================================================
// mpi stuff:

void __FASTCALL__ vf_mpi_clear(mp_image_t* mpi,int x0,int y0,int w,int h){
    int y;
    if(mpi->flags&MP_IMGFLAG_PLANAR){
	y0&=~1;h+=h&1;
	if(x0==0 && w==mpi->width){
	    // full width clear:
	    memset(mpi->planes[0]+mpi->stride[0]*y0,0x10,mpi->stride[0]*h);
	    memset(mpi->planes[1]+mpi->stride[1]*(y0>>mpi->chroma_y_shift),0x80,mpi->stride[1]*(h>>mpi->chroma_y_shift));
	    memset(mpi->planes[2]+mpi->stride[2]*(y0>>mpi->chroma_y_shift),0x80,mpi->stride[2]*(h>>mpi->chroma_y_shift));
	} else
	{
	    for(y=y0;y<y0+h;y++){
		memset(mpi->planes[0]+x0+mpi->stride[0]*y,0x10,w);
	    }
	    for(y=y0;y<y0+(h>>mpi->chroma_y_shift);y++){
		memset(mpi->planes[1]+(x0>>mpi->chroma_x_shift)+mpi->stride[1]*y,0x80,(w>>mpi->chroma_x_shift));
		memset(mpi->planes[2]+(x0>>mpi->chroma_x_shift)+mpi->stride[2]*y,0x80,(w>>mpi->chroma_x_shift));
	    }
	}
	return;
    }
    // packed:
    for(y=y0;y<y0+h;y++){
	unsigned char* dst=mpi->planes[0]+mpi->stride[0]*y+(mpi->bpp>>3)*x0;
	if(mpi->flags&MP_IMGFLAG_YUV){
	    unsigned int* p=(unsigned int*) dst;
	    int size=(mpi->bpp>>3)*w/4;
	    int i;
#ifdef WORDS_BIGENDIAN
#define CLEAR_PACKEDYUV_PATTERN 0x10801080
#define CLEAR_PACKEDYUV_PATTERN_SWAPPED 0x80108010
#else
#define CLEAR_PACKEDYUV_PATTERN 0x80108010
#define CLEAR_PACKEDYUV_PATTERN_SWAPPED 0x10801080
#endif
	    if(mpi->flags&MP_IMGFLAG_SWAPPED){
		for(i=0;i<size-3;i+=4) p[i]=p[i+1]=p[i+2]=p[i+3]=CLEAR_PACKEDYUV_PATTERN_SWAPPED;
		for(;i<size;i++) p[i]=CLEAR_PACKEDYUV_PATTERN_SWAPPED;
	    } else {
		for(i=0;i<size-3;i+=4) p[i]=p[i+1]=p[i+2]=p[i+3]=CLEAR_PACKEDYUV_PATTERN;
		for(;i<size;i++) p[i]=CLEAR_PACKEDYUV_PATTERN;
	    }
	} else
	    memset(dst,0,(mpi->bpp>>3)*w);
    }
}

mp_image_t* __FASTCALL__ vf_get_new_image(vf_instance_t* vf, unsigned int outfmt, int mp_imgtype, int mp_imgflag, int w, int h,unsigned idx){
    int is_static=0;
    mp_image_t* mpi=NULL;
    int w2=(mp_imgflag&MP_IMGFLAG_ACCEPT_ALIGNED_STRIDE)?((w+15)&(~15)):w;
    unsigned xp_idx=idx;
    MSG_DBG2("vf_get_new_image(%s,0x%X,0x%X,0x%X,0x%X,0x%X) was called\n",vf->info->name,outfmt,mp_imgtype,mp_imgflag,w,h);

    if(vf->put_slice==vf_next_put_slice){
	MSG_DBG2("passthru mode to %s\n",vf->next->info->name);
	return vf_get_new_image(vf->next,outfmt,mp_imgtype,mp_imgflag,w,h,xp_idx);
    }
    // Note: we should call libvo first to check if it supports direct rendering
    // and if not, then fallback to software buffers:
    switch(mp_imgtype){
	case MP_IMGTYPE_IP:
	case MP_IMGTYPE_STATIC:
	    is_static=1;
	    break;
	case MP_IMGTYPE_IPB:
	    if(mp_imgflag&MP_IMGFLAG_READABLE) is_static=1;
	default:
	    break;
    }
    mpi=new_mp_image(w2,h,xp_idx);
    if(mpi){
	mpi->type=mp_imgtype;
	mpi->w=w; mpi->h=h;
	// keep buffer allocation status & color flags only:
	mpi->flags&=MP_IMGFLAG_ALLOCATED|MP_IMGFLAG_TYPE_DISPLAYED|MP_IMGFLAGMASK_COLORS;
	// accept restrictions & draw_slice flags only:
	mpi->flags|=mp_imgflag&(MP_IMGFLAGMASK_RESTRICTIONS|MP_IMGFLAG_DRAW_CALLBACK);
	MSG_DBG2("vf_get_new_image fills mpi structure. flags=0x%X\n",mpi->flags);
	if(mpi->width!=w2 || mpi->height!=h){
	    if(mpi->flags&MP_IMGFLAG_ALLOCATED){
		if(mpi->width<w2 || mpi->height<h){
		    // need to re-allocate buffer memory:
		    delete mpi->planes[0];
		    mpi->flags&=~MP_IMGFLAG_ALLOCATED;
		    MSG_DBG2("vf.c: have to REALLOCATE buffer memory :(\n");
		}
	    }
	    mpi->width=w2; mpi->chroma_width=(w2 + (1<<mpi->chroma_x_shift) - 1)>>mpi->chroma_x_shift;
	    mpi->height=h; mpi->chroma_height=(h + (1<<mpi->chroma_y_shift) - 1)>>mpi->chroma_y_shift;
	}
	if(!mpi->bpp) mp_image_setfmt(mpi,outfmt);
	MSG_DBG2("vf_get_new_image setfmt. flags=0x%X\n",mpi->flags);
	if(!(mpi->flags&MP_IMGFLAG_ALLOCATED) && mpi->type>MP_IMGTYPE_EXPORT) {
	    // check libvo first!
	    if(vf->get_image) vf->get_image(vf,mpi);
	    MSG_DBG2("[vf->get_image] returns xp_idx=%u\n",mpi->xp_idx);

	    if(!(mpi->flags&MP_IMGFLAG_DIRECT)) {
		// non-direct and not yet allocated image. allocate it!
		// check if codec prefer aligned stride:
		if(mp_imgflag&MP_IMGFLAG_PREFER_ALIGNED_STRIDE) {
		    int align=( mpi->flags&MP_IMGFLAG_PLANAR &&
				mpi->flags&MP_IMGFLAG_YUV) ?
				(8<<mpi->chroma_x_shift)-1 : 15; // -- maybe FIXME
		    w2=((w+align)&(~align));
		    if(mpi->width!=w2) {
			// we have to change width... check if we CAN co it:
			int flags=vf->query_format(vf,outfmt,w,h); // should not fail
			if(!(flags&3)) MSG_WARN("??? vf_get_new_image{vf->query_format(outfmt)} failed!\n");
			if(flags&VFCAP_ACCEPT_STRIDE){
			    mpi->width=w2;
			    mpi->chroma_width=(w2 + (1<<mpi->chroma_x_shift) - 1)>>mpi->chroma_x_shift;
			}
		    }
		}
		if(is_static) {
		    unsigned idx=0;
		    if(mpi->flags&(MP_IMGTYPE_IP|MP_IMGTYPE_IPB)) {
			idx=vf->imgctx.static_idx;
			vf->imgctx.static_idx^=1;
		    }
		    if(!vf->imgctx.static_planes[idx]) {
			mpi_alloc_planes(mpi);
			vf->imgctx.static_planes[idx]=mpi->planes[0];
		    }
		    mpi->planes[0]=vf->imgctx.static_planes[idx];
		    mpi->flags&=~MP_IMGFLAG_ALLOCATED;
		} else
		    mpi_alloc_planes(mpi);
	    } // if !DIRECT
	} else {
	    MSG_DBG2("vf_get_new_image forces xp_idx retrieving\n");
	    mpi->xp_idx=dae_curr_vdecoded(mpxp_context().engine().xp_core);
	    mpi->flags&=~MP_IMGFLAG_ALLOCATED;
	}
	if(mpi->flags&MP_IMGFLAG_DRAW_CALLBACK && vf->start_slice)
	    vf->start_slice(vf,mpi);
	if(!(mpi->flags&MP_IMGFLAG_TYPE_DISPLAYED)){
	    MSG_V("*** [%s] %s%s mp_image_t, %dx%dx%dbpp %s %s, %d bytes\n",
		vf->info->name,
		(mpi->type==MP_IMGTYPE_EXPORT)?"Exporting":
		((mpi->flags&MP_IMGFLAG_DIRECT)?"Direct Rendering":"Allocating"),
		(mpi->flags&MP_IMGFLAG_DRAW_CALLBACK)?" (slices)":"",
		mpi->width,mpi->height,mpi->bpp,
		(mpi->flags&MP_IMGFLAG_YUV)?"YUV":((mpi->flags&MP_IMGFLAG_SWAPPED)?"BGR":"RGB"),
		(mpi->flags&MP_IMGFLAG_PLANAR)?"planar":"packed",
		mpi->bpp*mpi->width*mpi->height/8);
	    MSG_DBG2("(imgfmt: %x, planes: %x,%x,%x strides: %d,%d,%d, chroma: %dx%d, shift: h:%d,v:%d)\n",
		mpi->imgfmt, mpi->planes[0], mpi->planes[1], mpi->planes[2],
		mpi->stride[0], mpi->stride[1], mpi->stride[2],
		mpi->chroma_width, mpi->chroma_height, mpi->chroma_x_shift, mpi->chroma_y_shift);
	    mpi->flags|=MP_IMGFLAG_TYPE_DISPLAYED;
	}
    }
    check_pin("vfilter",vf->pin,VF_PIN);
    MSG_DBG2("vf_get_new_image returns xp_idx=%i\n",mpi->xp_idx);
    return mpi;
}

mp_image_t* __FASTCALL__ vf_get_new_genome(vf_instance_t* vf, const mp_image_t* mpi){
    return vf_get_new_image(vf,mpi->imgfmt,mpi->type,mpi->flags,mpi->w,mpi->h,mpi->xp_idx);
}

mp_image_t* __FASTCALL__ vf_get_new_exportable_genome(vf_instance_t* vf, int mp_imgtype, int mp_imgflag, const mp_image_t* mpi){
    return vf_get_new_image(vf,mpi->imgfmt,mp_imgtype,mp_imgflag,mpi->w,mpi->h,mpi->xp_idx);
}

mp_image_t* __FASTCALL__ vf_get_new_temp_genome(vf_instance_t* vf, const mp_image_t* mpi){
    return vf_get_new_exportable_genome(vf,MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE,mpi);
}

//============================================================================

// By default vf doesn't accept MPEGPES
static int __FASTCALL__ vf_default_query_format(vf_instance_t* vf, unsigned int fmt,unsigned w,unsigned h){
    UNUSED(vf);
    UNUSED(w);
    UNUSED(h);
    if(fmt == IMGFMT_MPEGPES) return 0;
    return 1;//vf_next_query_format(vf,fmt,w,h);
}

static vf_instance_t* __FASTCALL__ vf_open_plugin(vf_instance_t* next,const char *name,const char *args,libinput_t* libinput,const vf_conf_t* conf){
    vf_instance_t* vf;
    int i;
    for(i=0;;i++){
	if(filter_list[i]==&vf_info_null){
	    MSG_ERR("Can't find video filter: %s\n",name);
	    return NULL; // no such filter!
	}
	if(!strcmp(filter_list[i]->name,name)) break;
    }
    vf=new(zeromem) vf_instance_t;
    fill_false_pointers(vf->antiviral_hole,offsetof(vf_instance_t,pin)-offsetof(vf_instance_t,antiviral_hole));
    vf->pin=VF_PIN;
    vf->info=filter_list[i];
    vf->next=next;
    vf->prev=NULL;
    vf->config_vf=vf_next_config;
    vf->control_vf=vf_next_control;
    vf->query_format=vf_default_query_format;
    vf->put_slice=vf_next_put_slice;
    vf->default_caps=VFCAP_ACCEPT_STRIDE;
    vf->default_reqs=0;
    vf->conf.w=conf->w;
    vf->conf.h=conf->h;
    vf->conf.fourcc=conf->fourcc;
    vf->libinput=libinput;
    if(next) next->prev=vf;
    if(vf->info->open(vf,(char*)args)==MPXP_Ok) return vf; // Success!
    delete vf;
    MSG_ERR("Can't open video filter: %s\n",name);
    return NULL;
}

vf_instance_t* __FASTCALL__ vf_open_filter(vf_instance_t* next,const char *name,const char *args,libinput_t*libinput,const vf_conf_t* conf){
    if(strcmp(name,"vo")) {
	MSG_V("Open video filter: [%s] <%ux%u %s>\n", name,conf->w,conf->h,vo_format_name(conf->fourcc));
    }
    if(next) check_pin("vfilter",next->pin,VF_PIN);
    return vf_open_plugin(next,name,args,libinput,conf);
}

//============================================================================
unsigned int __FASTCALL__ vf_match_csp(vf_instance_t** vfp,unsigned int* list,const vf_conf_t* conf){
    vf_instance_t* vf=*vfp;
    unsigned int* p;
    unsigned int best=0;
    int ret;
    if((p=list)) while(*p){
	ret=vf->query_format(vf,*p,conf->w,conf->h);
	MSG_V("[%s] query(%s) -> %d\n",vf->info->name,vo_format_name(*p),ret&3);
	if(ret&2){ best=*p; break;} // no conversion -> bingo!
	if(ret&1 && !best) best=*p; // best with conversion
	++p;
    }
    if(best) return best; // bingo, they have common csp!
    // ok, then try with scale:
    if(vf->info == &vf_info_scale) return 0; // avoid infinite recursion!
    vf=vf_open_filter(vf,"fmtcvt",NULL,vf->libinput,conf);
    if(!vf) return 0; // failed to init "scale"
    // try the preferred csp first:
    if(conf->fourcc && vf->query_format(vf,conf->fourcc,conf->w,conf->h)) best=conf->fourcc;
    else
    // try the list again, now with "scaler" :
    if((p=list)) while(*p){
	ret=vf->query_format(vf,*p,conf->w,conf->h);
	MSG_V("[%s] query(%s) -> %d\n",vf->info->name,vo_format_name(*p),ret&3);
	if(ret&2){ best=*p; break;} // no conversion -> bingo!
	if(ret&1 && !best) best=*p; // best with conversion
	++p;
    }
    if(best) *vfp=vf; // else uninit vf  !FIXME!
    return best;
}

void __FASTCALL__ vf_clone_mpi_attributes(mp_image_t* dst, mp_image_t* src){
    dst->x=src->x;
    dst->y=dst->y;
    dst->pict_type= src->pict_type;
    dst->fields = src->fields;
    dst->qscale_type= src->qscale_type;
    if(dst->width == src->width && dst->height == src->height){
	dst->qstride= src->qstride;
	dst->qscale= src->qscale;
    }
}

int __FASTCALL__ vf_next_config(vf_instance_t* vf,
	int width, int height, int d_width, int d_height,
	vo_flags_e voflags, unsigned int outfmt){
    int miss;
    int flags=vf_next_query_format(vf,outfmt,d_width,d_height);
    if(!flags){
	// hmm. colorspace mismatch!!!
	// let's insert the 'scale' filter, it does the job for us:
	vf_instance_t* vf2;
	if(vf->next->info==&vf_info_scale) return 0; // scale->scale
	vf2=vf_open_filter(vf->next,"scale",NULL,vf->libinput,&vf->conf);
	if(!vf2) return 0; // shouldn't happen!
	vf->next=vf2;
	flags=vf_next_query_format(vf->next,outfmt,d_width,d_height);
	if(!flags){
	    MSG_ERR("Can't find colorspace for %s\n",vo_format_name(outfmt));
	    return 0; // FAIL
	}
    }
    MSG_V("REQ: flags=0x%X  req=0x%X  \n",flags,vf->default_reqs);
    miss=vf->default_reqs - (flags&vf->default_reqs);
    if(miss&VFCAP_ACCEPT_STRIDE){
	// vf requires stride support but vf->next doesn't support it!
	// let's insert the 'expand' filter, it does the job for us:
	vf_instance_t* vf2=vf_open_filter(vf->next,"expand",NULL,vf->libinput,&vf->conf);
	if(!vf2) return 0; // shouldn't happen!
	vf->next=vf2;
    }
    vf_showlist(vf);
    check_pin("vfilter",vf->pin,VF_PIN);
    return vf->next->config_vf(vf->next,width,height,d_width,d_height,voflags,outfmt);
}

MPXP_Rc __FASTCALL__ vf_next_control(vf_instance_t* vf, int request, any_t* data){
    return vf->next->control_vf(vf->next,request,data);
}

int __FASTCALL__ vf_next_query_format(vf_instance_t* vf, unsigned int fmt,unsigned width,unsigned height){
    int flags=vf->next?vf->next->query_format(vf->next,fmt,width,height):(int)vf->default_caps;
    if(flags) flags|=vf->default_caps;
    check_pin("vfilter",vf->pin,VF_PIN);
    return flags;
}

int __FASTCALL__ vf_query_format(vf_instance_t* vf, unsigned int fmt,unsigned width,unsigned height)
{
    check_pin("vfilter",vf->pin,VF_PIN);
    return vf->query_format(vf,fmt,width,height);
}

int __FASTCALL__ vf_next_put_slice(vf_instance_t* vf,mp_image_t *mpi){
    int rc;
    rc = vf->next->put_slice(vf->next,mpi);
    free_mp_image(mpi);
    return rc;
}

//============================================================================

vf_instance_t* __FASTCALL__ append_filters(vf_instance_t* last){
  return last;
}

//============================================================================

void __FASTCALL__ vf_uninit_filter(vf_instance_t* vf){
    if(vf->uninit) vf->uninit(vf);
    if(vf->imgctx.static_planes[0]) free(vf->imgctx.static_planes[0]);
    if(vf->imgctx.static_planes[1]) free(vf->imgctx.static_planes[1]);
    delete vf;
}

void __FASTCALL__ vf_uninit_filter_chain(vf_instance_t* vf){
    while(vf){
	vf_instance_t* next=vf->next;
	vf_uninit_filter(vf);
	vf=next;
    }
}

extern vf_cfg_t vf_cfg;
vf_instance_t* __FASTCALL__ vf_init_filter(libinput_t* libinput,const vf_conf_t* conf)
{
    char *vf_last=NULL,*vf_name=vf_cfg.list;
    char *arg;
    vf_instance_t* vfi=NULL,*vfi_prev=NULL,*vfi_first;
//    sh_video=sh;
    vfi=vf_open_filter(NULL,"vo",NULL,libinput,conf);
    vfi_prev=vfi;
    if(vf_name)
    while(vf_name!=vf_last){
	vf_last=strrchr(vf_name,',');
	if(vf_last) { *vf_last=0; vf_last++; }
	else vf_last=vf_name;
	arg=strchr(vf_last,'=');
	if(arg) { *arg=0; arg++; }
	MSG_V("Attach filter %s\n",vf_last);
	vfi=vf_open_plugin(vfi,vf_last,arg,libinput,conf);
	if(!vfi) vfi=vfi_prev;
	vfi_prev=vfi;
    }
    vfi_prev=NULL;
    vfi_first=vfi;
#if 1
    while(1)
    {
	if(!vfi) break;
	vfi->prev=vfi_prev;
	vfi_prev=vfi;
	vfi=vfi->next;
    }
#endif
#if 1
    vfi=vfi_first;
    while(1)
    {
	if(!vfi) break;
	MSG_V("%s(%s, %s) ",vfi->info->name,vfi->prev?vfi->prev->info->name:"NULL",vfi->next?vfi->next->info->name:"NULL");
	vfi=vfi->next;
    }
    MSG_V("\n");
#endif
    return vfi_first;
}

void __FASTCALL__ vf_showlist(vf_instance_t*vfi)
{
  vf_instance_t *next=vfi;
  MSG_INFO("[libvf] Using video filters chain:\n");
  do{
	MSG_INFO("  ");
	if(next->print_conf) next->print_conf(next);
	else
	    MSG_INFO("[vf_%s] %s [%dx%d,%s] \n",next->info->name,next->info->info,
			next->conf.w,next->conf.h,vo_format_name(next->conf.fourcc));
	next=next->next;
  }while(next);
}

unsigned __FASTCALL__ vf_query_flags(vf_instance_t*vfi)
{
  unsigned flags=0xFFFFFFFF;
  vf_instance_t *next=vfi;
  do{
	MSG_DBG2("[vf_%s] flags: %08X\n",next->info->name,next->info->flags);
	flags &= next->info->flags;
	next=next->next;
  }while(next);
  return flags;
}

static int __FASTCALL__ dummy_config(vf_instance_t* vf,
	int width, int height, int d_width, int d_height,
	vo_flags_e voflags, unsigned int outfmt){
    UNUSED(vf);
    UNUSED(width);
    UNUSED(height);
    UNUSED(d_width);
    UNUSED(d_height);
    UNUSED(voflags);
    UNUSED(outfmt);
    return 1;
}

static void vf_report_chain(vf_instance_t* first)
{
    vf_instance_t *_this=first;
    MSG_V("vf->first: %ix%i@%s\n",first->conf.w,first->conf.h,vo_format_name(first->conf.fourcc));
    while(1)
    {
	if(!_this) break;
	MSG_V("[%ux%i@%s](%s<-%s->%s)\n"
	    ,_this->conf.w
	    ,_this->conf.h
	    ,vo_format_name(_this->conf.fourcc)
	    ,_this->prev?_this->prev->info->name:"NULL"
	    ,_this->info->name
	    ,_this->next?_this->next->info->name:"NULL");
	_this=_this->next;
    }
}
vf_instance_t* __FASTCALL__ vf_reinit_vo(vf_instance_t* first,unsigned w,unsigned h,unsigned fmt,int reset_cache)
{
    vf_instance_t *vf_scaler=NULL;
    vf_instance_t* _saved=NULL;
    vf_instance_t* _this=first;
    vf_instance_t* _first=first;
    unsigned sw,sh,sfourcc;
    MSG_V("Call vf_reinit_vo <%s: %ix%i@%s> -> <vo: %ix%i@%s>\n"
	,_first->info->name
	,_first->conf.w,_first->conf.h,vo_format_name(_first->conf.fourcc)
	,w,h,vo_format_name(fmt));
    _this=first;
    _saved=NULL;
    while(1)
    {
	if(!_this) break;
	_this->prev=_saved;
	_saved=_this;
	_this=_this->next;
    }
    vf_report_chain(_first);
    _this=_saved->prev;
    if(_this)
    if(strcmp(_this->info->name,"fmtcvt")==0 || strcmp(_this->info->name,"scale")==0)
    {
	vf_instance_t* i;
	MSG_V("Unlinking 'fmtcvt'\n");
	i=_this->prev;
	if(i) i->next=_this->next;
	else first=_this->next;
	_this->next->prev=i;
	vf_uninit_filter(_this);
	vf_report_chain(_this);
    }
    /* _this == vo */
    _this=first;
    _saved=NULL;
    while(1)
    {
	if(!_this) break;
	_saved=_this;
	_this=_this->next;
    }
    _this=_saved;
    if(_this->prev)
    {
	sw=_this->prev->conf.w;
	sh=_this->prev->conf.h;
	sfourcc=_this->prev->conf.fourcc;
	MSG_V("Using(%s) %ix%i@%s\n",_this->prev->info->name,sw,sh,vo_format_name(sfourcc));
    }
    else
    {
	sw=_first->conf.w;
	sh=_first->conf.h;
	sfourcc=_first->conf.fourcc;
	MSG_V("Using(first:%s) %ix%i@%s\n",first->info->name,sw,sh,vo_format_name(sfourcc));
    }
    if(w==sw && h==sh && fmt==sfourcc); /* nothing todo */
    else
    {
	MSG_V("vf_reinit->config_vf %i %i %s=> %i %i %s\n",sw,sh,vo_format_name(sfourcc),w,h,vo_format_name(fmt));
	_saved=_this->prev;
	vf_scaler=vf_open_filter(_this,(w==sw&&h==sh)?"fmtcvt":"scale",NULL,_this->libinput,&_first->conf);
	if(vf_scaler)
	{
	    vf_config_fun_t sfnc;
	    sfnc=vf_scaler->next->config_vf;
	    vf_scaler->next->config_vf=dummy_config;
	    if(vf_scaler->config_vf(vf_scaler,sw,sh,
				w,h,
				VOFLAG_SWSCALE,
				sfourcc)==0){
		MSG_WARN(MSGTR_CannotInitVO);
		vf_scaler=NULL;
	    }
	    if(vf_scaler) vf_scaler->next->config_vf=sfnc;
	}
	if(vf_scaler)
	{
	    MSG_V("Insert scaler before '%s' after '%s'\n",_this->info->name,_saved?_saved->info->name:"NULL");
	    vf_scaler->prev=_saved;
	    if(_saved)	_saved->next=vf_scaler;
	    else	first=vf_scaler;
	    _this->conf.w=w;
	    _this->conf.h=h;
	    _this->conf.fourcc=fmt;
	    if(reset_cache) mpxp_reset_vcache();
	    mpxp_context().video().output->reset();
	}
    }
    _this=first;
    vf_report_chain(_this);
    return _this;
}

vf_instance_t* __FASTCALL__ vf_first(const vf_instance_t* it) {
    vf_instance_t* curr = const_cast<vf_instance_t*>(it);
    while(curr->prev) curr=curr->prev;
    return curr;
}

vf_instance_t* __FASTCALL__ vf_last(const vf_instance_t* it) {
    vf_instance_t* curr = const_cast<vf_instance_t*>(it);
    while(curr->next) curr=curr->next;
    return curr;
}
/*----------------------------------------*/
namespace mpxp {

void vf_help(){
    int i=0;
    MSG_INFO( "Available video filters:\n");
    while(filter_list[i]){
	MSG_INFO("\t%-10s: %s\n",filter_list[i]->name,filter_list[i]->info);
	i++;
    }
    MSG_INFO("\n");
}

vf_stream_t* vf_init(libinput_t* libinput,const vf_conf_t* conf) {
    vf_stream_t* s = new(zeromem) vf_stream_t;
    s->libinput=libinput;
    s->first=vf_init_filter(libinput,conf);
    s->first->parent=s;
    return s;
}

void vf_uninit(vf_stream_t* s) { vf_uninit_filter_chain(s->first); delete s; }

void __FASTCALL__ vf_reinit_vo(vf_stream_t* s,unsigned w,unsigned h,unsigned fmt,int reset_cache) {
    vf_instance_t* first=s->first;
    MSG_V("[stream: vf_reinit_vo]: %p\n",first);
    if(first) s->first=vf_reinit_vo(first,w,h,fmt,reset_cache);
}

void __FASTCALL__ vf_showlist(vf_stream_t* s) { vf_showlist(s->first); }

int __FASTCALL__ vf_query_flags(vf_stream_t* s) {
    vf_instance_t* first=s->first;
    return vf_query_flags(first);
}

int __FASTCALL__ vf_config(vf_stream_t* s,
			int width, int height, int d_width, int d_height,
			vo_flags_e flags, unsigned int outfmt) {
    vf_instance_t* first=s->first;
    return first->config_vf(first,width,height,d_width,d_height,flags,outfmt);
}
int __FASTCALL__ vf_query_format(vf_stream_t* s,unsigned int fmt,unsigned w,unsigned h) {
    vf_instance_t* first=s->first;
    return first->query_format(first,fmt,w,h);
}
void __FASTCALL__ vf_get_image(vf_stream_t* s,mp_image_t *mpi) {
    vf_instance_t* first=s->first;
    return first->get_image(first,mpi);
}
int __FASTCALL__ vf_put_slice(vf_stream_t* s,mp_image_t *mpi) {
    vf_instance_t* first=s->first;
    return first->put_slice(first,mpi);
}
void __FASTCALL__ vf_start_slice(vf_stream_t* s,mp_image_t *mpi) {
    vf_instance_t* first=s->first;
    return first->start_slice(first,mpi);
}

MPXP_Rc __FASTCALL__ vf_control(vf_stream_t* s,int request, any_t* data) {
    vf_instance_t* first=s->first;
    return first->control_vf(first,request,data);
}

mp_image_t* __FASTCALL__ vf_get_new_image(vf_stream_t* s, unsigned int outfmt, int mp_imgtype, int mp_imgflag, int w, int h,unsigned idx) {
    vf_instance_t* first=s->first;
    return vf_get_new_image(first,outfmt,mp_imgtype,mp_imgflag,w,h,idx);
}

void __FASTCALL__ vf_prepend_filter(vf_stream_t* s,const char *name,const vf_conf_t* conf,const char *args) {
    vf_instance_t* first=s->first;
    s->first=vf_open_filter(first,name,args,s->libinput,conf);
    s->first->parent=s;
}

void __FASTCALL__ vf_remove_first(vf_stream_t* s) {
    vf_instance_t* first=s->first;
    if(first->next) {
	s->first=first->next;
	vf_uninit_filter(first);
    }
}

const char * __FASTCALL__ vf_get_first_name(vf_stream_t* s) {
    vf_instance_t* first=s->first;
    return first->info->name;
}
} // namespace mpxp
