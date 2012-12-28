#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
/*
 * EsounD (Enlightened Sound Daemon) audio output driver for MPlayerXP
 *
 * copyright (c) 2002 Juergen Keil <jk@tools.de>
 *
 * This file is part of MPlayer.
 *
 * MPlayer is mp_free software; you can redistribute it and/or modify
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

#include "audio_out.h"
#include "audio_out_internal.h"
#include "postproc/af.h"
#include "afmt.h"
#include "ao_msg.h"


namespace mpxp {
static const int ESD_RESAMPLES=0;
#define ESD_DEBUG 0

#if	ESD_DEBUG
#define	dprintf(...)	printf(__VA_ARGS__)
#else
#define	dprintf(...)	/**/
#endif

static const char* ESD_CLIENT_NAME="MPlayerXP";
static const float ESD_MAX_DELAY=1.0f;	/* max amount of data buffered in esd (#sec) */

class Esd_AO_Interface : public AO_Interface {
    public:
	Esd_AO_Interface(const std::string& subdevice);
	virtual ~Esd_AO_Interface();

	virtual MPXP_Rc		open(unsigned flags);
	virtual MPXP_Rc		configure(unsigned rate,unsigned channels,unsigned format);
	virtual unsigned	samplerate() const;
	virtual unsigned	channels() const;
	virtual unsigned	format() const;
	virtual unsigned	buffersize() const;
	virtual unsigned	outburst() const;
	virtual MPXP_Rc		test_rate(unsigned r) const;
	virtual MPXP_Rc		test_channels(unsigned c) const;
	virtual MPXP_Rc		test_format(unsigned f) const;
	virtual void		reset();
	virtual unsigned	get_space();
	virtual float		get_delay();
	virtual unsigned	play(const any_t* data,unsigned len,unsigned flags);
	virtual void		pause();
	virtual void		resume();
	virtual MPXP_Rc		ctrl(int cmd,long arg) const;
    private:
	unsigned	_channels,_samplerate,_format;
	unsigned	_buffersize,_outburst;
	unsigned	bps() const { return _channels*_samplerate*afmt2bps(_format); }

	int		fd;
	int		play_fd;
	esd_server_info_t*svinfo;
	int		latency;
	int		bytes_per_sample;
	unsigned long	samples_written;
	struct timeval	play_start;
	float		audio_delay;
};

Esd_AO_Interface::Esd_AO_Interface(const std::string& _subdevice)
		:AO_Interface(_subdevice) {}
Esd_AO_Interface::~Esd_AO_Interface() {
    if (play_fd >= 0) {
	esd_close(play_fd);
	play_fd = -1;
    }

    if (svinfo) {
	esd_free_server_info(svinfo);
	svinfo = NULL;
    }

    if (fd >= 0) {
	esd_close(fd);
	fd = -1;
    }
}
/*
 * to set/get/query special features/parameters
 */
MPXP_Rc Esd_AO_Interface::ctrl(int cmd, long arg) const {
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
	    return MPXP_Ok;
	}

	dprintf("esd: get vol\n");
	if ((esd_i = esd_get_all_info(fd)) == NULL)
	    return MPXP_Error;

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

	return MPXP_Ok;

    case AOCONTROL_SET_VOLUME:
	dprintf("esd: set vol\n");
	if ((esd_i = esd_get_all_info(fd)) == NULL)
	    return MPXP_Error;

	for (esd_pi = esd_i->player_list; esd_pi != NULL; esd_pi = esd_pi->next)
	    if (strcmp(esd_pi->name, ESD_CLIENT_NAME) == 0)
		break;

	if (esd_pi != NULL) {
	    ao_control_vol_t *vol = (ao_control_vol_t *)arg;
	    esd_set_stream_pan(fd, esd_pi->source_id,
			       vol->left  * ESD_VOLUME_BASE / 100,
			       vol->right * ESD_VOLUME_BASE / 100);

	    vol_cache = *vol;
	    time(&vol_cache_time);
	}
	esd_free_all_info(esd_i);
	return MPXP_Ok;

    default:
	return MPXP_Unknown;
    }
}


/*
 * open & setup audio device
 * return: 1=success 0=fail
 */
MPXP_Rc Esd_AO_Interface::open(unsigned flags) {
    fd=play_fd=-1;
    UNUSED(flags);
    if (fd < 0) {
	fd = esd_open_sound(subdevice.c_str());
	if (fd < 0) {
	    mpxp_err<<"ESD: Can't open sound: "<<strerror(errno)<<std::endl;
	    return MPXP_False;
	}
    }
    return MPXP_Ok;
}

MPXP_Rc Esd_AO_Interface::configure(unsigned r,unsigned c,unsigned f)
{
    std::string server = subdevice;  /* NULL for localhost */
    esd_format_t esd_fmt;
    int _bytes_per_sample;
    int fl;
    float lag_seconds, lag_net, lag_serv;
    struct timeval proto_start, proto_end;

    /* get server info, and measure network latency */
    gettimeofday(&proto_start, NULL);
    svinfo = esd_get_server_info(fd);
    if(!server.empty()) {
	gettimeofday(&proto_end, NULL);
	lag_net  = (proto_end.tv_sec  - proto_start.tv_sec) +
	    (proto_end.tv_usec - proto_start.tv_usec) / 1000000.0;
	lag_net /= 2.0; /* round trip -> one way */
    } else
	lag_net = 0.0;  /* no network lag */
    esd_fmt = ESD_STREAM | ESD_PLAY;

#if	ESD_RESAMPLES
    /* let the esd daemon convert sample rate */
#else
    /* let mplayer's audio filter convert the sample rate */
    if (svinfo != NULL)
	r = svinfo->rate;
#endif
    _samplerate = r;

    /* EsounDscan play mono or stereo */
    switch (c) {
	case 1:
	    esd_fmt |= ESD_MONO;
	    _channels = _bytes_per_sample = 1;
	    break;
	default:
	    esd_fmt |= ESD_STEREO;
	    _channels = _bytes_per_sample = 2;
	break;
    }

    /* EsounD can play 8bit unsigned and 16bit signed native */
    switch (f) {
	case AFMT_S8:
	case AFMT_U8:
	    esd_fmt |= ESD_BITS8;
	    _format = AFMT_U8;
	    break;
	default:
	    esd_fmt |= ESD_BITS16;
	    _format = AFMT_S16_NE;
	    _bytes_per_sample *= 2;
	    break;
    }

    /* modify priv->audio_delay depending on priv->latency
     * latency is number of samples @ 44.1khz stereo 16 bit
     * adjust according to rate_hz & bytes_per_sample
     */
#ifdef CONFIG_ESD_LATENCY
    latency = esd_get_latency(fd);
#else
    latency = ((_channels == 1 ? 2 : 1) * ESD_DEFAULT_RATE *
		   (ESD_BUF_SIZE + 64 * (4.0f / _bytes_per_sample))
		   ) / _samplerate;
    latency += ESD_BUF_SIZE * 2;
#endif
    if(latency > 0) {
	lag_serv = (latency * 4.0f) / (_bytes_per_sample * _samplerate);
	lag_seconds = lag_net + lag_serv;
	audio_delay += lag_seconds;
	mpxp_info<<"ESD: LatencyInfo: "<<lag_serv<<" "<<lag_net<<" "<<lag_seconds<<std::endl;
    }

    play_fd = esd_play_stream_fallback(esd_fmt, _samplerate,
					server.c_str(), ESD_CLIENT_NAME);
    if (play_fd < 0) {
	mpxp_err<<"ESD: Can't open play stream: "<<strerror(errno)<<std::endl;
	return MPXP_False;
    }

    /* enable non-blocking i/o on the socket connection to the esd server */
    if ((fl = ::fcntl(play_fd, F_GETFL)) >= 0) ::fcntl(play_fd, F_SETFL, O_NDELAY|fl);

#if ESD_DEBUG
    {
	int sbuf, rbuf, len;
	len = sizeof(sbuf);
	getsockopt(play_fd, SOL_SOCKET, SO_SNDBUF, &sbuf, &len);
	len = sizeof(rbuf);
	getsockopt(play_fd, SOL_SOCKET, SO_RCVBUF, &rbuf, &len);
	dprintf("esd: send/receive socket buffer space %d/%d bytes\n",
		sbuf, rbuf);
    }
#endif

    _outburst = bps() > 100000 ? 4*ESD_BUF_SIZE : 2*ESD_BUF_SIZE;

    play_start.tv_sec = 0;
    samples_written = 0;
    bytes_per_sample = _bytes_per_sample;

    return MPXP_Ok;
}

/*
 * plays 'len' bytes of 'data'
 * it should round it down to outburst*n
 * return: number of bytes played
 */
unsigned Esd_AO_Interface::play(const any_t* data, unsigned len, unsigned flags) {
    unsigned offs;
    unsigned nwritten;
    int nsamples;
    int n;
    UNUSED(flags);
    /* round down buffersize to a multiple of ESD_BUF_SIZE bytes */
    len = len / ESD_BUF_SIZE * ESD_BUF_SIZE;

#define	SINGLE_WRITE 0
#if	SINGLE_WRITE
    nwritten = ::write(play_fd, data, len);
#else
    for (offs = 0, nwritten=0; offs + ESD_BUF_SIZE <= len; offs += ESD_BUF_SIZE) {
	/*
	 * note: we're writing to a non-blocking socket here.
	 * A partial write means, that the socket buffer is full.
	 */
	n = ::write(play_fd, (char*)data + offs, ESD_BUF_SIZE);
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
	if (!play_start.tv_sec)
	    ::gettimeofday(&play_start, NULL);
	nsamples = nwritten / bytes_per_sample;
	samples_written += nsamples;

	dprintf("esd play: %d %lu\n", nsamples, samples_written);
    } else {
	dprintf("esd play: blocked / %lu\n", samples_written);
    }
    return nwritten;
}

/*
 * stop playing, keep buffers (for pause)
 */
void Esd_AO_Interface::pause() {}
/*
 * resume playing, after audio_pause()
 */
void Esd_AO_Interface::resume() {
    /*
     * not possible with priv->
     *
     * Let's hope the pause was long enough that the esd ran out of
     * buffered data;  we restart our time based delay computation
     * for an audio resume.
     */
    play_start.tv_sec = 0;
    samples_written = 0;
}

/*
 * stop playing and empty buffers (for seeking/pause)
 */
void Esd_AO_Interface::reset() {
#ifdef	__svr4__
    /* throw away data buffered in the esd connection */
    if (::ioctl(play_fd, I_FLUSH, FLUSHW)) perror("I_FLUSH");
#endif
}

/*
 * return: how many bytes can be played without blocking
 */
unsigned Esd_AO_Interface::get_space()
{
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
    if ((current_delay = get_delay()) >= ESD_MAX_DELAY) {
	dprintf("esd get_space: too much data buffered\n");
	return 0;
    }

    FD_ZERO(&wfds);
    FD_SET(play_fd, &wfds);
    tmout.tv_sec = 0;
    tmout.tv_usec = 0;

    if (::select(play_fd + 1, NULL, &wfds, NULL, &tmout) != 1)
	return 0;

    if (!FD_ISSET(play_fd, &wfds))
	return 0;

    /* try to fill 50% of the remaining "mp_free" buffer space */
    space = (ESD_MAX_DELAY - current_delay) * bps() * 0.5f;

    /* round up to next multiple of ESD_BUF_SIZE */
    space = (space + ESD_BUF_SIZE-1) / ESD_BUF_SIZE * ESD_BUF_SIZE;

    dprintf("esd get_space: %d\n", space);
    return space;
}

/*
 * return: delay in seconds between first and last sample in buffer
 */
float Esd_AO_Interface::get_delay() {
    struct timeval now;
    double buffered_samples_time;
    double play_time;

    if (!play_start.tv_sec) return 0;

    buffered_samples_time = (float)samples_written / _samplerate;
    ::gettimeofday(&now, NULL);
    play_time  =  now.tv_sec  - play_start.tv_sec;
    play_time += (now.tv_usec - play_start.tv_usec) / 1000000.;

    /* dprintf("esd delay: %f %f\n", play_time, buffered_samples_time); */

    if (play_time > buffered_samples_time) {
	dprintf("esd: underflow\n");
	play_start.tv_sec = 0;
	samples_written = 0;
	return 0;
    }

    dprintf("esd: get_delay %f\n", buffered_samples_time - play_time);
    return buffered_samples_time - play_time;
}

unsigned Esd_AO_Interface::samplerate() const { return _samplerate; }
unsigned Esd_AO_Interface::channels() const { return _channels; }
unsigned Esd_AO_Interface::format() const { return _format; }
unsigned Esd_AO_Interface::buffersize() const { return _buffersize; }
unsigned Esd_AO_Interface::outburst() const { return _outburst; }
MPXP_Rc  Esd_AO_Interface::test_channels(unsigned c) const { UNUSED(c); return MPXP_Ok; }
MPXP_Rc  Esd_AO_Interface::test_rate(unsigned r) const { UNUSED(r); return MPXP_Ok; }
MPXP_Rc  Esd_AO_Interface::test_format(unsigned f) const { UNUSED(f); return MPXP_Ok; }

static AO_Interface* query_interface(const std::string& sd) { return new(zeromem) Esd_AO_Interface(sd); }

extern const ao_info_t audio_out_esd = {
    "EsounD audio output",
    "esd",
    "Juergen Keil <jk@tools.de>",
    "",
    query_interface
};
} // namespace mpxp
