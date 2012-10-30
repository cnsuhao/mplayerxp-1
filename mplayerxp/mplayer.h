#ifndef __MPLAYERXP_MAIN
#define __MPLAYERXP_MAIN 1

#include <pthread.h>
#include "mp_config.h"

typedef struct mp_conf_s {
    int		has_video;
    int		has_audio;
    int		has_dvdsub;
    int		use_stdin;
    int		slave_mode;
// XP-core
    int		xp;   /* XP-mode */
    int		gomp; /* currently it's experimental feature */
    char*	stream_dump; // dump name
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
    char*	audio_lang;
    char*	dvdsub_lang;
    char*	spudec_ifo;
    unsigned	force_srate;
// seek:
    char*	seek_to_sec;
    long long int seek_to_byte;
    int		loop_times;
    int		shuffle_playback;
    int		play_n_frames;
/* codecs: */
    char*	audio_codec;  /* override audio codec */
    char*	video_codec;  /* override video codec */
    char*	audio_family; /* override audio codec family */
    char*	video_family; /* override video codec family */
// drivers:
    char*	video_driver; //"mga"; // default
    char*	audio_driver;
// sub:
    int		osd_level;
    char*	font_name;
    float	font_factor;
    char*	sub_name;
    float	sub_fps;
    int		sub_auto;
    char*	vobsub_name;
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
typedef struct mp_data_s {
    int		seek_time;
    int		output_quality;
    int		use_pts_fix2;
    unsigned	mplayer_accel;
    any_t* 	subtitles;
    any_t*	mconfig;
    time_usage_t*bench;
    any_t*	priv;
    any_t*	msg_priv;
}mp_data_t;
extern mp_data_t* mp_data;


extern void exit_player(char* how);
extern void mpxp_resync_audio_stream(void);
extern void mpxp_reset_vcache(void);
extern void __exit_sighandler(void);

extern void mplayer_put_key(int code);
extern int mplayer_get_key(void);
#endif
