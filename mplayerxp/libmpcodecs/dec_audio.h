
// dec_audio.c:
extern int init_audio(sh_audio_t *sh_audio);
extern void uninit_audio(sh_audio_t *sh_audio);
extern int decode_audio(sh_audio_t *sh_audio,unsigned char *buf,int minlen,int maxlen,int buflen,float *pts);
extern void resync_audio_stream(sh_audio_t *sh_audio);
extern void skip_audio_frame(sh_audio_t *sh_audio);

// MP3 decoder buffer callback:
extern int mplayer_audio_read(char *buf,int size,float *pts);

extern int init_audio_filters(sh_audio_t *sh_audio, 
	int in_samplerate, int in_channels, int in_format, int in_bps,
	int out_samplerate, int out_channels, int out_format, int out_bps,
	int out_minsize, int out_maxsize);
extern int preinit_audio_filters(sh_audio_t *sh_audio,
        int in_samplerate, int in_channels, int in_format, int in_bps,
        int* out_samplerate, int* out_channels, int* out_format, int out_bps);
extern int reinit_audio_filters(sh_audio_t *sh_audio, 
	int in_samplerate, int in_channels, int in_format, int in_bps,
	int out_samplerate, int out_channels, int out_format, int out_bps,
	int out_minsize, int out_maxsize);

extern void afm_help(void);
