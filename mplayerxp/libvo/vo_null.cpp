#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
/*
 *  video_out_null.c
 *
 *  Copyright (C) Aaron Holtzman - June 2000
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
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "video_out.h"
#include "video_out_internal.h"
#include "dri_vo.h"
#ifdef CONFIG_VIDIX
#include <vidix/vidix.h>
#endif
#include "vo_msg.h"

class Null_VO_Interface : public VO_Interface {
    public:
	Null_VO_Interface(const char* args);
	virtual ~Null_VO_Interface();

	virtual MPXP_Rc	configure(uint32_t width,
				uint32_t height,
				uint32_t d_width,
				uint32_t d_height,
				unsigned flags,
				const char *title,
				uint32_t format);
	virtual MPXP_Rc	select_frame(unsigned idx);
	virtual void	get_surface_caps(dri_surface_cap_t *caps) const;
	virtual void	get_surface(dri_surface_t *surf);
	virtual MPXP_Rc	query_format(vo_query_fourcc_t* format) const;
	virtual unsigned get_num_frames() const;

	virtual uint32_t check_events(const vo_resize_t*);
	virtual MPXP_Rc	ctrl(uint32_t request, any_t*data);
    private:

	uint32_t	image_width, image_height,frame_size,fourcc;
	uint8_t *	bm_buffs[MAX_DRI_BUFFERS];
	uint32_t	num_frames;
	uint32_t	pitch_y,pitch_u,pitch_v;
	uint32_t	offset_y,offset_u,offset_v;
};

MPXP_Rc Null_VO_Interface::select_frame(unsigned idx)
{
    UNUSED(idx);
    return MPXP_Ok;
}

MPXP_Rc Null_VO_Interface::configure(uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height,unsigned flags,const char *title, uint32_t format)
{
    unsigned awidth;
    size_t i;
    image_width = width;
    image_height = height;
    num_frames = vo_conf.xp_buffs;
    fourcc=format;
    UNUSED(d_width);
    UNUSED(d_height);
    UNUSED(title);
    UNUSED(flags);
    pitch_y=pitch_u=pitch_v=1;
    offset_y=offset_u=offset_v=0;
    switch(format) {
    case IMGFMT_Y800:
		awidth = (width + (pitch_y-1)) & ~(pitch_y-1);
		frame_size = awidth*height;
		break;
    case IMGFMT_YVU9:
    case IMGFMT_IF09:
		awidth = (width + (pitch_y-1)) & ~(pitch_y-1);
		frame_size = awidth*(height+height/8);
		offset_u=awidth*height;
		offset_v=awidth*height/16;
		break;
    case IMGFMT_I420:
    case IMGFMT_YV12:
    case IMGFMT_IYUV:
		awidth = (width + (pitch_y-1)) & ~(pitch_y-1);
		frame_size = awidth*(height+height/2);
		offset_u=awidth*height;
		offset_v=awidth*height/4;
		break;
    case IMGFMT_RGB32:
    case IMGFMT_BGR32:
		awidth = (width*4 + (pitch_y-1)) & ~(pitch_y-1);
		frame_size = awidth*height;
		break;
    /* YUY2 YVYU, RGB15, RGB16 */
    default:
		awidth = (width*2 + (pitch_y-1)) & ~(pitch_y-1);
		frame_size = awidth*height;
		break;
    }
    for(i=0;i<num_frames;i++) {
	if(!bm_buffs[i])
	    bm_buffs[i] = new(alignmem,getpagesize()) uint8_t[frame_size];
	if(!(bm_buffs[i])) {
		MSG_ERR("Can't allocate memory for busmastering\n");
		return MPXP_False;
	}
    }
    return MPXP_Ok;
}

Null_VO_Interface::~Null_VO_Interface()
{
    size_t i;
    for(i=0;i<num_frames;i++) {
	delete bm_buffs[i];
	bm_buffs[i]=NULL;
    }
}

Null_VO_Interface::Null_VO_Interface(const char *arg)
		:VO_Interface(arg)
{
    if(arg) MSG_ERR("vo_null: Unknown subdevice: %s\n",arg);
}

void Null_VO_Interface::get_surface_caps(dri_surface_cap_t *caps) const
{
    caps->caps =DRI_CAP_TEMP_VIDEO |
		DRI_CAP_HORZSCALER | DRI_CAP_VERTSCALER |
		DRI_CAP_DOWNSCALER | DRI_CAP_UPSCALER;
    caps->fourcc = fourcc;
    caps->width=image_width;
    caps->height=image_height;
    /* in case of vidix movie fit surface */
    caps->x = caps->y=0;
    caps->w=caps->width;
    caps->h=caps->height;
    caps->strides[0] = pitch_y;
    caps->strides[1] = pitch_v;
    caps->strides[2] = pitch_u;
    caps->strides[3] = 0;
}

void Null_VO_Interface::get_surface(dri_surface_t *surf)
{
    surf->planes[0] = bm_buffs[surf->idx] + offset_y;
    surf->planes[1] = bm_buffs[surf->idx] + offset_v;
    surf->planes[2] = bm_buffs[surf->idx] + offset_u;
    surf->planes[3] = 0;
}

unsigned Null_VO_Interface::get_num_frames() const { return num_frames; }

MPXP_Rc Null_VO_Interface::query_format(vo_query_fourcc_t* format) const {
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

uint32_t Null_VO_Interface::check_events(const vo_resize_t* vr) {
    UNUSED(vr);
    return 0;
}

MPXP_Rc Null_VO_Interface::ctrl(uint32_t request, any_t*data) {
    return MPXP_NA;
}

static VO_Interface* query_interface(const char* args) { return new(zeromem) Null_VO_Interface(args); }
extern const vo_info_t null_vo_info = {
    "Null video output",
    "null",
    "Aaron Holtzman <aholtzma@ess.engr.uvic.ca>",
    "",
    query_interface
};
