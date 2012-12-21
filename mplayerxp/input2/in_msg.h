#ifndef IN_MSG_H
#define IN_MSG_H

#include "mpxp_msg.h"

namespace mpxp {
    static mpxp_ostream_info	mpxp_info(MSGT_INPUT);
    static mpxp_ostream_fatal	mpxp_fatal(MSGT_INPUT);
    static mpxp_ostream_err	mpxp_err(MSGT_INPUT);
    static mpxp_ostream_warn	mpxp_warn(MSGT_INPUT);
    static mpxp_ostream_ok	mpxp_ok(MSGT_INPUT);
    static mpxp_ostream_hint	mpxp_hint(MSGT_INPUT);
    static mpxp_ostream_status	mpxp_status(MSGT_INPUT);
    static mpxp_ostream_v	mpxp_v(MSGT_INPUT);
    static mpxp_ostream_dbg2	mpxp_dbg2(MSGT_INPUT);
    static mpxp_ostream_dbg3	mpxp_dbg3(MSGT_INPUT);
    static mpxp_ostream_dbg4	mpxp_dbg4(MSGT_INPUT);
} // namespace mpxp

#endif
