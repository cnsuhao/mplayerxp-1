#ifndef DEC_AUDIO_H_INCLUDED
#define DEC_AUDIO_H_INCLUDED 1
#include "ad.h"
#include "xmpcore/mp_aframe.h"
#include "xmpcore/xmp_enums.h"

// dec_audio.c:
extern const ad_functions_t* __FASTCALL__ mpca_find_driver(const char *name);
extern any_t* __FASTCALL__ mpca_init(sh_audio_t *sh_audio);
extern void   __FASTCALL__ mpca_uninit(any_t *handle);
extern unsigned __FASTCALL__ mpca_decode(any_t *handle,unsigned char *buf,unsigned minlen,unsigned maxlen,unsigned buflen,float *pts);
extern void __FASTCALL__ mpca_resync_stream(any_t *handle);
extern void __FASTCALL__ mpca_skip_frame(any_t *handle);
struct codecs_st;
extern struct codecs_st* __FASTCALL__ find_lavc_audio(sh_audio_t*);

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
