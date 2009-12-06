/*
   Generic alpha renderers for all YUV modes and RGB depths.
   These are "reference implementations", should be optimized later (MMX, etc)
   Templating Code from Michael Niedermayer (michaelni@gmx.at) is under GPL
*/

#include <stdio.h>
#include <pthread.h>
#include "../mp_config.h"
//#define ENABLE_PROFILE
#include "../my_profile.h"
#include <inttypes.h>
#include "../cpudetect.h"
#include "../mangle.h"
#include "vo_msg.h"
#include "../mplayer.h"
#include "osd.h"

#if defined(__MMX__)
static const uint64_t bFF __attribute__((used))  __attribute__((aligned(8))) = 0xFFFFFFFFFFFFFFFFULL;
static const unsigned long long mask24lh  __attribute__((used)) __attribute__((aligned(8))) = 0xFFFF000000000000ULL;
static const unsigned long long mask24hl  __attribute__((used)) __attribute__((aligned(8))) = 0x0000FFFFFFFFFFFFULL;
#endif

/* generic version */
#undef OPTIMIZE_AVX
#undef OPTIMIZE_SSE4
#undef OPTIMIZE_SSSE3
#undef OPTIMIZE_SSE3
#undef OPTIMIZE_SSE2
#undef OPTIMIZE_SSE
#undef OPTIMIZE_MMX2
#undef OPTIMIZE_MMX
#define RENAME(a) a ## _C
#include "osd_template.c"

#ifndef __x86_64__
#ifdef __MMX2__
#define OPTIMIZE_MMX2
#undef RENAME
#define RENAME(a) a ## _MMX2
#include "osd_template.c"
#endif
#endif // __x86_64__
#ifdef __SSE2__
#define OPTIMIZE_SSE2
#undef RENAME
#define RENAME(a) a ## _SSE2
#include "osd_template.c"
#endif
#ifdef __SSE3__
#define OPTIMIZE_SSE3
#undef RENAME
#define RENAME(a) a ## _SSE3
#include "osd_template.c"
#endif
#ifdef __SSSE3__
#define OPTIMIZE_SSSE3
#undef RENAME
#define RENAME(a) a ## _SSSE3
#include "osd_template.c"
#endif
#ifdef __SSE4_1__
#define OPTIMIZE_SSE4
#undef RENAME
#define RENAME(a) a ## _SSE4
#include "osd_template.c"
#endif


#ifdef FAST_OSD_TABLE
static unsigned short fast_osd_15bpp_table[256];
static unsigned short fast_osd_16bpp_table[256];
#endif

static void __FASTCALL__ vo_draw_alpha_rgb15_C(int w,int h, const unsigned char* src, const unsigned char *srca, int srcstride, unsigned char* dstbase,int dststride){
    int y;
    for(y=0;y<h;y++){
        register unsigned short *dst = (unsigned short*) dstbase;
        register int x;
        for(x=0;x<w;x++){
            if(srca[x]){
#ifdef FAST_OSD
#ifdef FAST_OSD_TABLE
                dst[x]=fast_osd_15bpp_table[src[x]];
#else
		register unsigned int a=src[x]>>3;
                dst[x]=(a<<10)|(a<<5)|a;
#endif
#else
                unsigned char r=dst[x]&0x1F;
                unsigned char g=(dst[x]>>5)&0x1F;
                unsigned char b=(dst[x]>>10)&0x1F;
                r=(((r*srca[x])>>5)+src[x])>>3;
                g=(((g*srca[x])>>5)+src[x])>>3;
                b=(((b*srca[x])>>5)+src[x])>>3;
                dst[x]=(b<<10)|(g<<5)|r;
#endif
            }
        }
        src+=srcstride;
        srca+=srcstride;
        dstbase+=dststride;
    }
    return;
}

static void __FASTCALL__ vo_draw_alpha_rgb16_C(int w,int h, const unsigned char* src, const unsigned char *srca, int srcstride, unsigned char* dstbase,int dststride){
    int y;
    for(y=0;y<h;y++){
        register unsigned short *dst = (unsigned short*) dstbase;
        register int x;
        for(x=0;x<w;x++){
            if(srca[x]){
#ifdef FAST_OSD
#ifdef FAST_OSD_TABLE
                dst[x]=fast_osd_16bpp_table[src[x]];
#else
                dst[x]=((src[x]>>3)<<11)|((src[x]>>2)<<5)|(src[x]>>3);
#endif
#else
                unsigned char r=dst[x]&0x1F;
                unsigned char g=(dst[x]>>5)&0x3F;
                unsigned char b=(dst[x]>>11)&0x1F;
                r=(((r*srca[x])>>5)+src[x])>>3;
                g=(((g*srca[x])>>6)+src[x])>>2;
                b=(((b*srca[x])>>5)+src[x])>>3;
                dst[x]=(b<<11)|(g<<5)|r;
#endif
            }
        }
        src+=srcstride;
        srca+=srcstride;
        dstbase+=dststride;
    }
    return;
}

static void __FASTCALL__ vo_draw_alpha_uyvy_C(int w,int h, const unsigned char* src, const unsigned char *srca, int srcstride, unsigned char* dstbase,int dststride){
    (*vo_draw_alpha_yuy2_ptr)(w,h,src,srca,srcstride,dstbase+1,dststride);
}

draw_alpha_f vo_draw_alpha_yv12_ptr=NULL;
draw_alpha_f vo_draw_alpha_yuy2_ptr=NULL;
draw_alpha_f vo_draw_alpha_rgb24_ptr=NULL;
draw_alpha_f vo_draw_alpha_rgb32_ptr=NULL;
draw_alpha_f vo_draw_alpha_uyvy_ptr=vo_draw_alpha_uyvy_C;
draw_alpha_f vo_draw_alpha_rgb15_ptr=vo_draw_alpha_rgb15_C;
draw_alpha_f vo_draw_alpha_rgb16_ptr=vo_draw_alpha_rgb16_C;

void vo_draw_alpha_init( void ){
#ifdef FAST_OSD_TABLE
    int i;
    for(i=0;i<256;i++){
        fast_osd_15bpp_table[i]=((i>>3)<<10)|((i>>3)<<5)|(i>>3);
        fast_osd_16bpp_table[i]=((i>>3)<<11)|((i>>2)<<5)|(i>>3);
    }
#endif
/*FIXME the optimized stuff is a lie for 15/16bpp as they arent optimized yet*/
// ordered per speed fasterst first
#ifdef __SSE4_1__
if(gCpuCaps.hasSSE41)
{
	MSG_V("Using SSE4 Optimized OnScreenDisplay\n");
	vo_draw_alpha_yv12_ptr=vo_draw_alpha_yv12_SSE4;
	vo_draw_alpha_yuy2_ptr=vo_draw_alpha_yuy2_SSE4;
	vo_draw_alpha_rgb24_ptr=vo_draw_alpha_rgb24_SSE4;
	vo_draw_alpha_rgb32_ptr=vo_draw_alpha_rgb32_SSE4;
}
else
#endif
#ifdef __SSSE3__
if(gCpuCaps.hasSSSE3)
{
	MSG_V("Using SSSE3 Optimized OnScreenDisplay\n");
	vo_draw_alpha_yv12_ptr=vo_draw_alpha_yv12_SSSE3;
	vo_draw_alpha_yuy2_ptr=vo_draw_alpha_yuy2_SSSE3;
	vo_draw_alpha_rgb24_ptr=vo_draw_alpha_rgb24_SSSE3;
	vo_draw_alpha_rgb32_ptr=vo_draw_alpha_rgb32_SSSE3;
}
else
#endif
#ifdef __SSE3__
if(gCpuCaps.hasSSE3)
{
	MSG_V("Using SSE3 Optimized OnScreenDisplay\n");
	vo_draw_alpha_yv12_ptr=vo_draw_alpha_yv12_SSE3;
	vo_draw_alpha_yuy2_ptr=vo_draw_alpha_yuy2_SSE3;
	vo_draw_alpha_rgb24_ptr=vo_draw_alpha_rgb24_SSE3;
	vo_draw_alpha_rgb32_ptr=vo_draw_alpha_rgb32_SSE3;
}
else
#endif
#ifdef __SSE2__
if(gCpuCaps.hasSSE2)
{
	MSG_V("Using SSE2 Optimized OnScreenDisplay\n");
	vo_draw_alpha_yv12_ptr=vo_draw_alpha_yv12_SSE2;
	vo_draw_alpha_yuy2_ptr=vo_draw_alpha_yuy2_SSE2;
	vo_draw_alpha_rgb24_ptr=vo_draw_alpha_rgb24_SSE2;
	vo_draw_alpha_rgb32_ptr=vo_draw_alpha_rgb32_SSE2;
}
else
#endif
#ifndef __x86_64__
#ifdef __MMX2__
if(gCpuCaps.hasMMX2)
{
	MSG_V("Using MMX (with tiny bit MMX2) Optimized OnScreenDisplay\n");
	vo_draw_alpha_yv12_ptr=vo_draw_alpha_yv12_MMX2;
	vo_draw_alpha_yuy2_ptr=vo_draw_alpha_yuy2_MMX2;
	vo_draw_alpha_rgb24_ptr=vo_draw_alpha_rgb24_MMX2;
	vo_draw_alpha_rgb32_ptr=vo_draw_alpha_rgb32_MMX2;
}
else
#endif
#endif
{
	MSG_V("Using generic OnScreenDisplay\n");
	vo_draw_alpha_yv12_ptr=vo_draw_alpha_yv12_C;
	vo_draw_alpha_yuy2_ptr=vo_draw_alpha_yuy2_C;
	vo_draw_alpha_rgb24_ptr=vo_draw_alpha_rgb24_C;
	vo_draw_alpha_rgb32_ptr=vo_draw_alpha_rgb32_C;
}
}
