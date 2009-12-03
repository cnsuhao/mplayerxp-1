#ifndef _MPLAYER_SPUDEC_H
#define _MPLAYER_SPUDEC_H

#include "libvo/video_out.h"
#include "libvo/sub.h"

void __FASTCALL__ spudec_heartbeat(void *_this, unsigned int pts100);
void __FASTCALL__ spudec_assemble(void *_this, unsigned char *packet, unsigned int len, unsigned int pts100);
void __FASTCALL__ spudec_draw(void *this, draw_osd_f draw_alpha);
void __FASTCALL__ spudec_draw_scaled(void *_this, unsigned int dxs, unsigned int dys,draw_osd_f draw_alpha);
void __FASTCALL__ spudec_update_palette(void *_this,const unsigned int *palette);
void* __FASTCALL__ spudec_new_scaled(unsigned int *palette, unsigned int frame_width, unsigned int frame_height);
void* __FASTCALL__ spudec_new_scaled_vobsub(unsigned int *palette, unsigned int *cuspal, unsigned int custom, unsigned int frame_width, unsigned int frame_height);
void* __FASTCALL__ spudec_new(unsigned int *palette);
void __FASTCALL__ spudec_free(void *_this);
void __FASTCALL__ spudec_reset(void *_this);	// called after seek
int __FASTCALL__ spudec_visible(void *_this); // check if spu is visible
void __FASTCALL__ spudec_set_font_factor(void *_this, double factor); // sets the equivalent to ffactor
void __FASTCALL__ spudec_set_hw_spu(void *_this, vo_functions_t *hw_spu);
int __FASTCALL__ spudec_changed(void *_this);
void __FASTCALL__ spudec_calc_bbox(void *me, unsigned int dxs, unsigned int dys, unsigned int* bbox);
void __FASTCALL__ spudec_set_forced_subs_only(void * const _this, const unsigned int flag);

#endif

