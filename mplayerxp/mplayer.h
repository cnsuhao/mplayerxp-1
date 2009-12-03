#ifndef __MPLAYERXP_MAIN
#define __MPLAYERXP_MAIN 1

typedef struct initial_audio_pts_correction_s
{
    int need_correction;
    int pts_bytes;
    int nbytes;
}initial_audio_pts_correction_t;

extern initial_audio_pts_correction_t initial_audio_pts_corr;
extern float initial_audio_pts;
extern int enable_xp;
extern unsigned verbose;
extern unsigned xp_num_frames;
extern int xp_id;
extern unsigned mplayer_accel;
extern int frame_dropping;

extern int audio_id;
extern int video_id;
extern int dvdsub_id;
extern int vobsub_id;
extern char* audio_lang;
extern char* dvdsub_lang;

extern int stream_cache_size;

extern int av_sync_pts;
extern int av_force_pts_fix;
extern int frame_reorder;
extern int use_pts_fix2;
extern int av_force_pts_fix2;


extern void exit_player(char* how);
extern void mpxp_resync_audio_stream(void);
extern void mpxp_reset_vcache(void);
extern void killall_threads(pthread_t pth_id);
extern void __exit_sighandler(void);

extern void mplayer_put_key(int code);
extern int mplayer_get_key(void);

/* Benchmarking */

extern int benchmark;
extern double video_time_usage;
extern double vout_time_usage;
extern double audio_decode_time_usage_correction;
extern double audio_decode_time_usage;  /**< time for decoding in thread */
extern double min_audio_decode_time_usage;
extern double max_audio_decode_time_usage;
extern double max_demux_time_usage;
extern double demux_time_usage;
extern double min_demux_time_usage;
extern double max_c2_time_usage;
extern double c2_time_usage;
extern double min_c2_time_usage;
extern double max_video_time_usage;
extern double cur_video_time_usage;
extern double min_video_time_usage;
extern double max_vout_time_usage;
extern double cur_vout_time_usage;
extern double min_vout_time_usage;

#endif
