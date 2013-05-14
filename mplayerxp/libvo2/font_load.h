#ifndef __FONT_LOAD_H
#define __FONT_LOAD_H 1
#include <string>

struct raw_file {
    unsigned char *bmp;
    unsigned char *pal;
    int w,h,c;
};

struct font_desc_t {
    char *name;
    char *fpath;
    int spacewidth;
    int charspace;
    int height;
//    char *fname_a;
//    char *fname_b;
    raw_file* pic_a[16];
    raw_file* pic_b[16];
    short font[65536];
    int start[65536];
    short width[65536];
};

raw_file* load_raw(const std::string& name,int verbose);
font_desc_t* read_font_desc(const std::string& fname,float factor,int verbose);

#endif
