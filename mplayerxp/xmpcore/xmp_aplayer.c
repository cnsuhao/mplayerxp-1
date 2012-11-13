#include "mplayerxp.h"
#include "mp_msg.h"
#include "sig_hand.h"
#include "xmp_core.h"
#include "xmp_aplayer.h"
#include "xmp_adecoder.h"
#include "osdep/timer.h"
#include "libmpcodecs/dec_audio.h"

#include "libao2/audio_out.h"

#include <stdio.h>
#include <unistd.h> // for usleep()
#include <pthread.h>
#include <math.h>

#ifdef ENABLE_DEC_AHEAD_DEBUG
#define MSG_T(args...) mp_msg(MSGT_GLOBAL, MSGL_DBG2,__FILE__,__LINE__, ## args )
#else
#define MSG_T(args...)
#endif

#define MIN_AUDIO_TIME 0.05
#define NOTHING_PLAYED (-1.0)
#define XP_MIN_TIMESLICE 0.010 /* under Linux on x86 min time_slice = 10 ms */

extern ao_data_t*ao_data;

static int decore_audio(demux_stream_t *d_audio,sh_audio_t* sh_audio,sh_video_t*sh_video)
{
    int eof = 0;
/*========================== PLAY AUDIO ============================*/
while(sh_audio){
  unsigned int t;
  double tt;
  int playsize;
  float pts=HUGE;
  int ret=0;

  ao_data->pts=sh_audio->timer*90000.0;
  playsize=ao_get_space(ao_data);

  if(!playsize) {
    if(sh_video)
      break; // buffer is full, do not block here!!!
    usec_sleep(10000); // Wait a tick before retry
    continue;
  }

  if(playsize>MAX_OUTBURST) playsize=MAX_OUTBURST; // we shouldn't exceed it!
  //if(playsize>outburst) playsize=outburst;

  // Update buffer if needed
  MP_UNIT("mpca_decode");   // Enter AUDIO decoder module
  t=GetTimer();
  while(sh_audio->a_buffer_len<playsize && !xp_core->audio->eof){
      if(!xmp_test_model(XMP_Run_AudioPlayback)) {
          ret=read_audio_buffer(sh_audio,&sh_audio->a_buffer[sh_audio->a_buffer_len],
                              playsize-sh_audio->a_buffer_len,sh_audio->a_buffer_size-sh_audio->a_buffer_len,&pts);
      } else {
          ret=RND_RENAME3(mpca_decode)(sh_audio,&sh_audio->a_buffer[sh_audio->a_buffer_len],
                           playsize-sh_audio->a_buffer_len,sh_audio->a_buffer_size-sh_audio->a_buffer_len,sh_audio->a_buffer_size-sh_audio->a_buffer_len,&pts);
      }
    if(ret>0) sh_audio->a_buffer_len+=ret;
    else {
      if(!d_audio->eof) break;
      xp_core->audio->eof=1;
      break;
    }
  }
  MP_UNIT("play_audio");   // Leave AUDIO decoder module
  t=GetTimer()-t;
  tt = t*0.000001f;
  mp_data->bench->audio+=tt;
  if(mp_conf.benchmark)
  {
    if(tt > mp_data->bench->max_audio) mp_data->bench->max_audio = tt;
    if(tt < mp_data->bench->min_audio) mp_data->bench->min_audio = tt;
    mp_data->bench->cur_audio=tt;
  }
  if(playsize>sh_audio->a_buffer_len) playsize=sh_audio->a_buffer_len;

  if(xmp_test_model(XMP_Run_AudioPlayer)) dec_ahead_audio_delay=ao_get_delay(ao_data);

  playsize=RND_RENAME6(ao_play)(ao_data,sh_audio->a_buffer,playsize,0);

  if(playsize>0){
      sh_audio->a_buffer_len-=playsize;
      memcpy(sh_audio->a_buffer,&sh_audio->a_buffer[playsize],sh_audio->a_buffer_len);
      if(!mp_conf.av_sync_pts && xmp_test_model(XMP_Run_AudioPlayer))
          pthread_mutex_lock(&audio_timer_mutex);
      if(mp_data->use_pts_fix2) {
	  if(sh_audio->a_pts != HUGE) {
	      sh_audio->a_pts_pos-=playsize;
	      if(sh_audio->a_pts_pos > -ao_get_delay(ao_data)*sh_audio->af_bps) {
		  sh_audio->timer+=playsize/(float)(sh_audio->af_bps);
	      } else {
		  sh_audio->timer=sh_audio->a_pts-(float)sh_audio->a_pts_pos/(float)sh_audio->af_bps;
		  MSG_V("Audio chapter change detected\n");
		  sh_audio->chapter_change=1;
		  sh_audio->a_pts = HUGE;
	      }
	  } else if(pts != HUGE) {
	      if(pts < 1.0 && sh_audio->timer > 2.0) {
		  sh_audio->timer+=playsize/(float)(sh_audio->af_bps);
		  sh_audio->a_pts=pts;
		  sh_audio->a_pts_pos=sh_audio->a_buffer_len-ret;
	      } else {
		  sh_audio->timer=pts+(ret-sh_audio->a_buffer_len)/(float)(sh_audio->af_bps);
		  sh_audio->a_pts=HUGE;
	      }
	  } else
	      sh_audio->timer+=playsize/(float)(sh_audio->af_bps);
      } else if(mp_conf.av_sync_pts && pts!=HUGE)
	  sh_audio->timer=pts+(ret-sh_audio->a_buffer_len)/(float)(sh_audio->af_bps);
      else
	  sh_audio->timer+=playsize/(float)(sh_audio->af_bps);
      if(!mp_conf.av_sync_pts && xmp_test_model(XMP_Run_AudioPlayer))
          pthread_mutex_unlock(&audio_timer_mutex);
  }

  break;
 } // if(sh_audio)
 return eof;
}


extern ao_data_t* ao_data;
any_t* audio_play_routine( any_t* arg )
{
    mpxp_thread_t* priv=arg;
    sh_audio_t* sh_audio=priv->dae->sh;
    demux_stream_t *d_audio=sh_audio->ds;
    demuxer_t *demuxer=d_audio->demuxer;
    sh_video_t* sh_video=demuxer->video->sh;

    int eof = 0;
    struct timeval now;
    struct timespec timeout;
    float d;
    int retval;
    const float MAX_AUDIO_TIME = (float)ao_get_space(ao_data) / sh_audio->af_bps + ao_get_delay(ao_data);
    float min_audio_time = MAX_AUDIO_TIME;
    float min_audio, max_audio;
    int samples, collect_samples;
    float audio_buff_max, audio_buff_norm, audio_buff_min, audio_buff_alert;

    audio_buff_alert = max(XP_MIN_TIMESLICE, min(MIN_AUDIO_TIME,MAX_AUDIO_TIME/4));
    audio_buff_max = max(audio_buff_alert, min(MAX_AUDIO_TIME-XP_MIN_TIMESLICE, audio_buff_alert*4));
    audio_buff_min = min(audio_buff_max, audio_buff_alert*2);
    audio_buff_norm = (audio_buff_max + audio_buff_min) / 2;

    MSG_DBG2("alert %f, min %f, norm %f, max %f \n", audio_buff_alert, audio_buff_min, audio_buff_norm, audio_buff_max );

    samples = 5;
    collect_samples = 1;
    min_audio = MAX_AUDIO_TIME;
    max_audio = 0;

    priv->pid = getpid();
    __MP_UNIT(priv->p_idx,"audio_play_routine");
    priv->state=Pth_Run;
    if(xmp_test_model(XMP_Run_AudioPlayback))
	priv->name = "audio decoder+player";
    dec_ahead_can_aseek=0;

    while(priv->state!=Pth_Canceling) {
	if(priv->state==Pth_Sleep) {
	    priv->state=Pth_ASleep;
	    while(priv->state==Pth_ASleep) usleep(0);
	    continue;
	}
	__MP_UNIT(priv->p_idx,"audio decore_audio");
	dec_ahead_audio_delay = NOTHING_PLAYED;
	eof = decore_audio(d_audio,sh_audio,sh_video);

	if(priv->state==Pth_Canceling) break;

	__MP_UNIT(priv->p_idx,"audio sleep");

	dec_ahead_can_aseek = 1;  /* Safe for other threads to seek */

	if( dec_ahead_audio_delay == NOTHING_PLAYED ) { /* To fast, we can sleep longer */
	    if( min_audio_time > audio_buff_alert ) {
		min_audio_time *= 0.75;
		MSG_DBG2("To fast, set min_audio_time %.5f (delay %5f) \n", min_audio_time, dec_ahead_audio_delay );
	    }
	    collect_samples = 1;
	    samples = 5;
	    min_audio = MAX_AUDIO_TIME;
	    max_audio = 0;
	} else if( dec_ahead_audio_delay <= audio_buff_alert ) { /* To slow, sleep shorter */
	    if ( min_audio_time < MAX_AUDIO_TIME ) {
		min_audio_time *= 2.0;
		MSG_DBG2("To slow, set min_audio_time %.5f (delay %5f) \n", min_audio_time, dec_ahead_audio_delay );
	    }
	    collect_samples = 1;
	    samples = 10;
	    min_audio = MAX_AUDIO_TIME;
	    max_audio = 0;
	} else if( !xp_core->audio->eof && collect_samples) {
	    if( dec_ahead_audio_delay < min_audio )
		min_audio = dec_ahead_audio_delay;
	    if( dec_ahead_audio_delay > max_audio )
		max_audio = dec_ahead_audio_delay;
	    samples--;

	    if( samples <= 0 ) {
		if( min_audio > audio_buff_max ) {
		    min_audio_time -= min_audio-audio_buff_norm;
		    collect_samples = 1;
		    MSG_DBG2("Decrease min_audio_time %.5f (min %.5f max %.5f) \n", min_audio_time, min_audio, max_audio );
		} else if( max_audio < audio_buff_min ) {
		    min_audio_time *= 1.25;
		    collect_samples = 1;
		    MSG_DBG2("Increase min_audio_time %.5f (min %.5f max %.5f) \n", min_audio_time, min_audio, max_audio );
		} else {
		    collect_samples = 0; /* No change, stop */
		    MSG_DBG2("Stop collecting samples time %.5f (min %.5f max %.5f) \n", min_audio_time, min_audio, max_audio );
		}
		if(collect_samples) {
		    samples = 5;
		    min_audio = MAX_AUDIO_TIME;
		    max_audio = 0;
		}
	    }
	}

	LOCK_AUDIO_PLAY();
	d = ao_get_delay(ao_data) - min_audio_time;
	if( d > 0 ) {
	    gettimeofday(&now,NULL);
	    audio_play_timeout.tv_nsec = now.tv_usec * 1000 + d*1000000000l;
	    if( audio_play_timeout.tv_nsec > 1000000000l ) {
		audio_play_timeout.tv_nsec-=1000000000l;
		audio_play_timeout.tv_sec = now.tv_sec + 1;
	    } else
		audio_play_timeout.tv_sec = now.tv_sec;
	    audio_play_in_sleep=1;
	    pthread_cond_timedwait( &audio_play_cond, &audio_play_mutex, &audio_play_timeout );
	    audio_play_in_sleep=0;
	}
	UNLOCK_AUDIO_PLAY();

	if(priv->state==Pth_Canceling) break;

	LOCK_AUDIO_PLAY();
	if(eof && priv->state!=Pth_Canceling) {
	    __MP_UNIT(priv->p_idx,"wait end of work");
	    pthread_cond_wait( &audio_play_cond, &audio_play_mutex );
	}
	UNLOCK_AUDIO_PLAY();

	if(priv->state==Pth_Canceling) break;

	__MP_UNIT(priv->p_idx,"audio pause");
	LOCK_AUDIO_PLAY();
	while( xp_core->in_pause && priv->state!=Pth_Canceling) {
	    pthread_cond_wait( &audio_play_cond, &audio_play_mutex );
	}
	UNLOCK_AUDIO_PLAY();

	if(priv->state==Pth_Canceling) break;

	__MP_UNIT(priv->p_idx,"audio seek");
	LOCK_AUDIO_PLAY();
#if 0
	while( priv->state==Pth_Sleep && priv->state!=Pth_Canceling) {
	    gettimeofday(&now,NULL);
	    timeout.tv_nsec = now.tv_usec * 1000;
	    timeout.tv_sec = now.tv_sec + 1;
	    retval = pthread_cond_timedwait( &audio_play_cond, &audio_play_mutex, &timeout );
	    if( retval == ETIMEDOUT )
		MSG_V("Audio seek timeout\n");
	}
#endif
	dec_ahead_can_aseek = 0; /* Not safe to seek */
	UNLOCK_AUDIO_PLAY();
    }
    fflush(stdout);
    __MP_UNIT(priv->p_idx,"audio exit");
    dec_ahead_can_aseek=1;
    priv->state=Pth_Stand;
    return arg;
}

void sig_audio_play( void )
{
    MSG_T("sig_audio_play\n");
    mp_msg_flush();

    dec_ahead_can_aseek=1;

    UNLOCK_AUDIO_PLAY();

    xmp_killall_threads(pthread_self());
    __exit_sighandler();
}
