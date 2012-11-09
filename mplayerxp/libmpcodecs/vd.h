#ifndef VD_H_INCLUDED
#define VD_H_INCLUDED 1

#include "libmpconf/cfgparser.h"
#include "xmpcore/xmp_enums.h"

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
    MPXP_Rc		(*__FASTCALL__ init)(sh_video_t *sh);
    void		(*__FASTCALL__ uninit)(sh_video_t *sh);
    MPXP_Rc		(* control)(sh_video_t *sh,int cmd,any_t* arg, ...);
    mp_image_t*		(*__FASTCALL__ decode)(sh_video_t *sh,any_t* data,int len,int flags);
} vd_functions_t;

// NULL terminated array of all drivers
extern const vd_functions_t* mpcodecs_vd_drivers[];

enum {
    VDCTRL_QUERY_FORMAT		=3, /* test for availabilty of a format */
    VDCTRL_QUERY_MAX_PP_LEVEL	=4, /* test for postprocessing support (max level) */
    VDCTRL_SET_PP_LEVEL		=5, /* set postprocessing level */
    VDCTRL_SET_EQUALIZER	=6, /* set color options (brightness,contrast etc) */
    VDCTRL_RESYNC_STREAM	=7 /* resync video stream if needed */
};
// callbacks:
MPXP_Rc		__FASTCALL__ mpcodecs_config_vo(sh_video_t *sh, int w, int h, any_t*tune);
mp_image_t*	__FASTCALL__ mpcodecs_get_image(sh_video_t *sh, int mp_imgtype, int mp_imgflag,int w, int h);
void		__FASTCALL__ mpcodecs_draw_slice(sh_video_t* sh, mp_image_t*);
void		__FASTCALL__ mpcodecs_draw_image(sh_video_t* sh, mp_image_t *mpi);

#endif
