/*
   eXtra performance MPlayer's CORE
   Licence: GPL v2
   Note: Threaded engine to decode frames ahead
*/

#ifndef __XMP_CORE_H
#define __XMP_CORE_H 1

#include <inttypes.h>
#include <pthread.h>

#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;

#include "libvo2/video_out.h"
#include "libmpdemux/stheader.h"

namespace mpxp {
    enum xp_modes { XP_NA=0, XP_UniCore, XP_DualCore, XP_TripleCore, XP_MultiCore };

    struct xmp_frame_t {
	float		v_pts;		/* presentation time-stamp from input stream
					   __huge_valf indicates EOF */
	float		duration;	/* frame duration */
	any_t*		priv;
    };

    struct dec_ahead_engine_t;

    typedef any_t* (*func_new_frame_priv_t)(struct dec_ahead_engine_t*);
    typedef void   (*func_free_frame_priv_t)(struct dec_ahead_engine_t*,any_t*);

    struct dec_ahead_engine_t {
	volatile unsigned	player_idx;	/* index of frame which is currently played */
	volatile unsigned	decoder_idx;	/* index of frame which is currently decoded */
	unsigned		nframes;	/* number of frames in buffer */
	xmp_frame_t*		frame;		/* frame related attributes */
	Opaque*			sh;		/* corresponded sh_audio_t or sh_video_t */
	int			eof;		/* EOF for stream */
	/* methods */
	func_new_frame_priv_t	new_priv;
	func_free_frame_priv_t	free_priv;
	/* for statistics */
	unsigned		num_slow_frames;/* number of frames which were delayed due slow computer */
	long long int		num_played_frames;
	long long int		num_decoded_frames; /* for frame dropping */
	long long int		num_dropped_frames;
    };

    enum { main_id=0 };
    enum { MAX_MPXP_THREADS=16 };
    enum mpxp_thread_state { Pth_Stand=0, Pth_Canceling, Pth_Run, Pth_Sleep, Pth_ASleep };

    typedef any_t*(*mpxp_routine_t)(any_t*);
    typedef void (*sig_handler_t)(void);

    enum xmp_model_e {
	XMP_Run_AudioPlayer	=0x00000001,
	XMP_Run_AudioPlayback	=0x00000002, /* audio player+decoder together */
	XMP_Run_VideoPlayer	=0x00000004,
	XMP_Run_VA_Decoder	=0x00000008, /* audio+video decoders together */
	XMP_Run_AudioDecoder	=0x00000010,
	XMP_Run_VideoDecoder	=0x00000020
    };

    inline xmp_model_e operator~(xmp_model_e a) { return static_cast<xmp_model_e>(~static_cast<unsigned>(a)); }
    inline xmp_model_e operator|(xmp_model_e a, xmp_model_e b) { return static_cast<xmp_model_e>(static_cast<unsigned>(a)|static_cast<unsigned>(b)); }
    inline xmp_model_e operator&(xmp_model_e a, xmp_model_e b) { return static_cast<xmp_model_e>(static_cast<unsigned>(a)&static_cast<unsigned>(b)); }
    inline xmp_model_e operator^(xmp_model_e a, xmp_model_e b) { return static_cast<xmp_model_e>(static_cast<unsigned>(a)^static_cast<unsigned>(b)); }
    inline xmp_model_e operator|=(xmp_model_e a, xmp_model_e b) { return (a=static_cast<xmp_model_e>(static_cast<unsigned>(a)|static_cast<unsigned>(b))); }
    inline xmp_model_e operator&=(xmp_model_e a, xmp_model_e b) { return (a=static_cast<xmp_model_e>(static_cast<unsigned>(a)&static_cast<unsigned>(b))); }
    inline xmp_model_e operator^=(xmp_model_e a, xmp_model_e b) { return (a=static_cast<xmp_model_e>(static_cast<unsigned>(a)^static_cast<unsigned>(b))); }

    struct mpxp_thread_t {
	unsigned	p_idx;
	const char*	name;
	const char*	unit;
	pthread_t	pth_id;
	pid_t		pid;
	mpxp_routine_t	routine;
	sig_handler_t	sigfunc;
	dec_ahead_engine_t* dae;
	volatile enum mpxp_thread_state state;
    };

    struct initial_audio_pts_correction_t {
	int need_correction;
	int pts_bytes;
	int nbytes;
    };

    struct xp_core_t {
	xmp_model_e			flags;
	dec_ahead_engine_t*		video;
	dec_ahead_engine_t*		audio;
	volatile int		in_pause;
	volatile int		in_resize;
	/* XMP engine */
	mpxp_thread_t*		mpxp_threads[MAX_MPXP_THREADS];
	unsigned			num_threads;
	pthread_t			main_pth_id;
	/* doubtful stuff */
	unsigned			num_a_buffs; // number of audio buffers
	unsigned			num_v_buffs; // number of video buffers
	int				bad_pts;     // for MPEGxx codecs
	float			initial_apts;
	initial_audio_pts_correction_t initial_apts_corr;
    };

    int		xmp_test_model(xmp_model_e value);

    void	xmp_init(void);
    void	xmp_uninit(void);

    /* returns idx of main thread */
    unsigned	xmp_register_main(sig_handler_t sigfunc);
    /* returns idx of the thread or UINT_MAX on fault */
    unsigned	xmp_register_thread(dec_ahead_engine_t* dae,sig_handler_t sigfunc,mpxp_routine_t routine,const char *name);
    void	xmp_stop_threads(int force);

    void	xmp_halt_threads(int is_reset_vcache);
    void	xmp_restart_threads(int xp_id);

    void	xmp_killall_threads(pthread_t _self);

    int		xmp_init_engine(sh_video_t*stream, sh_audio_t *astream);
    void	xmp_uninit_engine( int force );
    int		xmp_run_decoders( void );
    int		xmp_run_players( void );
    void	xmp_reset_sh_video(sh_video_t* shv);

    void	dae_init(dec_ahead_engine_t* it,unsigned nframes,Opaque* sh);
    void	dae_uninit(dec_ahead_engine_t* it);
    void	dae_reset(dec_ahead_engine_t* it); /* after mpxp_seek */

    /* returns 1 - on success 0 - if busy */
    int		dae_try_inc_played(dec_ahead_engine_t* it);
    int		dae_inc_played(dec_ahead_engine_t* it);
    int		dae_inc_decoded(dec_ahead_engine_t* it);

    unsigned	dae_prev_played(const dec_ahead_engine_t* it);
    unsigned	dae_prev_decoded(const dec_ahead_engine_t* it);
    unsigned	dae_next_played(const dec_ahead_engine_t* it);
    unsigned	dae_next_decoded(const dec_ahead_engine_t* it);
    /* returns normalized decoder_idx-player_idx */
    unsigned	dae_get_decoder_outrun(const dec_ahead_engine_t* it);
    void	dae_wait_decoder_outrun(const dec_ahead_engine_t* it);

    inline unsigned dae_curr_vplayed(const xp_core_t* xpc) { return xpc->video->player_idx; }
    inline unsigned dae_curr_vdecoded(const xp_core_t* xpc) { return xpc->video->decoder_idx; }
    inline unsigned dae_prev_vplayed(const xp_core_t* xpc) { return dae_prev_played(xpc->video); }
    inline unsigned dae_prev_vdecoded(const xp_core_t* xpc) { return dae_prev_decoded(xpc->video); }
    inline unsigned dae_next_vplayed(const xp_core_t* xpc) { return dae_next_played(xpc->video); }
    inline unsigned dae_next_vdecoded(const xp_core_t* xpc) { return dae_next_decoded(xpc->video); }

    xmp_frame_t	dae_played_frame(const dec_ahead_engine_t* it); // used in ao_wav.c
    xmp_frame_t	dae_decoded_frame(const dec_ahead_engine_t* it);
    xmp_frame_t	dae_next_played_frame(const dec_ahead_engine_t* it);
    xmp_frame_t	dae_next_decoded_frame(const dec_ahead_engine_t* it);
    xmp_frame_t	dae_prev_played_frame(const dec_ahead_engine_t* it);
    xmp_frame_t	dae_prev_decoded_frame(const dec_ahead_engine_t* it);

    int		dae_played_eof(const dec_ahead_engine_t* it);
    int		dae_decoded_eof(const dec_ahead_engine_t* it);
    void	dae_decoded_mark_eof(dec_ahead_engine_t* it);
    inline void dae_decoded_clear_eof(dec_ahead_engine_t* it) { UNUSED(it); }
} // namespace mpxp;

extern pthread_mutex_t audio_play_mutex;
extern pthread_cond_t audio_play_cond;

extern pthread_mutex_t audio_decode_mutex;
extern pthread_cond_t audio_decode_cond;

#ifdef ENABLE_DEC_AHEAD_DEBUG
#include "mp_msg.h"
#ifndef DA_PREFIX
#define DA_PREFIX "dec_ahead:"
#endif
#define MSG_D(args...) { mp_msg(MSGT_GLOBAL, MSGL_V, __FILE__, __LINE__, ## args ); mp_msg_flush(); }
#else
#define MSG_D(args...)
#endif

#define LOCK_AUDIO_PLAY() { MSG_D(DA_PREFIX"LOCK_AUDIO_PLAY\n"); pthread_mutex_lock(&audio_play_mutex); }
#define UNLOCK_AUDIO_PLAY() { MSG_D(DA_PREFIX"UNLOCK_AUDIO_PLAY\n"); pthread_mutex_unlock(&audio_play_mutex); }

#define LOCK_AUDIO_DECODE() { MSG_D(DA_PREFIX"LOCK_AUDIO_DECODE\n"); pthread_mutex_lock(&audio_decode_mutex); }
#define UNLOCK_AUDIO_DECODE() { MSG_D(DA_PREFIX"UNLOCK_AUDIO_DECODE\n"); pthread_mutex_unlock(&audio_decode_mutex); }

#define __MP_ATOMIC(OP) { static pthread_mutex_t loc_mutex; pthread_mutex_lock(&loc_mutex); OP; pthread_mutex_unlock(&loc_mutex); }
#define __MP_SYNCHRONIZE(mtx,OP) { pthread_mutex_lock(&mtx); OP; pthread_mutex_unlock(&mtx); }

extern volatile int dec_ahead_can_aseek;

extern struct timespec audio_play_timeout;
extern int audio_play_in_sleep;

/* Audio stuff */
extern volatile float dec_ahead_audio_delay;

#endif
