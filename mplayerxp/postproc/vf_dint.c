#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "../mp_config.h"
#include "../libvo/fastmemcpy.h"

#include "mp_image.h"
#include "../libvo/img_format.h"
#include "vf.h"
#include "pp_msg.h"

struct vf_priv_s {
  float sense; // first parameter
  float level; // second parameter
  unsigned int imgfmt;
  char diff;
  uint32_t max;
  int was_dint;
  mp_image_t *pmpi; // previous mpi

    int	frame;
    int	map;
    int	order;
    int	thresh;
    int	sharp;
    int	twoway;
};

/***************************************************************************/


static int __FASTCALL__ kd_config(struct vf_instance_s* vf,
        int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt,any_t*tune){

	return vf_next_config(vf,width,height,d_width,d_height,flags,outfmt,tune);
}


static void __FASTCALL__ uninit(struct vf_instance_s* vf)
{
	free(vf->priv);
}

static inline int IsRGB(mp_image_t *mpi)
{
	return mpi->imgfmt == IMGFMT_RGB;
}

static inline int IsYUY2(mp_image_t *mpi)
{
	return mpi->imgfmt == IMGFMT_YUY2;
}

#define PLANAR_Y 0
#define PLANAR_U 1
#define PLANAR_V 2

static int __FASTCALL__ kd_put_slice(struct vf_instance_s* vf, mp_image_t *mpi){
	int cw= mpi->w >> mpi->chroma_x_shift;
	int ch= mpi->h >> mpi->chroma_y_shift;
        int W = mpi->w, H = mpi->h;
	const unsigned char *prvp, *prvpp, *prvpn, *prvpnn, *prvppp, *prvp4p, *prvp4n;
	const unsigned char *srcp_saved;
	const unsigned char *srcp, *srcpp, *srcpn, *srcpnn, *srcppp, *srcp3p, *srcp3n, *srcp4p, *srcp4n;
	unsigned char *dstp, *dstp_saved;
	int src_pitch;
	int psrc_pitch;
	int dst_pitch;
	int x, y, z;
	int n = vf->priv->frame++;
	int val, hi, lo, w, h;
	double valf;
	int plane;
	int threshold = vf->priv->thresh;
	int order = vf->priv->order;
	int map = vf->priv->map;
	int sharp = vf->priv->sharp;
	int twoway = vf->priv->twoway;
	int finalize;

	mp_image_t *pmpi;
	mp_image_t *dmpi=vf_get_image(vf->next,mpi->imgfmt,
		MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE,
		mpi->w,mpi->h,mpi->xp_idx);
	if(!dmpi) return 0;
	pmpi=dmpi;
	finalize = dmpi->flags&MP_IMGFLAG_FINALIZED;

#ifdef _OPENMP
#pragma omp parallel for
#endif
	for (z=0; z<mpi->num_planes; z++) {
		if (z == 0) plane = PLANAR_Y;
		else if (z == 1) plane = PLANAR_U;
		else plane = PLANAR_V;

		h = plane == PLANAR_Y ? H : ch;
		w = plane == PLANAR_Y ? W : cw;

		srcp = srcp_saved = mpi->planes[z];
		src_pitch = mpi->stride[z];
		psrc_pitch = pmpi->stride[z];
		dstp = dstp_saved = dmpi->planes[z];
		dst_pitch = dmpi->stride[z];
		srcp = srcp_saved + (1-order) * src_pitch;
		dstp = dstp_saved + (1-order) * dst_pitch;

		for (y=0; y<h; y+=2) {
			if(finalize)
			    stream_copy(dstp,srcp, w);
			else
			    memcpy(dstp, srcp, w);
			srcp += 2*src_pitch;
			dstp += 2*dst_pitch;
		}

		// Copy through the lines that will be missed below.
		if(finalize) {
		    stream_copy(dstp_saved + order*dst_pitch, srcp_saved + (1-order)*src_pitch, w);
		    stream_copy(dstp_saved + (2+order)*dst_pitch, srcp_saved + (3-order)*src_pitch, w);
		    stream_copy(dstp_saved + (h-2+order)*dst_pitch, srcp_saved + (h-1-order)*src_pitch, w);
		    stream_copy(dstp_saved + (h-4+order)*dst_pitch, srcp_saved + (h-3-order)*src_pitch, w);
		} else {
		    memcpy(dstp_saved + order*dst_pitch, srcp_saved + (1-order)*src_pitch, w);
		    memcpy(dstp_saved + (2+order)*dst_pitch, srcp_saved + (3-order)*src_pitch, w);
		    memcpy(dstp_saved + (h-2+order)*dst_pitch, srcp_saved + (h-1-order)*src_pitch, w);
		    memcpy(dstp_saved + (h-4+order)*dst_pitch, srcp_saved + (h-3-order)*src_pitch, w);
		}
		/* For the other field choose adaptively between using the previous field
		   or the interpolant from the current field. */

		prvp = pmpi->planes[z] + 5*psrc_pitch - (1-order)*psrc_pitch;
		prvpp = prvp - psrc_pitch;
		prvppp = prvp - 2*psrc_pitch;
		prvp4p = prvp - 4*psrc_pitch;
		prvpn = prvp + psrc_pitch;
		prvpnn = prvp + 2*psrc_pitch;
		prvp4n = prvp + 4*psrc_pitch;
		srcp = srcp_saved + 5*src_pitch - (1-order)*src_pitch;
		srcpp = srcp - src_pitch;
		srcppp = srcp - 2*src_pitch;
		srcp3p = srcp - 3*src_pitch;
		srcp4p = srcp - 4*src_pitch;
		srcpn = srcp + src_pitch;
		srcpnn = srcp + 2*src_pitch;
		srcp3n = srcp + 3*src_pitch;
		srcp4n = srcp + 4*src_pitch;
		dstp =  dstp_saved  + 5*dst_pitch - (1-order)*dst_pitch;
		for (y = 5 - (1-order); y <= h - 5 - (1-order); y+=2)
		{
			for (x = 0; x < w; x++)
			{
				if ((threshold == 0) || (n == 0) ||
					(abs((int)prvp[x] - (int)srcp[x]) > threshold) ||
					(abs((int)prvpp[x] - (int)srcpp[x]) > threshold) ||
					(abs((int)prvpn[x] - (int)srcpn[x]) > threshold))
				{
					if (map == 1)
					{
						int g = x & ~3;
						if (IsRGB(mpi) == 1)
						{
							dstp[g++] = 255;
							dstp[g++] = 255;
							dstp[g++] = 255;
							dstp[g] = 255;
							x = g;
						}
						else if (IsYUY2(mpi) == 1)
						{
							dstp[g++] = 235;
							dstp[g++] = 128;
							dstp[g++] = 235;
							dstp[g] = 128;
							x = g;
						}
						else
						{
							if (plane == PLANAR_Y) dstp[x] = 235;
							else dstp[x] = 128;
						}
					}
					else
					{
						if (IsRGB(mpi))
						{
							hi = 255;
							lo = 0;
						}
						else if (IsYUY2(mpi))
						{
							hi = (x & 1) ? 240 : 235;
							lo = 16;
						}
						else
						{
							hi = (plane == PLANAR_Y) ? 235 : 240;
							lo = 16;
						}

						if (sharp == 1)
						{
							if (twoway == 1)
								valf = + 0.526*((int)srcpp[x] + (int)srcpn[x])
								   + 0.170*((int)srcp[x] + (int)prvp[x])
								   - 0.116*((int)srcppp[x] + (int)srcpnn[x] + (int)prvppp[x] + (int)prvpnn[x])
					 			   - 0.026*((int)srcp3p[x] + (int)srcp3n[x])
								   + 0.031*((int)srcp4p[x] + (int)srcp4n[x] + (int)prvp4p[x] + (int)prvp4n[x]);
							else
								valf = + 0.526*((int)srcpp[x] + (int)srcpn[x])
								   + 0.170*((int)prvp[x])
								   - 0.116*((int)prvppp[x] + (int)prvpnn[x])
					 			   - 0.026*((int)srcp3p[x] + (int)srcp3n[x])
								   + 0.031*((int)prvp4p[x] + (int)prvp4p[x]);
							if (valf > hi) valf = hi;
							else if (valf < lo) valf = lo;
							dstp[x] = (int) valf;
						}
						else
						{
							if (twoway == 1)
								val = (8*((int)srcpp[x] + (int)srcpn[x]) + 2*((int)srcp[x] + (int)prvp[x]) -
									(int)(srcppp[x]) - (int)(srcpnn[x]) -
									(int)(prvppp[x]) - (int)(prvpnn[x])) >> 4;
							else
								val = (8*((int)srcpp[x] + (int)srcpn[x]) + 2*((int)prvp[x]) -
									(int)(prvppp[x]) - (int)(prvpnn[x])) >> 4;
							if (val > hi) val = hi;
							else if (val < lo) val = lo;
							dstp[x] = (int) val;
						}
					}
				}
				else
				{
					dstp[x] = srcp[x];
				}
			}
			prvp  += 2*psrc_pitch;
			prvpp  += 2*psrc_pitch;
			prvppp  += 2*psrc_pitch;
			prvpn  += 2*psrc_pitch;
			prvpnn  += 2*psrc_pitch;
			prvp4p  += 2*psrc_pitch;
			prvp4n  += 2*psrc_pitch;
			srcp  += 2*src_pitch;
			srcpp += 2*src_pitch;
			srcppp += 2*src_pitch;
			srcp3p += 2*src_pitch;
			srcp4p += 2*src_pitch;
			srcpn += 2*src_pitch;
			srcpnn += 2*src_pitch;
			srcp3n += 2*src_pitch;
			srcp4n += 2*src_pitch;
			dstp  += 2*dst_pitch;
		}

		srcp = mpi->planes[z];
		dstp = pmpi->planes[z];
		for (y=0; y<h; y++) {
			if(finalize)
			    stream_copy(dstp,srcp, w);
			else
			    memcpy(dstp, srcp, w);
			srcp += src_pitch;
			dstp += psrc_pitch;
		}
	}

	return vf_next_put_slice(vf,dmpi);
}

//===========================================================================//

static int __FASTCALL__ kd_query_format(struct vf_instance_s* vf, unsigned int fmt,unsigned w,unsigned h){
        switch(fmt)
	{
	case IMGFMT_YV12:
	case IMGFMT_RGB:
	case IMGFMT_YUY2:
		return vf_next_query_format(vf, fmt,w,h);
	}
	return 0;
}

static void __FASTCALL__ print_conf(struct vf_instance_s* vf)
{
    MSG_INFO("Drop-interlaced: %dx%d diff %d / level %u\n",
	   vf->priv->pmpi->width, vf->priv->pmpi->height,
	   (int)vf->priv->diff, (unsigned int)vf->priv->max);
}
#define MAXROWSIZE 1200

static int __FASTCALL__ config (struct vf_instance_s* vf,
        int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt,any_t*tune)
{
    int rowsize;

    vf->priv->pmpi = vf_get_image (vf->next, outfmt, MP_IMGTYPE_TEMP,
				   0,width, height, XP_IDX_INVALID);
    if (!(vf->priv->pmpi->flags & MP_IMGFLAG_PLANAR) &&
	outfmt != IMGFMT_RGB32 && outfmt != IMGFMT_BGR32 &&
	outfmt != IMGFMT_RGB24 && outfmt != IMGFMT_BGR24 &&
	outfmt != IMGFMT_RGB16 && outfmt != IMGFMT_BGR16)
    {
      MSG_WARN("Drop-interlaced filter doesn't support this outfmt :(\n");
      return 0;
    }
    vf->priv->imgfmt = outfmt;
    // recalculate internal values
    rowsize = vf->priv->pmpi->width;
    if (rowsize > MAXROWSIZE) rowsize = MAXROWSIZE;
    vf->priv->max = vf->priv->level * vf->priv->pmpi->height * rowsize / 2;
    if (vf->priv->pmpi->flags & MP_IMGFLAG_PLANAR) // planar YUV
      vf->priv->diff = vf->priv->sense * 256;
    else
      vf->priv->diff = vf->priv->sense * (1 << (vf->priv->pmpi->bpp/3));
    if (vf->priv->diff < 0) vf->priv->diff = 0;
    if (!(vf->priv->pmpi->flags & MP_IMGFLAG_PLANAR) &&
	vf->priv->pmpi->bpp < 24 && vf->priv->diff > 31)
      vf->priv->diff = 31;
//    vf->priv->rdfr = vf->priv->dfr = 0;
    vf->priv->was_dint = 0;
    return vf_next_config(vf,width,height,d_width,d_height,flags,outfmt,tune);
}

static int __FASTCALL__ put_slice (struct vf_instance_s* vf, mp_image_t *mpi)
{
    char rrow0[MAXROWSIZE];
    char rrow1[MAXROWSIZE];
    char rrow2[MAXROWSIZE];
    char *row0 = rrow0, *row1 = rrow1, *row2 = rrow2/*, *row3 = rrow3*/;
    int rowsize = mpi->width;
    uint32_t nok = 0, max = vf->priv->max;
    int diff = vf->priv->diff;
    int i, j;
    register int n1, n2;
    unsigned char *cur0, *prv0;
    register unsigned char *cur, *prv;

    // check if nothing to do
    if (mpi->imgfmt == vf->priv->imgfmt)
    {
      cur0 = mpi->planes[0] + mpi->stride[0];
      prv0 = mpi->planes[0];
      for (j = 1; j < mpi->height && nok <= max; j++)
      {
	cur = cur0;
	prv = prv0;
	// analyse row (row0)
	if (mpi->flags & MP_IMGFLAG_PLANAR) // planar YUV - check luminance
	  for (i = 0; i < rowsize; i++)
	  {
	    if (cur[0] - prv[0] > diff)
	      row0[i] = 1;
	    else if (cur[0] - prv[0] < -diff)
	      row0[i] = -1;
	    else
	      row0[i] = 0;
	    cur++;
	    prv++;
	    // check if row0 is 1 but row1 is 0, and row2 is 1 or row2 is 0
	    // but row3 is 1 so it's interlaced ptr (nok++)
	    if (j > 2 && row0[i] > 0 && (row1[i] < 0 || (!row1[i] && row2[i] < 0)) &&
		(++nok) > max)
	      break;
	  }
	else if (mpi->bpp < 24) // RGB/BGR 16 - check all colors
	  for (i = 0; i < rowsize; i++)
	  {
	    n1 = cur[0] + (cur[1]<<8);
	    n2 = prv[0] + (prv[1]<<8);
	    if ((n1&0x1f) - (n2&0x1f) > diff ||
		((n1>>5)&0x3f) - ((n2>>5)&0x3f) > diff ||
		((n1>>11)&0x1f) - ((n2>>11)&0x1f) > diff)
	      row0[i] = 1;
	    else if ((n1&0x1f) - (n2&0x1f) < -diff ||
		     ((n1>>5)&0x3f) - ((n2>>5)&0x3f) < -diff ||
		     ((n1>>11)&0x1f) - ((n2>>11)&0x1f) < -diff)
	      row0[i] = -1;
	    else
	      row0[i] = 0;
	    cur += 2;
	    prv += 2;
	    // check if row0 is 1 but row1 is 0, and row2 is 1 or row2 is 0
	    // but row3 is 1 so it's interlaced ptr (nok++)
	    if (j > 2 && row0[i] > 0 && (row1[i] < 0 || (!row1[i] && row2[i] < 0)) &&
		(++nok) > max)
	      break;
	  }
	else // RGB/BGR 24/32
	  for (i = 0; i < rowsize; i++)
	  {
	    if (cur[0] - prv[0] > diff ||
		cur[1] - prv[1] > diff ||
		cur[2] - prv[2] > diff)
	      row0[i] = 1;
	    else if (prv[0] - cur[0] > diff ||
		     prv[1] - cur[1] > diff ||
		     prv[2] - cur[2] > diff)
	      row0[i] = -1;
	    else
	      row0[i] = 0;
	    cur += mpi->bpp/8;
	    prv += mpi->bpp/8;
	    // check if row0 is 1 but row1 is 0, and row2 is 1 or row2 is 0
	    // but row3 is 1 so it's interlaced ptr (nok++)
	    if (j > 2 && row0[i] > 0 && (row1[i] < 0 || (!row1[i] && row2[i] < 0)) &&
		(++nok) > max)
	      break;
	  }
	cur0 += mpi->stride[0];
	prv0 += mpi->stride[0];
	// rotate rows
	cur = row2;
	row2 = row1;
	row1 = row0;
	row0 = cur;
      }
    }
    // check if number of interlaced is above of max
    if (nok > max)
    {
//      vf->priv->dfr++;
      if (vf->priv->was_dint < 1) // can skip at most one frame!
      {
	vf->priv->was_dint++;
//	vf->priv->rdfr++;
	return 0;
      }
    }
    vf->priv->was_dint = 0;
    return vf_next_put_slice (vf, mpi);
}

static int __FASTCALL__ vf_open (vf_instance_t *vf,const char* args){
    int e;
    float a,b;
    vf->config = config;
    vf->put_slice = put_slice;
    vf->print_conf = print_conf;
//    vf->default_reqs=VFCAP_ACCEPT_STRIDE;
    vf->priv = malloc (sizeof(struct vf_priv_s));
    vf->priv->sense = 0.1;
    vf->priv->level = 0.15;
    vf->priv->pmpi = NULL;

    vf->priv->map = 0;
    vf->priv->order = 0;
    vf->priv->thresh = 10;
    vf->priv->sharp = 0;
    vf->priv->twoway = 0;
    e=0;
    if (args)
    {
	e=sscanf(args, "%f:%f:%d:%d:%d",
		&a, &b,
		&vf->priv->order, &vf->priv->sharp,
		&vf->priv->twoway);
	if(e==2)
	{
	    vf->priv->sense=a;
	    vf->priv->level=b;
	}
	else
	{
	    if(e!=5) return 0;
	    vf->priv->thresh=a;
	    vf->priv->map=b;
	    vf->uninit=uninit;
	    vf->config=kd_config;
	    vf->put_slice = kd_put_slice;
	    vf->query_format=kd_query_format;
	}
    }
    return 1;
}

const vf_info_t vf_info_dint = {
    "Kernel Deinterlacer/drop interlaced frames",
    "dint",
    "Donald Graft/A.G.",
    "",
    VF_FLAGS_THREADS,
    vf_open
};
