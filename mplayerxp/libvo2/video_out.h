/*
 *  video_out.h
 *
 *      Copyright (C) Aaron Holtzman - Aug 1999
 *	Strongly modified, most parts rewritten: A'rpi/ESP-team - 2000-2001
 *
 */

#ifndef __VIDEO_OUT_H
#define __VIDEO_OUT_H 1

#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;

#include <string>
#include <inttypes.h>
#include <stdarg.h>

#ifdef HAVE_X11
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif

#include "dri_vo.h"
#include "font_load.h"
#include "sub.h"
#include "libmpsub/subreader.h"
#include "img_format.h"
#include "xmpcore/xmp_image.h"
#include "xmpcore/xmp_enums.h"

namespace	usr {
    enum {
	VO_EVENT_EXPOSE		=1,
	VO_EVENT_RESIZE		=2,
	VO_EVENT_KEYPRESS	=4,
	VO_EVENT_FORCE_UPDATE	=0x80000000
    };

    enum {
	VOCTRL_SET_EQUALIZER=1,	/**< Set video equalizer */
	VOCTRL_GET_EQUALIZER	/**< Get video equalizer */
    };

    enum vo_flags_e {
	VOFLAG_NONE		=0x00,	/**< User wants to have fullscreen playback */
	VOFLAG_FULLSCREEN	=0x01,	/**< User wants to have fullscreen playback */
	VOFLAG_MODESWITCHING	=0x02,	/**< User enables to find the best video mode */
	VOFLAG_SWSCALE		=0x04,	/**< Obsolete. User enables slow Software scaler */
	VOFLAG_FLIPPING		=0x08	/**< User enables page flipping (doublebuffering / XP mode) */
    };

    /** Text description of VO-driver */
    class VO_Interface;
    typedef VO_Interface* (*query_interface_t)(const std::string& args);
    struct vo_info_t {
	const char* name;	/**< driver name ("Matrox Millennium G200/G400") */
	const char* short_name; /**< short name (for config strings) ("mga") */
	const char* author;	/**< author ("Aaron Holtzman <aholtzma@ess.engr.uvic.ca>") */
	const char* comment;/**< any additional comments */
	query_interface_t query_interface;
    };

    enum {
	VOCAP_NA=0x00,
	VOCAP_SUPPORTED=0x01,
	VOCAP_HWSCALER=0x02,
	VOCAP_FLIP=0x04
    };
    inline vo_flags_e operator~(vo_flags_e a) { return static_cast<vo_flags_e>(~static_cast<unsigned>(a)); }
    inline vo_flags_e operator|(vo_flags_e a, vo_flags_e b) { return static_cast<vo_flags_e>(static_cast<unsigned>(a)|static_cast<unsigned>(b)); }
    inline vo_flags_e operator&(vo_flags_e a, vo_flags_e b) { return static_cast<vo_flags_e>(static_cast<unsigned>(a)&static_cast<unsigned>(b)); }
    inline vo_flags_e operator^(vo_flags_e a, vo_flags_e b) { return static_cast<vo_flags_e>(static_cast<unsigned>(a)^static_cast<unsigned>(b)); }
    inline vo_flags_e operator|=(vo_flags_e& a, vo_flags_e b) { return (a=static_cast<vo_flags_e>(static_cast<unsigned>(a)|static_cast<unsigned>(b))); }
    inline vo_flags_e operator&=(vo_flags_e& a, vo_flags_e b) { return (a=static_cast<vo_flags_e>(static_cast<unsigned>(a)&static_cast<unsigned>(b))); }
    inline vo_flags_e operator^=(vo_flags_e& a, vo_flags_e b) { return (a=static_cast<vo_flags_e>(static_cast<unsigned>(a)^static_cast<unsigned>(b))); }

    /** Request for supported FOURCC by VO-driver */
    struct vo_query_fourcc_t {
	uint32_t	fourcc;	/**< Fourcc of decoded image */
	unsigned	w,h;	/**< Width and height of decoded image */
	unsigned	flags;  /**< Flags for this fourcc VOCAP_*  */
    };

    /** Named video equalizer */
    struct vo_videq_t {
#define VO_EC_BRIGHTNESS "Brightness"
#define VO_EC_CONTRAST	 "Contrast"
#define VO_EC_GAMMA	 "Gamma"
#define VO_EC_HUE	 "Hue"
#define VO_EC_SATURATION "Saturation"
#define VO_EC_RED_INTENSITY "RedIntensity"
#define VO_EC_GREEN_INTENSITY "GreenIntensity"
#define VO_EC_BLUE_INTENSITY "BlueIntensity"
	const char *name;	/**< name of equalizer control */
	int	    value;	/**< value of equalizer control in range -1000 +1000 */
    };

    struct vo_gamma_t{
	int		brightness;
	int		saturation;
	int		contrast;
	int		hue;
	int		red_intensity;
	int		green_intensity;
	int		blue_intensity;
    };

    struct vo_rect_t {
	unsigned x,y,w,h;
    };

    struct vo_rect2 {
	int left, right, top, bottom, width, height;
    };

    struct VO_Config {
	VO_Config();
	~VO_Config() {}

	char*		mDisplayName;
	int		xinerama_screen;

	int		vsync;

	unsigned	xp_buffs; /**< contains number of buffers for decoding ahead */
	unsigned	use_bm; /**< indicates user's agreement for using busmastering */

	vo_gamma_t	gamma;

	int		image_width; //opt_screen_size_x
	int		image_height; //opt_screen_size_y
	float		image_zoom; //screen_size_xy

	float		movie_aspect;
	int		fsmode;
	int		vidmode;
	int		fullscreen;
	int		softzoom;
	int		flip;
	unsigned	dbpp;
    };
    extern VO_Config vo_conf;

    class video_private : public Opaque {
	public:
	    video_private() {}
	    virtual ~video_private() {}
    };

    struct vf_stream_t;
    class Video_Output : public Opaque {
	public:
	    Video_Output();
	    virtual ~Video_Output();

	    int		ZOOM() const	{ return flags&VOFLAG_SWSCALE; }
	    void	ZOOM_SET()	{ flags|=VOFLAG_SWSCALE; }
	    void	ZOOM_UNSET()	{ flags&=~VOFLAG_SWSCALE; }
	    int		FS() const	{ return flags&VOFLAG_FULLSCREEN; }
	    void	FS_SET()	{ flags|=VOFLAG_FULLSCREEN; }
	    void	FS_UNSET()	{ flags&=~VOFLAG_FULLSCREEN; }
	    int		VM() const	{ return flags&VOFLAG_MODESWITCHING; }
	    void	VM_SET()	{ flags|=VOFLAG_MODESWITCHING; }
	    void	VM_UNSET()	{ flags&=~VOFLAG_MODESWITCHING; }
	    int		FLIP() const	{ return flags&VOFLAG_FLIPPING; }
	    void	FLIP_SET()	{ flags|=VOFLAG_FLIPPING; }
	    void	FLIP_UNSET()	{ flags&=~VOFLAG_FLIPPING; }
	    void	FLIP_REVERT()	{ flags^=VOFLAG_FLIPPING; }

	    virtual MPXP_Rc	init(const std::string& driver_name) const;
	    static void		print_help();
	    virtual const vo_info_t* get_info() const;
	    virtual MPXP_Rc	configure(vf_stream_t* parent,uint32_t width,
				    uint32_t height, uint32_t d_width,
				    uint32_t d_height, vo_flags_e fullscreen,
				    const std::string& title,
				    uint32_t format);
	    virtual uint32_t	query_format(uint32_t* fourcc,unsigned src_w,unsigned src_h) const;

	    virtual MPXP_Rc	reset() const;
	    virtual MPXP_Rc	fullscreen() const;
	    virtual MPXP_Rc	screenshot(unsigned idx) const;
	    virtual MPXP_Rc	pause() const;
	    virtual MPXP_Rc	resume() const;

	    virtual MPXP_Rc	get_surface(mp_image_t* mpi) const;
	    virtual MPXP_Rc	get_surface_caps(dri_surface_cap_t*) const;

	    virtual int		check_events() const;
	    virtual unsigned	get_num_frames() const;
	    virtual MPXP_Rc	draw_slice(const mp_image_t& mpi) const;
	    virtual void	select_frame(unsigned idx) const;
	    virtual void	flush_page(unsigned decoder_idx) const;
	    virtual void	draw_osd(unsigned idx) const;
	    virtual void	draw_spudec_direct(unsigned idx) const;
	    virtual MPXP_Rc	ctrl(uint32_t request, any_t*data) const;
	    virtual int		is_final() const;

	    virtual int		adjust_size(unsigned cw,unsigned ch,unsigned *nw,unsigned *nh) const;
	    virtual void	dri_remove_osd(unsigned idx,int x0,int _y0, int w,int h) const;
	    virtual void	dri_draw_osd(unsigned idx,int x0,int _y0, int w,int h,const unsigned char* src,const unsigned char *srca, int stride) const;

	    Opaque		unusable;
	    vo_flags_e		flags;
	    /* subtitle support */
	    std::string		osd_text;
	    any_t*		spudec;
	    any_t*		vobsub;
	    font_desc_t*	font;
	    int			osd_progbar_type;
	    int			osd_progbar_value;   // 0..255
	    const subtitle*	sub;
	private:
	    void		dri_config(uint32_t fourcc) const;
	    void		ps_tune(unsigned width,unsigned height) const;
	    void		dri_tune(unsigned width,unsigned height) const;
	    void		dri_reconfig(int is_resize) const;

	    void		clear_rect(unsigned _y0,unsigned h,uint8_t *dest,unsigned stride,unsigned dstride,uint8_t filler) const;
	    void		clear_rect2(unsigned _y0,unsigned h,uint8_t *dest,unsigned stride,unsigned dstride,uint8_t filler) const;
	    void		clear_rect4(unsigned _y0,unsigned h,uint8_t *dest,unsigned stride,unsigned dstride,uint8_t filler) const;
	    void		clear_rect_rgb(unsigned _y0,unsigned h,uint8_t *dest,unsigned stride,unsigned dstride) const;
	    void		clear_rect_yuy2(unsigned _y0,unsigned h,uint8_t *dest,unsigned stride,unsigned dstride) const;

	    video_private&	vo_priv;/* private data of vo structure */
	    int			inited;
    };

    /** Notification event when windowed output has been resized (as data of VOCTRL_CHECK_EVENT) */
    typedef int (*__FASTCALL__ vo_adjust_size_t)(const Video_Output*,unsigned cw,unsigned ch,unsigned *nw,unsigned *nh);
    struct vo_resize_t {
	uint32_t		event_type; /**< X11 event type */

	/** callback to adjust size of window keeping aspect ratio
	    * @param cw	current window width
	    * @param ch	current window height
	    * @param nw	storage for new width to be stored current window width
	    * @param nh	storage for new height to be stored current window width
	    * @return	0 if fail  !0 if success
	**/
	const Video_Output*	vo;
	vo_adjust_size_t	adjust_size;
    };
    /** Contains geometry of fourcc */
    struct vo_format_desc {
	unsigned bpp;
	/* in some strange fourccs (NV12) horz period != vert period of UV */
	unsigned x_mul[4],x_div[4];
	unsigned y_mul[4],y_div[4];
    };
    extern int	__FASTCALL__	vo_describe_fourcc(uint32_t fourcc,vo_format_desc *vd);
} // namespace	usr
#endif
