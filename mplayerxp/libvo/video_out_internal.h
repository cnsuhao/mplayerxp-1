/*
 *  video_out_internal.h
 *
 *	Copyright (C) Aaron Holtzman - Aug 1999
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
#ifndef __VIDEO_OUT_INTERNAL_H
#define __VIDEO_OUT_INTERNAL_H 1
static MPXP_Rc __FASTCALL__ control(vo_data_t*vo,uint32_t request, any_t*data);
static MPXP_Rc __FASTCALL__ config(vo_data_t*vo,uint32_t width, uint32_t height, uint32_t d_width,
		     uint32_t d_height, uint32_t fullscreen, char *title,
		     uint32_t format);
static const vo_info_t* __FASTCALL__ get_info(vo_data_t*vo);
static void __FASTCALL__ select_frame(vo_data_t*vo,unsigned idx);
static void __FASTCALL__ uninit(vo_data_t*vo);
static MPXP_Rc __FASTCALL__ preinit(vo_data_t*vo,const char *);

#define LIBVO_EXTERN(x) vo_functions_t video_out_##x =\
{\
	preinit,\
	config,\
	control,\
	get_info,\
	select_frame,\
	uninit\
};

#include "osd.h"

#endif
