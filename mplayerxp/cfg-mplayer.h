/*
 * config for cfgparser
 */

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

extern int subcc_enabled;

#ifdef USE_OSD
extern int osd_level;
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

#if defined( ARCH_X86 ) || defined(ARCH_X86_64)
extern int x86_simd;
extern int x86_mmx;
extern int x86_mmx2;
extern int x86_3dnow;
extern int x86_3dnow2;
extern int x86_sse;
extern int x86_sse2;
extern int x86_sse3;
extern int x86_ssse3;
extern int x86_sse41;
extern int x86_sse42;
extern int x86_aes;
extern int x86_avx;
extern int x86_fma;
#endif

extern float playbackspeed_factor;
/* from libvo/aspect.c */
extern float monitor_pixel_aspect;

/* from dec_audio, currently used for ac3surround decoder only */
extern int audio_output_channels;

extern int sws_flags;
extern int readPPOpt(any_t*conf, char *arg);
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

extern af_cfg_t af_cfg; // Configuration for audio filters
extern vf_cfg_t vf_cfg; // Configuration for audio filters

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

static const config_t xpcore_config[]={
	{"xp", &enable_xp, CONF_TYPE_INT, CONF_RANGE, 0, 4, NULL, "starts MPlayerXP in multi-thread and multi-buffer XP mode"},
	{"dump", &stream_dump, CONF_TYPE_STRING, 0, 0, 0, NULL, "specifies dump type and name for the dump of stream"},
	{"gomp", &enable_gomp, CONF_TYPE_FLAG, 0, 0, 1, NULL, "enables usage of OpenMP extensions"},
	{"nogomp", &enable_gomp, CONF_TYPE_FLAG, 0, 1, 0, NULL, "disables usage of OpenMP extensions"},
	{"da_buffs", &vo.da_buffs, CONF_TYPE_INT, CONF_RANGE, 4, 1024, NULL, "specifies number of buffers for decoding-ahead in XP mode"},
	{"cache", &stream_cache_size, CONF_TYPE_INT, CONF_RANGE, 4, 65536, NULL,"specifies amount of memory for precaching a file/URL"},
	{"nocache", &stream_cache_size, CONF_TYPE_FLAG, 0, 1, 0, NULL,"disables precaching a file/URL"},
	{"autoq", &auto_quality, CONF_TYPE_INT, CONF_RANGE, 0, 100, NULL, "dynamically changes the level of postprocessing depending on spare CPU time available"},
	{"benchmark", &benchmark, CONF_TYPE_FLAG, 0, 0, 1, NULL, "performs benchmarking to estimate performance of MPlayerXP"},
	{NULL, NULL, 0, 0, 0, 0, NULL,NULL},
};

#ifdef HAVE_STREAMING
static const config_t net_config[]={
	{"ipv4", &network_prefer_ipv4, CONF_TYPE_FLAG, 0, 0, 1, NULL, "forces mplayerxp to use IPv4 protocol over network"},
#ifdef HAVE_AF_INET6
	{"ipv6", &network_prefer_ipv4, CONF_TYPE_FLAG, 0, 1, 0, NULL, "forces mplayerxp to use IPv6 protocol over network"},
#else
	{"ipv6", "MPlayerXP was compiled without IPv6 support.\n", CONF_TYPE_PRINT, 0, 0, 0, NULL},
#endif /* HAVE_AF_INET6 */
	{"ipv4-only-proxy", &network_ipv4_only_proxy, CONF_TYPE_FLAG, 0, 0, 1, NULL, "skip the proxy for IPv6 addresses"},
	{"user", &network_username, CONF_TYPE_STRING, 0, 0, 0, NULL, "specifies username for HTTP authentication"},
	{"passwd", &network_password, CONF_TYPE_STRING, 0, 0, 0, NULL, "specifies password for HTTP authentication"},
	{"bandwidth", &network_bandwidth, CONF_TYPE_INT, CONF_MIN, 0, 0, NULL, "specifies the maximum bandwidth for network streaming"},
	{"user-agent", &network_useragent, CONF_TYPE_STRING, 0, 0, 0, NULL, "specifies string as user agent for HTTP streaming"},
	{"cookies", &network_cookies_enabled, CONF_TYPE_FLAG, 0, 0, 1, NULL,"send cookies when making HTTP requests"},
	{"cookies-file", &cookies_file, CONF_TYPE_STRING, 0, 0, 0, NULL,"Read HTTP cookies from file"},
	{NULL, NULL, 0, 0, 0, 0, NULL,NULL},
};
#endif

#if defined( ARCH_X86 ) || defined(ARCH_X86_64)
static const config_t cpu_config[]={
	{"simd", &x86_simd, CONF_TYPE_FLAG, 0, 0, 1, NULL, "enables using of SIMD extensions of CPU"},
	{"nosimd", &x86_simd, CONF_TYPE_FLAG, 0, 1, 0, NULL, "disables using of SIMD extensions of CPU"},
	{"mmx", &x86_mmx, CONF_TYPE_FLAG, 0, 0, 1, NULL, "enables using of MMX extensions of CPU"},
	{"nommx", &x86_mmx, CONF_TYPE_FLAG, 0, 1, 0, NULL, "disables using of MMX extensions of CPU"},
	{"mmx2", &x86_mmx2, CONF_TYPE_FLAG, 0, 0, 1, NULL, "enables using of MMX2 extensions of CPU"},
	{"nommx2", &x86_mmx2, CONF_TYPE_FLAG, 0, 1, 0, NULL, "disables using of MMX2 extensions of CPU"},
	{"3dnow", &x86_3dnow, CONF_TYPE_FLAG, 0, 0, 1, NULL, "enables using of 3DNow! extensions of CPU"},
	{"no3dnow", &x86_3dnow, CONF_TYPE_FLAG, 0, 1, 0, NULL, "disables using of 3DNow! extensions of CPU"},
	{"3dnow2", &x86_3dnow2, CONF_TYPE_FLAG, 0, 0, 1, NULL, "enables using of 3DNow-2! extensions of CPU"},
	{"no3dnow2", &x86_3dnow2, CONF_TYPE_FLAG, 0, 1, 0, NULL, "disables using of 3DNow-2! extensions of CPU"},
	{"sse", &x86_sse, CONF_TYPE_FLAG, 0, 0, 1, NULL, "enables using of SSE extensions of CPU"},
	{"nosse", &x86_sse, CONF_TYPE_FLAG, 0, 1, 0, NULL, "disables using of SSE extensions of CPU"},
	{"sse2", &x86_sse2, CONF_TYPE_FLAG, 0, 0, 1, NULL, "enables using of SSE2 extensions of CPU"},
	{"nosse2", &x86_sse2, CONF_TYPE_FLAG, 0, 1, 0, NULL, "disables using of SSE2 extensions of CPU"},
	{"sse3", &x86_sse3, CONF_TYPE_FLAG, 0, 0, 1, NULL, "enables using of SSE3 extensions of CPU"},
	{"nosse3", &x86_sse3, CONF_TYPE_FLAG, 0, 1, 0, NULL, "disables using of SSE3 extensions of CPU"},
	{"ssse3", &x86_ssse3, CONF_TYPE_FLAG, 0, 0, 1, NULL, "enables using of SSSE3 extensions of CPU"},
	{"nossse3", &x86_ssse3, CONF_TYPE_FLAG, 0, 1, 0, NULL, "disables using of SSSE3 extensions of CPU"},
	{"sse41", &x86_sse41, CONF_TYPE_FLAG, 0, 0, 1, NULL, "enables using of SSE41 extensions of CPU"},
	{"nosse41", &x86_sse41, CONF_TYPE_FLAG, 0, 1, 0, NULL, "disables using of SSE41 extensions of CPU"},
	{"sse42", &x86_sse42, CONF_TYPE_FLAG, 0, 0, 1, NULL, "enables using of SSE42 extensions of CPU"},
	{"nosse42", &x86_sse42, CONF_TYPE_FLAG, 0, 1, 0, NULL, "disables using of SSE42 extensions of CPU"},
	{"aes", &x86_aes, CONF_TYPE_FLAG, 0, 0, 1, NULL, "enables using of AES extensions of CPU"},
	{"noaes", &x86_aes, CONF_TYPE_FLAG, 0, 1, 0, NULL, "disables using of AES extensions of CPU"},
	{"avx", &x86_avx, CONF_TYPE_FLAG, 0, 0, 1, NULL, "enables using of AVX extensions of CPU"},
	{"noavx", &x86_avx, CONF_TYPE_FLAG, 0, 1, 0, NULL, "disables using of AVX extensions of CPU"},
	{"fma", &x86_fma, CONF_TYPE_FLAG, 0, 0, 1, NULL, "enables using of FMA extensions of CPU"},
	{"nofma", &x86_fma, CONF_TYPE_FLAG, 0, 1, 0, NULL, "disables using of FMA extensions of CPU"},
	{NULL, NULL, 0, 0, 0, 0, NULL,NULL},
};
#endif

static const config_t osd_config[]={
	{"level", &osd_level, CONF_TYPE_INT, CONF_RANGE, 0, 2 , NULL, "specifies initial mode of the OSD"},
#ifdef USE_OSD
	{"font", &font_name, CONF_TYPE_STRING, 0, 0, 0, NULL, "specifies an alternative directory of font.desc location"},
	{"ffactor", &font_factor, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 10.0, NULL, "specifies resampling of alphamap of the font"},
	{"spualign", &spu_alignment, CONF_TYPE_INT, CONF_RANGE, -1, 2, NULL, "specifies align position of SPU (DVD-VOBsub) subtitles"},
	{"spuaa", &spu_aamode, CONF_TYPE_INT, CONF_RANGE, 0, 31, NULL, "specifies antialiasing/scaling mode for SPU"},
	{"spugauss", &spu_gaussvar, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 3.0, NULL, "specifies variance parameter of gaussian for -spuaa"},
#endif
	{NULL, NULL, 0, 0, 0, 0, NULL,NULL},
};

static const config_t veq_config[]={
	{"brightness",&vo.gamma.brightness, CONF_TYPE_INT, CONF_RANGE, -1000, 1000, NULL, "specifies brightness-level for output image"},
	{"saturation",&vo.gamma.saturation, CONF_TYPE_INT, CONF_RANGE, -1000, 1000, NULL, "specifies saturation-level for output image"},
	{"contrast",&vo.gamma.contrast, CONF_TYPE_INT, CONF_RANGE, -1000, 1000, NULL, "specifies contrast-level for output image"},
	{"hue",&vo.gamma.hue, CONF_TYPE_INT, CONF_RANGE, -1000, 1000, NULL, "specifies hue of gamma-correction for output image"},
	{"red",&vo.gamma.red_intensity, CONF_TYPE_INT, CONF_RANGE, -1000, 1000, NULL, "specifies intensity of red component for output image"},
	{"green",&vo.gamma.green_intensity, CONF_TYPE_INT, CONF_RANGE, -1000, 1000, NULL, "specifies intensity of green component for output image"},
	{"blue",&vo.gamma.blue_intensity, CONF_TYPE_INT, CONF_RANGE, -1000, 1000, NULL, "specifies intensity of blue component for output image"},
	{NULL, NULL, 0, 0, 0, 0, NULL,NULL},
};


static const config_t avsync_config[]={
	{"framedrop", &frame_dropping, CONF_TYPE_FLAG, 0, 0, 1, NULL, "enables frame-dropping on slow systems: decodes all video frames, but skips displaying some ones"},
/*UD*/	{"hardframedrop", &frame_dropping, CONF_TYPE_FLAG, 0, 0, 2, NULL, "enables hard frame-dropping on slow systems: skips displaying and decoding of some frames"},
	{"noframedrop", &frame_dropping, CONF_TYPE_FLAG, 0, 1, 0, NULL, "disables frame dropping"},
	{"pts", &av_sync_pts, CONF_TYPE_FLAG, 0, 0, 1, NULL, "use PTS-based method of A/V synchronization"},
	{"nopts", &av_sync_pts, CONF_TYPE_FLAG, 0, 1, 0, NULL, "use BPS-based method of A/V synchronization"},
	{"dap", &dapsync, CONF_TYPE_FLAG, 0, 0, 1, NULL, "use alternative method of A/V synchronization"},
	{"nodap", &dapsync, CONF_TYPE_FLAG, 0, 1, 0, NULL, "disable alternative method of A/V synchronization"},
	{"force_pts_fix", &av_force_pts_fix, CONF_TYPE_FLAG, 0, 0, 1, NULL, "force PTS fixing for \"bad\" files"},
	{"noforce_pts_fix", &av_force_pts_fix, CONF_TYPE_FLAG, 0, 1, 0, NULL, "disable PTS fixing for \"bad\" files"},
	{"force_pts_fix2", &av_force_pts_fix2, CONF_TYPE_FLAG, 0, 0, 1, NULL, "force PTS fixing for \"bad\" files without PTS changing"},
	{"noforce_pts_fix2", &av_force_pts_fix2, CONF_TYPE_FLAG, 0, 1, 0, NULL, "disable PTS fixing for \"bad\" files without PTS changing"},
	{"frame_reorder", &frame_reorder, CONF_TYPE_FLAG, 0, 0, 1, NULL, "recalc PTS of frames as they were added to the buffer"},
	{"noframe_reorder", &frame_reorder, CONF_TYPE_FLAG, 0, 1, 0, NULL, "keep original PTS of each frame"},
	{"softsleep", &softsleep, CONF_TYPE_FLAG, 0, 0, 1, NULL, "enables high quality software timers for A/V synchronization"},
#ifdef HAVE_RTC
	{"rtc", &nortc, CONF_TYPE_FLAG, 0, 1, 0, NULL, "enables using of /dev/rtc (real-time clock chip) to compute PTS"},
	{"nortc", &nortc, CONF_TYPE_FLAG, 0, 0, 1, NULL, "disables using of /dev/rtc (real-time clock chip) to compute PTS"},
#endif
	{"mc", &default_max_pts_correction, CONF_TYPE_FLOAT, CONF_RANGE, 0, 10, NULL, "maximum sync correction per 5 frames (in seconds)"},
	{"fps", &force_fps, CONF_TYPE_FLOAT, CONF_MIN, 0, 0, NULL, "forces frame rate (if value is wrong in the header)"},
	{"vsync", &vo.vsync, CONF_TYPE_FLAG, 0, 0, 1, NULL, "forces video hardware to wait VSYNC signal before frame switching"},
	{"novsync", &vo.vsync, CONF_TYPE_FLAG, 0, 1, 0, NULL, "disables video hardware to wait VSYNC signal before frame switching"},
	{NULL, NULL, 0, 0, 0, 0, NULL,NULL},
};


static const config_t subtitle_config[]={
	{"on", &has_dvdsub, CONF_TYPE_FLAG, 0, 0, 1, NULL, "enables subtitle-steam playback"},
	{"off", &has_dvdsub, CONF_TYPE_FLAG, 0, 1, 0, NULL, "disables subtitle-stream playback"},
	{"vob", &vobsub_name, CONF_TYPE_STRING, 0, 0, 0, NULL, "specifies the VobSub files that are to be used for subtitle"},
	{"vobid", &vobsub_id, CONF_TYPE_INT, CONF_RANGE, 0, 31, NULL, "specifies the VobSub subtitle id"},
#ifdef USE_SUB
	{"file", &sub_name, CONF_TYPE_STRING, 0, 0, 0, NULL, "specifies the subtitle file"},
#ifdef USE_ICONV
	{"cp", &sub_data.cp, CONF_TYPE_STRING, 0, 0, 0, NULL, "specifies codepage of subtitles"},
#endif	
	{"delay", &sub_delay, CONF_TYPE_FLOAT, 0, 0.0, 10.0, NULL, "delays subtitles by given number of seconds"},
	{"fps", &sub_fps, CONF_TYPE_FLOAT, 0, 0.0, 10.0, NULL, "specifies frame/sec rate of subtitle file"},
        {"noauto", &sub_auto, CONF_TYPE_FLAG, 0, 1, 0, NULL, "disable autodetection of vobsub for textsubs if vobsub found"},
	{"unicode", &sub_data.unicode, CONF_TYPE_FLAG, 0, 0, 1, NULL, "tells MPlayerXP to handle the subtitle file as UNICODE"},
	{"nounicode", &sub_data.unicode, CONF_TYPE_FLAG, 0, 1, 0, NULL, "tells MPlayerXP to handle the subtitle file as non-UNICODE"},
	{"utf8", &sub_data.utf8, CONF_TYPE_FLAG, 0, 0, 1, NULL, "tells MPlayerXP to handle the subtitle file as UTF8"},
	{"noutf8", &sub_data.utf8, CONF_TYPE_FLAG, 0, 1, 0, NULL, "tells MPlayerXP to handle the subtitle file as non-UTF8"},
	{"pos",&sub_data.pos,  CONF_TYPE_INT, CONF_RANGE, 0, 100, NULL, "specifies vertical shift of subtitles"},
#endif
	{"cc", &subcc_enabled, CONF_TYPE_FLAG, 0, 0, 1, NULL, "enable DVD Closed Caption (CC) subtitles"},
	{"nocc", &subcc_enabled, CONF_TYPE_FLAG, 0, 1, 0, NULL, "disable DVD Closed Caption (CC) subtitles"},
	{"id", &dvdsub_id, CONF_TYPE_INT, CONF_RANGE, 0, 31, NULL, "selects subtitle channel"},
	{"lang", &dvdsub_lang, CONF_TYPE_STRING, 0, 0, 0, NULL, "specifies language of DVD-subtitle stream as two-letter country code(s)"},
	{"ifo", &spudec_ifo, CONF_TYPE_STRING, 0, 0, 0, NULL, "specifies .ifo file for DVD subtitles"},
	{NULL, NULL, 0, 0, 0, 0, NULL,NULL},
};

#ifdef HAVE_X11
static const config_t x11_config[]={
	{"display", &vo.mDisplayName, CONF_TYPE_STRING, 0, 0, 0, NULL, "specifies the hostname and display number of the X server"},
	{"wid", &vo.WinID, CONF_TYPE_INT, 0, 0, 0, NULL, "tells MPlayerXP to use existing X11 window (for using MPlayerXP as plugin)"},
	{"rootwin", &vo.WinID, CONF_TYPE_FLAG, 0, -1, 0, NULL, "render movie in the root window (desktop background)"},
#ifdef HAVE_XINERAMA
	{"xinerama", &xinerama_screen, CONF_TYPE_INT, CONF_RANGE, 0, 32, NULL, "tells MPlayerXP the display for movie playback"},
#endif
	{NULL, NULL, 0, 0, 0, 0, NULL,NULL},
};
#endif

static const config_t audio_config[]={
	{"on", &has_audio, CONF_TYPE_FLAG, 0, 0, 1, NULL, "enables audio-steam playback"},
	{"off", &has_audio, CONF_TYPE_FLAG, 0, 1, 0, NULL, "disables audio-stream playback"},
	{"mixer", &oss_mixer_device, CONF_TYPE_STRING, 0, 0, 0, NULL, "select audio-mixer device"},
	{"channels", &audio_output_channels, CONF_TYPE_INT, CONF_RANGE, 2, 8, NULL, "select number of audio output channels to be used"},
	{"rate", &force_srate, CONF_TYPE_INT, CONF_RANGE, 1000, 8*48000, NULL, "specifies Hz for audio playback"},
	{"lang", &audio_lang, CONF_TYPE_STRING, 0, 0, 0, NULL, "specifies language of DVD-audio stream as two-letter country code(s)"},
	{"id", &audio_id, CONF_TYPE_INT, CONF_RANGE, 0, 255, NULL, "selects audio channel"},
#ifdef USE_FAKE_MONO
	{"stereo", &fakemono, CONF_TYPE_INT, CONF_RANGE, 0, 2, NULL, "selects type of MP2/MP3 stereo output"},
#endif
	{"bs", &ao_data.buffersize, CONF_TYPE_INT, CONF_MIN, 0, 0, NULL, "specifies sound card audio buffer size in bytes. Default: measuring"},
	{NULL, NULL, 0, 0, 0, 0, NULL,NULL},
};

static const config_t video_config[]={
	{"on", &has_video, CONF_TYPE_FLAG, 0, 0, 1, NULL, "enables video-steam playback"},
	{"off", &has_video, CONF_TYPE_FLAG, 0, 1, 0, NULL, "disables video-stream playback"},
	{"width", &vo.opt_screen_size_x, CONF_TYPE_INT, CONF_RANGE, 0, 4096, NULL, "scale output image to width (if driver supports)"},
	{"height", &vo.opt_screen_size_y, CONF_TYPE_INT, CONF_RANGE, 0, 4096, NULL, "scale output image to height (if driver supports)"},
	{"zoom", &vo.screen_size_xy, CONF_TYPE_FLOAT, CONF_RANGE, 0, 4096, NULL, "scale output image by given factor"},
	{"screenw", &vo.screenwidth, CONF_TYPE_INT, CONF_RANGE, 0, 4096, NULL, "specifies the horizontal resolution of the screen (if supported)"},
	{"screenh", &vo.screenheight, CONF_TYPE_INT, CONF_RANGE, 0, 4096, NULL, "specifies the vertical resolution of the screen (if supported)"},
	{"speed", &playbackspeed_factor, CONF_TYPE_FLOAT, CONF_RANGE, 0.01, 100.0, NULL, "sets playback speed factor"},
	{"aspect", &vo.movie_aspect, CONF_TYPE_FLOAT, CONF_RANGE, 0.2, 3.0, NULL, "sets aspect-ratio of movies (autodetect)"},
	{"noaspect", &vo.movie_aspect, CONF_TYPE_FLAG, 0, 0, 0, NULL, "unsets aspect-ratio of movies"},
	{"aspect-ratio", &vo.softzoom, CONF_TYPE_FLAG, 0, 0, 1, NULL, "keeps aspect-ratio of the movie during window resize"},
	{"noaspect-ratio", &vo.softzoom, CONF_TYPE_FLAG, 0, 1, 0, NULL, "render movie to the user-defined window's geometry"},
	{"monitorpixelaspect", &monitor_pixel_aspect, CONF_TYPE_FLOAT, CONF_RANGE, 0.2, 9.0, NULL, "sets the aspect-ratio of a single pixel of TV screen"},
	{"vm", &vo.vidmode, CONF_TYPE_FLAG, 0, 0, 1, NULL, "enables video-mode changing during playback"},
	{"novm", &vo.vidmode, CONF_TYPE_FLAG, 0, 1, 0, NULL, "disables video-mode changing during playback"},
	{"fs", &vo.fullscreen, CONF_TYPE_FLAG, 0, 0, 1, NULL, "fullscreen playback"},
	{"nofs", &vo.fullscreen, CONF_TYPE_FLAG, 0, 1, 0, NULL, "windowed playback"},
	{"fsmode", &vo.fsmode, CONF_TYPE_INT, CONF_RANGE, 0, 15, NULL, "enables workaround for some fullscreen related problems"},
	{"flip", &vo.flip, CONF_TYPE_FLAG, 0, -1, 1, NULL, "flip output image upside-down"},
	{"noflip", &vo.flip, CONF_TYPE_FLAG, 0, -1, 0, NULL, "render output image as is"},
	{"bpp", &vo.dbpp, CONF_TYPE_INT, CONF_RANGE, 0, 32, NULL, "use different color depth than autodetect"},
	{"bm", &vo.use_bm, CONF_TYPE_FLAG, 0, 0, 1, NULL, "enables using of bus-mastering (if it available for given OS/videocard)"},
	{"bm2", &vo.use_bm, CONF_TYPE_FLAG, 0, 0, 2, NULL, "enables using of bus-mastering to store all decoded-ahead frames in video-memory"},
	{"nobm", &vo.use_bm, CONF_TYPE_FLAG, 0, 1, 0, NULL, "disables using of bus-mastering"},
	{"id", &video_id, CONF_TYPE_INT, CONF_RANGE, 0, 255, NULL, "selects video channel"},
	{"pp", &npp_options, CONF_TYPE_STRING, 0, 0, 0, NULL, "specifies options of post-processing"},
	{"sws", &sws_flags, CONF_TYPE_INT, 0, 0, 2, NULL, "specifies the quality of the software scaler"},
#ifdef HAVE_PNG
	{"z", &z_compression, CONF_TYPE_INT, CONF_RANGE, 0, 9, NULL, "specifies compression level for PNG output"},
#endif
#ifdef HAVE_SDL
	{"noxv", &sdl_noxv, CONF_TYPE_FLAG, 0, 0, 1, NULL, "disable XVideo hardware acceleration for SDL"},
	{"forcexv", &sdl_forcexv, CONF_TYPE_FLAG, 0, 0, 1, NULL, "force XVideo hardware acceleration for SDL"},
	{"forcegl", &sdl_forcegl, CONF_TYPE_FLAG, 0, 0, 1, NULL, "force OpenGL hardware acceleration for SDL"},
#endif
	{"eq",&veq_config, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL, "Video-equalizer specific options"},
	{NULL, NULL, 0, 0, 0, 0, NULL,NULL},
};

static const config_t playback_config[]={
	{"sb", &seek_to_byte, CONF_TYPE_INT, CONF_MIN, 0, 0, NULL, "seek to given byte position before playback"},
	{"ss", &seek_to_sec, CONF_TYPE_STRING, CONF_MIN, 0, 0, NULL, "seek to given time position before playback"},
	{"loop", &loop_times, CONF_TYPE_INT, CONF_RANGE, -1, 10000, NULL, "loops movie playback given number of times. 0 means forever"},
	{"noloop", &loop_times, CONF_TYPE_FLAG, 0, 0, -1, NULL, "disable loop of playback"},
	{"shuffle",&shuffle_playback, CONF_TYPE_FLAG, 0, 0, 1, NULL, "play files in random order"},
	{"noshuffle",&shuffle_playback, CONF_TYPE_FLAG, 0, 1, 0, NULL, "play files in regular order"},
	{"list", NULL, CONF_TYPE_STRING, 0, 0, 0, NULL, "specifies playlist (1 file/row or Winamp or ASX format)"},
	{"frames", &play_n_frames, CONF_TYPE_INT, CONF_MIN, 0, 0, NULL, "play given number of frames and exit"},
	{NULL, NULL, 0, 0, 0, 0, NULL,NULL},
};


static const config_t mplayer_opts[]={
	/* name, pointer, type, flags, min, max, help */
	{"include", cfg_include, CONF_TYPE_FUNC_PARAM, CONF_NOSAVE, 0, 0, NULL, ""}, /* this don't need anymore to be the first!!! */

//---------------------- libao/libvo/mplayer options ------------------------
	{"vo", &video_driver, CONF_TYPE_STRING, 0, 0, 0, NULL, "select video output driver and optinaly device"},
	{"ao", &audio_driver, CONF_TYPE_STRING, 0, 0, 0, NULL, "select audio output driver and optinaly device"},
	{"af", &af_cfg.list, CONF_TYPE_STRING, 0, 0, 0, NULL, "selects audio filter"},
	{"vf", &vf_cfg.list, CONF_TYPE_STRING, 0, 0, 0, NULL, "selects video filter"},
	{"afm", &audio_family, CONF_TYPE_STRING, 0, 0, 0, NULL, "forces usage of specified audio-decoders family"},
	{"vfm", &video_family, CONF_TYPE_STRING, 0, 0, 0, NULL, "forces usage of specified video-decoders family"},
	{"ac", &audio_codec, CONF_TYPE_STRING, 0, 0, 0, NULL, "forces usage of specified audio-decoder"},
	{"vc", &video_codec, CONF_TYPE_STRING, 0, 0, 0, NULL, "forces usage of specified video-decoder"},
/*UD*/	{"verbose", &verbose, CONF_TYPE_INT, CONF_RANGE|CONF_GLOBAL, 0, 100, NULL, "verbose output"},
	{"v", cfg_inc_verbose, CONF_TYPE_FUNC, CONF_GLOBAL|CONF_NOSAVE, 0, 0, NULL, "verbose output (more -v means more verbosity)"},
	{"slave", &slave_mode, CONF_TYPE_FLAG,CONF_GLOBAL , 0, 1, NULL, "turns MPlayerXP into slave mode as a backend for other programs"},
	{"use-stdin", &use_stdin, CONF_TYPE_FLAG, CONF_GLOBAL, 0, 1, NULL, "forces reading of keyboard codes from STDIN instead of terminal's console"},
	{"msgfilter", &mp_msg_filter, CONF_TYPE_INT, CONF_RANGE, 0, 0xFFFFFFFF, NULL, "specifies filter for verbosed messages"},
#ifdef HAVE_LIBCSS
	{"dvdauth", &dvd_auth_device, CONF_TYPE_STRING, 0, 0, 0, NULL,"provides authentification of encrypted DVD disk"},
	{"dvdkey", &dvdimportkey, CONF_TYPE_STRING, 0, 0, 0, NULL,"specifies key to decrypt stream encrypted with CSS"},
	{"csslib", &css_so, CONF_TYPE_STRING, 0, 0, 0, NULL,"specifies path to libcss.so"},
#endif

	{"core", &xpcore_config, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL, "XP-core related options" },
	{"play", &playback_config, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL, "Playback specific options" },
	{"audio", &audio_config, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL, "Audio related options" },
	{"video", &video_config, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL, "Video related options" },
	{"sub", &subtitle_config, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL, "Subtitle related options" },
#ifdef HAVE_X11
	{"x", &x11_config, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL, "X11-specific options" },
#endif
	{"osd", &osd_config, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL, "OSD-related options"},
	{"sync", &avsync_config, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL, "AV-synchronization related options" },
#if defined( ARCH_X86 ) || defined(ARCH_X86_64)
	{"cpu", &cpu_config, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL, "CPU specific options" },
#endif
// ------------------------- stream options --------------------
#ifdef HAVE_STREAMING
	{ "net", &net_config, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL, "Network specific options" },
#endif
// ------------------------- codec/pp options --------------------
	{NULL, NULL, 0, 0, 0, 0, NULL, NULL}
};
