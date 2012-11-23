#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "ad_internal.h"

static const ad_info_t info = {
    "Null audio decoder",
    "null",
    "Nickols_K",
    "build-in"
};

static const config_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

LIBAD_EXTERN(null)

static const audio_probe_t* __FASTCALL__ probe(sh_audio_t* sh,uint32_t wtag) { return NULL; }

MPXP_Rc init(sh_audio_t *sh)
{
    UNUSED(sh);
    return MPXP_Ok;
}

MPXP_Rc preinit(sh_audio_t *sh)
{
    UNUSED(sh);
    return MPXP_Ok;
}

void uninit(sh_audio_t *sh)
{
    UNUSED(sh);
}

MPXP_Rc control(sh_audio_t *sh,int cmd,any_t* arg, ...)
{
    UNUSED(sh);
    UNUSED(cmd);
    UNUSED(arg);
    return MPXP_Unknown;
}

unsigned decode(sh_audio_t *sh_audio,unsigned char *buf,unsigned minlen,unsigned maxlen,float *pts)
{
    UNUSED(sh_audio);
    UNUSED(buf);
    UNUSED(minlen);
    UNUSED(maxlen);
    *pts=0;
    return 0;
}
