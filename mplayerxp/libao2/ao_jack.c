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

#include "mp_config.h"

#include "audio_out.h"
#include "audio_out_internal.h"
#include "postproc/af.h"
#include "afmt.h"
#include "osdep/timer.h"
#include "osdep/mplib.h"
#include "ao_msg.h"

#include "libavutil/fifo.h"
#include <jack/jack.h>

static const ao_info_t info =
{
  "JACK audio output",
  "jack",
  "Reimar Döffinger <Reimar.Doeffinger@stud.uni-karlsruhe.de>",
  "based on ao_sdl.c"
};

LIBAO_EXTERN(jack)

//! maximum number of channels supported, avoids lots of mallocs
#define MAX_CHANS 6
typedef struct priv_s {
    jack_port_t *	ports[MAX_CHANS];
    unsigned		num_ports; ///< Number of used ports == number of channels
    jack_client_t *	client;
    float		latency;
    int			estimate;
    volatile int	paused; ///< set if paused
    volatile int	underrun; ///< signals if an priv->underrun occured

    volatile float	callback_interval;
    volatile float	callback_time;

    AVFifoBuffer *	buffer; //! buffer for audio data
}priv_t;


//! size of one chunk, if this is too small MPlayer will start to "stutter"
//! after a short time of playback
#define CHUNK_SIZE (16 * 1024)
//! number of "virtual" chunks the priv->buffer consists of
#define NUM_CHUNKS 8
#define BUFFSIZE (NUM_CHUNKS * CHUNK_SIZE)


/**
 * \brief insert len bytes into priv->buffer
 * \param data data to insert
 * \param len length of data
 * \return number of bytes inserted into priv->buffer
 *
 * If there is not enough room, the priv->buffer is filled up
 */
static int write_buffer(ao_data_t* ao,unsigned char* data, int len) {
    priv_t*priv=ao->priv;
  int _free = av_fifo_space(priv->buffer);
  if (len > _free) len = _free;
  return av_fifo_generic_write(priv->buffer, data, len, NULL);
}

static void silence(float **bufs, int cnt, int num_bufs);

struct deinterleave {
  float **bufs;
  int num_bufs;
  int cur_buf;
  int pos;
};

static void deinterleave(any_t*_info, any_t*src, int len) {
  struct deinterleave *di = _info;
  float *s = src;
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
static unsigned read_buffer(ao_data_t* ao,float **bufs, unsigned cnt, unsigned num_bufs) {
    priv_t*priv=ao->priv;
  struct deinterleave di = {bufs, num_bufs, 0, 0};
  unsigned buffered = av_fifo_size(priv->buffer);
  if (cnt * sizeof(float) * num_bufs > buffered) {
    silence(bufs, cnt, num_bufs);
    cnt = buffered / sizeof(float) / num_bufs;
  }
  av_fifo_generic_read(priv->buffer, &di, cnt * num_bufs * sizeof(float), deinterleave);
  return cnt;
}

// end ring priv->buffer stuff

static MPXP_Rc control(ao_data_t* ao,int cmd, long arg) {
    UNUSED(ao);
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
static void silence(float **bufs, int cnt, int num_bufs) {
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
static int outputaudio(jack_nframes_t nframes, any_t* ao) {
    priv_t*priv=((ao_data_t*)ao)->priv;
  float *bufs[MAX_CHANS];
  unsigned i;
  for (i = 0; i < priv->num_ports; i++)
    bufs[i] = jack_port_get_buffer(priv->ports[i], nframes);
  if (priv->paused || priv->underrun)
    silence(bufs, nframes, priv->num_ports);
  else
    if (read_buffer(ao,bufs, nframes, priv->num_ports) < nframes)
      priv->underrun = 1;
  if (priv->estimate) {
    float now = (float)GetTimer() / 1000000.0;
    float diff = priv->callback_time + priv->callback_interval - now;
    if ((diff > -0.002) && (diff < 0.002))
      priv->callback_time += priv->callback_interval;
    else
      priv->callback_time = now;
    priv->callback_interval = (float)nframes / (float)((ao_data_t*)ao)->samplerate;
  }
  return 0;
}

#if 0
/**
 * \brief print suboption usage help
 */
static void print_help (void)
{
  MSG_FATAL(
           "\n-ao jack commandline help:\n"
           "Example: mplayer -ao jack:port=myout\n"
           "  connects MPlayer to the jack priv->ports named myout\n"
           "\nOptions:\n"
           "  port=<port name>\n"
           "    Connects to the given priv->ports instead of the default physical ones\n"
           "  name=<client name>\n"
           "    priv->client name to pass to JACK\n"
           "  priv->estimate\n"
           "    priv->estimates the amount of data in priv->buffers (experimental)\n"
           "  autostart\n"
           "    Automatically start JACK server if necessary\n"
         );
}
#endif
static MPXP_Rc init(ao_data_t* ao,unsigned flags) {
    ao->priv=mp_mallocz(sizeof(priv_t));
    UNUSED(flags);
    return MPXP_Ok;
}

static MPXP_Rc configure(ao_data_t* ao,unsigned rate,unsigned channels,unsigned format) {
    priv_t*priv=ao->priv;
    const char **matching_ports = NULL;
    char *port_name = NULL;
    char *client_name = NULL;
    int autostart = 0;
/*
  const opt_t subopts[] = {
    {"port", OPT_ARG_MSTRZ, &port_name, NULL},
    {"name", OPT_ARG_MSTRZ, &client_name, NULL},
    {"priv->estimate", OPT_ARG_BOOL, &priv->estimate, NULL},
    {"autostart", OPT_ARG_BOOL, &autostart, NULL},
    {NULL}
  };
*/
    jack_options_t open_options = JackUseExactName;
    int port_flags = JackPortIsInput;
    unsigned i;
    priv->estimate = 1;
    UNUSED(format);
/*
  if (subopt_parse(ao->subdevice, subopts) != 0) {
    print_help();
    return 0;
  }
*/
    if (channels > MAX_CHANS) {
	MSG_FATAL("[JACK] Invalid number of channels: %i\n", channels);
	goto err_out;
    }
    if (!client_name) {
	client_name = mp_malloc(40);
	sprintf(client_name, "MPlayerXP [%d]", getpid());
    }
    if (!autostart)
	open_options |= JackNoStartServer;
    priv->client = jack_client_open(client_name, open_options, NULL);
    if (!priv->client) {
	MSG_FATAL("[JACK] cannot open server\n");
	goto err_out;
    }
    priv->buffer = av_fifo_alloc(BUFFSIZE);
    jack_set_process_callback(priv->client, outputaudio, ao);

    // list matching priv->ports
    if (!port_name)
	port_flags |= JackPortIsPhysical;
    matching_ports = jack_get_ports(priv->client, ao->subdevice, NULL, port_flags);
    if (!matching_ports || !matching_ports[0]) {
	MSG_FATAL("[JACK] no physical priv->ports available\n");
	goto err_out;
    }
    i = 1;
    while (matching_ports[i]) i++;
    if (channels > i) channels = i;
    priv->num_ports = channels;

    // create out output priv->ports
    for (i = 0; i < priv->num_ports; i++) {
	char pname[30];
	snprintf(pname, 30, "out_%d", i);
	priv->ports[i] = jack_port_register(priv->client, pname, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
	if (!priv->ports[i]) {
	    MSG_FATAL("[JACK] not enough priv->ports available\n");
	    goto err_out;
	}
    }
    if (jack_activate(priv->client)) {
	MSG_FATAL("[JACK] activate failed\n");
	goto err_out;
    }
    for (i = 0; i < priv->num_ports; i++) {
	if (jack_connect(priv->client, jack_port_name(priv->ports[i]), matching_ports[i])) {
	    MSG_FATAL( "[JACK] connecting failed\n");
	    goto err_out;
	}
    }
    rate = jack_get_sample_rate(priv->client);
    priv->latency = (float)(jack_port_get_total_latency(priv->client, priv->ports[0]) +
			    jack_get_buffer_size(priv->client)) / (float)rate;
    priv->callback_interval = 0;

    ao->channels = channels;
    ao->samplerate = rate;
    ao->format = AFMT_FLOAT32;
    ao->bps = channels * rate * sizeof(float);
    ao->buffersize = CHUNK_SIZE * NUM_CHUNKS;
    ao->outburst = CHUNK_SIZE;
    mp_free(matching_ports);
    mp_free(port_name);
    mp_free(client_name);
    return MPXP_Ok;

err_out:
    mp_free(matching_ports);
    mp_free(port_name);
    mp_free(client_name);
    if (priv->client) jack_client_close(priv->client);
    av_fifo_free(priv->buffer);
    priv->buffer = NULL;
    return MPXP_False;
}

// close audio device
static void uninit(ao_data_t* ao) {
    priv_t*priv=ao->priv;
  // HACK, make sure jack doesn't loop-output dirty priv->buffers
  reset(ao);
  usec_sleep(100 * 1000);
  jack_client_close(priv->client);
  av_fifo_free(priv->buffer);
  priv->buffer = NULL;
  mp_free(priv);
}

/**
 * \brief stop playing and empty priv->buffers (for seeking/pause)
 */
static void reset(ao_data_t* ao) {
    priv_t*priv=ao->priv;
  priv->paused = 1;
  av_fifo_reset(priv->buffer);
  priv->paused = 0;
}

/**
 * \brief stop playing, keep priv->buffers (for pause)
 */
static void audio_pause(ao_data_t* ao) {
    priv_t*priv=ao->priv;
  priv->paused = 1;
}

/**
 * \brief resume playing, after audio_pause()
 */
static void audio_resume(ao_data_t* ao) {
    priv_t*priv=ao->priv;
  priv->paused = 0;
}

static unsigned get_space(ao_data_t* ao) {
    priv_t*priv=ao->priv;
  return av_fifo_space(priv->buffer);
}

/**
 * \brief write data into priv->buffer and reset priv->underrun flag
 */
static unsigned play(ao_data_t* ao,any_t*data, unsigned len, unsigned flags) {
    priv_t*priv=ao->priv;
  priv->underrun = 0;
  UNUSED(flags);
  return write_buffer(ao,data, len);
}

static float get_delay(ao_data_t* ao) {
    priv_t*priv=ao->priv;
  int buffered = av_fifo_size(priv->buffer); // could be less
  float in_jack = priv->latency;
  if (priv->estimate && priv->callback_interval > 0) {
    float elapsed = (float)GetTimer() / 1000000.0 - priv->callback_time;
    in_jack += priv->callback_interval - elapsed;
    if (in_jack < 0) in_jack = 0;
  }
  return (float)buffered / (float)ao->bps + in_jack;
}

