/*
 *  vosub_vidix.h
 *
 *	Copyright (C) Nickols_K <nickols_k@mail.ru> - 2002
 *
 *  You can redistribute this file under terms and conditions
 *  of GNU General Public licence v2.
 *
 * This file contains vosub_vidix interface to any mplayer's VO driver
 */

#ifndef __VOSUB_VIDIX_INCLUDED
#define __VOSUB_VIDIX_INCLUDED
#include "xmpcore/xmp_enums.h"

#include "osdep/mplib.h"
#include "video_out.h"
#include "dri_vo.h"
#include <vidix/vidix.h>
#include <vidix/vidixlibxx.h>

using namespace vidix;
using namespace mpxp;

#define NUM_FRAMES MAX_DRI_BUFFERS /* Temporary: driver will overwrite it */

struct vidix_priv_t : public video_private {
    vidix_priv_t(Vidix& it) : vidix(it) {}
    virtual ~vidix_priv_t() {}

    unsigned		image_Bpp,image_height,image_width,src_format,forced_fourcc;

    Vidix&		vidix;
    uint8_t *		mem;
    int			video_on;

    int			inited;

/* bus mastering */
    int			bm_locked; /* requires root privelegies */
    uint8_t *		bm_buffs[NUM_FRAMES];
    unsigned		bm_total_frames,bm_slow_frames;
};

typedef MPXP_Rc (* vidix_control_t)(vidix_priv_t*,uint32_t request, any_t*data);
typedef void (* vidix_select_frame_t)(vidix_priv_t*,unsigned idx);
typedef struct vidix_server_s {
    vidix_select_frame_t	select_frame;
    vidix_control_t	control;
}vidix_server_t;
		    /* drvname can be NULL */
vidix_priv_t*	 __FASTCALL__ vidix_preinit(const char *drvname);
vidix_server_t* __FASTCALL__ vidix_get_server(vidix_priv_t*);
MPXP_Rc  __FASTCALL__ vidix_init(vidix_priv_t*,unsigned src_width,unsigned src_height,
		    unsigned dest_x,unsigned dest_y,unsigned dst_width,
		    unsigned dst_height,unsigned format,unsigned dest_bpp,
		    unsigned vid_w,unsigned vid_h);
int	 vidix_start(vidix_priv_t*);
int	 vidix_stop(vidix_priv_t*);
void     vidix_term(vidix_priv_t*);

#include <vidix/vidix.h>
/* graphic keys */
int __FASTCALL__ vidix_grkey_support(vidix_priv_t*);
int __FASTCALL__ vidix_grkey_get(vidix_priv_t*,vidix_grkey_t *gr_key);
int __FASTCALL__ vidix_grkey_set(vidix_priv_t*,const vidix_grkey_t *gr_key);

#endif
