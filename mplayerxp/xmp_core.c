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
#ifdef HAVE_MALLOC
#include <malloc.h>
#endif
#define DA_PREFIX "DEC_AHEAD:"
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

xp_core_t xp_core;

void xp_core_init(void) {
    memset(&xp_core,0,sizeof(xp_core_t));
    xp_core.in_lseek=NoSeek;
}

void xp_core_uninit(void) {}

void dae_reset(dec_ahead_engine_t* it) {
    it->player_idx=0;
    it->decoder_idx=0;
    it->num_slow_frames=0;
    it->num_played_frames=0;
    it->num_decoded_frames=0;
}

void dae_init(dec_ahead_engine_t* it,unsigned nframes)
{
    it->nframes=nframes;
    it->fra = malloc(sizeof(frame_attr_t)*nframes);
    dae_reset(it);
}

void dae_uninit(dec_ahead_engine_t* it) { free(it->fra); it->fra=0; }

/* returns 1 - on success 0 - if busy */
int dae_inc_played(dec_ahead_engine_t* it) {
    unsigned new_idx;
    new_idx=(it->player_idx+1)%it->nframes;
    if(new_idx==it->decoder_idx) {
	it->num_slow_frames++;
	return 0;
    }
    it->player_idx=new_idx;
    it->num_slow_frames=0;
    it->num_played_frames++;
    return 1;
}
/* returns 1 - on success 0 - if busy */
int dae_inc_decoded(dec_ahead_engine_t* it) {
    unsigned new_idx;
    new_idx=(it->decoder_idx+1)%it->nframes;
    if(new_idx==it->player_idx) return 0;
    it->decoder_idx=new_idx;
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

frame_attr_t dae_played_fra(const dec_ahead_engine_t* it) {
    unsigned idx=it->player_idx;
    return it->fra[idx];
}
frame_attr_t dae_decoded_fra(const dec_ahead_engine_t* it) {
    unsigned idx=it->decoder_idx;
    return it->fra[idx];
}

pthread_mutex_t audio_play_mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t audio_play_cond=PTHREAD_COND_INITIALIZER;

pthread_mutex_t audio_decode_mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t audio_decode_cond=PTHREAD_COND_INITIALIZER;

extern volatile int xp_drop_frame;
extern volatile unsigned xp_drop_frame_cnt;
extern int output_quality;

int ao_da_buffs;

extern volatile float xp_screen_pts;
volatile int dec_ahead_can_aseek=0;  /* It is safe to seek audio */
volatile int dec_ahead_can_adseek=1;  /* It is safe to seek audio buffer thread */

static pthread_t pthread_id=0;
static pthread_attr_t our_attr;
static pthread_attr_t audio_attr;
static sh_video_t *sh_video;
static sh_audio_t *sh_audio;
extern demux_stream_t *d_video;

/* Support for '-loop' option */
extern int loop_times; /* it's const for xp mode */

extern float rel_seek_secs;	/* FIXME: in hope that user will not rewind */
extern int sof_seek_pos;	/* the movie at end of file :( */

extern int decore_audio( int xp_id );
extern int mpxp_seek_time;

extern void update_osd( float v_pts );
volatile int xp_eof=0;
int xp_audio_eof=0;
#define NORM_FRAME(a) ((a)%xp_num_frames)

/* To let audio decoder thread sleep as long as player */
struct timespec audio_play_timeout;
int audio_play_in_sleep=0;

extern int init_audio_buffer(int size, int min_reserv, int indices, sh_audio_t *sh_audio);
extern void uninit_audio_buffer(void);
extern int read_audio_buffer(sh_audio_t *audio, unsigned char *buffer, unsigned minlen, unsigned maxlen );
extern float get_delay_audio_buffer(void);
extern int decode_audio_buffer(unsigned len);
extern void reset_audio_buffer(void);
extern int get_len_audio_buffer(void);
extern int get_free_audio_buffer(void);

any_t* audio_play_routine( any_t* arg );

static volatile int pthread_is_living=0;
static volatile int a_pthread_is_living=0;
static volatile int pthread_audio_is_living=0;
static volatile int pthread_end_of_work=0;
static volatile int a_pthread_end_of_work=0;
static volatile int pthread_audio_end_of_work=0;

int xp_is_bad_pts=0;

/* this routine decodes video+audio but intends to be video only  */

static void show_warn_cant_sync(float max_frame_delay) {
    static int warned=0;
    static float prev_warn_delay=0;
    if(!warned || max_frame_delay > prev_warn_delay) {
	warned=1;
	MSG_WARN("*********************************************\n"
		     "** Can't stabilize A-V sync!!!             **\n"
		     "*********************************************\n"
		     "Try increase number of buffer for decoding ahead\n"
		     "Exist: %u, need: %u\n"
		     ,xp_num_frames,(unsigned)(max_frame_delay*3*sh_video->fps)+3);
	prev_warn_delay=max_frame_delay;
    }
}

static unsigned compute_frame_dropping(float v_pts,float drop_barrier) {
    unsigned rc=0;
    static float prev_delta=64;
    float delta,max_frame_delay;/* delay for decoding of top slow frame */
    max_frame_delay = time_usage.max_video+time_usage.max_vout;

    /*
	TODO:
	    Replace the constants with some values which are depended on
	    xp_num_frames and max_frame_delay to find out the smoothest way
	    to display frames on slow machines.
	MAYBE!!!: (won't work with some realmedia streams for example)
	    Try to borrow avifile's logic (btw, GPL'ed ;) for very slow systems:
	    - fill a full buffer (is not always reachable)
	    - while(video.pts < audio.pts)
		video.seek_to_key_frame(video.get_next_key_frame(video.get_cur_pos()))
    */
    delta=v_pts-xp_screen_pts;
    if(max_frame_delay*3 > drop_barrier) {
	if(drop_barrier < (float)(xp_num_frames-2)/sh_video->fps) drop_barrier += 1/sh_video->fps;
	else
	if(mp_conf.verbose) show_warn_cant_sync(max_frame_delay);
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
	rc = (dae_curr_vdecoded()%fr_skip_divisor)?0:1;
	if(delta>prev_delta) rc=0;
    }
    MSG_D("DEC_AHEAD: max_frame_delay*3=%f drop_barrier=%f prev_delta=%f delta=%f(v_pts=%f screen_pts=%f) n_fr_to_drop=%u\n",max_frame_delay*3,drop_barrier,prev_delta,delta,v_pts,xp_screen_pts,xp_n_frame_to_drop);
    prev_delta=delta;
    return rc;
}

static void reorder_pts_in_mpeg(void) {
    unsigned idx0=0, idx1, idx2, idx3;

    idx1 = dae_curr_vdecoded();
    idx2 = dae_prev_vdecoded();
    frame_attr_t* fra=xp_core.video->fra;
    while( dae_curr_vplayed() != idx2 &&
	   fra[idx2].v_pts > fra[idx1].v_pts &&
	   fra[idx2].v_pts < fra[idx1].v_pts+1.0 ) {
	float tmp;
	tmp = fra[idx1].v_pts;
	fra[idx1].v_pts = fra[idx2].v_pts;
	fra[idx2].v_pts = tmp;

	fra[idx1].stream_pts = fra[idx1].v_pts;
	fra[idx2].stream_pts = fra[idx2].v_pts;
	fra[idx2].duration =   fra[idx1].v_pts - fra[idx2].v_pts;

	idx3=(idx2-1)%xp_num_frames;
	if(fra[idx2].v_pts > fra[idx3].v_pts &&
	   fra[idx2].v_pts - fra[idx3].v_pts < 1.0)
		fra[idx3].duration = fra[idx2].v_pts - fra[idx3].v_pts;

	if(idx1 != dae_curr_vdecoded()) fra[idx1].duration = fra[idx0].v_pts - fra[idx1].v_pts;

	idx0 = idx1;
	idx1 = idx2;
	idx2=(idx2-1)%xp_num_frames;
    }
}

any_t* Va_dec_ahead_routine( any_t* arg )
{
    float duration=0;
    float drop_barrier;
    int blit_frame=0;
    int drop_param=0;
    unsigned xp_n_frame_to_drop;
    int _xp_id;
    float v_pts,mpeg_timer=HUGE;

    pthread_is_living=1;
    xp_eof = 0;
    xp_audio_eof=0;
    MSG_T("\nDEC_AHEAD: entering...\n");
    _xp_id=init_signal_handling(sig_dec_ahead_video,uninit_dec_ahead);
    MP_UNIT(_xp_id,"dec_ahead");
    pinfo[_xp_id].pid = getpid(); /* Only for testing */
    pinfo[_xp_id].pth_id = pthread_self();
    pinfo[_xp_id].thread_name = (xp_core.has_audio && mp_conf.xp < XP_VAFull) ?
				"video+audio decoding+filtering ahead" :
				"video decoding+vf ahead";
    drop_barrier=(float)(xp_num_frames/2)*(1/sh_video->fps);
    if(mp_conf.av_sync_pts == -1 && !use_pts_fix2)
	xp_is_bad_pts = d_video->demuxer->file_format == DEMUXER_TYPE_MPEG_ES ||
			d_video->demuxer->file_format == DEMUXER_TYPE_MPEG4_ES ||
			d_video->demuxer->file_format == DEMUXER_TYPE_H264_ES ||
			d_video->demuxer->file_format == DEMUXER_TYPE_MPEG_PS ||
			d_video->demuxer->file_format == DEMUXER_TYPE_MPEG_TS;
    else
	xp_is_bad_pts = mp_conf.av_sync_pts?0:1;
while(!xp_eof){
    unsigned char* start=NULL;
    int in_size;
    if(pthread_end_of_work) break;
    if(xp_core.in_lseek==PreSeek) {
	MP_UNIT(_xp_id,"Pre seek");
	xp_core.in_lseek=Seek;
    }
    MP_UNIT(_xp_id,"dec_ahead 1");

/* get it! */
#if 0
    /* prevent reent access to non-reent demuxer */
    //if(sh_video->num_frames>200)  *((char*)0x100) = 1; // Testing crash
    if(xp_core.has_audio && mp_conf.xp<XP_VAFull) {
	MP_UNIT(_xp_id,"decode audio");
	while(2==xp_thread_decode_audio()) ;
	MP_UNIT(_xp_id,"dec_ahead 2");
    }
#endif
/*--------------------  Decode a frame: -----------------------*/
    in_size=video_read_frame_r(sh_video,&duration,&v_pts,&start,sh_video->fps);
    if(xp_core.in_lseek==Seek) {
	MP_UNIT(_xp_id,"Post seek");
	if(xp_is_bad_pts) mpeg_timer=HUGE;
	xp_core.in_lseek=NoSeek;
	MP_UNIT(_xp_id,"dec_ahead 3");
    }
    if(in_size<0) {
	xp_core.video->fra[xp_core.video->decoder_idx].eof=1;
	xp_eof=1;
	if(xp_core.in_lseek) {
	    xp_eof=0;
	    continue;
	}
	break;
    }
    /* in_size==0: it's or broken stream or demuxer's bug */
    if(in_size==0 && !pthread_end_of_work) {
	xp_core.in_lseek=NoSeek;
	continue;
    }
    /* frame was decoded into current decoder_idx */
    if(xp_is_bad_pts) {
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
	if(cur_time - mpxp_seek_time > (xp_num_frames/sh_video->fps)*100) xp_n_frame_to_drop=compute_frame_dropping(v_pts,drop_barrier);
    } /* if( mp_conf.frame_dropping ) */
    if(xp_core.in_lseek!=NoSeek) continue;
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
    if(output_quality) {
	unsigned total = xp_num_frames/2;
	unsigned distance = dae_get_decoder_outrun(xp_core.video);
	int our_quality;
	our_quality = output_quality*distance/total;
	if(drop_param) mpcv_set_quality(sh_video,0);
	else
	if(mp_conf.autoq) mpcv_set_quality(sh_video,our_quality>0?our_quality:0);
    }
    blit_frame=mpcv_decode(sh_video,start,in_size,drop_param,v_pts);
    if(output_quality) {
	if(drop_param) mpcv_set_quality(sh_video,output_quality);
    }
    if(!blit_frame && drop_param) xp_drop_frame_cnt++;
    if(blit_frame) {
	unsigned idx=dae_curr_vdecoded();
	if(xp_is_bad_pts)
	    xp_core.video->fra[idx].v_pts=mpeg_timer;
	else
	    xp_core.video->fra[idx].v_pts = v_pts;
	xp_core.video->fra[idx].stream_pts = v_pts;
	xp_core.video->fra[idx].duration=duration;
	xp_core.video->fra[idx].eof=0;
	if(!xp_is_bad_pts) {
	    int _idx = dae_prev_vdecoded();
	    xp_core.video->fra[_idx].duration=v_pts-xp_core.video->fra[_idx].v_pts;
	}
	if(mp_conf.frame_reorder) reorder_pts_in_mpeg();
    } /* if (blit_frame) */

    /* ------------ sleep --------------- */
    /* sleep if thread is too fast ;) */
    if(blit_frame)
    while(!dae_inc_decoded(xp_core.video)) {
	MSG_T("DEC_AHEAD: sleep: player=%i decoder=%i)\n"
	    ,dae_curr_vplayed(),dae_curr_vdecoded());
	if(pthread_end_of_work) goto pt_exit;
	if(xp_core.in_lseek!=NoSeek) break;
	if(xp_core.has_audio && mp_conf.xp<XP_VAFull) {
	    MP_UNIT(_xp_id,"decode audio");
	    xp_thread_decode_audio();
	    MP_UNIT(_xp_id,"dec_ahead 5");
	}
	usleep(1);
    }
/*------------------------ frame decoded. --------------------*/
} /* while(!xp_eof)*/

if(xp_core.has_audio && mp_conf.xp<XP_VAFull) {
    while(!xp_audio_eof && !xp_core.in_lseek && !pthread_end_of_work) {
	MP_UNIT(_xp_id,"decode audio");
	if(!xp_thread_decode_audio()) usleep(1);
	MP_UNIT(_xp_id,NULL);
    }
}
  pt_exit:
  MSG_T("\nDEC_AHEAD: leaving...\n");
  pthread_is_living=0;
  pthread_end_of_work=0;
  uninit_signal_handling(_xp_id);
  return arg; /* terminate thread here !!! */
}

/* this routine decodes audio only */
any_t* a_dec_ahead_routine( any_t* arg )
{
    int xp_id;
    int ret, retval;
    struct timeval now;
    struct timespec timeout;
    float d;

    a_pthread_is_living=1;
    xp_eof = 0;
    xp_audio_eof=0;
    MSG_T("\nDEC_AHEAD: entering...\n");
    xp_id=init_signal_handling(sig_audio_decode,uninit_dec_ahead);
    MP_UNIT(xp_id,"dec_ahead");
    pinfo[xp_id].thread_name = "audio decoding+af ahead";

    dec_ahead_can_adseek=0;
    while(!a_pthread_end_of_work) {
	MP_UNIT(xp_id,"decode audio");
	while((ret = xp_thread_decode_audio()) == 2) /* Almost empty buffer */
	    if(xp_audio_eof) break;
	
	dec_ahead_can_adseek=1;
	
	if(a_pthread_end_of_work)
	    break;
	
	MP_UNIT(xp_id,"sleep");
	LOCK_AUDIO_DECODE();
	if( !xp_core.in_lseek && !a_pthread_end_of_work) {
	    if(xp_audio_eof) {
		MP_UNIT(xp_id,"wait end of work");
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
		    if(xp_core.in_pause)
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
	
	if(a_pthread_end_of_work)
	    break;
	
	MP_UNIT(xp_id,"seek");
	LOCK_AUDIO_DECODE();
	while( xp_core.in_lseek!=NoSeek && !a_pthread_end_of_work) {
	    gettimeofday(&now,NULL);
	    timeout.tv_nsec = now.tv_usec * 1000;
	    timeout.tv_sec = now.tv_sec + 1;
	    retval = pthread_cond_timedwait( &audio_decode_cond, &audio_decode_mutex, &timeout );
	    if( retval == ETIMEDOUT )
		MSG_V("Audio decode seek timeout\n");
	}
	dec_ahead_can_adseek = 0; /* Not safe to seek */
	UNLOCK_AUDIO_DECODE();
    }
    MP_UNIT(xp_id,"exit");
    dec_ahead_can_adseek = 1;
    a_pthread_is_living=0;
    a_pthread_end_of_work=0;
    uninit_signal_handling(xp_id);
    return arg; /* terminate thread here !!! */
}

void uninit_dec_ahead( int force )
{
  if(pthread_id && pthread_is_living && xp_core.has_video)
  {
    pthread_end_of_work=1;
    while(pthread_is_living && !force) usleep(0);
    pthread_is_living=0;
    pthread_attr_destroy(&our_attr);
    dae_uninit(xp_core.video);
    xp_core.has_video=0;
  }

  if(xp_core.has_audio && !force) { /* audio state doesn't matter on segfault :( */
      a_pthread_end_of_work=1;
      xp_audio_eof=1;
      if(a_pthread_is_living) {
	  __MP_SYNCHRONIZE(audio_decode_mutex,pthread_cond_signal(&audio_decode_cond));
      }
      while(a_pthread_is_living && !force) usleep(0);
      a_pthread_is_living=0;
      if( pthread_audio_is_living ) {
	  pthread_audio_end_of_work=1;
	  LOCK_AUDIO_PLAY();
	  pthread_cond_signal(&audio_play_cond);
	  uninit_audio_buffer();
	  xp_core.has_audio=0;
	  UNLOCK_AUDIO_PLAY();
	  while(pthread_audio_is_living && !force)
	      usleep(0);
	  pthread_audio_is_living=0;
	  pthread_attr_destroy(&audio_attr);
      }
      if(xp_core.has_audio) {
	  uninit_audio_buffer();
	  xp_core.has_audio=0;
      }
  }
  xp_core_uninit();
}

/* Min audio buffer to keep free, used to tell differ between full and empty buffer */
#define MIN_BUFFER_RESERV 8

int init_dec_ahead(sh_video_t *shv, sh_audio_t *sha)
{
  pthread_attr_init(&our_attr);
  xp_core_init();
  if(shv) {
    sh_video = shv;
    xp_core.has_video=1;
    xp_core.video=malloc(sizeof(dec_ahead_engine_t));
    dae_init(xp_core.video,xp_num_frames);
  }
  else {/* if (mp_conf.xp >= XP_VAFull) mp_conf.xp = XP_VAPlay;*/ }
  if(sha) sh_audio = sha; /* currently is unused */

  if(mp_conf.xp>=XP_VideoAudio && sha) {
    int asize;
    unsigned o_bps;
    unsigned min_reserv;
      o_bps=sh_audio->afilter_inited?sh_audio->af_bps:sh_audio->o_bps;
      if(xp_core.has_video)	asize = max(3*sha->audio_out_minsize,max(3*MAX_OUTBURST,o_bps*xp_num_frames/sh_video->fps))+MIN_BUFFER_RESERV;
      else		asize = o_bps*ao_da_buffs;
      /* FIXME: get better indices from asize/real_audio_packet_size */
      min_reserv = sha->audio_out_minsize;
      if (o_bps > sha->o_bps)
          min_reserv = (float)min_reserv * (float)o_bps / (float)sha->o_bps;
      init_audio_buffer(asize+min_reserv,min_reserv+MIN_BUFFER_RESERV,asize/(sha->audio_out_minsize<10000?sha->audio_out_minsize:4000)+100,sha);
      xp_core.has_audio=1;
      if( mp_conf.xp >= XP_VAPlay )
	  pthread_attr_init(&audio_attr);
  }

  return 0;
}

int run_dec_ahead( void )
{
  int retval;
  retval = pthread_attr_setdetachstate(&our_attr,PTHREAD_CREATE_DETACHED);
  if(retval)
  {
    if(mp_conf.verbose) printf("running thread: attr_setdetachstate fault!!!\n");
    return retval;
  }
  pthread_attr_setscope(&our_attr,PTHREAD_SCOPE_SYSTEM);

  if( xp_core.has_audio && mp_conf.xp >= XP_VAPlay ) {
      retval = pthread_attr_setdetachstate(&audio_attr,PTHREAD_CREATE_DETACHED);
      if(retval) {
	  if(mp_conf.verbose) printf("running audio thread: attr_setdetachstate fault!!!\n");
	  return retval;
      }
      pthread_attr_setscope(&audio_attr,PTHREAD_SCOPE_SYSTEM);
  }

#if 0
  /* requires root privelegies */
  pthread_attr_setschedpolicy(&our_attr,SCHED_FIFO);
#endif
  if( (xp_core.has_audio && mp_conf.xp >= XP_VAFull) || !xp_core.has_video )
  {
	retval =	pthread_create(&pthread_id,&audio_attr,a_dec_ahead_routine,NULL);
	if( retval ) return retval;
	while(!a_pthread_is_living) usleep(0);
  }
  if( xp_core.has_video ) {
      retval = pthread_create(&pthread_id,&our_attr,Va_dec_ahead_routine,NULL);
      if(retval) return retval;
      while(!pthread_is_living) usleep(0);
  }
  return 0;
}

int run_xp_aplayers(void)
{
  int retval;
  if( xp_core.has_audio && mp_conf.xp >= XP_VAPlay )
  {
	retval =	pthread_create(&pthread_id,&audio_attr,audio_play_routine,NULL);
	if( retval ) return retval;
	while(!pthread_audio_is_living) usleep(0);
  }
  return 0;
}


/* Halt threads before seek */
void dec_ahead_halt_threads(int is_reset_vcache)
{
    xp_core.in_lseek = PreSeek;

    if(pthread_audio_is_living) {
	LOCK_AUDIO_PLAY();
	while(!dec_ahead_can_aseek)
	    usleep(1);
	UNLOCK_AUDIO_PLAY();
    }

    if(a_pthread_is_living) {
	LOCK_AUDIO_DECODE();
	while(!dec_ahead_can_adseek)
	    usleep(1);
	UNLOCK_AUDIO_DECODE();
    }

    if(pthread_is_living) {
	while(xp_core.in_lseek==PreSeek)
	    usleep(1);
    }
    xp_core.in_lseek = Seek;
}

/* Restart threads after seek */
void dec_ahead_restart_threads(int xp_id)
{
    /* reset counters */
    dae_reset(xp_core.video);
    /* tempoarry solution */
    reset_audio_buffer();
    /* Ugly hack: but we should read audio packet before video after seeking.
       Else we'll get picture destortion on the screen */
    initial_audio_pts=HUGE;

    if(mp_conf.xp && !pthread_is_living && !a_pthread_is_living) {
        xp_core.in_lseek = NoSeek; /* Threads not started, do nothing */
        return;
    }
    if(!xp_core.has_audio)
	decore_audio(xp_id);
    else
	xp_thread_decode_audio();

    if(pthread_is_living) {
	while(xp_core.in_lseek==Seek) usleep(1);
	while(dae_curr_vdecoded() == dae_curr_vplayed() && !xp_eof)
	    usleep(1); /* Wait for thread to decode first frame */
    }

    xp_core.in_lseek = NoSeek;

    if(a_pthread_is_living)
	__MP_SYNCHRONIZE(audio_decode_mutex,pthread_cond_signal(&audio_decode_cond));

    if(pthread_audio_is_living)
	__MP_SYNCHRONIZE(audio_play_mutex,pthread_cond_signal(&audio_play_cond));
}

/* Audio stuff */
volatile float dec_ahead_audio_delay;
int xp_thread_decode_audio()
{
    int free_buf, vbuf_size, pref_buf;
    unsigned len=0;

    if(xp_core.in_lseek) {
	xp_audio_eof = 0;
	reset_audio_buffer();
	decode_audio_buffer(MAX_OUTBURST);
	return 1;
    }

    free_buf = get_free_audio_buffer();

    if( free_buf == -1 ) { /* End of file */
	xp_audio_eof = 1;
	return 0;
    }
    if( free_buf < (int)sh_audio->audio_out_minsize ) /* full */
	return 0;

    len = get_len_audio_buffer();

    if( len < MAX_OUTBURST ) /* Buffer underrun */
	return decode_audio_buffer(MAX_OUTBURST);

    if(xp_core.has_video) {
	/* Match video buffer */
	vbuf_size = dae_get_decoder_outrun(xp_core.video);
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

#define MIN_AUDIO_TIME 0.05
#define NOTHING_PLAYED (-1.0)
#define XP_MIN_TIMESLICE 0.010 /* under Linux on x86 min time_slice = 10 ms */

extern ao_data_t* ao_data;
any_t* audio_play_routine( any_t* arg )
{
    int xp_id;
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

    xp_id=init_signal_handling(sig_audio_play,uninit_dec_ahead);
    MP_UNIT(xp_id,"audio_play_routine");
    pinfo[xp_id].thread_name = "audio play thread";
    pthread_audio_is_living=1;
    dec_ahead_can_aseek=0;

    while(!pthread_audio_end_of_work) {
	MP_UNIT(xp_id,"audio decore_audio");
	dec_ahead_audio_delay = NOTHING_PLAYED;
	eof = decore_audio(xp_id);

	if(pthread_audio_end_of_work)
	    break;

	MP_UNIT(xp_id,"audio sleep");

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
	} else if( !xp_audio_eof && collect_samples) {
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
	if( !xp_core.in_lseek && d > 0 ) {
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

	if(pthread_audio_end_of_work)
	    break;

	LOCK_AUDIO_PLAY();
	if(eof && !pthread_audio_end_of_work) {
	    MP_UNIT(xp_id,"wait end of work");
	    pthread_cond_wait( &audio_play_cond, &audio_play_mutex );
	}
	UNLOCK_AUDIO_PLAY();

	if(pthread_audio_end_of_work)
	    break;

	MP_UNIT(xp_id,"audio pause");
	LOCK_AUDIO_PLAY();
	while( xp_core.in_pause && !pthread_audio_end_of_work ) {
	    pthread_cond_wait( &audio_play_cond, &audio_play_mutex );
	}
	UNLOCK_AUDIO_PLAY();

	if(pthread_audio_end_of_work)
	    break;

	MP_UNIT(xp_id,"audio seek");
	LOCK_AUDIO_PLAY();
	while( xp_core.in_lseek!=NoSeek && !pthread_audio_end_of_work) {
	    gettimeofday(&now,NULL);
	    timeout.tv_nsec = now.tv_usec * 1000;
	    timeout.tv_sec = now.tv_sec + 1;
	    retval = pthread_cond_timedwait( &audio_play_cond, &audio_play_mutex, &timeout );
	    if( retval == ETIMEDOUT )
		MSG_V("Audio seek timeout\n");
	}
	dec_ahead_can_aseek = 0; /* Not safe to seek */
	UNLOCK_AUDIO_PLAY();
    }
    fflush(stdout);
    MP_UNIT(xp_id,"audio exit");
    dec_ahead_can_aseek=1;
    pthread_audio_is_living=0;
    pthread_audio_end_of_work=0;
    uninit_signal_handling(xp_id);
    return arg;
}

void dec_ahead_reset_sh_video(sh_video_t *shv)
{
    sh_video->vfilter = shv->vfilter;
}

extern void __exit_sighandler(void);
extern void killall_threads(pthread_t pth_id);
void sig_dec_ahead_video( void )
{
    MSG_T("sig_dec_ahead_video\n");
    mp_msg_flush();

    xp_eof = 1;
    xp_core.video->fra[dae_curr_vdecoded()].eof=1;
    /*
	Unlock all mutex
	( man page says it may deadlock, but what is worse deadlock here or later? )
    */
    pthread_is_living=0;
    xp_core.in_lseek=NoSeek;
    killall_threads(pthread_self());
    __exit_sighandler();
}

void sig_audio_play( void )
{
    MSG_T("sig_audio_play\n");
    mp_msg_flush();

    dec_ahead_can_aseek=1;
    pthread_audio_is_living=0;

    UNLOCK_AUDIO_PLAY();

    killall_threads(pthread_self());
    __exit_sighandler();
}

void sig_audio_decode( void )
{
    MSG_T("sig_audio_decode\n");
    mp_msg_flush();

    dec_ahead_can_adseek=1;
    a_pthread_is_living=0;

    UNLOCK_AUDIO_DECODE();

    killall_threads(pthread_self());
    __exit_sighandler();
}
