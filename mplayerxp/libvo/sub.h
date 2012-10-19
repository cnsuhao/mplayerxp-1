#ifndef __MPLAYER_SUB_H
#define __MPLAYER_SUB_H
#include <inttypes.h>
#include "osd.h"

typedef struct mp_osd_bbox_s {
    int x1,y1,x2,y2;
} mp_osd_bbox_t;

#define OSDTYPE_OSD		1
#define OSDTYPE_SUBTITLE	2
#define OSDTYPE_PROGBAR		3
#define OSDTYPE_SPU		4
#define OSDTYPE_VOBSUB		5
#define OSDTYPE_DVDNAV		6
#define OSDTYPE_TELETEXT	7

#define OSDFLAG_VISIBLE		1
#define OSDFLAG_CHANGED		2
#define OSDFLAG_BBOX		4
#define OSDFLAG_OLD_BBOX	8
#define OSDFLAG_FORCE_UPDATE	16

#define MAX_UCS			1600
#define MAX_UCSLINES		16

typedef struct mp_osd_obj_s {
    struct mp_osd_obj_s* next;
    unsigned char type;
    unsigned char alignment; // 2 bits: x;y percents, 2 bits: x;y relative to parent; 2 bits: alignment left/right/center
    unsigned short flags;
    int x,y;
    int dxs,dys;
    mp_osd_bbox_t bbox; // bounding box
    mp_osd_bbox_t old_bbox; // the renderer will save bbox here
    int cleared_frames; // The number of frames that has been cleared from old OSD, -1 = don't clear
    union {
	struct {
	    const any_t* sub;		// value of vo_sub at last update
	    int utbl[MAX_UCS+1];	// subtitle text
	    int xtbl[MAX_UCSLINES];	// x positions
	    int lines;			// no. of lines
	} subtitle;
	struct {
	    int elems;
	} progbar;
    } params;
    int stride;
    int allocated;
    unsigned char *alpha_buffer;
    unsigned char *bitmap_buffer;
} mp_osd_obj_t;


#if 0

// disable subtitles:
static inline void vo_draw_text_osd(int dxs,int dys,void (*draw_alpha)(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride)){
}

#else

#define OSD_PLAY	0x01
#define OSD_PAUSE	0x02
#define OSD_STOP	0x03
#define OSD_REW		0x04
#define OSD_FFW		0x05
#define OSD_CLOCK	0x06
#define OSD_CONTRAST	0x07
#define OSD_SATURATION	0x08
#define OSD_VOLUME	0x09
#define OSD_BRIGHTNESS	0x0A
#define OSD_HUE		0x0B
#define OSD_DVDMENU	0x0C

#define OSD_PB_START	0x10
#define OSD_PB_0	0x11
#define OSD_PB_END	0x12
#define OSD_PB_1	0x13

typedef struct sub_data_s {
    char *	cp;
    int		unicode;
    int		utf8;
    int		pos;
    int		bg_color; /* subtitles background color */
    int		bg_alpha;
}sub_data_t;
extern sub_data_t sub_data;

typedef void (* __FASTCALL__ draw_osd_f)(unsigned idx,int x0,int y0, int w,int h,const unsigned char* src,const unsigned char *srca, int stride);
typedef void (* __FASTCALL__ clear_osd_f)(unsigned idx,int x0,int y0, int w,int h);

extern void __FASTCALL__ vo_draw_text(unsigned idx,int dxs,int dys, draw_osd_f draw_alpha);
extern void __FASTCALL__ vo_remove_text(unsigned idx,int dxs,int dys,clear_osd_f remove);

void vo_init_osd(void);
int __FASTCALL__ vo_update_osd(int dxs,int dys);
int __FASTCALL__ vo_osd_changed(int new_value);
int __FASTCALL__ get_osd_height(int c,int h);
void __FASTCALL__ osd_set_nav_box (uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey);

#endif
#endif
