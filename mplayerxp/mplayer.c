/* MplayerXP (C) 2000-2002. by A'rpi/ESP-team (C) 2002. by Nickols_K */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#ifdef _OPENMP
#include <omp.h>
#endif
#define __USE_ISOC99 1 /* for lrint */
#include <math.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <signal.h>
#include <time.h>
#include <fcntl.h>

#include "version.h"
#include "mp_config.h"
#include "sig_hand.h"
#include "mplayer.h"
#include "postproc/swscale.h"
#include "postproc/af.h"
#include "postproc/vf.h"
#define HELP_MP_DEFINE_STATIC
#include "help_mp.h"

#include "libmpdemux/stream.h"
#include "playtree.h"
#include "cfgparser.h"
#include "cfg-mplayer-def.h"

#ifdef USE_SUB
#include "subreader.h"
#endif

#include "libvo/video_out.h"
extern void* mDisplay; // Display* mDisplay;

#include "libvo/sub.h"
#include "libao2/audio_out.h"
#include "codec-cfg.h"
#include "dvdauth.h"
#include "spudec.h"
#include "vobsub.h"

#include "osdep/getch2.h"
#include "osdep/keycodes.h"
#include "osdep/timer.h"
#include "osdep/shmem.h"

#include "cpudetect.h"
#include "mm_accel.h"

#include "input/input.h"
#include "dump.h"
#include "nls/nls.h"
#include "m_struct.h"
#include "postproc/libmenu/menu.h"

int slave_mode=0;
unsigned verbose=0;

#define ABS(x) (((x)>=0)?(x):(-(x)))

#define MSGT_CLASS MSGT_CPLAYER
#include "__mp_msg.h"


/**************************************************************************
             Playtree
**************************************************************************/

play_tree_t* playtree;
int shuffle_playback=0;

#define PT_NEXT_ENTRY 1
#define PT_PREV_ENTRY -1
#define PT_NEXT_SRC 2
#define PT_PREV_SRC -2
#define PT_UP_NEXT 3
#define PT_UP_PREV -3

/**************************************************************************
             Config
**************************************************************************/

m_config_t* mconfig;

/**************************************************************************
             Decoding ahead
**************************************************************************/
#include "dec_ahead.h"

int enable_xp=XP_VAPlay;
int enable_gomp=1;
volatile int dec_ahead_active_frame=0;
volatile unsigned abs_dec_ahead_active_frame=0;
volatile unsigned loc_dec_ahead_active_frame=0;
volatile unsigned xp_drop_frame_cnt=0;
unsigned xp_num_frames=0;
float xp_screen_pts;
float playbackspeed_factor=1.0;
int mpxp_seek_time=-1;
unsigned mpxp_after_seek=0;
int audio_eof=0;
demux_stream_t *d_video=NULL;
static int osd_show_framedrop = 0;
static int osd_show_av_delay = 0;
static int osd_show_sub_delay = 0;
static int osd_function=OSD_PLAY;
int output_quality=0;

int xp_id=0;
pid_t mplayer_pid;
pthread_t mplayer_pth_id;

int av_sync_pts=-1;
int av_force_pts_fix=0;
int frame_reorder=1;

int use_pts_fix2=-1;
int av_force_pts_fix2=-1;

/************************************************************************
    Special case: inital audio PTS:
    example: some movies has a_pts = v_pts = XX sec
    but mplayerxp always starts audio playback at 0 sec
************************************************************************/
float initial_audio_pts=HUGE;
initial_audio_pts_correction_t initial_audio_pts_corr;

/*
   Acceleration for codecs
*/
unsigned mplayer_accel=0;

/**************************************************************************
             Config file
**************************************************************************/
static int cfg_inc_verbose(struct config *conf){ UNUSED(conf); ++verbose; return 0;}

static int cfg_include(struct config *conf, char *filename){
	UNUSED(conf);
	return m_config_parse_config_file(mconfig, filename);
}

#include "osdep/get_path.h"

/**************************************************************************
             Input media streaming & demultiplexer:
**************************************************************************/

static int max_framesize=0;

#include "stream.h"
#include "demuxer.h"
#include "stheader.h"
#include "parse_es.h"

#include "libmpcodecs/dec_video.h"
#include "libmpcodecs/dec_audio.h"

/* Common FIFO functions, and keyboard/event FIFO code */
#include "fifo.c"
int use_stdin=0;
/**************************************************************************/

static int vo_inited=0;
static int ao_inited=0;
//ao_functions_t *audio_out=NULL;

/* benchmark: */
double video_time_usage=0;
double vout_time_usage=0;
static double audio_time_usage=0;
double audio_decode_time_usage_correction=0;
double audio_decode_time_usage=0;  /* time for decoding in thread */
double min_audio_decode_time_usage=0;
double max_audio_decode_time_usage=0;
static int total_time_usage_start=0;
int benchmark=0;
static unsigned bench_dropped_frames=0;
double max_demux_time_usage=0;
double demux_time_usage=0;
double min_demux_time_usage=0;
double max_c2_time_usage=0;
double c2_time_usage=0;
double min_c2_time_usage=0;
double max_video_time_usage=0;
double cur_video_time_usage=0;
double min_video_time_usage=0;
double max_vout_time_usage=0;
double cur_vout_time_usage=0;
double min_vout_time_usage=0;
static double max_audio_time_usage=0;
static double cur_audio_time_usage=0;
static double min_audio_time_usage=0;
static float max_av_resync=0;

/* quality's options: */
int auto_quality=0;

int osd_level=2;

// seek:
char *seek_to_sec=NULL;
off_t seek_to_byte=0;
int loop_times=-1;

/* codecs: */
int has_audio=1;
int has_video=1;
char *audio_codec=NULL;  /* override audio codec */
char *audio_codec_param=NULL;
char *video_codec=NULL;  /* override video codec */
char *audio_family=NULL; /* override audio codec family */
char *video_family=NULL; /* override video codec family */

// streaming:
int audio_id=-1;
int video_id=-1;
int dvdsub_id=-1;
int vobsub_id=-1;
char* audio_lang=NULL;
char* dvdsub_lang=NULL;
static char* spudec_ifo=NULL;

// cache2:
int stream_cache_size=0; /*was 1024 let it be 1MB by default */

// dump:
static char *stream_dump=NULL;

// A-V sync:
static float default_max_pts_correction=-1;//0.01f;
static float max_pts_correction=0;//default_max_pts_correction;
static float c_total=0;
static float audio_delay=0;

static int dapsync=0;
static int softsleep=0;

static float force_fps=0;
int force_srate=0;
int frame_dropping=0; // option  0=no drop  1= drop vo  2= drop decode
static int play_n_frames=-1;
static uint32_t our_n_frames=0;

// screen info:
char* video_driver=NULL; //"mga"; // default
char* audio_driver=NULL;

// sub:
char *font_name=NULL;
float font_factor=0.75;
char *sub_name=NULL;
float sub_delay=0;
float sub_fps=0;
int   sub_auto = 1;
char *vobsub_name=NULL;

static stream_t* stream=NULL;

//char* current_module=NULL; // for debugging
int nortc;

static unsigned int inited_flags=0;
#define INITED_VO	0x00000001
#define INITED_AO	0x00000002
#define INITED_GETCH2	0x00000004
#define INITED_LIRC	0x00000008
#define INITED_SPUDEC	0x00000010
#define INITED_STREAM	0x00000020
#define INITED_INPUT	0x00000040
#define INITED_DEMUXER  0x00000080
#define INITED_ACODEC	0x00000100
#define INITED_VCODEC	0x00000200
#define INITED_VOBSUB	0x00000400
#define INITED_SUBTITLE 0x10000000
#define INITED_ALL	0xFFFFFFFF

demux_stream_t *d_audio=NULL;
demux_stream_t *d_dvdsub=NULL;

static sh_audio_t *sh_audio=NULL;
static sh_video_t *sh_video=NULL;
static demuxer_t *demuxer=NULL;
pthread_mutex_t audio_timer_mutex=PTHREAD_MUTEX_INITIALIZER;
#ifdef USE_SUB
static subtitle* subtitles=NULL;
static float sub_last_pts = -303;
#endif
/* XP audio buffer */
typedef struct audio_buffer_index_s {
    float pts;
    int index;
} audio_buffer_index_t;

typedef struct audio_buffer_s {
    unsigned char* buffer;
    int head;
    int tail;
    int len;
    int size;
    int min_reserv;
    int min_len;
    int eof;
    int HasReset;
    int blocked_readers;
    pthread_mutex_t head_mutex;
    pthread_mutex_t tail_mutex;
    pthread_cond_t wait_buffer_cond;
    sh_audio_t *sh_audio;

    audio_buffer_index_t *indices;
    int index_head;
    int index_tail;
    int index_len;
} audio_buffer_t;

audio_buffer_t audio_buffer;

int init_audio_buffer( int size, int min_reserv, int indices, sh_audio_t *sh_audio )
{
    MSG_V("Using audio buffer %i bytes (min reserve = %i, indices %i)\n",size,min_reserv, indices);
    if( !(audio_buffer.buffer = malloc(size)) )
        return ENOMEM;
    if( !(audio_buffer.indices = malloc(indices*sizeof(audio_buffer_index_t))) ) {
        free(audio_buffer.buffer);
        audio_buffer.buffer=NULL;
        return ENOMEM;
    }
    audio_buffer.index_len=indices;
    audio_buffer.index_head=0;
    audio_buffer.index_tail=0;
    audio_buffer.head = 0;
    audio_buffer.tail = 0;
    audio_buffer.len = size;
    audio_buffer.size = size;
    audio_buffer.min_reserv = min_reserv;
    audio_buffer.min_len = size/indices+1;
    audio_buffer.eof = 0;
    audio_buffer.HasReset = 0;
    audio_buffer.blocked_readers = 0;
    pthread_mutex_init( &audio_buffer.head_mutex, NULL);
    pthread_mutex_init( &audio_buffer.tail_mutex, NULL);
    pthread_cond_init( &audio_buffer.wait_buffer_cond, NULL);
    audio_buffer.sh_audio = sh_audio;
    return 0;
}

void uninit_audio_buffer(void)
{
    audio_buffer.eof = 1;

    if( audio_buffer.blocked_readers > 0 ) {     /* Make blocked reader exit */
        int loops = 10;
        pthread_cond_broadcast( &audio_buffer.wait_buffer_cond );
        while( audio_buffer.blocked_readers > 0 && loops > 0 ) {
            usleep(1);
            loops--;
        }
        if( audio_buffer.blocked_readers > 0 )
            MSG_V("uninit_audio_buffer: %d blocked readers did not wake up\n",
                  audio_buffer.blocked_readers);
    }

    audio_buffer.index_len=0;
    audio_buffer.index_head=0;
    audio_buffer.index_tail=0;
    audio_buffer.head = 0;
    audio_buffer.tail = 0;
    audio_buffer.len = 0;
    audio_buffer.size = 0;
    audio_buffer.min_reserv = 0;
    audio_buffer.min_len = 0;
    audio_buffer.HasReset = 0;
    audio_buffer.blocked_readers = 0;

    pthread_mutex_lock( &audio_buffer.head_mutex );
    pthread_mutex_unlock( &audio_buffer.head_mutex );
    pthread_mutex_destroy( &audio_buffer.head_mutex );

    pthread_mutex_lock( &audio_buffer.tail_mutex );
    pthread_mutex_unlock( &audio_buffer.tail_mutex );
    pthread_mutex_destroy( &audio_buffer.tail_mutex );

    pthread_cond_destroy( &audio_buffer.wait_buffer_cond );

    if( audio_buffer.buffer )
        free( audio_buffer.buffer );
    audio_buffer.buffer = NULL;

    if( audio_buffer.indices )
        free( audio_buffer.indices );
    audio_buffer.indices = NULL;
    /* audio_buffer.sh_audio = ?; */
}

int read_audio_buffer( sh_audio_t *audio, unsigned char *buffer, int minlen, int maxlen, float *pts ) 
{
    int len = 0, l = 0;
    int next_idx;
    int head_idx;
    int head_pos;
    int head;
    UNUSED(audio);
    pthread_mutex_lock( &audio_buffer.tail_mutex );

    while( len < minlen ) {
        if( audio_buffer.tail == audio_buffer.head ) {
            if( audio_buffer.eof ) {
                break;
            }
            audio_buffer.blocked_readers++;
            dec_ahead_can_aseek=1; /* Safe to seek while we wait for data */
            pthread_cond_wait(&audio_buffer.wait_buffer_cond, &audio_buffer.tail_mutex );
            dec_ahead_can_aseek=0;
            audio_buffer.blocked_readers--;
            if( audio_buffer.HasReset ) {
                audio_buffer.HasReset = 0;
                len = 0;
                l =0;
            }
            continue;
        }

        l = min( maxlen - len, audio_buffer.head - audio_buffer.tail );
        if(l<0) {
            l = min( maxlen - len, audio_buffer.len - audio_buffer.tail );
            if( l == 0 ) {
                if( audio_buffer.head != audio_buffer.tail )
                    audio_buffer.tail = 0;
                continue;
            }
        }

        memcpy( &buffer[len], &audio_buffer.buffer[audio_buffer.tail], l );
        len += l;
        audio_buffer.tail += l;
        if( audio_buffer.tail >= audio_buffer.len && audio_buffer.tail != audio_buffer.head )
            audio_buffer.tail = 0; 
    }

    if( len > 0 ) { /* get pts to return and calculate next pts */
        next_idx = (audio_buffer.index_tail+1)%audio_buffer.index_len;
        head_idx = audio_buffer.index_head;
        head_pos = audio_buffer.indices[(head_idx-1+audio_buffer.index_len)%audio_buffer.index_len].index;
        head = audio_buffer.head;
        if( next_idx != head_idx && audio_buffer.indices[next_idx].index == audio_buffer.indices[audio_buffer.index_tail].index ) {
            audio_buffer.index_tail = next_idx; /* Buffer was empty */
            next_idx = (audio_buffer.index_tail+1)%audio_buffer.index_len;
        }
        *pts = audio_buffer.indices[audio_buffer.index_tail].pts;
        
        MSG_DBG3("audio_ahead: len %i, tail %i pts %.3f  tail_idx %3i  head_idx %3i  head_pos %3i\n", len,audio_buffer.tail,*pts, audio_buffer.index_tail, head_idx, head_pos );
        while( next_idx != head_idx &&
               ((audio_buffer.tail <= head &&
                 (audio_buffer.indices[next_idx].index <= audio_buffer.tail || 
                  head_pos < audio_buffer.indices[next_idx].index)) ||
                (head < audio_buffer.indices[next_idx].index &&
                 audio_buffer.indices[next_idx].index <= audio_buffer.tail))) {
            MSG_DBG3("audio_ahead: next_idx %3i index %3i \n", next_idx, audio_buffer.indices[next_idx].index);
            next_idx=(next_idx+1)%audio_buffer.index_len;
        }
        audio_buffer.index_tail = (next_idx-1+audio_buffer.index_len)%audio_buffer.index_len;
        if( audio_buffer.indices[audio_buffer.index_tail].index != audio_buffer.tail ) {
            int buff_len = audio_buffer.len;
            MSG_DBG3("audio_ahead: orig idx %3i pts %.3f  pos %i   \n",audio_buffer.index_tail, audio_buffer.indices[audio_buffer.index_tail].pts,audio_buffer.indices[audio_buffer.index_tail].index );
            audio_buffer.indices[audio_buffer.index_tail].pts += (float)((audio_buffer.tail - audio_buffer.indices[audio_buffer.index_tail].index + buff_len) % buff_len) / (float)audio_buffer.sh_audio->f_bps;
            audio_buffer.indices[audio_buffer.index_tail].index = audio_buffer.tail;
            MSG_DBG3("audio_ahead: read next_idx %3i next_pts %.3f  pos %i \n", audio_buffer.index_tail,audio_buffer.indices[audio_buffer.index_tail].pts,audio_buffer.indices[audio_buffer.index_tail].index );
        }
    }

    pthread_mutex_unlock( &audio_buffer.tail_mutex );

    return len;
}

float get_delay_audio_buffer(void)
{
    int delay = audio_buffer.head - audio_buffer.tail;
    if( delay < 0 )
        delay += audio_buffer.len;
    return (float)delay / (float)audio_buffer.sh_audio->f_bps;
}

int decode_audio_buffer(int len)
{
    int ret, blen, l, l2;
    int next_idx;
    unsigned int t;

    pthread_mutex_lock( &audio_buffer.head_mutex );

    t = GetTimer();
    if (len < audio_buffer.sh_audio->audio_out_minsize)
        len = audio_buffer.sh_audio->audio_out_minsize;

    if( audio_buffer.size - audio_buffer.head <= audio_buffer.min_reserv ) {
        if( audio_buffer.tail == 0 ) {
            pthread_mutex_unlock( &audio_buffer.head_mutex );
            return 0;
        }
        audio_buffer.len = audio_buffer.head;
        audio_buffer.head = 0;
        len = min( len, audio_buffer.tail - audio_buffer.head - audio_buffer.min_reserv);
        if( len < audio_buffer.sh_audio->audio_out_minsize ) {
            pthread_mutex_unlock( &audio_buffer.head_mutex );
            return 0;
        }
    }

    blen = audio_buffer.size - audio_buffer.head;
    if( (l = (blen - audio_buffer.min_reserv)) < len ) {
        len = max(l,audio_buffer.sh_audio->audio_out_minsize);
    }

    if( (l = (audio_buffer.tail - audio_buffer.head)) > 0 ) {
        blen = l;
        l -= audio_buffer.min_reserv;
        if( l < len ) {
            len = l;
            if( len < audio_buffer.sh_audio->audio_out_minsize ) {
                pthread_mutex_unlock( &audio_buffer.head_mutex );
                return 0;
            }
        }
    }
    MSG_DBG3("decode audio %d   h %d, t %d, l %d \n", len, audio_buffer.head, audio_buffer.tail,  audio_buffer.len);
        
    for( l = 0, l2 = len, ret = 0; l < len && l2 >= audio_buffer.sh_audio->audio_out_minsize; ) {
	float pts;
        ret = decode_audio( audio_buffer.sh_audio, &audio_buffer.buffer[audio_buffer.head], audio_buffer.min_len, l2,blen,&pts);
        if( ret <= 0 )
            break;

        next_idx = (audio_buffer.index_head+1)%audio_buffer.index_len;
        if( next_idx != audio_buffer.index_tail ) {
            MSG_DBG3("decode audio idx %3i tail %3i next pts %.3f  %i\n",audio_buffer.index_head, audio_buffer.index_tail, pts, audio_buffer.head );
            audio_buffer.indices[audio_buffer.index_head].pts = pts;
            audio_buffer.indices[audio_buffer.index_head].index = audio_buffer.head;
            audio_buffer.index_head = next_idx;
        }
        audio_buffer.head+=ret;
        MSG_DBG3("new head %6d  \n", audio_buffer.head);
        l += ret;
        l2 -= ret;
        blen -= ret;
    }
    MSG_DBG2("decoded audio %d   diff %d\n", l, l - len);
    
    if( ret <= 0 && d_audio->eof) {
        MSG_V("xp_audio_eof\n");
        audio_buffer.eof=1;
        pthread_mutex_unlock( &audio_buffer.head_mutex );
        pthread_mutex_lock( &audio_buffer.tail_mutex );
        pthread_cond_broadcast( &audio_buffer.wait_buffer_cond );
        pthread_mutex_unlock( &audio_buffer.tail_mutex );
        return 0;
    }

    if( audio_buffer.head > audio_buffer.len )
        audio_buffer.len=audio_buffer.head;
    if( audio_buffer.head >= audio_buffer.size && audio_buffer.tail > 0 )
        audio_buffer.head = 0;

    pthread_cond_signal( &audio_buffer.wait_buffer_cond );    

    t=GetTimer()-t;
    audio_decode_time_usage+=t*0.000001f;
    audio_decode_time_usage-=audio_decode_time_usage_correction;
    if(benchmark)
    {
	if(t > max_audio_decode_time_usage) max_audio_decode_time_usage = t;
	if(t < min_audio_decode_time_usage) min_audio_decode_time_usage = t;
    }

    pthread_mutex_unlock( &audio_buffer.head_mutex );


    blen = audio_buffer.head - audio_buffer.tail;
    if( blen < 0 )
        blen += audio_buffer.len;
    if( blen < MAX_OUTBURST ) {
        return 2;
    }
    return 1;
}

void reset_audio_buffer(void)
{
    pthread_mutex_lock( &audio_buffer.head_mutex );
    pthread_mutex_lock( &audio_buffer.tail_mutex );

    audio_buffer.tail = audio_buffer.head;
    audio_buffer.len = audio_buffer.size;
    audio_buffer.eof = 0;
    audio_buffer.HasReset = 1;
    audio_buffer.index_tail = audio_buffer.index_head;

    pthread_mutex_unlock( &audio_buffer.tail_mutex );
    pthread_mutex_unlock( &audio_buffer.head_mutex );
}

int get_len_audio_buffer(void)
{
    int len = audio_buffer.head - audio_buffer.tail;
    if( len < 0 )
        len += audio_buffer.len;
    return len;
}

int get_free_audio_buffer(void)
{
    int len;

    if( audio_buffer.eof )
        return -1;

    if( audio_buffer.size - audio_buffer.head < audio_buffer.min_reserv &&
        audio_buffer.tail > 0 ) {
        pthread_mutex_lock( &audio_buffer.head_mutex );
        audio_buffer.len = audio_buffer.head;
        audio_buffer.head = 0;
        pthread_mutex_unlock( &audio_buffer.head_mutex );
    }

    len = audio_buffer.tail - audio_buffer.head;
    if( len <= 0 )
        len += audio_buffer.size;
    len -= audio_buffer.min_reserv;

    if( len <= 0 )
        return 0;

    return len;
}


void uninit_player(unsigned int mask){
  fflush(stdout);
  fflush(stderr);
  mask=inited_flags&mask;

  if(enable_xp!=XP_None) 
  {
    pinfo[xp_id].current_module="uninit_xp";
    uninit_dec_ahead(0);
  }

  if (mask&INITED_SPUDEC){
    inited_flags&=~INITED_SPUDEC;
    pinfo[xp_id].current_module="uninit_spudec";
    spudec_free(vo_spudec);
    vo_spudec=NULL;
  }

  if (mask&INITED_VOBSUB){
    inited_flags&=~INITED_VOBSUB;
    pinfo[xp_id].current_module="uninit_vobsub";
    vobsub_close(vo_vobsub);
    vo_vobsub=NULL;
  }

  if(mask&INITED_VCODEC){
    inited_flags&=~INITED_VCODEC;
    pinfo[xp_id].current_module="uninit_vcodec";
    uninit_video(sh_video);
    sh_video=NULL;
  }

  if(mask&INITED_VO){
    inited_flags&=~INITED_VO;
    pinfo[xp_id].current_module="uninit_vo";
    vo_uninit();
  }

  if(mask&INITED_ACODEC){
    inited_flags&=~INITED_ACODEC;
    pinfo[xp_id].current_module="uninit_acodec";
    uninit_audio(sh_audio);
    sh_audio=NULL;
  }

  if(mask&INITED_AO){
    inited_flags&=~INITED_AO;
    pinfo[xp_id].current_module="uninit_ao";
    ao_uninit();
  }

  if(mask&INITED_GETCH2){
    inited_flags&=~INITED_GETCH2;
    pinfo[xp_id].current_module="uninit_getch2";
  // restore terminal:
    getch2_disable();
  }

  if(mask&INITED_DEMUXER){
    inited_flags&=~INITED_DEMUXER;
    pinfo[xp_id].current_module="free_demuxer";
    FREE_DEMUXER(demuxer);
  }

  if(mask&INITED_STREAM){
    inited_flags&=~INITED_STREAM;
    pinfo[xp_id].current_module="uninit_stream";
    if(stream) free_stream(stream);
    stream=NULL;
  }

  if(mask&INITED_INPUT){
    inited_flags&=~INITED_INPUT;
    pinfo[xp_id].current_module="uninit_input";
    mp_input_uninit();
  }

#ifdef USE_SUB
  if(mask&INITED_SUBTITLE){
    inited_flags&=~INITED_SUBTITLE;
    pinfo[xp_id].current_module="sub_free";
    mp_input_uninit();
    sub_free( subtitles );
    sub_name=NULL;
    vo_sub=NULL;
    subtitles=NULL;
  }
#endif

  pinfo[xp_id].current_module=NULL;

}

void exit_player(char* how){

  fflush(stdout);
  fflush(stderr);
  uninit_player(INITED_ALL);

  pinfo[xp_id].current_module="exit_player";

  sws_uninit();
  if(how) MSG_HINT(MSGTR_Exiting,how);
  MSG_DBG2("max framesize was %d bytes\n",max_framesize);
  mp_msg_uninit();
  if(how) exit(0);
  return; /* Still try coredump!!!*/
}


static void soft_exit_player(void)
{
  fflush(stdout);
  fflush(stderr);
  uninit_player(INITED_DEMUXER|INITED_STREAM);
  if(sh_audio) while(get_len_audio_buffer()) usleep(0);
  if(sh_video && shva)
  {
    volatile int dec_ahead_active_frame;
    for(;;)
    {
	LOCK_VDEC_ACTIVE();
	vo_get_active_frame(&dec_ahead_active_frame);
	UNLOCK_VDEC_ACTIVE();
	LOCK_VDECA();
	if(shva[dec_ahead_active_frame].eof) break;
	usleep(0);
    }
  }
  uninit_player((INITED_ALL)&(~(INITED_STREAM|INITED_DEMUXER)));

  pinfo[xp_id].current_module="exit_player";
  MSG_HINT(MSGTR_Exiting,MSGTR_Exit_quit);
  MSG_DBG2("max framesize was %d bytes\n",max_framesize);
  mp_msg_uninit();
  exit(0);
}

void killall_threads(pthread_t pth_id)
{
  if(enable_xp > XP_None)
  {
	unsigned i;
	for(i=0;i < MAX_XPTHREADS;i++)
	{
	    if(pth_id && pinfo[i].pth_id && pinfo[i].pth_id != mplayer_pth_id)
	    {
		pthread_kill(pinfo[i].pth_id,SIGKILL);
		if(pinfo[i].unlink) pinfo[i].unlink(pth_id);
	    }
	}
	enable_xp=XP_None;
  }
}

void __exit_sighandler(void)
{
  static int sig_count=0;
  ++sig_count;
//  return;
  if(sig_count==2) return;
  if(sig_count>2){
    // can't stop :(
    kill(getpid(),SIGKILL);
    return;
  }
  exit_player(NULL);
}


void exit_sighandler(void)
{
  killall_threads(pthread_self());
  __exit_sighandler();
}

extern void mp_register_options(m_config_t* cfg);

#include "mixer.h"
#include "cfg-mplayer.h"

void parse_cfgfiles( m_config_t* conf )
{
char *conffile;
int conffile_fd;
if (m_config_parse_config_file(conf, "/etc/mplayer.conf") < 0)
  exit(1);
if ((conffile = get_path("")) == NULL) {
  MSG_WARN(MSGTR_NoHomeDir);
} else {
  mkdir(conffile, 0777);
  free(conffile);
  if ((conffile = get_path("config")) == NULL) {
    MSG_ERR(MSGTR_GetpathProblem);
    conffile=(char*)malloc(strlen("config")+1);
    if(conffile)
      strcpy(conffile,"config");
  }
  {
    if ((conffile_fd = open(conffile, O_CREAT | O_EXCL | O_WRONLY, 0666)) != -1) {
      MSG_INFO(MSGTR_CreatingCfgFile, conffile);
      write(conffile_fd, default_config, strlen(default_config));
      close(conffile_fd);
    }
    if (m_config_parse_config_file(conf, conffile) < 0)
      exit(1);
    free(conffile);
  }
}
}

// When libmpdemux perform a blocking operation (network connection or cache filling)
// if the operation fail we use this function to check if it was interrupted by the user.
// The function return a new value for eof.
static int libmpdemux_was_interrupted(int eof)
{
  mp_cmd_t* cmd;
  if((cmd = mp_input_get_cmd(0,0,0)) != NULL) {
       switch(cmd->id) {
       case MP_CMD_QUIT:
       case MP_CMD_SOFT_QUIT: // should never happen
	 exit_player(MSGTR_Exit_quit);
       case MP_CMD_PLAY_TREE_STEP: {
	 eof = (cmd->args[0].v.i > 0) ? PT_NEXT_ENTRY : PT_PREV_ENTRY;
       } break;
       case MP_CMD_PLAY_TREE_UP_STEP: {
	 eof = (cmd->args[0].v.i > 0) ? PT_UP_NEXT : PT_UP_PREV;
       } break;	  
       case MP_CMD_PLAY_ALT_SRC_STEP: {
	 eof = (cmd->args[0].v.i > 0) ?  PT_NEXT_SRC : PT_PREV_SRC;
       } break;
       }
       mp_cmd_free(cmd);
  }
  return eof;
}

unsigned xp_num_cpu;
static unsigned get_number_cpu(void) {
#ifdef _OPENMP
    return omp_get_num_procs();
#else
    /* TODO ? */
    return 1;
#endif
}


#if defined( ARCH_X86 ) || defined(ARCH_X86_64)
int x86_mmx=-1;
int x86_mmx2=-1;
int x86_3dnow=-1;
int x86_3dnow2=-1;
int x86_sse=-1;
int x86_sse2=-1;
static void get_mmx_optimizations( void )
{
  GetCpuCaps(&gCpuCaps);
  if(x86_mmx != -1) gCpuCaps.hasMMX=x86_mmx;
  if(x86_mmx2 != -1) gCpuCaps.hasMMX2=x86_mmx2;
  if(x86_3dnow != -1) gCpuCaps.has3DNow=x86_3dnow;
  if(x86_3dnow2 != -1) gCpuCaps.has3DNowExt=x86_3dnow2;
  if(x86_sse != -1) gCpuCaps.hasSSE=x86_sse;
  if(x86_sse2 != -1) gCpuCaps.hasSSE2=x86_sse2;
  MSG_V("User corrected CPU flags: MMX=%d MMX2=%d 3DNow=%d 3DNow2=%d SSE=%d SSE2=%d SSE3=%d SSSE3=%d SSE41=%d SSE42=%d AES=%d AVX=%d FMA=%d\n",
	gCpuCaps.hasMMX,
	gCpuCaps.hasMMX2,
	gCpuCaps.has3DNow,
	gCpuCaps.has3DNowExt,
	gCpuCaps.hasSSE,
	gCpuCaps.hasSSE2,
	gCpuCaps.hasSSE3,
	gCpuCaps.hasSSSE3,
	gCpuCaps.hasSSE41,
	gCpuCaps.hasSSE42,
	gCpuCaps.hasAES,
	gCpuCaps.hasAVX,
	gCpuCaps.hasFMA);
  if(gCpuCaps.hasMMX) 		mplayer_accel |= MM_ACCEL_X86_MMX;
  if(gCpuCaps.hasMMX2) 		mplayer_accel |= MM_ACCEL_X86_MMXEXT;
  if(gCpuCaps.hasSSE) 		mplayer_accel |= MM_ACCEL_X86_SSE;
  if(gCpuCaps.has3DNow) 	mplayer_accel |= MM_ACCEL_X86_3DNOW;
  if(gCpuCaps.has3DNowExt) 	mplayer_accel |= MM_ACCEL_X86_3DNOWEXT;
  MSG_V("mplayer_accel=%i\n",mplayer_accel);
}
#endif


static void init_player( void )
{
    if(video_driver && strcmp(video_driver,"help")==0)
    {
	vo_print_help();
	exit(0);
    }
    if(audio_driver && strcmp(audio_driver,"help")==0)
    {
	ao_print_help();
	exit(0);
    }
    if(video_family && strcmp(video_family,"help")==0){
      vfm_help();
      exit(0);
    }
    if(audio_family && strcmp(audio_family,"help")==0){
      afm_help();
      exit(0);
    }
    if(vf_cfg.list && strcmp(vf_cfg.list,"help")==0){
      vf_help();
      MSG_INFO("\n");
      exit(0);
    }
    if(af_cfg.list && strcmp(af_cfg.list,"help")==0){
      af_help();
      MSG_INFO("\n");
      exit(0);
    }

    /* check codec.conf*/
    if(!parse_codec_cfg(get_path("codecs.conf"))){
      if(!parse_codec_cfg(CONFDIR"/codecs.conf")){
        MSG_HINT(MSGTR_CopyCodecsConf);
        exit(0);
      }
    }

    if(audio_codec && strcmp(audio_codec,"help")==0){
      list_codecs(1);
      exit(0);
    }
    if(video_codec && strcmp(video_codec,"help")==0){
      list_codecs(0);
      exit(0);
    }
}

void show_help(void) {
    // no file/vcd/dvd -> show HELP:
    MSG_INFO("%s",help_text);
    print_stream_drivers();
    MSG_INFO("\nUse --long-help option for full help\n");
}

void show_long_help(void) {
    show_help();
    m_config_show_options(mplayer_opts);
    vo_print_help();
    ao_print_help();
    vf_help();
    af_help();
    vfm_help();
    afm_help();
    /* check codec.conf*/
    if(!parse_codec_cfg(get_path("codecs.conf"))){
      if(!parse_codec_cfg(CONFDIR"/codecs.conf")){
        MSG_HINT(MSGTR_CopyCodecsConf);
        exit(0);
      }
    }
    list_codecs(0);
    list_codecs(1);
}

int decore_audio( int xp_id )
{
  int eof = 0;
/*========================== PLAY AUDIO ============================*/
while(sh_audio){
  unsigned int t;
  double tt;
  int playsize;
  float pts=HUGE;
  int ret=0;

  ao_data.pts=sh_audio->timer*90000.0;
  playsize=ao_get_space();

  if(!playsize) {
    if(sh_video)
      break; // buffer is full, do not block here!!!
    usec_sleep(10000); // Wait a tick before retry
    continue;
  }

  if(playsize>MAX_OUTBURST) playsize=MAX_OUTBURST; // we shouldn't exceed it!
  //if(playsize>outburst) playsize=outburst;

  // Update buffer if needed
  pinfo[xp_id].current_module="decode_audio";   // Enter AUDIO decoder module
  t=GetTimer();
  while(sh_audio->a_buffer_len<playsize && !audio_eof){
      if(enable_xp>=XP_VideoAudio) {
          ret=read_audio_buffer(sh_audio,&sh_audio->a_buffer[sh_audio->a_buffer_len],
                              playsize-sh_audio->a_buffer_len,sh_audio->a_buffer_size-sh_audio->a_buffer_len,&pts);
      } else
      {
          ret=decode_audio(sh_audio,&sh_audio->a_buffer[sh_audio->a_buffer_len],
                           playsize-sh_audio->a_buffer_len,sh_audio->a_buffer_size-sh_audio->a_buffer_len,sh_audio->a_buffer_size-sh_audio->a_buffer_len,&pts);
      }
    if(ret>0) sh_audio->a_buffer_len+=ret; 
    else {
      if(!d_audio->eof)
	break;
      if(!sh_video)
	eof = PT_NEXT_ENTRY;
      else
      {
        MSG_V("audio_stream_eof\n");
	inited_flags&=~INITED_AO;
	pinfo[xp_id].current_module="uninit_ao";
	ao_uninit();
      }
      audio_eof=1;
      break;
    }
  }
  pinfo[xp_id].current_module="play_audio";   // Leave AUDIO decoder module
  t=GetTimer()-t;
  tt = t*0.000001f;
  audio_time_usage+=tt;
  if(benchmark)
  {
    if(tt > max_audio_time_usage) max_audio_time_usage = tt;
    if(tt < min_audio_time_usage) min_audio_time_usage = tt;
    cur_audio_time_usage=tt;
  }
  if(playsize>sh_audio->a_buffer_len) playsize=sh_audio->a_buffer_len;

  if(enable_xp>=XP_VAPlay) dec_ahead_audio_delay=ao_get_delay();

  playsize=ao_play(sh_audio->a_buffer,playsize,0);

  if(playsize>0){
      sh_audio->a_buffer_len-=playsize;
      memcpy(sh_audio->a_buffer,&sh_audio->a_buffer[playsize],sh_audio->a_buffer_len);
      if(!av_sync_pts && enable_xp>=XP_VAPlay)
          pthread_mutex_lock(&audio_timer_mutex);
      if(use_pts_fix2) {
	  if(sh_audio->a_pts != HUGE) {
	      sh_audio->a_pts_pos-=playsize;
	      if(sh_audio->a_pts_pos > -ao_get_delay()*sh_audio->f_bps) {
		  sh_audio->timer+=playsize/(float)(sh_audio->f_bps);
	      } else {
		  sh_audio->timer=sh_audio->a_pts-(float)sh_audio->a_pts_pos/(float)sh_audio->f_bps;
		  MSG_V("Audio chapter change detected\n");
		  sh_audio->chapter_change=1;
		  sh_audio->a_pts = HUGE;
	      }
	  } else if(pts != HUGE) {
	      if(pts < 1.0 && sh_audio->timer > 2.0) {
		  sh_audio->timer+=playsize/(float)(sh_audio->f_bps);
		  sh_audio->a_pts=pts;
		  sh_audio->a_pts_pos=sh_audio->a_buffer_len-ret;
	      } else {
		  sh_audio->timer=pts+(ret-sh_audio->a_buffer_len)/(float)(sh_audio->f_bps);
		  sh_audio->a_pts=HUGE;
	      }
	  } else
	      sh_audio->timer+=playsize/(float)(sh_audio->f_bps);
      } else if(av_sync_pts && pts!=HUGE)
	  sh_audio->timer=pts+(ret-sh_audio->a_buffer_len)/(float)(sh_audio->f_bps);
      else
	  sh_audio->timer+=playsize/(float)(sh_audio->f_bps);
      if(!av_sync_pts && enable_xp>=XP_VAPlay)
          pthread_mutex_unlock(&audio_timer_mutex);
  }

  break;
 } // if(sh_audio)
 return eof;
}

typedef struct
{
    int drop_frame_cnt;
    int too_slow_frame_cnt;
    int too_fast_frame_cnt;
}video_stat_t;

#ifdef USE_OSD

void update_osd( float v_pts )
{
static char osd_text_buffer[64];
static int osd_last_pts=-303;
//================= Update OSD ====================
  if(osd_level>=2){
      int pts=(osd_level==3&&demuxer->movi_length!=UINT_MAX)?demuxer->movi_length-v_pts:v_pts;
      int addon=(osd_level==3&&demuxer->movi_length!=UINT_MAX)?-1:1;
      char osd_text_tmp[64];
      if(pts==osd_last_pts-addon) 
      {
        if(osd_level==3&&demuxer->movi_length!=UINT_MAX) ++pts;
	else --pts;
      }
      else osd_last_pts=pts;
      vo_osd_text=osd_text_buffer;
      if (osd_show_sub_delay) {
	  sprintf(osd_text_tmp, "Sub delay: %d ms",(int)(sub_delay*1000));
	  osd_show_sub_delay--;
      } else
      if (osd_show_framedrop) {
	  sprintf(osd_text_tmp, "Framedrop: %s",frame_dropping>1?"hard":frame_dropping?"vo":"none");
	  osd_show_framedrop--;
      } else
      if (osd_show_av_delay) {
	  sprintf(osd_text_tmp, "A-V delay: %d ms",(int)(audio_delay*1000));
	  osd_show_av_delay--;
      } else
#ifdef ENABLE_DEC_AHEAD_DEBUG
          if(verbose) sprintf(osd_text_tmp,"%c %02d:%02d:%02d abs frame: %u",osd_function,pts/3600,(pts/60)%60,pts%60,abs_dec_ahead_active_frame);
	  else sprintf(osd_text_tmp,"%c %02d:%02d:%02d",osd_function,pts/3600,(pts/60)%60,pts%60);
#else
          sprintf(osd_text_tmp,"%c %02d:%02d:%02d",osd_function,pts/3600,(pts/60)%60,pts%60);
#endif
      if(strcmp(vo_osd_text, osd_text_tmp)) {
	      strcpy(vo_osd_text, osd_text_tmp);
	      vo_osd_changed(OSDTYPE_OSD);
      }
  } else {
      if(vo_osd_text) {
      vo_osd_text=NULL;
	  vo_osd_changed(OSDTYPE_OSD);
      }
  }
}
#endif

//================= Update OSD ====================
void update_subtitle(float v_pts)
{
  
#ifdef USE_SUB
  // find sub
  if(subtitles && v_pts>0){
      float pts=v_pts;
      if(sub_fps==0) sub_fps=sh_video->fps;
      pinfo[xp_id].current_module="find_sub";
      if (pts > sub_last_pts || pts < sub_last_pts-1.0 ) {
         find_sub(subtitles,sub_uses_time?(100*(pts+sub_delay)):((pts+sub_delay)*sub_fps)); // FIXME! frame counter...
         sub_last_pts = pts;
      }
      pinfo[xp_id].current_module=NULL;
  }
#endif
  
  // DVD sub:
  if(vo_flags & 0x08){
    static vo_mpegpes_t packet;
    static vo_mpegpes_t *pkg=&packet;
    packet.timestamp=sh_video->timer*90000.0;
    packet.id=0x20; /* Subpic */
    while((packet.size=ds_get_packet_sub(d_dvdsub,&packet.data))>0){
      MSG_V("\rDVD sub: len=%d  v_pts=%5.3f  s_pts=%5.3f  \n",packet.size,v_pts,d_dvdsub->pts);
      vo_draw_frame(&pkg);
    }
  }else if(vo_spudec){
    unsigned char* packet=NULL;
    int len,timestamp;
    pinfo[xp_id].current_module="spudec";
    spudec_heartbeat(vo_spudec,90000*v_pts);
    if (vo_vobsub) {
      if (v_pts+sub_delay >= 0) {
	while((len=vobsub_get_packet(vo_vobsub, v_pts+sub_delay,(void**)&packet, &timestamp))>0){
		timestamp -= (v_pts + sub_delay - sh_video->timer)*90000;
		MSG_V("\rVOB sub: len=%d v_pts=%5.3f v_timer=%5.3f sub=%5.3f ts=%d \n",len,v_pts,sh_video->timer,timestamp / 90000.0,timestamp);
		spudec_assemble(vo_spudec,packet,len,90000*d_dvdsub->pts);

	}
      }
    } else {
	while((len=ds_get_packet_sub(d_dvdsub,&packet))>0){
		MSG_V("\rDVD sub: len=%d  v_pts=%5.3f  s_pts=%5.3f  \n",len,v_pts,d_dvdsub->pts);
		spudec_assemble(vo_spudec,packet,len,90000*d_dvdsub->pts);
	}
    }
    /* detect wether the sub has changed or not */
    if(spudec_changed(vo_spudec))
      vo_osd_changed(OSDTYPE_SPU);
    pinfo[xp_id].current_module=NULL;
  }
}

int mp09_decore_video( int rtc_fd, video_stat_t *vstat, float *aq_sleep_time, float *v_pts )
{
/*========================== PLAY VIDEO ============================*/

  static float next_frame_time=0;
  static int frame_time_remaining=0; // flag
  static int dropped_frames=0; // how many frames dropped since last non-dropped frame
  static int drop_frame=0;     // current dropping status
  static int total_frame_cnt=0;
  static unsigned int lastframeout_ts=0;
  float frame_time=next_frame_time;
  float time_frame=0;
  float AV_delay=0; // average of A-V timestamp differences
  float time_frame_corr_avg=0;
  int blit_frame=0;
  int delay_corrected=1;
  vo_pts=sh_video->timer*90000.0;
  vo_fps=sh_video->fps;

  if(!frame_time_remaining){
    //--------------------  Decode a frame: -----------------------
    while(1)
    {   unsigned char* start=NULL;
	int in_size;
	float v_pts;
	// get it!
	pinfo[xp_id].current_module="video_read_frame";
        in_size=video_read_frame(sh_video,&next_frame_time,&v_pts,&start,force_fps);
	if(in_size<0) return 1;
	if(in_size>max_framesize) max_framesize=in_size; // stats
	sh_video->timer+=frame_time;
	time_frame+=frame_time;  // for nosound
	// check for frame-drop:
	pinfo[xp_id].current_module="check_framedrop";
	if(sh_audio && !d_audio->eof){
	    float delay=ao_get_delay();
	    float d=(sh_video->timer)-(sh_audio->timer-delay);
	    // we should avoid dropping to many frames in sequence unless we
	    // are too late. and we allow 100ms A-V delay here:
	    if(d<-dropped_frames*frame_time-0.100){
		drop_frame=frame_dropping;
		++vstat->drop_frame_cnt;
		++dropped_frames;
	    } else {
		drop_frame=dropped_frames=0;
	    }
	    ++total_frame_cnt;
	}
	// decode:
	pinfo[xp_id].current_module="decode_video";
	blit_frame=decode_video(sh_video,start,in_size,drop_frame,v_pts);
	break;
    }
    //------------------------ frame decoded. --------------------

    MSG_DBG2("*** ftime=%5.3f ***\n",frame_time);

  }

// ==========================================================================
    
    pinfo[xp_id].current_module="draw_osd";
    vo_draw_osd();

    pinfo[xp_id].current_module="calc_sleep_time";

#if 0
{	// debug frame dropping code
	  float delay=ao_get_delay();
	  MSG_V("\r[V] %5.3f [A] %5.3f => {%5.3f}  (%5.3f) [%d]   \n",
	      sh_video->timer,sh_audio->timer-delay,
	      sh_video->timer-(sh_audio->timer-delay),
	      delay,drop_frame);
}
#endif

    if(drop_frame && !frame_time_remaining){

      time_frame=0;	// don't sleep!
      blit_frame=0;	// don't display!
      
    } else {

      // It's time to sleep...
      
      frame_time_remaining=0;
      time_frame-=GetRelativeTime(); // reset timer

      if(sh_audio && !d_audio->eof){
	  float delay=ao_get_delay();
	  MSG_DBG2("delay=%f\n",delay);

	if(!dapsync){

	      /* Arpi's AV-sync */

          time_frame=sh_video->timer;
          time_frame-=sh_audio->timer-delay;

	} else {  // if(!dapsync)

	      /* DaP's AV-sync */

    	      float SH_AV_delay;
	      /* SH_AV_delay = sh_video->timer - (sh_audio->timer - (float)((float)delay + sh_audio->a_buffer_len) / (float)sh_audio->f_bps); */
	      SH_AV_delay = sh_video->timer - (sh_audio->timer - (float)((float)delay) / (float)sh_audio->f_bps);
	      if(SH_AV_delay<-2*frame_time){
		  static int drop_message=0;
	          drop_frame=frame_dropping; // tricky!
	          ++vstat->drop_frame_cnt;
		  if(vstat->drop_frame_cnt>50 && AV_delay>0.5 && !drop_message){
		      drop_message=1;
		      if(mpxp_after_seek) mpxp_after_seek--;
		      else {
			MSG_WARN(MSGTR_SystemTooSlow);
		      }
	         }
		MSG_INFO("A-V SYNC: FRAMEDROP (SH_AV_delay=%.3f)!\n", SH_AV_delay);
	        MSG_DBG2("\nframe drop %d, %.2f\n", drop_frame, time_frame);
	        /* go into unlimited-TF cycle */
    		time_frame = SH_AV_delay;
	      } else {
#define	SL_CORR_AVG_LEN	125
	        /* don't adjust under framedropping */
	        time_frame_corr_avg = (time_frame_corr_avg * (SL_CORR_AVG_LEN - 1) +
	    				(SH_AV_delay - time_frame)) / SL_CORR_AVG_LEN;
#define	UNEXP_CORR_MAX	0.1	/* limit of unexpected correction between two frames (percentage) */
#define	UNEXP_CORR_WARN	1.0	/* warn limit of A-V lag (percentage) */
	        time_frame += time_frame_corr_avg;
	        if (SH_AV_delay - time_frame < (frame_time + time_frame_corr_avg) * UNEXP_CORR_MAX &&
		    SH_AV_delay - time_frame > (frame_time + time_frame_corr_avg) * -UNEXP_CORR_MAX)
		    time_frame = SH_AV_delay;
	        else {
		    if (SH_AV_delay - time_frame > (frame_time + time_frame_corr_avg) * UNEXP_CORR_WARN ||
		        SH_AV_delay - time_frame < (frame_time + time_frame_corr_avg) * -UNEXP_CORR_WARN)
		        MSG_WARN( "WARNING: A-V SYNC LAG TOO LARGE: %.3f {%.3f - %.3f} (too little UNEXP_CORR_MAX?)\n",
		  	    SH_AV_delay - time_frame, SH_AV_delay, time_frame);
		        time_frame += (frame_time + time_frame_corr_avg) * ((SH_AV_delay > time_frame) ?
		    		      UNEXP_CORR_MAX : -UNEXP_CORR_MAX);
	        }
	      }	/* /start dropframe */

	} // if(dapsync)

	if(delay>0.25) delay=0.25; else
	if(delay<0.10) delay=0.10;
	if(time_frame>delay*0.6){
	    // sleep time too big - may cause audio drops (buffer underrun)
	    frame_time_remaining=1;
	    time_frame=delay*0.5;
	}

      } else {

          // NOSOUND:
          if( (time_frame<-3*frame_time || time_frame>3*frame_time) || benchmark)
	      time_frame=0;
	  
      }

      (*aq_sleep_time)+=time_frame;

    }	// !drop_frame
    
//============================== SLEEP: ===================================

// flag 256 means: libvo driver does its timing (dvb card)
if(time_frame>0.001 && !(vo_flags&256)){
    pinfo[xp_id].current_module="sleep_usleep";
    time_frame=SleepTime(rtc_fd,softsleep,time_frame);
}

//if(!frame_time_remaining){	// should we display the frame now?

//====================== FLIP PAGE (VIDEO BLT): =========================

        pinfo[xp_id].current_module="change_frame1";

	vo_check_events(); /* check events AST */
        if(blit_frame && !frame_time_remaining){
	   unsigned int t2=GetTimer();
	   double tt;
	   float j;
#define	FRAME_LAG_WARN	0.2
	   j = ((float)t2 - lastframeout_ts) / 1000000;
	   lastframeout_ts = GetTimer();
	   if (j < frame_time + frame_time * -FRAME_LAG_WARN)
		vstat->too_fast_frame_cnt++;
		/* printf ("PANIC: too fast frame (%.3f)!\n", j); */
	   else if (j > frame_time + frame_time * FRAME_LAG_WARN)
		vstat->too_slow_frame_cnt++;
		/* printf ("PANIC: too slow frame (%.3f)!\n", j); */

	   vo_change_frame();
//        usec_sleep(50000); // test only!
	   t2=GetTimer()-t2;
	   tt = t2*0.000001f;
	   vout_time_usage+=tt;
	   if(benchmark)
	   {
		/* we need compute draw_slice+change_frame here */
		cur_vout_time_usage+=tt;
		if(cur_vout_time_usage > max_vout_time_usage) max_vout_time_usage = cur_vout_time_usage;
		if(cur_vout_time_usage < min_vout_time_usage) min_vout_time_usage = cur_vout_time_usage;
		if((cur_video_time_usage + cur_vout_time_usage + cur_audio_time_usage)*vo_fps > 1)
							bench_dropped_frames ++;
	   }
	}

//====================== A-V TIMESTAMP CORRECTION: =========================

  pinfo[xp_id].current_module="av_sync";

  if(sh_audio){
    float a_pts=0;

    // unplayed bytes in our and soundcard/dma buffer:
    float delay=ao_get_delay()+(float)sh_audio->a_buffer_len/(float)sh_audio->f_bps;

    if(pts_from_bps){
	// PTS = sample_no / samplerate
        unsigned int samples=(sh_audio->audio.dwSampleSize)?
          ((ds_tell(d_audio)-sh_audio->a_in_buffer_len)/sh_audio->audio.dwSampleSize) :
          (d_audio->pack_no); // <- used for VBR audio
	samples+=sh_audio->audio.dwStart; // offset
        a_pts=samples*(float)sh_audio->audio.dwScale/(float)sh_audio->audio.dwRate;
	delay_corrected=1;
    } else {
      // PTS = (last timestamp) + (bytes after last timestamp)/(bytes per sec)
      a_pts=d_audio->pts;
      if(!delay_corrected) if(a_pts) delay_corrected=1;
      a_pts+=(ds_tell_pts(d_audio)-sh_audio->a_in_buffer_len)/(float)sh_audio->i_bps;
    }
    *v_pts=d_video->pts;

      MSG_DBG2("### A:%8.3f (%8.3f)  V:%8.3f  A-V:%7.4f  \n",a_pts,a_pts-audio_delay-delay,*v_pts,(a_pts-delay-audio_delay)-*v_pts);

      if(delay_corrected){
        float x;
	AV_delay=(a_pts-delay-audio_delay)-*v_pts;
        x=AV_delay*0.1f;
        if(x<-max_pts_correction) x=-max_pts_correction; else
        if(x> max_pts_correction) x= max_pts_correction;
        if(default_max_pts_correction>=0)
          max_pts_correction=default_max_pts_correction;
        else
          max_pts_correction=sh_video->frametime*0.10; // +-10% of time
	if(!frame_time_remaining){ sh_audio->timer+=x; c_total+=x;} // correction
        if(benchmark && verbose) MSG_STATUS("A:%6.1f V:%6.1f A-V:%7.3f ct:%7.3f  %3d/%3d  %2d%% %2d%% %4.1f%% %d %d\r",
	  a_pts-audio_delay-delay,*v_pts,AV_delay,c_total,
          (int)sh_video->num_frames,(int)sh_video->num_frames_decoded,
          (sh_video->timer>0.5)?(int)(100.0*video_time_usage/(double)sh_video->timer):0,
          (sh_video->timer>0.5)?(int)(100.0*vout_time_usage/(double)sh_video->timer):0,
          (sh_video->timer>0.5)?(100.0*audio_time_usage/(double)sh_video->timer):0
          ,vstat->drop_frame_cnt
	  ,output_quality
        );
        fflush(stdout);
      }
    }
    /* let it paints audio timer instead of video */
    if(sh_audio) *v_pts = sh_audio->timer-ao_get_delay();
    return 0;
}

int xp_decore_video( int rtc_fd, video_stat_t *vstat, float *aq_sleep_time, float *v_pts )
{
    float time_frame=0;
    float AV_delay=0; /* average of A-V timestamp differences */
    volatile int da_locked_frame;
    volatile unsigned ada_locked_frame;
    volatile unsigned ada_blitted_frame;
    int blit_frame=0;
    int delay_corrected=1;
    int final_frame=0;
    int num_frames_decoded = 0;
    LOCK_VDEC_ACTIVE();
    vo_get_active_frame(&dec_ahead_active_frame);
    UNLOCK_VDEC_ACTIVE();
    LOCK_VDECA();
    final_frame = shva[dec_ahead_active_frame].eof;
    sh_video->num_frames = shva[dec_ahead_active_frame].num_frames;
    sh_video->num_frames_decoded = shva[dec_ahead_active_frame].num_frames_decoded;
    *v_pts = shva[dec_ahead_active_frame].v_pts;
    num_frames_decoded = dec_ahead_num_frames_decoded;
    UNLOCK_VDECA();
    if(xp_eof && final_frame) return 1;
    /*------------------------ frame decoded. --------------------*/
/* blit frame */
    LOCK_VDEC_LOCKED();
    da_locked_frame = dec_ahead_locked_frame;
    ada_locked_frame = abs_dec_ahead_locked_frame;
    ada_blitted_frame = abs_dec_ahead_blitted_frame;
    UNLOCK_VDEC_LOCKED();
    blit_frame=!(
    dec_ahead_active_frame == da_locked_frame || /* like: we are in lseek */
     /* don't flip if frame which is being decoded is equal to frame
	which is being displayed or will be displayed.
	Many card can perform page flipping only during VSYNC
	so we can't flip to locked frame too */
    (dec_ahead_active_frame+1)%xp_num_frames == da_locked_frame ||
    (dec_ahead_active_frame-1+xp_num_frames)%xp_num_frames == da_locked_frame);
    if(xp_eof) blit_frame=1; /* force blitting until end of stream will be reached */
    if(use_pts_fix2 && sh_audio) {
	if(sh_video->chapter_change == -1) { /* First frame after seek */
	    while(*v_pts < 1.0 && sh_audio->timer==0.0 && ao_get_delay()==0.0)
		usleep(0);		 /* Wait for audio to start play */
	    if(sh_audio->timer > 2.0 && *v_pts < 1.0) { 
		MSG_V("Video chapter change detected\n");
		sh_video->chapter_change=1;
	    } else {
		sh_video->chapter_change=0;
	    }
	} else if(*v_pts < 1.0 && sh_video->timer > 2.0) {
	    MSG_V("Video chapter change detected\n");
	    sh_video->chapter_change=1;
	}
	if(sh_video->chapter_change && sh_audio->chapter_change) {
	    MSG_V("Reset chapter change\n");
	    sh_video->chapter_change=0;
	    sh_audio->chapter_change=0;
	}
    }
    if(blit_frame) xp_screen_pts=sh_video->timer=*v_pts-(av_sync_pts?0:initial_audio_pts);
#if 0
MSG_INFO("initial_audio_pts=%f a_eof=%i a_pts=%f sh_audio->timer=%f sh_video->timer=%f v_pts=%f stream_pts=%f duration=%f\n"
,initial_audio_pts
,audio_eof
,sh_audio && !audio_eof?d_audio->pts+(ds_tell_pts_r(d_audio)-sh_audio->a_in_buffer_len)/(float)sh_audio->i_bps:0
,sh_audio && !audio_eof?sh_audio->timer-ao_get_delay():0
,sh_video->timer
,shva[dec_ahead_active_frame].v_pts
,shva[dec_ahead_active_frame].stream_pts
,shva[dec_ahead_active_frame].duration);
#endif
    /* It's time to sleep ;)...*/
    pinfo[xp_id].current_module="sleep";
    GetRelativeTime(); /* reset timer */
    if(sh_audio)
    {
	/* FIXME!!! need the same technique to detect audio_eof as for video_eof!
	   often ao_get_delay() never returns 0 :( */
	if(audio_eof && !get_delay_audio_buffer()) goto nosound_model;
	if((!audio_eof || ao_get_delay()) &&
	(!use_pts_fix2 || (!sh_audio->chapter_change && !sh_video->chapter_change)))
	    time_frame=sh_video->timer-(sh_audio->timer-ao_get_delay());
	else if(use_pts_fix2 && sh_audio->chapter_change)
	    time_frame=0;
        else
	    goto nosound_model;
    }
    else
    {
	nosound_model:
	time_frame=shva[dec_ahead_active_frame].duration;
    }
    (*aq_sleep_time)=0; /* we have other way to control that */
    if(benchmark)
	if(time_frame < 0)
	    if(time_frame < max_av_resync) max_av_resync=time_frame;
    if(!(vo_flags&256)){ /* flag 256 means: libvo driver does its timing (dvb card) */
#define XP_MIN_TIMESLICE 0.010 /* under Linux on x86 min time_slice = 10 ms */
#define XP_MIN_AUDIOBUFF 0.05
#define XP_MAX_TIMESLICE 0.1

        if(sh_audio && (!audio_eof || ao_get_delay()) && time_frame>XP_MAX_TIMESLICE) {
            float t;
            if(benchmark && verbose) {
                float a_pts=0;
                float delay=ao_get_delay();
                float video_pts = *v_pts;
                if(av_sync_pts) {
                    a_pts = sh_audio->timer;
                } else if(pts_from_bps){
                    unsigned int samples=(sh_audio->audio.dwSampleSize)?
                        ((ds_tell(d_audio)-sh_audio->a_in_buffer_len)/sh_audio->audio.dwSampleSize) :
                        (d_audio->pack_no); // <- used for VBR audio
                    samples+=sh_audio->audio.dwStart; // offset
                    a_pts=samples*(float)sh_audio->audio.dwScale/(float)sh_audio->audio.dwRate;
                } else {
                    // PTS = (last timestamp) + (bytes after last timestamp)/(bytes per sec)
                    a_pts=d_audio->pts;
                    a_pts+=(ds_tell_pts_r(d_audio)-sh_audio->a_in_buffer_len)/(float)sh_audio->i_bps;
                }
                if( !av_sync_pts && enable_xp>=XP_VideoAudio )
                    delay += get_delay_audio_buffer();
                AV_delay = a_pts-audio_delay-delay - video_pts;
                MSG_V("A:%6.1f V:%6.1f A-V:%7.3f ct:%7.3f  %3d/%3d  %2d%% %2d%% %4.1f%% %d %d\r",
                           a_pts-audio_delay-delay,video_pts,AV_delay,c_total,
                           (int)sh_video->num_frames,num_frames_decoded,
                           (sh_video->timer>0.5)?(int)(100.0*video_time_usage/(double)sh_video->timer):0,
                           (sh_video->timer>0.5)?(int)(100.0*vout_time_usage/(double)sh_video->timer):0,
                           (sh_video->timer>0.5)?(100.0*(audio_time_usage+audio_decode_time_usage)/(double)sh_video->timer):0
                           ,vstat->drop_frame_cnt
                           ,output_quality
                           );
                fflush(stdout);
            }

            if( enable_xp < XP_VAPlay ) {
                t=ao_get_delay()-XP_MIN_AUDIOBUFF;
                if(t>XP_MAX_TIMESLICE)
                    t=XP_MAX_TIMESLICE;
            } else
                t = XP_MAX_TIMESLICE;

            usleep(t*1000000);
            time_frame-=GetRelativeTime();
            if(enable_xp >= XP_VAPlay || t<XP_MAX_TIMESLICE || time_frame>XP_MAX_TIMESLICE) {
                vo_check_events();
		return 0;
            }
        }

    while(time_frame>XP_MIN_TIMESLICE)
    {
	/* free cpu for threads */
	usleep(1);
	time_frame-=GetRelativeTime();
    }
    pinfo[xp_id].current_module="sleep_usleep";
    time_frame=SleepTime(rtc_fd,softsleep,time_frame);
    }
    pinfo[xp_id].current_module="change_frame2";
    vo_check_events();
    /* don't flip if there is nothing new to display */
    if(!blit_frame)
    {
        static int drop_message=0;
        if(!drop_message &&
	    abs_dec_ahead_active_frame > 50 &&
	    ada_locked_frame <= abs_dec_ahead_active_frame - 50)
	    {
		drop_message=1;
		if(mpxp_after_seek) mpxp_after_seek--;
		else
		MSG_WARN(MSGTR_SystemTooSlow);
		MSG_D("\ndec_ahead_main: TOO SLOW (locked=%u active=%u)\n"
		,ada_locked_frame,abs_dec_ahead_active_frame);
	    }
	    MSG_D("\ndec_ahead_main: stalling %u (locked: %u blitted: %u local: %u)\n",dec_ahead_active_frame,da_locked_frame,ada_blitted_frame,loc_dec_ahead_active_frame);
	/* Don't burn CPU here! With using of v_pts for A-V sync we will enter
	   xp_decore_video without any delay (like while(1);)
	   Sleeping for 10 ms doesn't matter with frame dropping */
	usleep(0);
    }
    else
    {
	unsigned int t2=GetTimer();
	double tt;
	vo_change_frame();
	MSG_D("\ndec_ahead_main: schedule %u on screen (abs_active: %u loc_active: %u abs_blitted %u)\n",dec_ahead_active_frame,abs_dec_ahead_active_frame,loc_dec_ahead_active_frame,ada_blitted_frame);
	LOCK_VDEC_ACTIVE();
	dec_ahead_active_frame=(dec_ahead_active_frame+1)%xp_num_frames;
	loc_dec_ahead_active_frame++;
	UNLOCK_VDEC_ACTIVE();
#ifdef USE_OSD
	/*--------- add OSD to the next frame contents ---------*/
	MSG_D("dec_ahead_main: draw_osd to %u\n",dec_ahead_active_frame);
	pinfo[xp_id].current_module="draw_osd";
	update_osd(shva[dec_ahead_active_frame].stream_pts);
	vo_draw_osd();
#endif
	t2=GetTimer()-t2;
	tt = t2*0.000001f;
	vout_time_usage+=tt;
	if(benchmark)
	{
		/* we need compute draw_slice+change_frame here */
		cur_vout_time_usage+=tt;
		if((cur_video_time_usage + cur_vout_time_usage + cur_audio_time_usage)*vo_fps > 1)
							bench_dropped_frames ++;
	}
    }
    LOCK_VDEC_ACTIVE();
    abs_dec_ahead_active_frame++;
    UNLOCK_VDEC_ACTIVE();
    pinfo[xp_id].current_module=NULL;

/*================ A-V TIMESTAMP CORRECTION: =========================*/
  /* FIXME: this block was added to fix A-V resync caused by some strange things
     like playing 48KHz audio on 44.1KHz soundcard and other.
     Now we know PTS of every audio frame so don't need to have it */
  if(sh_audio && (!audio_eof || ao_get_delay()) && !av_sync_pts){
    float a_pts=0;

    // unplayed bytes in our and soundcard/dma buffer:
    float delay=ao_get_delay()+(float)sh_audio->a_buffer_len/(float)sh_audio->f_bps;
    if(enable_xp>=XP_VideoAudio)
        delay += get_delay_audio_buffer();

    if(pts_from_bps){
        unsigned int samples=(sh_audio->audio.dwSampleSize)?
          ((ds_tell(d_audio)-sh_audio->a_in_buffer_len)/sh_audio->audio.dwSampleSize) :
          (d_audio->pack_no); // <- used for VBR audio
	samples+=sh_audio->audio.dwStart; // offset
        a_pts=samples*(float)sh_audio->audio.dwScale/(float)sh_audio->audio.dwRate;
      delay_corrected=1;
    } else {
      // PTS = (last timestamp) + (bytes after last timestamp)/(bytes per sec)
      a_pts=d_audio->pts;
      if(!delay_corrected) if(a_pts) delay_corrected=1;
      a_pts+=(ds_tell_pts_r(d_audio)-sh_audio->a_in_buffer_len)/(float)sh_audio->i_bps;
    }

      MSG_DBG2("### A:%8.3f (%8.3f)  V:%8.3f  A-V:%7.4f  \n",a_pts,a_pts-audio_delay-delay,*v_pts,(a_pts-delay-audio_delay)-*v_pts);

      if(delay_corrected && blit_frame){
        float x;
	AV_delay=(a_pts-delay-audio_delay)-*v_pts;
        x=AV_delay*0.1f;
        if(x<-max_pts_correction) x=-max_pts_correction; else
        if(x> max_pts_correction) x= max_pts_correction;
        if(default_max_pts_correction>=0)
          max_pts_correction=default_max_pts_correction;
        else
	{
	    LOCK_VDECA();
	    max_pts_correction=shva[dec_ahead_active_frame].duration*0.10; // +-10% of time
	    UNLOCK_VDECA();
	}
        if(enable_xp>=XP_VAPlay)
            pthread_mutex_lock(&audio_timer_mutex);
        sh_audio->timer+=x;
        if(enable_xp>=XP_VAPlay)
            pthread_mutex_unlock(&audio_timer_mutex);
        c_total+=x;
        if(benchmark && verbose)
	    MSG_STATUS("A:%6.1f V:%6.1f A-V:%7.3f ct:%7.3f  %3d/%3d  %2d%% %2d%% %4.1f%% %d %d\r",
	    a_pts-audio_delay-delay,*v_pts,AV_delay,c_total,
            (int)sh_video->num_frames,num_frames_decoded,
            (sh_video->timer>0.5)?(int)(100.0*video_time_usage/(double)sh_video->timer):0,
            (sh_video->timer>0.5)?(int)(100.0*vout_time_usage/(double)sh_video->timer):0,
            (sh_video->timer>0.5)?(100.0*(audio_time_usage+audio_decode_time_usage)/(double)sh_video->timer):0
            ,vstat->drop_frame_cnt
	    ,output_quality
	    );
        fflush(stdout);
      }

  } else {
    // No audio or pts:

    if(benchmark && verbose) {
        if(av_sync_pts && sh_audio && (!audio_eof || ao_get_delay())) {
            float a_pts = sh_audio->timer-ao_get_delay();
	    MSG_STATUS("A:%6.1f V:%6.1f A-V:%7.3f ct:%7.3f  %3d/%3d  %2d%% %2d%% %4.1f%% %d %d\r",
	    a_pts,sh_video->timer,a_pts-sh_video->timer,0.0,
            (int)sh_video->num_frames,num_frames_decoded,
            (sh_video->timer>0.5)?(int)(100.0*video_time_usage/(double)sh_video->timer):0,
            (sh_video->timer>0.5)?(int)(100.0*vout_time_usage/(double)sh_video->timer):0,
            (sh_video->timer>0.5)?(100.0*(audio_time_usage+audio_decode_time_usage)/(double)sh_video->timer):0
            ,vstat->drop_frame_cnt
	    ,output_quality
    	    );
            ;
        } else
	MSG_STATUS("V:%6.1f  %3d  %2d%% %2d%% %4.1f%% %d %d\r",*v_pts,
        (int)sh_video->num_frames,
        (sh_video->timer>0.5)?(int)(100.0*video_time_usage/(double)sh_video->timer):0,
        (sh_video->timer>0.5)?(int)(100.0*vout_time_usage/(double)sh_video->timer):0,
        (sh_video->timer>0.5)?(100.0*(audio_time_usage+audio_decode_time_usage)/(double)sh_video->timer):0
          ,vstat->drop_frame_cnt
	  ,output_quality
        );

      fflush(stdout);
    }
  }
  return 0;
}

void mpxp_seek( int xp_id, video_stat_t *vstat, int *osd_visible,float v_pts,float pos,int flags)
{
  int seek_rval=1;
  audio_eof=0;
  if(pos || flags&DEMUX_SEEK_SET) {
    seek_rval=demux_seek_r(demuxer,pos,flags);
    mpxp_after_seek=25; /* 1 sec delay */
  }
  if(seek_rval){
      mpxp_seek_time = GetTimerMS();
      if(enable_xp!=XP_None && sh_video)
      { // Send back frame info to decoding thread
          dec_ahead_seek_num_frames = sh_video->num_frames;
          dec_ahead_seek_num_frames_decoded = sh_video->num_frames_decoded;
      }

      // success:
      /* FIXME there should be real seeking for vobsub */
      if (vo_vobsub)
	vobsub_reset(vo_vobsub);
      if (vo_spudec)
	spudec_reset(vo_spudec);

      if(sh_audio){
	if(verbose){
	    float a_pts=d_audio->pts;
            a_pts+=(ds_tell_pts_r(d_audio)-sh_audio->a_in_buffer_len)/(float)sh_audio->i_bps;
	    MSG_V("SEEK: A: %5.3f  V: %5.3f  A-V: %5.3f   \n",a_pts,v_pts,a_pts-v_pts);
	}
        MSG_V("A:%6.1f  V:%6.1f  A-V:%7.3f  ct: ?   \r",d_audio->pts,v_pts,0.0f);
	sh_audio->chapter_change=0;
	sh_audio->a_pts=HUGE;
      } else {
        MSG_V("A: ---   V:%6.1f   \r",v_pts);
      }
      fflush(stdout);

      if(sh_video){
	 pinfo[xp_id].current_module="seek_video_reset";
	 resync_video_stream(sh_video);
         vo_reset();
         sh_video->chapter_change=-1;
      }
      
      if(sh_audio){
        pinfo[xp_id].current_module="seek_audio_reset";
	resync_audio_stream(sh_audio);
        ao_reset(); // stop audio, throwing away buffered data
      }

      if (vo_vobsub) {
	pinfo[xp_id].current_module = "seek_vobsub_reset";
	vobsub_seek(vo_vobsub, v_pts);
      }

#ifdef USE_OSD
        // Set OSD:
      if(osd_level){
        int len=((demuxer->movi_end-demuxer->movi_start)>>8);
        if (len>0){
	   if(osd_visible) *osd_visible=sh_video->fps<=60?sh_video->fps:25;
	   vo_osd_progbar_type=0;
	   vo_osd_progbar_value=(demuxer->filepos-demuxer->movi_start)/len;
	   vo_osd_changed(OSDTYPE_PROGBAR);
	}
      }
#endif
      if(sh_video) {
	c_total=0;
	max_pts_correction=0.1;
	if(osd_visible) *osd_visible=sh_video->fps<=60?sh_video->fps:25; // to rewert to PLAY pointer after 1 sec
	audio_time_usage=0; audio_decode_time_usage=0; video_time_usage=0; vout_time_usage=0;
	if(vstat)
	{
	    vstat->drop_frame_cnt=0;
	    vstat->too_slow_frame_cnt=0;
	    vstat->too_fast_frame_cnt=0;
	}
        if(vo_spudec) {
          unsigned char* packet=NULL;
          while(ds_get_packet_sub(d_dvdsub,&packet)>0)
              ; // Empty stream
          spudec_reset(vo_spudec);
        }
      }
  }
}

void mpxp_reset_vcache(void)
{
 unsigned i;
 for(i=0;i<xp_threads;i++) if(strcmp(pinfo[i].thread_name,"main")==0) break;
 if(shva) mpxp_seek(i,NULL,NULL,shva[dec_ahead_active_frame].v_pts,0,DEMUX_SEEK_CUR|DEMUX_SEEK_SECONDS);
 return;
}

void mpxp_resync_audio_stream(void)
{
  resync_audio_stream(sh_audio);
}

static void __FASTCALL__ mpxp_stream_event_handler(struct stream_s *s,const stream_packet_t *sp)
{
    s->driver->control(s,SCRTL_EVT_HANDLE,(void *)sp);
}

static void init_benchmark(void)
{
	max_audio_time_usage=0; max_video_time_usage=0; max_vout_time_usage=0;
	min_audio_time_usage=HUGE; min_video_time_usage=HUGE; min_vout_time_usage=HUGE;

	min_audio_decode_time_usage=HUGE;
	max_audio_decode_time_usage=0;
	bench_dropped_frames=0;

	max_demux_time_usage=0;
	demux_time_usage=0;
	min_demux_time_usage=HUGE;

	cur_video_time_usage=0;
	cur_vout_time_usage=0;
	cur_audio_time_usage=0;

	max_av_resync=0;
}

static void show_benchmark(video_stat_t *vstat)
{
  double tot=(video_time_usage+vout_time_usage+audio_time_usage+audio_decode_time_usage+demux_time_usage+c2_time_usage);
  double min_tot=(min_video_time_usage+min_vout_time_usage+min_audio_time_usage+min_audio_decode_time_usage+min_demux_time_usage+min_c2_time_usage)*our_n_frames;
  double max_tot=(max_video_time_usage+max_vout_time_usage+max_audio_time_usage+max_audio_decode_time_usage+max_demux_time_usage+max_c2_time_usage)*our_n_frames;
  double total_time_usage;
  if(enable_xp==XP_None)
  {
	max_video_time_usage *= our_n_frames;
	max_vout_time_usage *= our_n_frames;
	max_audio_time_usage *= our_n_frames;
	max_demux_time_usage *= our_n_frames;
	max_c2_time_usage *= our_n_frames;
	min_video_time_usage *= our_n_frames;
	min_vout_time_usage *= our_n_frames;
	min_audio_time_usage *= our_n_frames;
	min_demux_time_usage *= our_n_frames;
	min_c2_time_usage *= our_n_frames;
  }
  total_time_usage_start=GetTimer()-total_time_usage_start;
  total_time_usage = (float)total_time_usage_start*0.000001;
  if(enable_xp!=XP_None)
  {
    MSG_INFO("\nMIN BENCHMARKs: *** MEANINGLESS IN XP MODE *** \n");
    MSG_INFO("MIN BENCHMARK%%: *** MEANINGLESS IN XP MODE *** \n");
  }
  else
  {
  MSG_INFO("\nMIN BENCHMARKs: VC:%8.3fs VO:%8.3fs A:%8.3fs D:%8.3fs C:%8.3fs\n",
	 min_video_time_usage,min_vout_time_usage,
	 min_audio_time_usage+min_audio_decode_time_usage,
	 min_demux_time_usage,min_c2_time_usage);
  if(total_time_usage>0.0)
    MSG_INFO("MIN BENCHMARK%%: VC:%8.4f%% VO:%8.4f%% A:%8.4f%% D:%8.4f%% C:%8.4f%%= %8.4f%%\n",
	   100.0*min_video_time_usage/total_time_usage,
	   100.0*min_vout_time_usage/total_time_usage,
	   100.0*(min_audio_time_usage+min_audio_decode_time_usage)/total_time_usage,
	   100.0*min_demux_time_usage/total_time_usage,
	   100.0*min_c2_time_usage/total_time_usage,
	   100.0*min_tot/total_time_usage
	   );
  }
  MSG_INFO("\nAVE BENCHMARKs: VC:%8.3fs VO:%8.3fs A:%8.3fs D:%8.3fs = %8.4fs C:%8.3fs\n",
	 video_time_usage,vout_time_usage,audio_time_usage+audio_decode_time_usage,
	 demux_time_usage,c2_time_usage,tot);
  if(total_time_usage>0.0)
    MSG_INFO("AVE BENCHMARK%%: VC:%8.4f%% VO:%8.4f%% A:%8.4f%% D:%8.4f%% C:%8.4f%% = %8.4f%%\n",
	   100.0*video_time_usage/total_time_usage,
	   100.0*vout_time_usage/total_time_usage,
	   100.0*(audio_time_usage+audio_decode_time_usage)/total_time_usage,
	   100.0*demux_time_usage/total_time_usage,
	   100.0*c2_time_usage/total_time_usage,
	   100.0*tot/total_time_usage);
  if(enable_xp!=XP_None)
  {
    MSG_INFO("\nMAX BENCHMARKs: *** MEANINGLESS IN XP MODE *** \n");
    MSG_INFO("MAX BENCHMARK%%: *** MEANINGLESS IN XP MODE *** \n");
  }
  else
  {
  MSG_INFO("\nMAX BENCHMARKs: VC:%8.3fs VO:%8.3fs A:%8.3fs D:%8.3fs C:%8.3fs\n",
	 max_video_time_usage,max_vout_time_usage,
	 max_audio_time_usage+max_audio_decode_time_usage,max_demux_time_usage,
	 max_c2_time_usage);
  if(total_time_usage>0.0)
    MSG_INFO("MAX BENCHMARK%%: VC:%8.4f%% VO:%8.4f%% A:%8.4f%% D:%8.4f%% C:%8.4f%% = %8.4f%%\n",
	   100.0*max_video_time_usage/total_time_usage,
	   100.0*max_vout_time_usage/total_time_usage,
	   100.0*(max_audio_time_usage+max_audio_decode_time_usage)/total_time_usage,
	   100.0*max_demux_time_usage/total_time_usage,
	   100.0*max_c2_time_usage/total_time_usage,
	   100.0*max_tot/total_time_usage
	   );
/* This code computes number of frame which should be dropped
   in ideal case (without SYSTIME); i.e. when file is located
   in RAM and kernel+other_tasks eat 0% of CPU. */
    MSG_INFO("TOTAL BENCHMARK: from %u frames should be dropped: %u (at least)\n"
	    ,our_n_frames,bench_dropped_frames);
  }
  if(enable_xp!=XP_None)
  MSG_INFO("\nREAL RESULTS: from %u was dropped=%u\n"
	,our_n_frames,xp_drop_frame_cnt);
  else
  MSG_INFO("\nREAL RESULTS: dropped=%u too slow=%u too fast=%u\n"
	,vstat->drop_frame_cnt,vstat->too_slow_frame_cnt,vstat->too_fast_frame_cnt);
  MSG_INFO("\nMax. A-V resync is: %f\n",fabs(max_av_resync));
}

static void show_benchmark_status(void)
{
    if( enable_xp <= XP_Video )
		MSG_STATUS("A:%6.1f %4.1f%%\r"
			,sh_audio->timer-ao_get_delay()
			,(sh_audio->timer>0.5)?100.0*(audio_time_usage+audio_decode_time_usage)/(double)sh_audio->timer:0
			);
    else
	MSG_STATUS("A:%6.1f %4.1f%%  B:%4.1f\r"
		,sh_audio->timer-ao_get_delay()
		,(sh_audio->timer>0.5)?100.0*(audio_time_usage+audio_decode_time_usage)/(double)sh_audio->timer:0
		,get_delay_audio_buffer()
		);
}
 
int v_bright=0;
int v_cont=0;
int v_hue=0;
int v_saturation=0;
/*
For future:
int v_red_intensity=0;
int v_green_intensity=0;
int v_blue_intensity=0;
*/
// for multifile support:
play_tree_iter_t* playtree_iter = NULL;

/******************************************\
* MAIN MPLAYERXP FUNCTION !!!              *
\******************************************/
int main(int argc,char* argv[], char *envp[]){

int stream_dump_type=0;
int after_dvdmenu=0;
char* filename=NULL; //"MI2-Trailer.avi";
int file_format=DEMUXER_TYPE_UNKNOWN;
int (*decore_video_ptr)( int rtc_fd, video_stat_t *vstat, float *aq_sleep_time, float *v_pts )
			 = mp09_decore_video;

// movie info:
int eof=0;

int osd_visible=100;
int osd_info_factor=9;
video_stat_t vstat;
int rtc_fd=-1;
int i;
int forced_subs_only=0;
float seek_secs=0;
int seek_flags=DEMUX_SEEK_CUR|DEMUX_SEEK_SECONDS;


    mplayer_pid=
    pinfo[xp_id].pid=getpid();
    mplayer_pth_id=
    pinfo[xp_id].pth_id=pthread_self();
    pinfo[xp_id].thread_name = "main";
    memset(&vstat,0,sizeof(video_stat_t));

    mp_msg_init(MSGL_STATUS);
    MSG_INFO("%s",banner_text);
//  memset(&vtune,0,sizeof(vo_tune_info_t));
  /* Test for cpu capabilities (and corresponding OS support) for optimizing */

    playtree = play_tree_new();

    mconfig = m_config_new(playtree);
    m_config_register_options(mconfig,mplayer_opts);
    // TODO : add something to let modules register their options
    mp_register_options(mconfig);
    parse_cfgfiles(mconfig);

    if(m_config_parse_command_line(mconfig, argc, argv, envp) < 0) exit(1); // error parsing cmdline

    xp_num_cpu=get_number_cpu();
#if defined( ARCH_X86 ) || defined(ARCH_X86_64)
    get_mmx_optimizations();
#endif
    if(!sws_init()) {
	MSG_ERR("MPlayerXP requires working copy of libswscaler\n");
	return 0;
    }
    if(shuffle_playback) playtree->flags|=PLAY_TREE_RND;
    else		 playtree->flags&=~PLAY_TREE_RND;
    playtree = play_tree_cleanup(playtree);
    if(playtree) {
      playtree_iter = play_tree_iter_new(playtree,mconfig);
      if(playtree_iter) {  
	if(play_tree_iter_step(playtree_iter,0,0) != PLAY_TREE_ITER_ENTRY) {
	  play_tree_iter_free(playtree_iter);
	  playtree_iter = NULL;
	}
	filename = play_tree_iter_get_file(playtree_iter,1);
      }
    }

    ao_da_buffs = vo_da_buffs;
    if(enable_xp!=XP_None) vo_doublebuffering=1;
    else	  vo_use_bm = 0;
	
    init_player();

    if(!filename){
	show_help();
	return 0;
    }

    // Many users forget to include command line in bugreports...
    if(verbose){
      MSG_INFO("CommandLine:");
      for(i=1;i<argc;i++) MSG_INFO(" '%s'",argv[i]);
      MSG_INFO("\n");
    }

    mp_msg_init(verbose+MSGL_STATUS);

//------ load global data first ------


// check font
#ifdef USE_OSD
  if(font_name){
       vo_font=read_font_desc(font_name,font_factor,verbose>1);
       if(!vo_font) MSG_ERR(MSGTR_CantLoadFont,font_name);
  } else {
      // try default:
       vo_font=read_font_desc(get_path("font/font.desc"),font_factor,verbose>1);
       if(!vo_font)
       vo_font=read_font_desc(DATADIR"/font/font.desc",font_factor,verbose>1);
  }
#endif
  /* Configure menu here */
  {
     char *menu_cfg;
     menu_cfg = get_path("menu.conf");
     if(menu_init(NULL, menu_cfg))
       MSG_INFO("Menu initialized: %s\n", menu_cfg);
     else {
       menu_cfg="/etc/menu.conf";
       if(menu_init(NULL, menu_cfg))
	MSG_INFO("Menu initialized: %s\n", menu_cfg);
       else {
         MSG_WARN("Menu init failed\n");
       }
     }
  }
  pinfo[xp_id].current_module="init_osd";
  vo_init_osd();

#ifdef HAVE_TERMCAP
  load_termcap(NULL); // load key-codes
#endif

// ========== Init keyboard FIFO (connection to libvo) ============
make_pipe(&keyb_fifo_get,&keyb_fifo_put);

/* Init input system */
pinfo[xp_id].current_module = "init_input";
mp_input_init();
if(keyb_fifo_get > 0)
  mp_input_add_key_fd(keyb_fifo_get,1,NULL,NULL);
if(slave_mode)
   mp_input_add_cmd_fd(0,1,NULL,NULL);
else if(!use_stdin)
  mp_input_add_key_fd(0,1,NULL,NULL);
inited_flags|=INITED_INPUT;
pinfo[xp_id].current_module = NULL;

xp_id = init_signal_handling(exit_sighandler,NULL);

// ******************* Now, let's see the per-file stuff ********************
play_next_file:

// We must enable getch2 here to be able to interrupt network connection
// or cache filling
if(!use_stdin && !slave_mode){
  getch2_enable();  // prepare stdin for hotkeys...
  inited_flags|=INITED_GETCH2;
}

    // check video_out driver name:
    if (video_driver)
	if ((i = strcspn(video_driver, ":")) > 0) {
	    size_t i2 = strlen(video_driver);
	    if (video_driver[i] == ':') {
		vo_subdevice = malloc(i2-i);
		if (vo_subdevice != NULL)
		    strncpy(vo_subdevice, (char *)(video_driver+i+1), i2-i);
		video_driver[i] = '\0';
	    }
	}
    pinfo[xp_id].current_module="vo_register";
    vo_inited = (vo_register(video_driver)!=NULL)?1:0;

    if(!vo_inited){
	MSG_FATAL(MSGTR_InvalidVOdriver,video_driver?video_driver:"?");
	exit_player(MSGTR_Exit_error);
    }
    pinfo[xp_id].current_module="vo_init";
    if((i=vo_init(vo_subdevice))!=0)
    {
	MSG_FATAL("Error opening/initializing the selected video_out (-vo) device!\n");
	exit_player(MSGTR_Exit_error);
    }

// check audio_out driver name:
    pinfo[xp_id].current_module="ao_init";
    if (audio_driver)
	if ((i = strcspn(audio_driver, ":")) > 0)
	{
	    size_t i2 = strlen(audio_driver);

	    if (audio_driver[i] == ':')
	    {
		ao_subdevice = malloc(i2-i);
		if (ao_subdevice != NULL)
		    strncpy(ao_subdevice, (char *)(audio_driver+i+1), i2-i);
		audio_driver[i] = '\0';
	    }
	}
  ao_inited=(ao_register(audio_driver)!=NULL)?1:0;
  if (!ao_inited){
    MSG_FATAL(MSGTR_InvalidAOdriver,audio_driver);
    exit_player(MSGTR_Exit_error);
  }

    if(filename) MSG_OK(MSGTR_Playing, filename);

    pinfo[xp_id].current_module="vobsub";
    if (vobsub_name){
      vo_vobsub=vobsub_open(vobsub_name,spudec_ifo,1,&vo_spudec);
      if(vo_vobsub==NULL)
        MSG_ERR(MSGTR_CantLoadSub,vobsub_name);
      else {
	inited_flags|=INITED_VOBSUB;
	vobsub_set_from_lang(vo_vobsub, dvdsub_lang);
	// check if vobsub requested only to display forced subtitles
	forced_subs_only=vobsub_get_forced_subs_flag(vo_vobsub);
      }
    }else if(sub_auto && filename && (strlen(filename)>=5)){
      /* try to autodetect vobsub from movie filename ::atmos */
      char *buf = malloc((strlen(filename)-3) * sizeof(char));
      memset(buf,0,strlen(filename)-3); // make sure string is terminated
      strncpy(buf, filename, strlen(filename)-4); 
      vo_vobsub=vobsub_open(buf,spudec_ifo,0,&vo_spudec);
      free(buf);
    }
    if(vo_vobsub)
    {
      sub_auto=0; // don't do autosub for textsubs if vobsub found
      inited_flags|=INITED_VOBSUB;
    }

    pinfo[xp_id].current_module="mplayer";
    if(!after_dvdmenu) {
	stream=NULL;
	demuxer=NULL;
    }
    d_audio=NULL;
    d_video=NULL;
    sh_audio=NULL;
    sh_video=NULL;

//============ Open & Sync STREAM --- fork cache2 ====================
    stream_dump_type=0;
    if(stream_dump)
	if((stream_dump_type=dump_parse(stream_dump))==0)
	{
	    MSG_ERR("Wrong dump parameters! Unable to continue\n");
	    exit_player(MSGTR_Exit_error);
	}

  if(stream_dump_type) stream_cache_size=0;
  pinfo[xp_id].current_module="open_stream";
  if(!after_dvdmenu) stream=open_stream(filename,&file_format,stream_dump_type>1?dump_stream_event_handler:mpxp_stream_event_handler);
  if(!stream) { // error...
    eof = libmpdemux_was_interrupted(PT_NEXT_ENTRY);
    goto goto_next_file;
  }
  inited_flags|=INITED_STREAM;

  if(stream->type & STREAMTYPE_TEXT) {
    play_tree_t* entry;
    // Handle playlist
    pinfo[xp_id].current_module="handle_playlist";
    MSG_V("Parsing playlist %s...\n",filename);
    entry = parse_playtree(stream);
    if(!entry) {      
      entry = playtree_iter->tree;
      if(play_tree_iter_step(playtree_iter,1,0) != PLAY_TREE_ITER_ENTRY) {
	eof = PT_NEXT_ENTRY;
	goto goto_next_file;
      }
      if(playtree_iter->tree == entry ) { // Loop with a single file
	if(play_tree_iter_up_step(playtree_iter,1,0) != PLAY_TREE_ITER_ENTRY) {
	  eof = PT_NEXT_ENTRY;
	  goto goto_next_file;
	}
      }
      play_tree_remove(entry,1,1);
      eof = PT_NEXT_SRC;
      goto goto_next_file;
    }
    play_tree_insert_entry(playtree_iter->tree,entry);
    entry = playtree_iter->tree;
    if(play_tree_iter_step(playtree_iter,1,0) != PLAY_TREE_ITER_ENTRY) {
      eof = PT_NEXT_ENTRY;
      goto goto_next_file;
    }      
    play_tree_remove(entry,1,1);
    eof = PT_NEXT_SRC;
    goto goto_next_file;
  }

#ifdef HAVE_LIBCSS
  pinfo[xp_id].current_module="libcss";
  if (dvdimportkey) {
    if (dvd_import_key(dvdimportkey)) {
	MSG_FATAL(MSGTR_ErrorDVDkey);
	exit_player(MSGTR_Exit_error);
    }
    MSG_INFO(MSGTR_CmdlineDVDkey);
  }
  if (dvd_auth_device) {
    if (dvd_auth(dvd_auth_device,filename)) {
	MSG_FATAL("Error in DVD auth...\n");
	exit_player(MSGTR_Exit_error);
      } 
    MSG_INFO(MSGTR_DVDauthOk);
  }
#endif

/* Add NLS support here */
if(!audio_lang) audio_lang=nls_get_screen_cp();
{
  char *lang;
  pinfo[xp_id].current_module="dvd lang->id";
  if(audio_lang)
  {
    lang=malloc(max(strlen(audio_lang)+1,4));
    strcpy(lang,audio_lang);
    if(audio_id==-1 && stream->driver->control(stream,SCTRL_LNG_GET_AID,lang) == SCTRL_OK)
    {
	audio_id=*(int *)lang;
    }
    free(lang);
  }
  if(dvdsub_lang)
  {
    lang=malloc(max(strlen(dvdsub_lang)+1,4));
    strcpy(lang,dvdsub_lang);
    if(dvdsub_id==-1 && stream->driver->control(stream,SCTRL_LNG_GET_SID,lang) == SCTRL_OK)
    {
	dvdsub_id=*(int *)lang;
    }
    free(lang);
  }
  pinfo[xp_id].current_module=NULL;
}

// CACHE2: initial prefill: 20%  later: 5%  (should be set by -cacheopts)
if(stream_cache_size && !stream_dump_type){
  pinfo[xp_id].current_module="enable_cache";
  if(!stream_enable_cache(stream,stream_cache_size*1024,stream_cache_size*1024/5,stream_cache_size*1024/20))
    if((eof = libmpdemux_was_interrupted(PT_NEXT_ENTRY))) goto goto_next_file;
}

// DUMP STREAMS:
if(stream_dump_type==1) dump_stream(stream);

//============ Open DEMUXERS --- DETECT file type =======================
initial_audio_pts=HUGE;
if(!has_audio) audio_id=-2; // do NOT read audio packets...

pinfo[xp_id].current_module="demux_open";

if(!after_dvdmenu) demuxer=demux_open(stream,file_format,audio_id,video_id,dvdsub_id);
if(!demuxer) goto goto_next_file; // exit_player(MSGTR_Exit_error); // ERROR
inited_flags|=INITED_DEMUXER;
after_dvdmenu=0;

d_audio=demuxer->audio;
d_video=demuxer->video;
d_dvdsub=demuxer->sub;

if(seek_to_byte) stream_skip(stream,seek_to_byte);


sh_audio=d_audio->sh;
sh_video=d_video->sh;

MSG_INFO("[Stream]:");
if(sh_video)
{
    MSG_INFO("Video=");
    {
	int fmt;
	char *c;
	if(sh_video->bih) fmt=sh_video->bih->biCompression;
	else		  fmt=sh_video->format;
	c=(char *)&fmt;
	if(isprint(c[0]) && isprint(c[1]) && isprint(c[2]) && isprint(c[3]))
	    MSG_INFO("%.4s",c);
	else
	    MSG_INFO("%08X",fmt);
    }
}
if(sh_audio)
{
    MSG_INFO(" Audio=");
    {
	int fmt;
	char *c;
	fmt=sh_audio->format;
	c=(char *)&fmt;
	if(isprint(c[0]) && isprint(c[1]) && isprint(c[2]) && isprint(c[3]))
	    MSG_INFO("%.4s",c);
	else
	    MSG_INFO("%08X",fmt);
    }
}
MSG_INFO("\n");

if(sh_video){

  pinfo[xp_id].current_module="video_read_properties";
  if(!video_read_properties(sh_video))
  {
    MSG_ERR("Video: can't read properties\n");
    sh_video=d_video->sh=NULL;
  }
  else {
    MSG_V("[V] filefmt:%d  fourcc:0x%X  size:%dx%d  fps:%5.2f  ftime:=%6.4f\n",
	   demuxer->file_format,sh_video->format, sh_video->disp_w,sh_video->disp_h,
	   sh_video->fps,sh_video->frametime
	   );

    vo_fps = sh_video->fps;
    /* need to set fps here for output encoders to pick it up in their init */
    if(force_fps){
      sh_video->fps=force_fps;
      sh_video->frametime=1.0f/sh_video->fps;
      vo_fps = force_fps;
    }

    if(!sh_video->fps && !force_fps){
      MSG_ERR(MSGTR_FPSnotspecified);
      sh_video=d_video->sh=NULL;
    }
  }
}

if(sh_video)
{
    if(sh_video->is_static || (stream->type&STREAMTYPE_MENU)==STREAMTYPE_MENU)
    {
	vo_da_buffs=1;
	enable_xp=XP_None; /* supress video mode for static pictures */
    }
}else 
    if(enable_xp < XP_VAPlay) enable_xp=XP_None;

fflush(stdout);

if(!sh_video && !sh_audio){
    MSG_FATAL("No stream found\n");
    goto goto_next_file; // exit_player(MSGTR_Exit_error);
}

//================== Read SUBTITLES (DVD & TEXT) ==========================
if(sh_video){

if (spudec_ifo) {
  unsigned int palette[16], width, height;
  pinfo[xp_id].current_module="spudec_init_vobsub";
  if (vobsub_parse_ifo(NULL,spudec_ifo, palette, &width, &height, 1, -1, NULL) >= 0)
    vo_spudec=spudec_new_scaled(palette, sh_video->disp_w, sh_video->disp_h);
}

if (vo_spudec==NULL) {
  unsigned *pal;
  pinfo[xp_id].current_module="spudec_init";
  if(stream->driver->control(stream,SCTRL_VID_GET_PALETTE,&pal)==SCTRL_OK)
	vo_spudec=spudec_new_scaled(pal,sh_video->disp_w, sh_video->disp_h);
}

if (vo_spudec==NULL) {
  pinfo[xp_id].current_module="spudec_init_normal";
  vo_spudec=spudec_new_scaled(NULL, sh_video->disp_w, sh_video->disp_h);
  spudec_set_font_factor(vo_spudec,font_factor);
}

if (vo_spudec!=NULL) {
  inited_flags|=INITED_SPUDEC;
  // Apply current settings for forced subs
  spudec_set_forced_subs_only(vo_spudec,forced_subs_only);
}

#ifdef USE_SUB
// after reading video params we should load subtitles because
// we know fps so now we can adjust subtitles time to ~6 seconds AST
// check .sub
  pinfo[xp_id].current_module="read_subtitles_file";
  if(sub_name){
    subtitles=sub_read_file(sub_name, sh_video->fps);
    if(!subtitles) MSG_ERR(MSGTR_CantLoadSub,sub_name);
  } else
  if(sub_auto) { // auto load sub file ...
    subtitles=sub_read_file( filename ? sub_filename( get_path("sub/"), filename )
	                              : "default.sub", sh_video->fps );
  }
  if(subtitles)
  {
    inited_flags|=INITED_SUBTITLE;
    if(stream_dump_type>1) list_sub_file(subtitles);
  }
#endif	

}
//================== Init AUDIO (codec) ==========================
pinfo[xp_id].current_module="init_audio_codec";

/* accept parameters*/
if(audio_codec)
{
  char *prm;
  prm = strchr(audio_codec,':');
  if(prm)
  {
    audio_codec_param=prm+1;
    *prm=0;
  }
}

if(sh_audio){
  // Go through the codec.conf and find the best codec...
  sh_audio->codec=NULL;
  if(audio_family) MSG_INFO(MSGTR_TryForceAudioFmt,audio_family);
  while(1){
    const char *fmt;
    sh_audio->codec=find_codec(sh_audio->format,NULL,sh_audio->codec,1);
    if(!sh_audio->codec){
      if(audio_family) {
        sh_audio->codec=NULL; /* re-search */
        MSG_ERR(MSGTR_CantFindAfmtFallback);
        audio_family=NULL;
        continue;      
      }
	MSG_ERR(MSGTR_CantFindAudioCodec);
	fmt = (const char *)&sh_audio->format;
	if(isprint(fmt[0]) && isprint(fmt[1]) && isprint(fmt[2]) && isprint(fmt[3]))
	    MSG_ERR(" '%c%c%c%c'!\n",fmt[0],fmt[1],fmt[2],fmt[3]);
	else MSG_ERR(" 0x%08X!\n",sh_audio->format);
	MSG_HINT( MSGTR_TryUpgradeCodecsConfOrRTFM,get_path("codecs.conf"));
	sh_audio=d_audio->sh=NULL;
	break;
    }
    if(audio_codec && strcmp(sh_audio->codec->codec_name,audio_codec)) continue;
    else if(audio_family && strcmp(sh_audio->codec->driver_name,audio_family)) continue;
    MSG_V("%s audio codec: [%s] drv:%s (%s)\n",audio_codec?"Forcing":"Detected",sh_audio->codec->codec_name,sh_audio->codec->driver_name,sh_audio->codec->s_info);
    break;
  }
}

if(stream_dump_type>1)
{
  dump_mux_init(demuxer);
  goto dump_file;
}

if(!ao_init(0)){
    MSG_ERR(MSGTR_CannotInitAO);
    sh_audio=d_audio->sh=NULL;
}

if(sh_audio){
  MSG_V("Initializing audio codec...\n");
  if(!init_audio(sh_audio)){
    MSG_ERR(MSGTR_CouldntInitAudioCodec);
    sh_audio=d_audio->sh=NULL;
  } else {
    MSG_V("AUDIO: srate=%d  chans=%d  bps=%d  sfmt=0x%X  ratio: %d->%d\n",sh_audio->samplerate,sh_audio->channels,sh_audio->samplesize,
        sh_audio->sample_format,sh_audio->i_bps,sh_audio->f_bps);
  }
}

if(sh_audio)   inited_flags|=INITED_ACODEC;

if(stream_dump_type>1)
{
  dump_file:
  dump_mux(demuxer,av_sync_pts,seek_to_sec,play_n_frames);
  goto goto_next_file;
}

/*================== Init VIDEO (codec & libvo) ==========================*/
if(!sh_video)
   goto main;

pinfo[xp_id].current_module="init_video_filters";
if(sh_video->vfilter_inited<=0) {
    sh_video->vfilter=vf_init(sh_video);
    sh_video->vfilter_inited=1;
}

pinfo[xp_id].current_module="init_video_codec";

/* Go through the codec.conf and find the best codec...*/
sh_video->inited=0;
codecs_reset_selection(0);
if(video_codec){
    /* forced codec by name: */
    MSG_INFO("Forced video codec: %s\n",video_codec);
    init_video(sh_video,video_codec,NULL,-1);
} else {
    int status;
    /* try in stability order: UNTESTED, WORKING, BUGGY, BROKEN */
    if(video_family) MSG_INFO(MSGTR_TryForceVideoFmt,video_family);
    for(status=CODECS_STATUS__MAX;status>=CODECS_STATUS__MIN;--status){
	if(video_family) /* try first the preferred codec family:*/
	    if(init_video(sh_video,NULL,video_family,status)) break;
	if(init_video(sh_video,NULL,NULL,status)) break;
    }
}
if(!sh_video->inited){
    const char *fmt;
    MSG_ERR(MSGTR_CantFindVideoCodec);
    fmt = (const char *)&sh_video->format;
    if(isprint(fmt[0]) && isprint(fmt[1]) && isprint(fmt[2]) && isprint(fmt[3]))
	MSG_ERR(" '%c%c%c%c'!\n",fmt[0],fmt[1],fmt[2],fmt[3]);
    else MSG_ERR(" 0x%08X!\n",sh_video->format);
    MSG_HINT( MSGTR_TryUpgradeCodecsConfOrRTFM,get_path("codecs.conf"));
    if(!sh_audio) goto goto_next_file;
    sh_video = d_video->sh = NULL;
    goto main;
} else   inited_flags|=INITED_VCODEC;


MSG_V("%s video codec: [%s] vfm:%s (%s)\n",
    video_codec?"Forcing":"Detected",sh_video->codec->codec_name,sh_video->codec->driver_name,sh_video->codec->s_info);

if(auto_quality>0){
    /* Auto quality option enabled*/
    output_quality=get_video_quality_max(sh_video);
    if(auto_quality>output_quality) auto_quality=output_quality;
    else output_quality=auto_quality;
    MSG_V("AutoQ: setting quality to %d\n",output_quality);
    set_video_quality(sh_video,output_quality);
}

vf_showlist(sh_video->vfilter);
// ========== Init display (sh_video->disp_w*sh_video->disp_h/out_fmt) ============

    inited_flags|=INITED_VO;
    MSG_V("INFO: Video OUT driver init OK!\n");
    pinfo[xp_id].current_module="init_libvo";
    fflush(stdout);

//================== MAIN: ==========================
   main:
if(!sh_video) osd_level = 0;
else if(sh_video->fps<60) osd_info_factor=sh_video->fps/2; /* 0.5 sec */

//================ SETUP AUDIO ==========================

if(sh_audio){
  const ao_info_t *info=ao_get_info();
  pinfo[xp_id].current_module="setup_audio";
  MSG_V("AO: [%s] %iHz %s %s\n",
      info->short_name,
      force_srate?force_srate:sh_audio->samplerate,
      sh_audio->channels>7?"surround71":
      sh_audio->channels>6?"surround61":
      sh_audio->channels>5?"surround51":
      sh_audio->channels>4?"surround41":
      sh_audio->channels>3?"surround40":
      sh_audio->channels>2?"stereo2.1":
      sh_audio->channels>1?"Stereo":"Mono",
      ao_format_name(sh_audio->sample_format)
  );
  MSG_V("AO: Description: %s\nAO: Author: %s\n",
      info->name, info->author);
  if(strlen(info->comment) > 0)
      MSG_V("AO: Comment: %s\n", info->comment);

  if(sh_audio){
	pinfo[xp_id].current_module="af_preinit";
	ao_data.samplerate=force_srate?force_srate:sh_audio->samplerate;
	ao_data.channels=audio_output_channels?audio_output_channels:sh_audio->channels;
	ao_data.format=sh_audio->sample_format;
#if 1
	if(!preinit_audio_filters(sh_audio,
	    // input:
	    (int)(sh_audio->samplerate),
	    sh_audio->channels, sh_audio->sample_format, sh_audio->samplesize,
	    // output:
	    &ao_data.samplerate, &ao_data.channels, &ao_data.format,
	    ao_format_bits(ao_data.format)/8)){
	    MSG_ERR("Audio filter chain preinit failed\n");
	} else {
	    MSG_V("AF_pre: %dHz %dch (%s)%08X\n",
		ao_data.samplerate, ao_data.channels,
		ao_format_name(ao_data.format),ao_data.format);
	}
#endif  
    if(!ao_configure(force_srate?force_srate:ao_data.samplerate,
		    ao_data.channels,ao_data.format))
	{
	    MSG_ERR("Can't configure audio device\n");
	    sh_audio=d_audio->sh=NULL;
	    if(sh_video == NULL)  goto goto_next_file;
	}
	else
	{
	    inited_flags|=INITED_AO;
	    pinfo[xp_id].current_module="af_init";
	    if(!init_audio_filters(sh_audio, 
		(int)(sh_audio->samplerate),
	    sh_audio->channels, sh_audio->sample_format, sh_audio->samplesize,
	    ao_data.samplerate, ao_data.channels, ao_data.format,
	    ao_format_bits(ao_data.format)/8, /* ao_data.bps, */
	    ao_data.outburst*4, ao_data.buffersize)){
		MSG_ERR("No matching audio filter found!\n");
	    }
	}
  }
}

pinfo[xp_id].current_module="av_init";

if(av_force_pts_fix2==1 || 
   (av_force_pts_fix2==-1 && av_sync_pts &&
    (d_video->demuxer->file_format == DEMUXER_TYPE_MPEG_ES ||
     d_video->demuxer->file_format == DEMUXER_TYPE_MPEG4_ES ||
     d_video->demuxer->file_format == DEMUXER_TYPE_H264_ES ||
     d_video->demuxer->file_format == DEMUXER_TYPE_MPEG_PS ||
     d_video->demuxer->file_format == DEMUXER_TYPE_MPEG_TS)))
    use_pts_fix2=1;
else
    use_pts_fix2=0;

if(sh_video) {
    sh_video->timer=0;
    sh_video->chapter_change=0;
}
if(sh_audio) {
    sh_audio->timer=-audio_delay;
    sh_audio->chapter_change=0;
    sh_audio->a_pts=HUGE;
}

if(!sh_audio){
  MSG_INFO(MSGTR_NoSound);
  if(verbose) MSG_V("Freeing %d unused audio chunks\n",d_audio->packs);
  ds_free_packs(d_audio); // free buffered chunks
  d_audio->id=-2;         // do not read audio chunks
  if(ao_inited) uninit_player(INITED_AO); // close device
}
if(!sh_video){
   MSG_INFO("Video: no video!!!\n");
   if(verbose) MSG_V("Freeing %d unused video chunks\n",d_video->packs);
   ds_free_packs(d_video);
   d_video->id=-2;
   if(vo_inited) uninit_player(INITED_VO);
}
if(!sh_audio && !sh_video) exit_player("Nothing to do");

if(demuxer->file_format!=DEMUXER_TYPE_AVI) pts_from_bps=0; // it must be 0 for mpeg/asf!
if(force_fps && sh_video){
  vo_fps = sh_video->fps=force_fps;
  sh_video->frametime=1.0f/sh_video->fps;
  MSG_INFO(MSGTR_FPSforced,sh_video->fps,sh_video->frametime);
}
    if(vo_get_num_frames(&xp_num_frames)!=VO_TRUE)
    {
	if(enable_xp!=XP_None)
	    exit_player("Selected -vo driver doesn't support DECODING AHEAD! Try other.");
	xp_num_frames=0;
    }

    /* Init timers and benchmarking */
    rtc_fd=InitTimer();
    if(!nortc && rtc_fd>0) { close(rtc_fd); rtc_fd=-1; }
    MSG_V("Using %s timing\n",rtc_fd>0?"rtc":softsleep?"software":"usleep()");

    total_time_usage_start=GetTimer();
    audio_time_usage=0; audio_decode_time_usage=0; video_time_usage=0; vout_time_usage=0;
    audio_decode_time_usage_correction=0;

    if(benchmark) init_benchmark();

    /* display clip info */
    demux_info_print(demuxer,filename);
    if(enable_xp!=XP_None)
    {
      pinfo[xp_id].current_module="init_xp";
        if(sh_video && xp_num_frames < 5) /* we need at least 5 buffers to suppress screen judering */
	{
	    MSG_FATAL("Not enough buffers for DECODING AHEAD!\nNeed %u buffers but exist only %u\n",5,xp_num_frames);
	    exit_player("Try other '-vo' driver.\n");
	}
	if(init_dec_ahead(sh_video,sh_audio)!=0)
	{
	    enable_xp = XP_None;
	    exit_player("Can't initialize decoding ahead!\n");
	}
	if(run_dec_ahead()!=0)
	{
	    enable_xp = XP_None;
	    exit_player("Can't run decoding ahead!\n");
	}
	if(sh_video)	MSG_OK("Using DECODING AHEAD mplayer's core with %u video buffers\n",xp_num_frames);
	else 		MSG_OK("Using DECODING AHEAD mplayer's core with %u audio buffers\n",ao_da_buffs);
	decore_video_ptr = xp_decore_video;
	/* reset counters */
	dec_ahead_active_frame=0;
	abs_dec_ahead_active_frame=0;
	loc_dec_ahead_active_frame=0;
	xp_drop_frame_cnt=0;
    }


fflush(stdout);
fflush(stderr);
/*
   let thread will decode ahead!
   We may print something in block window ;)
 */
    mpxp_seek_time = GetTimerMS();
    if(enable_xp!=XP_None && sh_video)
    {
	volatile unsigned ada_blitted_frame;
	do {
	usleep(0);
	LOCK_VDEC_LOCKED();
	ada_blitted_frame = abs_dec_ahead_blitted_frame;
	UNLOCK_VDEC_LOCKED();
	}while(ada_blitted_frame < xp_num_frames/2 && !xp_eof);
    }
    if(run_xp_players()!=0) exit_player("Can't run xp players!\n");
    MSG_OK("Using the next %i threads:\n",xp_threads);
    for(i=0;i<xp_threads;i++) MSG_OK("[%i] %s (id=%u, pth_id=%lu)\n",i,pinfo[i].thread_name,pinfo[i].pid,pinfo[i].pth_id);
//==================== START PLAYING =======================

MSG_OK(MSGTR_StartPlaying);fflush(stdout);

mp_msg_flush();
while(!eof){
    int in_pause=0,need_repaint=0;
    float v_pts=0;
//    unsigned int aq_total_time=GetTimer();
    float aq_sleep_time=0;

    if(play_n_frames>=0){
      --play_n_frames;
      if(play_n_frames<0) eof = PT_NEXT_ENTRY;
    }

    if( enable_xp < XP_VAPlay )
        eof |= decore_audio(xp_id);
/*========================== UPDATE TIMERS ============================*/
    pinfo[xp_id].current_module="Update timers";
if(!sh_video) {
  if(benchmark && verbose) show_benchmark_status();
  else
  {
    /* PAINT audio OSD */
    unsigned ipts,rpts;
    unsigned char h,m,s,rh,rm,rs;
    static char ph=0,pm=0,ps=0;
	ipts=(unsigned)(sh_audio->timer-ao_get_delay());
	rpts=demuxer->movi_length-ipts;
	h = ipts/3600;
	m = (ipts/60)%60;
	s = ipts%60;
	if(demuxer->movi_length!=UINT_MAX)
	{
	    rh = rpts/3600;
	    rm = (rpts/60)%60;
	    rs = rpts%60;
	}
	else rh=rm=rs=0;
	if(h != ph || m != pm || s != ps)
	{
	    MSG_STATUS(">%02u:%02u:%02u (%02u:%02u:%02u)\r",h,m,s,rh,rm,rs);
	    ph = h;
	    pm = m;
	    ps = s;
	}
  }
  if(enable_xp >= XP_VAPlay) { usleep(100000); eof = audio_eof; }
  goto read_input;
}else{
  int l_eof;

/*========================== PLAY VIDEO ============================*/

vo_pts=sh_video->timer*90000.0;
vo_fps=sh_video->fps;

    if(need_repaint) goto repaint;
    if((sh_video->is_static ||(stream->type&STREAMTYPE_MENU)==STREAMTYPE_MENU) && our_n_frames)
    {
     /* don't decode if it's picture */
     usleep(0);
     if(vo_check_events()) goto repaint;
    }
    else
    {
	repaint:
	l_eof = (*decore_video_ptr)(rtc_fd,&vstat,&aq_sleep_time,&v_pts);
	eof |= l_eof;
	if(eof) goto do_loop;
    }
/*Output quality adjustments:*/
if(auto_quality>0){
  if(output_quality<auto_quality && aq_sleep_time>0)
      ++output_quality;
  else
  if(output_quality>1 && aq_sleep_time<0)
      --output_quality;
  else
  if(output_quality>0 && aq_sleep_time<-0.050f) // 50ms
      output_quality=0;
  set_video_quality(sh_video,output_quality);
}

read_input:

#ifdef USE_OSD
  if(osd_visible){
    if (!--osd_visible){
       vo_osd_progbar_type=-1; // disable
       vo_osd_changed(OSDTYPE_PROGBAR);
       if (!((osd_function == OSD_PAUSE)||(osd_function==OSD_DVDMENU)))
	   osd_function = OSD_PLAY;
    }
  }
#endif
 if(osd_function==OSD_DVDMENU) {
    rect_highlight_t hl;
    if(stream->driver->control(stream,SCTRL_VID_GET_HILIGHT,&hl)==SCTRL_OK) {
	osd_set_nav_box (hl.sx, hl.sy, hl.ex, hl.ey);
	MSG_V("Set nav box: %i %i %i %i\n",hl.sx, hl.sy, hl.ex, hl.ey);
	vo_osd_changed (OSDTYPE_DVDNAV);
    }
  }
  if(osd_function==OSD_PAUSE||osd_function==OSD_DVDMENU){
    mp_cmd_t* cmd;
      if (vo_inited && sh_video) {
	if(osd_level>1 && !in_pause) {
	  in_pause = 1;
	  goto repaint;
	}
	 vo_pause();
      }
      if(verbose) {
	MSG_STATUS("\n------ PAUSED -------\r");
	fflush(stdout);
      }

      if (ao_inited && sh_audio) {
         if( enable_xp >= XP_VAPlay ) {
             dec_ahead_in_pause=1;
             while( !dec_ahead_can_aseek )
                 usleep(0);
         }
         ao_pause();	// pause audio, keep data if possible
      }

      while( (cmd = mp_input_get_cmd(20,1,1)) == NULL) {
	     if(sh_video && vo_inited) vo_check_events();
             usleep(20000);
      }
      if (cmd && cmd->id == MP_CMD_PAUSE) {
          cmd = mp_input_get_cmd(0,1,0);
          mp_cmd_free(cmd);
      }
      if(osd_function==OSD_PAUSE) osd_function=OSD_PLAY;
      if (ao_inited && sh_audio) {
        ao_resume();	// resume audio
         if( enable_xp >= XP_VAPlay ) {
             dec_ahead_in_pause=0;
             LOCK_AUDIO_PLAY();
             pthread_cond_signal(&audio_play_cond);
             UNLOCK_AUDIO_PLAY();
         }
      }
      if (vo_inited && sh_video)
        vo_resume();	// resume video
      in_pause=0;
      (void)GetRelativeTime();	// keep TF around FT in next cycle
  }
} /* else if(!sh_video) */
our_n_frames++;

//================= Keyboard events, SEEKing ====================

{
  mp_cmd_t* cmd;
  while( (cmd = mp_input_get_cmd(0,0,0)) != NULL) {
    switch(cmd->id) {
    case MP_CMD_SEEK : {
      int v,abs;
      v = cmd->args[0].v.i;
      abs = (cmd->nargs > 1) ? cmd->args[1].v.i : 0;
      if(abs) {
	seek_flags = DEMUX_SEEK_SET|DEMUX_SEEK_PERCENTS;
	if(sh_video) osd_function= (v > sh_video->timer) ? OSD_FFW : OSD_REW;
	seek_secs = v/100.;
      }
      else {
	seek_flags = DEMUX_SEEK_CUR|DEMUX_SEEK_SECONDS;
	if(sh_video) osd_function= (v > 0) ? OSD_FFW : OSD_REW;
	seek_secs+= v;
      }
    } break;
    case MP_CMD_AUDIO_DELAY : {
      float v = cmd->args[0].v.f;
      audio_delay += v;
      osd_show_av_delay = osd_info_factor;
      if(sh_audio) {
          if(enable_xp>=XP_VAPlay)
              pthread_mutex_lock(&audio_timer_mutex);
          sh_audio->timer+= v;
          if(enable_xp>=XP_VAPlay)
              pthread_mutex_unlock(&audio_timer_mutex);
      }
    } break;
    case MP_CMD_SPEED_INCR :
    case MP_CMD_SPEED_MULT :
    case MP_CMD_SPEED_SET :
	MSG_WARN("Speed adjusting is not implemented yet!\n");
	break;
    case MP_CMD_SWITCH_AUDIO :
	MSG_INFO("ID_AUDIO_TRACK=%i\n",demuxer_switch_audio_r(demuxer, demuxer->audio->id+1));
	break;
    case MP_CMD_SWITCH_VIDEO :
	MSG_INFO("ID_VIDEO_TRACK=%i\n",demuxer_switch_video_r(demuxer, demuxer->video->id+1));
	break;
    case MP_CMD_SWITCH_SUB :
	MSG_INFO("ID_SUB_TRACK=%i\n",demuxer_switch_subtitle_r(demuxer, demuxer->sub->id+1));
	break;
    case MP_CMD_FRAME_STEP :
    case MP_CMD_PAUSE : {
      osd_function=OSD_PAUSE;
    } break;
    case MP_CMD_SOFT_QUIT : {
      soft_exit_player();
      break;
    }
    case MP_CMD_QUIT : {
      exit_player(MSGTR_Exit_quit);
    }
    case MP_CMD_PLAY_TREE_STEP : {
      int n = cmd->args[0].v.i > 0 ? 1 : -1;
      play_tree_iter_t* i = play_tree_iter_new_copy(playtree_iter);
      
      if(play_tree_iter_step(i,n,0) == PLAY_TREE_ITER_ENTRY)
	eof = (n > 0) ? PT_NEXT_ENTRY : PT_PREV_ENTRY;
      play_tree_iter_free(i);
    } break;
    case MP_CMD_PLAY_TREE_UP_STEP : {
      int n = cmd->args[0].v.i > 0 ? 1 : -1;
      play_tree_iter_t* i = play_tree_iter_new_copy(playtree_iter);
      if(play_tree_iter_up_step(i,n,0) == PLAY_TREE_ITER_ENTRY)
	eof = (n > 0) ? PT_UP_NEXT : PT_UP_PREV;
      play_tree_iter_free(i);
    } break;
    case MP_CMD_PLAY_ALT_SRC_STEP : {
      if(playtree_iter->num_files > 1) {
	int v = cmd->args[0].v.i;
	if(v > 0 && playtree_iter->file < playtree_iter->num_files)
	  eof = PT_NEXT_SRC;
	else if(v < 0 && playtree_iter->file > 1)
	  eof = PT_PREV_SRC;
      }
    } break;
    case MP_CMD_SUB_DELAY : {
      int abs= cmd->args[1].v.i;
      float v = cmd->args[0].v.f;
      if(abs)
	sub_delay = v;
      else
	sub_delay += v;
      osd_show_sub_delay = osd_info_factor; // show the subdelay in OSD
    } break;
    case MP_CMD_OSD : 
      if(sh_video) {
	int v = cmd->args[0].v.i;
	if(v < 0)
	  osd_level=(osd_level+1)%4;
	else
	  osd_level= v > 3 ? 3 : v;
      } break;
    case MP_CMD_MUTE:
      mixer_mute();
      break;
    case MP_CMD_VOLUME :  {
      int v = cmd->args[0].v.i;
      if(v > 0)
	mixer_incvolume();
      else
	mixer_decvolume();
#ifdef USE_OSD
      if(osd_level){
	osd_visible=sh_video->fps; // 1 sec
	vo_osd_progbar_type=OSD_VOLUME;
	vo_osd_progbar_value=(mixer_getbothvolume()*256.0)/100.0;
	vo_osd_changed(OSDTYPE_PROGBAR);
      }
#endif
    } break;
    case MP_CMD_CONTRAST :  {
      int v = cmd->args[0].v.i, abs = cmd->args[1].v.i;
      if(abs) v_cont=v;
      else    v_cont+=v;
      if(v_cont > 100) v_cont=100;
      if(v_cont < -100) v_cont = -100;
      if(set_video_colors(sh_video,VO_EC_CONTRAST,v_cont)){
#ifdef USE_OSD
	if(osd_level){
	  osd_visible=sh_video->fps; // 1 sec
	  vo_osd_progbar_type=OSD_CONTRAST;
	  vo_osd_progbar_value=((v_cont)<<8)/100;
	  vo_osd_progbar_value = ((v_cont+100)<<8)/200;
	  vo_osd_changed(OSDTYPE_PROGBAR);
	}
#endif
      }
    } break;
    case MP_CMD_BRIGHTNESS :  {
      int v = cmd->args[0].v.i, abs = cmd->args[1].v.i;
      if(abs) v_bright=v;
      else    v_bright+=v;
      if(v_bright > 100) v_bright = 100;
      if(v_bright < -100) v_bright = -100;
      if(set_video_colors(sh_video,VO_EC_BRIGHTNESS,v_bright)){
#ifdef USE_OSD
	if(osd_level){
	  osd_visible=sh_video->fps; // 1 sec
	  vo_osd_progbar_type=OSD_BRIGHTNESS;
	  vo_osd_progbar_value=((v_bright)<<8)/100;
	  vo_osd_progbar_value = ((v_bright+100)<<8)/200;
	  vo_osd_changed(OSDTYPE_PROGBAR);
	}
#endif
      }      
    } break;
    case MP_CMD_HUE :  {
      int v = cmd->args[0].v.i, abs = cmd->args[1].v.i;
      if(abs) v_hue=v;
      else    v_hue+=v;
      if(v_hue > 100) v_hue = 100;
      if(v_hue < -100) v_hue = -100;
      if(set_video_colors(sh_video,VO_EC_HUE,v_hue)){
#ifdef USE_OSD
	if(osd_level){
	  osd_visible=sh_video->fps; // 1 sec
	  vo_osd_progbar_type=OSD_HUE;
	  vo_osd_progbar_value=((v_hue)<<8)/100;
	  vo_osd_progbar_value = ((v_hue+100)<<8)/200;
	  vo_osd_changed(OSDTYPE_PROGBAR);
	}
#endif
      }	
    } break;
    case MP_CMD_SATURATION :  {
      int v = cmd->args[0].v.i, abs = cmd->args[1].v.i;
      if(abs) v_saturation=v;
      else    v_saturation+=v;
      if(v_saturation > 100) v_saturation = 100;
      if(v_saturation < -100) v_saturation = -100;
      if(set_video_colors(sh_video,VO_EC_SATURATION,v_saturation)){
#ifdef USE_OSD
	if(osd_level){
	  osd_visible=sh_video->fps; // 1 sec
	  vo_osd_progbar_type=OSD_SATURATION;
	  vo_osd_progbar_value=((v_saturation)<<8)/100;
	  vo_osd_progbar_value = ((v_saturation+100)<<8)/200;
	  vo_osd_changed(OSDTYPE_PROGBAR);
	}
#endif
      }
    } break;
    case MP_CMD_FRAMEDROPPING :  {
      int v = cmd->args[0].v.i;
      if(v < 0)
	frame_dropping = (frame_dropping+1)%3;
      else
	frame_dropping = v > 2 ? 2 : v;
      osd_show_framedrop = osd_info_factor;
    } break;
    case MP_CMD_TV_STEP_CHANNEL:
	if(cmd->args[0].v.i > 0) cmd->id=MP_CMD_TV_STEP_CHANNEL_UP;
	else	  cmd->id=MP_CMD_TV_STEP_CHANNEL_DOWN;
    case MP_CMD_TV_STEP_NORM:
    case MP_CMD_TV_STEP_CHANNEL_LIST:
	stream->driver->control(stream,SCRTL_MPXP_CMD,cmd->id);
	break;
    case MP_CMD_DVDNAV:
      if(stream->driver->control(stream,SCRTL_MPXP_CMD,cmd->args[0].v.i)==SCTRL_OK)
      {
	if(cmd->args[0].v.i!=MP_CMD_DVDNAV_SELECT)
	{
//		seek_flags = DEMUX_SEEK_SET|DEMUX_SEEK_PERCENTS;
//		seek_secs = 0.;
		stream->type|=STREAMTYPE_MENU;
		need_repaint=1;
	}
	osd_function=OSD_DVDMENU;
	if(cmd->args[0].v.i==MP_CMD_DVDNAV_SELECT)
	{
		osd_function=NULL;
		need_repaint=1;
		after_dvdmenu=1;
		goto goto_next_file; /* menu may have different size against of movie */
	}
      }
      break;
    case MP_CMD_VO_FULLSCREEN:
	vo_fullscreen();
	break;
    case MP_CMD_VO_SCREENSHOT:
	vo_screenshot();
	break;
    case MP_CMD_SUB_POS:
    {
        int v;
	v = cmd->args[0].v.i;
    
	sub_pos+=v;
	if(sub_pos >100) sub_pos=100;
	if(sub_pos <0) sub_pos=0;
	vo_osd_changed(OSDTYPE_SUBTITLE);
    }	break;
    default : {
      MSG_ERR("Received unknow cmd %s\n",cmd->name);
    }
    }
    mp_cmd_free(cmd);
  }
}

  if (seek_to_sec) {
    int a,b; float d;
    
    if (sscanf(seek_to_sec, "%d:%d:%f", &a,&b,&d)==3)
	seek_secs += 3600*a +60*b +d ;
    else if (sscanf(seek_to_sec, "%d:%f", &a, &d)==2)
	seek_secs += 60*a +d;
    else if (sscanf(seek_to_sec, "%f", &d)==1)
	seek_secs += d;

     seek_to_sec = NULL;
  }
do_loop:
  /* Looping. */
  if(eof && loop_times>=0) {

    MSG_V("loop_times = %d, eof = %d\n", loop_times,eof);

    if(loop_times>1) loop_times--; else
    if(loop_times==1) loop_times=-1;

    eof=0;
    audio_eof=0;
    seek_flags=DEMUX_SEEK_SET|DEMUX_SEEK_PERCENTS;
    seek_secs=0; // seek to start of movie (0%)

  }

if(seek_secs || (seek_flags&DEMUX_SEEK_SET)){
  pinfo[xp_id].current_module="seek";

  if(enable_xp!=XP_None)
  {
      dec_ahead_halt_threads(0);
      LOCK_VREADING();
  }

  if(seek_secs) {
      if(enable_xp>XP_None && sh_video)
	seek_secs -= (xp_is_bad_pts?shva[dec_ahead_locked_frame>0?dec_ahead_locked_frame-1:xp_num_frames-1].v_pts:d_video->pts)-shva[dec_ahead_active_frame].v_pts;
  }

  mpxp_seek(xp_id,&vstat,&osd_visible,v_pts,seek_secs,seek_flags);

  audio_eof=0;
  seek_secs=0;
  seek_flags=DEMUX_SEEK_CUR|DEMUX_SEEK_SECONDS;

  if(enable_xp!=XP_None) 
  {
      UNLOCK_VREADING();
      dec_ahead_restart_threads(xp_id);
  }
  /* Disable threads for DVD menus */

  pinfo[xp_id].current_module=NULL;
}
#ifdef USE_OSD
    if(!enable_xp!=XP_None) update_osd(d_video->pts);
#endif
} // while(!eof)

MSG_V("EOF code: %d\n",eof);

goto_next_file:  // don't jump here after ao/vo/getch initialization!

if(benchmark) show_benchmark(&vstat);

if(playtree_iter != NULL && !after_dvdmenu)
{
if(eof == PT_NEXT_ENTRY || eof == PT_PREV_ENTRY) {
  eof = eof == PT_NEXT_ENTRY ? 1 : -1;
  if(play_tree_iter_step(playtree_iter,eof,0) == PLAY_TREE_ITER_ENTRY) {
    uninit_player(INITED_ALL-(INITED_LIRC+INITED_INPUT+INITED_VO));
    eof = 1;
  } else {
    play_tree_iter_free(playtree_iter);
    playtree_iter = NULL;
  }
} else if (eof == PT_UP_NEXT || eof == PT_UP_PREV) {
  eof = eof == PT_UP_NEXT ? 1 : -1;
  if(play_tree_iter_up_step(playtree_iter,eof,0) == PLAY_TREE_ITER_ENTRY) {
    uninit_player(INITED_ALL-(INITED_LIRC+INITED_INPUT+INITED_VO));
    eof = 1;
  } else {
    play_tree_iter_free(playtree_iter);
    playtree_iter = NULL;
  }
}else { // NEXT PREV SRC
     uninit_player(INITED_ALL-(INITED_LIRC+INITED_INPUT+INITED_VO+INITED_DEMUXER));
     eof = eof == PT_PREV_SRC ? -1 : 1;
}
}
uninit_player(INITED_VO);

if(eof == 0) eof = 1;

if(!after_dvdmenu)
while(playtree_iter != NULL) {
  filename = play_tree_iter_get_file(playtree_iter,eof);
  if(filename == NULL) {
    if( play_tree_iter_step(playtree_iter,eof,0) != PLAY_TREE_ITER_ENTRY) {
      play_tree_iter_free(playtree_iter);
      playtree_iter = NULL;
    };
  } else
    break;
} 

if( playtree_iter != NULL ){
    int flg;
    flg=INITED_ALL;
    if(after_dvdmenu) flg &=~(INITED_STREAM|INITED_DEMUXER);
    uninit_player(flg&(~INITED_INPUT)); /* TODO: |(~INITED_AO)|(~INITED_VO) */
    vo_inited=0;
    ao_inited=0;
    eof = 0;
    audio_eof=0;
    goto play_next_file;

}

if(stream_dump_type>1) dump_mux_close(demuxer);
exit_player(MSGTR_Exit_eof);

return 1;
}
