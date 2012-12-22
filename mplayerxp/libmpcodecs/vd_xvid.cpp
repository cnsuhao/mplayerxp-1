#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
/*
   Xvid codec - is successor of odivx
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>

#include "mpxp_help.h"

#include <dlfcn.h> /* GLIBC specific. Exists under cygwin too! */

#include "vd_internal.h"
#include "codecs_ld.h"
#include "libvo2/video_out.h"
#include "osdep/bswap.h"

static const vd_info_t info = {
    "XviD MPEG4 codec ",
    "xvid",
    "Nickols_K <(C) Christoph Lampert (gruel@web.de)>",
    "http://www.xvid.org",
};

struct vd_private_t {
    int			cs;
    unsigned char	img_type;
    any_t*		hdl;
    mp_image_t*		mpi;
    int			vo_initialized;
    int			pp_level;
    int			brightness;
    int			resync;
    sh_video_t*		sh;
    video_decoder_t*	parent;
};

static const config_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

LIBVD_EXTERN(xvid)

static const video_probe_t probes[] = {
    { "xvid", "libxvidcore"SLIBSUFFIX, 0x4,                         VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_YUY2, IMGFMT_UYVY, IMGFMT_YVYU, IMGFMT_BGR32, IMGFMT_BGR24, IMGFMT_BGR15, IMGFMT_BGR16}, {VideoFlag_None, VideoFlag_None } },
    { "xvid", "libxvidcore"SLIBSUFFIX, 0x10000004,                  VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_YUY2, IMGFMT_UYVY, IMGFMT_YVYU, IMGFMT_BGR32, IMGFMT_BGR24, IMGFMT_BGR15, IMGFMT_BGR16}, {VideoFlag_None, VideoFlag_None } },
    { "xvid", "libxvidcore"SLIBSUFFIX, FOURCC_TAG('3','I','V','2'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_YUY2, IMGFMT_UYVY, IMGFMT_YVYU, IMGFMT_BGR32, IMGFMT_BGR24, IMGFMT_BGR15, IMGFMT_BGR16}, {VideoFlag_None, VideoFlag_None } },
    { "xvid", "libxvidcore"SLIBSUFFIX, FOURCC_TAG('B','L','Z','0'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_YUY2, IMGFMT_UYVY, IMGFMT_YVYU, IMGFMT_BGR32, IMGFMT_BGR24, IMGFMT_BGR15, IMGFMT_BGR16}, {VideoFlag_None, VideoFlag_None } },
    { "xvid", "libxvidcore"SLIBSUFFIX, FOURCC_TAG('F','M','P','4'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_YUY2, IMGFMT_UYVY, IMGFMT_YVYU, IMGFMT_BGR32, IMGFMT_BGR24, IMGFMT_BGR15, IMGFMT_BGR16}, {VideoFlag_None, VideoFlag_None } },
    { "xvid", "libxvidcore"SLIBSUFFIX, FOURCC_TAG('F','F','D','S'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_YUY2, IMGFMT_UYVY, IMGFMT_YVYU, IMGFMT_BGR32, IMGFMT_BGR24, IMGFMT_BGR15, IMGFMT_BGR16}, {VideoFlag_None, VideoFlag_None } },
    { "xvid", "libxvidcore"SLIBSUFFIX, FOURCC_TAG('F','M','P','4'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_YUY2, IMGFMT_UYVY, IMGFMT_YVYU, IMGFMT_BGR32, IMGFMT_BGR24, IMGFMT_BGR15, IMGFMT_BGR16}, {VideoFlag_None, VideoFlag_None } },
    { "xvid", "libxvidcore"SLIBSUFFIX, FOURCC_TAG('D','C','O','D'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_YUY2, IMGFMT_UYVY, IMGFMT_YVYU, IMGFMT_BGR32, IMGFMT_BGR24, IMGFMT_BGR15, IMGFMT_BGR16}, {VideoFlag_None, VideoFlag_None } },
    { "xvid", "libxvidcore"SLIBSUFFIX, FOURCC_TAG('D','I','G','I'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_YUY2, IMGFMT_UYVY, IMGFMT_YVYU, IMGFMT_BGR32, IMGFMT_BGR24, IMGFMT_BGR15, IMGFMT_BGR16}, {VideoFlag_None, VideoFlag_None } },
    { "xvid", "libxvidcore"SLIBSUFFIX, FOURCC_TAG('D','I','V','1'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_YUY2, IMGFMT_UYVY, IMGFMT_YVYU, IMGFMT_BGR32, IMGFMT_BGR24, IMGFMT_BGR15, IMGFMT_BGR16}, {VideoFlag_None, VideoFlag_None } },
    { "xvid", "libxvidcore"SLIBSUFFIX, FOURCC_TAG('D','I','V','X'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_YUY2, IMGFMT_UYVY, IMGFMT_YVYU, IMGFMT_BGR32, IMGFMT_BGR24, IMGFMT_BGR15, IMGFMT_BGR16}, {VideoFlag_None, VideoFlag_None } },
    { "xvid", "libxvidcore"SLIBSUFFIX, FOURCC_TAG('D','M','4','V'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_YUY2, IMGFMT_UYVY, IMGFMT_YVYU, IMGFMT_BGR32, IMGFMT_BGR24, IMGFMT_BGR15, IMGFMT_BGR16}, {VideoFlag_None, VideoFlag_None } },
    { "xvid", "libxvidcore"SLIBSUFFIX, FOURCC_TAG('D','M','K','2'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_YUY2, IMGFMT_UYVY, IMGFMT_YVYU, IMGFMT_BGR32, IMGFMT_BGR24, IMGFMT_BGR15, IMGFMT_BGR16}, {VideoFlag_None, VideoFlag_None } },
    { "xvid", "libxvidcore"SLIBSUFFIX, FOURCC_TAG('D','P','0','2'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_YUY2, IMGFMT_UYVY, IMGFMT_YVYU, IMGFMT_BGR32, IMGFMT_BGR24, IMGFMT_BGR15, IMGFMT_BGR16}, {VideoFlag_None, VideoFlag_None } },
    { "xvid", "libxvidcore"SLIBSUFFIX, FOURCC_TAG('D','R','E','X'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_YUY2, IMGFMT_UYVY, IMGFMT_YVYU, IMGFMT_BGR32, IMGFMT_BGR24, IMGFMT_BGR15, IMGFMT_BGR16}, {VideoFlag_None, VideoFlag_None } },
    { "xvid", "libxvidcore"SLIBSUFFIX, FOURCC_TAG('D','X','5','0'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_YUY2, IMGFMT_UYVY, IMGFMT_YVYU, IMGFMT_BGR32, IMGFMT_BGR24, IMGFMT_BGR15, IMGFMT_BGR16}, {VideoFlag_None, VideoFlag_None } },
    { "xvid", "libxvidcore"SLIBSUFFIX, FOURCC_TAG('D','X','G','M'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_YUY2, IMGFMT_UYVY, IMGFMT_YVYU, IMGFMT_BGR32, IMGFMT_BGR24, IMGFMT_BGR15, IMGFMT_BGR16}, {VideoFlag_None, VideoFlag_None } },
    { "xvid", "libxvidcore"SLIBSUFFIX, FOURCC_TAG('E','M','4','A'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_YUY2, IMGFMT_UYVY, IMGFMT_YVYU, IMGFMT_BGR32, IMGFMT_BGR24, IMGFMT_BGR15, IMGFMT_BGR16}, {VideoFlag_None, VideoFlag_None } },
    { "xvid", "libxvidcore"SLIBSUFFIX, FOURCC_TAG('E','P','H','V'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_YUY2, IMGFMT_UYVY, IMGFMT_YVYU, IMGFMT_BGR32, IMGFMT_BGR24, IMGFMT_BGR15, IMGFMT_BGR16}, {VideoFlag_None, VideoFlag_None } },
    { "xvid", "libxvidcore"SLIBSUFFIX, FOURCC_TAG('I','N','M','C'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_YUY2, IMGFMT_UYVY, IMGFMT_YVYU, IMGFMT_BGR32, IMGFMT_BGR24, IMGFMT_BGR15, IMGFMT_BGR16}, {VideoFlag_None, VideoFlag_None } },
    { "xvid", "libxvidcore"SLIBSUFFIX, FOURCC_TAG('L','M','P','4'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_YUY2, IMGFMT_UYVY, IMGFMT_YVYU, IMGFMT_BGR32, IMGFMT_BGR24, IMGFMT_BGR15, IMGFMT_BGR16}, {VideoFlag_None, VideoFlag_None } },
    { "xvid", "libxvidcore"SLIBSUFFIX, FOURCC_TAG('M','P','4','S'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_YUY2, IMGFMT_UYVY, IMGFMT_YVYU, IMGFMT_BGR32, IMGFMT_BGR24, IMGFMT_BGR15, IMGFMT_BGR16}, {VideoFlag_None, VideoFlag_None } },
    { "xvid", "libxvidcore"SLIBSUFFIX, FOURCC_TAG('M','P','4','V'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_YUY2, IMGFMT_UYVY, IMGFMT_YVYU, IMGFMT_BGR32, IMGFMT_BGR24, IMGFMT_BGR15, IMGFMT_BGR16}, {VideoFlag_None, VideoFlag_None } },
    { "xvid", "libxvidcore"SLIBSUFFIX, FOURCC_TAG('M','4','S','2'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_YUY2, IMGFMT_UYVY, IMGFMT_YVYU, IMGFMT_BGR32, IMGFMT_BGR24, IMGFMT_BGR15, IMGFMT_BGR16}, {VideoFlag_None, VideoFlag_None } },
    { "xvid", "libxvidcore"SLIBSUFFIX, FOURCC_TAG('M','4','T','3'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_YUY2, IMGFMT_UYVY, IMGFMT_YVYU, IMGFMT_BGR32, IMGFMT_BGR24, IMGFMT_BGR15, IMGFMT_BGR16}, {VideoFlag_None, VideoFlag_None } },
    { "xvid", "libxvidcore"SLIBSUFFIX, FOURCC_TAG('M','V','X','M'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_YUY2, IMGFMT_UYVY, IMGFMT_YVYU, IMGFMT_BGR32, IMGFMT_BGR24, IMGFMT_BGR15, IMGFMT_BGR16}, {VideoFlag_None, VideoFlag_None } },
    { "xvid", "libxvidcore"SLIBSUFFIX, FOURCC_TAG('P','M','4','V'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_YUY2, IMGFMT_UYVY, IMGFMT_YVYU, IMGFMT_BGR32, IMGFMT_BGR24, IMGFMT_BGR15, IMGFMT_BGR16}, {VideoFlag_None, VideoFlag_None } },
    { "xvid", "libxvidcore"SLIBSUFFIX, FOURCC_TAG('Q','M','P','4'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_YUY2, IMGFMT_UYVY, IMGFMT_YVYU, IMGFMT_BGR32, IMGFMT_BGR24, IMGFMT_BGR15, IMGFMT_BGR16}, {VideoFlag_None, VideoFlag_None } },
    { "xvid", "libxvidcore"SLIBSUFFIX, FOURCC_TAG('R','M','P','4'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_YUY2, IMGFMT_UYVY, IMGFMT_YVYU, IMGFMT_BGR32, IMGFMT_BGR24, IMGFMT_BGR15, IMGFMT_BGR16}, {VideoFlag_None, VideoFlag_None } },
    { "xvid", "libxvidcore"SLIBSUFFIX, FOURCC_TAG('S','E','D','G'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_YUY2, IMGFMT_UYVY, IMGFMT_YVYU, IMGFMT_BGR32, IMGFMT_BGR24, IMGFMT_BGR15, IMGFMT_BGR16}, {VideoFlag_None, VideoFlag_None } },
    { "xvid", "libxvidcore"SLIBSUFFIX, FOURCC_TAG('S','I','P','P'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_YUY2, IMGFMT_UYVY, IMGFMT_YVYU, IMGFMT_BGR32, IMGFMT_BGR24, IMGFMT_BGR15, IMGFMT_BGR16}, {VideoFlag_None, VideoFlag_None } },
    { "xvid", "libxvidcore"SLIBSUFFIX, FOURCC_TAG('S','M','P','4'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_YUY2, IMGFMT_UYVY, IMGFMT_YVYU, IMGFMT_BGR32, IMGFMT_BGR24, IMGFMT_BGR15, IMGFMT_BGR16}, {VideoFlag_None, VideoFlag_None } },
    { "xvid", "libxvidcore"SLIBSUFFIX, FOURCC_TAG('S','N','4','0'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_YUY2, IMGFMT_UYVY, IMGFMT_YVYU, IMGFMT_BGR32, IMGFMT_BGR24, IMGFMT_BGR15, IMGFMT_BGR16}, {VideoFlag_None, VideoFlag_None } },
    { "xvid", "libxvidcore"SLIBSUFFIX, FOURCC_TAG('U','L','D','X'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_YUY2, IMGFMT_UYVY, IMGFMT_YVYU, IMGFMT_BGR32, IMGFMT_BGR24, IMGFMT_BGR15, IMGFMT_BGR16}, {VideoFlag_None, VideoFlag_None } },
    { "xvid", "libxvidcore"SLIBSUFFIX, FOURCC_TAG('U','M','P','4'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_YUY2, IMGFMT_UYVY, IMGFMT_YVYU, IMGFMT_BGR32, IMGFMT_BGR24, IMGFMT_BGR15, IMGFMT_BGR16}, {VideoFlag_None, VideoFlag_None } },
    { "xvid", "libxvidcore"SLIBSUFFIX, FOURCC_TAG('V','I','D','M'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_YUY2, IMGFMT_UYVY, IMGFMT_YVYU, IMGFMT_BGR32, IMGFMT_BGR24, IMGFMT_BGR15, IMGFMT_BGR16}, {VideoFlag_None, VideoFlag_None } },
    { "xvid", "libxvidcore"SLIBSUFFIX, FOURCC_TAG('V','S','P','X'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_YUY2, IMGFMT_UYVY, IMGFMT_YVYU, IMGFMT_BGR32, IMGFMT_BGR24, IMGFMT_BGR15, IMGFMT_BGR16}, {VideoFlag_None, VideoFlag_None } },
    { "xvid", "libxvidcore"SLIBSUFFIX, FOURCC_TAG('W','A','W','V'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_YUY2, IMGFMT_UYVY, IMGFMT_YVYU, IMGFMT_BGR32, IMGFMT_BGR24, IMGFMT_BGR15, IMGFMT_BGR16}, {VideoFlag_None, VideoFlag_None } },
    { "xvid", "libxvidcore"SLIBSUFFIX, FOURCC_TAG('X','V','I','D'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_YUY2, IMGFMT_UYVY, IMGFMT_YVYU, IMGFMT_BGR32, IMGFMT_BGR24, IMGFMT_BGR15, IMGFMT_BGR16}, {VideoFlag_None, VideoFlag_None } },
    { "xvid", "libxvidcore"SLIBSUFFIX, FOURCC_TAG('X','V','I','X'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420, IMGFMT_YUY2, IMGFMT_UYVY, IMGFMT_YVYU, IMGFMT_BGR32, IMGFMT_BGR24, IMGFMT_BGR15, IMGFMT_BGR16}, {VideoFlag_None, VideoFlag_None } },
    { NULL, NULL, 0x0, VCodecStatus_NotWorking, {0x0}, { VideoFlag_None }}
};

static const video_probe_t* __FASTCALL__ probe(uint32_t fourcc) {
    unsigned i;
    for(i=0;probes[i].driver;i++)
	if(fourcc==probes[i].fourcc)
	    return &probes[i];
    return NULL;
}

#define XVID_MAKE_VERSION(a,b,c) ((((a)&0xff)<<16) | (((b)&0xff)<<8) | ((c)&0xff))
#define XVID_VERSION_MAJOR(a)    ((char)(((a)>>16) & 0xff))
#define XVID_VERSION_MINOR(a)    ((char)(((a)>> 8) & 0xff))
#define XVID_VERSION_PATCH(a)    ((char)(((a)>> 0) & 0xff))

#define XVID_MAKE_API(a,b)       ((((a)&0xff)<<16) | (((b)&0xff)<<0))
#define XVID_API_MAJOR(a)        (((a)>>16) & 0xff)
#define XVID_API_MINOR(a)        (((a)>> 0) & 0xff)

#define XVID_VERSION             XVID_MAKE_VERSION(1,1,3)
#define XVID_API                 XVID_MAKE_API(4, 1)

#define XVID_GBL_INIT    0 /* initialize xvidcore; must be called before using xvid_decore, or xvid_encore) */
#define XVID_GBL_INFO    1 /* return some info about xvidcore, and the host computer */
#define XVID_GBL_CONVERT 2 /* colorspace conversion utility */

/* XVID_GBL_INIT param1 */
typedef struct {
    int version;
    unsigned int cpu_flags; /* [in:opt] zero = autodetect cpu; XVID_CPU_FORCE|{cpu features} = force cpu features */
    int debug;     /* [in:opt] debug level */
} xvid_gbl_init_t;


/* XVID_GBL_INFO param1 */
typedef struct {
    int version;
    int actual_version; /* [out] returns the actual xvidcore version */
    const char * build; /* [out] if !null, points to description of this xvid core build */
    unsigned int cpu_flags;      /* [out] detected cpu features */
    int num_threads;    /* [out] detected number of cpus/threads */
} xvid_gbl_info_t;

#define XVID_DEC_CREATE  0 /* create decore instance; return 0 on success */
#define XVID_DEC_DESTROY 1 /* destroy decore instance: return 0 on success */
#define XVID_DEC_DECODE  2 /* decode a frame: returns number of bytes consumed >= 0 */

typedef struct {
    int version;
    int width;     /* [in:opt] image width */
    int height;    /* [in:opt] image width */
    any_t* handle; /* [out]	   decore context handle */
/* ------- v1.3.x ------- */
    int fourcc;     /* [in:opt] fourcc of the input video */
    int num_threads;/* [in:opt] number of threads to use in decoder */
} xvid_dec_create_t;

/* colorspace values */
#define XVID_CSP_PLANAR   (1<< 0) /* 4:2:0 planar (==I420, except for pointers/strides) */
#define XVID_CSP_USER	  XVID_CSP_PLANAR
#define XVID_CSP_I420     (1<< 1) /* 4:2:0 planar */
#define XVID_CSP_YV12     (1<< 2) /* 4:2:0 planar */
#define XVID_CSP_YUY2     (1<< 3) /* 4:2:2 packed */
#define XVID_CSP_UYVY     (1<< 4) /* 4:2:2 packed */
#define XVID_CSP_YVYU     (1<< 5) /* 4:2:2 packed */
#define XVID_CSP_BGRA     (1<< 6) /* 32-bit bgra packed */
#define XVID_CSP_ABGR     (1<< 7) /* 32-bit abgr packed */
#define XVID_CSP_RGBA     (1<< 8) /* 32-bit rgba packed */
#define XVID_CSP_ARGB     (1<<15) /* 32-bit argb packed */
#define XVID_CSP_BGR      (1<< 9) /* 24-bit bgr packed */
#define XVID_CSP_RGB555   (1<<10) /* 16-bit rgb555 packed */
#define XVID_CSP_RGB565   (1<<11) /* 16-bit rgb565 packed */
#define XVID_CSP_SLICE    (1<<12) /* decoder only: 4:2:0 planar, per slice rendering */
#define XVID_CSP_INTERNAL (1<<13) /* decoder only: 4:2:0 planar, returns ptrs to internal buffers */
#define XVID_CSP_NULL     (1<<14) /* decoder only: dont output anything */
#define XVID_CSP_VFLIP    (1<<31) /* vertical flip mask */
/* xvid_image_t
	for non-planar colorspaces use only plane[0] and stride[0]
	four plane reserved for alpha*/
typedef struct {
    int csp;			/* [in] colorspace; or with XVID_CSP_VFLIP to perform vertical flip */
    any_t* plane[4];		/* [in] image plane ptrs */
    int stride[4];		/* [in] image stride; "bytes per row"*/
} xvid_image_t;

typedef struct {
	int version;
	int general;         /* [in:opt] general flags */
	any_t*bitstream;     /* [in]     bitstream (read from)*/
	int length;          /* [in]     bitstream length */
	xvid_image_t output; /* [in]     output image (written to) */
/* ------- v1.1.x ------- */
	int brightness;		 /* [in]	 brightness offset (0=none) */
} xvid_dec_frame_t;

/* XVID_DEC_DECODE param2 :: optional */
typedef struct
{
    int version;

    int type;                   /* [out] output data type */
    union {
	struct { /* type>0 {XVID_TYPE_IVOP,XVID_TYPE_PVOP,XVID_TYPE_BVOP,XVID_TYPE_SVOP} */
	    int general;        /* [out] flags */
	    int time_base;      /* [out] time base */
	    int time_increment; /* [out] time increment */

	    /* XXX: external deblocking stuff */
	    int * qscale;	    /* [out] pointer to quantizer table */
	    int qscale_stride;  /* [out] quantizer scale stride */

	} vop;
	struct {	/* XVID_TYPE_VOL */
	    int general;        /* [out] flags */
	    int width;          /* [out] width */
	    int height;         /* [out] height */
	    int par;            /* [out] pixel aspect ratio (refer to XVID_PAR_xxx above) */
	    int par_width;      /* [out] aspect ratio width  [1..255] */
	    int par_height;     /* [out] aspect ratio height [1..255] */
	} vol;
    } data;
} xvid_dec_stats_t;

/* frame type flags */
#define XVID_TYPE_VOL     -1 /* decoder only: vol was decoded */
#define XVID_TYPE_NOTHING  0 /* decoder only (encoder stats): nothing was decoded/encoded */
#define XVID_TYPE_AUTO     0 /* encoder: automatically determine coding type */
#define XVID_TYPE_IVOP     1 /* intra frame */
#define XVID_TYPE_PVOP     2 /* predicted frame */
#define XVID_TYPE_BVOP     3 /* bidirectionally encoded */
#define XVID_TYPE_SVOP     4 /* predicted+sprite frame */

/* aspect ratios */
#define XVID_PAR_11_VGA    1 /* 1:1 vga (square), default if supplied PAR is not a valid value */
#define XVID_PAR_43_PAL    2 /* 4:3 pal (12:11 625-line) */
#define XVID_PAR_43_NTSC   3 /* 4:3 ntsc (10:11 525-line) */
#define XVID_PAR_169_PAL   4 /* 16:9 pal (16:11 625-line) */
#define XVID_PAR_169_NTSC  5 /* 16:9 ntsc (40:33 525-line) */
#define XVID_PAR_EXT      15 /* extended par; use par_width, par_height */

/* XVID_DEC_DECODE param1 */
/* general flags */
#define XVID_LOWDELAY      (1<<0) /* lowdelay mode  */
#define XVID_DISCONTINUITY (1<<1) /* indicates break in stream */
#define XVID_DEBLOCKY      (1<<2) /* perform luma deblocking */
#define XVID_DEBLOCKUV     (1<<3) /* perform chroma deblocking */
#define XVID_FILMEFFECT    (1<<4) /* adds film grain */
#define XVID_DERINGUV      (1<<5) /* perform chroma deringing, requires deblocking to work */
#define XVID_DERINGY       (1<<6) /* perform luma deringing, requires deblocking to work */

#define XVID_DEC_FAST      (1<<29) /* disable postprocessing to decrease cpu usage *todo* */
#define XVID_DEC_DROP      (1<<30) /* drop bframes to decrease cpu usage *todo* */
#define XVID_DEC_PREROLL   (1<<31) /* decode as fast as you can, don't even show output *todo* */

static int (*xvid_decore_ptr)(any_t* handle,
			int dec_opt,
			any_t*param1,
			any_t*param2);
static int (*xvid_global_ptr)(any_t*handle, int opt, any_t*param1, any_t*param2);
static any_t*dll_handle;


static int load_lib( const char *libname )
{
  if(!(dll_handle=ld_codec(libname,mpcodecs_vd_xvid.info->url))) return 0;
  xvid_decore_ptr = (int (*)(any_t*,int,any_t*,any_t*))ld_sym(dll_handle,"xvid_decore");
  xvid_global_ptr = (int (*)(any_t*,int,any_t*,any_t*))ld_sym(dll_handle,"xvid_global");
  return xvid_decore_ptr != NULL && xvid_global_ptr!=NULL;
}

/* Returns DAR value according to VOL's informations contained in stats
 * param */
#if 0
static float stats2aspect(xvid_dec_stats_t *stats)
{
	if (stats->type == XVID_TYPE_VOL) {
		float wpar;
		float hpar;
		float dar;

		/* MPEG4 strem stores PAR (Pixel Aspect Ratio), mplayer uses
		 * DAR (Display Aspect Ratio)
		 *
		 * Both are related thanks to the equation:
		 *            width
		 *      DAR = ----- x PAR
		 *            height
		 *
		 * As MPEG4 is so well designed (*cough*), VOL header carries
		 * both informations together -- lucky eh ? */

		switch (stats->data.vol.par) {
		case XVID_PAR_11_VGA: /* 1:1 vga (square), default if supplied PAR is not a valid value */
			wpar = hpar = 1.0f;
			break;
		case XVID_PAR_43_PAL: /* 4:3 pal (12:11 625-line) */
			wpar = 12;
			hpar = 11;
			break;
		case XVID_PAR_43_NTSC: /* 4:3 ntsc (10:11 525-line) */
			wpar = 10;
			hpar = 11;
			break;
		case XVID_PAR_169_PAL: /* 16:9 pal (16:11 625-line) */
			wpar = 16;
			hpar = 11;
			break;
		case XVID_PAR_169_NTSC: /* 16:9 ntsc (40:33 525-line) */
			wpar = 40;
			hpar = 33;
			break;
		case XVID_PAR_EXT: /* extended par; use par_width, par_height */
			wpar = stats->data.vol.par_width;
			hpar = stats->data.vol.par_height;
			break;
		default:
			wpar = hpar = 1.0f;
			break;
		}

		dar  = ((float)stats->data.vol.width*wpar);
		dar /= ((float)stats->data.vol.height*hpar);

		return dar;
	}

	return 0.0f;
}
#endif

static vd_private_t* preinit(const video_probe_t* probe,sh_video_t *sh,put_slice_info_t* psi){
    UNUSED(psi);
    if(!load_lib(probe->codec_dll)) return NULL;
    vd_private_t* priv = new(zeromem) vd_private_t;
    priv->sh=sh;
    return priv;
}

// init driver
static MPXP_Rc init(vd_private_t *priv,video_decoder_t* opaque){
    xvid_gbl_info_t xvid_gbl_info;
    xvid_gbl_init_t xvid_ini;
    xvid_dec_create_t dec_p;
    sh_video_t* sh = priv->sh;
    int cs;


    memset(&xvid_gbl_info, 0, sizeof(xvid_gbl_info_t));
    xvid_gbl_info.version = XVID_VERSION;

    memset(&xvid_ini, 0, sizeof(xvid_gbl_init_t));
    xvid_ini.version = XVID_VERSION;

    memset(&dec_p, 0, sizeof(xvid_dec_create_t));
    dec_p.version = XVID_VERSION;

    switch(sh->codec->outfmt[sh->outfmtidx]){
	case IMGFMT_YUY2:
	    cs = XVID_CSP_YUY2;
	    break;
	case IMGFMT_UYVY:
	    cs = XVID_CSP_UYVY;
	    break;
	case IMGFMT_YVYU:
	    cs = XVID_CSP_YVYU;
	    break;
	case IMGFMT_YV12:
	    /* We will use our own buffers, this speeds decoding avoiding
	     * frame memcpy's overhead */
	    cs = XVID_CSP_YV12;
	    break;
	case IMGFMT_I420:
	case IMGFMT_IYUV:
	    cs = XVID_CSP_I420;
	    break;
	case IMGFMT_BGR15:
	    cs = XVID_CSP_RGB555;
	    break;
	case IMGFMT_BGR16:
	    cs = XVID_CSP_RGB565;
	    break;
	case IMGFMT_BGR24:
	    cs = XVID_CSP_BGR;
	    break;
	case IMGFMT_BGR32:
	    cs = XVID_CSP_BGRA;
	    break;
	default:
	    MSG_ERR("Unsupported out_fmt: 0x%X\n",
		    sh->codec->outfmt[sh->outfmtidx]);
	    return MPXP_False;
    }

    /* Gather some information about the host library */
    if((*xvid_global_ptr)(NULL, XVID_GBL_INFO, &xvid_gbl_info, NULL) < 0) {
	MSG_INFO("[XVID] could not get information about the library\n");
    } else {
	MSG_INFO("[XVID] using library version %d.%d.%d (build %s)\n",
		XVID_VERSION_MAJOR(xvid_gbl_info.actual_version),
		XVID_VERSION_MINOR(xvid_gbl_info.actual_version),
		XVID_VERSION_PATCH(xvid_gbl_info.actual_version),
		xvid_gbl_info.build);
    }
    if(xvid_gbl_info.actual_version < XVID_VERSION) {
	MSG_ERR("[XVID] please upgrade xvid library from %d.%d.%d to %d.%d.%d\n",
		XVID_VERSION_MAJOR(xvid_gbl_info.actual_version),
		XVID_VERSION_MINOR(xvid_gbl_info.actual_version),
		XVID_VERSION_PATCH(xvid_gbl_info.actual_version),
		XVID_VERSION_MAJOR(XVID_VERSION),
		XVID_VERSION_MINOR(XVID_VERSION),
		XVID_VERSION_PATCH(XVID_VERSION));
	return MPXP_False;
    }

    xvid_ini.cpu_flags = 0; /* autodetect */
    xvid_ini.debug = 0;
    /* Initialize the xvidcore library */
    if((*xvid_global_ptr)(NULL, XVID_GBL_INIT, &xvid_ini, NULL))
	return MPXP_False;

    /* We use 0 width and height so xvidcore will resize its buffers
     * if required. That allows this vd plugin to do resize on first
     * VOL encountered (don't trust containers' width and height) */
    dec_p.width =
    dec_p.height= 0;
    dec_p.fourcc= sh->fourcc;
    dec_p.num_threads=xvid_gbl_info.num_threads;

    /* Get a decoder instance */
    if((*xvid_decore_ptr)(0, XVID_DEC_CREATE, &dec_p, NULL)<0) {
	MSG_ERR("[XVID] init failed\n");
	return MPXP_False;
    }

    if((*xvid_global_ptr)(NULL, XVID_GBL_INFO, &xvid_gbl_info, NULL))
	return MPXP_False;
    MSG_INFO("[XVID] using %u cpus/threads. Flags: %08X\n"
	,xvid_gbl_info.num_threads
	,xvid_gbl_info.cpu_flags);

    priv->parent = opaque;
    priv->cs = cs;
    priv->hdl = dec_p.handle;
    priv->vo_initialized = 0;

    switch(cs) {
	case XVID_CSP_INTERNAL:
	    priv->img_type = MP_IMGTYPE_EXPORT;
	    break;
	case XVID_CSP_USER:
	    priv->img_type = MP_IMGTYPE_STATIC;
	    break;
	default:
	    priv->img_type = MP_IMGTYPE_TEMP;
	    break;
    }
    return mpcodecs_config_vf(opaque,sh->src_w,sh->src_h);
}

// uninit driver
static void uninit(vd_private_t *priv){
    if(!priv) return;
    if(priv->hdl) (*xvid_decore_ptr)(priv->hdl,XVID_DEC_DESTROY, NULL, NULL);
    delete priv;
    dlclose(dll_handle);
}


// decode a frame
static mp_image_t* decode(vd_private_t *priv,const enc_frame_t* frame){
    xvid_dec_frame_t dec;
    xvid_dec_stats_t stats;
    mp_image_t* mpi = NULL;
    int consumed;
    sh_video_t* sh = priv->sh;

    if(frame->len <= 0) return NULL;

    memset(&dec,0,sizeof(xvid_dec_frame_t));
    memset(&stats, 0, sizeof(xvid_dec_stats_t));
    dec.version = XVID_VERSION;
    stats.version = XVID_VERSION;

    dec.bitstream = frame->data;
    dec.length = frame->len;
    dec.general = 0;
    dec.brightness = priv->brightness;
    if(!priv->pp_level)	dec.general |= XVID_LOWDELAY | XVID_DEC_FAST;
    if(priv->pp_level>0)	dec.general = 0;
    if(priv->pp_level>1)	dec.general |= XVID_DEBLOCKY;
    if(priv->pp_level>2)	dec.general |= XVID_DEBLOCKUV;
    if(priv->pp_level>3)	dec.general |= XVID_DERINGY;
    if(priv->pp_level>4)	dec.general |= XVID_DERINGUV;
    if(priv->resync)	{ dec.general |= XVID_DISCONTINUITY; priv->resync=0; }

    if(frame->flags&3) dec.general |= XVID_DEC_DROP;
    dec.output.csp = priv->cs;
    mpi = mpcodecs_get_image(priv->parent, priv->img_type,  MP_IMGFLAG_ACCEPT_STRIDE,
				 sh->src_w, sh->src_h);
    if(mpi->flags&MP_IMGFLAG_DIRECT) mpi->flags|=MP_IMGFLAG_RENDERED;
    if(priv->cs != XVID_CSP_INTERNAL) {
	dec.output.plane[0] = mpi->planes[0];
	dec.output.plane[1] = mpi->planes[1];
	dec.output.plane[2] = mpi->planes[2];
	dec.output.stride[0] = mpi->stride[0];
	dec.output.stride[1] = mpi->stride[1];
	dec.output.stride[2] = mpi->stride[2];
    }
    while(dec.length > 0) {
	/* Decode data */
	consumed = (*xvid_decore_ptr)(priv->hdl, XVID_DEC_DECODE, &dec, &stats);
	if (consumed < 0) {
	    MSG_ERR("Decoding error\n");
	    return NULL;
	}
	if(!(stats.type == XVID_TYPE_VOL || stats.type == XVID_TYPE_NOTHING)) break;
	dec.bitstream = reinterpret_cast<any_t*>(reinterpret_cast<long>(dec.bitstream)+consumed);
	dec.length -= consumed;
    }
    if (mpi != NULL && priv->cs == XVID_CSP_INTERNAL) {
	mpi->planes[0] = reinterpret_cast<unsigned char *>(dec.output.plane[0]);
	mpi->planes[1] = reinterpret_cast<unsigned char *>(dec.output.plane[1]);
	mpi->planes[2] = reinterpret_cast<unsigned char *>(dec.output.plane[2]);

	mpi->stride[0] = dec.output.stride[0];
	mpi->stride[1] = dec.output.stride[1];
	mpi->stride[2] = dec.output.stride[2];
    }
    return (stats.type == XVID_TYPE_NOTHING)?NULL:mpi;
}

// to set/get/query special features/parameters
static MPXP_Rc control_vd(vd_private_t* priv,int cmd,any_t* arg,...){
    switch(cmd){
	case VDCTRL_QUERY_MAX_PP_LEVEL:
	    *((unsigned*)arg)=5;
	    return MPXP_Ok;
	case VDCTRL_SET_PP_LEVEL: {
	    int quality=*((int*)arg);
	    if(quality<0 || quality>5) quality=5;
	    priv->pp_level=quality;
	    return MPXP_Ok;
	}
	case VDCTRL_SET_EQUALIZER: {
	    va_list ap;
	    int value;
	    va_start(ap, arg);
	    value=va_arg(ap, int);
	    va_end(ap);

	    if(strcmp(reinterpret_cast<char*>(arg),VO_EC_BRIGHTNESS)!=0) return MPXP_False;

	    priv->brightness = (value * 256) / 100;
	    return MPXP_Ok;
	}
	case VDCTRL_QUERY_FORMAT:
	    if (*((int*)arg) == IMGFMT_YUY2 ||
		*((int*)arg) == IMGFMT_UYVY ||
		*((int*)arg) == IMGFMT_YVYU ||
		*((int*)arg) == IMGFMT_YV12 ||
		*((int*)arg) == IMGFMT_I420 ||
		*((int*)arg) == IMGFMT_IYUV ||
		*((int*)arg) == IMGFMT_BGR15 ||
		*((int*)arg) == IMGFMT_BGR16 ||
		*((int*)arg) == IMGFMT_BGR24 ||
		*((int*)arg) == IMGFMT_BGR32)
			return MPXP_True;
	    else	return MPXP_False;
	case VDCTRL_RESYNC_STREAM:
	    priv->resync=1;
	    return MPXP_True;
	default: break;
    }
    return MPXP_Unknown;
}
