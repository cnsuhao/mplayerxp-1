#ifndef __MPLAYER_SUBREADER_H
#define __MPLAYER_SUBREADER_H

extern int sub_uses_time;
extern int sub_errs;
extern int sub_num;         // number of subtitle structs

// subtitle formats
#define SUB_INVALID   -1
#define SUB_MICRODVD  0
#define SUB_SUBRIP    1
#define SUB_SUBVIEWER 2
#define SUB_SAMI      3
#define SUB_VPLAYER   4
#define SUB_RT        5
#define SUB_SSA       6
#define SUB_DUNNOWHAT 7		// FIXME what format is it ?
#define SUB_MPSUB     8
#define SUB_AQTITLE   9

// One of the SUB_* constant above
extern int sub_format;
extern char *sub_cp;

#define SUB_MAX_TEXT 5

typedef struct {

    int lines;

    unsigned long start;
    unsigned long end;

    char *text[SUB_MAX_TEXT];
} subtitle;

extern subtitle* sub_read_file (const char *filename, float pts);
extern char * sub_filename(const char *path,const char *fname);
extern void list_sub_file(subtitle* subs);
extern void dump_mpsub(subtitle* subs, float fps);
extern void sub_free(subtitle* subs );
extern void find_sub(subtitle* subtitles,unsigned long key);

extern void subcp_open (void);
extern void subcp_close (void);
extern subtitle* subcp_recode (subtitle *sub);
extern subtitle* subcp_recode1 (subtitle *sub);

#endif
