#ifndef __CODEC_CFG_H
#define __CODEC_CFG_H

#include <stdint.h>

enum {
    CODECS_MAX_FOURCC	=128,
    CODECS_MAX_OUTFMT	=16,
    CODECS_MAX_INFMT	=16
};
enum {
// Global flags:
    CODECS_FLAG_SEEKABLE	=(1<<0),
    CODECS_FLAG_SELECTED	=(1<<15),  /* for internal use */

// Outfmt flags:
    CODECS_FLAG_FLIP		=(1<<0),
    CODECS_FLAG_NOFLIP		=(1<<1),
    CODECS_FLAG_YUVHACK		=(1<<2),

    CODECS_STATUS__MIN		=0,
    CODECS_STATUS_NOT_WORKING	=0,
    CODECS_STATUS_UNTESTED	=-1,
    CODECS_STATUS_PROBLEMS	=1,
    CODECS_STATUS_WORKING	=2,
    CODECS_STATUS__MAX		=2
};
#ifndef GUID_TYPE
#define GUID_TYPE
typedef struct {
	uint32_t f1;
	uint16_t f2;
	uint16_t f3;
	uint8_t  f4[8];
} GUID;
#endif


typedef struct codecs_st {
	uint32_t fourcc[CODECS_MAX_FOURCC];
	uint32_t fourccmap[CODECS_MAX_FOURCC];
	uint32_t outfmt[CODECS_MAX_OUTFMT];
	unsigned char outflags[CODECS_MAX_OUTFMT];
	uint32_t infmt[CODECS_MAX_INFMT];
	unsigned char inflags[CODECS_MAX_INFMT];
	char codec_name[256];
	char s_info[256];
	char s_comment[256];
	char dll_name[256];
	GUID guid;
	char  driver_name[256];
	short flags;
	short status;
	short cpuflags;
  short priority;
} codecs_t;

extern int parse_codec_cfg(const char *cfgfile);
extern codecs_t* find_video_codec(unsigned int fourcc, unsigned int *fourccmap,const codecs_t *start);
extern codecs_t* find_audio_codec(unsigned int fourcc, unsigned int *fourccmap,const codecs_t *start);
extern codecs_t* find_codec(unsigned int fourcc,unsigned int *fourccmap,const codecs_t *start,int audioflag);
extern void list_codecs(int audioflag);
extern void codecs_reset_selection(int audioflag);

#endif
