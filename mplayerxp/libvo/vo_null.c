/*
 *  video_out_null.c
 *
 *	Copyright (C) Aaron Holtzman - June 2000
 *
 *  This file is part of mpeg2dec, a mp_free MPEG-2 video stream decoder.
 *
 *  mpeg2dec is mp_free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  mpeg2dec is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
#include "mp_config.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "osdep/mplib.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "dri_vo.h"
#ifdef CONFIG_VIDIX
#include <vidix/vidixlib.h>
#endif
#include "vo_msg.h"

LIBVO_EXTERN(null)

static vo_info_t vo_info =
{
	"Null video output",
	"null",
	"Aaron Holtzman <aholtzma@ess.engr.uvic.ca>",
	""
};

typedef struct priv_s {
    uint32_t	image_width, image_height,frame_size,fourcc;
    uint8_t *	bm_buffs[MAX_DRI_BUFFERS];
    uint32_t	num_frames;
    uint32_t	pitch_y,pitch_u,pitch_v;
    uint32_t	offset_y,offset_u,offset_v;
}priv_t;

static void __FASTCALL__ select_frame(vo_data_t*vo,unsigned idx)
{
    UNUSED(vo);
    UNUSED(idx);
}

static MPXP_Rc __FASTCALL__ config(vo_data_t*vo,uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t fullscreen, char *title, uint32_t format)
{
    priv_t*priv=(priv_t*)vo->priv;
    unsigned awidth;
    size_t i;
    priv->image_width = width;
    priv->image_height = height;
    priv->num_frames = vo_conf.xp_buffs;
    priv->fourcc=format;
    UNUSED(d_width);
    UNUSED(d_height);
    UNUSED(fullscreen);
    UNUSED(title);
    priv->pitch_y=priv->pitch_u=priv->pitch_v=1;
    priv->offset_y=priv->offset_u=priv->offset_v=0;
    switch(format) {
    case IMGFMT_Y800:
		awidth = (width + (priv->pitch_y-1)) & ~(priv->pitch_y-1);
		priv->frame_size = awidth*height;
		break;
    case IMGFMT_YVU9:
    case IMGFMT_IF09:
		awidth = (width + (priv->pitch_y-1)) & ~(priv->pitch_y-1);
		priv->frame_size = awidth*(height+height/8);
		priv->offset_u=awidth*height;
		priv->offset_v=awidth*height/16;
		break;
    case IMGFMT_I420:
    case IMGFMT_YV12:
    case IMGFMT_IYUV:
		awidth = (width + (priv->pitch_y-1)) & ~(priv->pitch_y-1);
		priv->frame_size = awidth*(height+height/2);
		priv->offset_u=awidth*height;
		priv->offset_v=awidth*height/4;
		break;
    case IMGFMT_RGB32:
    case IMGFMT_BGR32:
		awidth = (width*4 + (priv->pitch_y-1)) & ~(priv->pitch_y-1);
		priv->frame_size = awidth*height;
		break;
    /* YUY2 YVYU, RGB15, RGB16 */
    default:
		awidth = (width*2 + (priv->pitch_y-1)) & ~(priv->pitch_y-1);
		priv->frame_size = awidth*height;
		break;
    }
    for(i=0;i<priv->num_frames;i++) {
	if(!priv->bm_buffs[i])
#ifdef HAVE_MEMALIGN
	    priv->bm_buffs[i] = mp_memalign(getpagesize(),priv->frame_size);
#else
	    priv->bm_buffs[i] = mp_malloc(priv->frame_size);
#endif
	if(!(priv->bm_buffs[i])) {
		MSG_ERR("Can't allocate memory for busmastering\n");
		return MPXP_False;
	}
    }
    return MPXP_Ok;
}

static const vo_info_t* get_info(const vo_data_t*vo)
{
    UNUSED(vo);
    return &vo_info;
}

static void uninit(vo_data_t*vo)
{
    priv_t*priv=(priv_t*)vo->priv;
    size_t i;
    for(i=0;i<priv->num_frames;i++) {
	mp_free(priv->bm_buffs[i]);
	priv->bm_buffs[i]=NULL;
    }
    mp_free(priv);
}

static MPXP_Rc __FASTCALL__ preinit(vo_data_t*vo,const char *arg)
{
    if(arg) {
	MSG_ERR("vo_null: Unknown subdevice: %s\n",arg);
	return MPXP_False;
    }
    vo->priv=mp_mallocz(sizeof(priv_t));
    return MPXP_Ok;
}

static void __FASTCALL__ null_dri_get_surface_caps(const vo_data_t*vo,dri_surface_cap_t *caps)
{
    priv_t*priv=(priv_t*)vo->priv;
    caps->caps =DRI_CAP_TEMP_VIDEO |
		DRI_CAP_HORZSCALER | DRI_CAP_VERTSCALER |
		DRI_CAP_DOWNSCALER | DRI_CAP_UPSCALER;
    caps->fourcc = priv->fourcc;
    caps->width=priv->image_width;
    caps->height=priv->image_height;
    /* in case of vidix movie fit surface */
    caps->x = caps->y=0;
    caps->w=caps->width;
    caps->h=caps->height;
    caps->strides[0] = priv->pitch_y;
    caps->strides[1] = priv->pitch_v;
    caps->strides[2] = priv->pitch_u;
    caps->strides[3] = 0;
}

static void __FASTCALL__ null_dri_get_surface(const vo_data_t*vo,dri_surface_t *surf)
{
    priv_t*priv=(priv_t*)vo->priv;
    surf->planes[0] = priv->bm_buffs[surf->idx] + priv->offset_y;
    surf->planes[1] = priv->bm_buffs[surf->idx] + priv->offset_v;
    surf->planes[2] = priv->bm_buffs[surf->idx] + priv->offset_u;
    surf->planes[3] = 0;
}

static int __FASTCALL__ null_query_format(vo_query_fourcc_t* format) {
    /* we must avoid compressed-fourcc here */
    switch(format->fourcc) {
    case IMGFMT_444P16_LE:
    case IMGFMT_444P16_BE:
    case IMGFMT_422P16_LE:
    case IMGFMT_422P16_BE:
    case IMGFMT_420P16_LE:
    case IMGFMT_420P16_BE:
    case IMGFMT_420A:
    case IMGFMT_444P:
    case IMGFMT_422P:
    case IMGFMT_YV12:
    case IMGFMT_I420:
    case IMGFMT_IYUV:
    case IMGFMT_YVU9:
    case IMGFMT_IF09:
    case IMGFMT_411P:
    case IMGFMT_YUY2:
    case IMGFMT_UYVY:
// RGB and grayscale (Y8 and Y800):
    case IMGFMT_RGB48LE:
    case IMGFMT_RGB48BE:
    case IMGFMT_BGR32:
    case IMGFMT_RGB32:
    case IMGFMT_BGR24:
    case IMGFMT_RGB24:
    case IMGFMT_BGR16:
    case IMGFMT_RGB16:
    case IMGFMT_BGR15:
    case IMGFMT_RGB15:
    case IMGFMT_Y800:
    case IMGFMT_Y8:
    case IMGFMT_BGR8:
    case IMGFMT_RGB8:
    case IMGFMT_BGR4:
    case IMGFMT_RGB4:
    case IMGFMT_BG4B:
    case IMGFMT_RG4B:
    case IMGFMT_BGR1:
    case IMGFMT_RGB1: return MPXP_True;
    }
    return MPXP_False;
}

static MPXP_Rc __FASTCALL__ control(vo_data_t*vo,uint32_t request, any_t*data)
{
    priv_t*priv=(priv_t*)vo->priv;
  switch (request) {
    case VOCTRL_QUERY_FORMAT:
	return null_query_format(data);
    case VOCTRL_GET_NUM_FRAMES:
	*(uint32_t *)data = priv->num_frames;
	return MPXP_True;
    case DRI_GET_SURFACE_CAPS:
	null_dri_get_surface_caps(vo,data);
	return MPXP_True;
    case DRI_GET_SURFACE:
	null_dri_get_surface(vo,data);
	return MPXP_True;
    case VOCTRL_FLUSH_PAGES:
	return MPXP_True;
  }
  return MPXP_NA;
}
