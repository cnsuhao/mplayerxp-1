#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
/*
    Copyright (C) 2002 Michael Niedermayer <michaelni@gmx.at>

    This program is mp_free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>

#ifdef USE_SETLOCALE
#include <locale.h>
#endif

#include "libvo2/img_format.h"
#include "xmpcore/xmp_image.h"
#include "vf.h"
#include "vf_internal.h"
#include "osdep/fastmemcpy.h"
#include "postproc/swscale.h"
#include "vf_scale.h"
#include "pp_msg.h"

//===========================================================================//

struct FilterParam{
	float radius;
	float strength;
	float preFilterRadius;
	int threshold;
	float quality;
	struct SwsContext *filterContext;

	struct SwsContext *preFilterContext;
	uint8_t *preFilterBuf;
	int preFilterStride;
	int distWidth;
	int distStride;
	int *distCoeff;
	int colorDiffCoeff[512];
};

struct vf_priv_t {
	FilterParam luma;
	FilterParam chroma;
	int (* __FASTCALL__ allocStuff)(FilterParam *f, int width, int height);
	void (* __FASTCALL__ freeBuffers)(FilterParam *f);
};
/* boxblur stuff */

//FIXME stupid code duplication
static void __FASTCALL__ getSubSampleFactors(int *h, int *v, int format){
	switch(format){
	case IMGFMT_YV12:
	case IMGFMT_I420:
		*h=1;
		*v=1;
		break;
	case IMGFMT_YVU9:
		*h=2;
		*v=2;
		break;
	case IMGFMT_444P:
		*h=0;
		*v=0;
		break;
	case IMGFMT_422P:
		*h=1;
		*v=0;
		break;
	case IMGFMT_411P:
		*h=2;
		*v=0;
		break;
	}
}

static inline void bb_blur(uint8_t *dst, uint8_t *src, int w, int radius, int dstStep, int srcStep){
	int x;
	const int length= radius*2 + 1;
	const int inv= ((1<<16) + length/2)/length;

	int sum= 0;

	for(x=0; x<radius; x++){
		sum+= src[x*srcStep]<<1;
	}
	sum+= src[radius*srcStep];

	for(x=0; x<=radius; x++){
		sum+= src[(radius+x)*srcStep] - src[(radius-x)*srcStep];
		dst[x*dstStep]= (sum*inv + (1<<15))>>16;
	}

	for(; x<w-radius; x++){
		sum+= src[(radius+x)*srcStep] - src[(x-radius-1)*srcStep];
		dst[x*dstStep]= (sum*inv + (1<<15))>>16;
	}

	for(; x<w; x++){
		sum+= src[(2*w-radius-x-1)*srcStep] - src[(x-radius-1)*srcStep];
		dst[x*dstStep]= (sum*inv + (1<<15))>>16;
	}
}

static inline void bb_blur2(uint8_t *dst, uint8_t *src, int w, int radius, int power, int dstStep, int srcStep){
	uint8_t temp[2][4096];
	uint8_t *a= temp[0], *b=temp[1];

	if(radius){
		bb_blur(a, src, w, radius, 1, srcStep);
		for(; power>2; power--){
			uint8_t *c;
			bb_blur(b, a, w, radius, 1, 1);
			c=a; a=b; b=c;
		}
		if(power>1)
			bb_blur(dst, a, w, radius, dstStep, 1);
		else{
			int i;
			for(i=0; i<w; i++)
				dst[i*dstStep]= a[i];
		}
	}else{
		int i;
		for(i=0; i<w; i++)
			dst[i*dstStep]= src[i*srcStep];
	}
}

static void __FASTCALL__ hBlur(uint8_t *dst, uint8_t *src, int w, int h, int dstStride, int srcStride, int radius, int power){
	int y;

	if(radius==0 && dst==src) return;

	for(y=0; y<h; y++){
		bb_blur2(dst + y*dstStride, src + y*srcStride, w, radius, power, 1, 1);
	}
}

//FIXME optimize (x before y !!!)
static void __FASTCALL__ vBlur(uint8_t *dst, uint8_t *src, int w, int h, int dstStride, int srcStride, int radius, int power){
	int x;

	if(radius==0 && dst==src) return;

	for(x=0; x<w; x++){
		bb_blur2(dst + x, src + x, h, radius, power, dstStride, srcStride);
	}
}

static int __FASTCALL__ bb_put_slice(vf_instance_t* vf, mp_image_t *mpi){
	int cw= mpi->w >> mpi->chroma_x_shift;
	int ch= mpi->h >> mpi->chroma_y_shift;

	mp_image_t *dmpi=vf_get_new_temp_genome(vf->next,mpi);

	assert(mpi->flags&MP_IMGFLAG_PLANAR);

	hBlur(dmpi->planes[0], mpi->planes[0], mpi->w,mpi->h,
		dmpi->stride[0], mpi->stride[0], vf->priv->luma.radius, vf->priv->luma.strength);
	hBlur(dmpi->planes[1], mpi->planes[1], cw,ch,
		dmpi->stride[1], mpi->stride[1], vf->priv->chroma.radius, vf->priv->chroma.strength);
	hBlur(dmpi->planes[2], mpi->planes[2], cw,ch,
		dmpi->stride[2], mpi->stride[2], vf->priv->chroma.radius, vf->priv->chroma.strength);

	vBlur(dmpi->planes[0], dmpi->planes[0], mpi->w,mpi->h,
		dmpi->stride[0], dmpi->stride[0], vf->priv->luma.radius, vf->priv->luma.strength);
	vBlur(dmpi->planes[1], dmpi->planes[1], cw,ch,
		dmpi->stride[1], dmpi->stride[1], vf->priv->chroma.radius, vf->priv->chroma.strength);
	vBlur(dmpi->planes[2], dmpi->planes[2], cw,ch,
		dmpi->stride[2], dmpi->stride[2], vf->priv->chroma.radius, vf->priv->chroma.strength);

	return vf_next_put_slice(vf,dmpi);
}

static int __FASTCALL__ sab_allocStuff(FilterParam *f, int width, int height){
	int stride= (width+7)&~7;
	SwsVector *vec;
	SwsFilter swsF;
	int i,x,y;
	f->preFilterBuf=new(alignmem,8) uint8_t[stride*height];
	f->preFilterStride= stride;

	vec = sws_getGaussianVec(f->preFilterRadius, f->quality);
	swsF.lumH= swsF.lumV= vec;
	swsF.chrH= swsF.chrV= NULL;
	f->preFilterContext= sws_getContext(
		width, height, pixfmt_from_fourcc(IMGFMT_Y800), width, height, pixfmt_from_fourcc(IMGFMT_Y800), get_sws_cpuflags(), &swsF, NULL, NULL);

	sws_freeVec(vec);
	vec = sws_getGaussianVec(f->strength, 5.0);
	for(i=0; i<512; i++){
		double d;
		int index= i-256 + vec->length/2;

		if(index<0 || index>=vec->length) 	d= 0.0;
		else					d= vec->coeff[index];

		f->colorDiffCoeff[i]= (int)(d/vec->coeff[vec->length/2]*(1<<12) + 0.5);
	}
	sws_freeVec(vec);
	vec = sws_getGaussianVec(f->radius, f->quality);
	f->distWidth= vec->length;
	f->distStride= (vec->length+7)&~7;
	f->distCoeff=new(alignmem,8) int32_t[f->distWidth*f->distStride];

	for(y=0; y<vec->length; y++){
		for(x=0; x<vec->length; x++){
			double d= vec->coeff[x] * vec->coeff[y];

			f->distCoeff[x + y*f->distStride]= (int)(d*(1<<10) + 0.5);
//			if(y==vec->length/2)
//				printf("%6d ", f->distCoeff[x + y*f->distStride]);
		}
	}
	sws_freeVec(vec);

	return 0;
}

static void __FASTCALL__ sab_freeBuffers(FilterParam *f){
	if(f->preFilterContext) sws_freeContext(f->preFilterContext);
	f->preFilterContext=NULL;

	if(f->preFilterBuf) delete f->preFilterBuf;
	f->preFilterBuf=NULL;

	if(f->distCoeff) delete f->distCoeff;
	f->distCoeff=NULL;
}

static inline void sab_blur(uint8_t *dst, uint8_t *src, int w, int h, int dstStride, int srcStride, FilterParam *fp){
	int x, y;
	FilterParam f= *fp;
	const int radius= f.distWidth/2;
	uint8_t *srcArray[3]= {src, NULL, NULL};
	uint8_t *dstArray[3]= {f.preFilterBuf, NULL, NULL};
	int srcStrideArray[3]= {srcStride, 0, 0};
	int dstStrideArray[3]= {f.preFilterStride, 0, 0};

//	f.preFilterContext->swScale(f.preFilterContext, srcArray, srcStrideArray, 0, h, dstArray, dstStrideArray);
	sws_scale(f.preFilterContext, srcArray, srcStrideArray, 0, h, dstArray, dstStrideArray);

	for(y=0; y<h; y++){
		for(x=0; x<w; x++){
			int sum=0;
			int div=0;
			int dy;
			const int preVal= f.preFilterBuf[x + y*f.preFilterStride];
#if 0
			const int srcVal= src[x + y*srcStride];
if((x/32)&1){
    dst[x + y*dstStride]= srcVal;
    if(y%32==0) dst[x + y*dstStride]= 0;
    continue;
}
#endif
			if(x >= radius && x < w - radius){
				for(dy=0; dy<radius*2+1; dy++){
					int dx;
					int iy= y+dy - radius;
					if     (iy<0)  iy=  -iy;
					else if(iy>=h) iy= h-iy-1;

					for(dx=0; dx<radius*2+1; dx++){
						const int ix= x+dx - radius;
						int factor;

						factor= f.colorDiffCoeff[256+preVal - f.preFilterBuf[ix + iy*f.preFilterStride] ]
							*f.distCoeff[dx + dy*f.distStride];
						sum+= src[ix + iy*srcStride] *factor;
						div+= factor;
					}
				}
			}else{
				for(dy=0; dy<radius*2+1; dy++){
					int dx;
					int iy= y+dy - radius;
					if     (iy<0)  iy=  -iy;
					else if(iy>=h) iy= h-iy-1;

					for(dx=0; dx<radius*2+1; dx++){
						int ix= x+dx - radius;
						int factor;
						if     (ix<0)  ix=  -ix;
						else if(ix>=w) ix= w-ix-1;

						factor= f.colorDiffCoeff[256+preVal - f.preFilterBuf[ix + iy*f.preFilterStride] ]
							*f.distCoeff[dx + dy*f.distStride];
						sum+= src[ix + iy*srcStride] *factor;
						div+= factor;
					}
				}
			}
			dst[x + y*dstStride]= (sum + div/2)/div;
		}
	}
}

static int __FASTCALL__ sab_put_slice(vf_instance_t* vf, mp_image_t *mpi){
	int cw= mpi->w >> mpi->chroma_x_shift;
	int ch= mpi->h >> mpi->chroma_y_shift;

	mp_image_t *dmpi=vf_get_new_temp_genome(vf->next,mpi);

	assert(mpi->flags&MP_IMGFLAG_PLANAR);

	sab_blur(dmpi->planes[0], mpi->planes[0], mpi->w,mpi->h, dmpi->stride[0], mpi->stride[0], &vf->priv->luma);
	sab_blur(dmpi->planes[1], mpi->planes[1], cw    , ch   , dmpi->stride[1], mpi->stride[1], &vf->priv->chroma);
	sab_blur(dmpi->planes[2], mpi->planes[2], cw    , ch   , dmpi->stride[2], mpi->stride[2], &vf->priv->chroma);

	return vf_next_put_slice(vf,dmpi);
}

/***************************************************************************/

static int __FASTCALL__ allocStuff(FilterParam *f, int width, int height){
	SwsVector *vec;
	SwsFilter swsF;

	vec = sws_getGaussianVec(f->radius, f->quality);
	sws_scaleVec(vec, f->strength);
	vec->coeff[vec->length/2]+= 1.0 - f->strength;
	swsF.lumH= swsF.lumV= vec;
	swsF.chrH= swsF.chrV= NULL;
	f->filterContext= sws_getContext(
		width, height, pixfmt_from_fourcc(IMGFMT_Y800), width, height, pixfmt_from_fourcc(IMGFMT_Y800), get_sws_cpuflags(), &swsF, NULL, NULL);

	sws_freeVec(vec);

	return 0;
}

static int __FASTCALL__ vf_config(vf_instance_t* vf,
	int width, int height, int d_width, int d_height,
	vo_flags_e flags, unsigned int outfmt){

	int sw, sh;

	if(vf->priv->allocStuff)
	    vf->priv->allocStuff(&vf->priv->luma, width, height);

	getSubSampleFactors(&sw, &sh, outfmt);
	if(vf->priv->allocStuff)
	    vf->priv->allocStuff(&vf->priv->chroma, width>>sw, height>>sh);

	return vf_next_config(vf,width,height,d_width,d_height,flags,outfmt);
}

static void __FASTCALL__ freeBuffers(FilterParam *f){
	if(f->filterContext) sws_freeContext(f->filterContext);
	f->filterContext=NULL;
}

static void __FASTCALL__ uninit(vf_instance_t* vf){
	if(!vf->priv) return;

	if(vf->priv->freeBuffers)
	{
	    vf->priv->freeBuffers(&vf->priv->luma);
	    vf->priv->freeBuffers(&vf->priv->chroma);
	}
	delete vf->priv;
	vf->priv=NULL;
}

static inline void blur(uint8_t *dst, uint8_t *src, int w, int h, int dstStride, int srcStride, FilterParam *fp){
	int x, y;
	FilterParam f= *fp;
	uint8_t *srcArray[3]= {src, NULL, NULL};
	uint8_t *dstArray[3]= {dst, NULL, NULL};
	int srcStrideArray[3]= {srcStride, 0, 0};
	int dstStrideArray[3]= {dstStride, 0, 0};

	sws_scale(f.filterContext, srcArray, srcStrideArray, 0, h, dstArray, dstStrideArray);

	if(f.threshold > 0){
		for(y=0; y<h; y++){
			for(x=0; x<w; x++){
				const int orig= src[x + y*srcStride];
				const int filtered= dst[x + y*dstStride];
				const int diff= orig - filtered;

				if(diff > 0){
					if(diff > 2*f.threshold){
						dst[x + y*dstStride]= orig;
					}else if(diff > f.threshold){
						dst[x + y*dstStride]= filtered + diff - f.threshold;
					}
				}else{
					if(-diff > 2*f.threshold){
						dst[x + y*dstStride]= orig;
					}else if(-diff > f.threshold){
						dst[x + y*dstStride]= filtered + diff + f.threshold;
					}
				}
			}
		}
	}else if(f.threshold < 0){
		for(y=0; y<h; y++){
			for(x=0; x<w; x++){
				const int orig= src[x + y*srcStride];
				const int filtered= dst[x + y*dstStride];
				const int diff= orig - filtered;

				if(diff > 0){
					if(diff > -2*f.threshold){
					}else if(diff > -f.threshold){
						dst[x + y*dstStride]= orig - diff - f.threshold;
					}else
						dst[x + y*dstStride]= orig;
				}else{
					if(diff < 2*f.threshold){
					}else if(diff < f.threshold){
						dst[x + y*dstStride]= orig - diff + f.threshold;
					}else
						dst[x + y*dstStride]= orig;
				}
			}
		}
	}
}

static int __FASTCALL__ put_slice(vf_instance_t* vf, mp_image_t *mpi){
	int cw= mpi->w >> mpi->chroma_x_shift;
	int ch= mpi->h >> mpi->chroma_y_shift;

	mp_image_t *dmpi=vf_get_new_temp_genome(vf->next,mpi);

	assert(mpi->flags&MP_IMGFLAG_PLANAR);

#ifdef _OPENMP
#pragma omp parallel sections
{
#pragma omp section
#endif
	blur(dmpi->planes[0], mpi->planes[0], mpi->w,mpi->h, dmpi->stride[0], mpi->stride[0], &vf->priv->luma);
#ifdef _OPENMP
#pragma omp section
#endif
	blur(dmpi->planes[1], mpi->planes[1], cw    , ch   , dmpi->stride[1], mpi->stride[1], &vf->priv->chroma);
#ifdef _OPENMP
#pragma omp section
#endif
	blur(dmpi->planes[2], mpi->planes[2], cw    , ch   , dmpi->stride[2], mpi->stride[2], &vf->priv->chroma);
#ifdef _OPENMP
}
#endif
	return vf_next_put_slice(vf,dmpi);
}

//===========================================================================//

static int __FASTCALL__ query_format(vf_instance_t* vf, unsigned int fmt,unsigned w,unsigned h){
	switch(fmt)
	{
	case IMGFMT_YV12:
	case IMGFMT_I420:
	case IMGFMT_IYUV:
	case IMGFMT_YVU9:
	case IMGFMT_444P:
	case IMGFMT_422P:
	case IMGFMT_411P:
		return vf_next_query_format(vf, fmt,w,h);
	}
	return 0;
}

static MPXP_Rc __FASTCALL__ vf_open(vf_instance_t *vf,const char* args){
	int e;

	vf->config_vf=vf_config;
	vf->put_slice=put_slice;
	vf->query_format=query_format;
	vf->uninit=uninit;
	vf->priv=new(zeromem) vf_priv_t;

	if(args==NULL) return MPXP_False;

#ifdef USE_SETLOCALE
    setlocale( LC_NUMERIC, "C" );
#endif
	e=sscanf(args, "%f:%f:%f:%d:%f:%f:%f:%d",
		&vf->priv->luma.radius,
		&vf->priv->luma.strength,
		&vf->priv->luma.preFilterRadius,
		&vf->priv->luma.threshold,
		&vf->priv->chroma.radius,
		&vf->priv->chroma.strength,
		&vf->priv->chroma.preFilterRadius,
		&vf->priv->chroma.threshold
		);
#ifdef USE_SETLOCALE
    setlocale( LC_NUMERIC, "" );
#endif

	vf->priv->luma.quality = vf->priv->chroma.quality= 3.0;

	if(e==4){
		vf->priv->chroma.radius= vf->priv->luma.radius;
		vf->priv->chroma.strength= vf->priv->luma.strength;
		vf->priv->chroma.threshold = vf->priv->luma.threshold;
		vf->priv->chroma.preFilterRadius = vf->priv->luma.preFilterRadius;
	}else if(e!=8)
		return MPXP_False;
	vf->priv->allocStuff=allocStuff;
	vf->priv->freeBuffers=freeBuffers;
	if(vf->priv->chroma.preFilterRadius!=0. || vf->priv->luma.preFilterRadius!=0.)
	{
	    vf->put_slice=sab_put_slice;
	    vf->priv->allocStuff=sab_allocStuff;
	    vf->priv->freeBuffers=sab_freeBuffers;
	}
	else
	if(vf->priv->luma.threshold==0 && vf->priv->chroma.threshold==0)
	{
	    if(vf->priv->luma.radius < 0) return MPXP_False;
	    if(vf->priv->chroma.radius < 0) return MPXP_False;
	    vf->priv->allocStuff=NULL;
	    vf->priv->freeBuffers=NULL;
	    vf->put_slice=bb_put_slice;
	}
    check_pin("vfilter",vf->pin,VF_PIN);
    return MPXP_Ok;
}

extern const vf_info_t vf_info_smartblur = {
    "smart blur",
    "smartblur",
    "Michael Niedermayer",
    "",
    VF_FLAGS_THREADS,
    vf_open
};

//===========================================================================//
