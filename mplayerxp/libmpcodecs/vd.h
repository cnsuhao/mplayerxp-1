#ifndef VD_H_INCLUDED
#define VD_H_INCLUDED 1

#include "libmpconf/cfgparser.h"
#include "xmpcore/xmp_enums.h"
#include "dec_video.h"

enum {
    Video_MaxOutFmt	=16,
};

// Outfmt flags:
typedef enum {
    VideoFlag_Flip		=0x00000001,
    VideoFlag_YUVHack		=0x00000002
}video_flags_e;

typedef enum {
    VCodecStatus_Working	=3,
    VCodecStatus_Problems	=2,
    VCodecStatus_Untested	=1,
    VCodecStatus_NotWorking	=0,
}vcodec_status_e;

typedef struct video_probe_s {
    const char*		driver;
    const char*		codec_dll;
    uint32_t		fourcc;
    vcodec_status_e	status;
    uint32_t		pix_fmt[Video_MaxOutFmt];
    video_flags_e	flags[Video_MaxOutFmt];
}video_probe_t;

typedef struct vd_info_s
{
    const char *descr; /* driver description ("Autodesk FLI/FLC Animation decoder" */
    const char *driver_name; /* driver name ("dshow") */
    const char *author; /* interface author/maintainer */
    const char *url; /* URL of homepage */
} vd_info_t;

/* interface of video decoder drivers */
typedef struct vd_functions_s
{
    const vd_info_t*	info;
    const config_t*	options;/**< Optional: MPlayerXP's option related */
    const video_probe_t*(*__FASTCALL__ probe)(sh_video_t *sh,uint32_t fourcc);
    MPXP_Rc		(*__FASTCALL__ init)(sh_video_t *sh,any_t* libinput);
    void		(*__FASTCALL__ uninit)(sh_video_t *sh);
    MPXP_Rc		(* control)(sh_video_t *sh,int cmd,any_t* arg, ...);
    mp_image_t*		(*__FASTCALL__ decode)(sh_video_t *sh,const enc_frame_t* frame);
} vd_functions_t;

const vd_functions_t* vfm_find_driver(const char *name);

enum {
    VDCTRL_QUERY_FORMAT		=3, /* test for availabilty of a format */
    VDCTRL_QUERY_MAX_PP_LEVEL	=4, /* test for postprocessing support (max level) */
    VDCTRL_SET_PP_LEVEL		=5, /* set postprocessing level */
    VDCTRL_SET_EQUALIZER	=6, /* set color options (brightness,contrast etc) */
    VDCTRL_RESYNC_STREAM	=7 /* resync video stream if needed */
};
// callbacks:
MPXP_Rc		__FASTCALL__ mpcodecs_config_vo(sh_video_t *sh, int w, int h, any_t* libinput);
mp_image_t*	__FASTCALL__ mpcodecs_get_image(sh_video_t *sh, int mp_imgtype, int mp_imgflag,int w, int h);
void		__FASTCALL__ mpcodecs_draw_slice(sh_video_t* sh, mp_image_t*);
void		__FASTCALL__ mpcodecs_draw_image(sh_video_t* sh, mp_image_t *mpi);

#endif
