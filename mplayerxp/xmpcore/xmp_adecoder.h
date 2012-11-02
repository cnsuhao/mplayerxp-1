#ifndef XMP_ADECODER_H_INCLUDED
#define XMP_ADECODER_H_INCLUDED 1

extern int get_len_audio_buffer(void);
extern float get_delay_audio_buffer(void);
extern int init_audio_buffer(int size, int min_reserv, int indices, sh_audio_t *sh_audio);
extern void uninit_audio_buffer(void);
extern void reset_audio_buffer(void);
extern int read_audio_buffer(sh_audio_t *audio, unsigned char *buffer, unsigned minlen, unsigned maxlen, float *pts );

extern any_t* a_dec_ahead_routine( any_t* arg );
extern void sig_audio_decode( void );
extern int xp_thread_decode_audio(demux_stream_t *d_audio);
#endif
