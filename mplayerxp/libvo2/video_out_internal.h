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
#include "dri_vo.h"
#include <string>

namespace	usr {
class VO_Interface : public Opaque {
    public:
	VO_Interface(const std::string& args) { UNUSED(args); };
	virtual ~VO_Interface() {};

	virtual MPXP_Rc configure(uint32_t width,
				uint32_t height,
				uint32_t d_width,
				uint32_t d_height,
				unsigned flags,
				const std::string& title,
				uint32_t format) = 0;
	virtual MPXP_Rc	select_frame(unsigned idx) = 0;
	virtual MPXP_Rc	flush_page(unsigned idx) { UNUSED(idx); return MPXP_NA; }
	virtual MPXP_Rc	toggle_fullscreen() { return MPXP_NA; }
	virtual MPXP_Rc	reset() { return MPXP_NA; }
	virtual MPXP_Rc	pause() { return MPXP_NA; }
	virtual MPXP_Rc	resume() { return MPXP_NA; }

	virtual MPXP_Rc	query_format(vo_query_fourcc_t* format) const = 0;
	virtual void	get_surface_caps(dri_surface_cap_t *caps) const = 0;
	virtual void	get_surface(dri_surface_t *surf) = 0;
	virtual unsigned get_num_frames() const = 0;

	virtual uint32_t check_events(const vo_resize_t*) = 0;
	virtual MPXP_Rc ctrl(uint32_t request, any_t*data) = 0;
};
} // namespace
#endif
