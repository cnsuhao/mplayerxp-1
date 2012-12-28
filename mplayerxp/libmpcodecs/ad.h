/*
   ad.h - audio decoders interface
*/
#ifndef AD_H_INCLUDED
#define AD_H_INCLUDED 1

#include "libmpconf/cfgparser.h"
#include "xmpcore/xmp_enums.h"
#include "libao3/afmt.h"

struct ad_info_t {
    const char *descr; /* driver description ("Autodesk FLI/FLC Animation decoder" */
    const char *driver_name; /* driver name ("dshow") */
    const char *author; /* interface author/maintainer */
    const char *url; /* URL of homepage */
};

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
    int			afilter_inited;
};

/* interface of video decoder drivers */
struct ad_functions_t
{
    const ad_info_t*	info;
    const mpxp_option_t*	options;/**< Optional: MPlayerXP's option related */
    const audio_probe_t*(* __FASTCALL__ probe)(uint32_t wtag);
    Opaque*		(* __FASTCALL__ preinit)(const audio_probe_t&,sh_audio_t*,audio_filter_info_t&);
    MPXP_Rc		(* __FASTCALL__ init)(Opaque& ctx);
    void		(* __FASTCALL__ uninit)(Opaque& ctx);
    MPXP_Rc		(*control_ad)(Opaque& ctx,int cmd,any_t* arg, ...);
    unsigned		(* __FASTCALL__ decode)(Opaque& ctx,unsigned char *buf,unsigned minlen,unsigned maxlen,float& pts);
};

extern const ad_functions_t* afm_find_driver(const std::string& name);
extern const audio_probe_t* afm_probe_driver(Opaque& ctx,sh_audio_t*sh,audio_filter_info_t& afi);
inline float FIX_APTS(sh_audio_t* sh_audio,float& pts,unsigned in_size) { return sh_audio->i_bps?(pts+float(in_size))/float(sh_audio->i_bps):pts; }

#endif
