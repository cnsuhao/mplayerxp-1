#ifndef DEC_VIDEO_H_INCLUDED
#define DEC_VIDEO_H_INCLUDED 1
#include "xmpcore/xmp_enums.h"
#include "libmpdemux/demuxer_r.h"

namespace	usr {
    struct libinput_t;
    class Video_Decoder;
    struct vd_info_t;
    struct put_slice_info_t;

    class VD_Interface : public Opaque {
	public:
	    VD_Interface(sh_video_t& sh_video, const std::string& codec_name,const std::string& family,int status,libinput_t&libinput);
	    virtual ~VD_Interface();

	    virtual int		run(const enc_frame_t& frame) const;

	    virtual MPXP_Rc	get_quality_max(unsigned& quality) const;
	    virtual MPXP_Rc	set_quality(int quality) const;
	    virtual MPXP_Rc	set_colors(const std::string& item,int value) const;
	    virtual void	resync_stream() const;

	    virtual MPXP_Rc	config_vf(int w, int h) const;
	    virtual mp_image_t*	get_image(int mp_imgtype, int mp_imgflag,int w, int h) const;
	    virtual void	draw_slice(const mp_image_t*) const;
	    virtual void	draw_image(const mp_image_t*) const;
	    static void		print_help();
	private:
	    void		print_codec_info() const;
	    void		update_subtitle(float v_pts,unsigned idx) const;
	    const vd_info_t*	find_driver(const std::string& name);
	    Video_Decoder*	probe_driver(sh_video_t& sh,put_slice_info_t& psi);

	    Opaque&		vd_private;
    };
} // namepsace	usr
#endif
