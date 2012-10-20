#ifndef DEC_AUDIO_H_INCLUDED
#define DEC_AUDIO_H_INCLUDED 1
// dec_audio.c:
extern int mpca_init(sh_audio_t *sh_audio);
extern void mpca_uninit(sh_audio_t *sh_audio);
extern unsigned mpca_decode(sh_audio_t *sh_audio,unsigned char *buf,unsigned minlen,unsigned maxlen,unsigned buflen,float *pts);
extern void mpca_resync_stream(sh_audio_t *sh_audio);
extern void mpca_skip_frame(sh_audio_t *sh_audio);

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
