/* 
 *  video_out_null.c
 *
 *	Copyright (C) Aaron Holtzman - June 2000
 *
 *  This file is part of mpeg2dec, a free MPEG-2 video stream decoder.
 *	
 *  mpeg2dec is free software; you can redistribute it and/or modify
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
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#ifdef HAVE_MEMALIGN
#include <malloc.h>
#endif
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

static uint32_t null_image_width, null_image_height,null_frame_size,null_fourcc;
static uint8_t *bm_buffs[MAX_DRI_BUFFERS];
static uint32_t null_num_frames;
static uint32_t pitch_y,pitch_u,pitch_v;
static uint32_t offset_y,offset_u,offset_v;


static void __FASTCALL__ change_frame(unsigned idx)
{
    UNUSED(idx);
}

static uint32_t __FASTCALL__ config(uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t fullscreen, char *title, uint32_t format,const vo_tune_info_t *info)
{
    unsigned awidth;
    size_t i;
    null_image_width = width;
    null_image_height = height;
    null_num_frames = vo_da_buffs;
    null_fourcc=format;
    UNUSED(d_width);
    UNUSED(d_height);
    UNUSED(fullscreen);
    UNUSED(title);
    pitch_y=pitch_u=pitch_v=1;
    if(info)
    {
	switch(((const vo_tune_info_t *)info)->pitch[0])
	{
	    case 2:
	    case 4:
	    case 8:
	    case 16:
	    case 32:
	    case 64:
	    case 128:
	    case 256: pitch_y = ((const vo_tune_info_t *)info)->pitch[0];
		      break;
	    default: break;
	}
	switch(((const vo_tune_info_t *)info)->pitch[1])
	{
	    case 2:
	    case 4:
	    case 8:
	    case 16:
	    case 32:
	    case 64:
	    case 128:
	    case 256: pitch_u = ((const vo_tune_info_t *)info)->pitch[1];
		      break;
	    default: break;
	}
	switch(((const vo_tune_info_t *)info)->pitch[2])
	{
	    case 2:
	    case 4:
	    case 8:
	    case 16:
	    case 32:
	    case 64:
	    case 128:
	    case 256: pitch_v = ((const vo_tune_info_t *)info)->pitch[2];
		      break;
	    default: break;
	}
    }
    offset_y=offset_u=offset_v=0;
    switch(format)
    {
    case IMGFMT_Y800:
		awidth = (width + (pitch_y-1)) & ~(pitch_y-1);
		null_frame_size = awidth*height;
		break;
    case IMGFMT_YVU9:
    case IMGFMT_IF09:
		awidth = (width + (pitch_y-1)) & ~(pitch_y-1);
		null_frame_size = awidth*(height+height/8);
		offset_u=awidth*height;
		offset_v=awidth*height/16;
		break;
    case IMGFMT_I420:
    case IMGFMT_YV12:
    case IMGFMT_IYUV:
		awidth = (width + (pitch_y-1)) & ~(pitch_y-1);
		null_frame_size = awidth*(height+height/2);
		offset_u=awidth*height;
		offset_v=awidth*height/4;
		break;
    case IMGFMT_RGB32:
    case IMGFMT_BGR32:
		awidth = (width*4 + (pitch_y-1)) & ~(pitch_y-1);
		null_frame_size = awidth*height;
		break;
    /* YUY2 YVYU, RGB15, RGB16 */
    default:	
		awidth = (width*2 + (pitch_y-1)) & ~(pitch_y-1);
		null_frame_size = awidth*height;
		break;
    }
    for(i=0;i<null_num_frames;i++)
    {
	if(!bm_buffs[i])
#ifdef HAVE_MEMALIGN
	    bm_buffs[i] = memalign(getpagesize(),null_frame_size);
#else
	    bm_buffs[i] = malloc(null_frame_size);
#endif
	if(!(bm_buffs[i]))
	{
		MSG_ERR("Can't allocate memory for busmastering\n");
		return -1;
	}
    }
    return 0;
}

static const vo_info_t* get_info(void)
{
	return &vo_info;
}

static void uninit(void)
{
    size_t i;
    for(i=0;i<null_num_frames;i++)
    {
	free(bm_buffs[i]);
	bm_buffs[i]=NULL;
    }
}

static uint32_t __FASTCALL__ preinit(const char *arg)
{
    if(arg) 
    {
	MSG_ERR("vo_null: Unknown subdevice: %s\n",arg);
	return ENOSYS;
    }
    return 0;
}

static void __FASTCALL__ null_dri_get_surface_caps(dri_surface_cap_t *caps)
{
    caps->caps = DRI_CAP_TEMP_VIDEO;
    caps->caps |= DRI_CAP_HORZSCALER | DRI_CAP_VERTSCALER;
    caps->caps |= DRI_CAP_DOWNSCALER | DRI_CAP_UPSCALER;
    caps->fourcc = null_fourcc;
    caps->width=null_image_width;
    caps->height=null_image_height;
    /* in case of vidix movie fit surface */
    caps->x = caps->y=0;
    caps->w=caps->width;
    caps->h=caps->height;
    caps->strides[0] = pitch_y;
    caps->strides[1] = pitch_v;
    caps->strides[2] = pitch_u;
    caps->strides[3] = 0;
}

static void __FASTCALL__ null_dri_get_surface(dri_surface_t *surf)
{
	surf->planes[0] = bm_buffs[surf->idx] + offset_y;
	surf->planes[1] = bm_buffs[surf->idx] + offset_v;
	surf->planes[2] = bm_buffs[surf->idx] + offset_u;
	surf->planes[3] = 0;
}

static uint32_t __FASTCALL__ control(uint32_t request, void *data)
{
  switch (request) {
    case VOCTRL_QUERY_FORMAT:
	return VO_TRUE;
    case VOCTRL_GET_NUM_FRAMES:
	*(uint32_t *)data = null_num_frames;
	return VO_TRUE;
    case DRI_GET_SURFACE_CAPS:
	null_dri_get_surface_caps(data);
	return VO_TRUE;
    case DRI_GET_SURFACE: 
	null_dri_get_surface(data);
	return VO_TRUE;
    case VOCTRL_FLUSH_PAGES:
	return VO_TRUE;
  }
  return VO_NOTIMPL;
}
