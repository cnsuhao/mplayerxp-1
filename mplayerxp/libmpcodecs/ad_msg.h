#ifndef AD_MSG_H
#define AD_MSG_H 1
#undef MSG_WARN
#undef MSG_HINT
#undef MSG_V
#undef MSG_INFO
#undef MSG_ERR
#undef MSG_FATAL
#undef MSG_DBG2
#undef MSG_DBG3
#undef MSG_OK
#define MSG_INFO(args...) mp_msg(MSGT_DECAUDIO,MSGL_INFO,__FILE__,__LINE__, ##args )
#define MSG_FATAL(args...) mp_msg(MSGT_DECAUDIO,MSGL_FATAL,__FILE__,__LINE__, ##args )
#define MSG_WARN(args...) mp_msg(MSGT_DECAUDIO,MSGL_WARN,__FILE__,__LINE__, ##args )
#define MSG_ERR(args...) mp_msg(MSGT_DECAUDIO,MSGL_ERR,__FILE__,__LINE__, ##args )
#define MSG_V(args...) mp_msg(MSGT_DECAUDIO,MSGL_V,__FILE__,__LINE__, ##args )
#define MSG_OK(args...) mp_msg(MSGT_DECAUDIO,MSGL_OK,__FILE__,__LINE__, ##args )
#define MSG_DBG2(args...) mp_msg(MSGT_DECAUDIO,MSGL_DBG2,__FILE__,__LINE__, ##args )
#define MSG_DBG3(args...) mp_msg(MSGT_DECAUDIO,MSGL_DBG3,__FILE__,__LINE__, ##args )
#define MSG_HINT(args...) mp_msg(MSGT_DECAUDIO,MSGL_HINT,__FILE__,__LINE__, ##args )
#endif
