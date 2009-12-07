#ifndef __MP_MSG_H
#define __MP_MSG_H
#include "./mp_msg.h"
#undef MSG_INFO
#undef MSG_FATAL
#undef MSG_WARN
#undef MSG_ERR
#undef MSG_V
#undef MSG_OK
#undef MSG_DBG2
#undef MSG_DBG3
#undef MSG_HINT
#undef MSG_STATUS
#define MSG_INFO(args...) mp_msg(MSGT_CLASS,MSGL_INFO,__FILE__,__LINE__,##args )
#define MSG_FATAL(args...) mp_msg(MSGT_CLASS,MSGL_FATAL,__FILE__,__LINE__,##args )
#define MSG_WARN(args...) mp_msg(MSGT_CLASS,MSGL_WARN,__FILE__,__LINE__,##args )
#define MSG_ERR(args...) mp_msg(MSGT_CLASS,MSGL_ERR,__FILE__,__LINE__,##args )
#define MSG_V(args...) mp_msg(MSGT_CLASS,MSGL_V,__FILE__,__LINE__,##args )
#define MSG_OK(args...) mp_msg(MSGT_CLASS,MSGL_OK,__FILE__,__LINE__,##args )
#define MSG_HINT(args...) mp_msg(MSGT_CLASS,MSGL_HINT,__FILE__,__LINE__,##args )
#define MSG_STATUS(args...) mp_msg(MSGT_CLASS,MSGL_STATUS,__FILE__,__LINE__,##args )
#ifndef NDEBUG
#define MSG_DBG2(args...) mp_msg(MSGT_CLASS,MSGL_DBG2,__FILE__,__LINE__,##args )
#define MSG_DBG3(args...) mp_msg(MSGT_CLASS,MSGL_DBG3,__FILE__,__LINE__,##args )
#else
#define MSG_DBG2(args...)
#define MSG_DBG3(args...)
#endif
#endif
