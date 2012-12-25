#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
#include <stdio.h>
#include <math.h>

#include "libmpcodecs/dec_video.h"

#include "sig_hand.h"
#include "player_msg.h"
#include "mplayerxp.h"
#include "osdep/timer.h"
#include "xmp_core.h"
#include "xmp_adecoder.h"
#include "xmp_vdecoder.h"

namespace mpxp {
/* this routine decodes video+audio but intends to be video only  */

static void show_warn_cant_sync(sh_video_t*sh_video,float max_frame_delay) {
    static int warned=0;
    static float prev_warn_delay=0;
    if(!warned || max_frame_delay > prev_warn_delay) {
	warned=1;
	mpxp_warn<<"*********************************************"<<std::endl;
	mpxp_warn<<"** Can't stabilize A-V sync!!!             **"<<std::endl;
	mpxp_warn<<"*********************************************"<<std::endl;
	mpxp_warn<<"Try increase number of buffer for decoding ahead"<<std::endl;
	mpxp_warn<<"Exist: "<<mpxp_context().engine().xp_core->num_v_buffs<<", need: "<<((unsigned)(max_frame_delay*3*sh_video->fps)+3)<<std::endl;
	prev_warn_delay=max_frame_delay;
    }
}

static unsigned compute_frame_dropping(sh_video_t* sh_video,float v_pts,float drop_barrier) {
    unsigned rc=0;
    float screen_pts=dae_played_frame(mpxp_context().engine().xp_core->video).v_pts-(mp_conf.av_sync_pts?0:mpxp_context().engine().xp_core->initial_apts);
    static float prev_delta=64;
    float delta,max_frame_delay;/* delay for decoding of top slow frame */
    max_frame_delay = mpxp_context().bench->max_video+mpxp_context().bench->max_vout;

    /*
	TODO:
	    Replace the constants with some values which are depended on
	    mpxp_context().engine().xp_core->num_v_buffs and max_frame_delay to find out the smoothest way
	    to display frames on slow machines.
	MAYBE!!!: (won't work with some realmedia streams for example)
	    Try to borrow avifile's logic (btw, GPL'ed ;) for very slow systems:
	    - fill a full buffer (is not always reachable)
	    - while(video.pts < audio.pts)
		video.seek_to_key_frame(video.get_next_key_frame(video.get_cur_pos()))
    */
    delta=v_pts-screen_pts;
    if(max_frame_delay*3 > drop_barrier) {
	if(drop_barrier < (float)(mpxp_context().engine().xp_core->num_v_buffs-2)/sh_video->fps) drop_barrier += 1/sh_video->fps;
	else
	if(mp_conf.verbose) show_warn_cant_sync(sh_video,max_frame_delay);
    }
    if(delta > drop_barrier) rc=0;
    else if(delta < max_frame_delay*3) rc=1;
    else {
	unsigned fr_skip_divisor;
	/*
	if(delta < drop_barrier/4) fr_skip_divisor=1; -- drop every frame is not smooth thing
	else
	*/
	if(delta < drop_barrier/2) fr_skip_divisor=2;
	else
	if(delta < drop_barrier*2/3) fr_skip_divisor=3;
	else
	fr_skip_divisor=4; /* delta < drop_barrier */
	rc = (dae_curr_vdecoded(mpxp_context().engine().xp_core)%fr_skip_divisor)?0:1;
	if(delta>prev_delta) rc=0;
    }
    mpxp_dbg2<<"DEC_AHEAD: max_frame_delay*3="<<(max_frame_delay*3)
	    <<" drop_barrier="<<drop_barrier
	    <<" prev_delta="<<prev_delta
	    <<" delta="<<delta
	    <<"(v_pts="<<v_pts<<" screen_pts="<<screen_pts
	    <<") n_fr_to_drop="<<rc<<std::endl;
    prev_delta=delta;
    return rc;
}

static void reorder_pts_in_mpeg(void) {
    unsigned idx0=0, idx1, idx2, idx3;

    idx1 = dae_curr_vdecoded(mpxp_context().engine().xp_core);
    idx2 = dae_prev_vdecoded(mpxp_context().engine().xp_core);
    xmp_frame_t* fra=mpxp_context().engine().xp_core->video->frame;
    while( dae_curr_vplayed(mpxp_context().engine().xp_core) != idx2 &&
	   fra[idx2].v_pts > fra[idx1].v_pts &&
	   fra[idx2].v_pts < fra[idx1].v_pts+1.0 ) {
	float tmp;
	tmp = fra[idx1].v_pts;
	fra[idx1].v_pts = fra[idx2].v_pts;
	fra[idx2].v_pts = tmp;

	fra[idx2].duration =   fra[idx1].v_pts - fra[idx2].v_pts;

	idx3=(idx2-1)%mpxp_context().engine().xp_core->num_v_buffs;
	if(fra[idx2].v_pts > fra[idx3].v_pts &&
	   fra[idx2].v_pts - fra[idx3].v_pts < 1.0)
		fra[idx3].duration = fra[idx2].v_pts - fra[idx3].v_pts;

	if(idx1 != dae_curr_vdecoded(mpxp_context().engine().xp_core)) fra[idx1].duration = fra[idx0].v_pts - fra[idx1].v_pts;

	idx0 = idx1;
	idx1 = idx2;
	idx2=(idx2-1)%mpxp_context().engine().xp_core->num_v_buffs;
    }
}

any_t* xmp_video_decoder( any_t* arg )
{
    mpxp_thread_t* priv=reinterpret_cast<mpxp_thread_t*>(arg);
    sh_video_t* sh_video=static_cast<sh_video_t*>(priv->dae->sh);
    Demuxer_Stream *d_video=sh_video->ds;
    Demuxer* demuxer=d_video->demuxer;
    Demuxer_Stream* d_audio=demuxer->audio;
    enc_frame_t* frame;

    float duration=0;
    float drop_barrier;
    int blit_frame=0;
    int drop_param=0;
    unsigned xp_n_frame_to_drop;
    float mpeg_timer=HUGE;

    priv->state=Pth_Run;
    priv->dae->eof = 0;
    if(mpxp_context().engine().xp_core->audio) mpxp_context().engine().xp_core->audio->eof=0;
    mpxp_dbg2<<std::endl<<"DEC_AHEAD: entering..."<<std::endl;
    __MP_UNIT(priv->p_idx,"dec_ahead");
    priv->pid = getpid();
    if(!xmp_test_model(XMP_Run_VA_Decoder) && mpxp_context().engine().xp_core->audio)
	priv->name = "video decoder";
    drop_barrier=(float)(mpxp_context().engine().xp_core->num_v_buffs/2)*(1/sh_video->fps);
    if(mp_conf.av_sync_pts == -1 && !mpxp_context().use_pts_fix2)
	mpxp_context().engine().xp_core->bad_pts = d_video->demuxer->file_format == Demuxer::Type_MPEG_ES ||
			d_video->demuxer->file_format == Demuxer::Type_MPEG4_ES ||
			d_video->demuxer->file_format == Demuxer::Type_H264_ES ||
			d_video->demuxer->file_format == Demuxer::Type_MPEG_PS ||
			d_video->demuxer->file_format == Demuxer::Type_MPEG_TS;
    else
	mpxp_context().engine().xp_core->bad_pts = mp_conf.av_sync_pts?0:1;
while(!priv->dae->eof){
    if(priv->state==Pth_Canceling) break;
    if(priv->state==Pth_Sleep) {
pt_sleep:
	priv->state=Pth_ASleep;
	while(priv->state==Pth_ASleep) yield_timeslice();
	if(mpxp_context().engine().xp_core->bad_pts) mpeg_timer=HUGE;
	continue;
    }
    __MP_UNIT(priv->p_idx,"dec_ahead 1");
/* get it! */
#if 0
    /* prevent reent access to non-reent demuxer */
    //if(sh_video->num_frames>200)  *((char*)0x100) = 1; // Testing crash
    if(mpxp_context().engine().xp_core->audio && mp_conf.xp<XP_VAFull) {
	__MP_UNIT(priv->p_idx,"decode audio");
	while(2==xp_thread_decode_audio()) ;
	__MP_UNIT(priv->p_idx,"dec_ahead 2");
    }
#endif
/*--------------------  Decode a frame: -----------------------*/
    frame=video_read_frame_r(sh_video,sh_video->fps);
    if(!frame) {
	pt_exit_loop:
	dae_decoded_mark_eof(mpxp_context().engine().xp_core->video);
	priv->dae->eof=1;
	break;
    }
    if(mp_conf.play_n_frames>0 && mpxp_context().engine().xp_core->video->num_decoded_frames >= mp_conf.play_n_frames) goto pt_exit_loop;
    /* frame was decoded into current decoder_idx */
    if(mpxp_context().engine().xp_core->bad_pts) {
	if(mpeg_timer==HUGE) mpeg_timer=frame->pts;
	else if( mpeg_timer-duration<frame->pts ) {
	    mpeg_timer=frame->pts;
	    mpxp_dbg2<<"Sync mpeg pts "<<mpeg_timer<<std::endl;
	}
	else mpeg_timer+=frame->duration;
    }
    /* compute frame dropping */
    xp_n_frame_to_drop=0;
    if(mp_conf.frame_dropping) {
	int cur_time;
	cur_time = GetTimerMS();
	/* Ugly solution: disable frame dropping right after seeking! */
	if(cur_time - mpxp_context().seek_time > (mpxp_context().engine().xp_core->num_v_buffs/sh_video->fps)*100) xp_n_frame_to_drop=compute_frame_dropping(sh_video,frame->pts,drop_barrier);
    } /* if( mp_conf.frame_dropping ) */
    if(!finite(frame->pts)) mpxp_warn<<"Bug of demuxer! Value of video pts="<<frame->pts<<std::endl;
    if(frame->type!=VideoFrame) escape_player("VideoDecoder doesn't parse non video frames",mp_conf.max_trace);
#if 0
/*
    We can't seriously examine question of too slow machines
    by motivation reasons
*/
if(ada_active_frame) /* don't emulate slow systems until xp_players are not started */
{
    int i,delay; /* sleeping 200 ms is far enough for 25 fps */
    delay=xp_n_frame_to_drop?0:20;
    for(i=0;i<delay;i++) yield_timeslice();
}
#endif
    if(xp_n_frame_to_drop)	drop_param=mp_conf.frame_dropping;
    else			drop_param=0;
    /* decode: */
    if(mpxp_context().output_quality) {
	unsigned total = mpxp_context().engine().xp_core->num_v_buffs/2;
	unsigned distance = dae_get_decoder_outrun(mpxp_context().engine().xp_core->video);
	int our_quality;
	our_quality = mpxp_context().output_quality*distance/total;
	if(drop_param) mpcv_set_quality(*mpxp_context().video().decoder,0);
	else
	if(mp_conf.autoq) mpcv_set_quality(*mpxp_context().video().decoder,our_quality>0?our_quality:0);
    }
    frame->flags=drop_param;
    blit_frame=mpcv_decode(*mpxp_context().video().decoder,*frame);
    mpxp_dbg2<<"DECODER: "<<dae_curr_vdecoded(mpxp_context().engine().xp_core)<<"["<<frame->len<<"] "<<frame->pts<<std::endl;
    if(mpxp_context().output_quality) {
	if(drop_param) mpcv_set_quality(*mpxp_context().video().decoder,mpxp_context().output_quality);
    }
    if(!blit_frame && drop_param) priv->dae->num_dropped_frames++;
    if(blit_frame) {
	unsigned idx=dae_curr_vdecoded(mpxp_context().engine().xp_core);
	if(mpxp_context().engine().xp_core->bad_pts)
	    mpxp_context().engine().xp_core->video->frame[idx].v_pts=mpeg_timer;
	else
	    mpxp_context().engine().xp_core->video->frame[idx].v_pts = frame->pts;
	mpxp_context().engine().xp_core->video->frame[idx].duration=duration;
	dae_decoded_clear_eof(mpxp_context().engine().xp_core->video);
	if(!mpxp_context().engine().xp_core->bad_pts) {
	    int _idx = dae_prev_vdecoded(mpxp_context().engine().xp_core);
	    mpxp_context().engine().xp_core->video->frame[_idx].duration=frame->pts-mpxp_context().engine().xp_core->video->frame[_idx].v_pts;
	}
	if(mp_conf.frame_reorder) reorder_pts_in_mpeg();
    } /* if (blit_frame) */

    /* ------------ sleep --------------- */
    /* sleep if thread is too fast ;) */
    if(blit_frame)
    while(!dae_inc_decoded(mpxp_context().engine().xp_core->video)) {
	if(priv->state==Pth_Canceling) goto pt_exit;
	if(priv->state==Pth_Sleep) goto pt_sleep;
	if(mpxp_context().engine().xp_core->audio && xmp_test_model(XMP_Run_VA_Decoder)) {
	    __MP_UNIT(priv->p_idx,"decode audio");
	    xp_thread_decode_audio(d_audio);
	    __MP_UNIT(priv->p_idx,"dec_ahead 5");
	}
	yield_timeslice();
    }
    free_enc_frame(frame);
/*------------------------ frame decoded. --------------------*/
} /* while(!priv->dae->eof)*/

if(mpxp_context().engine().xp_core->audio && xmp_test_model(XMP_Run_VA_Decoder)) {
    while(!mpxp_context().engine().xp_core->audio->eof && priv->state!=Pth_Canceling && priv->state!=Pth_Sleep) {
	__MP_UNIT(priv->p_idx,"decode audio");
	if(!xp_thread_decode_audio(d_audio)) yield_timeslice();
	__MP_UNIT(priv->p_idx,NULL);
    }
}
  pt_exit:
  mpxp_dbg2<<std::endl<<"DEC_AHEAD: leaving..."<<std::endl;
  priv->state=Pth_Stand;
  return arg; /* terminate thread here !!! */
}

void sig_video_decode( void )
{
    mpxp_dbg2<<"sig_video_decode"<<std::endl;
    mpxp_print_flush();

    mpxp_context().engine().xp_core->video->eof = 1;
    dae_decoded_mark_eof(mpxp_context().engine().xp_core->video);
    /*
	Unlock all mutex
	( man page says it may deadlock, but what is worse deadlock here or later? )
    */
    xmp_killall_threads(pthread_self());
    __exit_sighandler();
}

} // namespace mpxp
