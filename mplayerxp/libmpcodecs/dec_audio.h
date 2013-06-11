#ifndef DEC_AUDIO_H_INCLUDED
#define DEC_AUDIO_H_INCLUDED 1
#include "ad.h"
#include "xmpcore/xmp_aframe.h"
#include "xmpcore/xmp_enums.h"

struct codecs_st;
namespace	usr {
    class AD_Interface : public Opaque {
	public:
	    AD_Interface(sh_audio_t& sh_audio);
	    virtual ~AD_Interface();

	    virtual unsigned		run(unsigned char *buf,unsigned minlen,unsigned maxlen,unsigned buflen,float& pts) const;
	    virtual void		resync_stream() const;
	    virtual void		skip_frame() const;

	    virtual MPXP_Rc		init_filters(unsigned in_samplerate, unsigned in_channels, mpaf_format_e in_format,
						    unsigned out_samplerate, unsigned out_channels,mpaf_format_e out_format,
						    unsigned out_minsize, unsigned out_maxsize) const;
	    virtual MPXP_Rc		preinit_filters(unsigned in_samplerate, unsigned in_channels, unsigned in_format,
						    unsigned& out_samplerate, unsigned& out_channels, unsigned& out_format) const;
	    virtual MPXP_Rc		reinit_filters(unsigned in_samplerate, unsigned in_channels, mpaf_format_e in_format,
						    unsigned out_samplerate, unsigned out_channels, mpaf_format_e out_format,
						    unsigned out_minsize, unsigned out_maxsize) const;
	    static void			print_help();
	private:
	    const ad_info_t*		find_driver(const std::string& name) const;
	    Audio_Decoder*		probe_driver(sh_audio_t& sh,audio_filter_info_t& afi) const;

	    Opaque&	ad_private;
    };
} //namespace usr
#endif
