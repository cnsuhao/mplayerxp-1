/*
 * OpenAL audio output driver for MPlayerXP
 *
 * Copyleft 2006 by Reimar Döffinger (Reimar.Doeffinger@stud.uni-karlsruhe.de)
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
 * along with MPlayer; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "mp_config.h"

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <AL/alc.h>
#include <AL/al.h>

#include "audio_out.h"
#include "audio_out_internal.h"
#include "postproc/af_format.h"
#include "afmt.h"
#include "osdep/timer.h"
//#include "subopt-helper.h"
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
typedef struct openal_priv_s {
    ALCdevice*	alc_dev;
    ALuint	buffers[MAX_CHANS][NUM_BUF];
    ALuint	sources[MAX_CHANS];

    int		cur_buf[MAX_CHANS];
    int		unqueue_buf[MAX_CHANS];
    int16_t*	tmpbuf;
}openal_priv_t;
static openal_priv_t openal;

static int control(int cmd, long arg) {
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
      return CONTROL_TRUE;
    }
  }
  return CONTROL_UNKNOWN;
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
static int init(unsigned flags)
{
  UNUSED(flags);
  openal.alc_dev = alcOpenDevice(NULL);
  if (!openal.alc_dev) {
    MSG_ERR("[OpenAL] could not open device\n");
    return 0;
  }
  return 1;
}

static int configure(unsigned rate, unsigned channels, unsigned format)
{
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
  if (subopt_parse(ao_subdevice, subopts) != 0) {
    print_help();
    return 0;
  }
*/
  UNUSED(format);
  if (channels > MAX_CHANS) {
    MSG_ERR("[OpenAL] Invalid number of channels: %i\n", channels);
    goto err_out;
  }
  ctx = alcCreateContext(openal.alc_dev, attribs);
  alcMakeContextCurrent(ctx);
  alListenerfv(AL_POSITION, position);
  alListenerfv(AL_ORIENTATION, direction);
  alGenSources(channels, openal.sources);
  for (i = 0; i < channels; i++) {
    openal.cur_buf[i] = 0;
    openal.unqueue_buf[i] = 0;
    alGenBuffers(NUM_BUF, openal.buffers[i]);
    alSourcefv(openal.sources[i], AL_POSITION, sppos[i]);
    alSource3f(openal.sources[i], AL_VELOCITY, 0, 0, 0);
  }
  if (channels == 1)
    alSource3f(openal.sources[0], AL_POSITION, 0, 0, 1);
  ao_data.channels = channels;
  alcGetIntegerv(openal.alc_dev, ALC_FREQUENCY, 1, &freq);
  if (alcGetError(openal.alc_dev) == ALC_NO_ERROR && freq)
    rate = freq;
  ao_data.samplerate = rate;
  ao_data.format = AFMT_S16_NE;
  ao_data.bps = channels * rate * 2;
  ao_data.buffersize = CHUNK_SIZE * NUM_BUF;
  ao_data.outburst = channels * CHUNK_SIZE;
  openal.tmpbuf = malloc(CHUNK_SIZE);
  return 1;

err_out:
  return 0;
}

// close audio device
static void uninit(void) {
  int immed=0;
  ALCcontext *ctx = alcGetCurrentContext();
  ALCdevice *dev = alcGetContextsDevice(ctx);
  free(openal.tmpbuf);
  if (!immed) {
    ALint state;
    alGetSourcei(openal.sources[0], AL_SOURCE_STATE, &state);
    while (state == AL_PLAYING) {
      usec_sleep(10000);
      alGetSourcei(openal.sources[0], AL_SOURCE_STATE, &state);
    }
  }
  reset();
  alcMakeContextCurrent(NULL);
  alcDestroyContext(ctx);
  alcCloseDevice(dev);
}

static void unqueue_buffers(void) {
  ALint p;
  unsigned s;
  for (s = 0;  s < ao_data.channels; s++) {
    int till_wrap = NUM_BUF - openal.unqueue_buf[s];
    alGetSourcei(openal.sources[s], AL_BUFFERS_PROCESSED, &p);
    if (p >= till_wrap) {
      alSourceUnqueueBuffers(openal.sources[s], till_wrap, &openal.buffers[s][openal.unqueue_buf[s]]);
      openal.unqueue_buf[s] = 0;
      p -= till_wrap;
    }
    if (p) {
      alSourceUnqueueBuffers(openal.sources[s], p, &openal.buffers[s][openal.unqueue_buf[s]]);
      openal.unqueue_buf[s] += p;
    }
  }
}

/**
 * \brief stop playing and empty openal.buffers (for seeking/pause)
 */
static void reset(void) {
  alSourceStopv(ao_data.channels, openal.sources);
  unqueue_buffers();
}

/**
 * \brief stop playing, keep openal.buffers (for pause)
 */
static void audio_pause(void) {
  alSourcePausev(ao_data.channels, openal.sources);
}

/**
 * \brief resume playing, after audio_pause()
 */
static void audio_resume(void) {
  alSourcePlayv(ao_data.channels, openal.sources);
}

static unsigned get_space(void) {
  ALint queued;
  unqueue_buffers();
  alGetSourcei(openal.sources[0], AL_BUFFERS_QUEUED, &queued);
  queued = NUM_BUF - queued - 3;
  if (queued < 0) return 0;
  return queued * CHUNK_SIZE * ao_data.channels;
}

/**
 * \brief write data into buffer and reset underrun flag
 */
static unsigned play(void *data, unsigned len, unsigned flags) {
  ALint state;
  unsigned i, j, k;
  unsigned ch;
  int16_t *d = data;
  UNUSED(flags);
  len /= ao_data.channels * CHUNK_SIZE;
  for (i = 0; i < len; i++) {
    for (ch = 0; ch < ao_data.channels; ch++) {
      for (j = 0, k = ch; j < CHUNK_SIZE / 2; j++, k += ao_data.channels)
        openal.tmpbuf[j] = d[k];
      alBufferData(openal.buffers[ch][openal.cur_buf[ch]], AL_FORMAT_MONO16, openal.tmpbuf,
                     CHUNK_SIZE, ao_data.samplerate);
      alSourceQueueBuffers(openal.sources[ch], 1, &openal.buffers[ch][openal.cur_buf[ch]]);
      openal.cur_buf[ch] = (openal.cur_buf[ch] + 1) % NUM_BUF;
    }
    d += ao_data.channels * CHUNK_SIZE / 2;
  }
  alGetSourcei(openal.sources[0], AL_SOURCE_STATE, &state);
  if (state != AL_PLAYING) // checked here in case of an underrun
    alSourcePlayv(ao_data.channels, openal.sources);
  return len * ao_data.channels * CHUNK_SIZE;
}

static float get_delay(void) {
  ALint queued;
  unqueue_buffers();
  alGetSourcei(openal.sources[0], AL_BUFFERS_QUEUED, &queued);
  return queued * CHUNK_SIZE / 2 / (float)ao_data.samplerate;
}

