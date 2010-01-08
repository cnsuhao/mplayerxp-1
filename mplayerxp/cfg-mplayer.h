/*
 * config for cfgparser
 */

#include "cfg-common.h"

extern int av_sync_pts;
extern int av_force_pts_fix;
extern int av_force_pts_fix2;
extern int frame_reorder;
extern int use_stdin;
extern uint32_t mp_msg_filter;
#ifdef HAVE_PNG
extern int z_compression;
#endif
#ifdef HAVE_SDL
//extern char *sdl_driver;
extern int sdl_noxv;
extern int sdl_forcexv;
extern int sdl_forcegl;
//extern char *sdl_adriver;
#endif
#ifdef USE_FAKE_MONO
extern int fakemono; // defined in dec_audio.c
#endif

#ifdef HAVE_LIRC
extern char *lirc_configfile;
#endif

extern int vo_doublebuffering;
extern int vo_vsync;
extern int vo_fsmode;
/* gamma correction */
extern int vo_gamma_brightness;
extern int vo_gamma_saturation;
extern int vo_gamma_contrast;
extern int vo_gamma_hue;
extern int vo_gamma_red_intensity;
extern int vo_gamma_green_intensity;
extern int vo_gamma_blue_intensity;

#ifdef USE_SUB
extern int sub_unicode;
extern int sub_utf8;
#ifdef USE_ICONV
extern char *sub_cp;
#endif
extern int sub_pos;
#endif
extern int subcc_enabled;

#ifdef USE_OSD
extern int osd_level;
#endif

#ifdef HAVE_X11
extern char *mDisplayName;
extern int WinID;
#endif

#ifdef HAVE_AA
extern int vo_aa_parseoption(struct config * conf, char *opt, char * param);
extern void vo_aa_revertoption(config_t* opt,char* param);
#endif

#ifdef HAVE_ZR
extern int vo_zr_parseoption(struct config * conf, char *opt, char * param);
extern void vo_zr_revertoption(config_t* opt,char* pram);
#endif

#ifdef HAVE_XINERAMA
extern int xinerama_screen;
#endif

#ifdef HAVE_RTC
extern int nortc;
#endif

extern int enable_xp;
extern int enable_gomp;
extern int enable_xp_audio;
extern unsigned vo_da_buffs;
extern unsigned vo_use_bm;

#if defined( ARCH_X86 ) || defined(ARCH_X86_64)
extern int x86_mmx;
extern int x86_mmx2;
extern int x86_3dnow;
extern int x86_3dnow2;
extern int x86_sse;
extern int x86_sse2;
#endif

extern float playbackspeed_factor;
/* from libvo/aspect.c */
extern float monitor_pixel_aspect;

/* from dec_audio, currently used for ac3surround decoder only */
extern int audio_output_channels;

extern int sws_flags;
extern int readPPOpt(void *conf, char *arg);
extern char *npp_options;

extern int shuffle_playback;

extern int   network_prefer_ipv4;
extern char *network_username;
extern char *network_password;
extern int   network_bandwidth;
extern char *network_useragent;
extern int   network_ipv4_only_proxy;
extern int   network_cookies_enabled;
extern char *cookies_file;

/*
 * CONF_TYPE_FUNC_FULL :
 * allows own implemtations for passing the params
 * 
 * the function receives parameter name and argument (if it does not start with - )
 * useful with a conf.name like 'aa*' to parse several parameters to a function
 * return 0 =ok, but we didn't need the param (could be the filename)
 * return 1 =ok, we accepted the param
 * negative values: see cfgparser.h, ERR_XXX
 *
 * by Folke
 */

static const config_t mplayer_opts[]={
	/* name, pointer, type, flags, min, max */
	{"include", cfg_include, CONF_TYPE_FUNC_PARAM, CONF_NOSAVE, 0, 0, NULL}, /* this don't need anymore to be the first!!! */

//---------------------- libao/libvo/mplayer options ------------------------
	{"o", "Option -o has been renamed to -vo (video-out), use -vo !\n",
            CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
	{"vo", &video_driver, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"ao", &audio_driver, CONF_TYPE_STRING, 0, 0, 0, NULL},
//	{"dsp", &dsp, CONF_TYPE_STRING, CONF_NOCFG, 0, 0, NULL},
	{"dsp", "Use -ao oss:dsp_path!\n", CONF_TYPE_PRINT, CONF_NOCFG, 0, 0, NULL},
        {"mixer", &mixer_device, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"master", "Option -master has been removed, use -aop list=volume instead.\n", CONF_TYPE_PRINT, 0, 0, 0, NULL},
	{"channels", &audio_output_channels, CONF_TYPE_INT, CONF_RANGE, 2, 8, NULL},
#ifdef HAVE_X11
	{"display", &mDisplayName, CONF_TYPE_STRING, 0, 0, 0, NULL},
#endif
	{"osdlevel", &osd_level, CONF_TYPE_INT, CONF_RANGE, 0, 2 , NULL},
	{"msgfilter", &mp_msg_filter, CONF_TYPE_INT, CONF_RANGE, 0, 0xFFFFFFFF, NULL},

//	{"encode", &encode_name, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"vobsub", &vobsub_name, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"vobsubid", &vobsub_id, CONF_TYPE_INT, CONF_RANGE, 0, 31, NULL},
#ifdef USE_SUB
	{"sub", &sub_name, CONF_TYPE_STRING, 0, 0, 0, NULL},
#ifdef USE_ICONV
	{"subcp", &sub_cp, CONF_TYPE_STRING, 0, 0, 0, NULL},
#endif	
	{"subdelay", &sub_delay, CONF_TYPE_FLOAT, 0, 0.0, 10.0, NULL},
	{"subfps", &sub_fps, CONF_TYPE_FLOAT, 0, 0.0, 10.0, NULL},
        {"noautosub", &sub_auto, CONF_TYPE_FLAG, 0, 1, 0, NULL},
	{"unicode", &sub_unicode, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"nounicode", &sub_unicode, CONF_TYPE_FLAG, 0, 1, 0, NULL},
	{"utf8", &sub_utf8, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"noutf8", &sub_utf8, CONF_TYPE_FLAG, 0, 1, 0, NULL},
 	{"subpos",&sub_pos,  CONF_TYPE_INT, CONF_RANGE, 0, 100, NULL},
#endif
	{"subcc", &subcc_enabled, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"nosubcc", &subcc_enabled, CONF_TYPE_FLAG, 0, 1, 0, NULL},
#ifdef USE_OSD
	{"font", &font_name, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"ffactor", &font_factor, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 10.0, NULL},
	{"spualign", &spu_alignment, CONF_TYPE_INT, CONF_RANGE, -1, 2, NULL},
	{"spuaa", &spu_aamode, CONF_TYPE_INT, CONF_RANGE, 0, 31, NULL},
	{"spugauss", &spu_gaussvar, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 3.0, NULL},
#endif
//	{"bg", &play_in_bg, CONF_TYPE_FLAG, 0, 0, 1, NULL},
//	{"nobg", &play_in_bg, CONF_TYPE_FLAG, 0, 1, 0, NULL},
	{"sb", &seek_to_byte, CONF_TYPE_INT, CONF_MIN, 0, 0, NULL},
	{"ss", &seek_to_sec, CONF_TYPE_STRING, CONF_MIN, 0, 0, NULL},
	{"noloop", &loop_times, CONF_TYPE_FLAG, 0, 0, -1, NULL},
	{"loop", &loop_times, CONF_TYPE_INT, CONF_RANGE, -1, 10000, NULL},
	{"abs", &ao_data.buffersize, CONF_TYPE_INT, CONF_MIN, 0, 0, NULL},
	{"delay", &audio_delay, CONF_TYPE_FLOAT, CONF_RANGE, -10.0, 10.0, NULL},

	{"alsa", "Option -alsa has been removed, new audio code doesn't need it! Remove it from your config file!\n",
            CONF_TYPE_PRINT, 0, 0, 0, NULL},
	{"noalsa", "Option -noalsa has been removed, new audio code doesn't need it! Remove it from your config file!\n",
            CONF_TYPE_PRINT, 0, 0, 0, NULL},

	{"framedrop", &frame_dropping, CONF_TYPE_FLAG, 0, 0, 1, NULL},
/*UD*/	{"hardframedrop", &frame_dropping, CONF_TYPE_FLAG, 0, 0, 2, NULL},
	{"noframedrop", &frame_dropping, CONF_TYPE_FLAG, 0, 1, 0, NULL},
	
	{"pts", &av_sync_pts, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"nopts", &av_sync_pts, CONF_TYPE_FLAG, 0, 1, 0, NULL},
	{"force_pts_fix", &av_force_pts_fix, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"noforce_pts_fix", &av_force_pts_fix, CONF_TYPE_FLAG, 0, 1, 0, NULL},
	{"force_pts_fix2", &av_force_pts_fix2, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"noforce_pts_fix2", &av_force_pts_fix2, CONF_TYPE_FLAG, 0, 1, 0, NULL},
	{"frame_reorder", &frame_reorder, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"noframe_reorder", &frame_reorder, CONF_TYPE_FLAG, 0, 1, 0, NULL},

	{"autoq", &auto_quality, CONF_TYPE_INT, CONF_RANGE, 0, 100, NULL},

	{"benchmark", &benchmark, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	
	{"dump", &stream_dump, CONF_TYPE_STRING, 0, 0, 0, NULL},
#ifdef HAVE_PNG
	{"z", &z_compression, CONF_TYPE_INT, CONF_RANGE, 0, 9, NULL},
#endif
#ifdef HAVE_SDL
/*UD*/	{"sdl", "Use -vo sdl:driver instead of -vo sdl -sdl driver\n",
	    CONF_TYPE_PRINT, 0, 0, 0, NULL},
	{"noxv", &sdl_noxv, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"forcexv", &sdl_forcexv, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"forcegl", &sdl_forcegl, CONF_TYPE_FLAG, 0, 0, 1, NULL},
/*UD*/	{"sdla", "Use -ao sdl:driver instead of -ao sdl -sdla driver\n",
	    CONF_TYPE_PRINT, 0, 0, 0, NULL},
#endif	
	{"x", &opt_screen_size_x, CONF_TYPE_INT, CONF_RANGE, 0, 4096, NULL},
	{"y", &opt_screen_size_y, CONF_TYPE_INT, CONF_RANGE, 0, 4096, NULL},
	{"xy", &screen_size_xy, CONF_TYPE_FLOAT, CONF_RANGE, 0, 4096, NULL},
	{"screenw", &vo_screenwidth, CONF_TYPE_INT, CONF_RANGE, 0, 4096, NULL},
	{"screenh", &vo_screenheight, CONF_TYPE_INT, CONF_RANGE, 0, 4096, NULL},
	{"speed", &playbackspeed_factor, CONF_TYPE_FLOAT, CONF_RANGE, 0.01, 100.0, NULL},
	{"aspect", &movie_aspect, CONF_TYPE_FLOAT, CONF_RANGE, 0.2, 3.0, NULL},
	{"noaspect", &movie_aspect, CONF_TYPE_FLAG, 0, 0, 0, NULL},
	{"monitorpixelaspect", &monitor_pixel_aspect, CONF_TYPE_FLOAT, CONF_RANGE, 0.2, 9.0, NULL},
        {"vm", &vidmode, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"novm", &vidmode, CONF_TYPE_FLAG, 0, 1, 0, NULL},
	{"fs", &fullscreen, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"nofs", &fullscreen, CONF_TYPE_FLAG, 0, 1, 0, NULL},
        {"zoom", &softzoom, CONF_TYPE_FLAG, 0, 0, 1, NULL},
        {"nozoom", &softzoom, CONF_TYPE_FLAG, 0, 1, 0, NULL},
        {"flip", &flip, CONF_TYPE_FLAG, 0, -1, 1, NULL},
        {"noflip", &flip, CONF_TYPE_FLAG, 0, -1, 0, NULL},
       
        {"bpp", &vo_dbpp, CONF_TYPE_INT, CONF_RANGE, 0, 32, NULL},
	{"fsmode", &vo_fsmode, CONF_TYPE_INT, CONF_RANGE, 0, 15, NULL},
	{"double", &vo_doublebuffering, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"nodouble", &vo_doublebuffering, CONF_TYPE_FLAG, 0, 1, 0, NULL},
	{"vsync", &vo_vsync, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"novsync", &vo_vsync, CONF_TYPE_FLAG, 0, 1, 0, NULL},
	{"brightness",&vo_gamma_brightness, CONF_TYPE_INT, CONF_RANGE, -1000, 1000, NULL},
	{"saturation",&vo_gamma_saturation, CONF_TYPE_INT, CONF_RANGE, -1000, 1000, NULL},
	{"contrast",&vo_gamma_contrast, CONF_TYPE_INT, CONF_RANGE, -1000, 1000, NULL},
	{"hue",&vo_gamma_hue, CONF_TYPE_INT, CONF_RANGE, -1000, 1000, NULL},
	{"red_intensity",&vo_gamma_red_intensity, CONF_TYPE_INT, CONF_RANGE, -1000, 1000, NULL},
	{"green_intensity",&vo_gamma_green_intensity, CONF_TYPE_INT, CONF_RANGE, -1000, 1000, NULL},
	{"blue_intensity",&vo_gamma_blue_intensity, CONF_TYPE_INT, CONF_RANGE, -1000, 1000, NULL},

        {"xp", &enable_xp, CONF_TYPE_INT, CONF_RANGE, 0, 4, NULL},
        {"noxp", &enable_xp, CONF_TYPE_FLAG, 0, 1, 0, NULL},
        {"gomp", &enable_gomp, CONF_TYPE_FLAG, 0, 0, 1, NULL},
        {"nogomp", &enable_gomp, CONF_TYPE_FLAG, 0, 1, 0, NULL},
        {"da_buffs", &vo_da_buffs, CONF_TYPE_INT, CONF_RANGE, 4, 1024, NULL},
        {"enable_bm", &vo_use_bm, CONF_TYPE_FLAG, 0, 0, 1, NULL},
        {"enable_bm2", &vo_use_bm, CONF_TYPE_FLAG, 0, 0, 2, NULL},
        {"disable_bm", &vo_use_bm, CONF_TYPE_FLAG, 0, 1, 0, NULL},

#if defined( ARCH_X86 ) || defined(ARCH_X86_64)
        {"mmx", &x86_mmx, CONF_TYPE_FLAG, 0, 0, 1, NULL},
        {"nommx", &x86_mmx, CONF_TYPE_FLAG, 0, 1, 0, NULL},
        {"mmx2", &x86_mmx2, CONF_TYPE_FLAG, 0, 0, 1, NULL},
        {"nommx2", &x86_mmx2, CONF_TYPE_FLAG, 0, 1, 0, NULL},
        {"3dnow", &x86_3dnow, CONF_TYPE_FLAG, 0, 0, 1, NULL},
        {"no3dnow", &x86_3dnow, CONF_TYPE_FLAG, 0, 1, 0, NULL},
        {"3dnow2", &x86_3dnow2, CONF_TYPE_FLAG, 0, 0, 1, NULL},
        {"no3dnow2", &x86_3dnow2, CONF_TYPE_FLAG, 0, 1, 0, NULL},
        {"sse", &x86_sse, CONF_TYPE_FLAG, 0, 0, 1, NULL},
        {"nosse", &x86_sse, CONF_TYPE_FLAG, 0, 1, 0, NULL},
        {"sse2", &x86_sse2, CONF_TYPE_FLAG, 0, 0, 1, NULL},
        {"nosse2", &x86_sse2, CONF_TYPE_FLAG, 0, 1, 0, NULL},
#endif

#ifdef HAVE_AA
	{"aa*",	vo_aa_parseoption,  CONF_TYPE_FUNC_FULL, 0, 0, 0 , &vo_aa_revertoption},
#endif

#ifdef HAVE_ZR
	{"zr*", vo_zr_parseoption, CONF_TYPE_FUNC_FULL, 0, 0, 0, &vo_zr_revertoption },
#endif

#ifdef HAVE_LIRC
	{"lircconf", &lirc_configfile, CONF_TYPE_STRING, CONF_GLOBAL, 0, 0, NULL}, 
#endif

	{"alang", &audio_lang, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"slang", &dvdsub_lang, CONF_TYPE_STRING, 0, 0, 0, NULL},

	{"playlist", NULL, CONF_TYPE_STRING, 0, 0, 0, NULL},
	{"dapsync", &dapsync, CONF_TYPE_FLAG, 0, 0, 1, NULL},
	{"nodapsync", &dapsync, CONF_TYPE_FLAG, 0, 1, 0, NULL},
	{"softsleep", &softsleep, CONF_TYPE_FLAG, 0, 0, 1, NULL},

	{"slave", &slave_mode, CONF_TYPE_FLAG,CONF_GLOBAL , 0, 1, NULL},
	{"use-stdin", &use_stdin, CONF_TYPE_FLAG, CONF_GLOBAL, 0, 1, NULL},

#ifdef HAVE_X11
	{"wid", &WinID, CONF_TYPE_INT, 0, 0, 0, NULL},
	{"rootwin", &WinID, CONF_TYPE_FLAG, 0, -1, 0, NULL},
#endif

#ifdef HAVE_XINERAMA
	{"xineramascreen", &xinerama_screen, CONF_TYPE_INT, CONF_RANGE, 0, 32, NULL},
#endif

#ifdef HAVE_RTC
	{"nortc", &nortc, CONF_TYPE_FLAG, 0, 0, 1, NULL},
#endif
        {"shuffle",&shuffle_playback, CONF_TYPE_FLAG, 0, 0, 1, NULL},
        {"noshuffle",&shuffle_playback, CONF_TYPE_FLAG, 0, 1, 0, NULL},
#define MAIN_CONF
#include "cfg-common.h"
#undef MAIN_CONF
        
/*UD*/	{"verbose", &verbose, CONF_TYPE_INT, CONF_RANGE|CONF_GLOBAL, 0, 100, NULL},
	{"v", cfg_inc_verbose, CONF_TYPE_FUNC, CONF_GLOBAL|CONF_NOSAVE, 0, 0, NULL},
	{"-help", help_text, CONF_TYPE_PRINT, CONF_NOCFG|CONF_GLOBAL, 0, 0, NULL},
	{"help", help_text, CONF_TYPE_PRINT, CONF_NOCFG|CONF_GLOBAL, 0, 0, NULL},
	{"h", help_text, CONF_TYPE_PRINT, CONF_NOCFG|CONF_GLOBAL, 0, 0, NULL},
	{NULL, NULL, 0, 0, 0, 0, NULL}
};
