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
    VideoFlag_None		=0x00000000,
    VideoFlag_Flip		=0x00000001,
    VideoFlag_YUVHack		=0x00000002
}video_flags_e;
inline video_flags_e operator~(video_flags_e a) { return static_cast<video_flags_e>(~static_cast<unsigned>(a)); }
inline video_flags_e operator|(video_flags_e a, video_flags_e b) { return static_cast<video_flags_e>(static_cast<unsigned>(a)|static_cast<unsigned>(b)); }
inline video_flags_e operator&(video_flags_e a, video_flags_e b) { return static_cast<video_flags_e>(static_cast<unsigned>(a)&static_cast<unsigned>(b)); }
inline video_flags_e operator^(video_flags_e a, video_flags_e b) { return static_cast<video_flags_e>(static_cast<unsigned>(a)^static_cast<unsigned>(b)); }
inline video_flags_e operator|=(video_flags_e a, video_flags_e b) { return (a=static_cast<video_flags_e>(static_cast<unsigned>(a)|static_cast<unsigned>(b))); }
inline video_flags_e operator&=(video_flags_e a, video_flags_e b) { return (a=static_cast<video_flags_e>(static_cast<unsigned>(a)&static_cast<unsigned>(b))); }
inline video_flags_e operator^=(video_flags_e a, video_flags_e b) { return (a=static_cast<video_flags_e>(static_cast<unsigned>(a)^static_cast<unsigned>(b))); }

typedef enum {
    VCodecStatus_Working	=3,
    VCodecStatus_Problems	=2,
    VCodecStatus_Untested	=1,
    VCodecStatus_NotWorking	=0,
}vcodec_status_e;

struct video_probe_t {
    const char*		driver;
    const char*		codec_dll;
    uint32_t		fourcc;
    vcodec_status_e	status;
    uint32_t		pix_fmt[Video_MaxOutFmt];
    video_flags_e	flags[Video_MaxOutFmt];
};

struct vd_info_t {
    const char *descr; /* driver description ("Autodesk FLI/FLC Animation decoder" */
    const char *driver_name; /* driver name ("dshow") */
    const char *author; /* interface author/maintainer */
    const char *url; /* URL of homepage */
};

struct put_slice_info_t {
    int			vf_flags;
    unsigned		active_slices; // used in dec_video+vd_ffmpeg only!!!
};
/* interface of video decoder drivers */
struct vd_functions_t {
    const vd_info_t*	info;
    const mpxp_option_t*	options;/**< Optional: MPlayerXP's option related */
    const video_probe_t*(*__FASTCALL__ probe)(uint32_t fourcc);
    Opaque*		(*__FASTCALL__ preinit)(const video_probe_t& probe,sh_video_t *sh,put_slice_info_t& psi);
    MPXP_Rc		(*__FASTCALL__ init)(Opaque& ctx,video_decoder_t& opaque);
    void		(*__FASTCALL__ uninit)(Opaque& ctx);
    MPXP_Rc		(*control_vd)(Opaque& ctx,int cmd,any_t* arg, ...);
    mp_image_t*		(*__FASTCALL__ decode)(Opaque& ctx,const enc_frame_t& frame);
};

extern const vd_functions_t* vfm_find_driver(const std::string& name);
extern const video_probe_t* vfm_driver_probe(Opaque& ctx,sh_video_t *sh,put_slice_info_t& psi);

enum {
    VDCTRL_QUERY_FORMAT		=3, /* test for availabilty of a format */
    VDCTRL_QUERY_MAX_PP_LEVEL	=4, /* test for postprocessing support (max level) */
    VDCTRL_SET_PP_LEVEL		=5, /* set postprocessing level */
    VDCTRL_SET_EQUALIZER	=6, /* set color options (brightness,contrast etc) */
    VDCTRL_RESYNC_STREAM	=7 /* resync video stream if needed */
};
// callbacks:
MPXP_Rc		__FASTCALL__ mpcodecs_config_vf(video_decoder_t& opaque, int w, int h);
mp_image_t*	__FASTCALL__ mpcodecs_get_image(video_decoder_t& opaque, int mp_imgtype, int mp_imgflag,int w, int h);
void		__FASTCALL__ mpcodecs_draw_slice(video_decoder_t& opaque, mp_image_t*);
void		__FASTCALL__ mpcodecs_draw_image(video_decoder_t& opaque, mp_image_t *mpi);

#endif
