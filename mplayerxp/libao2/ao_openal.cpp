/*
 * OpenAL audio output driver for MPlayerXP
 *
 * Copyleft 2006 by Reimar Döffinger (Reimar.Doeffinger@stud.uni-karlsruhe.de)
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

#include "mp_config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <AL/alc.h>
#include <AL/al.h>

#include "audio_out.h"
#include "audio_out_internal.h"
#include "postproc/af.h"
#include "afmt.h"
#include "osdep/timer.h"
#include "osdep/mplib.h"
#include "ao_msg.h"

static const ao_info_t info =
{
  "OpenAL audio output",
  "openal",
  "Reimar Döffinger <Reimar.Doeffinger@stud.uni-karlsruhe.de>",
  ""
};

LIBAO_EXTERN(openal)

#define MAX_CHANS 8
#define NUM_BUF 128
#define CHUNK_SIZE 512
typedef struct priv_s {
    ALCdevice*	alc_dev;
    ALuint	buffers[MAX_CHANS][NUM_BUF];
    ALuint	sources[MAX_CHANS];

    int		cur_buf[MAX_CHANS];
    int		unqueue_buf[MAX_CHANS];
    int16_t*	tmpbuf;
}priv_t;

static MPXP_Rc control(const ao_data_t* ao,int cmd, long arg) {
    UNUSED(ao);
  switch (cmd) {
    case AOCONTROL_GET_VOLUME:
    case AOCONTROL_SET_VOLUME: {
      ALfloat volume;
      ao_control_vol_t *vol = (ao_control_vol_t *)arg;
      if (cmd == AOCONTROL_SET_VOLUME) {
	volume = (vol->left + vol->right) / 200.0;
	alListenerf(AL_GAIN, volume);
      }
      alGetListenerf(AL_GAIN, &volume);
      vol->left = vol->right = volume * 100;
      return MPXP_True;
    }
  }
  return MPXP_Unknown;
}

#if 0
/**
 * \brief print suboption usage help
 */
static void print_help(void) {
  MSG_FATAL(
	  "\n-ao openal commandline help:\n"
	  "Example: mplayer -ao openal\n"
	  "\nOptions:\n"
	);
}
#endif
static MPXP_Rc init(ao_data_t* ao,unsigned flags)
{
    priv_t*priv;
    priv=new(zeromem) priv_t;
    ao->priv=priv;
    UNUSED(flags);
    priv->alc_dev = alcOpenDevice(NULL);
    if (!priv->alc_dev) {
	MSG_ERR("[OpenAL] could not open device\n");
	return MPXP_False;
    }
    return MPXP_Ok;
}

static MPXP_Rc configure(ao_data_t* ao,unsigned rate, unsigned channels, unsigned format)
{
    priv_t*priv=reinterpret_cast<priv_t*>(ao->priv);
    ALCcontext *ctx = NULL;
    float position[3] = {0, 0, 0};
    float direction[6] = {0, 0, 1, 0, -1, 0};
    float sppos[MAX_CHANS][3] = {
	{-1, 0, 0.5}, {1, 0, 0.5},
	{-1, 0,  -1}, {1, 0,  -1},
	{0,  0,   1}, {0, 0, 0.1},
	{-1, 0,   0}, {1, 0,   0},
    };
    ALCint freq = 0;
    ALCint attribs[] = {ALC_FREQUENCY, rate, 0, 0};
    unsigned i;
/*
  const opt_t subopts[] = {
    {NULL}
  };
  if (subopt_parse(ao->subdevice, subopts) != 0) {
    print_help();
    return 0;
  }
*/
    UNUSED(format);
    if (channels > MAX_CHANS) {
	MSG_ERR("[OpenAL] Invalid number of channels: %i\n", channels);
	goto err_out;
    }
    ctx = alcCreateContext(priv->alc_dev, attribs);
    alcMakeContextCurrent(ctx);
    alListenerfv(AL_POSITION, position);
    alListenerfv(AL_ORIENTATION, direction);
    alGenSources(channels, priv->sources);
    for (i = 0; i < channels; i++) {
	priv->cur_buf[i] = 0;
	priv->unqueue_buf[i] = 0;
	alGenBuffers(NUM_BUF, priv->buffers[i]);
	alSourcefv(priv->sources[i], AL_POSITION, sppos[i]);
	alSource3f(priv->sources[i], AL_VELOCITY, 0, 0, 0);
    }
    if (channels == 1)
	alSource3f(priv->sources[0], AL_POSITION, 0, 0, 1);
    ao->channels = channels;
    alcGetIntegerv(priv->alc_dev, ALC_FREQUENCY, 1, &freq);
    if (alcGetError(priv->alc_dev) == ALC_NO_ERROR && freq)
	rate = freq;
    ao->samplerate = rate;
    ao->format = AFMT_S16_NE;
    ao->bps = channels * rate * 2;
    ao->buffersize = CHUNK_SIZE * NUM_BUF;
    ao->outburst = channels * CHUNK_SIZE;
    priv->tmpbuf = new int16_t[CHUNK_SIZE];
    return MPXP_Ok;
err_out:
    return MPXP_False;
}

// close audio device
static void uninit(ao_data_t* ao) {
  int immed=0;
  ALCcontext *ctx = alcGetCurrentContext();
  ALCdevice *dev = alcGetContextsDevice(ctx);
  priv_t*priv=reinterpret_cast<priv_t*>(ao->priv);
  mp_free(priv->tmpbuf);
  if (!immed) {
    ALint state;
    alGetSourcei(priv->sources[0], AL_SOURCE_STATE, &state);
    while (state == AL_PLAYING) {
      usec_sleep(10000);
      alGetSourcei(priv->sources[0], AL_SOURCE_STATE, &state);
    }
  }
  reset(ao);
  alcMakeContextCurrent(NULL);
  alcDestroyContext(ctx);
  alcCloseDevice(dev);
  mp_free(ao->priv);
}

static void unqueue_buffers(const ao_data_t* ao) {
    priv_t*priv=reinterpret_cast<priv_t*>(ao->priv);
  ALint p;
  unsigned s;
  for (s = 0;  s < ao->channels; s++) {
    int till_wrap = NUM_BUF - priv->unqueue_buf[s];
    alGetSourcei(priv->sources[s], AL_BUFFERS_PROCESSED, &p);
    if (p >= till_wrap) {
      alSourceUnqueueBuffers(priv->sources[s], till_wrap, &priv->buffers[s][priv->unqueue_buf[s]]);
      priv->unqueue_buf[s] = 0;
      p -= till_wrap;
    }
    if (p) {
      alSourceUnqueueBuffers(priv->sources[s], p, &priv->buffers[s][priv->unqueue_buf[s]]);
      priv->unqueue_buf[s] += p;
    }
  }
}

/**
 * \brief stop playing and empty priv->buffers (for seeking/pause)
 */
static void reset(ao_data_t* ao) {
    priv_t*priv=reinterpret_cast<priv_t*>(ao->priv);
  alSourceStopv(ao->channels, priv->sources);
  unqueue_buffers(ao);
}

/**
 * \brief stop playing, keep priv->buffers (for pause)
 */
static void audio_pause(ao_data_t* ao) {
    priv_t*priv=reinterpret_cast<priv_t*>(ao->priv);
  alSourcePausev(ao->channels, priv->sources);
}

/**
 * \brief resume playing, after audio_pause()
 */
static void audio_resume(ao_data_t* ao) {
    priv_t*priv=reinterpret_cast<priv_t*>(ao->priv);
  alSourcePlayv(ao->channels, priv->sources);
}

static unsigned get_space(const ao_data_t* ao) {
    priv_t*priv=reinterpret_cast<priv_t*>(ao->priv);
  ALint queued;
  unqueue_buffers(ao);
  alGetSourcei(priv->sources[0], AL_BUFFERS_QUEUED, &queued);
  queued = NUM_BUF - queued - 3;
  if (queued < 0) return 0;
  return queued * CHUNK_SIZE * ao->channels;
}

/**
 * \brief write data into buffer and reset underrun flag
 */
static unsigned play(ao_data_t* ao,const any_t*data, unsigned len, unsigned flags) {
    priv_t*priv=reinterpret_cast<priv_t*>(ao->priv);
  ALint state;
  unsigned i, j, k;
  unsigned ch;
  const int16_t *d = reinterpret_cast<const int16_t*>(data);
  UNUSED(flags);
  len /= ao->channels * CHUNK_SIZE;
  for (i = 0; i < len; i++) {
    for (ch = 0; ch < ao->channels; ch++) {
      for (j = 0, k = ch; j < CHUNK_SIZE / 2; j++, k += ao->channels)
	priv->tmpbuf[j] = d[k];
      alBufferData(priv->buffers[ch][priv->cur_buf[ch]], AL_FORMAT_MONO16, priv->tmpbuf,
		     CHUNK_SIZE, ao->samplerate);
      alSourceQueueBuffers(priv->sources[ch], 1, &priv->buffers[ch][priv->cur_buf[ch]]);
      priv->cur_buf[ch] = (priv->cur_buf[ch] + 1) % NUM_BUF;
    }
    d += ao->channels * CHUNK_SIZE / 2;
  }
  alGetSourcei(priv->sources[0], AL_SOURCE_STATE, &state);
  if (state != AL_PLAYING) // checked here in case of an underrun
    alSourcePlayv(ao->channels, priv->sources);
  return len * ao->channels * CHUNK_SIZE;
}

static float get_delay(const ao_data_t* ao) {
    priv_t*priv=reinterpret_cast<priv_t*>(ao->priv);
  ALint queued;
  unqueue_buffers(ao);
  alGetSourcei(priv->sources[0], AL_BUFFERS_QUEUED, &queued);
  return queued * CHUNK_SIZE / 2 / (float)ao->samplerate;
}

