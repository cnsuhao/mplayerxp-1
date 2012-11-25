#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
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
#include <algorithm>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "libvo/img_format.h"
#include "xmpcore/mp_image.h"
#include "vf.h"
#include "swscale.h"
#include "osdep/fastmemcpy.h"
#include "pp_msg.h"

//===========================================================================//

#include <inttypes.h>
#include <math.h>

#define WIDTH 512
#define HEIGHT 512

struct vf_priv_s {
    unsigned int fmt;
    int frame_num;
};

static unsigned int __FASTCALL__ rgb_getfmt(unsigned int outfmt){
    switch(outfmt){
    case IMGFMT_RGB15:
    case IMGFMT_RGB16:
    case IMGFMT_RGB24:
    case IMGFMT_RGBA:
    case IMGFMT_ARGB:
    case IMGFMT_BGR15:
    case IMGFMT_BGR16:
    case IMGFMT_BGR24:
    case IMGFMT_BGRA:
    case IMGFMT_ABGR:
	return outfmt;
    }
    return 0;
}

static void __FASTCALL__ rgb_put_pixel(uint8_t *buf, int x, int y, int stride, int r, int g, int b, int fmt){
    switch(fmt){
    case IMGFMT_BGR15: ((uint16_t*)(buf + y*stride))[x]= ((r>>3)<<10) | ((g>>3)<<5) | (b>>3);
    break;
    case IMGFMT_RGB15: ((uint16_t*)(buf + y*stride))[x]= ((b>>3)<<10) | ((g>>3)<<5) | (r>>3);
    break;
    case IMGFMT_BGR16: ((uint16_t*)(buf + y*stride))[x]= ((r>>3)<<11) | ((g>>2)<<5) | (b>>3);
    break;
    case IMGFMT_RGB16: ((uint16_t*)(buf + y*stride))[x]= ((b>>3)<<11) | ((g>>2)<<5) | (r>>3);
    break;
    case IMGFMT_RGB24:
	buf[3*x + y*stride + 0]= r;
	buf[3*x + y*stride + 1]= g;
	buf[3*x + y*stride + 2]= b;
    break;
    case IMGFMT_BGR24:
	buf[3*x + y*stride + 0]= b;
	buf[3*x + y*stride + 1]= g;
	buf[3*x + y*stride + 2]= r;
    break;
    case IMGFMT_RGBA:
	buf[4*x + y*stride + 0]= r;
	buf[4*x + y*stride + 1]= g;
	buf[4*x + y*stride + 2]= b;
    break;
    case IMGFMT_BGRA:
	buf[4*x + y*stride + 0]= b;
	buf[4*x + y*stride + 1]= g;
	buf[4*x + y*stride + 2]= r;
    break;
    case IMGFMT_ARGB:
	buf[4*x + y*stride + 1]= r;
	buf[4*x + y*stride + 2]= g;
	buf[4*x + y*stride + 3]= b;
    break;
    case IMGFMT_ABGR:
	buf[4*x + y*stride + 1]= b;
	buf[4*x + y*stride + 2]= g;
	buf[4*x + y*stride + 3]= r;
    break;
    }
}

static int __FASTCALL__ rgb_config(struct vf_instance_s* vf,
	int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt){
    vf->priv->fmt=rgb_getfmt(outfmt);
    MSG_V("rgb test format:%s\n", vo_format_name(outfmt));
    return vf_next_config(vf,width,height,d_width,d_height,flags,vf->priv->fmt);
}

static int __FASTCALL__ rgb_put_slice(struct vf_instance_s* vf, mp_image_t *mpi){
    mp_image_t *dmpi;
    int x, y;

    // hope we'll get DR buffer:
    dmpi=vf_get_new_image(vf->next,vf->priv->fmt,
	MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE,
	mpi->w, mpi->h, mpi->xp_idx);

     for(y=0; y<mpi->h; y++){
	 for(x=0; x<mpi->w; x++){
	     int c= 256*x/mpi->w;
	     int r=0,g=0,b=0;

	     if(3*y<mpi->h)        r=c;
	     else if(3*y<2*mpi->h) g=c;
	     else                  b=c;

	     rgb_put_pixel(dmpi->planes[0], x, y, dmpi->stride[0], r, g, b, vf->priv->fmt);
	 }
     }

    return vf_next_put_slice(vf,dmpi);
}

static int __FASTCALL__ rgb_query_format(struct vf_instance_s* vf, unsigned int outfmt,unsigned w,unsigned h){
    unsigned int fmt=rgb_getfmt(outfmt);
    if(!fmt) return 0;
    return vf_next_query_format(vf,fmt,w,h) & (~VFCAP_CSP_SUPPORTED_BY_HW);
}

//===========================================================================//

static int __FASTCALL__ vf_config(struct vf_instance_s* vf,
	int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt){

    if(vf_next_query_format(vf,IMGFMT_YV12,d_width,d_height)<=0){
	printf("yv12 not supported by next filter/vo :(\n");
	return 0;
    }

    //hmm whats the meaning of these ... ;)
    d_width= width= WIDTH;
    d_height= height= HEIGHT;

    return vf_next_config(vf,width,height,d_width,d_height,flags,IMGFMT_YV12);
}

static double c[64];

static void initIdct(void)
{
	int i;

	for (i=0; i<8; i++)
	{
		double s= i==0 ? sqrt(0.125) : 0.5;
		int j;

		for(j=0; j<8; j++)
			c[i*8+j]= s*cos((3.141592654/8.0)*i*(j+0.5));
	}
}


static void __FASTCALL__ idct(uint8_t *dst, int dstStride, int src[64])
{
	int i, j, k;
	double tmp[64];

	for(i=0; i<8; i++)
	{
		for(j=0; j<8; j++)
		{
			double sum= 0.0;

			for(k=0; k<8; k++)
				sum+= c[k*8+j]*src[8*i+k];

			tmp[8*i+j]= sum;
		}
	}

	for(j=0; j<8; j++)
	{
		for(i=0; i<8; i++)
		{
			int v;
			double sum= 0.0;

			for(k=0; k<8; k++)
				sum+= c[k*8+i]*tmp[8*k+j];

			v= (int)floor(sum+0.5);
			if(v<0) v=0;
			else if(v>255) v=255;

			dst[dstStride*i + j] = v;
		}
	}
}

static void __FASTCALL__ drawDc(uint8_t *dst, int stride, int color, int w, int h)
{
	int y;
	for(y=0; y<h; y++)
	{
		int x;
		for(x=0; x<w; x++)
		{
			dst[x + y*stride]= color;
		}
	}
}

static void __FASTCALL__ drawBasis(uint8_t *dst, int stride, int amp, int freq, int dc)
{
	int src[64];

	memset(src, 0, 64*sizeof(int));
	src[0]= dc;
	if(amp) src[freq]= amp;
	idct(dst, stride, src);
}

static void __FASTCALL__ drawCbp(uint8_t *dst[3], int stride[3], int cbp, int amp, int dc)
{
	if(cbp&1) drawBasis(dst[0]              , stride[0], amp, 1, dc);
	if(cbp&2) drawBasis(dst[0]+8            , stride[0], amp, 1, dc);
	if(cbp&4) drawBasis(dst[0]+  8*stride[0], stride[0], amp, 1, dc);
	if(cbp&8) drawBasis(dst[0]+8+8*stride[0], stride[0], amp, 1, dc);
	if(cbp&16)drawBasis(dst[1]              , stride[1], amp, 1, dc);
	if(cbp&32)drawBasis(dst[2]              , stride[2], amp, 1, dc);
}

static void __FASTCALL__ dc1Test(uint8_t *dst, int stride, int w, int h, int off)
{
	const int step= std::max(256/(w*h/256), 1);
	int y;
	int color=off;
	for(y=0; y<h; y+=16)
	{
		int x;
		for(x=0; x<w; x+=16)
		{
			drawDc(dst + x + y*stride, stride, color, 8, 8);
			color+=step;
		}
	}
}

static void __FASTCALL__ freq1Test(uint8_t *dst, int stride, int off)
{
	int y;
	int freq=0;
	for(y=0; y<8*16; y+=16)
	{
		int x;
		for(x=0; x<8*16; x+=16)
		{
			drawBasis(dst + x + y*stride, stride, 4*(96+off), freq, 128*8);
			freq++;
		}
	}
}

static void __FASTCALL__ amp1Test(uint8_t *dst, int stride, int off)
{
	int y;
	int amp=off;
	for(y=0; y<16*16; y+=16)
	{
		int x;
		for(x=0; x<16*16; x+=16)
		{
			drawBasis(dst + x + y*stride, stride, 4*(amp), 1, 128*8);
			amp++;
		}
	}
}

static void __FASTCALL__ cbp1Test(uint8_t *dst[3], int stride[3], int off)
{
	int y;
	int cbp=0;
	for(y=0; y<16*8; y+=16)
	{
		int x;
		for(x=0; x<16*8; x+=16)
		{
			uint8_t *dst1[3];
			dst1[0]= dst[0] + x*2 + y*2*stride[0];
			dst1[1]= dst[1] + x + y*stride[1];
			dst1[2]= dst[2] + x + y*stride[2];

			drawCbp(dst1, stride, cbp, (64+off)*4, 128*8);
			cbp++;
		}
	}
}

static void __FASTCALL__ mv1Test(uint8_t *dst, int stride, int off)
{
	int y;
	for(y=0; y<16*16; y++)
	{
		int x;
		if(y&16) continue;
		for(x=0; x<16*16; x++)
		{
			dst[x + y*stride]= x + off*8/(y/32+1);
		}
	}
}

static void __FASTCALL__ ring1Test(uint8_t *dst, int stride, int off)
{
	int y;
	int color=0;
	for(y=off; y<16*16; y+=16)
	{
		int x;
		for(x=off; x<16*16; x+=16)
		{
			drawDc(dst + x + y*stride, stride, ((x+y)&16) ? color : -color, 16, 16);
//			dst[x + y*stride]= 255 + (off&1);
			color++;
		}
	}
}

static void __FASTCALL__ ring2Test(uint8_t *dst, int stride, int off)
{
	int y;
	for(y=0; y<16*16; y++)
	{
		int x;
		for(x=0; x<16*16; x++)
		{
			double d= sqrt((x-8*16)*(x-8*16) + (y-8*16)*(y-8*16));
			double r= d/20 - (int)(d/20);
			if(r<off/30.0)
			{
				dst[x + y*stride]= 255;
				dst[x + y*stride+256]= 0;
			}
			else{
				dst[x + y*stride]= x;
				dst[x + y*stride+256]= x;
			}
		}
	}
}

static int __FASTCALL__ put_slice(struct vf_instance_s* vf, mp_image_t *mpi){
    mp_image_t *dmpi;
    int frame= vf->priv->frame_num;

    // hope we'll get DR buffer:
    dmpi=vf_get_new_image(vf->next,IMGFMT_YV12,
	MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE,
	mpi->w,mpi->h,mpi->xp_idx);

    // clean
    memset(dmpi->planes[0], 0, dmpi->stride[0]*dmpi->h);
    memset(dmpi->planes[1], 128, dmpi->stride[1]*dmpi->h>>dmpi->chroma_y_shift);
    memset(dmpi->planes[2], 128, dmpi->stride[2]*dmpi->h>>dmpi->chroma_y_shift);

    if(frame%30)
    {
	switch(frame/30)
	{
	case 0:   dc1Test(dmpi->planes[0], dmpi->stride[0], 256, 256, frame%30); break;
	case 1:   dc1Test(dmpi->planes[1], dmpi->stride[1], 256, 256, frame%30); break;
	case 2: freq1Test(dmpi->planes[0], dmpi->stride[0], frame%30); break;
	case 3: freq1Test(dmpi->planes[1], dmpi->stride[1], frame%30); break;
	case 4:  amp1Test(dmpi->planes[0], dmpi->stride[0], frame%30); break;
	case 5:  amp1Test(dmpi->planes[1], dmpi->stride[1], frame%30); break;
	case 6:  cbp1Test(dmpi->planes   , reinterpret_cast<int*>(dmpi->stride), frame%30); break;
	case 7:   mv1Test(dmpi->planes[0], dmpi->stride[0], frame%30); break;
	case 8: ring1Test(dmpi->planes[0], dmpi->stride[0], frame%30); break;
	case 9: ring2Test(dmpi->planes[0], dmpi->stride[0], frame%30); break;
	}
    }

    frame++;
    vf->priv->frame_num= frame;
    return vf_next_put_slice(vf,dmpi);
}

//===========================================================================//

static int __FASTCALL__ query_format(struct vf_instance_s* vf, unsigned int fmt,unsigned w,unsigned h){
    UNUSED(fmt);
    return vf_next_query_format(vf,IMGFMT_YV12,w,h) & (~VFCAP_CSP_SUPPORTED_BY_HW);
}

static MPXP_Rc __FASTCALL__ vf_open(vf_instance_t *vf,const char* args){
    char method;
    vf->config_vf=vf_config;
    vf->put_slice=put_slice;
    vf->query_format=query_format;
    vf->priv=new(zeromem) struct vf_priv_s;
    method='!';
    if(args) {
	sscanf(args,"%c",&method);
	args++;
	if(*args) args++;
	if(method!='r' && *args)
	{
	    vf->priv->frame_num= atoi(args);
	}
    }
    if(method=='r') {
	vf->config_vf=rgb_config;
	vf->put_slice=rgb_put_slice;
	vf->query_format=rgb_query_format;
    }
    else {
	initIdct();
    }
    check_pin("vfilter",vf->pin,VF_PIN);
    return MPXP_Ok;
}

extern const vf_info_t vf_info_test = {
    "test pattern generator",
    "test",
    "Michael Niedermayer",
    "",
    VF_FLAGS_THREADS,
    vf_open
};

//===========================================================================//
