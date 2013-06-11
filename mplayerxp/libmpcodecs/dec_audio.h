#ifndef DEC_AUDIO_H_INCLUDED
#define DEC_AUDIO_H_INCLUDED 1
#include "ad.h"
#include "xmpcore/xmp_aframe.h"
#include "xmpcore/xmp_enums.h"

struct codecs_st;
namespace	usr {
    struct audio_decoder_t {
	Opaque*	ad_private;
    };

    // dec_audio.c:
    audio_decoder_t*		__FASTCALL__ mpca_init(sh_audio_t *sh_audio);
    void			__FASTCALL__ mpca_uninit(audio_decoder_t& handle);
    unsigned			__FASTCALL__ mpca_decode(audio_decoder_t& handle,unsigned char *buf,unsigned minlen,unsigned maxlen,unsigned buflen,float& pts);
    void			__FASTCALL__ mpca_resync_stream(audio_decoder_t& handle);
    void			__FASTCALL__ mpca_skip_frame(audio_decoder_t& handle);

    MPXP_Rc mpca_init_filters(audio_decoder_t& sh_audio,
	unsigned in_samplerate, unsigned in_channels, mpaf_format_e in_format,
	unsigned out_samplerate, unsigned out_channels,mpaf_format_e out_format,
	unsigned out_minsize, unsigned out_maxsize);
    MPXP_Rc mpca_preinit_filters(audio_decoder_t& sh_audio,
	unsigned in_samplerate, unsigned in_channels, unsigned in_format,
	unsigned& out_samplerate, unsigned& out_channels, unsigned& out_format);
    MPXP_Rc mpca_reinit_filters(audio_decoder_t& sh_audio,
	unsigned in_samplerate, unsigned in_channels, mpaf_format_e in_format,
	unsigned out_samplerate, unsigned out_channels, mpaf_format_e out_format,
	unsigned out_minsize, unsigned out_maxsize);
    void afm_help(void);
} //namespace usr
#endif
