#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "ad_internal.h"

struct ad_private_t {
    sh_audio_t* sh;
};

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

static const audio_probe_t* __FASTCALL__ probe(uint32_t wtag) { UNUSED(wtag); return NULL; }

MPXP_Rc init(ad_private_t *priv)
{
    UNUSED(priv);
    return MPXP_Ok;
}

ad_private_t* preinit(const audio_probe_t* probe,sh_audio_t *sh,audio_filter_info_t* afi)
{
    UNUSED(probe);
    UNUSED(afi);
    ad_private_t* priv = new(zeromem) ad_private_t;
    priv->sh = sh;
    return priv;
}

void uninit(ad_private_t *priv)
{
    delete priv;
}

MPXP_Rc control_ad(ad_private_t *priv,int cmd,any_t* arg, ...)
{
    UNUSED(priv);
    UNUSED(cmd);
    UNUSED(arg);
    return MPXP_Unknown;
}

unsigned decode(ad_private_t *priv,unsigned char *buf,unsigned minlen,unsigned maxlen,float *pts)
{
    UNUSED(priv);
    UNUSED(buf);
    UNUSED(minlen);
    UNUSED(maxlen);
    *pts=0;
    return 0;
}
