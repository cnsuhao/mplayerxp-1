#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
#include <algorithm>

#include <stdio.h>
#include <pthread.h>
#include <math.h>
#include <sys/time.h>

#include "mplayerxp.h"
#include "player_msg.h"
#include "sig_hand.h"

#include "libmpdemux/demuxer.h"
#include "libmpcodecs/dec_audio.h"
#include "libao3/audio_out.h"

#include "osdep/timer.h"
#include "xmp_core.h"
#include "xmp_aplayer.h"
#include "xmp_adecoder.h"

static const float MIN_AUDIO_TIME=0.05f;
static const float NOTHING_PLAYED=-1.0f;
static const float XP_MIN_TIMESLICE=0.010f; /* under Linux on x86 min time_slice = 10 ms */

namespace mpxp {

static int decore_audio(Demuxer_Stream *d_audio,sh_audio_t* sh_audio,sh_video_t*sh_video)
{
    int eof = 0;
/*========================== PLAY AUDIO ============================*/
while(sh_audio){
  unsigned int t;
  double tt;
  int playsize;
  float pts=HUGE;
  int ret=0;

  mpxp_context().audio().output->pts=sh_audio->timer*90000.0;
  playsize=mpxp_context().audio().output->get_space();

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
  while(sh_audio->a_buffer_len<playsize && !mpxp_context().engine().xp_core->audio->eof){
      if(!xmp_test_model(XMP_Run_AudioPlayback)) {
	  ret=read_audio_buffer(sh_audio,(unsigned char *)&sh_audio->a_buffer[sh_audio->a_buffer_len],
			      playsize-sh_audio->a_buffer_len,sh_audio->a_buffer_size-sh_audio->a_buffer_len,&pts);
      } else {
	  ret=mpca_decode(mpxp_context().audio().decoder,(unsigned char *)&sh_audio->a_buffer[sh_audio->a_buffer_len],
			   playsize-sh_audio->a_buffer_len,sh_audio->a_buffer_size-sh_audio->a_buffer_len,sh_audio->a_buffer_size-sh_audio->a_buffer_len,&pts);
      }
    if(ret>0) sh_audio->a_buffer_len+=ret;
    else {
      if(!d_audio->eof) break;
      mpxp_context().engine().xp_core->audio->eof=1;
      break;
    }
  }
  MP_UNIT("play_audio");   // Leave AUDIO decoder module
  t=GetTimer()-t;
  tt = t*0.000001f;
  mpxp_context().bench->audio+=tt;
  if(mp_conf.benchmark)
  {
    if(tt > mpxp_context().bench->max_audio) mpxp_context().bench->max_audio = tt;
    if(tt < mpxp_context().bench->min_audio) mpxp_context().bench->min_audio = tt;
    mpxp_context().bench->cur_audio=tt;
  }
  if(playsize>sh_audio->a_buffer_len) playsize=sh_audio->a_buffer_len;

  if(xmp_test_model(XMP_Run_AudioPlayer)) dec_ahead_audio_delay=mpxp_context().audio().output->get_delay();

  playsize=mpxp_context().audio().output->play(sh_audio->a_buffer,playsize,0);

  if(playsize>0){
      sh_audio->a_buffer_len-=playsize;
      memcpy(sh_audio->a_buffer,&sh_audio->a_buffer[playsize],sh_audio->a_buffer_len);
      if(!mp_conf.av_sync_pts && xmp_test_model(XMP_Run_AudioPlayer))
	  pthread_mutex_lock(&audio_timer_mutex);
      if(mpxp_context().use_pts_fix2) {
	  if(sh_audio->a_pts != HUGE) {
	      sh_audio->a_pts_pos-=playsize;
	      if(sh_audio->a_pts_pos > mpxp_context().audio().output->get_delay()*sh_audio->af_bps) {
		  sh_audio->timer+=playsize/(float)(sh_audio->af_bps);
	      } else {
		  sh_audio->timer=sh_audio->a_pts-(float)sh_audio->a_pts_pos/(float)sh_audio->af_bps;
		  mpxp_v<<"Audio chapter change detected"<<std::endl;
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

any_t* audio_play_routine( any_t* arg )
{
    mpxp_thread_t* priv=reinterpret_cast<mpxp_thread_t*>(arg);
    sh_audio_t* sh_audio=static_cast<sh_audio_t*>(priv->dae->sh);
    Demuxer_Stream *d_audio=sh_audio->ds;
    Demuxer *demuxer=d_audio->demuxer;
    sh_video_t* sh_video=static_cast<sh_video_t*>(demuxer->video->sh);

    int eof = 0;
    struct timeval now;
    float d;
    const float MAX_AUDIO_TIME = (float)mpxp_context().audio().output->get_space() / sh_audio->af_bps + mpxp_context().audio().output->get_delay();
    float min_audio_time = MAX_AUDIO_TIME;
    float min_audio, max_audio;
    int samples, collect_samples;
    float audio_buff_max, audio_buff_norm, audio_buff_min, audio_buff_alert;

    audio_buff_alert = std::max(float(XP_MIN_TIMESLICE), std::min(float(MIN_AUDIO_TIME),MAX_AUDIO_TIME/4));
    audio_buff_max = std::max(audio_buff_alert, std::min(float(MAX_AUDIO_TIME-XP_MIN_TIMESLICE), audio_buff_alert*4));
    audio_buff_min = std::min(audio_buff_max, audio_buff_alert*2);
    audio_buff_norm = (audio_buff_max + audio_buff_min) / 2;

    mpxp_dbg2<<"alert "<<audio_buff_alert<<", min "<<audio_buff_min<<", norm "<<audio_buff_norm<<", max"<<audio_buff_max<<std::endl;

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
	    while(priv->state==Pth_ASleep) yield_timeslice();
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
		mpxp_dbg2<<"To fast, set min_audio_time "<<min_audio_time<<" (delay "<<dec_ahead_audio_delay<<")"<<std::endl;
	    }
	    collect_samples = 1;
	    samples = 5;
	    min_audio = MAX_AUDIO_TIME;
	    max_audio = 0;
	} else if( dec_ahead_audio_delay <= audio_buff_alert ) { /* To slow, sleep shorter */
	    if ( min_audio_time < MAX_AUDIO_TIME ) {
		min_audio_time *= 2.0;
		mpxp_dbg2<<"To slow, set min_audio_time "<<min_audio_time<<" (delay "<<dec_ahead_audio_delay<<")"<<std::endl;
	    }
	    collect_samples = 1;
	    samples = 10;
	    min_audio = MAX_AUDIO_TIME;
	    max_audio = 0;
	} else if( !mpxp_context().engine().xp_core->audio->eof && collect_samples) {
	    if( dec_ahead_audio_delay < min_audio )
		min_audio = dec_ahead_audio_delay;
	    if( dec_ahead_audio_delay > max_audio )
		max_audio = dec_ahead_audio_delay;
	    samples--;

	    if( samples <= 0 ) {
		if( min_audio > audio_buff_max ) {
		    min_audio_time -= min_audio-audio_buff_norm;
		    collect_samples = 1;
		    mpxp_dbg2<<"Decrease min_audio_time "<<min_audio_time<<" (min "<<min_audio<<" max "<<max_audio<<")"<<std::endl;
		} else if( max_audio < audio_buff_min ) {
		    min_audio_time *= 1.25;
		    collect_samples = 1;
		    mpxp_dbg2<<"Increase min_audio_time "<<min_audio_time<<" (min "<<min_audio<<" max "<<max_audio<<")"<<std::endl;
		} else {
		    collect_samples = 0; /* No change, stop */
		    mpxp_dbg2<<"Stop collecting samples time "<<min_audio_time<<" (min "<<min_audio<<" max "<<max_audio<<")"<<std::endl;
		}
		if(collect_samples) {
		    samples = 5;
		    min_audio = MAX_AUDIO_TIME;
		    max_audio = 0;
		}
	    }
	}

	LOCK_AUDIO_PLAY();
	d = mpxp_context().audio().output->get_delay() - min_audio_time;
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
	while( mpxp_context().engine().xp_core->in_pause && priv->state!=Pth_Canceling) {
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
		mpxp_v<<"Audio seek timeout"<<std::endl;
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
    mpxp_dbg2<<"sig_audio_play"<<std::endl;
    mpxp_print_flush();

    dec_ahead_can_aseek=1;

    UNLOCK_AUDIO_PLAY();

    xmp_killall_threads(pthread_self());
    __exit_sighandler();
}

} // namespace mpxp
