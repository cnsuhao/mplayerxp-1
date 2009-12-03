/*
 *  video_out_internal.h
 *
 *	Copyright (C) Aaron Holtzman - Aug 1999
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
#ifndef __VIDEO_OUT_INTERNAL_H
#define __VIDEO_OUT_INTERNAL_H 1
static uint32_t __FASTCALL__ control(uint32_t request, void *data);
static uint32_t __FASTCALL__ config(uint32_t width, uint32_t height, uint32_t d_width,
		     uint32_t d_height, uint32_t fullscreen, char *title,
		     uint32_t format,const vo_tune_info_t *);
static const vo_info_t* get_info(void);
static void __FASTCALL__ flip_page(unsigned idx);
static void uninit(void);
static uint32_t __FASTCALL__ query_format(vo_query_fourcc_t* format);
static uint32_t __FASTCALL__ preinit(const char *);

#define LIBVO_EXTERN(x) vo_functions_t video_out_##x =\
{\
	preinit,\
	config,\
	control,\
	get_info,\
	flip_page,\
	uninit\
};

#include "osd.h"

#define UNUSED(x) ((void)(x)) /* Removes warning about unused arguments */

#endif
