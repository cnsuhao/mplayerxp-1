/*
   Decoding ahead
   Licence: GPL v2
   Author: Nickols_K
   Note: Threaded engine to decode frames ahead
*/

#include "mp_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#define __USE_ISOC99 1 /* for lrint */
#include <math.h>
#include <sys/time.h>
#include "osdep/mplib.h"
#define DA_PREFIX "DEC_AHEAD:"
#define MSGT_CLASS MSGT_CPLAYER
#include "mp_msg.h"

#include "xmp_core.h"
#include "mplayer.h"
#include "libao2/audio_out.h"
#include "libvo/video_out.h"

#include "libmpcodecs/dec_video.h"
#include "libmpcodecs/dec_audio.h"
#include "sig_hand.h"
#include "osdep/timer.h"

#ifdef ENABLE_DEC_AHEAD_DEBUG
#define MSG_T(args...) mp_msg(MSGT_GLOBAL, MSGL_DBG2,__FILE__,__LINE__, ## args )
#else
#define MSG_T(args...)
#endif

void xmp_init(void) {
    xp_core=mp_mallocz(sizeof(xp_core_t));
    xp_core->initial_apts=HUGE;
}

void xmp_uninit(void) {
    mp_free(xp_core->mpxp_threads[0]);
    mp_free(xp_core);
    xp_core=NULL;
}

unsigned xmp_register_main(sig_handler_t sigfunc) {
    unsigned idx=0;
    xp_core->mpxp_threads[idx]=mp_mallocz(sizeof(mpxp_thread_t));
    xp_core->mpxp_threads[idx]->p_idx=idx;
    xp_core->mpxp_threads[idx]->pid=getpid();
    xp_core->main_pth_id=xp_core->mpxp_threads[idx]->pth_id=pthread_self();
    xp_core->mpxp_threads[idx]->name = "main";
    xp_core->mpxp_threads[idx]->sigfunc = sigfunc;
    xp_core->mpxp_threads[idx]->dae = NULL;
    xp_core->num_threads++;

    return idx;
}

static void print_stopped_thread(unsigned idx) {
    MSG_OK("*** stop thread: [%i] %s\n",idx,xp_core->mpxp_threads[idx]->name);
}

void xmp_killall_threads(pthread_t _self)
{
    unsigned i;
    for(i=0;i < MAX_MPXP_THREADS;i++) {
	if( _self &&
	    xp_core->mpxp_threads[i]->pth_id &&
	    xp_core->mpxp_threads[i]->pth_id != xp_core->main_pth_id) {
		pthread_kill(xp_core->mpxp_threads[i]->pth_id,SIGKILL);
	    print_stopped_thread(i);
	    mp_free(xp_core->mpxp_threads[i]);
	    xp_core->mpxp_threads[i]=NULL;
	}
    }
}

void dae_reset(dec_ahead_engine_t* it) {
    it->player_idx=0;
    it->decoder_idx=0;
    it->num_slow_frames=0;
    it->num_played_frames=0;
    it->num_decoded_frames=0;
}

void dae_init(dec_ahead_engine_t* it,unsigned nframes,any_t* sh)
{
    it->nframes=nframes;
    it->frame = mp_malloc(sizeof(xmp_frame_t)*nframes);
    it->sh=sh;
    dae_reset(it);
}

void dae_uninit(dec_ahead_engine_t* it) { mp_free(it->frame); it->frame=NULL; }

/* returns 1 - on success 0 - if busy */
int dae_try_inc_played(dec_ahead_engine_t* it) {
    unsigned new_idx;
    new_idx=(it->player_idx+1)%it->nframes;
    if(new_idx==it->decoder_idx) {
	it->num_slow_frames++;
	return 0;
    }
    it->num_slow_frames=0;
    it->num_played_frames++;
    return 1;
}

int dae_inc_played(dec_ahead_engine_t* it) {
    unsigned new_idx;
    new_idx=(it->player_idx+1)%it->nframes;
    if(new_idx==it->decoder_idx) return 0;
    if(it->free_priv) (*it->free_priv)(it,it->frame[it->player_idx].priv);
    it->player_idx=new_idx;
    return 1;
}

/* returns 1 - on success 0 - if busy */
int dae_inc_decoded(dec_ahead_engine_t* it) {
    unsigned new_idx;
    new_idx=(it->decoder_idx+1)%it->nframes;
    if(new_idx==it->player_idx) return 0;
    it->decoder_idx=new_idx;
    if(it->new_priv) it->frame[it->player_idx].priv=(*it->new_priv)(it);
    it->num_decoded_frames++;
    return 1;
}

unsigned dae_prev_played(const dec_ahead_engine_t* it) { return (it->player_idx-1)%it->nframes; }
unsigned dae_prev_decoded(const dec_ahead_engine_t* it) { return (it->decoder_idx-1)%it->nframes; }
unsigned dae_next_played(const dec_ahead_engine_t* it) { return (it->player_idx+1)%it->nframes; }
unsigned dae_next_decoded(const dec_ahead_engine_t* it) { return (it->decoder_idx+1)%it->nframes; }

unsigned dae_get_decoder_outrun(const dec_ahead_engine_t* it) {
    unsigned decoder_idx=it->decoder_idx;
    if(decoder_idx<it->player_idx) decoder_idx+=it->nframes;
    return decoder_idx-it->player_idx;
}

void dae_wait_decoder_outrun(const dec_ahead_engine_t* it) {
    if(it) {
	do {
	    usleep(0);
	}while(dae_get_decoder_outrun(it) < xp_core->num_v_buffs/2);
    }
}

xmp_frame_t dae_played_frame(const dec_ahead_engine_t* it) {
    unsigned idx=it->player_idx;
    return it->frame[idx];
}
xmp_frame_t dae_decoded_frame(const dec_ahead_engine_t* it) {
    unsigned idx=it->decoder_idx;
    return it->frame[idx];
}
xmp_frame_t dae_next_played_frame(const dec_ahead_engine_t* it) {
    unsigned idx=dae_next_played(it);
    return it->frame[idx];
}
xmp_frame_t dae_next_decoded_frame(const dec_ahead_engine_t* it) {
    unsigned idx=dae_next_decoded(it);
    return it->frame[idx];
}
xmp_frame_t dae_prev_played_frame(const dec_ahead_engine_t* it) {
    unsigned idx=dae_prev_played(it);
    return it->frame[idx];
}
xmp_frame_t dae_prev_decoded_frame(const dec_ahead_engine_t* it) {
    unsigned idx=dae_prev_decoded(it);
    return it->frame[idx];
}

int dae_played_eof(const dec_ahead_engine_t* it) {
    unsigned idx=it->player_idx;
    return (it->frame[idx].v_pts==HUGE_VALF)?1:0;
}

int dae_decoded_eof(const dec_ahead_engine_t* it) {
    unsigned idx=it->decoder_idx;
    return (it->frame[idx].v_pts==HUGE_VALF)?1:0;
}

void dae_decoded_mark_eof(dec_ahead_engine_t* it) {
    unsigned idx=it->decoder_idx;
    it->frame[idx].v_pts=HUGE_VALF;
}

pthread_mutex_t audio_play_mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t audio_play_cond=PTHREAD_COND_INITIALIZER;

pthread_mutex_t audio_decode_mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t audio_decode_cond=PTHREAD_COND_INITIALIZER;

volatile int dec_ahead_can_aseek=0;  /* It is safe to seek audio */
volatile int dec_ahead_can_adseek=1;  /* It is safe to seek audio buffer thread */

extern int decore_audio( int xp_id );
extern void update_osd( float v_pts );

/* To let audio decoder thread sleep as long as player */
static struct timespec audio_play_timeout;
static int audio_play_in_sleep=0;

extern int init_audio_buffer(int size, int min_reserv, int indices, sh_audio_t *sh_audio);
extern void uninit_audio_buffer(void);
extern int read_audio_buffer(sh_audio_t *audio, unsigned char *buffer, unsigned minlen, unsigned maxlen );
extern float get_delay_audio_buffer(void);
extern int decode_audio_buffer(unsigned len);
extern void reset_audio_buffer(void);
extern int get_len_audio_buffer(void);
extern int get_free_audio_buffer(void);

any_t* audio_play_routine( any_t* arg );

/* Audio stuff */
volatile float dec_ahead_audio_delay;
static int xp_thread_decode_audio(void)
{
    sh_audio_t* sh_audio=xp_core->audio->sh;
    sh_video_t* sh_video=NULL;
    if(xp_core->video) sh_video=xp_core->video->sh;
    int free_buf, vbuf_size, pref_buf;
    unsigned len=0;

    free_buf = get_free_audio_buffer();

    if( free_buf == -1 ) { /* End of file */
	xp_core->audio->eof = 1;
	return 0;
    }
    if( free_buf < (int)sh_audio->audio_out_minsize ) /* full */
	return 0;

    len = get_len_audio_buffer();

    if( len < MAX_OUTBURST ) /* Buffer underrun */
	return decode_audio_buffer(MAX_OUTBURST);

    if(xp_core->video) {
	/* Match video buffer */
	vbuf_size = dae_get_decoder_outrun(xp_core->video);
	pref_buf = vbuf_size / sh_video->fps * sh_audio->af_bps;
	pref_buf -= len;
	if( pref_buf > 0 ) {
	    len = min( pref_buf, free_buf );
	    if( len > sh_audio->audio_out_minsize ) {
		return decode_audio_buffer(len);
	    }
	}
    } else
	return decode_audio_buffer(min(free_buf,MAX_OUTBURST));

    return 0;
}


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

any_t* Va_dec_ahead_routine( any_t* arg )
{
    mpxp_thread_t* priv=arg;
    sh_video_t* sh_video=priv->dae->sh;
    demux_stream_t *d_video=sh_video->ds;

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
	    xp_thread_decode_audio();
	    __MP_UNIT(priv->p_idx,"dec_ahead 5");
	}
	usleep(1);
    }
/*------------------------ frame decoded. --------------------*/
} /* while(!priv->dae->eof)*/

if(xp_core->audio && mp_conf.xp<XP_VAFull) {
    while(!xp_core->audio->eof && priv->state!=Pth_Canceling && priv->state!=Pth_Sleep) {
	__MP_UNIT(priv->p_idx,"decode audio");
	if(!xp_thread_decode_audio()) usleep(1);
	__MP_UNIT(priv->p_idx,NULL);
    }
}
  pt_exit:
  MSG_T("\nDEC_AHEAD: leaving...\n");
  priv->state=Pth_Stand;
  return arg; /* terminate thread here !!! */
}

/* this routine decodes audio only */
any_t* a_dec_ahead_routine( any_t* arg )
{
    mpxp_thread_t* priv=arg;

    int ret, retval;
    struct timeval now;
    struct timespec timeout;
    float d;

    priv->state=Pth_Run;
    if(xp_core->video) xp_core->video->eof=0;
    xp_core->audio->eof=0;
    MSG_T("\nDEC_AHEAD: entering...\n");
    priv->pid = getpid();
    __MP_UNIT(priv->p_idx,"dec_ahead");

    dec_ahead_can_adseek=0;
    while(priv->state!=Pth_Canceling) {
	if(priv->state==Pth_Sleep) {
	    priv->state=Pth_ASleep;
	    while(priv->state==Pth_ASleep) usleep(0);
	    continue;
	}
	__MP_UNIT(priv->p_idx,"decode audio");
	while((ret = xp_thread_decode_audio()) == 2) {/* Almost empty buffer */
	    if(xp_core->audio->eof) break;
	}
	dec_ahead_can_adseek=1;

	if(priv->state==Pth_Canceling) break;

	__MP_UNIT(priv->p_idx,"sleep");
	LOCK_AUDIO_DECODE();
	if(priv->state!=Pth_Canceling) {
	    if(xp_core->audio->eof) {
		__MP_UNIT(priv->p_idx,"wait end of work");
		pthread_cond_wait( &audio_decode_cond, &audio_decode_mutex );
	    } else if(ret==0) { /* Full buffer or end of file */
		if(audio_play_in_sleep) { /* Sleep a little longer than player thread */
		    timeout.tv_nsec = audio_play_timeout.tv_nsec + 10000;
		    if( timeout.tv_nsec > 1000000000l ) {
			timeout.tv_nsec-=1000000000l;
			timeout.tv_sec = audio_play_timeout.tv_sec;
		    } else
			timeout.tv_sec = audio_play_timeout.tv_sec;
		} else {
		    if(xp_core->in_pause)
			d = 1.0;
		    else
			d = 0.1;
		    gettimeofday(&now,NULL);
		    timeout.tv_nsec = now.tv_usec * 1000 + d*1000000000l;
		    if( timeout.tv_nsec > 1000000000l ) {
			timeout.tv_nsec-=1000000000l;
			timeout.tv_sec = now.tv_sec + 1;
		    } else
			timeout.tv_sec = now.tv_sec;
		}
		pthread_cond_timedwait( &audio_decode_cond, &audio_decode_mutex, &timeout );
	    } else
		usleep(1);
	}
	UNLOCK_AUDIO_DECODE();

	if(priv->state==Pth_Canceling) break;

	__MP_UNIT(priv->p_idx,"seek");
	LOCK_AUDIO_DECODE();
#if 0
	while(priv->state==Pth_Sleep && priv->state!=Pth_Canceling) {
	    gettimeofday(&now,NULL);
	    timeout.tv_nsec = now.tv_usec * 1000;
	    timeout.tv_sec = now.tv_sec + 1;
	    retval = pthread_cond_timedwait( &audio_decode_cond, &audio_decode_mutex, &timeout );
	    if( retval == ETIMEDOUT )
		MSG_V("Audio decode seek timeout\n");
	}
#endif
	dec_ahead_can_adseek = 0; /* Not safe to seek */
	UNLOCK_AUDIO_DECODE();
    }
    __MP_UNIT(priv->p_idx,"exit");
    dec_ahead_can_adseek = 1;
    priv->state=Pth_Stand;
    return arg; /* terminate thread here !!! */
}

void xmp_uninit_engine( int force )
{
    xmp_stop_threads(force);

    if(xp_core->video) {
	dae_uninit(xp_core->video);
	xp_core->video=NULL;
    }

    if(xp_core->audio) { /* audio state doesn't matter on segfault :( */
	uninit_audio_buffer();
	xp_core->audio=NULL;
    }
}

/* Min audio buffer to keep mp_free, used to tell differ between full and empty buffer */
#define MIN_BUFFER_RESERV 8

int xmp_init_engine(sh_video_t *shv, sh_audio_t *sha)
{
    if(shv) {
	xp_core->video=mp_mallocz(sizeof(dec_ahead_engine_t));
	dae_init(xp_core->video,xp_core->num_v_buffs,shv);
    }
    if(mp_conf.xp>=XP_VideoAudio && sha) {
	int asize;
	unsigned o_bps;
	unsigned min_reserv;
	o_bps=sha->afilter_inited?sha->af_bps:sha->o_bps;
	if(xp_core->video)	asize = max(3*sha->audio_out_minsize,max(3*MAX_OUTBURST,o_bps*xp_core->num_v_buffs/shv->fps))+MIN_BUFFER_RESERV;
	else			asize = o_bps*xp_core->num_a_buffs;
	/* FIXME: get better indices from asize/real_audio_packet_size */
	min_reserv = sha->audio_out_minsize;
	if (o_bps > sha->o_bps)
	    min_reserv = (float)min_reserv * (float)o_bps / (float)sha->o_bps;
	init_audio_buffer(asize+min_reserv,min_reserv+MIN_BUFFER_RESERV,asize/(sha->audio_out_minsize<10000?sha->audio_out_minsize:4000)+100,sha);
	xp_core->audio=mp_mallocz(sizeof(dec_ahead_engine_t));
	dae_init(xp_core->audio,xp_core->num_a_buffs,sha);
    }
    return 0;
}

unsigned xmp_register_thread(dec_ahead_engine_t* dae,sig_handler_t sigfunc,mpxp_routine_t routine,const char *name) {
    unsigned idx=xp_core->num_threads;
    int rc;
    if(idx>=MAX_MPXP_THREADS) return UINT_MAX;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    rc=pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
    if(rc) {
	MSG_ERR("running thread: attr_setdetachstate fault!!!\n");
	pthread_attr_destroy(&attr);
	return rc;
    }
    pthread_attr_setscope(&attr,PTHREAD_SCOPE_SYSTEM);
#if 0
    /* requires root privelegies */
    pthread_attr_setschedpolicy(&attr,SCHED_FIFO);
#endif
    xp_core->mpxp_threads[idx]=mp_mallocz(sizeof(mpxp_thread_t));

    xp_core->mpxp_threads[idx]->p_idx=idx;
    xp_core->mpxp_threads[idx]->name=name;
    xp_core->mpxp_threads[idx]->routine=routine;
    xp_core->mpxp_threads[idx]->sigfunc=sigfunc;
    xp_core->mpxp_threads[idx]->state=Pth_Stand;
    xp_core->mpxp_threads[idx]->dae=dae;

    rc=pthread_create(&xp_core->mpxp_threads[idx]->pth_id,&attr,routine,xp_core->mpxp_threads[idx]);
    pthread_attr_destroy(&attr);

    xp_core->num_threads++;
    return (rc?UINT_MAX:idx);
}

int xmp_run_decoders( void )
{
    unsigned rc;
    if((xp_core->audio && mp_conf.xp >= XP_VAFull) || !xp_core->video) {
	if((rc=xmp_register_thread(xp_core->audio,sig_audio_decode,a_dec_ahead_routine,"audio decoder+af"))==UINT_MAX) return rc;
	while(xp_core->mpxp_threads[rc]->state!=Pth_Run) usleep(0);
    }
    if(xp_core->video) {
	if((rc=xmp_register_thread(xp_core->video,sig_dec_ahead_video,Va_dec_ahead_routine,"video+audio decoders & af+vf"))==UINT_MAX) return rc;
	while(xp_core->mpxp_threads[rc]->state!=Pth_Run) usleep(0);
    }
    return 0;
}

int xmp_run_players(void)
{
    unsigned rc;
    if( xp_core->audio && mp_conf.xp >= XP_VAPlay ) {
	if((rc=xmp_register_thread(xp_core->audio,sig_audio_play,audio_play_routine,"audio player"))==UINT_MAX) return rc;
	while(xp_core->mpxp_threads[rc]->state!=Pth_Run) usleep(0);
    }
    return 0;
}

/* Stops threads before seek */
void xmp_stop_threads(int force)
{
    unsigned i;
    for(i=1;i<xp_core->num_threads;i++) {
	if(force) pthread_kill(xp_core->mpxp_threads[i]->pth_id,SIGKILL);
	else {
	    xp_core->mpxp_threads[i]->state=Pth_Canceling;
	    while(xp_core->mpxp_threads[i]->state==Pth_Canceling) usleep(0);
	}
	print_stopped_thread(i);
	mp_free(xp_core->mpxp_threads[i]);
	xp_core->mpxp_threads[i]=NULL;
    }
}

/* Halt threads before seek */
void xmp_halt_threads(int is_reset_vcache)
{
    unsigned i;
    for(i=1;i<xp_core->num_threads;i++) {
	xp_core->mpxp_threads[i]->state=Pth_Sleep;
	while(xp_core->mpxp_threads[i]->state==Pth_Sleep) usleep(0);
    }
}

/* Restart threads after seek */
void xmp_restart_threads(int xp_id)
{
    /* reset counters */
    dae_reset(xp_core->video);
    /* temporary solution */
    reset_audio_buffer();
    /* Ugly hack: but we should read audio packet before video after seeking.
       Else we'll get picture destortion on the screen */
    xp_core->initial_apts=HUGE;

    unsigned i;
    for(i=1;i<xp_core->num_threads;i++) {
	xp_core->mpxp_threads[i]->state=Pth_Run;
	while(xp_core->mpxp_threads[i]->state==Pth_ASleep) usleep(0);
    }
}

#define MIN_AUDIO_TIME 0.05
#define NOTHING_PLAYED (-1.0)
#define XP_MIN_TIMESLICE 0.010 /* under Linux on x86 min time_slice = 10 ms */

extern ao_data_t* ao_data;
any_t* audio_play_routine( any_t* arg )
{
    mpxp_thread_t* priv=arg;
    sh_audio_t* sh_audio=priv->dae->sh;

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
    dec_ahead_can_aseek=0;

    while(priv->state!=Pth_Canceling) {
	if(priv->state==Pth_Sleep) {
	    priv->state=Pth_ASleep;
	    while(priv->state==Pth_ASleep) usleep(0);
	    continue;
	}
	__MP_UNIT(priv->p_idx,"audio decore_audio");
	dec_ahead_audio_delay = NOTHING_PLAYED;
	eof = decore_audio(priv->p_idx);

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

void dec_ahead_reset_sh_video(sh_video_t *shv)
{
    sh_video_t* sh_video=xp_core->video->sh;
    sh_video->vfilter = shv->vfilter;
}

void sig_dec_ahead_video( void )
{
    MSG_T("sig_dec_ahead_video\n");
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

void sig_audio_play( void )
{
    MSG_T("sig_audio_play\n");
    mp_msg_flush();

    dec_ahead_can_aseek=1;

    UNLOCK_AUDIO_PLAY();

    xmp_killall_threads(pthread_self());
    __exit_sighandler();
}

void sig_audio_decode( void )
{
    MSG_T("sig_audio_decode\n");
    mp_msg_flush();

    dec_ahead_can_adseek=1;

    UNLOCK_AUDIO_DECODE();

    xmp_killall_threads(pthread_self());
    __exit_sighandler();
}
