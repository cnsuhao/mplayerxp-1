/*
   Decoding ahead
   Licence: GPL v2
   Author: Nickols_K
   Note: Threaded engine to decode frames ahead
*/

#ifndef __DEC_AHEAD_H
#define __DEC_AHEAD_H

#include <inttypes.h>
#include <pthread.h>
#include "libmpdemux/stream.h"
#include "libmpdemux/demuxer.h"
#include "libmpdemux/stheader.h"
#include "libmpdemux/demuxer_r.h"
#include "libvo/video_out.h"

//#define ENABLE_DEC_AHEAD_DEBUG 1

enum seek_states { NoSeek=0, PreSeek, Seek };

enum xp_modes { XP_None=0, XP_Video, XP_VideoAudio, XP_VAPlay, XP_VAFull };

extern pthread_mutex_t vdec_active_mutex; /* it's related with video decoding (main process) */
extern pthread_mutex_t vdec_locked_mutex; /* it's related with video decoding (thread) */
extern pthread_mutex_t vreading_mutex; /* it's related with reading of video stream  */
extern pthread_mutex_t vdecoding_mutex; /* it's related with video decoding process */
extern pthread_mutex_t vdeca_mutex; /* it's related with video decoding attributes */

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
#define LOCK_VDEC_ACTIVE() { MSG_D(DA_PREFIX"LOCK_VDEC_ACTIVE\n"); pthread_mutex_lock(&vdec_active_mutex); }
#define UNLOCK_VDEC_ACTIVE() { MSG_D(DA_PREFIX"UNLOCK_VDEC_ACTIVE\n"); pthread_mutex_unlock(&vdec_active_mutex); }

#define LOCK_VDEC_LOCKED() { MSG_D(DA_PREFIX"LOCK_VDEC_LOCKED\n"); pthread_mutex_lock(&vdec_locked_mutex); }
#define UNLOCK_VDEC_LOCKED() { MSG_D(DA_PREFIX"UNLOCK_VDEC_LOCKED\n"); pthread_mutex_unlock(&vdec_locked_mutex); }

#define LOCK_VDECODING() { MSG_D(DA_PREFIX"LOCK_VDECODING\n"); pthread_mutex_lock(&vdecoding_mutex); }
#define UNLOCK_VDECODING() { MSG_D(DA_PREFIX"UNLOCK_VDECODING\n"); pthread_mutex_unlock(&vdecoding_mutex); }

#define LOCK_VREADING() { MSG_D(DA_PREFIX"LOCK_VREADING\n"); pthread_mutex_lock(&vreading_mutex); }
#define UNLOCK_VREADING() { MSG_D(DA_PREFIX"UNLOCK_VREADING\n"); pthread_mutex_unlock(&vreading_mutex); }

#define LOCK_VDECA() { MSG_D(DA_PREFIX"LOCK_VDECA\n"); pthread_mutex_lock(&vdeca_mutex); }
#define UNLOCK_VDECA() { MSG_D(DA_PREFIX"UNLOCK_VDECA\n"); pthread_mutex_unlock(&vdeca_mutex); }

#define LOCK_AUDIO_PLAY() { MSG_D(DA_PREFIX"LOCK_AUDIO_PLAY\n"); pthread_mutex_lock(&audio_play_mutex); }
#define UNLOCK_AUDIO_PLAY() { MSG_D(DA_PREFIX"UNLOCK_AUDIO_PLAY\n"); pthread_mutex_unlock(&audio_play_mutex); }

#define LOCK_AUDIO_DECODE() { MSG_D(DA_PREFIX"LOCK_AUDIO_DECODE\n"); pthread_mutex_lock(&audio_decode_mutex); }
#define UNLOCK_AUDIO_DECODE() { MSG_D(DA_PREFIX"UNLOCK_AUDIO_DECODE\n"); pthread_mutex_unlock(&audio_decode_mutex); }

#define LOCK_VIDEO_DECODE() { MSG_D(DA_PREFIX"LOCK_VIDEO_DECODE\n"); pthread_mutex_lock(&video_decode_mutex); }
#define UNLOCK_VIDEO_DECODE() { MSG_D(DA_PREFIX"UNLOCK_VIDEO_DECODE\n"); pthread_mutex_unlock(&video_decode_mutex); }

#define __MP_ATOMIC(OP) { static pthread_mutex_t loc_mutex; pthread_mutex_lock(&loc_mutex); OP; pthread_mutex_unlock(&loc_mutex); }
#define __MP_SYNCHRONIZE(mtx,OP) { pthread_mutex_lock(&mtx); OP; pthread_mutex_unlock(&mtx); }

typedef struct sh_video_attr
{
  int eof;			/* indicates last frame in stream */
  float duration;		/* frame duration */
  float v_pts;			/* presentation time-stamp from input stream */
  float stream_pts;		/* real stream's PTS mainly for OSD */
  float num_frames;             /* number of frames played */
  int num_frames_decoded;       /* number of frames decoded */
}shva_t;

extern shva_t* shva;

extern volatile int xp_eof;
extern int xp_audio_eof;
extern int has_xp_audio;
extern int has_xp_video;
extern int xp_is_bad_pts;

extern volatile int dec_ahead_locked_frame;
extern volatile unsigned abs_dec_ahead_locked_frame;
extern volatile unsigned abs_dec_ahead_blitted_frame;
extern volatile int dec_ahead_in_lseek;
extern volatile int dec_ahead_in_pause;
extern volatile int dec_ahead_in_resize;
extern volatile float dec_ahead_seek_num_frames;       // frames played after seek
extern volatile int dec_ahead_seek_num_frames_decoded; // frames decoded after seek
extern volatile int dec_ahead_num_frames_decoded;      // frames decoded by thread
extern volatile int dec_ahead_can_aseek;
extern int ao_da_buffs;
			/* 
			   stream - pointer to openned stream
			   astream - pointer to audio stream
			 */

extern int dec_ahead_pid; /* Only for testing */
extern pthread_t dec_ahead_pth_id;

extern int init_dec_ahead(sh_video_t*stream, sh_audio_t *astream);
extern void uninit_dec_ahead( int force );
extern int run_dec_ahead( void );
extern int run_xp_players( void );
extern void dec_ahead_reset_sh_video(sh_video_t* shv);

extern void sig_dec_ahead_video( void );

/* Audio stuff */
int xp_thread_decode_audio();
extern void sig_audio_play( void );
extern void sig_audio_decode( void );
extern volatile float dec_ahead_audio_delay;
extern void dec_ahead_halt_threads(int is_reset_vcache);
extern void dec_ahead_restart_threads(int xp_id);
#endif
