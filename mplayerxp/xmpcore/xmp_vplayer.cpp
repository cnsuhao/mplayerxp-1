#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
#include <stdio.h>
#include <math.h>

#include "version.h"
#define HELP_MPXP_DEFINE_STATIC
#include "mpxp_help.h"
#include "player_msg.h"
#include "sig_hand.h"

#include "libao3/audio_out.h"
#include "libvo2/video_out.h"

#include "libmpdemux/demuxer_r.h"
#include "osdep/timer.h"
#include "mplayerxp.h"
#include "xmp_core.h"
#include "xmp_adecoder.h"
#include "xmp_vplayer.h"

float max_pts_correction=0;
namespace mpxp {

static void __show_status_line(float a_pts,float v_pts,float delay,float AV_delay) {
    mpxp_status <<"A:"<<(a_pts-delay)
		<<" V:"<<v_pts
		<<" A-V:"<<AV_delay
		<<" "<<mpxp_context().engine().xp_core->video->num_played_frames
		<<"/"<<mpxp_context().engine().xp_core->video->num_decoded_frames
		<<" "<<((v_pts>0.5)?(int)(100.0*mpxp_context().bench->video/(double)v_pts):0)<<"%"
		<<" "<<((v_pts>0.5)?(int)(100.0*mpxp_context().bench->vout/(double)v_pts):0)<<"%"
		<<" "<<((v_pts>0.5)?(100.0*(mpxp_context().bench->audio+mpxp_context().bench->audio_decode)/(double)v_pts):0)<<"%"
		<<" "<<mpxp_context().output_quality<<"%"
		<<" [frms: ["<<dae_curr_vplayed(mpxp_context().engine().xp_core)<<"]]\r";
    mpxp_status.flush();
}

static void show_status_line_no_apts(sh_audio_t* sh_audio,float v_pts) {
    if(mp_conf.av_sync_pts && sh_audio && (!mpxp_context().engine().xp_core->audio->eof || mpxp_context().audio().output->get_delay())) {
	float a_pts = sh_audio->timer-mpxp_context().audio().output->get_delay();
	mpxp_status<<"A:"<<a_pts
	<<" V:"<<v_pts
	<<" A-V:"<<(a_pts-v_pts)
	<<" ct: "<<mpxp_context().engine().xp_core->video->num_played_frames
	<<"/"<<mpxp_context().engine().xp_core->video->num_decoded_frames
	<<"  "<<((v_pts>0.5)?(int)(100.0*mpxp_context().bench->video/(double)v_pts):0)<<"%"
	<<" "<<((v_pts>0.5)?(int)(100.0*mpxp_context().bench->vout/(double)v_pts):0)<<"%"
	<<" "<<((v_pts>0.5)?(100.0*(mpxp_context().bench->audio+mpxp_context().bench->audio_decode)/(double)v_pts):0)<<"%"
	<<" "<<mpxp_context().output_quality<<"\r";
    } else
	mpxp_status<<"V:"<<v_pts
	<<" "<<mpxp_context().engine().xp_core->video->num_played_frames
	<<" "<<((v_pts>0.5)?(int)(100.0*mpxp_context().bench->video/(double)v_pts):0)<<"%"
	<<" "<<((v_pts>0.5)?(int)(100.0*mpxp_context().bench->vout/(double)v_pts):0)<<"%"
	<<" "<<((v_pts>0.5)?(100.0*(mpxp_context().bench->audio+mpxp_context().bench->audio_decode)/(double)v_pts):0)<<"%"
	<<" "<<mpxp_context().output_quality<<"\r";
    mpxp_status.flush();
}

static void vplayer_check_chapter_change(sh_audio_t* sh_audio,sh_video_t* sh_video,xmp_frame_t* shva_prev,float v_pts)
{
    if(mpxp_context().use_pts_fix2 && sh_audio) {
	if(sh_video->chapter_change == -1) { /* First frame after seek */
	    while(v_pts < 1.0 && sh_audio->timer==0.0 && mpxp_context().audio().output->get_delay()==0.0)
		yield_timeslice();	 /* Wait for audio to start play */
	    if(sh_audio->timer > 2.0 && v_pts < 1.0) {
		mpxp_v<<"Video chapter change detected"<<std::endl;
		sh_video->chapter_change=1;
	    } else {
		sh_video->chapter_change=0;
	    }
	} else if(v_pts < 1.0 && shva_prev->v_pts > 2.0) {
	    mpxp_v<<"Video chapter change detected"<<std::endl;
	    sh_video->chapter_change=1;
	}
	if(sh_video->chapter_change && sh_audio->chapter_change) {
	    mpxp_v<<"Reset chapter change"<<std::endl;
	    sh_video->chapter_change=0;
	    sh_audio->chapter_change=0;
	}
    }
}

static float vplayer_compute_sleep_time(sh_audio_t* sh_audio,sh_video_t* sh_video,xmp_frame_t* shva_prev,float screen_pts)
{
    float sleep_time=0;
    if(sh_audio && mpxp_context().engine().xp_core->audio) {
	/* FIXME!!! need the same technique to detect mpxp_context().engine().xp_core->audio->eof as for video_eof!
	   often ao_get_delay() never returns 0 :( */
	if(mpxp_context().engine().xp_core->audio->eof && !get_delay_audio_buffer()) goto nosound_model;
	if((!mpxp_context().engine().xp_core->audio->eof || mpxp_context().audio().output->get_delay()) &&
	(!mpxp_context().use_pts_fix2 || (!sh_audio->chapter_change && !sh_video->chapter_change)))
	    sleep_time=screen_pts-((sh_audio->timer-mpxp_context().audio().output->get_delay())
				+(mp_conf.av_sync_pts?0:mpxp_context().engine().xp_core->initial_apts));
	else if(mpxp_context().use_pts_fix2 && sh_audio->chapter_change)
	    sleep_time=0;
	else
	    goto nosound_model;
    } else {
	nosound_model:
	sleep_time=shva_prev->duration/mp_conf.playbackspeed_factor;
    }
    return sleep_time;
}

static const float XP_MIN_TIMESLICE=0.010f; /* under Linux on x86 min time_slice = 10 ms */
static const float XP_MIN_AUDIOBUFF=0.05f;
static const float XP_MAX_TIMESLICE=0.1f;
static int vplayer_do_sleep(sh_audio_t* sh_audio,int rtc_fd,float sleep_time)
{
    if(!mpxp_context().engine().xp_core->audio) sh_audio=NULL;
    if(sh_audio && (!mpxp_context().engine().xp_core->audio->eof || mpxp_context().audio().output->get_delay()) && sleep_time>XP_MAX_TIMESLICE) {
	float t;

	if(xmp_test_model(XMP_Run_AudioPlayback)) {
	    t=mpxp_context().audio().output->get_delay()-XP_MIN_AUDIOBUFF;
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

static int mpxp_play_video(Demuxer* demuxer,sh_audio_t* sh_audio,sh_video_t*sh_video)
{
    Demuxer_Stream *d_audio=demuxer->audio;
    float v_pts=0;
    float sleep_time=0;
    int can_blit=0;
    int delay_corrected=1;
    int final_frame=0;
    xmp_frame_t shva_prev,shva;

    shva_prev=dae_played_frame(mpxp_context().engine().xp_core->video);
    final_frame = dae_played_eof(mpxp_context().engine().xp_core->video);
    if(mpxp_context().engine().xp_core->video->eof && final_frame) return 1;

    can_blit=dae_try_inc_played(mpxp_context().engine().xp_core->video); /* <-- TRY SWITCH TO NEXT FRAME */
    shva=dae_next_played_frame(mpxp_context().engine().xp_core->video);
    v_pts = shva.v_pts;
    /*------------------------ frame decoded. --------------------*/
/* blit frame */

    if(mpxp_context().engine().xp_core->video->eof) can_blit=1; /* force blitting until end of stream will be reached */
    vplayer_check_chapter_change(sh_audio,sh_video,&shva_prev,v_pts);
    /*--------- add OSD to the next frame contents ---------*/
    if(can_blit) {
#ifdef USE_OSD
	MP_UNIT("draw_osd");
	update_osd(shva.v_pts);
	mpxp_context().video().output->draw_osd(dae_next_played(mpxp_context().engine().xp_core->video));
#endif
    }
    MP_UNIT("change_frame2");
    /* don't flip if there is nothing new to display */
    if(!can_blit) {
	static int drop_message=0;
	if(!drop_message && mpxp_context().engine().xp_core->video->num_slow_frames > 50) {
		drop_message=1;
		if(mpxp_context().mpxp_after_seek) mpxp_context().mpxp_after_seek--;
		else
		    for(unsigned j=0;MSGTR_SystemTooSlow[j];j++) mpxp_warn<<MSGTR_SystemTooSlow[j]<<std::endl;
	}
	mpxp_dbg2<<std::endl<<"dec_ahead_main: stalling: "<<dae_curr_vplayed(mpxp_context().engine().xp_core)<<" "<<dae_curr_vdecoded(mpxp_context().engine().xp_core)<<std::endl;
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

	if(!(mpxp_context().video().output->flags&256)){ /* flag 256 means: libvo driver does its timing (dvb card) */
	    if(!vplayer_do_sleep(sh_audio,mpxp_context().rtc_fd,sleep_time)) return 0;
	}

	player_idx=dae_next_played(mpxp_context().engine().xp_core->video);
	mpxp_context().video().output->select_frame(player_idx);
	dae_inc_played(mpxp_context().engine().xp_core->video);
	mpxp_dbg2<<std::endl<<"dec_ahead_main: schedule "<<player_idx<<" on screen"<<std::endl;
	t2=GetTimer()-t2;
	tt = t2*0.000001f;
	mpxp_context().bench->vout+=tt;
	if(mp_conf.benchmark) {
	    /* we need compute draw_slice+change_frame here */
	    mpxp_context().bench->cur_vout+=tt;
	}
    }
    MP_UNIT(NULL);

/*================ A-V TIMESTAMP CORRECTION: =========================*/
  /* FIXME: this block was added to fix A-V resync caused by some strange things
     like playing 48KHz audio on 44.1KHz soundcard and other.
     Now we know PTS of every audio frame so don't need to have it */
  if(!mpxp_context().engine().xp_core->audio) sh_audio=NULL;
  if(sh_audio && (!mpxp_context().engine().xp_core->audio->eof || mpxp_context().audio().output->get_delay()) && !mp_conf.av_sync_pts) {
    float a_pts=0;

    // unplayed bytes in our and soundcard/dma buffer:
    float delay=mpxp_context().audio().output->get_delay()+(float)sh_audio->a_buffer_len/(float)sh_audio->af_bps;
    if(xmp_test_model(XMP_Run_AudioPlayer))
	delay += get_delay_audio_buffer();

    // PTS = (last timestamp) + (bytes after last timestamp)/(bytes per sec)
    a_pts=d_audio->pts;
    if(!delay_corrected) if(a_pts) delay_corrected=1;
    a_pts+=(ds_tell_pts_r(d_audio)-sh_audio->a_in_buffer_len)/(float)sh_audio->i_bps;

    mpxp_dbg2<<"### A:"<<a_pts<<" ("<<(a_pts-delay)<<")  V:"<<v_pts<<"  A-V:"<<((a_pts-delay)-v_pts)<<std::endl;

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
    sh_video_t* sh_video=static_cast<sh_video_t*>(priv->dae->sh);
    Demuxer_Stream *d_video=sh_video->ds;
    Demuxer *demuxer=d_video->demuxer;
    sh_audio_t* sh_audio=static_cast<sh_audio_t*>(demuxer->audio->sh);

    priv->state=Pth_Run;
    priv->dae->eof = 0;
    if(mpxp_context().engine().xp_core->audio) mpxp_context().engine().xp_core->audio->eof=0;
    mpxp_dbg2<<std::endl<<"DEC_AHEAD: entering..."<<std::endl;
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
  mpxp_dbg2<<"DEC_AHEAD: leaving..."<<std::endl;
  priv->state=Pth_Stand;
  return arg; /* terminate thread here !!! */
}

void sig_video_play( void )
{
    mpxp_dbg2<<"sig_audio_play"<<std::endl;
    mpxp_print_flush();

    dec_ahead_can_aseek=1;

    xmp_killall_threads(pthread_self());
    __exit_sighandler();
}

} // namespace mpxp
