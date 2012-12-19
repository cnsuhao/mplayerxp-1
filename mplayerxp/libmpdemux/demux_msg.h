#ifndef DEMUX_MSG_H
#define DEMUX_MSG_H

#define MSGT_CLASS MSGT_DEMUXER
#include "mpxp_msg.h"

namespace mpxp {
    static mpxp_ostream_info	mpxp_info(MSGT_DEMUXER);
    static mpxp_ostream_fatal	mpxp_fatal(MSGT_DEMUXER);
    static mpxp_ostream_err	mpxp_err(MSGT_DEMUXER);
    static mpxp_ostream_warn	mpxp_warn(MSGT_DEMUXER);
    static mpxp_ostream_ok	mpxp_ok(MSGT_DEMUXER);
    static mpxp_ostream_hint	mpxp_hint(MSGT_DEMUXER);
    static mpxp_ostream_status	mpxp_status(MSGT_DEMUXER);
    static mpxp_ostream_v	mpxp_v(MSGT_DEMUXER);
    static mpxp_ostream_dbg2	mpxp_dbg2(MSGT_DEMUXER);
    static mpxp_ostream_dbg3	mpxp_dbg3(MSGT_DEMUXER);
    static mpxp_ostream_dbg4	mpxp_dbg4(MSGT_DEMUXER);
} // namespace mpxp

#endif
