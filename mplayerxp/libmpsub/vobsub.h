#ifndef MPLAYER_VOBSUB_H
#define MPLAYER_VOBSUB_H

#include "libmpdemux/demuxer.h" // for seek_args_t

extern any_t*  __FASTCALL__ vobsub_open(const char *subname, const char *const ifo, const int force, any_t** spu);
extern void __FASTCALL__ vobsub_close(any_t*__self);
extern void __FASTCALL__ vobsub_reset(any_t*vob);
extern int __FASTCALL__ vobsub_get_packet(any_t*vobhandle, float pts,any_t** data, int* timestamp);
extern int __FASTCALL__ vobsub_parse_ifo(any_t* vob, const char *const name, unsigned int *palette, unsigned int *width, unsigned int *height, int force, int sid, char *langid);
extern char * __FASTCALL__ vobsub_get_id(any_t* vob, unsigned int index);
extern int __FASTCALL__ vobsub_set_from_lang(any_t*vobhandle,const char * lang);
extern unsigned int __FASTCALL__ vobsub_get_forced_subs_flag(void const * const vobhandle);
extern void __FASTCALL__ vobsub_seek(any_t* vob,const seek_args_t* seek);
/// Convert palette value in idx file to yuv.
extern unsigned int vobsub_palette_to_yuv(unsigned int pal);
extern unsigned int vobsub_rgb_to_yuv(unsigned int rgb);

#endif /* MPLAYER_VOBSUB_H */
