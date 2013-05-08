#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
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
#include "ao_msg.h"

namespace	usr {
static const int MAX_CHANS=8;
static const int NUM_BUF=128;
static const int CHUNK_SIZE=512;
class OpenAL_AO_Interface : public AO_Interface {
    public:
	OpenAL_AO_Interface(const std::string& subdevice);
	virtual ~OpenAL_AO_Interface();

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
	void		unqueue_buffers();

	ALCdevice*	alc_dev;
	ALuint		buffers[MAX_CHANS][NUM_BUF];
	ALuint		sources[MAX_CHANS];

	int		cur_buf[MAX_CHANS];
	int		unqueue_buf[MAX_CHANS];
	int16_t*	tmpbuf;
};

OpenAL_AO_Interface::OpenAL_AO_Interface(const std::string& _subdevice)
		:AO_Interface(_subdevice) {}
OpenAL_AO_Interface::~OpenAL_AO_Interface() {
    int immed=0;
    ALCcontext *ctx = alcGetCurrentContext();
    ALCdevice *dev = alcGetContextsDevice(ctx);
    delete tmpbuf;
    if (!immed) {
	ALint state;
	alGetSourcei(sources[0], AL_SOURCE_STATE, &state);
	while (state == AL_PLAYING) {
	    ::usec_sleep(10000);
	    alGetSourcei(sources[0], AL_SOURCE_STATE, &state);
	}
    }
    reset();
    alcMakeContextCurrent(NULL);
    alcDestroyContext(ctx);
    alcCloseDevice(dev);
}

MPXP_Rc OpenAL_AO_Interface::ctrl(int cmd, long arg) const {
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

MPXP_Rc OpenAL_AO_Interface::open(unsigned flags) {
    UNUSED(flags);
    alc_dev = alcOpenDevice(NULL);
    if (!alc_dev) {
	mpxp_err<<"[OpenAL] could not open device"<<std::endl;
	return MPXP_False;
    }
    return MPXP_Ok;
}

MPXP_Rc OpenAL_AO_Interface::configure(unsigned r, unsigned c, unsigned f)
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
    ALCint attribs[] = {ALC_FREQUENCY, r, 0, 0};
    unsigned i;
    _format=f;
    if (c > MAX_CHANS) {
	mpxp_err<<"[OpenAL] Invalid number of channels: "<<c<<std::endl;
	goto err_out;
    }
    ctx = alcCreateContext(alc_dev, attribs);
    alcMakeContextCurrent(ctx);
    alListenerfv(AL_POSITION, position);
    alListenerfv(AL_ORIENTATION, direction);
    alGenSources(c, sources);
    for (i = 0; i < c; i++) {
	cur_buf[i] = 0;
	unqueue_buf[i] = 0;
	alGenBuffers(NUM_BUF, buffers[i]);
	alSourcefv(sources[i], AL_POSITION, sppos[i]);
	alSource3f(sources[i], AL_VELOCITY, 0, 0, 0);
    }
    if (c == 1) alSource3f(sources[0], AL_POSITION, 0, 0, 1);
    _channels = c;
    alcGetIntegerv(alc_dev, ALC_FREQUENCY, 1, &freq);
    if (alcGetError(alc_dev) == ALC_NO_ERROR && freq)
	r = freq;
    _samplerate = r;
    _format = AFMT_S16_NE;
    _buffersize = CHUNK_SIZE * NUM_BUF;
    _outburst = _channels * CHUNK_SIZE;
    tmpbuf = new int16_t[CHUNK_SIZE];
    return MPXP_Ok;
err_out:
    return MPXP_False;
}

void OpenAL_AO_Interface::unqueue_buffers() {
    ALint p;
    unsigned s;
    for (s = 0;  s < _channels; s++) {
	int till_wrap = NUM_BUF - unqueue_buf[s];
	alGetSourcei(sources[s], AL_BUFFERS_PROCESSED, &p);
	if (p >= till_wrap) {
	    alSourceUnqueueBuffers(sources[s], till_wrap, &buffers[s][unqueue_buf[s]]);
	    unqueue_buf[s] = 0;
	    p -= till_wrap;
	}
	if (p) {
	    alSourceUnqueueBuffers(sources[s], p, &buffers[s][unqueue_buf[s]]);
	    unqueue_buf[s] += p;
	}
    }
}

/**
 * \brief stop playing and empty priv->buffers (for seeking/pause)
 */
void OpenAL_AO_Interface::reset() {
    alSourceStopv(_channels, sources);
    unqueue_buffers();
}

/**
 * \brief stop playing, keep priv->buffers (for pause)
 */
void OpenAL_AO_Interface::pause() {
    alSourcePausev(_channels, sources);
}

/**
 * \brief resume playing, after audio_pause()
 */
void OpenAL_AO_Interface::resume() {
    alSourcePlayv(_channels, sources);
}

unsigned OpenAL_AO_Interface::get_space() {
    ALint queued;
    unqueue_buffers();
    alGetSourcei(sources[0], AL_BUFFERS_QUEUED, &queued);
    queued = NUM_BUF - queued - 3;
    if (queued < 0) return 0;
    return queued * CHUNK_SIZE * _channels;
}

/**
 * \brief write data into buffer and reset underrun flag
 */
unsigned OpenAL_AO_Interface::play(const any_t*data, unsigned len, unsigned flags) {
    ALint state;
    unsigned i, j, k;
    unsigned ch;
    const int16_t *d = reinterpret_cast<const int16_t*>(data);
    UNUSED(flags);
    len /= _channels * CHUNK_SIZE;
    for (i = 0; i < len; i++) {
	for (ch = 0; ch < _channels; ch++) {
	    for (j = 0, k = ch; j < CHUNK_SIZE / 2; j++, k += _channels)
		tmpbuf[j] = d[k];
	    alBufferData(buffers[ch][cur_buf[ch]], AL_FORMAT_MONO16, tmpbuf,
		     CHUNK_SIZE, _samplerate);
	    alSourceQueueBuffers(sources[ch], 1, &buffers[ch][cur_buf[ch]]);
	    cur_buf[ch] = (cur_buf[ch] + 1) % NUM_BUF;
	}
	d += _channels * CHUNK_SIZE / 2;
    }
    alGetSourcei(sources[0], AL_SOURCE_STATE, &state);
    if (state != AL_PLAYING) // checked here in case of an underrun
	alSourcePlayv(_channels, sources);
    return len * _channels * CHUNK_SIZE;
}

float OpenAL_AO_Interface::get_delay() {
    ALint queued;
    unqueue_buffers();
    alGetSourcei(sources[0], AL_BUFFERS_QUEUED, &queued);
    return queued * CHUNK_SIZE / 2 / (float)_samplerate;
}

unsigned OpenAL_AO_Interface::samplerate() const { return _samplerate; }
unsigned OpenAL_AO_Interface::channels() const { return _channels; }
unsigned OpenAL_AO_Interface::format() const { return _format; }
unsigned OpenAL_AO_Interface::buffersize() const { return _buffersize; }
unsigned OpenAL_AO_Interface::outburst() const { return _outburst; }
MPXP_Rc  OpenAL_AO_Interface::test_channels(unsigned c) const { return c>MAX_CHANS?MPXP_False:MPXP_Ok; }
MPXP_Rc  OpenAL_AO_Interface::test_rate(unsigned r) const { UNUSED(r); return MPXP_Ok; }
MPXP_Rc  OpenAL_AO_Interface::test_format(unsigned f) const { return f==AFMT_S16_NE?MPXP_Ok:MPXP_False; }

static AO_Interface* query_interface(const std::string& sd) { return new(zeromem) OpenAL_AO_Interface(sd); }

extern  const ao_info_t audio_out_openal = {
    "OpenAL audio output",
    "openal",
    "Reimar Döffinger <Reimar.Doeffinger@stud.uni-karlsruhe.de>",
    "",
    query_interface
};
} // namespace	usr
