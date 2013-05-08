/*
 * vidix_system.h
 *
 *	Copyright (C) Nickols_K <nickols_k@mail.ru> - 2002
 *
 *  You can redistribute this file under terms and conditions
 *  of GNU General Public licence v2.
 *
 * This file contains vosub_vidix interface to any mplayer's VO driver
 */

#ifndef __VIDIX_SYSTEM_H_INCLUDED
#define __VIDIX_SYSTEM_H_INCLUDED 1
#include "xmpcore/xmp_enums.h"

#include "osdep/mplib.h"
#include "video_out.h"
#include "dri_vo.h"
#include <vidix/vidix.h>
#include <vidix/vidixlibxx.h>

using namespace vidix;
using namespace	usr;

#define NUM_FRAMES MAX_DRI_BUFFERS /* Temporary: driver will overwrite it */

struct Vidix_System : public video_private {
    public:
	Vidix_System(const std::string& drvname);
	virtual ~Vidix_System();

	MPXP_Rc		configure(unsigned src_width,unsigned src_height,
				unsigned dest_x,unsigned dest_y,unsigned dst_width,
				unsigned dst_height,unsigned format,unsigned dest_bpp,
				unsigned vid_w,unsigned vid_h);
	int		start();
	int		stop();
	MPXP_Rc		select_frame(unsigned idx);
	MPXP_Rc		flush_page(unsigned idx);

	MPXP_Rc		query_fourcc(vo_query_fourcc_t* format);
	void		get_surface_caps(dri_surface_cap_t *caps) const;
	void		get_surface(dri_surface_t *surf) const;
	unsigned	get_num_frames() const;

	MPXP_Rc		grkey_support() const;
	int		grkey_get(vidix_grkey_t *gr_key) const;
	int		grkey_set(const vidix_grkey_t *gr_key);
	int		get_video_eq(vo_videq_t *info) const;
	int		set_video_eq(const vo_videq_t *info);
	int		get_num_fx(unsigned *info) const;
	int		get_oem_fx(vidix_oem_fx_t *info) const;
	int		set_oem_fx(const vidix_oem_fx_t *info);
	int		get_deint(vidix_deinterlace_t *info) const;
	int		set_deint(const vidix_deinterlace_t *info);
    private:
	void		copy_dma(unsigned idx,int sync_mode);

	unsigned	image_Bpp,image_height,image_width,src_format,forced_fourcc;

	LocalPtr<Vidix>	vidix;
	uint8_t *	mem;
	int		video_on;

/* bus mastering */
	int		bm_locked; /* requires root privelegies */
	uint8_t *	bm_buffs[NUM_FRAMES];
	unsigned	bm_total_frames,bm_slow_frames;
};
#endif
