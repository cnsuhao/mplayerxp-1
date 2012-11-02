#ifndef DEC_AUDIO_H_INCLUDED
#define DEC_AUDIO_H_INCLUDED 1
#include "ad.h"

// dec_audio.c:
extern const ad_functions_t* __FASTCALL__ mpca_find_driver(const char *name);
extern int __FASTCALL__ mpca_init(sh_audio_t *sh_audio);
extern void __FASTCALL__ mpca_uninit(sh_audio_t *sh_audio);
extern unsigned __FASTCALL__ mpca_decode(sh_audio_t *sh_audio,unsigned char *buf,unsigned minlen,unsigned maxlen,unsigned buflen,float *pts);
extern void __FASTCALL__ mpca_resync_stream(sh_audio_t *sh_audio);
extern void __FASTCALL__ mpca_skip_frame(sh_audio_t *sh_audio);
struct codecs_st;
extern struct codecs_st* __FASTCALL__ find_ffmpeg_audio(sh_audio_t*);

extern int mpca_init_filters(sh_audio_t *sh_audio, 
	int in_samplerate, int in_channels, int in_format, int in_bps,
	int out_samplerate, int out_channels, int out_format, int out_bps,
	int out_minsize, int out_maxsize);
extern int mpca_preinit_filters(sh_audio_t *sh_audio,
        int in_samplerate, int in_channels, int in_format, int in_bps,
        int* out_samplerate, int* out_channels, int* out_format, int out_bps);
extern int mpca_reinit_filters(sh_audio_t *sh_audio, 
	int in_samplerate, int in_channels, int in_format, int in_bps,
	int out_samplerate, int out_channels, int out_format, int out_bps,
	int out_minsize, int out_maxsize);
extern void afm_help(void);
#endif
