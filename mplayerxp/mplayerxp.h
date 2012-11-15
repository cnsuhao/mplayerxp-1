#ifndef __MPLAYERXP_MAIN
#define __MPLAYERXP_MAIN 1

#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include "mp_config.h"
#include "osdep/mplib.h"
#include "xmpcore/xmp_enums.h"
#include "libmpconf/cfgparser.h"
#include "libmpsub/subreader.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mp_conf_s {
    int		has_video;
    int		has_audio;
    int		has_dvdsub;
    uint32_t	msg_filter;
    int		test_av;
    int		malloc_debug;
    unsigned	max_trace;
// XP-core
    int		xp;   /* XP-mode */
    int		gomp; /* currently it's experimental feature */
    const char*	stream_dump; // dump name
    int		s_cache_size; /* cache2: was 1024 let it be 0 by default */
    int		autoq; /* quality's options: */
    unsigned	verbose;
    int		benchmark;
    float	playbackspeed_factor;
// sync
    int		frame_dropping; // option  0=no drop  1= drop vo  2= drop decode
    int		av_sync_pts;
    int		av_force_pts_fix;
    int		av_force_pts_fix2;
    int		frame_reorder;
    float	force_fps;
    int		softsleep;
    int		nortc;
// streaming:
    int		audio_id;
    int		video_id;
    int		dvdsub_id;
    int		vobsub_id;
    const char*	audio_lang;
    const char*	dvdsub_lang;
    const char*	spudec_ifo;
    unsigned	force_srate;
// seek:
    const char*	seek_to_sec;
    long long int seek_to_byte;
    int		loop_times;
    int		shuffle_playback;
    int		play_n_frames;
/* codecs: */
    const char*	audio_codec;  /* override audio codec */
    const char*	video_codec;  /* override video codec */
    const char*	audio_family; /* override audio codec family */
    const char*	video_family; /* override video codec family */
// drivers:
    char*	video_driver; //"mga"; // default
    char*	audio_driver;
// sub:
    int		osd_level;
    const char*	font_name;
    float	font_factor;
    const char*	sub_name;
    float	sub_fps;
    int		sub_auto;
    const char*	vobsub_name;
    int		subcc_enabled;
// others
    const char*	npp_options;
    unsigned	ao_channels;
    int		z_compression;
    int		xinerama_screen;
    float	monitor_pixel_aspect;
}mp_conf_t;
extern mp_conf_t mp_conf;

/* Benchmarking */
typedef struct time_usage_s {
    double video;
    double vout;
    double audio_decode_correction;
    double audio_decode;  /**< time for decoding in thread */
    double audio,max_audio,min_audio,cur_audio;
    double min_audio_decode;
    double max_audio_decode;
    double max_demux;
    double demux;
    double min_demux;
    double max_c2;
    double c2;
    double min_c2;
    double max_video;
    double cur_video;
    double min_video;
    double max_vout;
    double cur_vout;
    double min_vout;
    double total_start;
}time_usage_t;

/* non-configurable through command line stuff */
typedef struct MPXPContext_s {
    int		rtc_fd;
    int		seek_time;
    int		output_quality;
    unsigned	mpxp_after_seek;
    int		use_pts_fix2;
    unsigned	mplayer_accel;
    subtitle* 	subtitles;
    m_config_t*	mconfig;
    time_usage_t*bench;
    any_t*	priv;
    any_t*	msg_priv;
}MPXPContext_t;
extern MPXPContext_t* MPXPCtx;

extern void update_osd( float v_pts );

extern pthread_mutex_t audio_timer_mutex;

extern void exit_player(const char* why);

static inline void escape_player(const char* why,unsigned num_calls) {
    show_backtrace(why,num_calls);
    exit_player(why);
}

static inline MPXP_Rc check_pin(const char* module,unsigned pin1,unsigned pin2) {
    if(pin1!=pin2) {
	char msg[4096];
	strcpy(msg,"Found incorrect PIN in module: ");
	strcat(msg,module);
	escape_player(msg,mp_conf.max_trace);
    }
    return MPXP_Ok;
}

extern void mp_register_options(m_config_t* cfg);
extern void mpxp_resync_audio_stream(void);
extern void mpxp_reset_vcache(void);
extern void __exit_sighandler(void);

extern void mplayer_put_key(int code);

#ifdef __cplusplus
}
#endif
#endif
