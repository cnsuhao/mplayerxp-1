#ifndef __MPLAYER_SUBREADER_H
#define __MPLAYER_SUBREADER_H

#include <string>

namespace	usr {
    class Video_Output;
}
using namespace	usr;

extern int sub_uses_time;
extern int sub_errs;
extern int sub_num;         // number of subtitle structs

// subtitle formats
enum {
    SUB_INVALID		=-1,
    SUB_MICRODVD	=0,
    SUB_SUBRIP		=1,
    SUB_SUBVIEWER	=2,
    SUB_SAMI		=3,
    SUB_VPLAYER		=4,
    SUB_RT		=5,
    SUB_SSA		=6,
    SUB_DUNNOWHAT	=7, // FIXME what format is it ?
    SUB_MPSUB		=8,
    SUB_AQTITLE		=9,

    SUB_MAX_TEXT	=5
};

struct subtitle {
    int lines;

    unsigned long start;
    unsigned long end;

    char *text[SUB_MAX_TEXT];
};

extern subtitle* sub_read_file (const std::string& filename, float pts);
extern std::string sub_filename(const std::string& path,const std::string& fname);
extern void list_sub_file(subtitle* subs);
extern void dump_mpsub(subtitle* subs, float fps);
extern void sub_free(subtitle* subs );
extern void find_sub(subtitle* subtitles,unsigned long key,Video_Output*vo_data);

extern void subcp_open ();
extern void subcp_close ();
extern subtitle* subcp_recode (subtitle *sub);
extern subtitle* subcp_recode1 (subtitle *sub);

#endif
