/*
 *  video_out.h
 *
 *      Copyright (C) Aaron Holtzman - Aug 1999
 *	Strongly modified, most parts rewritten: A'rpi/ESP-team - 2000-2001
 *
 */
 
#ifndef __VIDEO_OUT_H
#define __VIDEO_OUT_H 1

#include "../mp_config.h"
#include <inttypes.h>
#include <stdarg.h>

#include "font_load.h"
#include "img_format.h"
#include "vo_msg.h"
#include "../mp_image.h"

#define VO_EVENT_EXPOSE 1
#define VO_EVENT_RESIZE 2
#define VO_EVENT_KEYPRESS 4
#define VO_EVENT_FORCE_UPDATE 0x80000000

#define VOCTRL_UNUSED0 1
#define VOCTRL_QUERY_FORMAT 2	/**< Query direct FOURCC support. Takes a pointer to uint32_t fourcc */
#define VOCTRL_RESET 3		/**< Signal a device reset seek */
#define VOCTRL_FULLSCREEN 4	/**< Signal to switch window to fullscreen. Must affect window only (not surfaces) */
#define VOCTRL_UNUSED 5
#define VOCTRL_PAUSE 6		/**< Notification to stop a device (for dxr3) */
#define VOCTRL_RESUME 7		/**< Notification to start/resume playback after pause (for dxr3) */
#define VOCTRL_UNUSED2 8
#define VOCTRL_CHECK_EVENTS 9	/**< Notification that user performs key pressing. Takes (vo_resize_t *)&vrest as arg. Must return at least VO_EVENT_RESIZE */
#define VOCTRL_GET_NUM_FRAMES 10/**< Query total number of allocated frames (multibuffering) */
#define VOCTRL_UNUSED3 12
#define VOCTRL_FLUSH_PAGES 13	/**< Flush pages of frame from RAM into video memory (bus mastering) */
#define VOCTRL_UNUSED4 14
#define VOCTRL_SET_EQUALIZER    1000 /**< Set video equalizer */
#define VOCTRL_GET_EQUALIZER    1001 /**< Get video equalizer */


#define VO_TRUE		1
#define VO_FALSE	0
#define VO_ERROR	-1
#define VO_NOTAVAIL	-2
#define VO_NOTIMPL	-3

#define VOFLAG_FULLSCREEN	0x01 /**< User wants to have fullscreen playback */
#define VOFLAG_MODESWITCHING	0x02 /**< User enables to find the best video mode */
#define VOFLAG_SWSCALE		0x04 /**< Obsolete. User enables slow Software scaler */
#define VOFLAG_FLIPPING		0x08 /**< User enables page flipping (doublebuffering / XP mode) */

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
	int		(*__FASTCALL__ adjust_size)(unsigned cw,unsigned ch,unsigned *nw,unsigned *nh);
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

/** VO-driver interface */
typedef struct vo_functions_s
{
	/** Preinitializes driver (real INITIALIZATION)
	 * @param arg	currently it's vo_subdevice
	 * @return	zero on successful initialization, non-zero on error.
	**/
	uint32_t (* __FASTCALL__ preinit)(const char *arg);

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
        uint32_t (* __FASTCALL__ config)(uint32_t width, uint32_t height, uint32_t d_width,
			 uint32_t d_height, uint32_t fullscreen, char *title,
			 uint32_t format,const vo_tune_info_t *);

	/** Control interface
	 * @param request	command. See VOCTRL_** for detail
	 * @param data		data associated with command
	 * @return		VO_TRUE if success VO_FALSE VO_ERROR VO_NOTIMPL otherwise
	 **/
	uint32_t (* __FASTCALL__ control)(uint32_t request, void *data);

        /** Returns driver information.
         * @return	read-only pointer to a vo_info_t structure.
         **/
        const vo_info_t* (*get_info)(void);

        /** Blit/Flip buffer to the screen. Must be called after each frame!
	 * @param idex	index of frame to be selected as active frame
         **/
        void (* __FASTCALL__ change_frame)(unsigned idx);

        /** Closes driver. Should restore the original state of the system.
         **/
        void (*uninit)(void);

} vo_functions_t;

/******************************************************
* High level VO functions to provide some abstraction *
* level for video out library                         *
\*****************************************************/
extern void		vo_print_help( void );
extern const vo_functions_t * vo_register(const char *driver_name);
extern const vo_info_t*	vo_get_info( void );
extern int	 __FASTCALL__ vo_init( const char *subdevice_name);
extern uint32_t  __FASTCALL__ vo_config(uint32_t width, uint32_t height, uint32_t d_width,
				  uint32_t d_height, uint32_t fullscreen, char *title,
				  uint32_t format,const vo_tune_info_t *);
extern uint32_t	 __FASTCALL__ vo_query_format( uint32_t* fourcc,unsigned src_w,unsigned src_h);
extern uint32_t		vo_reset( void );
extern uint32_t		vo_fullscreen( void );
extern uint32_t		vo_screenshot( void );
extern uint32_t		vo_pause( void );
extern uint32_t		vo_resume( void );
extern uint32_t	 __FASTCALL__ vo_get_surface( mp_image_t* mpi );
extern int		vo_check_events( void );
extern uint32_t	 __FASTCALL__ vo_get_num_frames( unsigned * );
extern uint32_t	 __FASTCALL__ vo_get_frame_num( volatile unsigned * );
extern uint32_t	 __FASTCALL__ vo_set_frame_num( volatile unsigned * );
extern uint32_t	 __FASTCALL__ vo_get_active_frame( volatile unsigned * );
extern uint32_t	 __FASTCALL__ vo_set_active_frame( volatile unsigned * );
extern uint32_t  __FASTCALL__ vo_draw_frame(const mp_image_t *mpi);
extern uint32_t  __FASTCALL__ vo_draw_slice(const mp_image_t *mpi);
extern void		vo_change_frame(void);
extern void		vo_flush_pages(void);
extern void		vo_draw_osd(void);
extern void		vo_uninit( void );
extern uint32_t __FASTCALL__ vo_control(uint32_t request, void *data);

struct vo_rect {
  int left, right, top, bottom, width, height;
};

/** Contains geometry of fourcc */
typedef struct s_vo_format_desc
{
    unsigned bpp;
    /* in some strange fourccs (NV12) horz period != vert period of UV */
    unsigned x_mul[4],x_div[4];
    unsigned y_mul[4],y_div[4];
}vo_format_desc;
extern int	__FASTCALL__	vo_describe_fourcc(uint32_t fourcc,vo_format_desc *vd);


int vo_x11_init(void);

// NULL terminated array of all drivers
extern const vo_functions_t* video_out_drivers[];

extern int vo_flags;

// correct resolution/bpp on screen:  (should be autodetected by vo_x11_init())
extern unsigned vo_depthonscreen;
extern unsigned vo_screenwidth;
extern unsigned vo_screenheight;

// requested resolution/bpp:  (-x -y -bpp options)
extern unsigned vo_dx;
extern unsigned vo_dy;
extern unsigned vo_dwidth;
extern unsigned vo_dheight;
extern unsigned vo_dbpp;

extern unsigned vo_old_x;
extern unsigned vo_old_y; 
extern unsigned vo_old_width;
extern unsigned vo_old_height;

extern int vo_doublebuffering;
extern int vo_vsync;
extern int vo_fs;
extern int vo_fsmode;

extern int vo_pts;
extern float vo_fps;

extern char *vo_subdevice;

extern int vo_gamma_brightness;
extern int vo_gamma_saturation;
extern int vo_gamma_contrast;
extern int vo_gamma_hue;
extern int vo_gamma_red_intensity;
extern int vo_gamma_green_intensity;
extern int vo_gamma_blue_intensity;

extern unsigned vo_da_buffs; /**< contains number of buffers for decoding ahead */
extern unsigned vo_use_bm; /**< indicates user's agreement for using busmastering */

extern int fullscreen;
extern int vidmode;
extern int softzoom;
extern int flip;
extern int opt_screen_size_x;
extern int opt_screen_size_y;
extern float screen_size_xy;
extern float movie_aspect;
extern int vo_flags;

#endif
