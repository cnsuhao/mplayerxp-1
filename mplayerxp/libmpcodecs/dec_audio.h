#ifndef DEC_AUDIO_H_INCLUDED
#define DEC_AUDIO_H_INCLUDED 1
#include "ad.h"
#include "xmpcore/mp_aframe.h"
#include "xmpcore/xmp_enums.h"

// dec_audio.c:
extern const ad_functions_t* __FASTCALL__ mpca_find_driver(const char *name);
extern MPXP_Rc __FASTCALL__ RND_RENAME2(mpca_init)(sh_audio_t *sh_audio);
extern void __FASTCALL__ mpca_uninit(sh_audio_t *sh_audio);
extern unsigned __FASTCALL__ RND_RENAME3(mpca_decode)(sh_audio_t *sh_audio,unsigned char *buf,unsigned minlen,unsigned maxlen,unsigned buflen,float *pts);
extern void __FASTCALL__ mpca_resync_stream(sh_audio_t *sh_audio);
extern void __FASTCALL__ mpca_skip_frame(sh_audio_t *sh_audio);
struct codecs_st;
extern struct codecs_st* __FASTCALL__ find_ffmpeg_audio(sh_audio_t*);

extern MPXP_Rc mpca_init_filters(sh_audio_t *sh_audio,
	unsigned in_samplerate, unsigned in_channels, mpaf_format_e in_format,
	unsigned out_samplerate, unsigned out_channels,mpaf_format_e out_format,
	unsigned out_minsize, unsigned out_maxsize);
extern MPXP_Rc mpca_preinit_filters(sh_audio_t *sh_audio,
	unsigned in_samplerate, unsigned in_channels, unsigned in_format,
	unsigned* out_samplerate, unsigned* out_channels, unsigned* out_format);
extern MPXP_Rc mpca_reinit_filters(sh_audio_t *sh_audio,
	unsigned in_samplerate, unsigned in_channels, mpaf_format_e in_format,
	unsigned out_samplerate, unsigned out_channels, mpaf_format_e out_format,
	unsigned out_minsize, unsigned out_maxsize);
extern void afm_help(void);
#endif
