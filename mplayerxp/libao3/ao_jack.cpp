#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
/*
 * JACK audio output driver for MPlayer
 *
 * Copyleft 2001 by Felix Bünemann (atmosfear@users.sf.net)
 * and Reimar Döffinger (Reimar.Doeffinger@stud.uni-karlsruhe.de)
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
 * along with MPlayer; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "audio_out.h"
#include "audio_out_internal.h"
#include "postproc/af.h"
#include "afmt.h"
#include "osdep/timer.h"
#include "ao_msg.h"

#include "mpxp_conf_lavc.h"
#include <jack/jack.h>

namespace mpxp {

//! maximum number of channels supported, avoids lots of mallocs
#define MAX_CHANS 6
class Jack_AO_Interface : public AO_Interface {
    public:
	Jack_AO_Interface(const std::string& subdevice);
	virtual ~Jack_AO_Interface();

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
	static void		deinterleave_data(any_t*_info, any_t*src, int len);
	static int		outputaudio(jack_nframes_t nframes, any_t* _ao);
    private:
	unsigned	_channels,_samplerate,_format;
	unsigned	_buffersize,_outburst;
	unsigned	bps() const { return _channels*_samplerate*afmt2bps(_format); }
	unsigned	read_buffer(float **bufs, unsigned cnt, unsigned num_bufs);
	int		write_buffer(const unsigned char* data, int len);
	static void	silence(float **bufs, int cnt, int num_bufs);

	jack_port_t *	ports[MAX_CHANS];
	unsigned	num_ports; ///< Number of used ports == number of channels
	jack_client_t *	client;
	float		latency;
	int		estimate;
	volatile int	paused; ///< set if paused
	volatile int	underrun; ///< signals if an priv->underrun occured

	volatile float	callback_interval;
	volatile float	callback_time;

	AVFifoBuffer *	buffer; //! buffer for audio data
};
//! size of one chunk, if this is too small MPlayer will start to "stutter"
//! after a short time of playback
#define CHUNK_SIZE (16 * 1024)
//! number of "virtual" chunks the priv->buffer consists of
#define NUM_CHUNKS 8
#define BUFFSIZE (NUM_CHUNKS * CHUNK_SIZE)
Jack_AO_Interface::Jack_AO_Interface(const std::string& _subdevice)
		:AO_Interface(_subdevice) {}
Jack_AO_Interface::~Jack_AO_Interface() {
    // HACK, make sure jack doesn't loop-output dirty priv->buffers
    reset();
    usec_sleep(100 * 1000);
    jack_client_close(client);
    av_fifo_free(buffer);
    buffer = NULL;
}
/**
 * \brief insert len bytes into priv->buffer
 * \param data data to insert
 * \param len length of data
 * \return number of bytes inserted into priv->buffer
 *
 * If there is not enough room, the priv->buffer is filled up
 */
int Jack_AO_Interface::write_buffer(const unsigned char* data, int len) {
    int _free = av_fifo_space(buffer);
    if (len > _free) len = _free;
    return av_fifo_generic_write(buffer, const_cast<unsigned char *>(data), len, NULL);
}

struct deinterleave {
  float **bufs;
  int num_bufs;
  int cur_buf;
  int pos;
};

void Jack_AO_Interface::deinterleave_data(any_t*_info, any_t*src, int len) {
  struct deinterleave *di = reinterpret_cast<struct deinterleave*>(_info);
  float *s = reinterpret_cast<float*>(src);
  int i;
  len /= sizeof(float);
  for (i = 0; i < len; i++) {
    di->bufs[di->cur_buf++][di->pos] = s[i];
    if (di->cur_buf >= di->num_bufs) {
      di->cur_buf = 0;
      di->pos++;
    }
  }
}

/**
 * \brief read data from priv->buffer and splitting it into channels
 * \param bufs num_bufs float priv->buffers, each will contain the data of one channel
 * \param cnt number of samples to read per channel
 * \param num_bufs number of channels to split the data into
 * \return number of samples read per channel, equals cnt unless there was too
 *         little data in the priv->buffer
 *
 * Assumes the data in the priv->buffer is of type float, the number of bytes
 * read is res * num_bufs * sizeof(float), where res is the return value.
 * If there is not enough data in the priv->buffer remaining parts will be filled
 * with silence.
 */
unsigned Jack_AO_Interface::read_buffer(float **bufs, unsigned cnt, unsigned num_bufs) {
    struct deinterleave di = {bufs, num_bufs, 0, 0};
    unsigned buffered = av_fifo_size(buffer);
    if (cnt * sizeof(float) * num_bufs > buffered) {
	silence(bufs, cnt, num_bufs);
	cnt = buffered / sizeof(float) / num_bufs;
    }
    av_fifo_generic_read(buffer, &di, cnt * num_bufs * sizeof(float), deinterleave_data);
    return cnt;
}

// end ring priv->buffer stuff

MPXP_Rc Jack_AO_Interface::ctrl(int cmd, long arg) const {
    UNUSED(cmd);
    UNUSED(arg);
    return MPXP_Unknown;
}

/**
 * \brief fill the priv->buffers with silence
 * \param bufs num_bufs float priv->buffers, each will contain the data of one channel
 * \param cnt number of samples in each priv->buffer
 * \param num_bufs number of priv->buffers
 */
void Jack_AO_Interface::silence(float **bufs, int cnt, int num_bufs) {
  int i;
  for (i = 0; i < num_bufs; i++)
    memset(bufs[i], 0, cnt * sizeof(float));
}

/**
 * \brief JACK Callback function
 * \param nframes number of frames to fill into priv->buffers
 * \param arg unused
 * \return currently always 0
 *
 * Write silence into priv->buffers if priv->paused or an priv->underrun occured
 */
int Jack_AO_Interface::outputaudio(jack_nframes_t nframes, any_t* _priv) {
    Jack_AO_Interface& _this=*reinterpret_cast<Jack_AO_Interface*>(_priv);
    float *bufs[MAX_CHANS];
    unsigned i;
    for (i = 0; i < _this.num_ports; i++)
	bufs[i] = (float*)jack_port_get_buffer(_this.ports[i], nframes);
    if (_this.paused || _this.underrun)
	silence(bufs, nframes, _this.num_ports);
    else if (_this.read_buffer(bufs, nframes, _this.num_ports) < nframes)
      _this.underrun = 1;
    if (_this.estimate) {
	float now = (float)GetTimer() / 1000000.0;
	float diff = _this.callback_time + _this.callback_interval - now;
	if ((diff > -0.002) && (diff < 0.002))
	    _this.callback_time += _this.callback_interval;
	else
	    _this.callback_time = now;
	_this.callback_interval = (float)nframes / (float)_this._samplerate;
    }
    return 0;
}

MPXP_Rc Jack_AO_Interface::open(unsigned flags) {
    UNUSED(flags);
    return MPXP_Ok;
}

MPXP_Rc Jack_AO_Interface::configure(unsigned r,unsigned c,unsigned f) {
    const char **matching_ports = NULL;
    char *port_name = NULL;
    char *client_name = NULL;
    int autostart = 0;

    jack_options_t open_options = JackUseExactName;
    int port_flags = JackPortIsInput;
    unsigned i;
    estimate = 1;
    UNUSED(f);

    if (c > MAX_CHANS) {
	MSG_FATAL("[JACK] Invalid number of channels: %i\n", c);
	goto err_out;
    }
    if (!client_name) {
	client_name = new char [40];
	sprintf(client_name, "MPlayerXP [%d]", getpid());
    }
    if (!autostart) open_options = jack_options_t(open_options|JackNoStartServer);
    client = jack_client_open(client_name, open_options, NULL);
    if (!client) {
	MSG_FATAL("[JACK] cannot open server\n");
	goto err_out;
    }
    buffer = av_fifo_alloc(BUFFSIZE);
    jack_set_process_callback(client, outputaudio, this);

    // list matching priv->ports
    if (!port_name) port_flags |= JackPortIsPhysical;
    matching_ports = jack_get_ports(client, subdevice.c_str(), NULL, port_flags);
    if (!matching_ports || !matching_ports[0]) {
	MSG_FATAL("[JACK] no physical priv->ports available\n");
	goto err_out;
    }
    i = 1;
    while (matching_ports[i]) i++;
    if (c > i) c = i;
    num_ports = c;

    // create out output priv->ports
    for (i = 0; i < num_ports; i++) {
	char pname[30];
	snprintf(pname, 30, "out_%d", i);
	ports[i] = jack_port_register(client, pname, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
	if (!ports[i]) {
	    MSG_FATAL("[JACK] not enough priv->ports available\n");
	    goto err_out;
	}
    }
    if (jack_activate(client)) {
	MSG_FATAL("[JACK] activate failed\n");
	goto err_out;
    }
    for (i = 0; i < num_ports; i++) {
	if (jack_connect(client, jack_port_name(ports[i]), matching_ports[i])) {
	    MSG_FATAL( "[JACK] connecting failed\n");
	    goto err_out;
	}
    }
    r = jack_get_sample_rate(client);
    latency = (float)(jack_port_get_total_latency(client, ports[0]) +
			jack_get_buffer_size(client)) / (float)r;
    callback_interval = 0;

    _channels = c;
    _samplerate = r;
    _format = AFMT_FLOAT32;
    _buffersize = CHUNK_SIZE * NUM_CHUNKS;
    _outburst = CHUNK_SIZE;
    delete matching_ports;
    delete port_name;
    delete client_name;
    return MPXP_Ok;

err_out:
    delete matching_ports;
    delete port_name;
    delete client_name;
    if (client) jack_client_close(client);
    av_fifo_free(buffer);
    buffer = NULL;
    return MPXP_False;
}

/**
 * \brief stop playing and empty priv->buffers (for seeking/pause)
 */
void Jack_AO_Interface::reset() {
    paused = 1;
    av_fifo_reset(buffer);
    paused = 0;
}

/**
 * \brief stop playing, keep priv->buffers (for pause)
 */
void Jack_AO_Interface::pause() { paused = 1; }

/**
 * \brief resume playing, after audio_pause()
 */
void Jack_AO_Interface::resume() { paused = 0; }

unsigned Jack_AO_Interface::get_space() {
    return av_fifo_space(buffer);
}

/**
 * \brief write data into priv->buffer and reset priv->underrun flag
 */
unsigned Jack_AO_Interface::play(const any_t*data, unsigned len, unsigned flags) {
    underrun = 0;
    UNUSED(flags);
    return write_buffer(reinterpret_cast<const unsigned char*>(data), len);
}

float Jack_AO_Interface::get_delay() {
    int buffered = av_fifo_size(buffer); // could be less
    float in_jack = latency;
    if (estimate && callback_interval > 0) {
	float elapsed = (float)GetTimer() / 1000000.0f - callback_time;
	in_jack += callback_interval - elapsed;
	if (in_jack < 0) in_jack = 0;
    }
    return (float)buffered / (float)bps() + in_jack;
}

unsigned Jack_AO_Interface::samplerate() const { return _samplerate; }
unsigned Jack_AO_Interface::channels() const { return _channels; }
unsigned Jack_AO_Interface::format() const { return _format; }
unsigned Jack_AO_Interface::buffersize() const { return _buffersize; }
unsigned Jack_AO_Interface::outburst() const { return _outburst; }
MPXP_Rc  Jack_AO_Interface::test_channels(unsigned c) const { UNUSED(c); return MPXP_Ok; }
MPXP_Rc  Jack_AO_Interface::test_rate(unsigned r) const { UNUSED(r); return MPXP_Ok; }
MPXP_Rc  Jack_AO_Interface::test_format(unsigned f) const { return f==AFMT_FLOAT32?MPXP_Ok:MPXP_False; }

static AO_Interface* query_interface(const std::string& sd) { return new(zeromem) Jack_AO_Interface(sd); }

extern const ao_info_t audio_out_jack = {
    "JACK audio output",
    "jack",
    "Reimar Döffinger <Reimar.Doeffinger@stud.uni-karlsruhe.de>",
    "based on ao_sdl.c",
    query_interface
};
} // namespace mpxp
