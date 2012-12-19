#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;

#include "mplayerxp.h"
#include "xmp_context.h"
#include "libmpconf/cfgparser.h"
#include "postproc/af.h"
#include "postproc/vf.h"
#include "libmpsub/spudec.h"
#include "libmpstream2/network.h"

namespace mpxp {
#ifdef HAVE_SDL
//extern char *sdl_driver;
extern int sdl_noxv;
extern int sdl_forcexv;
extern int sdl_forcegl;
//extern char *sdl_adriver;
#endif
extern const char *oss_mixer_device;

static const config_t xpcore_config[]={
	{"xp", &mp_conf.xp, CONF_TYPE_INT, CONF_RANGE, 0, UINT_MAX, "specifies number cpus to use for playback"},
	{"dump", &mp_conf.stream_dump, CONF_TYPE_STRING, 0, 0, 0, "specifies dump type and name for the dump of stream"},
	{"gomp", &mp_conf.gomp, CONF_TYPE_FLAG, 0, 0, 1, "enables usage of OpenMP extensions"},
	{"nogomp", &mp_conf.gomp, CONF_TYPE_FLAG, 0, 1, 0, "disables usage of OpenMP extensions"},
	{"xp_buffs", &vo_conf.xp_buffs, CONF_TYPE_INT, CONF_RANGE, 4, 1024, "specifies number of buffers for decoding-ahead in XP mode"},
	{"cache", &mp_conf.s_cache_size, CONF_TYPE_INT, CONF_RANGE, 4, 65536, "specifies amount of memory for precaching a file/URL"},
	{"nocache", &mp_conf.s_cache_size, CONF_TYPE_FLAG, 0, 1, 0, "disables precaching a file/URL"},
	{"autoq", &mp_conf.autoq, CONF_TYPE_INT, CONF_RANGE, 0, 100, "dynamically changes the level of postprocessing depending on spare CPU time available"},
	{"speed", &mp_conf.playbackspeed_factor, CONF_TYPE_FLOAT, CONF_RANGE, 0.01, 100.0, "sets playback speed factor"},
	{"benchmark", &mp_conf.benchmark, CONF_TYPE_FLAG, 0, 0, 1, "performs benchmarking to estimate performance of MPlayerXP"},
	{"test-av", &mp_conf.test_av, CONF_TYPE_FLAG, 0, 0, 1, "test antiviral protection of MPlayerXP"},
	{"malloc-debug", &mp_conf.malloc_debug, CONF_TYPE_INT, CONF_RANGE, 0, 3, "debugs malloc() calls in MPlayerXP"},
	{"max-trace", &mp_conf.max_trace, CONF_TYPE_INT, CONF_RANGE, 1, 1024, "maximal number of backtrace stack"},
	{NULL, NULL, 0, 0, 0, 0, NULL},
};

#ifdef HAVE_STREAMING
static const config_t net_config[]={
	{"ipv4", &net_conf.prefer_ipv4, CONF_TYPE_FLAG, 0, 0, 1, "forces mplayerxp to use IPv4 protocol over network"},
#ifdef HAVE_AF_INET6
	{"ipv6", &net_conf.prefer_ipv4, CONF_TYPE_FLAG, 0, 1, 0, "forces mplayerxp to use IPv6 protocol over network"},
#else
	{"ipv6", "MPlayerXP was compiled without IPv6 support.\n", CONF_TYPE_PRINT, 0, 0, 0, NULL},
#endif /* HAVE_AF_INET6 */
	{"ipv4-only-proxy", &net_conf.ipv4_only_proxy, CONF_TYPE_FLAG, 0, 0, 1, "skip the proxy for IPv6 addresses"},
	{"user", &net_conf.username, CONF_TYPE_STRING, 0, 0, 0, "specifies username for HTTP authentication"},
	{"passwd", &net_conf.password, CONF_TYPE_STRING, 0, 0, 0, "specifies password for HTTP authentication"},
	{"bandwidth", &net_conf.bandwidth, CONF_TYPE_INT, CONF_MIN, 0, 0, "specifies the maximum bandwidth for network streaming"},
	{"user-agent", &net_conf.useragent, CONF_TYPE_STRING, 0, 0, 0, "specifies string as user agent for HTTP streaming"},
	{"cookies", &net_conf.cookies_enabled, CONF_TYPE_FLAG, 0, 0, 1, "send cookies when making HTTP requests"},
	{"cookies-file", &net_conf.cookies_file, CONF_TYPE_STRING, 0, 0, 0, "Read HTTP cookies from file"},
	{NULL, NULL, 0, 0, 0, 0, NULL},
};
#endif

#if defined( ARCH_X86 ) || defined(ARCH_X86_64)
static const config_t cpu_config[]={
	{"simd", &mp_conf.x86.simd, CONF_TYPE_FLAG, 0, 0, 1, "enables using of SIMD extensions of CPU"},
	{"nosimd", &mp_conf.x86.simd, CONF_TYPE_FLAG, 0, 1, 0, "disables using of SIMD extensions of CPU"},
	{"mmx", &mp_conf.x86.mmx, CONF_TYPE_FLAG, 0, 0, 1, "enables using of MMX extensions of CPU"},
	{"nommx", &mp_conf.x86.mmx, CONF_TYPE_FLAG, 0, 1, 0, "disables using of MMX extensions of CPU"},
	{"mmx2", &mp_conf.x86.mmx2, CONF_TYPE_FLAG, 0, 0, 1, "enables using of MMX2 extensions of CPU"},
	{"nommx2", &mp_conf.x86.mmx2, CONF_TYPE_FLAG, 0, 1, 0, "disables using of MMX2 extensions of CPU"},
	{"3dnow", &mp_conf.x86._3dnow, CONF_TYPE_FLAG, 0, 0, 1, "enables using of 3DNow! extensions of CPU"},
	{"no3dnow", &mp_conf.x86._3dnow, CONF_TYPE_FLAG, 0, 1, 0, "disables using of 3DNow! extensions of CPU"},
	{"3dnow2", &mp_conf.x86._3dnow2, CONF_TYPE_FLAG, 0, 0, 1, "enables using of 3DNow-2! extensions of CPU"},
	{"no3dnow2", &mp_conf.x86._3dnow2, CONF_TYPE_FLAG, 0, 1, 0, "disables using of 3DNow-2! extensions of CPU"},
	{"sse", &mp_conf.x86.sse, CONF_TYPE_FLAG, 0, 0, 1, "enables using of SSE extensions of CPU"},
	{"nosse", &mp_conf.x86.sse, CONF_TYPE_FLAG, 0, 1, 0, "disables using of SSE extensions of CPU"},
	{"sse2", &mp_conf.x86.sse2, CONF_TYPE_FLAG, 0, 0, 1, "enables using of SSE2 extensions of CPU"},
	{"nosse2", &mp_conf.x86.sse2, CONF_TYPE_FLAG, 0, 1, 0, "disables using of SSE2 extensions of CPU"},
	{"sse3", &mp_conf.x86.sse3, CONF_TYPE_FLAG, 0, 0, 1, "enables using of SSE3 extensions of CPU"},
	{"nosse3", &mp_conf.x86.sse3, CONF_TYPE_FLAG, 0, 1, 0, "disables using of SSE3 extensions of CPU"},
	{"ssse3", &mp_conf.x86.ssse3, CONF_TYPE_FLAG, 0, 0, 1, "enables using of SSSE3 extensions of CPU"},
	{"nossse3", &mp_conf.x86.ssse3, CONF_TYPE_FLAG, 0, 1, 0, "disables using of SSSE3 extensions of CPU"},
	{"sse41", &mp_conf.x86.sse41, CONF_TYPE_FLAG, 0, 0, 1, "enables using of SSE41 extensions of CPU"},
	{"nosse41", &mp_conf.x86.sse41, CONF_TYPE_FLAG, 0, 1, 0, "disables using of SSE41 extensions of CPU"},
	{"sse42", &mp_conf.x86.sse42, CONF_TYPE_FLAG, 0, 0, 1, "enables using of SSE42 extensions of CPU"},
	{"nosse42", &mp_conf.x86.sse42, CONF_TYPE_FLAG, 0, 1, 0, "disables using of SSE42 extensions of CPU"},
	{"aes", &mp_conf.x86.aes, CONF_TYPE_FLAG, 0, 0, 1, "enables using of AES extensions of CPU"},
	{"noaes", &mp_conf.x86.aes, CONF_TYPE_FLAG, 0, 1, 0, "disables using of AES extensions of CPU"},
	{"avx", &mp_conf.x86.avx, CONF_TYPE_FLAG, 0, 0, 1, "enables using of AVX extensions of CPU"},
	{"noavx", &mp_conf.x86.avx, CONF_TYPE_FLAG, 0, 1, 0, "disables using of AVX extensions of CPU"},
	{"fma", &mp_conf.x86.fma, CONF_TYPE_FLAG, 0, 0, 1, "enables using of FMA extensions of CPU"},
	{"nofma", &mp_conf.x86.fma, CONF_TYPE_FLAG, 0, 1, 0, "disables using of FMA extensions of CPU"},
	{NULL, NULL, 0, 0, 0, 0, NULL},
};
#endif

static const config_t osd_config[]={
	{"level", &mp_conf.osd_level, CONF_TYPE_INT, CONF_RANGE, 0, 2 , "specifies initial mode of the OSD"},
#ifdef USE_OSD
	{"font", &mp_conf.font_name, CONF_TYPE_STRING, 0, 0, 0, "specifies an alternative directory of font.desc location"},
	{"ffactor", &mp_conf.font_factor, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 10.0, "specifies resampling of alphamap of the font"},
	{"spualign", &spu_alignment, CONF_TYPE_INT, CONF_RANGE, -1, 2, "specifies align position of SPU (DVD-VOBsub) subtitles"},
	{"spuaa", &spu_aamode, CONF_TYPE_INT, CONF_RANGE, 0, 31, "specifies antialiasing/scaling mode for SPU"},
	{"spugauss", &spu_gaussvar, CONF_TYPE_FLOAT, CONF_RANGE, 0.0, 3.0, "specifies variance parameter of gaussian for -spuaa"},
#endif
	{NULL, NULL, 0, 0, 0, 0, NULL},
};

static const config_t veq_config[]={
	{"brightness",&vo_conf.gamma.brightness, CONF_TYPE_INT, CONF_RANGE, -1000, 1000, "specifies brightness-level for output image"},
	{"saturation",&vo_conf.gamma.saturation, CONF_TYPE_INT, CONF_RANGE, -1000, 1000, "specifies saturation-level for output image"},
	{"contrast",&vo_conf.gamma.contrast, CONF_TYPE_INT, CONF_RANGE, -1000, 1000, "specifies contrast-level for output image"},
	{"hue",&vo_conf.gamma.hue, CONF_TYPE_INT, CONF_RANGE, -1000, 1000, "specifies hue of gamma-correction for output image"},
	{"red",&vo_conf.gamma.red_intensity, CONF_TYPE_INT, CONF_RANGE, -1000, 1000, "specifies intensity of red component for output image"},
	{"green",&vo_conf.gamma.green_intensity, CONF_TYPE_INT, CONF_RANGE, -1000, 1000, "specifies intensity of green component for output image"},
	{"blue",&vo_conf.gamma.blue_intensity, CONF_TYPE_INT, CONF_RANGE, -1000, 1000, "specifies intensity of blue component for output image"},
	{NULL, NULL, 0, 0, 0, 0, NULL},
};


static const config_t avsync_config[]={
	{"framedrop", &mp_conf.frame_dropping, CONF_TYPE_FLAG, 0, 0, 1, "enables frame-dropping on slow systems: decodes all video frames, but skips displaying some ones"},
/*UD*/	{"hardframedrop", &mp_conf.frame_dropping, CONF_TYPE_FLAG, 0, 0, 2, "enables hard frame-dropping on slow systems: skips displaying and decoding of some frames"},
	{"noframedrop", &mp_conf.frame_dropping, CONF_TYPE_FLAG, 0, 1, 0, "disables frame dropping"},
	{"pts", &mp_conf.av_sync_pts, CONF_TYPE_FLAG, 0, 0, 1, "use PTS-based method of A/V synchronization"},
	{"nopts", &mp_conf.av_sync_pts, CONF_TYPE_FLAG, 0, 1, 0, "use BPS-based method of A/V synchronization"},
	{"force_pts_fix", &mp_conf.av_force_pts_fix, CONF_TYPE_FLAG, 0, 0, 1, "force PTS fixing for \"bad\" files"},
	{"noforce_pts_fix", &mp_conf.av_force_pts_fix, CONF_TYPE_FLAG, 0, 1, 0, "disable PTS fixing for \"bad\" files"},
	{"force_pts_fix2", &mp_conf.av_force_pts_fix2, CONF_TYPE_FLAG, 0, 0, 1, "force PTS fixing for \"bad\" files without PTS changing"},
	{"noforce_pts_fix2", &mp_conf.av_force_pts_fix2, CONF_TYPE_FLAG, 0, 1, 0, "disable PTS fixing for \"bad\" files without PTS changing"},
	{"frame_reorder", &mp_conf.frame_reorder, CONF_TYPE_FLAG, 0, 0, 1, "recalc PTS of frames as they were added to the buffer"},
	{"noframe_reorder", &mp_conf.frame_reorder, CONF_TYPE_FLAG, 0, 1, 0, "keep original PTS of each frame"},
	{"softsleep", &mp_conf.softsleep, CONF_TYPE_FLAG, 0, 0, 1, "enables high quality software timers for A/V synchronization"},
#ifdef HAVE_RTC
	{"rtc", &mp_conf.nortc, CONF_TYPE_FLAG, 0, 1, 0, "enables using of /dev/rtc (real-time clock chip) to compute PTS"},
	{"nortc", &mp_conf.nortc, CONF_TYPE_FLAG, 0, 0, 1, "disables using of /dev/rtc (real-time clock chip) to compute PTS"},
#endif
	{"fps", &mp_conf.force_fps, CONF_TYPE_FLOAT, CONF_MIN, 0, 0, "forces frame rate (if value is wrong in the header)"},
	{"vsync", &vo_conf.vsync, CONF_TYPE_FLAG, 0, 0, 1, "forces video hardware to wait VSYNC signal before frame switching"},
	{"novsync", &vo_conf.vsync, CONF_TYPE_FLAG, 0, 1, 0, "disables video hardware to wait VSYNC signal before frame switching"},
	{NULL, NULL, 0, 0, 0, 0, NULL},
};

static const config_t subtitle_config[]={
	{"on", &mp_conf.has_dvdsub, CONF_TYPE_FLAG, 0, 0, 1, "enables subtitle-steam playback"},
	{"off", &mp_conf.has_dvdsub, CONF_TYPE_FLAG, 0, 1, 0, "disables subtitle-stream playback"},
	{"vob", &mp_conf.vobsub_name, CONF_TYPE_STRING, 0, 0, 0, "specifies the VobSub files that are to be used for subtitle"},
	{"vobid", &mp_conf.vobsub_id, CONF_TYPE_INT, CONF_RANGE, 0, 31, "specifies the VobSub subtitle id"},
#ifdef USE_SUB
	{"file", &mp_conf.sub_name, CONF_TYPE_STRING, 0, 0, 0, "specifies the subtitle file"},
#ifdef USE_ICONV
	{"cp", &sub_data.cp, CONF_TYPE_STRING, 0, 0, 0, "specifies codepage of subtitles"},
#endif
	{"fps", &mp_conf.sub_fps, CONF_TYPE_FLOAT, 0, 0.0, 10.0, "specifies frame/sec rate of subtitle file"},
	{"noauto", &mp_conf.sub_auto, CONF_TYPE_FLAG, 0, 1, 0, "disable autodetection of vobsub for textsubs if vobsub found"},
	{"unicode", &sub_data.unicode, CONF_TYPE_FLAG, 0, 0, 1, "tells MPlayerXP to handle the subtitle file as UNICODE"},
	{"nounicode", &sub_data.unicode, CONF_TYPE_FLAG, 0, 1, 0, "tells MPlayerXP to handle the subtitle file as non-UNICODE"},
	{"utf8", &sub_data.utf8, CONF_TYPE_FLAG, 0, 0, 1, "tells MPlayerXP to handle the subtitle file as UTF8"},
	{"noutf8", &sub_data.utf8, CONF_TYPE_FLAG, 0, 1, 0, "tells MPlayerXP to handle the subtitle file as non-UTF8"},
	{"pos",&sub_data.pos,  CONF_TYPE_INT, CONF_RANGE, 0, 100, "specifies vertical shift of subtitles"},
#endif
	{"cc", &mp_conf.subcc_enabled, CONF_TYPE_FLAG, 0, 0, 1, "enable DVD Closed Caption (CC) subtitles"},
	{"nocc", &mp_conf.subcc_enabled, CONF_TYPE_FLAG, 0, 1, 0, "disable DVD Closed Caption (CC) subtitles"},
	{"id", &mp_conf.dvdsub_id, CONF_TYPE_INT, CONF_RANGE, 0, 31, "selects subtitle channel"},
	{"lang", &mp_conf.dvdsub_lang, CONF_TYPE_STRING, 0, 0, 0, "specifies language of DVD-subtitle stream as two-letter country code(s)"},
	{"ifo", &mp_conf.spudec_ifo, CONF_TYPE_STRING, 0, 0, 0, "specifies .ifo file for DVD subtitles"},
	{NULL, NULL, 0, 0, 0, 0, NULL},
};

#ifdef HAVE_X11
static const config_t x11_config[]={
	{"display", &vo_conf.mDisplayName, CONF_TYPE_STRING, 0, 0, 0, "specifies the hostname and display number of the X server"},
#ifdef HAVE_XINERAMA
	{"xinerama", &vo_conf.xinerama_screen, CONF_TYPE_INT, CONF_RANGE, 0, 32, "tells MPlayerXP the display for movie playback"},
#endif
	{NULL, NULL, 0, 0, 0, 0, NULL},
};
#endif

static const config_t audio_config[]={
	{"on", &mp_conf.has_audio, CONF_TYPE_FLAG, 0, 0, 1, "enables audio-steam playback"},
	{"off", &mp_conf.has_audio, CONF_TYPE_FLAG, 0, 1, 0, "disables audio-stream playback"},
	{"mixer", &oss_mixer_device, CONF_TYPE_STRING, 0, 0, 0, "select audio-mixer device"},
	{"channels", &mp_conf.ao_channels, CONF_TYPE_INT, CONF_RANGE, 2, 8, "select number of audio output channels to be used"},
	{"rate", &mp_conf.force_srate, CONF_TYPE_INT, CONF_RANGE, 1000, 8*48000, "specifies Hz for audio playback"},
	{"lang", &mp_conf.audio_lang, CONF_TYPE_STRING, 0, 0, 0, "specifies language of DVD-audio stream as two-letter country code(s)"},
	{"id", &mp_conf.audio_id, CONF_TYPE_INT, CONF_RANGE, 0, 255, "selects audio channel"},
	{NULL, NULL, 0, 0, 0, 0, NULL},
};

static const config_t video_config[]={
	{"on", &mp_conf.has_video, CONF_TYPE_FLAG, 0, 0, 1, "enables video-steam playback"},
	{"off", &mp_conf.has_video, CONF_TYPE_FLAG, 0, 1, 0, "disables video-stream playback"},
	{"width", &vo_conf.image_width, CONF_TYPE_INT, CONF_RANGE, 0, 4096, "scale output image to width (if driver supports)"},
	{"height", &vo_conf.image_height, CONF_TYPE_INT, CONF_RANGE, 0, 4096, "scale output image to height (if driver supports)"},
	{"zoom", &vo_conf.image_zoom, CONF_TYPE_FLOAT, CONF_RANGE, 0, 4096, "scale output image by given factor"},
	{"aspect", &vo_conf.movie_aspect, CONF_TYPE_FLOAT, CONF_RANGE, 0.2, 3.0, "sets aspect-ratio of movies (autodetect)"},
	{"noaspect", &vo_conf.movie_aspect, CONF_TYPE_FLAG, 0, 0, 0, "unsets aspect-ratio of movies"},
	{"aspect-ratio", &vo_conf.softzoom, CONF_TYPE_FLAG, 0, 0, 1, "keeps aspect-ratio of the movie during window resize"},
	{"noaspect-ratio", &vo_conf.softzoom, CONF_TYPE_FLAG, 0, 1, 0, "render movie to the user-defined window's geometry"},
	{"monitorpixelaspect", &mp_conf.monitor_pixel_aspect, CONF_TYPE_FLOAT, CONF_RANGE, 0.2, 9.0, "sets the aspect-ratio of a single pixel of TV screen"},
	{"vm", &vo_conf.vidmode, CONF_TYPE_FLAG, 0, 0, 1, "enables video-mode changing during playback"},
	{"novm", &vo_conf.vidmode, CONF_TYPE_FLAG, 0, 1, 0, "disables video-mode changing during playback"},
	{"fs", &vo_conf.fullscreen, CONF_TYPE_FLAG, 0, 0, 1, "fullscreen playback"},
	{"nofs", &vo_conf.fullscreen, CONF_TYPE_FLAG, 0, 1, 0, "windowed playback"},
	{"fsmode", &vo_conf.fsmode, CONF_TYPE_INT, CONF_RANGE, 0, 15, "enables workaround for some fullscreen related problems"},
	{"flip", &vo_conf.flip, CONF_TYPE_FLAG, 0, -1, 1, "flip output image upside-down"},
	{"noflip", &vo_conf.flip, CONF_TYPE_FLAG, 0, -1, 0, "render output image as is"},
	{"bpp", &vo_conf.dbpp, CONF_TYPE_INT, CONF_RANGE, 0, 32, "use different color depth than autodetect"},
	{"bm", &vo_conf.use_bm, CONF_TYPE_FLAG, 0, 0, 1, "enables using of bus-mastering (if it available for given OS/videocard)"},
	{"bm2", &vo_conf.use_bm, CONF_TYPE_FLAG, 0, 0, 2, "enables using of bus-mastering to store all decoded-ahead frames in video-memory"},
	{"nobm", &vo_conf.use_bm, CONF_TYPE_FLAG, 0, 1, 0, "disables using of bus-mastering"},
	{"id", &mp_conf.video_id, CONF_TYPE_INT, CONF_RANGE, 0, 255, "selects video channel"},
	{"pp", &mp_conf.npp_options, CONF_TYPE_STRING, 0, 0, 0, "specifies options of post-processing"},
#ifdef HAVE_PNG
	{"z", &mp_conf.z_compression, CONF_TYPE_INT, CONF_RANGE, 0, 9, "specifies compression level for PNG output"},
#endif
#ifdef HAVE_SDL
	{"noxv", &sdl_noxv, CONF_TYPE_FLAG, 0, 0, 1, "disable XVideo hardware acceleration for SDL"},
	{"forcexv", &sdl_forcexv, CONF_TYPE_FLAG, 0, 0, 1, "force XVideo hardware acceleration for SDL"},
	{"forcegl", &sdl_forcegl, CONF_TYPE_FLAG, 0, 0, 1, "force OpenGL hardware acceleration for SDL"},
#endif
	{"eq",(any_t*)&veq_config, CONF_TYPE_SUBCONFIG, 0, 0, 0, "Video-equalizer specific options"},
	{NULL, NULL, 0, 0, 0, 0, NULL},
};

static const config_t playback_config[]={
	{"sb", &mp_conf.seek_to_byte, CONF_TYPE_INT, CONF_MIN, 0, 0, "seek to given byte position before playback"},
	{"ss", &mp_conf.seek_to_sec, CONF_TYPE_STRING, CONF_MIN, 0, 0, "seek to given time position before playback"},
	{"loop", &mp_conf.loop_times, CONF_TYPE_INT, CONF_RANGE, -1, 10000, "loops movie playback given number of times. 0 means forever"},
	{"noloop", &mp_conf.loop_times, CONF_TYPE_FLAG, 0, 0, -1, "disable loop of playback"},
	{"shuffle",&mp_conf.shuffle_playback, CONF_TYPE_FLAG, 0, 0, 1, "play files in random order"},
	{"noshuffle",&mp_conf.shuffle_playback, CONF_TYPE_FLAG, 0, 1, 0, "play files in regular order"},
	{"list", NULL, CONF_TYPE_STRING, 0, 0, 0, "specifies playlist (1 file/row or Winamp or ASX format)"},
	{"frames", &mp_conf.play_n_frames, CONF_TYPE_INT, CONF_MIN, 0, 0, "play given number of frames and exit"},
	{NULL, NULL, 0, 0, 0, 0, NULL},
};

static const config_t mplayer_options[]={
	/* name, pointer, type, flags, min, max, help */
	{"include", NULL, CONF_TYPE_INCLUDE, CONF_NOSAVE, 0, 0, ""}, /* this don't need anymore to be the first!!! */

//---------------------- libao/libvo/mplayer options ------------------------
	{"vo", &mp_conf.video_driver, CONF_TYPE_STRING, 0, 0, 0, "select video output driver and optinaly device"},
	{"ao", &mp_conf.audio_driver, CONF_TYPE_STRING, 0, 0, 0, "select audio output driver and optinaly device"},
	{"af", &af_cfg.list, CONF_TYPE_STRING, 0, 0, 0, "selects audio filter"},
	{"vf", &vf_cfg.list, CONF_TYPE_STRING, 0, 0, 0, "selects video filter"},
	{"afm", &mp_conf.audio_family, CONF_TYPE_STRING, 0, 0, 0, "forces usage of specified audio-decoders family"},
	{"vfm", &mp_conf.video_family, CONF_TYPE_STRING, 0, 0, 0, "forces usage of specified video-decoders family"},
	{"ac", &mp_conf.audio_codec, CONF_TYPE_STRING, 0, 0, 0, "forces usage of specified audio-decoder"},
	{"vc", &mp_conf.video_codec, CONF_TYPE_STRING, 0, 0, 0, "forces usage of specified video-decoder"},
/*UD*/	{"verbose", &mp_conf.verbose, CONF_TYPE_INT, CONF_RANGE|CONF_GLOBAL, 0, 100, "verbose output"},
	{"v", &mp_conf.verbose, CONF_TYPE_INC, 0, 0, 0, "verbose output (more -v means more verbosity)"},
	{"msgfilter", &mp_conf.msg_filter, CONF_TYPE_INT, CONF_RANGE, 0, 0xFFFFFFFF, "specifies filter for verbosed messages"},

	{"core", (any_t*)&xpcore_config, CONF_TYPE_SUBCONFIG, 0, 0, 0, "XP-core related options" },
	{"play", (any_t*)&playback_config, CONF_TYPE_SUBCONFIG, 0, 0, 0, "Playback specific options" },
	{"audio", (any_t*)&audio_config, CONF_TYPE_SUBCONFIG, 0, 0, 0, "Audio related options" },
	{"video", (any_t*)&video_config, CONF_TYPE_SUBCONFIG, 0, 0, 0, "Video related options" },
	{"sub", (any_t*)&subtitle_config, CONF_TYPE_SUBCONFIG, 0, 0, 0, "Subtitle related options" },
#ifdef HAVE_X11
	{"x", (any_t*)&x11_config, CONF_TYPE_SUBCONFIG, 0, 0, 0, "X11-specific options" },
#endif
	{"osd", (any_t*)&osd_config, CONF_TYPE_SUBCONFIG, 0, 0, 0, "OSD-related options"},
	{"sync", (any_t*)&avsync_config, CONF_TYPE_SUBCONFIG, 0, 0, 0, "AV-synchronization related options" },
#if defined( ARCH_X86 ) || defined(ARCH_X86_64)
	{"cpu", (any_t*)&cpu_config, CONF_TYPE_SUBCONFIG, 0, 0, 0, "CPU specific options" },
#endif
// ------------------------- stream options --------------------
#ifdef HAVE_STREAMING
	{ "net", (any_t*)&net_config, CONF_TYPE_SUBCONFIG, 0, 0, 0, "Network specific options" },
#endif
// ------------------------- codec/pp options --------------------
	{NULL, NULL, 0, 0, 0, 0, NULL}
};
const config_t* mplayer_opts=mplayer_options;

} // namespace mpxp
extern void libmpcodecs_ad_register_options(m_config_t& cfg);
extern void libmpcodecs_vd_register_options(m_config_t& cfg);

namespace mpxp {
#ifdef HAVE_LIBCDIO
extern void cdda_register_options(m_config_t& cfg);
#endif
extern void mp_input_register_options(m_config_t& cfg);
extern void libmpdemux_register_options(m_config_t& cfg);
extern void demuxer_register_options(m_config_t& cfg);
void mp_register_options(m_config_t& cfg)
{
    mp_input_register_options(cfg);
    libmpdemux_register_options(cfg);
    demuxer_register_options(cfg);
#ifdef HAVE_LIBCDIO
    cdda_register_options(cfg);
#endif
    libmpcodecs_ad_register_options(cfg);
    libmpcodecs_vd_register_options(cfg);
}

} // namespace mpxp
