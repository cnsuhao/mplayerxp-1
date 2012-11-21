#ifndef __MP_MSG_H
#define __MP_MSG_H 1

#include "mplayerxp.h"

// verbosity elevel:

// stuff from level MSGL_FATAL-MSGL_HINT should be translated.

/* TODO: more highlighted levels */
enum {
    MSGL_FATAL	=0U, /* will exit/abort			LightRed */
    MSGL_ERR	=1U, /* continues			Red */
    MSGL_WARN	=2U, /* only warning			Yellow */
    MSGL_OK	=3U, /* checkpoint was passed OK.	LightGreen */
    MSGL_HINT	=4U, /* short help message		LightCyan */
    MSGL_INFO	=5U, /* -quiet				LightGray */
    MSGL_STATUS	=6U, /* v=0 (old status line)		LightBlue */
    MSGL_V	=7U, /* v=1				Cyan */
    MSGL_DBG2	=8U, /* v=2				LightGray */
    MSGL_DBG3	=9U, /* v=3				LightGray */
    MSGL_DBG4	=10U,/* v=4				LightGray */

    MSGL_MASK	=0xF0000000
};
// code/module:
enum {
    MSGT_GLOBAL		=0x00000001,
    MSGT_CPLAYER	=0x00000002,
    MSGT_VO		=0x00000004,
    MSGT_AO		=0x00000008,
    MSGT_DEMUXER	=0x00000010,
    MSGT_CFGPARSER	=0x00000020,
    MSGT_DECAUDIO	=0x00000040,
    MSGT_DECVIDEO	=0x00000080,
    MSGT_VOBSUB		=0x00000100,
    MSGT_OSDEP		=0x00000200,
    MSGT_SPUDEC		=0x00000400,
    MSGT_PLAYTREE	=0x00000800,
    MSGT_INPUT		=0x00001000,
    MSGT_OSD		=0x00002000,
    MSGT_CPUDETECT	=0x00004000,
    MSGT_CODECCFG	=0x00008000,
    MSGT_SWS		=0x00010000,
    MSGT_FINDSUB	=0x00020000,
    MSGT_SUBREADER	=0x00040000,
    MSGT_PP		=0x00080000,
    MSGT_NLS		=0x00100000,
    MSGT_STREAM		=0x00200000,

    MSGT_MASK		=0x0FFFFFFF
};


void mpxp_print_init(int verbose);
void mpxp_print_uninit(void);
void mpxp_print_flush(void);
int  mpxp_printf( unsigned x, const char *format, ... );

#ifdef __GNUC__
static inline int mpxp_print_dummy(const char* args,...) { UNUSED(args); return 0;}
#define mpxp_print(mod,lev, args... ) ((lev<(mp_conf.verbose+MSGL_V))?(mpxp_printf(((lev&0xF)<<28)|(mod&0x0FFFFFFF),## args)):(mpxp_print_dummy(args)))
#else
#define mpxp_print(mod,lev, ... ) mpxp_printf(((lev&0xF)<<28)|(mod&0x0FFFFFFF),__VA_ARGS__)
#endif

#ifndef MSGT_CLASS
#define MSGT_CLASS MSGT_CPLAYER
#endif

#ifdef __va_arg_pack /* requires gcc-4.3.x */
static __always_inline int MSG_INFO(const char* args,...) { return mpxp_print(MSGT_CLASS,MSGL_INFO,args,__va_arg_pack ()); }
static __always_inline int MSG_FATAL(const char* args,...) { return mpxp_print(MSGT_CLASS,MSGL_FATAL,args,__va_arg_pack ()); }
static __always_inline int MSG_WARN(const char* args,...) { return mpxp_print(MSGT_CLASS,MSGL_WARN,args,__va_arg_pack ()); }
static __always_inline int MSG_ERR(const char* args,...) { return mpxp_print(MSGT_CLASS,MSGL_ERR,args,__va_arg_pack ()); }
static __always_inline int MSG_V(const char* args,...) { return mpxp_print(MSGT_CLASS,MSGL_V,args,__va_arg_pack ()); }
static __always_inline int MSG_OK(const char* args,...) { return mpxp_print(MSGT_CLASS,MSGL_OK,args,__va_arg_pack ()); }
static __always_inline int MSG_HINT(const char* args,...) { return mpxp_print(MSGT_CLASS,MSGL_HINT,args,__va_arg_pack ()); }
static __always_inline int MSG_STATUS(const char* args,...) { return mpxp_print(MSGT_CLASS,MSGL_STATUS,args,__va_arg_pack ()); }

#ifdef MP_DEBUG
static __always_inline int MSG_DBG2(const char* args,...) { return mpxp_print(MSGT_CLASS,MSGL_DBG3,args,__va_arg_pack ()); }
static __always_inline int MSG_DBG3(const char* args,...) { return mpxp_print(MSGT_CLASS,MSGL_DBG4,args,__va_arg_pack ()); }
#else
static __always_inline int MSG_DBG2(const char* args,...) { return mpxp_print_dummy(args); }
static __always_inline int MSG_DBG3(const char* args,...) { return mpxp_print_dummy(args); }
#endif

#else // __va_arg_pack
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
#define MSG_INFO(args...) mpxp_print(MSGT_CLASS,MSGL_INFO,##args )
#define MSG_FATAL(args...) mpxp_print(MSGT_CLASS,MSGL_FATAL,##args )
#define MSG_WARN(args...) mpxp_print(MSGT_CLASS,MSGL_WARN,##args )
#define MSG_ERR(args...) mpxp_print(MSGT_CLASS,MSGL_ERR,##args )
#define MSG_V(args...) mpxp_print(MSGT_CLASS,MSGL_V,##args )
#define MSG_OK(args...) mpxp_print(MSGT_CLASS,MSGL_OK,##args )
#define MSG_HINT(args...) mpxp_print(MSGT_CLASS,MSGL_HINT,##args )
#define MSG_STATUS(args...) mpxp_print(MSGT_CLASS,MSGL_STATUS,##args )
#ifdef MP_DEBUG
#define MSG_DBG2(args...) mpxp_print(MSGT_CLASS,MSGL_DBG2,##args )
#define MSG_DBG3(args...) mpxp_print(MSGT_CLASS,MSGL_DBG3,##args )
#else
#define MSG_DBG2(args...) mpxp_print_dummy();
#define MSG_DBG3(args...) mpxp_print_dummy();
#endif
#endif // __va_arg_pack

#endif // __MP_MSG_H
