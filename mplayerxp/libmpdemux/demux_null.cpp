#include "mp_config.h"

#include <stdlib.h>
#include <stdio.h>

#include "libmpstream/stream.h"
#include "demuxer.h"
#include "stheader.h"
#include "libmpconf/cfgparser.h"

static const config_t demux_null_opts[] = {
  {NULL, NULL, 0, 0, 0, 0, NULL}
};

static const config_t null_conf[] = {
  { "null", (any_t*)&demux_null_opts, CONF_TYPE_SUBCONFIG, 0, 0, 0, "Null specific commands"},
  { NULL,NULL, 0, 0, 0, 0, NULL}
};

static MPXP_Rc null_probe(demuxer_t* demuxer)
{
    return MPXP_False;
}

static demuxer_t* null_open(demuxer_t* demuxer) {
    return NULL;
}

static int null_demux(demuxer_t* demuxer, demux_stream_t *ds) {
    return 0;
}

static void null_seek(demuxer_t *demuxer,const seek_args_t* seeka){
}

static void null_close(demuxer_t* demuxer) {}

static MPXP_Rc null_control(const demuxer_t *demuxer,int cmd,any_t*args)
{
    return MPXP_Unknown;
}

extern const demuxer_driver_t demux_null =
{
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
