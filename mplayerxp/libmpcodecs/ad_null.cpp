#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "ad_internal.h"

struct anull_private_t : public Opaque {
    anull_private_t();
    virtual ~anull_private_t();

    sh_audio_t* sh;
};
anull_private_t::anull_private_t() {}
anull_private_t::~anull_private_t() {}

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

MPXP_Rc init(Opaque& ctx)
{
    UNUSED(ctx);
    return MPXP_Ok;
}

Opaque* preinit(const audio_probe_t& probe,sh_audio_t *sh,audio_filter_info_t& afi)
{
    UNUSED(probe);
    UNUSED(afi);
    anull_private_t* priv = new(zeromem) anull_private_t;
    priv->sh = sh;
    return priv;
}

void uninit(Opaque& ctx) { UNUSED(ctx); }

MPXP_Rc control_ad(Opaque& ctx,int cmd,any_t* arg, ...)
{
    UNUSED(ctx);
    UNUSED(cmd);
    UNUSED(arg);
    return MPXP_Unknown;
}

unsigned decode(Opaque& ctx,unsigned char *buf,unsigned minlen,unsigned maxlen,float& pts)
{
    UNUSED(ctx);
    UNUSED(buf);
    UNUSED(minlen);
    UNUSED(maxlen);
    pts=0;
    return 0;
}
