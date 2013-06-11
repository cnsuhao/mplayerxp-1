/*
   ad.h - audio decoders interface
*/
#ifndef AD_H_INCLUDED
#define AD_H_INCLUDED 1

#include "libmpconf/cfgparser.h"
#include "xmpcore/xmp_enums.h"
#include "libao3/afmt.h"

namespace	usr {
    struct mpxp_options_t;
    enum {
	ADCTRL_RESYNC_STREAM=0,
	ADCTRL_SKIP_FRAME	=1
    };

    enum {
	Audio_MaxOutSample	=16,
    };

    typedef enum {
	ACodecStatus_Working	=3,
	ACodecStatus_Problems	=2,
	ACodecStatus_Untested	=1,
	ACodecStatus_NotWorking	=0,
    }acodec_status_e;

    struct audio_probe_t {
	const char*		driver;
	const char*		codec_dll;
	uint32_t		wtag;
	acodec_status_e	status;
	unsigned		sample_fmt[Audio_MaxOutSample];
    };

    struct audio_filter_info_t {
	af_stream_t*	afilter;
	int		afilter_inited;
    };

    class Audio_Decoder : public Opaque {
	public:
	    Audio_Decoder(sh_audio_t&,audio_filter_info_t&,uint32_t wtag) { UNUSED(wtag); }
	    virtual ~Audio_Decoder() {}

	    virtual unsigned		run(unsigned char *buf,unsigned minlen,unsigned maxlen,float& pts) = 0;
	    virtual MPXP_Rc		ctrl(int cmd,any_t* arg) = 0;
	    virtual audio_probe_t	get_probe_information() const = 0;
    };

    struct ad_info_t {
	const char*	descr; /* driver description ("Autodesk FLI/FLC Animation decoder" */
	const char*	driver_name; /* driver name ("dshow") */
	const char*	author; /* interface author/maintainer */
	const char*	url; /* URL of homepage */
	Audio_Decoder*	(*query_interface)(sh_audio_t&,audio_filter_info_t&,uint32_t wtag);
	const mpxp_option_t*	options;
    };

    const ad_info_t*	afm_find_driver(const std::string& name);
    Audio_Decoder*	afm_probe_driver(sh_audio_t& sh,audio_filter_info_t& afi);
    inline float	FIX_APTS(sh_audio_t& sh_audio,float& pts,unsigned in_size) { return sh_audio.i_bps?(pts+float(in_size))/float(sh_audio.i_bps):pts; }
} //namespace	usr

#endif
