/*
   ad.h - audio decoders interface
*/
#ifndef AD_H_INCLUDED
#define AD_H_INCLUDED 1

#include "libmpconf/cfgparser.h"
#include "xmp_enums.h"

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

/* interface of video decoder drivers */
typedef struct ad_functions_s
{
    const ad_info_t *info;
    const config_t*  options;/**< Optional: MPlayerXP's option related */
    int (*preinit)(sh_audio_t *);
    int (*init)(sh_audio_t *sh);
    void (*uninit)(sh_audio_t *sh);
    ControlCodes (*control)(sh_audio_t *sh,int cmd,any_t* arg, ...);
    unsigned (*decode)(sh_audio_t *sh_audio,unsigned char *buf,unsigned minlen,unsigned maxlen,float *pts);
} ad_functions_t;

// NULL terminated array of all drivers
extern const ad_functions_t* mpcodecs_ad_drivers[];
#define FIX_APTS(sh_audio,pts,in_size) (sh_audio->i_bps?((float)(pts)+(float)(in_size)/(float)sh_audio->i_bps):((float)(pts)))

#endif
