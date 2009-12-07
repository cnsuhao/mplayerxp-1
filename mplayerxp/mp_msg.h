
#ifndef _MP_MSG_H
#define _MP_MSG_H

extern unsigned verbose; // defined in mplayer.c

// verbosity elevel:

// stuff from level MSGL_FATAL-MSGL_HINT should be translated.

/* TODO: more highlighted levels */
#define MSGL_FATAL	 0U /* will exit/abort			LightRed */
#define MSGL_ERR	 1U /* continues			Red */
#define MSGL_WARN	 2U /* only warning			Yellow */
#define MSGL_OK		 3U /* checkpoint was passed OK.	LightGreen */
#define MSGL_HINT	 4U /* short help message		LightCyan */
#define MSGL_INFO	 5U /* -quiet				LightGray */
#define MSGL_STATUS	 6U /* v=0 (old status line)		LightBlue */
#define MSGL_V		 7U /* v=1				Cyan */
#define MSGL_DBG2	 8U /* v=2				LightGray */
#define MSGL_DBG3	 9U /* v=3				LightGray */
#define MSGL_DBG4	10U /* v=4				LightGray */

// code/module:

#define MSGT_GLOBAL	0x00000001
#define MSGT_CPLAYER	0x00000002
#define MSGT_VO		0x00000004
#define MSGT_AO		0x00000008
#define MSGT_DEMUXER	0x00000010
#define MSGT_CFGPARSER	0x00000020
#define MSGT_DECAUDIO	0x00000040
#define MSGT_DECVIDEO	0x00000080
#define MSGT_VOBSUB	0x00000100
#define MSGT_OSDEP	0x00000200
#define MSGT_SPUDEC	0x00000400
#define MSGT_PLAYTREE	0x00000800
#define MSGT_INPUT	0x00001000
#define MSGT_OSD	0x00002000
#define MSGT_CPUDETECT	0x00004000
#define MSGT_CODECCFG	0x00008000
#define MSGT_SWS	0x00010000
#define MSGT_FINDSUB	0x00020000
#define MSGT_SUBREADER	0x00040000
#define MSGT_PP		0x00080000
#define MSGT_NLS	0x00100000

#define MSGT_MASK	0x0FFFFFFF

#define MSGL_MASK	0xF0000000

void mp_msg_init(int verbose);
void mp_msg_uninit(void);
void mp_msg_flush(void);
void mp_msg_c( unsigned x, const char *srcfile,unsigned linenum,const char *format, ... );

#ifdef __GNUC__
static inline void mp_msg_dummy(void) {}
#define mp_msg(mod,lev, file, args... ) ((lev<(verbose+MSGL_V))?(mp_msg_c(((lev&0xF)<<28)|(mod&0x0FFFFFFF),file,## args)):(mp_msg_dummy()))
#else
#define mp_msg(mod,lev, file, ... ) mp_msg_c(((lev&0xF)<<28)|(mod&0x0FFFFFFF),file,__VA_ARGS__)
#endif

#endif
