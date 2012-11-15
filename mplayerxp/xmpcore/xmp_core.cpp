/*
   Decoding ahead
   Licence: GPL v2
   Author: Nickols_K
   Note: Threaded engine to decode frames ahead
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#define __USE_ISOC99 1 /* for lrint */
#include <math.h>
#include <sys/time.h>
extern "C" {
#include "mp_config.h"
#define DA_PREFIX "DEC_AHEAD:"
#define MSGT_CLASS MSGT_CPLAYER
#include "mp_msg.h"
#include "osdep/mplib.h"

#include "xmp_core.h"

#include "mplayerxp.h"
#include "libao2/audio_out.h"
#include "libvo/video_out.h"

#include "libmpcodecs/dec_video.h"
#include "libmpcodecs/dec_audio.h"
#include "sig_hand.h"
#include "osdep/timer.h"
}
#include "xmp_aplayer.h"
#include "xmp_vplayer.h"
#include "xmp_adecoder.h"
#include "xmp_vdecoder.h"

#ifdef ENABLE_DEC_AHEAD_DEBUG
#define MSG_T(args...) mp_msg(MSGT_GLOBAL, MSGL_DBG2,__FILE__,__LINE__, ## args )
#else
#define MSG_T(args...)
#endif

void xmp_init(void) {
    xp_core=(xp_core_t*)mp_mallocz(sizeof(xp_core_t));
    xp_core->initial_apts=HUGE;
}

void xmp_uninit(void) {
    mp_free(xp_core->mpxp_threads[0]);
    mp_free(xp_core);
    xp_core=NULL;
}

unsigned xmp_register_main(sig_handler_t sigfunc) {
    unsigned idx=0;
    xp_core->mpxp_threads[idx]=(mpxp_thread_t*)mp_mallocz(sizeof(mpxp_thread_t));
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
    it->frame = (xmp_frame_t*)mp_malloc(sizeof(xmp_frame_t)*nframes);
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

/* To let audio decoder thread sleep as long as player */
struct timespec audio_play_timeout;
int audio_play_in_sleep=0;

/* Min audio buffer to keep mp_free, used to tell differ between full and empty buffer */
#define MIN_BUFFER_RESERV 8

static xmp_model_e xmp_engine_compute_model(sh_video_t *shv, sh_audio_t *sha) {
    xmp_model_e rc;
    if(!shv && sha) {
	switch(mp_conf.xp) {
	    case XP_UniCore:	rc=XMP_Run_AudioPlayback; break;
	    default:		rc=XMP_Run_AudioPlayer|XMP_Run_AudioDecoder; break;
	}
    } else if(shv && !sha) {
	switch(mp_conf.xp) {
	    default:
	    case XP_UniCore:	rc=XMP_Run_VideoPlayer|XMP_Run_VideoDecoder; break;
	}
    } else { /* both shv and sha */
	switch(mp_conf.xp) {
	    case XP_UniCore:
		rc=XMP_Run_VideoPlayer|XMP_Run_VA_Decoder|XMP_Run_AudioPlayer; break;
	    case XP_DualCore:
		rc=XMP_Run_VideoPlayer|XMP_Run_VideoDecoder|XMP_Run_AudioPlayback; break;
	    default: /* TripleCore */
		rc=XMP_Run_VideoPlayer|XMP_Run_VideoDecoder|XMP_Run_AudioPlayer|XMP_Run_AudioDecoder; break;
	}
    }
    return rc;
}

int xmp_init_engine(sh_video_t *shv, sh_audio_t *sha)
{
    xp_core->flags=xmp_engine_compute_model(shv,sha);
    if(shv) {
	xp_core->video=(dec_ahead_engine_t*)mp_mallocz(sizeof(dec_ahead_engine_t));
	dae_init(xp_core->video,xp_core->num_v_buffs,shv);
    }
    if(sha) {
	if(xmp_test_model(XMP_Run_AudioPlayer)) {
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
	}
	xp_core->audio=(dec_ahead_engine_t*)mp_mallocz(sizeof(dec_ahead_engine_t));
	dae_init(xp_core->audio,xp_core->num_a_buffs,sha);
    }
    return 0;
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

unsigned xmp_register_thread(dec_ahead_engine_t* dae,sig_handler_t sigfunc,mpxp_routine_t routine,const char *name) {
    unsigned stacksize=1000000;
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
    pthread_attr_setstack(&attr,mp_malloc(stacksize),stacksize);
#if 0
    /* requires root privelegies */
    pthread_attr_setschedpolicy(&attr,SCHED_FIFO);
#endif
    xp_core->mpxp_threads[idx]=(mpxp_thread_t*)mp_mallocz(sizeof(mpxp_thread_t));

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
    if(xp_core->audio && xmp_test_model(XMP_Run_AudioDecoder)) {
	if((rc=xmp_register_thread(xp_core->audio,sig_audio_decode,a_dec_ahead_routine,"audio decoder"))==UINT_MAX) return rc;
	while(xp_core->mpxp_threads[rc]->state!=Pth_Run) usleep(0);
    }
    if(xp_core->video && xmp_test_model(XMP_Run_VideoDecoder|XMP_Run_VA_Decoder)) {
	if((rc=xmp_register_thread(xp_core->video,sig_video_decode,xmp_video_decoder,"video+audio decoders"))==UINT_MAX) return rc;
	while(xp_core->mpxp_threads[rc]->state!=Pth_Run) usleep(0);
    }
    return 0;
}

int xmp_run_players(void)
{
    unsigned rc;
    if(xp_core->audio && xmp_test_model(XMP_Run_AudioPlayer|XMP_Run_AudioPlayback)) {
	if((rc=xmp_register_thread(xp_core->audio,sig_audio_play,audio_play_routine,"audio player"))==UINT_MAX) return rc;
	while(xp_core->mpxp_threads[rc]->state!=Pth_Run) usleep(0);
    }
    if(xp_core->video && xmp_test_model(XMP_Run_VideoPlayer)) {
	if((rc=xmp_register_thread(xp_core->video,sig_video_play,xmp_video_player,"video player"))==UINT_MAX) return rc;
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
