#ifndef TV_H
#define TV_H

namespace	usr {
    extern int tv_param_on;

#ifdef USE_TV
    struct tvi_info_t {
	const char *name;
	const char *short_name;
	const char *author;
	const char *comment;
    };

    struct priv_t;
    struct tvi_functions_t {
	int (*init)(priv_t *priv);
	int (*uninit)(priv_t *priv);
	int (*control)(priv_t *priv, int cmd, any_t*arg);
	int (*start)(priv_t *priv);
	double (*grab_video_frame)(priv_t *priv,unsigned char *buffer, int len);
#ifdef HAVE_TV_BSDBT848
	double (*grabimmediate_video_frame)(priv_t *priv,unsigned char *buffer, int len);
#endif
	int (*get_video_framesize)(priv_t *priv);
	double (*grab_audio_frame)(priv_t *priv,unsigned char *buffer, int len);
	int (*get_audio_framesize)(priv_t *priv);
    };

    struct tvi_handle_t : public Opaque {
	public:
	    tvi_handle_t() {}
	    virtual ~tvi_handle_t() {}

	    const tvi_info_t	*info;
	    const tvi_functions_t*functions;
	    priv_t*		priv;
	    int 		seq;

    /* specific */
	    int		norm;
	    int		chanlist;
	    const struct CHANLIST*chanlist_s;
	    int		channel;
    };

    enum {
	TVI_CONTROL_FALSE	=0,
	TVI_CONTROL_TRUE	=1,
	TVI_CONTROL_NA		=-1,
	TVI_CONTROL_UNKNOWN	=-2
    };
    /* ======================== CONTROLS =========================== */

    /* GENERIC controls */
    enum {
	TVI_CONTROL_IS_AUDIO	=0x1,
	TVI_CONTROL_IS_VIDEO	=0x2,
	TVI_CONTROL_IS_TUNER	=0x3,
#ifdef HAVE_TV_BSDBT848
	TVI_CONTROL_IMMEDIATE	=0x4,
#endif
/* VIDEO controls */
	TVI_CONTROL_VID_GET_FPS	=0x101,
	TVI_CONTROL_VID_GET_PLANES	=0x102,
	TVI_CONTROL_VID_GET_BITS	=0x103,
	TVI_CONTROL_VID_CHK_BITS	=0x104,
	TVI_CONTROL_VID_SET_BITS	=0x105,
	TVI_CONTROL_VID_GET_FORMAT	=0x106,
	TVI_CONTROL_VID_CHK_FORMAT	=0x107,
	TVI_CONTROL_VID_SET_FORMAT	=0x108,
	TVI_CONTROL_VID_GET_WIDTH	=0x109,
	TVI_CONTROL_VID_CHK_WIDTH	=0x110,
	TVI_CONTROL_VID_SET_WIDTH	=0x111,
	TVI_CONTROL_VID_GET_HEIGHT	=0x112,
	TVI_CONTROL_VID_CHK_HEIGHT	=0x113,
	TVI_CONTROL_VID_SET_HEIGHT	=0x114,
	TVI_CONTROL_VID_GET_BRIGHTNESS=0x115,
	TVI_CONTROL_VID_SET_BRIGHTNESS=0x116,
	TVI_CONTROL_VID_GET_HUE	=0x117,
	TVI_CONTROL_VID_SET_HUE	=0x118,
	TVI_CONTROL_VID_GET_SATURATION=0x119,
	TVI_CONTROL_VID_SET_SATURATION=0x11a,
	TVI_CONTROL_VID_GET_CONTRAST=0x11b,
	TVI_CONTROL_VID_SET_CONTRAST=0x11c,
	TVI_CONTROL_VID_GET_PICTURE	=0x11d,
	TVI_CONTROL_VID_SET_PICTURE	=0x11e,
/* TUNER controls */
	TVI_CONTROL_TUN_GET_FREQ	=0x201,
	TVI_CONTROL_TUN_SET_FREQ	=0x202,
	TVI_CONTROL_TUN_GET_TUNER	=0x203,/* update priv->tuner struct for used input */
	TVI_CONTROL_TUN_SET_TUNER	=0x204,/* update priv->tuner struct for used input */
	TVI_CONTROL_TUN_GET_NORM	=0x205,
	TVI_CONTROL_TUN_SET_NORM	=0x206,
/* AUDIO controls */
	TVI_CONTROL_AUD_GET_FORMAT	=0x301,
	TVI_CONTROL_AUD_GET_SAMPLERATE=0x302,
	TVI_CONTROL_AUD_GET_SAMPLESIZE=0x303,
	TVI_CONTROL_AUD_GET_CHANNELS=0x304,
	TVI_CONTROL_AUD_SET_SAMPLERATE=0x305,
/* SPECIFIC controls */
	TVI_CONTROL_SPC_GET_INPUT	=0x401,/* set input channel (tv,s-video,composite..) */
	TVI_CONTROL_SPC_SET_INPUT	=0x402,/* set input channel (tv,s-video,composite..) */
    };
    extern __FASTCALL__ tvi_handle_t *tv_begin(void);
    extern int __FASTCALL__ tv_init(tvi_handle_t *tvh);
    extern int __FASTCALL__ tv_uninit(tvi_handle_t *tvh);

    enum {
	TV_COLOR_BRIGHTNESS	=1,
	TV_COLOR_HUE	=2,
	TV_COLOR_SATURATION	=3,
	TV_COLOR_CONTRAST	=4
    };
    int __FASTCALL__ tv_set_color_options(tvi_handle_t *tvh, int opt, int val);
    enum {
	TV_CHANNEL_LOWER	=1,
	TV_CHANNEL_HIGHER	=2
    };
    int __FASTCALL__ tv_step_channel(tvi_handle_t *tvh, int direction);
    enum {
	TV_NORM_PAL		=1,
	TV_NORM_NTSC	=2,
	TV_NORM_SECAM	=3
    };
    int __FASTCALL__ tv_step_norm(tvi_handle_t *tvh);
    int __FASTCALL__ tv_step_chanlist(tvi_handle_t *tvh);
#endif /* USE_TV */
} // namespace	usr
#endif /* TV_H */
