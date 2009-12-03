#ifndef MPLAYER_VOBSUB_H
#define MPLAYER_VOBSUB_H

extern void *  __FASTCALL__ vobsub_open(const char *subname, const char *const ifo, const int force, void** spu);
extern void __FASTCALL__ vobsub_close(void *this);
extern void __FASTCALL__ vobsub_reset(void *vob);
extern int __FASTCALL__ vobsub_get_packet(void *vobhandle, float pts,void** data, int* timestamp);
extern int __FASTCALL__ vobsub_parse_ifo(void* vob, const char *const name, unsigned int *palette, unsigned int *width, unsigned int *height, int force, int sid, char *langid);
extern char * __FASTCALL__ vobsub_get_id(void* vob, unsigned int index);
extern int __FASTCALL__ vobsub_set_from_lang(void *vobhandle, unsigned char * lang);
extern unsigned int __FASTCALL__ vobsub_get_forced_subs_flag(void const * const vobhandle);
extern void __FASTCALL__ vobsub_seek(void * vob, float pts);
/// Convert palette value in idx file to yuv.
extern unsigned int vobsub_palette_to_yuv(unsigned int pal);
extern unsigned int vobsub_rgb_to_yuv(unsigned int rgb);
#endif /* MPLAYER_VOBSUB_H */
