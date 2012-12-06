/*
   ad.h - audio decoders interface
*/
#ifndef AD_H_INCLUDED
#define AD_H_INCLUDED 1

#include "libmpconf/cfgparser.h"
#include "xmpcore/xmp_enums.h"
#include "libao2/afmt.h"

typedef struct ad_info_s
{
    const char *descr; /* driver description ("Autodesk FLI/FLC Animation decoder" */
    const char *driver_name; /* driver name ("dshow") */
    const char *author; /* interface author/maintainer */
    const char *url; /* URL of homepage */
} ad_info_t;

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

typedef struct audio_probe_s {
    const char*		driver;
    const char*		codec_dll;
    uint32_t		wtag;
    acodec_status_e	status;
    unsigned		sample_fmt[Audio_MaxOutSample];
}audio_probe_t;

struct audio_filter_info_t {
    af_stream_t*	afilter;
    int			afilter_inited;
};

/* interface of video decoder drivers */
struct ad_private_t;
typedef struct ad_functions_s
{
    const ad_info_t*	info;
    const config_t*	options;/**< Optional: MPlayerXP's option related */
    const audio_probe_t*(* __FASTCALL__ probe)(uint32_t wtag);
    ad_private_t*		(* __FASTCALL__ preinit)(const audio_probe_t*,sh_audio_t *,audio_filter_info_t*);
    MPXP_Rc		(* __FASTCALL__ init)(ad_private_t *ctx);
    void		(* __FASTCALL__ uninit)(ad_private_t *ctx);
    MPXP_Rc		(*control_ad)(ad_private_t *ctx,int cmd,any_t* arg, ...);
    unsigned		(* __FASTCALL__ decode)(ad_private_t *ctx,unsigned char *buf,unsigned minlen,unsigned maxlen,float *pts);
} ad_functions_t;

extern const ad_functions_t* afm_find_driver(const char *name);
extern const audio_probe_t* afm_probe_driver(ad_private_t*ctx,sh_audio_t*sh,audio_filter_info_t* afi);
#define FIX_APTS(sh_audio,pts,in_size) (sh_audio->i_bps?((float)(pts)+(float)(in_size)/(float)sh_audio->i_bps):((float)(pts)))

#endif
