#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
#include <algorithm>

#include <errno.h>
#include <stdio.h>
#include <math.h>
#include <sys/time.h>

#include "mplayerxp.h"
#include "player_msg.h"
#include "sig_hand.h"

#include "libmpcodecs/dec_audio.h"

#include "osdep/timer.h"
#include "xmp_core.h"
#include "xmp_adecoder.h"

/* Audio stuff */
volatile float dec_ahead_audio_delay;
namespace mpxp {
#ifdef ENABLE_DEC_AHEAD_DEBUG
#define MSG_T(args...) mp_msg(MSGT_GLOBAL, MSGL_DBG2,__FILE__,__LINE__, ## args )
#else
#define MSG_T(args...)
#endif

extern int get_free_audio_buffer(void);

/************************************************************************
    AUDIO XP-CORE! ToDo: rewrite it in packet-mode
************************************************************************/
/* XP audio buffer */
pthread_mutex_t audio_timer_mutex=PTHREAD_MUTEX_INITIALIZER;
typedef struct audio_buffer_index_s {
    float pts;
    int index;
} audio_buffer_index_t;

typedef struct audio_buffer_s {
    unsigned char* buffer;
    int head;
    int tail;
    unsigned len;
    unsigned size;
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

int init_audio_buffer( int size, int min_reserv, int indices, sh_audio_t *sha )
{
    MSG_V("Using audio buffer %i bytes (min reserve = %i, indices %i)\n",size,min_reserv, indices);
    if( !(audio_buffer.buffer = new unsigned char[size]) )
	return ENOMEM;
    if( !(audio_buffer.indices = new audio_buffer_index_t[indices])) {
	delete audio_buffer.buffer;
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
    audio_buffer.sh_audio = sha;
    return 0;
}

void uninit_audio_buffer(void)
{
    audio_buffer.eof = 1;

    if( audio_buffer.blocked_readers > 0 ) {     /* Make blocked reader exit */
	int loops = 10;
	pthread_cond_broadcast( &audio_buffer.wait_buffer_cond );
	while( audio_buffer.blocked_readers > 0 && loops > 0 ) {
	    yield_timeslice();
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
	delete audio_buffer.buffer ;
    audio_buffer.buffer = NULL;

    if( audio_buffer.indices )
	delete audio_buffer.indices ;
    audio_buffer.indices = NULL;
    /* audio_buffer.sh_audio = ?; */
}

int read_audio_buffer( sh_audio_t *audio, unsigned char *buffer, unsigned minlen, unsigned maxlen, float *pts )
{
    unsigned len = 0;
    int l = 0;
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

	l = std::min( (int)(maxlen - len), audio_buffer.head - audio_buffer.tail );
	if(l<0) {
	    l = std::min( maxlen - len, audio_buffer.len - audio_buffer.tail );
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
	    audio_buffer.indices[audio_buffer.index_tail].pts += (float)((audio_buffer.tail - audio_buffer.indices[audio_buffer.index_tail].index + buff_len) % buff_len) / (float)audio_buffer.sh_audio->af_bps;
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
    return (float)delay / (float)audio_buffer.sh_audio->af_bps;
}

int decode_audio_buffer(Demuxer_Stream *d_audio,unsigned len)
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
	len = std::min(len, unsigned(audio_buffer.tail - audio_buffer.head - audio_buffer.min_reserv));
	if( len < audio_buffer.sh_audio->audio_out_minsize ) {
	    pthread_mutex_unlock( &audio_buffer.head_mutex );
	    return 0;
	}
    }

    blen = audio_buffer.size - audio_buffer.head;
    if( (l = (blen - audio_buffer.min_reserv)) < len ) {
	len = std::max(unsigned(l),audio_buffer.sh_audio->audio_out_minsize);
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
	ret = mpca_decode(mpxp_context().audio().decoder, &audio_buffer.buffer[audio_buffer.head], audio_buffer.min_len, l2,blen,&pts);
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
	MSG_V("audio eof\n");
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
    mpxp_context().bench->audio_decode+=t*0.000001f;
    mpxp_context().bench->audio_decode-=mpxp_context().bench->audio_decode_correction;
    if(mp_conf.benchmark)
    {
	if(t > mpxp_context().bench->max_audio_decode) mpxp_context().bench->max_audio_decode = t;
	if(t < mpxp_context().bench->min_audio_decode) mpxp_context().bench->min_audio_decode = t;
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

int xp_thread_decode_audio(Demuxer_Stream *d_audio)
{
    sh_audio_t* sh_audio=static_cast<sh_audio_t*>(mpxp_context().engine().xp_core->audio->sh);
    sh_video_t* sh_video=NULL;
    if(mpxp_context().engine().xp_core->video) sh_video=static_cast<sh_video_t*>(mpxp_context().engine().xp_core->video->sh);
    int free_buf, vbuf_size, pref_buf;
    unsigned len=0;

    free_buf = get_free_audio_buffer();

    if( free_buf == -1 ) { /* End of file */
	mpxp_context().engine().xp_core->audio->eof = 1;
	return 0;
    }
    if( free_buf < (int)sh_audio->audio_out_minsize ) /* full */
	return 0;

    len = get_len_audio_buffer();

    if( len < MAX_OUTBURST ) /* Buffer underrun */
	return decode_audio_buffer(d_audio,MAX_OUTBURST);

    if(mpxp_context().engine().xp_core->video) {
	/* Match video buffer */
	vbuf_size = dae_get_decoder_outrun(mpxp_context().engine().xp_core->video);
	pref_buf = vbuf_size / sh_video->fps * sh_audio->af_bps;
	pref_buf -= len;
	if( pref_buf > 0 ) {
	    len = std::min( pref_buf, free_buf );
	    if( len > sh_audio->audio_out_minsize ) {
		return decode_audio_buffer(d_audio,len);
	    }
	}
    } else
	return decode_audio_buffer(d_audio,std::min(free_buf,MAX_OUTBURST));

    return 0;
}

static volatile int dec_ahead_can_adseek=1;  /* It is safe to seek audio buffer thread */
/* this routine decodes audio only */
any_t* a_dec_ahead_routine( any_t* arg )
{
    mpxp_thread_t* priv=reinterpret_cast<mpxp_thread_t*>(arg);
    sh_audio_t* sh_audio=static_cast<sh_audio_t*>(priv->dae->sh);
    Demuxer_Stream *d_audio=sh_audio->ds;

    int ret;
    struct timeval now;
    struct timespec timeout;
    float d;

    priv->state=Pth_Run;
    if(mpxp_context().engine().xp_core->video) mpxp_context().engine().xp_core->video->eof=0;
    mpxp_context().engine().xp_core->audio->eof=0;
    MSG_T("\nDEC_AHEAD: entering...\n");
    priv->pid = getpid();
    __MP_UNIT(priv->p_idx,"dec_ahead");

    dec_ahead_can_adseek=0;
    while(priv->state!=Pth_Canceling) {
	if(priv->state==Pth_Sleep) {
	    priv->state=Pth_ASleep;
	    while(priv->state==Pth_ASleep) yield_timeslice();
	    continue;
	}
	__MP_UNIT(priv->p_idx,"decode audio");
	while((ret = xp_thread_decode_audio(d_audio)) == 2) {/* Almost empty buffer */
	    if(mpxp_context().engine().xp_core->audio->eof) break;
	}
	dec_ahead_can_adseek=1;

	if(priv->state==Pth_Canceling) break;

	__MP_UNIT(priv->p_idx,"sleep");
	LOCK_AUDIO_DECODE();
	if(priv->state!=Pth_Canceling) {
	    if(mpxp_context().engine().xp_core->audio->eof) {
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
		    if(mpxp_context().engine().xp_core->in_pause)
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
		yield_timeslice();
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

void sig_audio_decode( void )
{
    MSG_T("sig_audio_decode\n");
    mpxp_print_flush();

    dec_ahead_can_adseek=1;

    UNLOCK_AUDIO_DECODE();

    xmp_killall_threads(pthread_self());
    __exit_sighandler();
}

} // namespace mpxp
