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
#include "dec_ahead.h"
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
pthread_mutex_t vdecoding_mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t vreading_mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t vdec_active_mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t vdec_locked_mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t vdeca_mutex=PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t seek_mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t seek_cond_mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t seek_cond=PTHREAD_COND_INITIALIZER;

pthread_mutex_t audio_play_mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t audio_play_cond=PTHREAD_COND_INITIALIZER;

pthread_mutex_t audio_decode_mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t audio_decode_cond=PTHREAD_COND_INITIALIZER;

pthread_mutex_t video_decode_mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t video_decode_cond=PTHREAD_COND_INITIALIZER;

extern volatile int xp_drop_frame;
extern volatile unsigned xp_drop_frame_cnt;
extern int output_quality;
extern int auto_quality;

int ao_da_buffs;

volatile int dec_ahead_locked_frame=0;
volatile unsigned abs_dec_ahead_locked_frame=0;
volatile unsigned abs_dec_ahead_blitted_frame=0;
extern volatile unsigned abs_dec_ahead_active_frame;
extern volatile unsigned dec_ahead_active_frame;
extern volatile unsigned loc_dec_ahead_active_frame;
extern volatile float xp_screen_pts;
volatile int dec_ahead_in_lseek=NoSeek;
volatile int dec_ahead_can_aseek=0;  /* It is safe to seek audio */
volatile int dec_ahead_can_adseek=1;  /* It is safe to seek audio buffer thread */
volatile int dec_ahead_in_pause=0;
volatile int dec_ahead_in_resize=0;
volatile float dec_ahead_seek_num_frames=0;	  /* frames played after seek */
volatile int dec_ahead_seek_num_frames_decoded=0; /* frames decoded after seek */
volatile int dec_ahead_num_frames_decoded=0;	  /* frames decoded by thread */

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
extern int frame_dropping;

pid_t dec_ahead_pid; /* Only for testing */
pthread_t dec_ahead_pth_id;

extern void update_osd( float v_pts );
shva_t *shva;
volatile int xp_eof=0;
int xp_audio_eof=0;
int has_xp_audio=0;
int has_xp_video=0;
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
any_t* Va_dec_ahead_routine( any_t* arg )
{
    float duration=0;
    float drop_barrier;
    int blit_frame=0;
    int drop_param=0;
    volatile unsigned da_active_frame,lda_active_frame,ada_active_frame;
    unsigned xp_n_frame_to_drop;
    int _xp_id;
    static float prev_delta=0;
    float v_pts,mpeg_timer=HUGE;
    pthread_is_living=1;
    xp_eof = 0;
    xp_audio_eof=0;
    MSG_T("\nDEC_AHEAD: entering...\n");
    _xp_id=init_signal_handling(sig_dec_ahead_video,uninit_dec_ahead);
    pinfo[_xp_id].current_module = "dec_ahead";
    dec_ahead_pid =
    pinfo[_xp_id].pid = getpid(); /* Only for testing */
    dec_ahead_pth_id =
    pinfo[_xp_id].pth_id = pthread_self();
    pinfo[_xp_id].thread_name = (has_xp_audio && enable_xp < XP_VAFull) ? "video+audio decoding+filtering ahead" : "video decoding+vf ahead";
    prev_delta=xp_num_frames;
    drop_barrier=(float)(xp_num_frames/2)*(1/vo.fps);
    if(av_sync_pts == -1 && !use_pts_fix2)
	xp_is_bad_pts = d_video->demuxer->file_format == DEMUXER_TYPE_MPEG_ES ||
			d_video->demuxer->file_format == DEMUXER_TYPE_MPEG4_ES ||
			d_video->demuxer->file_format == DEMUXER_TYPE_H264_ES ||
			d_video->demuxer->file_format == DEMUXER_TYPE_MPEG_PS ||
			d_video->demuxer->file_format == DEMUXER_TYPE_MPEG_TS;
    else
	xp_is_bad_pts = av_sync_pts?0:1;
while(!xp_eof){
    if(pthread_end_of_work) break;
    /*--------------------  Decode a frame: -----------------------*/
    {	unsigned char* start=NULL;
	int in_size;
	if(dec_ahead_in_lseek==PreSeek) {
	    pinfo[_xp_id].current_module = "Pre seek";
	    LOCK_VIDEO_DECODE();
	    dec_ahead_in_lseek=Seek;
	    pthread_cond_wait( &video_decode_cond, &video_decode_mutex );
	    UNLOCK_VIDEO_DECODE();
	}
	pinfo[_xp_id].current_module = "dec_ahead 1";
	/* get it! */
	LOCK_VREADING();
	if(dec_ahead_in_lseek==Seek)
	{ /* Get info from player after a seek */
	    //*((char*)0x100) = 1; // Testing crash
	    sh_video->num_frames = dec_ahead_seek_num_frames;
	    sh_video->num_frames_decoded = dec_ahead_seek_num_frames_decoded;
	}
	/* prevent reent access to non-reent demuxer */
	//if(sh_video->num_frames>200)  *((char*)0x100) = 1; // Testing crash
	if(has_xp_audio && enable_xp<XP_VAFull) {
	    pinfo[_xp_id].current_module = "decode audio";
	    while(2==xp_thread_decode_audio()) ;
	    pinfo[_xp_id].current_module = "dec_ahead 2";
	}
	in_size=video_read_frame_r(sh_video,&duration,&v_pts,&start,vo.fps);
	UNLOCK_VREADING();
	if(dec_ahead_in_lseek==Seek)
	{
	    pinfo[_xp_id].current_module = "Post seek";
	    /* reset counters */
	    vo_get_active_frame(&dec_ahead_locked_frame);
	    LOCK_VDEC_ACTIVE();
	    LOCK_VDEC_LOCKED();
	    abs_dec_ahead_locked_frame = abs_dec_ahead_active_frame;
	    abs_dec_ahead_blitted_frame = loc_dec_ahead_active_frame;
	    dec_ahead_locked_frame = dec_ahead_active_frame;
	    UNLOCK_VDEC_LOCKED();
	    UNLOCK_VDEC_ACTIVE();
	    if(xp_is_bad_pts) mpeg_timer=HUGE;
	    dec_ahead_in_lseek=NoSeek;
	    MSG_T("\nDEC_AHEAD: reset counters to (%u %u) due lseek\n",dec_ahead_locked_frame,abs_dec_ahead_locked_frame);
	    pinfo[_xp_id].current_module = "dec_ahead 3";
	}
	if(in_size<0)
	{
	    __MP_SYNCHRONIZE(vdeca_mutex,shva[dec_ahead_locked_frame].eof=1);
	    xp_eof=1;
	    if(has_xp_audio && enable_xp<XP_VAFull) {
		while(!xp_audio_eof && !dec_ahead_in_lseek && !pthread_end_of_work) {
		    pinfo[_xp_id].current_module = "decode audio";
		    if(!xp_thread_decode_audio())
			usleep(1);
		    pinfo[_xp_id].current_module = NULL;
		}
		if(dec_ahead_in_lseek) {
		    xp_eof=0;
		    continue;
		}
	    }
	    LOCK_VIDEO_DECODE();
	    if(!pthread_end_of_work) {
		pinfo[_xp_id].current_module = "wait for end of work";
		pthread_cond_wait(&video_decode_cond,&video_decode_mutex);
	    }
	    UNLOCK_VIDEO_DECODE();
	    if(dec_ahead_in_lseek) {
		xp_eof=0;
		continue;
	    }
	    break;
	}
	/* in_size==0: it's or broken stream or demuxer's bug */
	if(in_size==0 && !pthread_end_of_work) {
	    dec_ahead_in_lseek=NoSeek;
	    continue;
	}
	if(xp_is_bad_pts)
	{
	    if(mpeg_timer==HUGE)mpeg_timer=v_pts;
		else if( mpeg_timer-duration<v_pts ) {
		    mpeg_timer=v_pts;
		    MSG_DBG2("Sync mpeg pts %f\n", mpeg_timer);
	    } else		mpeg_timer+=duration;
	}
	vo_set_decoding_frame_num(&dec_ahead_locked_frame);
	/* ----------- compute frame dropping ------------- */
	LOCK_VDEC_ACTIVE();
	ada_active_frame= abs_dec_ahead_active_frame;
	lda_active_frame= loc_dec_ahead_active_frame;
	da_active_frame = dec_ahead_active_frame;
	UNLOCK_VDEC_ACTIVE();
	xp_n_frame_to_drop=0;
	if(frame_dropping)
	{
	    int cur_time;
	    cur_time = GetTimerMS();
	    /* Ugly solution: disable frame dropping right after seeking! */
	    if(cur_time - mpxp_seek_time > (xp_num_frames/vo.fps)*100 &&
	       ada_active_frame>=xp_num_frames)
	    {
		float delta,max_frame_delay;/* delay for decoding of top slow frame */
		/*
		ada_active_frame	   - abs frame num. which is being displayed
		abs_dec_ahead_locked_frame - abs frame num. which is being decoded
		*/
		max_frame_delay = max_video_time_usage+max_vout_time_usage;
		
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
		if(max_frame_delay*3 > drop_barrier)
		{
		    if(drop_barrier < (float)(xp_num_frames-2)/vo.fps) drop_barrier += 1/vo.fps;
		    else
		    if(verbose)
		    {
			static int warned=0;
			static float prev_warn_delay=0;
			if(!warned || max_frame_delay > prev_warn_delay)
			{
			    warned=1;
			    MSG_WARN("*********************************************\n"
				     "** Can't stabilize A-V sync!!!             **\n"
				     "*********************************************\n"
				     "Try increase number of buffer for decoding ahead\n"
				     "Exist: %u, need: %u\n"
				     ,xp_num_frames,(unsigned)(max_frame_delay*3*vo.fps)+3);
			    prev_warn_delay=max_frame_delay;
			}
		    }
		}
		if(delta > drop_barrier) xp_n_frame_to_drop=0;
		else
		if(delta < max_frame_delay*3) xp_n_frame_to_drop=1;
		else
		{
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
		    xp_n_frame_to_drop = (abs_dec_ahead_locked_frame%fr_skip_divisor)?0:1;
		    if(delta>prev_delta) xp_n_frame_to_drop=0;
		}
		MSG_D("DEC_AHEAD: max_frame_delay*3=%f drop_barrier=%f prev_delta=%f delta=%f(v_pts=%f screen_pts=%f) n_fr_to_drop=%u\n",max_frame_delay*3,drop_barrier,prev_delta,delta,v_pts,xp_screen_pts,xp_n_frame_to_drop);
		prev_delta=delta;
	    }
	}
	/* ------------ sleep --------------- */
	/* sleep if thread is too fast ;) */
	while(abs_dec_ahead_blitted_frame >= lda_active_frame+xp_num_frames-2)
	{
	    MSG_T("DEC_AHEAD: sleep (abs (blitted(%u)>=active+xp-2(%u)))\n"
		,abs_dec_ahead_blitted_frame,lda_active_frame+xp_num_frames-2);
	    if(pthread_end_of_work) goto pt_exit;
	    if(dec_ahead_in_lseek!=NoSeek) break;
	    if(has_xp_audio && enable_xp<XP_VAFull) {
		pinfo[_xp_id].current_module = "decode audio";
		xp_thread_decode_audio();
		pinfo[_xp_id].current_module = "dec_ahead 5";
	    }
	    usleep(1);
	    LOCK_VDEC_ACTIVE();
	    lda_active_frame= loc_dec_ahead_active_frame;
	    da_active_frame = dec_ahead_active_frame;
	    UNLOCK_VDEC_ACTIVE();
	}
	if(dec_ahead_in_lseek!=NoSeek) continue;
	LOCK_VDECODING();
	if(dec_ahead_in_lseek!=NoSeek) {
	    UNLOCK_VDECODING();
	    continue;
	}
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
	if(xp_n_frame_to_drop)	drop_param=frame_dropping;
	else			drop_param=0;
	/* decode: */
	MSG_T("\nDEC_AHEAD: decode to %u (abs (blitted(%u)>=active+xp-2(%u)))\n"
		,abs_dec_ahead_blitted_frame,abs_dec_ahead_locked_frame,lda_active_frame+xp_num_frames-2);
	if(output_quality)
	{
	    unsigned total = xp_num_frames/2;
	    unsigned distance = abs_dec_ahead_blitted_frame-lda_active_frame;
	    int our_quality;
	    our_quality = output_quality*distance/total;
	    if(drop_param) set_video_quality(sh_video,0);
	    else
	    if(auto_quality) set_video_quality(sh_video,our_quality>0?our_quality:0);
	}
	blit_frame=decode_video(sh_video,start,in_size,drop_param,v_pts);
	if(output_quality)
	{
	    if(drop_param) set_video_quality(sh_video,output_quality);
	}
	if(!blit_frame && drop_param) xp_drop_frame_cnt++;
	UNLOCK_VDECODING();
	if(dec_ahead_in_resize)
	{
	    dec_ahead_in_resize=0;
	}
#ifdef ENABLE_DEC_AHEAD_DEBUG
	if(verbose)
	{
	MSG_T("\nDEC_AHEAD: frame %u decoded (blit=%u blit_param=%u size=%i)\n"
	,abs_dec_ahead_locked_frame,blit_frame,drop_param,in_size);
	}
#endif
	LOCK_VDEC_LOCKED();
	abs_dec_ahead_locked_frame++;
	if(blit_frame)
	{
	    LOCK_VDECA();
	    if(xp_is_bad_pts)
	        shva[dec_ahead_locked_frame].v_pts=mpeg_timer;
	    else
		shva[dec_ahead_locked_frame].v_pts = v_pts;
	    shva[dec_ahead_locked_frame].stream_pts = v_pts;
	    shva[dec_ahead_locked_frame].duration=duration;
	    shva[dec_ahead_locked_frame].eof=0;
	    shva[dec_ahead_locked_frame].num_frames = sh_video->num_frames;
	    shva[dec_ahead_locked_frame].num_frames_decoded = sh_video->num_frames_decoded;
	    if(!xp_is_bad_pts) {
                int idx = dec_ahead_locked_frame>0?dec_ahead_locked_frame-1:xp_num_frames-1;
		shva[idx].duration=v_pts-shva[idx].v_pts;
            }
	    if(frame_reorder) { /* Reorder pts in mpeg streams */
		int idx0=0, idx1, idx2, idx3;
		
		idx1 = dec_ahead_locked_frame;
		idx2 = dec_ahead_locked_frame>0?dec_ahead_locked_frame-1:xp_num_frames-1;
		while( dec_ahead_active_frame != idx2 &&
		       shva[idx2].v_pts > shva[idx1].v_pts &&
		       shva[idx2].v_pts < shva[idx1].v_pts+1.0 ) {
		    float tmp;
		    tmp = shva[idx1].v_pts;
		    shva[idx1].v_pts = shva[idx2].v_pts;
		    shva[idx2].v_pts = tmp;

		    shva[idx1].stream_pts = shva[idx1].v_pts;
		    shva[idx2].stream_pts = shva[idx2].v_pts;
		    shva[idx2].duration = shva[idx1].v_pts - shva[idx2].v_pts;

		    idx3=idx2-1;
                    if(idx3<0)
                        idx3 = xp_num_frames-1;
                    if(shva[idx2].v_pts > shva[idx3].v_pts &&
                       shva[idx2].v_pts - shva[idx3].v_pts < 1.0)
                        shva[idx3].duration = shva[idx2].v_pts - shva[idx3].v_pts;

		    if(idx1 != dec_ahead_locked_frame)
			shva[idx1].duration = shva[idx0].v_pts - shva[idx1].v_pts;

		    idx0 = idx1;
		    idx1 = idx2;
		    idx2--;
		    if(idx2<0)
			idx2 = xp_num_frames-1;
		}
	    }

	    dec_ahead_num_frames_decoded = sh_video->num_frames_decoded;
	    UNLOCK_VDECA();
	    dec_ahead_locked_frame=(dec_ahead_locked_frame+1)%xp_num_frames;
	    abs_dec_ahead_blitted_frame++;
	}
	else 
	  if(in_size && !drop_param)
	  /* do not count broken frames */
		abs_dec_ahead_locked_frame--;
	UNLOCK_VDEC_LOCKED();
    }
    /*------------------------ frame decoded. --------------------*/
} /* while(!xp_eof)*/
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
    pinfo[xp_id].current_module = "dec_ahead";
    pinfo[xp_id].thread_name = "audio decoding+af ahead";

    dec_ahead_can_adseek=0;
    while(!a_pthread_end_of_work) {
	pinfo[xp_id].current_module = "decode audio";
	while((ret = xp_thread_decode_audio()) == 2) /* Almost empty buffer */
	    if(xp_audio_eof) break;
	
	dec_ahead_can_adseek=1;
	
	if(a_pthread_end_of_work)
	    break;
	
	pinfo[xp_id].current_module = "sleep";
	LOCK_AUDIO_DECODE();
	if( !dec_ahead_in_lseek && !a_pthread_end_of_work) {
	    if(xp_audio_eof) {
		pinfo[xp_id].current_module = "wait end of work";
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
		    if(dec_ahead_in_pause)
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
	
	pinfo[xp_id].current_module = "seek";
	LOCK_AUDIO_DECODE();
	while( dec_ahead_in_lseek!=NoSeek && !a_pthread_end_of_work) {
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
    pinfo[xp_id].current_module = "exit";
    dec_ahead_can_adseek = 1;
    a_pthread_is_living=0;
    a_pthread_end_of_work=0;
    uninit_signal_handling(xp_id);
    return arg; /* terminate thread here !!! */
}

void uninit_dec_ahead( int force )
{
  if(pthread_id && pthread_is_living && has_xp_video) 
  {
    pthread_end_of_work=1;
    __MP_SYNCHRONIZE(video_decode_mutex,pthread_cond_signal(&video_decode_cond));
    while(pthread_is_living && !force) usleep(0);
    pthread_is_living=0;
    pthread_attr_destroy(&our_attr);
    free(shva);
    shva=NULL;
    has_xp_video=0;
  }

  if(has_xp_audio && !force) { /* audio state doesn't matter on segfault :( */
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
	  has_xp_audio=0;
	  UNLOCK_AUDIO_PLAY();
	  while(pthread_audio_is_living && !force)
	      usleep(0);
	  pthread_audio_is_living=0;
	  pthread_attr_destroy(&audio_attr);
      }
      if(has_xp_audio) {
	  uninit_audio_buffer();
	  has_xp_audio=0;
      }
  }
}

/* Min audio buffer to keep free, used to tell differ between full and empty buffer */
#define MIN_BUFFER_RESERV 8

int init_dec_ahead(sh_video_t *shv, sh_audio_t *sha)
{
  pthread_attr_init(&our_attr);
  if(shv) { sh_video = shv; has_xp_video=1; }
  else {/* if (enable_xp >= XP_VAFull) enable_xp = XP_VAPlay;*/ }
  if(!(shva = malloc(sizeof(shva_t)*xp_num_frames))) return ENOMEM;
  if(sha) sh_audio = sha; /* currently is unused */
  dec_ahead_locked_frame=0;
  abs_dec_ahead_locked_frame=0;
  abs_dec_ahead_blitted_frame=0;
  dec_ahead_in_lseek=NoSeek;
  dec_ahead_in_resize=0;

  if(enable_xp>=XP_VideoAudio && sha) {
    int asize;
    unsigned o_bps;
    unsigned min_reserv;
      o_bps=sh_audio->afilter_inited?sh_audio->af_bps:sh_audio->o_bps;
      if(has_xp_video)	asize = max(3*sha->audio_out_minsize,max(3*MAX_OUTBURST,o_bps*xp_num_frames/vo.fps))+MIN_BUFFER_RESERV;
      else		asize = o_bps*ao_da_buffs;
      /* FIXME: get better indices from asize/real_audio_packet_size */
      min_reserv = sha->audio_out_minsize;
      if (o_bps > sha->o_bps)
          min_reserv = (float)min_reserv * (float)o_bps / (float)sha->o_bps;
      init_audio_buffer(asize+min_reserv,min_reserv+MIN_BUFFER_RESERV,asize/(sha->audio_out_minsize<10000?sha->audio_out_minsize:4000)+100,sha);
      has_xp_audio=1;
      if( enable_xp >= XP_VAPlay )
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
    if(verbose) printf("running thread: attr_setdetachstate fault!!!\n");
    return retval;
  }
  pthread_attr_setscope(&our_attr,PTHREAD_SCOPE_SYSTEM);

  if( has_xp_audio && enable_xp >= XP_VAPlay ) {
      retval = pthread_attr_setdetachstate(&audio_attr,PTHREAD_CREATE_DETACHED);
      if(retval) {
	  if(verbose) printf("running audio thread: attr_setdetachstate fault!!!\n");
	  return retval;
      }
      pthread_attr_setscope(&audio_attr,PTHREAD_SCOPE_SYSTEM);
  }

#if 0
  /* requires root privelegies */
  pthread_attr_setschedpolicy(&our_attr,SCHED_FIFO);
#endif
  if( (has_xp_audio && enable_xp >= XP_VAFull) || !has_xp_video )
  {
	retval =	pthread_create(&pthread_id,&audio_attr,a_dec_ahead_routine,NULL);
	if( retval ) return retval;
	while(!a_pthread_is_living) usleep(0);
  }
  if( has_xp_video ) {
      retval = pthread_create(&pthread_id,&our_attr,Va_dec_ahead_routine,NULL);
      if(retval) return retval;
      while(!pthread_is_living) usleep(0);
  }
  return 0;
}

int run_xp_players(void)
{
  int retval;
  if( has_xp_audio && enable_xp >= XP_VAPlay )
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
    dec_ahead_in_lseek = PreSeek;

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

    if (is_reset_vcache)
        UNLOCK_VDECODING(); /* Release lock from vo_x11 */
    if(pthread_is_living) {
	__MP_SYNCHRONIZE(video_decode_mutex,pthread_cond_signal(&video_decode_cond));
	while(dec_ahead_in_lseek==PreSeek)
	    usleep(1);
    }
    dec_ahead_in_lseek = Seek;
}

/* Restart threads after seek */
void dec_ahead_restart_threads(int xp_id)
{
    /* Ugly hack: but we should read audio packet before video after seeking.
       Else we'll get picture destortion on the screen */
    initial_audio_pts=HUGE;

    if(enable_xp && !pthread_is_living && !a_pthread_is_living) {
        dec_ahead_in_lseek = NoSeek; /* Threads not started, do nothing */
        return;
    }
    if(!has_xp_audio)
	decore_audio(xp_id);
    else
	xp_thread_decode_audio();

    if(pthread_is_living) {
	__MP_SYNCHRONIZE(video_decode_mutex,pthread_cond_signal(&video_decode_cond));
	while(dec_ahead_in_lseek==Seek)
	    usleep(1);
	while(abs_dec_ahead_locked_frame == abs_dec_ahead_active_frame && !xp_eof)
	    usleep(1); /* Wait for thread to decode first frame */
    }

    dec_ahead_in_lseek = NoSeek;

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

    if(dec_ahead_in_lseek) {
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

    if(has_xp_video) {
	/* Match video buffer */
	vbuf_size = abs_dec_ahead_locked_frame - abs_dec_ahead_active_frame;
	pref_buf = vbuf_size / vo.fps * sh_audio->af_bps;
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

any_t* audio_play_routine( any_t* arg )
{
    int xp_id;
    int eof = 0;
    struct timeval now;
    struct timespec timeout;
    float d;
    int retval;
    const float MAX_AUDIO_TIME = (float)ao_get_space() / sh_audio->af_bps + ao_get_delay();
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
    pinfo[xp_id].current_module = "audio_play_routine";
    pinfo[xp_id].thread_name = "audio play thread";
    pthread_audio_is_living=1;
    dec_ahead_can_aseek=0;

    while(!pthread_audio_end_of_work) {
	pinfo[xp_id].current_module = "audio decore_audio";
	dec_ahead_audio_delay = NOTHING_PLAYED;
	eof = decore_audio(xp_id);

	if(pthread_audio_end_of_work)
	    break;

	pinfo[xp_id].current_module = "audio sleep";

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
	d = ao_get_delay() - min_audio_time;
	if( !dec_ahead_in_lseek && d > 0 ) {
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
	    pinfo[xp_id].current_module = "wait end of work";
	    pthread_cond_wait( &audio_play_cond, &audio_play_mutex );
	}
	UNLOCK_AUDIO_PLAY();

	if(pthread_audio_end_of_work)
	    break;

	pinfo[xp_id].current_module = "audio pause";
	LOCK_AUDIO_PLAY();
	while( dec_ahead_in_pause && !pthread_audio_end_of_work ) {
	    pthread_cond_wait( &audio_play_cond, &audio_play_mutex );
	}
	UNLOCK_AUDIO_PLAY();

	if(pthread_audio_end_of_work)
	    break;
	    
	pinfo[xp_id].current_module = "audio seek";
	LOCK_AUDIO_PLAY();
	while( dec_ahead_in_lseek!=NoSeek && !pthread_audio_end_of_work) {
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
    pinfo[xp_id].current_module = "audio exit";
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
    shva[dec_ahead_locked_frame].eof=1;
    /*
	Unlock all mutex 
	( man page says it may deadlock, but what is worse deadlock here or later? )
    */
    UNLOCK_VREADING();
    UNLOCK_VDEC_LOCKED();
    UNLOCK_VDEC_ACTIVE();
    UNLOCK_VDECA();
    UNLOCK_VDECODING();
    UNLOCK_VIDEO_DECODE();

    pthread_is_living=0;
    dec_ahead_in_lseek=NoSeek;
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
