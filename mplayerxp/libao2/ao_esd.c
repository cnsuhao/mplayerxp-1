/*
 * EsounD (Enlightened Sound Daemon) audio output driver for MPlayerXP
 *
 * copyright (c) 2002 Juergen Keil <jk@tools.de>
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

 /*
 * TODO / known problems:
 * - does not work well when the esd daemon has autostandby disabled
 *   (workaround: run esd with option "-as 2" - fortunatelly this is
 *    the default)
 * - plays noise on a linux 2.4.4 kernel with a SB16PCI card, when using
 *   a local tcp connection to the esd daemon; there is no noise when using
 *   a unix domain socket connection.
 *   (there are EIO errors reported by the sound card driver, so this is
 *   most likely a linux sound card driver problem)
 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#ifdef	__svr4__
#include <stropts.h>
#endif
#include <esd.h>

#include "mp_config.h"
#include "audio_out.h"
#include "audio_out_internal.h"
#include "postproc/af_format.h"
#include "afmt.h"
#include "ao_msg.h"


#define ESD_RESAMPLES 0
#define ESD_DEBUG 0

#if	ESD_DEBUG
#define	dprintf(...)	printf(__VA_ARGS__)
#else
#define	dprintf(...)	/**/
#endif


#define	ESD_CLIENT_NAME	"MPlayerXP"
#define	ESD_MAX_DELAY	(1.0f)	/* max amount of data buffered in esd (#sec) */

static const ao_info_t info =
{
    "EsounD audio output",
    "esd",
    "Juergen Keil <jk@tools.de>",
    ""
};

LIBAO_EXTERN(esd)

typedef struct priv_s {
    int			fd;
    int			play_fd;
    esd_server_info_t*	svinfo;
    int			latency;
    int			bytes_per_sample;
    unsigned long	samples_written;
    struct timeval	play_start;
    float		audio_delay;
}priv_t;

/*
 * to set/get/query special features/parameters
 */
static int control(ao_data_t* ao,int cmd, long arg)
{
    priv_t*priv=ao->priv;
    esd_player_info_t *esd_pi;
    esd_info_t        *esd_i;
    time_t	       now;
    static time_t      vol_cache_time;
    static ao_control_vol_t vol_cache;

    switch (cmd) {
    case AOCONTROL_GET_VOLUME:
	time(&now);
	if (now == vol_cache_time) {
	    *(ao_control_vol_t *)arg = vol_cache;
	    return CONTROL_OK;
	}

	dprintf("esd: get vol\n");
	if ((esd_i = esd_get_all_info(priv->fd)) == NULL)
	    return CONTROL_ERROR;

	for (esd_pi = esd_i->player_list; esd_pi != NULL; esd_pi = esd_pi->next)
	    if (strcmp(esd_pi->name, ESD_CLIENT_NAME) == 0)
		break;

	if (esd_pi != NULL) {
	    ao_control_vol_t *vol = (ao_control_vol_t *)arg;
	    vol->left =  esd_pi->left_vol_scale  * 100 / ESD_VOLUME_BASE;
	    vol->right = esd_pi->right_vol_scale * 100 / ESD_VOLUME_BASE;

	    vol_cache = *vol;
	    vol_cache_time = now;
	}
	esd_free_all_info(esd_i);

	return CONTROL_OK;

    case AOCONTROL_SET_VOLUME:
	dprintf("esd: set vol\n");
	if ((esd_i = esd_get_all_info(priv->fd)) == NULL)
	    return CONTROL_ERROR;

	for (esd_pi = esd_i->player_list; esd_pi != NULL; esd_pi = esd_pi->next)
	    if (strcmp(esd_pi->name, ESD_CLIENT_NAME) == 0)
		break;

	if (esd_pi != NULL) {
	    ao_control_vol_t *vol = (ao_control_vol_t *)arg;
	    esd_set_stream_pan(priv->fd, esd_pi->source_id,
			       vol->left  * ESD_VOLUME_BASE / 100,
			       vol->right * ESD_VOLUME_BASE / 100);

	    vol_cache = *vol;
	    time(&vol_cache_time);
	}
	esd_free_all_info(esd_i);
	return CONTROL_OK;

    default:
	return CONTROL_UNKNOWN;
    }
}


/*
 * open & setup audio device
 * return: 1=success 0=fail
 */
static int init(ao_data_t* ao,unsigned flags)
{
    ao->priv=malloc(sizeof(priv_t));
    priv_t*priv=ao->priv;
    memset(priv,0,sizeof(priv_t));
    priv->fd=priv->play_fd=-1;
    char *server = ao->subdevice;  /* NULL for localhost */
    UNUSED(flags);
    if (priv->fd < 0) {
	priv->fd = esd_open_sound(server);
	if (priv->fd < 0) {
	    MSG_ERR("ESD: Can't open sound: %s\n", strerror(errno));
	    return 0;
	}
    }
    return 1;
}

static int configure(ao_data_t* ao,unsigned rate_hz,unsigned channels,unsigned format)
{
    priv_t*priv=ao->priv;
    char *server = ao->subdevice;  /* NULL for localhost */
    esd_format_t esd_fmt;
    int bytes_per_sample;
    int fl;
    float lag_seconds, lag_net, lag_serv;
    struct timeval proto_start, proto_end;
	/* get server info, and measure network latency */
	gettimeofday(&proto_start, NULL);
	priv->svinfo = esd_get_server_info(priv->fd);
	if(server) {
	    gettimeofday(&proto_end, NULL);
	    lag_net  = (proto_end.tv_sec  - proto_start.tv_sec) +
		(proto_end.tv_usec - proto_start.tv_usec) / 1000000.0;
	    lag_net /= 2.0; /* round trip -> one way */
	} else
	    lag_net = 0.0;  /* no network lag */

	/*
	if (priv->svinfo) {
	    mp_msg(MSGT_AO, MSGL_INFO, "AO: [esd] server info:\n");
	    esd_print_server_info(priv->svinfo);
	}
	*/
    esd_fmt = ESD_STREAM | ESD_PLAY;

#if	ESD_RESAMPLES
    /* let the esd daemon convert sample rate */
#else
    /* let mplayer's audio filter convert the sample rate */
    if (priv->svinfo != NULL)
	rate_hz = priv->svinfo->rate;
#endif
    ao->samplerate = rate_hz;

    /* EsounD can play mono or stereo */
    switch (channels) {
    case 1:
	esd_fmt |= ESD_MONO;
	ao->channels = bytes_per_sample = 1;
	break;
    default:
	esd_fmt |= ESD_STEREO;
	ao->channels = bytes_per_sample = 2;
	break;
    }

    /* EsounD can play 8bit unsigned and 16bit signed native */
    switch (format) {
    case AFMT_S8:
    case AFMT_U8:
	esd_fmt |= ESD_BITS8;
	ao->format = AFMT_U8;
	break;
    default:
	esd_fmt |= ESD_BITS16;
	ao->format = AFMT_S16_NE;
	bytes_per_sample *= 2;
	break;
    }

    /* modify priv->audio_delay depending on priv->latency
     * latency is number of samples @ 44.1khz stereo 16 bit
     * adjust according to rate_hz & bytes_per_sample
     */
#ifdef CONFIG_ESD_LATENCY
    priv->latency = esd_get_latency(priv->fd);
#else
    priv->latency = ((channels == 1 ? 2 : 1) * ESD_DEFAULT_RATE *
		   (ESD_BUF_SIZE + 64 * (4.0f / bytes_per_sample))
		   ) / rate_hz;
    priv->latency += ESD_BUF_SIZE * 2;
#endif
    if(priv->latency > 0) {
	lag_serv = (priv->latency * 4.0f) / (bytes_per_sample * rate_hz);
	lag_seconds = lag_net + lag_serv;
	priv->audio_delay += lag_seconds;
	MSG_INFO("ESD: LatencyInfo: %f %f %f\n",lag_serv, lag_net, lag_seconds);
    }

    priv->play_fd = esd_play_stream_fallback(esd_fmt, rate_hz,
					   server, ESD_CLIENT_NAME);
    if (priv->play_fd < 0) {
	MSG_ERR("ESD: Can't open play stream: %s\n", strerror(errno));
	return 0;
    }

    /* enable non-blocking i/o on the socket connection to the esd server */
    if ((fl = fcntl(priv->play_fd, F_GETFL)) >= 0)
	fcntl(priv->play_fd, F_SETFL, O_NDELAY|fl);

#if ESD_DEBUG
    {
	int sbuf, rbuf, len;
	len = sizeof(sbuf);
	getsockopt(priv->play_fd, SOL_SOCKET, SO_SNDBUF, &sbuf, &len);
	len = sizeof(rbuf);
	getsockopt(priv->play_fd, SOL_SOCKET, SO_RCVBUF, &rbuf, &len);
	dprintf("esd: send/receive socket buffer space %d/%d bytes\n",
		sbuf, rbuf);
    }
#endif

    ao->bps = bytes_per_sample * rate_hz;
    ao->outburst = ao->bps > 100000 ? 4*ESD_BUF_SIZE : 2*ESD_BUF_SIZE;

    priv->play_start.tv_sec = 0;
    priv->samples_written = 0;
    priv->bytes_per_sample = bytes_per_sample;

    return 1;
}


/*
 * close audio device
 */
static void uninit(ao_data_t* ao)
{
    priv_t*priv=ao->priv;
    if (priv->play_fd >= 0) {
	esd_close(priv->play_fd);
	priv->play_fd = -1;
    }

    if (priv->svinfo) {
	esd_free_server_info(priv->svinfo);
	priv->svinfo = NULL;
    }

    if (priv->fd >= 0) {
	esd_close(priv->fd);
	priv->fd = -1;
    }
    free(priv);
}


/*
 * plays 'len' bytes of 'data'
 * it should round it down to outburst*n
 * return: number of bytes played
 */
static unsigned play(ao_data_t* ao,any_t* data, unsigned len, unsigned flags)
{
    priv_t*priv=ao->priv;
    unsigned offs;
    unsigned nwritten;
    int nsamples;
    int n;
    UNUSED(flags);
    /* round down buffersize to a multiple of ESD_BUF_SIZE bytes */
    len = len / ESD_BUF_SIZE * ESD_BUF_SIZE;

#define	SINGLE_WRITE 0
#if	SINGLE_WRITE
    nwritten = write(priv->play_fd, data, len);
#else
    for (offs = 0, nwritten=0; offs + ESD_BUF_SIZE <= len; offs += ESD_BUF_SIZE) {
	/*
	 * note: we're writing to a non-blocking socket here.
	 * A partial write means, that the socket buffer is full.
	 */
	n = write(priv->play_fd, (char*)data + offs, ESD_BUF_SIZE);
	if ( n < 0 ) {
	    if ( errno != EAGAIN ) {
		dprintf("esd play: write failed: %s\n", strerror(errno));
	    }
	    break;
	} else if ( n != ESD_BUF_SIZE ) {
	    nwritten += n;
	    break;
	} else
	    nwritten += n;
    }
#endif

    if (nwritten > 0) {
	if (!priv->play_start.tv_sec)
	    gettimeofday(&priv->play_start, NULL);
	nsamples = nwritten / priv->bytes_per_sample;
	priv->samples_written += nsamples;

	dprintf("esd play: %d %lu\n", nsamples, priv->samples_written);
    } else {
	dprintf("esd play: blocked / %lu\n", priv->samples_written);
    }

    return nwritten;
}


/*
 * stop playing, keep buffers (for pause)
 */
static void audio_pause(ao_data_t* ao)
{
    /*
     * not possible with priv->  the esd daemom will continue playing
     * buffered data (not more than ESD_MAX_DELAY seconds of samples)
     */
    UNUSED(ao);
}


/*
 * resume playing, after audio_pause()
 */
static void audio_resume(ao_data_t* ao)
{
    priv_t*priv=ao->priv;
    /*
     * not possible with priv->
     *
     * Let's hope the pause was long enough that the esd ran out of
     * buffered data;  we restart our time based delay computation
     * for an audio resume.
     */
    priv->play_start.tv_sec = 0;
    priv->samples_written = 0;
}


/*
 * stop playing and empty buffers (for seeking/pause)
 */
static void reset(ao_data_t* ao)
{
#ifdef	__svr4__
    priv_t*priv=ao->priv;
    /* throw away data buffered in the esd connection */
    if (ioctl(priv->play_fd, I_FLUSH, FLUSHW))
	perror("I_FLUSH");
#else
    UNUSED(ao);
#endif
}


/*
 * return: how many bytes can be played without blocking
 */
static unsigned get_space(ao_data_t* ao)
{
    priv_t*priv=ao->priv;
    struct timeval tmout;
    fd_set wfds;
    float current_delay;
    unsigned space;

    /*
     * Don't buffer too much data in the esd daemon.
     *
     * If we send too much, esd will block in write()s to the sound
     * device, and the consequence is a huge slow down for things like
     * esd_get_all_info().
     */
    if ((current_delay = get_delay(ao)) >= ESD_MAX_DELAY) {
	dprintf("esd get_space: too much data buffered\n");
	return 0;
    }

    FD_ZERO(&wfds);
    FD_SET(priv->play_fd, &wfds);
    tmout.tv_sec = 0;
    tmout.tv_usec = 0;

    if (select(priv->play_fd + 1, NULL, &wfds, NULL, &tmout) != 1)
	return 0;

    if (!FD_ISSET(priv->play_fd, &wfds))
	return 0;

    /* try to fill 50% of the remaining "free" buffer space */
    space = (ESD_MAX_DELAY - current_delay) * ao->bps * 0.5f;

    /* round up to next multiple of ESD_BUF_SIZE */
    space = (space + ESD_BUF_SIZE-1) / ESD_BUF_SIZE * ESD_BUF_SIZE;

    dprintf("esd get_space: %d\n", space);
    return space;
}


/*
 * return: delay in seconds between first and last sample in buffer
 */
static float get_delay(ao_data_t* ao)
{
    priv_t*priv=ao->priv;
    struct timeval now;
    double buffered_samples_time;
    double play_time;

    if (!priv->play_start.tv_sec)
	return 0;

    buffered_samples_time = (float)priv->samples_written / ao->samplerate;
    gettimeofday(&now, NULL);
    play_time  =  now.tv_sec  - priv->play_start.tv_sec;
    play_time += (now.tv_usec - priv->play_start.tv_usec) / 1000000.;

    /* dprintf("esd delay: %f %f\n", play_time, buffered_samples_time); */

    if (play_time > buffered_samples_time) {
	dprintf("esd: underflow\n");
	priv->play_start.tv_sec = 0;
	priv->samples_written = 0;
	return 0;
    }

    dprintf("esd: get_delay %f\n", buffered_samples_time - play_time);
    return buffered_samples_time - play_time;
}
