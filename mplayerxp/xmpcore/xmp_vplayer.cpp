#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
#include <stdio.h>
#include <math.h>

#include "help_mp.h"
#include "mp_msg.h"
#include "sig_hand.h"

#include "libao2/audio_out.h"
#include "libvo/video_out.h"

#include "osdep/timer.h"
#include "libmpdemux/demuxer.h"
#include "mplayerxp.h"
#include "xmp_core.h"
#include "xmp_adecoder.h"
#include "xmp_vplayer.h"

float max_pts_correction=0;
namespace mpxp {

#ifdef ENABLE_DEC_AHEAD_DEBUG
#define MSG_T(args...) mp_msg(MSGT_GLOBAL, MSGL_DBG2,__FILE__,__LINE__, ## args )
#else
#define MSG_T(args...)
#endif

static void __show_status_line(float a_pts,float v_pts,float delay,float AV_delay) {
    MSG_STATUS("A:%6.1f V:%6.1f A-V:%7.3f %3d/%3d  %2d%% %2d%% %4.1f%% %d [frms: [%i]]\n",
		a_pts-delay,v_pts,AV_delay
		,xp_core->video->num_played_frames,xp_core->video->num_decoded_frames
		,(v_pts>0.5)?(int)(100.0*MPXPCtx->bench->video/(double)v_pts):0
		,(v_pts>0.5)?(int)(100.0*MPXPCtx->bench->vout/(double)v_pts):0
		,(v_pts>0.5)?(100.0*(MPXPCtx->bench->audio+MPXPCtx->bench->audio_decode)/(double)v_pts):0
		,MPXPCtx->output_quality
		,dae_curr_vplayed(xp_core)
		);
    fflush(stdout);
}

static void show_status_line_no_apts(sh_audio_t* sh_audio,float v_pts) {
    if(mp_conf.av_sync_pts && sh_audio && (!xp_core->audio->eof || ao_get_delay(ao_data))) {
	float a_pts = sh_audio->timer-ao_get_delay(ao_data);
	MSG_STATUS("A:%6.1f V:%6.1f A-V:%7.3f ct:%7.3f  %3d/%3d  %2d%% %2d%% %4.1f%% %d\r"
	,a_pts
	,v_pts
	,a_pts-v_pts
	,0.0
	,xp_core->video->num_played_frames,xp_core->video->num_decoded_frames
	,(v_pts>0.5)?(int)(100.0*MPXPCtx->bench->video/(double)v_pts):0
	,(v_pts>0.5)?(int)(100.0*MPXPCtx->bench->vout/(double)v_pts):0
	,(v_pts>0.5)?(100.0*(MPXPCtx->bench->audio+MPXPCtx->bench->audio_decode)/(double)v_pts):0
	,MPXPCtx->output_quality
	);
    } else
	MSG_STATUS("V:%6.1f  %3d  %2d%% %2d%% %4.1f%% %d\r"
	,v_pts
	,xp_core->video->num_played_frames
	,(v_pts>0.5)?(int)(100.0*MPXPCtx->bench->video/(double)v_pts):0
	,(v_pts>0.5)?(int)(100.0*MPXPCtx->bench->vout/(double)v_pts):0
	,(v_pts>0.5)?(100.0*(MPXPCtx->bench->audio+MPXPCtx->bench->audio_decode)/(double)v_pts):0
	,MPXPCtx->output_quality
	);
    fflush(stdout);
}

static void vplayer_check_chapter_change(sh_audio_t* sh_audio,sh_video_t* sh_video,xmp_frame_t* shva_prev,float v_pts)
{
    if(MPXPCtx->use_pts_fix2 && sh_audio) {
	if(sh_video->chapter_change == -1) { /* First frame after seek */
	    while(v_pts < 1.0 && sh_audio->timer==0.0 && ao_get_delay(ao_data)==0.0)
		yield_timeslice();	 /* Wait for audio to start play */
	    if(sh_audio->timer > 2.0 && v_pts < 1.0) {
		MSG_V("Video chapter change detected\n");
		sh_video->chapter_change=1;
	    } else {
		sh_video->chapter_change=0;
	    }
	} else if(v_pts < 1.0 && shva_prev->v_pts > 2.0) {
	    MSG_V("Video chapter change detected\n");
	    sh_video->chapter_change=1;
	}
	if(sh_video->chapter_change && sh_audio->chapter_change) {
	    MSG_V("Reset chapter change\n");
	    sh_video->chapter_change=0;
	    sh_audio->chapter_change=0;
	}
    }
}

static float vplayer_compute_sleep_time(sh_audio_t* sh_audio,sh_video_t* sh_video,xmp_frame_t* shva_prev,float screen_pts)
{
    float sleep_time=0;
    if(sh_audio && xp_core->audio) {
	/* FIXME!!! need the same technique to detect xp_core->audio->eof as for video_eof!
	   often ao_get_delay() never returns 0 :( */
	if(xp_core->audio->eof && !get_delay_audio_buffer()) goto nosound_model;
	if((!xp_core->audio->eof || ao_get_delay(ao_data)) &&
	(!MPXPCtx->use_pts_fix2 || (!sh_audio->chapter_change && !sh_video->chapter_change)))
	    sleep_time=screen_pts-((sh_audio->timer-ao_get_delay(ao_data))
				+(mp_conf.av_sync_pts?0:xp_core->initial_apts));
	else if(MPXPCtx->use_pts_fix2 && sh_audio->chapter_change)
	    sleep_time=0;
	else
	    goto nosound_model;
    } else {
	nosound_model:
	sleep_time=shva_prev->duration/mp_conf.playbackspeed_factor;
    }
    return sleep_time;
}

static int vplayer_do_sleep(sh_audio_t* sh_audio,int rtc_fd,float sleep_time)
{
#define XP_MIN_TIMESLICE 0.010 /* under Linux on x86 min time_slice = 10 ms */
#define XP_MIN_AUDIOBUFF 0.05
#define XP_MAX_TIMESLICE 0.1
    if(!xp_core->audio) sh_audio=NULL;
    if(sh_audio && (!xp_core->audio->eof || ao_get_delay(ao_data)) && sleep_time>XP_MAX_TIMESLICE) {
	float t;

	if(xmp_test_model(XMP_Run_AudioPlayback)) {
	    t=ao_get_delay(ao_data)-XP_MIN_AUDIOBUFF;
	    if(t>XP_MAX_TIMESLICE)
		t=XP_MAX_TIMESLICE;
	} else
		t = XP_MAX_TIMESLICE;

	usleep(t*1000000);
	sleep_time-=GetRelativeTime();
	if(xmp_test_model(XMP_Run_AudioPlayer) || t<XP_MAX_TIMESLICE || sleep_time>XP_MAX_TIMESLICE) {
	    // exit due no sound in soundcard
	    return 0;
	}
    }

    while(sleep_time>XP_MIN_TIMESLICE) {
	yield_timeslice();
	sleep_time-=GetRelativeTime();
    }
    MP_UNIT("sleep_usleep");
    sleep_time=SleepTime(rtc_fd,mp_conf.softsleep,sleep_time);
    return 1;
}

static int mpxp_play_video(demuxer_t* demuxer,sh_audio_t* sh_audio,sh_video_t*sh_video)
{
    demux_stream_t *d_audio=demuxer->audio;
    float v_pts=0;
    float sleep_time=0;
    int can_blit=0;
    int delay_corrected=1;
    int final_frame=0;
    xmp_frame_t shva_prev,shva;

    shva_prev=dae_played_frame(xp_core->video);
    final_frame = dae_played_eof(xp_core->video);
    if(xp_core->video->eof && final_frame) return 1;

    can_blit=dae_try_inc_played(xp_core->video); /* <-- TRY SWITCH TO NEXT FRAME */
    shva=dae_next_played_frame(xp_core->video);
    v_pts = shva.v_pts;
    /*------------------------ frame decoded. --------------------*/
/* blit frame */

    if(xp_core->video->eof) can_blit=1; /* force blitting until end of stream will be reached */
    vplayer_check_chapter_change(sh_audio,sh_video,&shva_prev,v_pts);
#if 0
MSG_INFO("xp_core->initial_apts=%f a_eof=%i a_pts=%f sh_audio->timer=%f v_pts=%f stream_pts=%f duration=%f\n"
,xp_core->initial_apts
,xp_core->audio->eof
,sh_audio && !xp_core->audio->eof?d_audio->pts+(ds_tell_pts_r(d_audio)-sh_audio->a_in_buffer_len)/(float)sh_audio->i_bps:0
,sh_audio && !xp_core->audio->eof?sh_audio->timer-ao_get_delay(ao_data):0
,shva.v_pts
,shva.stream_pts
,shva.duration);
#endif
    /*--------- add OSD to the next frame contents ---------*/
    if(can_blit) {
#ifdef USE_OSD
	MSG_D("dec_ahead_main: draw_osd to %u\n",player_idx);
	MP_UNIT("draw_osd");
	update_osd(shva.v_pts);
	vo_data->draw_osd(dae_next_played(xp_core->video));
#endif
    }
    MP_UNIT("change_frame2");
    /* don't flip if there is nothing new to display */
    if(!can_blit) {
	static int drop_message=0;
	if(!drop_message && xp_core->video->num_slow_frames > 50) {
		drop_message=1;
		if(MPXPCtx->mpxp_after_seek) MPXPCtx->mpxp_after_seek--;
		else			  MSG_WARN(MSGTR_SystemTooSlow);
	}
	MSG_D("\ndec_ahead_main: stalling: %i %i\n",dae_cuurr_vplayed(),dae_curr_decoded());
	/* Don't burn CPU here! With using of v_pts for A-V sync we will enter
	   xp_decore_video without any delay (like while(1);)
	   Sleeping for 10 ms doesn't matter with frame dropping */
	yield_timeslice();
    } else {
	unsigned int t2=GetTimer();
	double tt;
	unsigned player_idx;

	/* It's time to sleep ;)...*/
	MP_UNIT("sleep");
	GetRelativeTime(); /* reset timer */
	sleep_time=vplayer_compute_sleep_time(sh_audio,sh_video,&shva_prev,v_pts);

	if(!(vo_data->flags&256)){ /* flag 256 means: libvo driver does its timing (dvb card) */
	    if(!vplayer_do_sleep(sh_audio,MPXPCtx->rtc_fd,sleep_time)) return 0;
	}

	player_idx=dae_next_played(xp_core->video);
	vo_data->select_frame(player_idx);
	dae_inc_played(xp_core->video);
	MSG_D("\ndec_ahead_main: schedule %u on screen\n",player_idx);
	t2=GetTimer()-t2;
	tt = t2*0.000001f;
	MPXPCtx->bench->vout+=tt;
	if(mp_conf.benchmark) {
	    /* we need compute draw_slice+change_frame here */
	    MPXPCtx->bench->cur_vout+=tt;
	}
    }
    MP_UNIT(NULL);

/*================ A-V TIMESTAMP CORRECTION: =========================*/
  /* FIXME: this block was added to fix A-V resync caused by some strange things
     like playing 48KHz audio on 44.1KHz soundcard and other.
     Now we know PTS of every audio frame so don't need to have it */
  if(!xp_core->audio) sh_audio=NULL;
  if(sh_audio && (!xp_core->audio->eof || ao_get_delay(ao_data)) && !mp_conf.av_sync_pts) {
    float a_pts=0;

    // unplayed bytes in our and soundcard/dma buffer:
    float delay=ao_get_delay(ao_data)+(float)sh_audio->a_buffer_len/(float)sh_audio->af_bps;
    if(xmp_test_model(XMP_Run_AudioPlayer))
	delay += get_delay_audio_buffer();

    // PTS = (last timestamp) + (bytes after last timestamp)/(bytes per sec)
    a_pts=d_audio->pts;
    if(!delay_corrected) if(a_pts) delay_corrected=1;
    a_pts+=(ds_tell_pts_r(d_audio)-sh_audio->a_in_buffer_len)/(float)sh_audio->i_bps;

    MSG_DBG2("### A:%8.3f (%8.3f)  V:%8.3f  A-V:%7.4f  \n",a_pts,a_pts-delay,v_pts,(a_pts-delay)-v_pts);

    if(delay_corrected && can_blit){
	float AV_delay=0; /* average of A-V timestamp differences */
	float x;
	AV_delay=(a_pts-delay)-v_pts;
	x=AV_delay*0.1f;
	if(x<-max_pts_correction) x=-max_pts_correction;
	else if(x> max_pts_correction) x= max_pts_correction;
	max_pts_correction=shva.duration*0.10; // +-10% of time
	if(xmp_test_model(XMP_Run_AudioPlayer))
	    pthread_mutex_lock(&audio_timer_mutex);
	sh_audio->timer+=x;
	if(xmp_test_model(XMP_Run_AudioPlayer))
	    pthread_mutex_unlock(&audio_timer_mutex);
	if(mp_conf.benchmark && mp_conf.verbose) __show_status_line(a_pts,v_pts,delay,AV_delay);
    }
  } else {
    // No audio or pts:
    if(mp_conf.benchmark && mp_conf.verbose) show_status_line_no_apts(sh_audio,v_pts);
  }
  return 0;
}

any_t* xmp_video_player( any_t* arg )
{
    mpxp_thread_t* priv=reinterpret_cast<mpxp_thread_t*>(arg);
    sh_video_t* sh_video=reinterpret_cast<sh_video_t*>(priv->dae->sh);
    demux_stream_t *d_video=sh_video->ds;
    demuxer_t *demuxer=d_video->demuxer;
    sh_audio_t* sh_audio=reinterpret_cast<sh_audio_t*>(demuxer->audio->sh);

    priv->state=Pth_Run;
    priv->dae->eof = 0;
    if(xp_core->audio) xp_core->audio->eof=0;
    MSG_T("\nDEC_AHEAD: entering...\n");
    __MP_UNIT(priv->p_idx,"dec_ahead");
    priv->pid = getpid();

while(!priv->dae->eof){
    if(priv->state==Pth_Canceling) break;
    if(priv->state==Pth_Sleep) {
	priv->state=Pth_ASleep;
	while(priv->state==Pth_ASleep) yield_timeslice();
	continue;
    }
    __MP_UNIT(priv->p_idx,"play video");

    priv->dae->eof=mpxp_play_video(demuxer,sh_audio,sh_video);
/*------------------------ frame decoded. --------------------*/
} /* while(!priv->dae->eof)*/
  MSG_T("\nDEC_AHEAD: leaving...\n");
  priv->state=Pth_Stand;
  return arg; /* terminate thread here !!! */
}

void sig_video_play( void )
{
    MSG_T("sig_audio_play\n");
    mpxp_print_flush();

    dec_ahead_can_aseek=1;

    xmp_killall_threads(pthread_self());
    __exit_sighandler();
}

} // namespace mpxp
