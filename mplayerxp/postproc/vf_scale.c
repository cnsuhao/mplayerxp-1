#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "../mp_config.h"
#include "../cpudetect.h"

#include "../libvo/img_format.h"
#include "mp_image.h"
#include "vf.h"

#include "../libvo/video_out.h"
#include "../libvo/fastmemcpy.h"
#include "swscale.h"
#include "libavutil/log.h"
#include "vf_scale.h"
#include "pp_msg.h"

struct vf_priv_s {
    int w,h,ofmt;
    int sw,sh,sfmt;
    int v_chr_drop;
    double param[2];
    unsigned int fmt;
    struct SwsContext *ctx;
    struct SwsContext *ctx2; //for interlaced slices only
    unsigned char* palette;
    int interlaced;
    int query_format_cache[64];
};
#if 0
 vf_priv_dflt = {
  -1,-1,
  0,
  {SWS_PARAM_DEFAULT, SWS_PARAM_DEFAULT},
  0,
  NULL,
  NULL,
  NULL,
  0
};
#endif
static int firstTime=1;
//===========================================================================//

void sws_getFlagsAndFilterFromCmdLine(int *flags, SwsFilter **srcFilterParam, SwsFilter **dstFilterParam);

static const unsigned int outfmt_list[]={
// YUV:
    IMGFMT_444P16_LE,
    IMGFMT_444P16_BE,
    IMGFMT_422P16_LE,
    IMGFMT_422P16_BE,
    IMGFMT_420P16_LE,
    IMGFMT_420P16_BE,
    IMGFMT_420A,
    IMGFMT_444P,
    IMGFMT_422P,
    IMGFMT_YV12,
    IMGFMT_I420,
    IMGFMT_IYUV,
    IMGFMT_YVU9,
    IMGFMT_IF09,
    IMGFMT_411P,
    IMGFMT_YUY2,
    IMGFMT_UYVY,
// RGB and grayscale (Y8 and Y800):
    IMGFMT_RGB48LE,
    IMGFMT_RGB48BE,
    IMGFMT_BGR32,
    IMGFMT_RGB32,
    IMGFMT_BGR24,
    IMGFMT_RGB24,
    IMGFMT_BGR16,
    IMGFMT_RGB16,
    IMGFMT_BGR15,
    IMGFMT_RGB15,
    IMGFMT_Y800,
    IMGFMT_Y8,
    IMGFMT_BGR8,
    IMGFMT_RGB8,
    IMGFMT_BGR4,
    IMGFMT_RGB4,
    IMGFMT_BG4B,
    IMGFMT_RG4B,
    IMGFMT_BGR1,
    IMGFMT_RGB1,
    0
};

static unsigned int __FASTCALL__ find_best_out(vf_instance_t *vf,unsigned w,unsigned h){
    unsigned int best=0;
    unsigned int i;

    // find the best outfmt:
    for(i=0; i<sizeof(outfmt_list)/sizeof(int)-1; i++){
        const int format= outfmt_list[i];
        int ret= vf->priv->query_format_cache[i]-1;
        if(ret == -1){
            ret= vf_next_query_format(vf, outfmt_list[i],w,h);
            vf->priv->query_format_cache[i]= ret+1;
        }

	MSG_DBG2("scale: query(%s) -> %d\n",vo_format_name(format),ret&3);
	if(ret&VFCAP_CSP_SUPPORTED_BY_HW){
            best=format; // no conversion -> bingo!
            break;
        }
	if(ret&VFCAP_CSP_SUPPORTED && !best) 
            best=format; // best with conversion
    }
    return best;
}

static void __FASTCALL__ print_conf(struct vf_instance_s* vf)
{
    MSG_INFO("[vf_scale]: in[%dx%d,%s] -> out[%dx%d,%s]\n",
	vf->priv->sw,vf->priv->sh,vo_format_name(vf->priv->sfmt),
	vf->priv->w,vf->priv->h,vo_format_name(vf->priv->ofmt));
}

static void __FASTCALL__ print_conf_fmtcvt(struct vf_instance_s* vf)
{
    MSG_INFO("[vf_fmtcvt]: video[%dx%d] in[%s] -> out[%s]\n",
	vf->priv->sw,vf->priv->sh,vo_format_name(vf->priv->sfmt),
	vo_format_name(vf->priv->ofmt));
}

static int __FASTCALL__ config(struct vf_instance_s* vf,
        int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt,void *tune){
    unsigned int best=find_best_out(vf,d_width,d_height);
    int vo_flags;
    int int_sws_flags=0;
    SwsFilter *srcFilter, *dstFilter;

    if(!best){
	MSG_WARN("SwScale: no supported outfmt found :(\n");
	return 0;
    }

    vo_flags=vf_next_query_format(vf,best,d_width,d_height);
    MSG_DBG2("vf_scale: %i=vf_next_query_format(%p,%X,%u,%u);\n"
	    ,vo_flags,vf,best,d_width,d_height);
    // scaling to dwidth*d_height, if all these TRUE:
    // - option -zoom
    // - no other sw/hw up/down scaling avail.
    // - we're after postproc
    // - user didn't set w:h
    if(!(vo_flags&VFCAP_POSTPROC) && (flags&4) && 
	    vf->priv->w<0 && vf->priv->h<0){	// -zoom
	int x=(vo_flags&VFCAP_SWSCALE) ? 0 : 1;
	if(d_width<width || d_height<height){
	    // downscale!
	    if(vo_flags&VFCAP_HWSCALE_DOWN) x=0;
	} else {
	    // upscale:
	    if(vo_flags&VFCAP_HWSCALE_UP) x=0;
	}
	if(x){
	    // user wants sw scaling! (-zoom)
	    vf->priv->w=d_width;
	    vf->priv->h=d_height;
	}
    }
    vf->priv->sw=width;
    vf->priv->sh=height;
    vf->priv->sfmt=outfmt;
    vf->priv->ofmt=best;
    // calculate the missing parameters:
    switch(best) {
    case IMGFMT_YUY2:		/* YUY2 needs w rounded to 2 */
    case IMGFMT_UYVY:
	if(vf->priv->w==-3) vf->priv->w=(vf->priv->h*width/height+1)&~1; else
	if(vf->priv->w==-2) vf->priv->w=(vf->priv->h*d_width/d_height+1)&~1;
	if(vf->priv->w<0) vf->priv->w=width; else
	if(vf->priv->w==0) vf->priv->w=d_width;
	if(vf->priv->h==-3) vf->priv->h=vf->priv->w*height/width; else
	if(vf->priv->h==-2) vf->priv->h=vf->priv->w*d_height/d_width;
	break;
    case IMGFMT_YV12:		/* YV12 needs w & h rounded to 2 */
    case IMGFMT_I420:
    case IMGFMT_IYUV:
	if(vf->priv->w==-3) vf->priv->w=(vf->priv->h*width/height+1)&~1; else
	if(vf->priv->w==-2) vf->priv->w=(vf->priv->h*d_width/d_height+1)&~1;
	if(vf->priv->w<0) vf->priv->w=width; else
	if(vf->priv->w==0) vf->priv->w=d_width;
	if(vf->priv->h==-3) vf->priv->h=(vf->priv->w*height/width+1)&~1; else
	if(vf->priv->h==-2) vf->priv->h=(vf->priv->w*d_height/d_width+1)&~1;
	break;
    default:
    if(vf->priv->w==-3) vf->priv->w=vf->priv->h*width/height; else
    if(vf->priv->w==-2) vf->priv->w=vf->priv->h*d_width/d_height;
    if(vf->priv->w<0) vf->priv->w=width; else
    if(vf->priv->w==0) vf->priv->w=d_width;
    if(vf->priv->h==-3) vf->priv->h=vf->priv->w*height/width; else
    if(vf->priv->h==-2) vf->priv->h=vf->priv->w*d_height/d_width;
    break;
    }

    if(vf->priv->h<0) vf->priv->h=height; else
    if(vf->priv->h==0) vf->priv->h=d_height;

    // free old ctx:
    if(vf->priv->ctx) sws_freeContext(vf->priv->ctx);
    if(vf->priv->ctx2)sws_freeContext(vf->priv->ctx2);

    // new swscaler:
    sws_getFlagsAndFilterFromCmdLine(&int_sws_flags, &srcFilter, &dstFilter);
    MSG_DBG2("vf_scale: sws_getFlagsAndFilterFromCmdLine(...);\n");
    int_sws_flags|= vf->priv->v_chr_drop << SWS_SRC_V_CHR_DROP_SHIFT;

    MSG_DBG2("vf_scale: sws_getContext(%u, %u, %s, %u, %u, %s, %X);\n"
	    ,width,height >> vf->priv->interlaced
	    ,vo_format_name(outfmt),vf->priv->w
	    ,vf->priv->h >> vf->priv->interlaced
	    ,vo_format_name(best),int_sws_flags | get_sws_cpuflags() | SWS_PRINT_INFO);
    vf->priv->ctx=sws_getContext(width, height >> vf->priv->interlaced,
	    pixfmt_from_fourcc(outfmt),
		  vf->priv->w, vf->priv->h >> vf->priv->interlaced,
	    pixfmt_from_fourcc(best),
	    int_sws_flags | get_sws_cpuflags() | SWS_PRINT_INFO,
	    srcFilter, dstFilter, vf->priv->param);
    MSG_DBG2("vf_scale: %p=sws_getContext\n",vf->priv->ctx);
    if(vf->priv->interlaced){
        vf->priv->ctx2=sws_getContext(width, height >> 1,
	    pixfmt_from_fourcc(outfmt),
		  vf->priv->w, vf->priv->h >> 1,
	    pixfmt_from_fourcc(best),
	    int_sws_flags | get_sws_cpuflags(), srcFilter, dstFilter, vf->priv->param);
    }
    if(!vf->priv->ctx){
	// error...
	MSG_WARN("Couldn't init SwScaler for this setup %p\n",vf->priv->ctx);
	return 0;
    }
    MSG_DBG2("vf_scale: SwScaler for was inited\n");
    vf->priv->fmt=best;

    if(vf->priv->palette){
	free(vf->priv->palette);
	vf->priv->palette=NULL;
    }
    switch(best) {
    case IMGFMT_RGB8: {
      /* set 332 palette for 8 bpp */
	int i;
	vf->priv->palette=malloc(4*256);
	for(i=0; i<256; i++){
	    vf->priv->palette[4*i+0]=4*(i>>6)*21;
	    vf->priv->palette[4*i+1]=4*((i>>3)&7)*9;
	    vf->priv->palette[4*i+2]=4*((i&7)&7)*9;
            vf->priv->palette[4*i+3]=0;
	}
	break; }
    case IMGFMT_BGR8: {
      /* set 332 palette for 8 bpp */
	int i;
	vf->priv->palette=malloc(4*256);
	for(i=0; i<256; i++){
	    vf->priv->palette[4*i+0]=4*(i&3)*21;
	    vf->priv->palette[4*i+1]=4*((i>>2)&7)*9;
	    vf->priv->palette[4*i+2]=4*((i>>5)&7)*9;
            vf->priv->palette[4*i+3]=0;
	}
	break; }
    case IMGFMT_BGR4: 
    case IMGFMT_BG4B: {
	int i;
	vf->priv->palette=malloc(4*16);
	for(i=0; i<16; i++){
	    vf->priv->palette[4*i+0]=4*(i&1)*63;
	    vf->priv->palette[4*i+1]=4*((i>>1)&3)*21;
	    vf->priv->palette[4*i+2]=4*((i>>3)&1)*63;
            vf->priv->palette[4*i+3]=0;
	}
	break; }
    case IMGFMT_RGB4:
    case IMGFMT_RG4B: {
	int i;
	vf->priv->palette=malloc(4*16);
	for(i=0; i<16; i++){
	    vf->priv->palette[4*i+0]=4*(i>>3)*63;
	    vf->priv->palette[4*i+1]=4*((i>>1)&3)*21;
	    vf->priv->palette[4*i+2]=4*((i&1)&1)*63;
            vf->priv->palette[4*i+3]=0;
	}
	break; }
    }

    if(!vo.opt_screen_size_x && !vo.opt_screen_size_y && !(vo.screen_size_xy >= 0.001)){
	// Compute new d_width and d_height, preserving aspect
	// while ensuring that both are >= output size in pixels.
	if (vf->priv->h * d_width > vf->priv->w * d_height) {
		d_width = vf->priv->h * d_width / d_height;
		d_height = vf->priv->h;
	} else {
		d_height = vf->priv->w * d_height / d_width;
		d_width = vf->priv->w;
	}
	//d_width=d_width*vf->priv->w/width;
	//d_height=d_height*vf->priv->h/height;
    }
    return vf_next_config(vf,vf->priv->w,vf->priv->h,d_width,d_height,flags,best,tune);
}

static void __FASTCALL__ scale(struct SwsContext *sws1, struct SwsContext *sws2, uint8_t *src[3], int src_stride[3], int y, int h, 
                  uint8_t *dst[3], int dst_stride[3], int interlaced){
    if(interlaced){
        int i;
        uint8_t *src2[3]={src[0], src[1], src[2]};
        uint8_t *dst2[3]={dst[0], dst[1], dst[2]};
        int src_stride2[3]={2*src_stride[0], 2*src_stride[1], 2*src_stride[2]};
        int dst_stride2[3]={2*dst_stride[0], 2*dst_stride[1], 2*dst_stride[2]};

        sws_scale(sws1, src2, src_stride2, y>>1, h>>1, dst2, dst_stride2);
        for(i=0; i<3; i++){
            src2[i] += src_stride[i];
            dst2[i] += dst_stride[i];
        }
        sws_scale(sws2, src2, src_stride2, y>>1, h>>1, dst2, dst_stride2);
    }else{
        sws_scale(sws1, src, src_stride, y, h, dst, dst_stride);
    }
}

static int __FASTCALL__ put_frame(struct vf_instance_s* vf, mp_image_t *mpi){
    mp_image_t *dmpi;//=mpi->priv;
    uint8_t *planes[4];
    int stride[4];
    planes[0]=mpi->planes[0];
    stride[0]=mpi->stride[0];
    if(mpi->flags&MP_IMGFLAG_PLANAR){
          planes[1]=mpi->planes[1];
          planes[2]=mpi->planes[2];
          planes[3]=mpi->planes[3];
          stride[1]=mpi->stride[1];
          stride[2]=mpi->stride[2];
          stride[3]=mpi->stride[3];
    }
    MSG_DBG2("vf_scale.put_frame was called\n");
    dmpi=vf_get_image(vf->next,vf->priv->fmt,
	MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE | MP_IMGFLAG_PREFER_ALIGNED_STRIDE,
	vf->priv->w, vf->priv->h);
    scale(vf->priv->ctx, vf->priv->ctx2, planes, stride, mpi->y, mpi->h, dmpi->planes, dmpi->stride, vf->priv->interlaced);
    return vf_next_put_slice(vf,dmpi);
}

static int __FASTCALL__ put_slice(struct vf_instance_s* vf, mp_image_t *mpi){
    mp_image_t *dmpi;//=mpi->priv;
    uint8_t *planes[4],*dplanes[4];
    int stride[4],newy,newh;
    planes[0]=mpi->planes[0];
    stride[0]=mpi->stride[0];
    if(mpi->flags&MP_IMGFLAG_PLANAR){
          planes[1]=mpi->planes[1];
          planes[2]=mpi->planes[2];
          planes[3]=mpi->planes[3];
          stride[1]=mpi->stride[1];
          stride[2]=mpi->stride[2];
          stride[3]=mpi->stride[3];
    }
    MSG_DBG2("vf_scale.put_slice was called[%i %i]\n",mpi->y, mpi->h);
    dmpi=vf_get_image(vf->next,vf->priv->fmt,
	MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE | MP_IMGFLAG_PREFER_ALIGNED_STRIDE,
	vf->priv->w, vf->priv->h);
    /* Try to fake first slice*/
    dplanes[0] = dmpi->planes[0];
    if(mpi->flags&MP_IMGFLAG_PLANAR) {
	dplanes[1] = dmpi->planes[1];
	dplanes[2] = dmpi->planes[2];
	dplanes[3] = dmpi->planes[3];
    }
    planes[0]  += mpi->y*mpi->stride[0];
    dplanes[0] += mpi->y*dmpi->stride[0];
    if(mpi->flags&MP_IMGFLAG_PLANAR){
	planes[1] += (mpi->y>>mpi->chroma_y_shift)*mpi->stride[1];
	planes[2] += (mpi->y>>mpi->chroma_y_shift)*mpi->stride[2];
	planes[3] += (mpi->y>>mpi->chroma_y_shift)*mpi->stride[3];
	dplanes[1]+= (mpi->y>>dmpi->chroma_y_shift)*dmpi->stride[0];
	dplanes[2]+= (mpi->y>>dmpi->chroma_y_shift)*dmpi->stride[1];
	dplanes[3]+= (mpi->y>>dmpi->chroma_y_shift)*dmpi->stride[2];
    }
    scale(vf->priv->ctx, vf->priv->ctx2, planes, stride, 0, mpi->h, dplanes, dmpi->stride, vf->priv->interlaced);
    dmpi->y = mpi->y;
    dmpi->h = mpi->h;
    return vf_next_put_slice(vf,dmpi);
}

static int __FASTCALL__ control(struct vf_instance_s* vf, int request, void* data){
    int *table;
    int *inv_table;
    int r;
    int brightness, contrast, saturation, srcRange, dstRange;
    vf_equalizer_t *eq;

  if(vf->priv->ctx)
    switch(request){
    case VFCTRL_GET_EQUALIZER:
	r= sws_getColorspaceDetails(vf->priv->ctx, &inv_table, &srcRange, &table, &dstRange, &brightness, &contrast, &saturation);
	if(r<0) break;

	eq = data;
	if (!strcmp(eq->item,"brightness")) {
		eq->value =  ((brightness*100) + (1<<15))>>16;
	}
	else if (!strcmp(eq->item,"contrast")) {
		eq->value = (((contrast  *100) + (1<<15))>>16) - 100;
	}
	else if (!strcmp(eq->item,"saturation")) {
		eq->value = (((saturation*100) + (1<<15))>>16) - 100;
	}
	else
		break;
	return CONTROL_TRUE;
    case VFCTRL_SET_EQUALIZER:
	r= sws_getColorspaceDetails(vf->priv->ctx, &inv_table, &srcRange, &table, &dstRange, &brightness, &contrast, &saturation);
	if(r<0) break;
//printf("set %f %f %f\n", brightness/(float)(1<<16), contrast/(float)(1<<16), saturation/(float)(1<<16));
	eq = data;

	if (!strcmp(eq->item,"brightness")) {
		brightness = (( eq->value     <<16) + 50)/100;
	}
	else if (!strcmp(eq->item,"contrast")) {
		contrast   = (((eq->value+100)<<16) + 50)/100;
	}
	else if (!strcmp(eq->item,"saturation")) {
		saturation = (((eq->value+100)<<16) + 50)/100;
	}
	else
		break;

	r= sws_setColorspaceDetails(vf->priv->ctx, inv_table, srcRange, table, dstRange, brightness, contrast, saturation);
	if(r<0) break;
	if(vf->priv->ctx2){
            r= sws_setColorspaceDetails(vf->priv->ctx2, inv_table, srcRange, table, dstRange, brightness, contrast, saturation);
            if(r<0) break;
        }

	return CONTROL_TRUE;
    default:
	break;
    }

    return vf_next_control(vf,request,data);
}

//===========================================================================//

//  supported Input formats: YV12, I420, IYUV, YUY2, UYVY, BGR32, BGR24, BGR16, BGR15, RGB32, RGB24, Y8, Y800

static int __FASTCALL__ query_format(struct vf_instance_s* vf, unsigned int fmt,unsigned w,unsigned h){
    MSG_DBG3("vf_scale: query_format(%p, %X(%s), %u, %u\n",vf,fmt,vo_format_name(fmt),w,h);
    switch(fmt){
    case IMGFMT_YV12:
    case IMGFMT_I420:
    case IMGFMT_IYUV:
    case IMGFMT_UYVY:
    case IMGFMT_YUY2:
    case IMGFMT_BGR32:
    case IMGFMT_BGR24:
    case IMGFMT_BGR16:
    case IMGFMT_BGR15:
    case IMGFMT_RGB32:
    case IMGFMT_RGB24:
    case IMGFMT_Y800:
    case IMGFMT_Y8:
    case IMGFMT_YVU9:
    case IMGFMT_IF09:
    case IMGFMT_444P:
    case IMGFMT_422P:
    case IMGFMT_411P:
    {
	unsigned int best=find_best_out(vf,w,h);
	int flags;
	if(!best) {
	    MSG_DBG2("[sw_scale] Can't find best out for %s\n",vo_format_name(fmt));
	    return 0;	 // no matching out-fmt
	}
	flags=vf_next_query_format(vf,best,w,h);
	if(!(flags&3)) {
	    MSG_DBG2("[sw_scale] Can't find HW support for %s on %s\n",vo_format_name(best),vf->next->info->name);
	    return 0; // huh?
	}
	MSG_DBG3("[sw_scale] %s supported on %s like %u\n",vo_format_name(best),vf->next->info->name,flags);
	if(fmt!=best) flags&=~VFCAP_CSP_SUPPORTED_BY_HW;
	// do not allow scaling, if we are before the PP fliter!
	if(!(flags&VFCAP_POSTPROC)) flags|=VFCAP_SWSCALE;
	MSG_DBG3("[sw_scale] returning: %u\n",flags);
	return flags;
      }
    }
    MSG_DBG2("format %s is not supported by sw_scaler\n",vo_format_name(fmt));
    return 0;	// nomatching in-fmt
}

static void __FASTCALL__ uninit(struct vf_instance_s *vf){
    if(vf->priv->ctx) sws_freeContext(vf->priv->ctx);
    if(vf->priv->ctx2) sws_freeContext(vf->priv->ctx2);
    if(vf->priv->palette) free(vf->priv->palette);
    free(vf->priv);
    firstTime=1;
}

static int __FASTCALL__ vf_open(vf_instance_t *vf,const char* args){
    vf->config=config;
    vf->put_slice=put_frame;
    vf->query_format=query_format;
    vf->control= control;
    vf->uninit=uninit;
    vf->print_conf=print_conf;
    if(!vf->priv) {
    vf->priv=malloc(sizeof(struct vf_priv_s));
    memset(vf->priv,0,sizeof(struct vf_priv_s));
    // TODO: parse args ->
    vf->priv->ctx=NULL;
    vf->priv->ctx2=NULL;
    vf->priv->w=
    vf->priv->h=-1;
    vf->priv->v_chr_drop=0;
    vf->priv->param[0]=
    vf->priv->param[1]=SWS_PARAM_DEFAULT;
    vf->priv->palette=NULL;
    } // if(!vf->priv)
    if(args) sscanf(args, "%d:%d:%d:%lf:%lf",
    &vf->priv->w,
    &vf->priv->h,
    &vf->priv->v_chr_drop,
    &vf->priv->param[0],
    &vf->priv->param[1]);
    MSG_V("SwScale params: %d x %d (-1=no scaling)\n",
    vf->priv->w,
    vf->priv->h);
    if(!verbose) av_log_set_level(AV_LOG_FATAL); /* suppress: slices start in the middle */
    return 1;
}

static int __FASTCALL__ vf_open_fmtcvt(vf_instance_t *vf,const char* args){
    int retval = vf_open(vf,args);
    vf->put_slice=put_slice;
    vf->print_conf=print_conf_fmtcvt;
    return retval;
}

//global sws_flags from the command line
int sws_flags=2;

//global srcFilter
static SwsFilter *src_filter= NULL;

float sws_lum_gblur= 0.0;
float sws_chr_gblur= 0.0;
int sws_chr_vshift= 0;
int sws_chr_hshift= 0;
float sws_chr_sharpen= 0.0;
float sws_lum_sharpen= 0.0;

int get_sws_cpuflags(){
    int flags=0;
    if(gCpuCaps.hasMMX) flags |= SWS_CPU_CAPS_MMX;
    if(gCpuCaps.hasMMX2) flags |= SWS_CPU_CAPS_MMX2;
    if(gCpuCaps.has3DNow) flags |= SWS_CPU_CAPS_3DNOW;
    return flags;
}

void sws_getFlagsAndFilterFromCmdLine(int *flags, SwsFilter **srcFilterParam, SwsFilter **dstFilterParam)
{
	*flags=0;

	if(firstTime)
	{
		firstTime=0;
		*flags= SWS_PRINT_INFO;
	}
	else if(verbose>1) *flags= SWS_PRINT_INFO;

	if(src_filter) sws_freeFilter(src_filter);

	src_filter= sws_getDefaultFilter(
		sws_lum_gblur, sws_chr_gblur,
		sws_lum_sharpen, sws_chr_sharpen,
		sws_chr_hshift, sws_chr_vshift, verbose>1);

	switch(sws_flags)
	{
		case 0: *flags|= SWS_FAST_BILINEAR; break;
		case 1: *flags|= SWS_BILINEAR; break;
		case 2: *flags|= SWS_BICUBIC; break;
		case 3: *flags|= SWS_X; break;
		case 4: *flags|= SWS_POINT; break;
		case 5: *flags|= SWS_AREA; break;
		case 6: *flags|= SWS_BICUBLIN; break;
		case 7: *flags|= SWS_GAUSS; break;
		case 8: *flags|= SWS_SINC; break;
		case 9: *flags|= SWS_LANCZOS; break;
		case 10:*flags|= SWS_SPLINE; break;
		default:*flags|= SWS_BILINEAR; break;
	}
	
	*srcFilterParam= src_filter;
	*dstFilterParam= NULL;
}

// will use sws_flags & src_filter (from cmd line)
struct SwsContext *sws_getContextFromCmdLine(int srcW, int srcH, int srcFormat, int dstW, int dstH, int dstFormat)
{
	int flags;
	SwsFilter *dstFilterParam, *srcFilterParam;
	sws_getFlagsAndFilterFromCmdLine(&flags, &srcFilterParam, &dstFilterParam);

	return sws_getContext(srcW, srcH, srcFormat, dstW, dstH, dstFormat, flags | get_sws_cpuflags(), srcFilterParam, dstFilterParam, NULL);
}

#if 0
/// An example of presets usage
static struct size_preset {
  char* name;
  int w, h;
} vf_size_presets_defs[] = {
  // TODO add more 'standard' resolutions
  { "qntsc", 352, 240 },
  { "qpal", 352, 288 },
  { "ntsc", 720, 480 },
  { "pal", 720, 576 },
  { "sntsc", 640, 480 },
  { "spal", 768, 576 },
  { NULL, 0, 0}
};
#endif
/* note: slices give unstable performance downgrading */
const vf_info_t vf_info_scale = {
    "software scaling",
    "scale",
    "A'rpi",
    "",
    VF_FLAGS_THREADS,
    vf_open
};

/* Just for analisys */
const vf_info_t vf_info_fmtcvt = {
    "format converter",
    "fmtcvt",
    "A'rpi",
    "",
    VF_FLAGS_THREADS|VF_FLAGS_SLICES,
    vf_open_fmtcvt
};

//===========================================================================//
