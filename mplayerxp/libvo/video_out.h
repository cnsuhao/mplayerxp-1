/*
 *  video_out.h
 *
 *      Copyright (C) Aaron Holtzman - Aug 1999
 *	Strongly modified, most parts rewritten: A'rpi/ESP-team - 2000-2001
 *
 */

#ifndef __VIDEO_OUT_H
#define __VIDEO_OUT_H 1

#include "mp_config.h"
#include <inttypes.h>
#include <stdarg.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "font_load.h"
#include "sub.h"
#include "libmpsub/subreader.h"
#include "img_format.h"
#include "xmpcore/mp_image.h"
#include "xmpcore/xmp_enums.h"

enum {
    VO_EVENT_EXPOSE=1,
    VO_EVENT_RESIZE=2,
    VO_EVENT_KEYPRESS=4,
    VO_EVENT_FORCE_UPDATE=0x80000000
};

enum {
    VOCTRL_UNUSED0=1,
    VOCTRL_QUERY_FORMAT=2,	/**< Query direct FOURCC support. Takes a pointer to uint32_t fourcc */
    VOCTRL_RESET=3,		/**< Signal a device reset seek */
    VOCTRL_FULLSCREEN=4,	/**< Signal to switch window to fullscreen. Must affect window only (not surfaces) */
    VOCTRL_UNUSED=5,
    VOCTRL_PAUSE=6,		/**< Notification to stop a device (for dxr3) */
    VOCTRL_RESUME=7,		/**< Notification to start/resume playback after pause (for dxr3) */
    VOCTRL_UNUSED2=8,
    VOCTRL_CHECK_EVENTS=9,	/**< Notification that user performs key pressing. Takes (vo_resize_t *)&vrest as arg. Must return at least VO_EVENT_RESIZE */
    VOCTRL_GET_NUM_FRAMES=10,	/**< Query total number of allocated frames (multibuffering) */
    VOCTRL_UNUSED3=12,
    VOCTRL_FLUSH_PAGES=13,	/**< Flush pages of frame from RAM into video memory (bus mastering) */
    VOCTRL_UNUSED4=14,
    VOCTRL_SET_EQUALIZER=1000,	/**< Set video equalizer */
    VOCTRL_GET_EQUALIZER=1001	/**< Get video equalizer */
};

enum {
    VOFLAG_FULLSCREEN=0x01,	/**< User wants to have fullscreen playback */
    VOFLAG_MODESWITCHING=0x02,	/**< User enables to find the best video mode */
    VOFLAG_SWSCALE=0x04,	/**< Obsolete. User enables slow Software scaler */
    VOFLAG_FLIPPING=0x08	/**< User enables page flipping (doublebuffering / XP mode) */
};

/** Text description of VO-driver */
typedef struct vo_info_s
{
        const char *name;	/**< driver name ("Matrox Millennium G200/G400") */
        const char *short_name; /**< short name (for config strings) ("mga") */
        const char *author;	/**< author ("Aaron Holtzman <aholtzma@ess.engr.uvic.ca>") */
        const char *comment;	/**< any additional comments */
} vo_info_t;

/** Misc info to tuneup VO-driver */
typedef struct vo_tune_info_s
{
	int	pitch[3]; /**< Picthes for image lines. Should be 0 if unknown else power of 2 */
}vo_tune_info_t;

/** Request for supported FOURCC by VO-driver */
typedef struct vo_query_fourcc_s
{
	uint32_t	fourcc;	/**< Fourcc of decoded image */
	unsigned	w,h;	/**< Width and height of decoded image */
}vo_query_fourcc_t;

/** Notification event when windowed output has been resized (as data of VOCTRL_CHECK_EVENT) */
typedef struct vo_resize_s
{
	uint32_t	event_type; /**< X11 event type */

	/** callback to adjust size of window keeping aspect ratio 
	 * @param cw	current window width
	 * @param ch	current window height
	 * @param nw	storage for new width to be stored current window width
	 * @param nh	storage for new height to be stored current window width
	 * @return	0 if fail  !0 if success
	**/
	int		(*__FASTCALL__ adjust_size)(any_t*,unsigned cw,unsigned ch,unsigned *nw,unsigned *nh);
}vo_resize_t;

/** Named video equalizer */
typedef struct vo_videq_s
{
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
}vo_videq_t;

typedef struct vo_gamma_s{
    int		brightness;
    int		saturation;
    int		contrast;
    int		hue;
    int		red_intensity;
    int		green_intensity;
    int		blue_intensity;
}vo_gamma_t;

typedef struct vo_rect_s {
    unsigned x,y,w,h;
}vo_rect_t;

struct vo_rect2 {
  int left, right, top, bottom, width, height;
};

typedef struct vo_conf_s {
    char *		subdevice; // currently unused
    char*		mDisplayName;

    int			WinID; /* output window id */
    int			vsync;

    unsigned		xp_buffs; /**< contains number of buffers for decoding ahead */
    unsigned		use_bm; /**< indicates user's agreement for using busmastering */

    vo_gamma_t		gamma;

    unsigned		screenwidth;
    unsigned		screenheight;

    int			opt_screen_size_x;
    int			opt_screen_size_y;

    float		screen_size_xy;
    float		movie_aspect;
    int			fsmode;
    int			vidmode;
    int			fullscreen;
    int			softzoom;
    int			flip;
    unsigned		dbpp;
}vo_conf_t;
extern vo_conf_t vo_conf;

typedef struct vo_data_s {
    Display*		mDisplay;
    int			mScreen;
    Window		window;
    GC			gc;

    int			flags;
// correct resolution/bpp on screen:  (should be autodetected by vo_x11_init())
    unsigned		depthonscreen;

// requested resolution/bpp:  (-x -y -bpp options)
    vo_rect_t		dest;

    any_t*		vo_priv;/* private data of vo structure */
    any_t*		priv;	/* private data of video driver */
    any_t*		priv2;	/* private data of X11 commons */
    any_t*		priv3;	/* private data of vidix commons */

    /* subtitle support */
    char*		osd_text;
    any_t*		spudec;
    any_t*		vobsub;
    font_desc_t*	font;
    int			osd_progbar_type;
    int			osd_progbar_value;   // 0..255
    const subtitle*	sub;
    int			osd_changed_flag;
}vo_data_t;
static inline int  vo_ZOOM(const vo_data_t*vo) { return vo->flags&VOFLAG_SWSCALE; }
static inline void vo_ZOOM_SET(vo_data_t*vo)   { vo->flags|=VOFLAG_SWSCALE; }
static inline void vo_ZOOM_UNSET(vo_data_t*vo) { vo->flags&=~VOFLAG_SWSCALE; }
static inline int  vo_FS(const vo_data_t*vo)   { return vo->flags&VOFLAG_FULLSCREEN; }
static inline void vo_FS_SET(vo_data_t*vo)     { vo->flags|=VOFLAG_FULLSCREEN; }
static inline void vo_FS_UNSET(vo_data_t*vo)   { vo->flags&=~VOFLAG_FULLSCREEN; }
static inline int  vo_VM(const vo_data_t*vo)   { return vo->flags&VOFLAG_MODESWITCHING; }
static inline void vo_VM_SET(vo_data_t*vo)     { vo->flags|=VOFLAG_MODESWITCHING; }
static inline void vo_VM_UNSET(vo_data_t*vo)   { vo->flags&=~VOFLAG_MODESWITCHING; }
static inline int  vo_FLIP(const vo_data_t*vo) { return vo->flags&VOFLAG_FLIPPING; }
static inline void vo_FLIP_SET(vo_data_t*vo)   { vo->flags|=VOFLAG_FLIPPING; }
static inline void vo_FLIP_UNSET(vo_data_t*vo) { vo->flags&=~VOFLAG_FLIPPING; }
static inline void vo_FLIP_REVERT(vo_data_t*vo){ vo->flags^=VOFLAG_FLIPPING; }

/** VO-driver interface */
typedef struct vo_functions_s
{
	/** Preinitializes driver (real INITIALIZATION)
	 * @param arg	currently it's vo_subdevice
	 * @return	zero on successful initialization, non-zero on error.
	**/
	uint32_t (* __FASTCALL__ preinit)(vo_data_t* vo,const char *arg);

        /** Initializes (means CONFIGURE) the display driver.
	 * @param width		width of source image
         * @param height	height of source image
	 * @param d_width	width of destinition image (may require prescaling)
	 * @param d_height	height of destinition image (may require prescaling)
	 * @param fullscreen	flags (see VOFLAG_XXX for detail)
	 * @param title		window title, if available
	 * @param format	fourcc of source image
         * @return		zero on successful initialization, non-zero on error.
         **/
        uint32_t (* __FASTCALL__ config)(vo_data_t* vo,uint32_t width, uint32_t height, uint32_t d_width,
			 uint32_t d_height, uint32_t fullscreen, char *title,
			 uint32_t format,const vo_tune_info_t *);

	/** Control interface
	 * @param request	command. See VOCTRL_** for detail
	 * @param data		data associated with command
	 * @return		MPXP_True if success MPXP_False VO_ERROR MPXP_NA otherwise
	 **/
	MPXP_Rc (* __FASTCALL__ control)(vo_data_t* vo,uint32_t request, any_t*data);

        /** Returns driver information.
         * @return	read-only pointer to a vo_info_t structure.
         **/
        const vo_info_t* (* __FASTCALL__ get_info)(vo_data_t* vo);

        /** Blit/Flip buffer to the screen. Must be called after each frame!
	 * @param idex	index of frame to be selected as active frame
         **/
        void (* __FASTCALL__ select_frame)(vo_data_t* vo,unsigned idx);

        /** Closes driver. Should restore the original state of the system.
         **/
        void (* __FASTCALL__ uninit)(vo_data_t* vo);

} vo_functions_t;

/******************************************************
* High level VO functions to provide some abstraction *
* level for video out library                         *
\*****************************************************/
extern vo_data_t*	 __FASTCALL__ vo_preinit_structs( void );
extern void		vo_print_help(vo_data_t*);
extern const vo_functions_t * vo_register(vo_data_t* vo,const char *driver_name);
extern const vo_info_t*	vo_get_info(vo_data_t* vo);
extern int	 __FASTCALL__ vo_init(vo_data_t* vo,const char *subdevice_name);
extern uint32_t  __FASTCALL__ vo_config(vo_data_t* vo,uint32_t width, uint32_t height, uint32_t d_width,
				  uint32_t d_height, uint32_t fullscreen, char *title,
				  uint32_t format,const vo_tune_info_t *);
extern uint32_t	 __FASTCALL__ vo_query_format(vo_data_t* vo,uint32_t* fourcc,unsigned src_w,unsigned src_h);
extern uint32_t		vo_reset(vo_data_t* vo);
extern uint32_t		vo_fullscreen(vo_data_t* vo);
extern uint32_t		vo_screenshot(vo_data_t* vo,unsigned idx );
extern uint32_t		vo_pause(vo_data_t* vo);
extern uint32_t		vo_resume(vo_data_t* vo);

extern void		vo_lock_surfaces(vo_data_t* vo);
extern void		vo_unlock_surfaces(vo_data_t* vo);
extern uint32_t	 __FASTCALL__ vo_get_surface(vo_data_t* vo,mp_image_t* mpi);

extern int		vo_check_events(vo_data_t* vo);
extern unsigned	 __FASTCALL__ vo_get_num_frames(vo_data_t* vo);
extern uint32_t  __FASTCALL__ vo_draw_slice(vo_data_t* vo,const mp_image_t *mpi);
extern void		vo_select_frame(vo_data_t* vo,unsigned idx);
extern void		vo_flush_page(vo_data_t* vo,unsigned decoder_idx);
extern void		vo_draw_osd(vo_data_t* vo,unsigned idx);
extern void		vo_draw_spudec_direct(vo_data_t* vo,unsigned idx);
extern void		vo_uninit(vo_data_t* vo);
extern MPXP_Rc __FASTCALL__ vo_control(vo_data_t* vo,uint32_t request, any_t*data);
extern int __FASTCALL__ vo_is_final(vo_data_t* vo);

/** Contains geometry of fourcc */
typedef struct s_vo_format_desc
{
    unsigned bpp;
    /* in some strange fourccs (NV12) horz period != vert period of UV */
    unsigned x_mul[4],x_div[4];
    unsigned y_mul[4],y_div[4];
}vo_format_desc;
extern int	__FASTCALL__	vo_describe_fourcc(uint32_t fourcc,vo_format_desc *vd);

// NULL terminated array of all drivers
extern const vo_functions_t* video_out_drivers[];

#endif
