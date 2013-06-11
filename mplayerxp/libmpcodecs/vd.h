#ifndef VD_H_INCLUDED
#define VD_H_INCLUDED 1

#include "libmpconf/cfgparser.h"
#include "xmpcore/xmp_enums.h"
#include "dec_video.h"

namespace	usr {
    enum {
	Video_MaxOutFmt	=16,
    };

// Outfmt flags:
    enum video_flags_e {
	VideoFlag_None		=0x00000000,
	VideoFlag_Flip		=0x00000001,
	VideoFlag_YUVHack	=0x00000002
    };
    inline video_flags_e operator~(video_flags_e a) { return static_cast<video_flags_e>(~static_cast<unsigned>(a)); }
    inline video_flags_e operator|(video_flags_e a, video_flags_e b) { return static_cast<video_flags_e>(static_cast<unsigned>(a)|static_cast<unsigned>(b)); }
    inline video_flags_e operator&(video_flags_e a, video_flags_e b) { return static_cast<video_flags_e>(static_cast<unsigned>(a)&static_cast<unsigned>(b)); }
    inline video_flags_e operator^(video_flags_e a, video_flags_e b) { return static_cast<video_flags_e>(static_cast<unsigned>(a)^static_cast<unsigned>(b)); }
    inline video_flags_e operator|=(video_flags_e& a, video_flags_e b) { return (a=static_cast<video_flags_e>(static_cast<unsigned>(a)|static_cast<unsigned>(b))); }
    inline video_flags_e operator&=(video_flags_e& a, video_flags_e b) { return (a=static_cast<video_flags_e>(static_cast<unsigned>(a)&static_cast<unsigned>(b))); }
    inline video_flags_e operator^=(video_flags_e& a, video_flags_e b) { return (a=static_cast<video_flags_e>(static_cast<unsigned>(a)^static_cast<unsigned>(b))); }

    enum vcodec_status_e {
	VCodecStatus_Working	=3,
	VCodecStatus_Problems	=2,
	VCodecStatus_Untested	=1,
	VCodecStatus_NotWorking	=0,
    };

    struct video_probe_t {
	const char*		driver;
	const char*		codec_dll;
	uint32_t		fourcc;
	vcodec_status_e		status;
	uint32_t		pix_fmt[Video_MaxOutFmt];
	video_flags_e		flags[Video_MaxOutFmt];
    };

    struct put_slice_info_t {
	int			vf_flags;
	unsigned		active_slices; // used in dec_video+vd_ffmpeg only!!!
    };

    enum {
	VDCTRL_QUERY_FORMAT		=3, /* test for availabilty of a format */
	VDCTRL_QUERY_MAX_PP_LEVEL	=4, /* test for postprocessing support (max level) */
	VDCTRL_SET_PP_LEVEL		=5, /* set postprocessing level */
	VDCTRL_SET_EQUALIZER		=6, /* set color options (brightness,contrast etc) */
	VDCTRL_RESYNC_STREAM		=7 /* resync video stream if needed */
    };

    /* interface of video decoder drivers */
    struct video_decoder_t;
    class Video_Decoder : public Opaque {
	public:
	    Video_Decoder(video_decoder_t&,sh_video_t&,put_slice_info_t&,uint32_t fourcc) { UNUSED(fourcc); }
	    virtual ~Video_Decoder() {}

	    virtual MPXP_Rc		ctrl(int cmd,any_t* arg,long arg2=0) = 0;
	    virtual mp_image_t*		run(const enc_frame_t& frame) = 0;
	    virtual video_probe_t	get_probe_information() const = 0;
    };

    struct vd_info_t {
	const char*	descr; /* driver description ("Autodesk FLI/FLC Animation decoder" */
	const char*	driver_name; /* driver name ("dshow") */
	const char*	author; /* interface author/maintainer */
	const char*	url; /* URL of homepage */
	Video_Decoder*	(*query_interface)(video_decoder_t&,sh_video_t&,put_slice_info_t&,uint32_t fourcc);
	const mpxp_option_t*	options;/**< Optional: MPlayerXP's option related */
    };

    const vd_info_t*	vfm_find_driver(const std::string& name);
    Video_Decoder*	vfm_probe_driver(video_decoder_t&,sh_video_t& sh,put_slice_info_t& psi);

    // callbacks:
    MPXP_Rc		__FASTCALL__ mpcodecs_config_vf(video_decoder_t& opaque, int w, int h);
    mp_image_t*		__FASTCALL__ mpcodecs_get_image(video_decoder_t& opaque, int mp_imgtype, int mp_imgflag,int w, int h);
    void		__FASTCALL__ mpcodecs_draw_slice(video_decoder_t& opaque, mp_image_t*);
    void		__FASTCALL__ mpcodecs_draw_image(video_decoder_t& opaque, mp_image_t *mpi);
} // namespace	usr
#endif
