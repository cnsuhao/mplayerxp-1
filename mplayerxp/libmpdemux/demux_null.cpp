#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
#include <stdlib.h>
#include <stdio.h>

#include "libmpstream/stream.h"
#include "demuxer.h"
#include "demuxer_internal.h"
#include "stheader.h"
#include "libmpconf/cfgparser.h"

static const config_t demux_null_opts[] = {
  {NULL, NULL, 0, 0, 0, 0, NULL}
};

static const config_t null_conf[] = {
  { "null", (any_t*)&demux_null_opts, CONF_TYPE_SUBCONFIG, 0, 0, 0, "Null specific commands"},
  { NULL,NULL, 0, 0, 0, 0, NULL}
};

static MPXP_Rc null_probe(Demuxer* demuxer)
{
    return MPXP_False;
}

static Demuxer* null_open(Demuxer* demuxer) {
    return NULL;
}

static int null_demux(Demuxer* demuxer, Demuxer_Stream *ds) {
    return 0;
}

static void null_seek(Demuxer *demuxer,const seek_args_t* seeka){
}

static void null_close(Demuxer* demuxer) {}

static MPXP_Rc null_control(const Demuxer *demuxer,int cmd,any_t*args)
{
    return MPXP_Unknown;
}

extern const demuxer_driver_t demux_null =
{
    "null",
    "NULL parser",
    "...",
    null_conf,
    null_probe,
    null_open,
    null_demux,
    null_seek,
    null_close,
    null_control
};
