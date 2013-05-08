#ifndef XMP_ADECODER_H_INCLUDED
#define XMP_ADECODER_H_INCLUDED 1
namespace	usr {
    int		get_len_audio_buffer(void);
    float	get_delay_audio_buffer(void);
    int		init_audio_buffer(int size, int min_reserv, int indices, sh_audio_t *sh_audio);
    void	uninit_audio_buffer(void);
    void	reset_audio_buffer(void);
    int		read_audio_buffer(sh_audio_t *audio, unsigned char *buffer, unsigned minlen, unsigned maxlen, float *pts );

    any_t*	a_dec_ahead_routine( any_t* arg );
    void	sig_audio_decode( void );
    int		xp_thread_decode_audio(Demuxer_Stream *d_audio);
} // namespace
#endif
