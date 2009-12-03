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

		    /* drvname can be NULL */
int	 __FASTCALL__ vidix_preinit(const char *drvname,const void *server);
int      __FASTCALL__ vidix_init(unsigned src_width,unsigned src_height,
		    unsigned dest_x,unsigned dest_y,unsigned dst_width,
		    unsigned dst_height,unsigned format,unsigned dest_bpp,
		    unsigned vid_w,unsigned vid_h,const void *info);
int	 vidix_start(void);
int	 vidix_stop(void);
void     vidix_term( void );
uint32_t __FASTCALL__ vidix_query_fourcc(vo_query_fourcc_t* fourcc);
void     __FASTCALL__ vidix_flip_page(unsigned idx);

#include <vidix/vidix.h>
/* graphic keys */
int __FASTCALL__ vidix_grkey_support(void);
int __FASTCALL__ vidix_grkey_get(vidix_grkey_t *gr_key);
int __FASTCALL__ vidix_grkey_set(const vidix_grkey_t *gr_key);

#endif
