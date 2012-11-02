#include "mplayer.h"
#include "mp_msg.h"
#include "sig_hand.h"
#include "xmp_core.h"
#include "xmp_adecoder.h"
#include "xmp_vdecoder.h"
#include "osdep/timer.h"
#include "libmpcodecs/dec_video.h"

#include <stdio.h>
#include <unistd.h> // for usleep()
#include <math.h>

#ifdef ENABLE_DEC_AHEAD_DEBUG
#define MSG_T(args...) mp_msg(MSGT_GLOBAL, MSGL_DBG2,__FILE__,__LINE__, ## args )
#else
#define MSG_T(args...)
#endif

/* this routine decodes video+audio but intends to be video only  */

static void show_warn_cant_sync(sh_video_t*sh_video,float max_frame_delay) {
    static int warned=0;
    static float prev_warn_delay=0;
    if(!warned || max_frame_delay > prev_warn_delay) {
	warned=1;
	MSG_WARN("*********************************************\n"
		     "** Can't stabilize A-V sync!!!             **\n"
		     "*********************************************\n"
		     "Try increase number of buffer for decoding ahead\n"
		     "Exist: %u, need: %u\n"
		     ,xp_core->num_v_buffs,(unsigned)(max_frame_delay*3*sh_video->fps)+3);
	prev_warn_delay=max_frame_delay;
    }
}

static unsigned compute_frame_dropping(sh_video_t* sh_video,float v_pts,float drop_barrier) {
    unsigned rc=0;
    float screen_pts=dae_played_frame(xp_core->video).v_pts-(mp_conf.av_sync_pts?0:xp_core->initial_apts);
    static float prev_delta=64;
    float delta,max_frame_delay;/* delay for decoding of top slow frame */
    max_frame_delay = mp_data->bench->max_video+mp_data->bench->max_vout;

    /*
	TODO:
	    Replace the constants with some values which are depended on
	    xp_core->num_v_buffs and max_frame_delay to find out the smoothest way
	    to display frames on slow machines.
	MAYBE!!!: (won't work with some realmedia streams for example)
	    Try to borrow avifile's logic (btw, GPL'ed ;) for very slow systems:
	    - fill a full buffer (is not always reachable)
	    - while(video.pts < audio.pts)
		video.seek_to_key_frame(video.get_next_key_frame(video.get_cur_pos()))
    */
    delta=v_pts-screen_pts;
    if(max_frame_delay*3 > drop_barrier) {
	if(drop_barrier < (float)(xp_core->num_v_buffs-2)/sh_video->fps) drop_barrier += 1/sh_video->fps;
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
	rc = (dae_curr_vdecoded(xp_core)%fr_skip_divisor)?0:1;
	if(delta>prev_delta) rc=0;
    }
    MSG_D("DEC_AHEAD: max_frame_delay*3=%f drop_barrier=%f prev_delta=%f delta=%f(v_pts=%f screen_pts=%f) n_fr_to_drop=%u\n",max_frame_delay*3,drop_barrier,prev_delta,delta,v_pts,xp_screen_pts,xp_n_frame_to_drop);
    prev_delta=delta;
    return rc;
}

static void reorder_pts_in_mpeg(void) {
    unsigned idx0=0, idx1, idx2, idx3;

    idx1 = dae_curr_vdecoded(xp_core);
    idx2 = dae_prev_vdecoded(xp_core);
    xmp_frame_t* fra=xp_core->video->frame;
    while( dae_curr_vplayed(xp_core) != idx2 &&
	   fra[idx2].v_pts > fra[idx1].v_pts &&
	   fra[idx2].v_pts < fra[idx1].v_pts+1.0 ) {
	float tmp;
	tmp = fra[idx1].v_pts;
	fra[idx1].v_pts = fra[idx2].v_pts;
	fra[idx2].v_pts = tmp;

	fra[idx2].duration =   fra[idx1].v_pts - fra[idx2].v_pts;

	idx3=(idx2-1)%xp_core->num_v_buffs;
	if(fra[idx2].v_pts > fra[idx3].v_pts &&
	   fra[idx2].v_pts - fra[idx3].v_pts < 1.0)
		fra[idx3].duration = fra[idx2].v_pts - fra[idx3].v_pts;

	if(idx1 != dae_curr_vdecoded(xp_core)) fra[idx1].duration = fra[idx0].v_pts - fra[idx1].v_pts;

	idx0 = idx1;
	idx1 = idx2;
	idx2=(idx2-1)%xp_core->num_v_buffs;
    }
}

any_t* xmp_video_decoder( any_t* arg )
{
    mpxp_thread_t* priv=arg;
    sh_video_t* sh_video=priv->dae->sh;
    demux_stream_t *d_video=sh_video->ds;
    demuxer_t* demuxer=d_video->demuxer;
    demux_stream_t* d_audio=demuxer->audio;

    float duration=0;
    float drop_barrier;
    int blit_frame=0;
    int drop_param=0;
    unsigned xp_n_frame_to_drop;
    float v_pts,mpeg_timer=HUGE;

    priv->state=Pth_Run;
    priv->dae->eof = 0;
    if(xp_core->audio) xp_core->audio->eof=0;
    MSG_T("\nDEC_AHEAD: entering...\n");
    __MP_UNIT(priv->p_idx,"dec_ahead");
    priv->pid = getpid();
    if(!(xp_core->audio && mp_conf.xp < XP_VAFull))
	priv->name = "video decoder+vf";
    drop_barrier=(float)(xp_core->num_v_buffs/2)*(1/sh_video->fps);
    if(mp_conf.av_sync_pts == -1 && !mp_data->use_pts_fix2)
	xp_core->bad_pts = d_video->demuxer->file_format == DEMUXER_TYPE_MPEG_ES ||
			d_video->demuxer->file_format == DEMUXER_TYPE_MPEG4_ES ||
			d_video->demuxer->file_format == DEMUXER_TYPE_H264_ES ||
			d_video->demuxer->file_format == DEMUXER_TYPE_MPEG_PS ||
			d_video->demuxer->file_format == DEMUXER_TYPE_MPEG_TS;
    else
	xp_core->bad_pts = mp_conf.av_sync_pts?0:1;
while(!priv->dae->eof){
    unsigned char* start=NULL;
    int in_size;
    if(priv->state==Pth_Canceling) break;
    if(priv->state==Pth_Sleep) {
pt_sleep:
	priv->state=Pth_ASleep;
	while(priv->state==Pth_ASleep) usleep(0);
	if(xp_core->bad_pts) mpeg_timer=HUGE;
	continue;
    }
    __MP_UNIT(priv->p_idx,"dec_ahead 1");

/* get it! */
#if 0
    /* prevent reent access to non-reent demuxer */
    //if(sh_video->num_frames>200)  *((char*)0x100) = 1; // Testing crash
    if(xp_core->audio && mp_conf.xp<XP_VAFull) {
	__MP_UNIT(priv->p_idx,"decode audio");
	while(2==xp_thread_decode_audio()) ;
	__MP_UNIT(priv->p_idx,"dec_ahead 2");
    }
#endif
/*--------------------  Decode a frame: -----------------------*/
    in_size=video_read_frame_r(sh_video,&duration,&v_pts,&start,sh_video->fps);
    if(in_size<0) {
	dae_decoded_mark_eof(xp_core->video);
	priv->dae->eof=1;
	break;
    }
    /* in_size==0: it's or broken stream or demuxer's bug */
    if(in_size==0 && priv->state!=Pth_Canceling) continue;
    /* frame was decoded into current decoder_idx */
    if(xp_core->bad_pts) {
	if(mpeg_timer==HUGE) mpeg_timer=v_pts;
	else if( mpeg_timer-duration<v_pts ) {
	    mpeg_timer=v_pts;
	    MSG_DBG2("Sync mpeg pts %f\n", mpeg_timer);
	}
	else mpeg_timer+=duration;
    }
    /* compute frame dropping */
    xp_n_frame_to_drop=0;
    if(mp_conf.frame_dropping) {
	int cur_time;
	cur_time = GetTimerMS();
	/* Ugly solution: disable frame dropping right after seeking! */
	if(cur_time - mp_data->seek_time > (xp_core->num_v_buffs/sh_video->fps)*100) xp_n_frame_to_drop=compute_frame_dropping(sh_video,v_pts,drop_barrier);
    } /* if( mp_conf.frame_dropping ) */
    if(!finite(v_pts)) MSG_WARN("Bug of demuxer! Value of video pts=%f\n",v_pts);
#if 0
/*
    We can't seriously examine question of too slow machines
    by motivation reasons
*/
if(ada_active_frame) /* don't emulate slow systems until xp_players are not started */
{
    int i,delay; /* sleeping 200 ms is far enough for 25 fps */
    delay=xp_n_frame_to_drop?0:20;
    for(i=0;i<delay;i++) usleep(0);
}
#endif
    if(xp_n_frame_to_drop)	drop_param=mp_conf.frame_dropping;
    else			drop_param=0;
    /* decode: */
    if(mp_data->output_quality) {
	unsigned total = xp_core->num_v_buffs/2;
	unsigned distance = dae_get_decoder_outrun(xp_core->video);
	int our_quality;
	our_quality = mp_data->output_quality*distance/total;
	if(drop_param) mpcv_set_quality(sh_video,0);
	else
	if(mp_conf.autoq) mpcv_set_quality(sh_video,our_quality>0?our_quality:0);
    }
    blit_frame=mpcv_decode(sh_video,start,in_size,drop_param,v_pts);
MSG_DBG2("DECODER: %i[%i] %f\n",dae_curr_vdecoded(xp_core),in_size,v_pts);
    if(mp_data->output_quality) {
	if(drop_param) mpcv_set_quality(sh_video,mp_data->output_quality);
    }
    if(!blit_frame && drop_param) priv->dae->num_dropped_frames++;
    if(blit_frame) {
	unsigned idx=dae_curr_vdecoded(xp_core);
	if(xp_core->bad_pts)
	    xp_core->video->frame[idx].v_pts=mpeg_timer;
	else
	    xp_core->video->frame[idx].v_pts = v_pts;
	xp_core->video->frame[idx].duration=duration;
	dae_decoded_clear_eof(xp_core->video);
	if(!xp_core->bad_pts) {
	    int _idx = dae_prev_vdecoded(xp_core);
	    xp_core->video->frame[_idx].duration=v_pts-xp_core->video->frame[_idx].v_pts;
	}
	if(mp_conf.frame_reorder) reorder_pts_in_mpeg();
    } /* if (blit_frame) */

    /* ------------ sleep --------------- */
    /* sleep if thread is too fast ;) */
    if(blit_frame)
    while(!dae_inc_decoded(xp_core->video)) {
	MSG_T("DEC_AHEAD: sleep: player=%i decoder=%i)\n"
	    ,dae_curr_vplayed(),dae_curr_vdecoded());
	if(priv->state==Pth_Canceling) goto pt_exit;
	if(priv->state==Pth_Sleep) goto pt_sleep;
	if(xp_core->audio && mp_conf.xp<XP_VAFull) {
	    __MP_UNIT(priv->p_idx,"decode audio");
	    xp_thread_decode_audio(d_audio);
	    __MP_UNIT(priv->p_idx,"dec_ahead 5");
	}
	usleep(1);
    }
/*------------------------ frame decoded. --------------------*/
} /* while(!priv->dae->eof)*/

if(xp_core->audio && mp_conf.xp<XP_VAFull) {
    while(!xp_core->audio->eof && priv->state!=Pth_Canceling && priv->state!=Pth_Sleep) {
	__MP_UNIT(priv->p_idx,"decode audio");
	if(!xp_thread_decode_audio(d_audio)) usleep(1);
	__MP_UNIT(priv->p_idx,NULL);
    }
}
  pt_exit:
  MSG_T("\nDEC_AHEAD: leaving...\n");
  priv->state=Pth_Stand;
  return arg; /* terminate thread here !!! */
}

void sig_video_decode( void )
{
    MSG_T("sig_video_decode\n");
    mp_msg_flush();

    xp_core->video->eof = 1;
    dae_decoded_mark_eof(xp_core->video);
    /*
	Unlock all mutex
	( man page says it may deadlock, but what is worse deadlock here or later? )
    */
    xmp_killall_threads(pthread_self());
    __exit_sighandler();
}
