#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
/*
 * aRts (KDE analogue Real-Time synthesizer) audio output driver for MPlayerXP
 *
 * copyright (c) 2002 Michele Balistreri <brain87@gmx.net>
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

#include <artsc.h>
#include <stdio.h>

#include "audio_out.h"
#include "audio_out_internal.h"
#include "postproc/af.h"
#include "afmt.h"
#include "ao_msg.h"

namespace mpxp {
/* Feel mp_free to experiment with the following values: */
#define ARTS_PACKETS 10 /* Number of audio packets */
#define ARTS_PACKET_SIZE_LOG2 11 /* Log2 of audio packet size */

class Arts_AO_Interface : public AO_Interface {
    public:
	Arts_AO_Interface(const std::string& subdevice);
	virtual ~Arts_AO_Interface();

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

	arts_stream_t	stream;
};

Arts_AO_Interface::Arts_AO_Interface(const std::string& _subdevice)
		:AO_Interface(_subdevice) {}
Arts_AO_Interface::~Arts_AO_Interface() {
    arts_close_stream(stream);
    arts_free();
}

MPXP_Rc Arts_AO_Interface::ctrl(int cmd, long arg) const {
    UNUSED(cmd);
    UNUSED(arg);
    return MPXP_Unknown;
}

MPXP_Rc Arts_AO_Interface::open(unsigned flags)
{
    int err;
    UNUSED(flags);

    if( (err=arts_init()) ) {
	MSG_ERR("[aRts] init failed: %s\n", arts_error_text(err));
	/*TODO: system("artsd -l0");*/
	return MPXP_False;
    }
    MSG_INFO("[aRts] connected to server\n");
    return MPXP_Ok;
}

MPXP_Rc Arts_AO_Interface::configure(unsigned r,unsigned c,unsigned f) {
    unsigned frag_spec,samplesize;
    /*
     * arts supports 8bit unsigned and 16bit signed sample formats
     * (16bit apparently in little endian format, even in the case
     * when artsd runs on a big endian cpu).
     *
     * Unsupported formats are translated to one of these two formats
     * using mplayer's audio filters.
     */
    _samplerate=r;
    _channels=c;
    _format=f;
    switch (f) {
	case AFMT_U8:
	case AFMT_S8:
	    _format = AFMT_U8;
	    samplesize=1;
	    break;
#if 0
	case AFMT_S24_LE:
	case AFMT_S24_BE:
	case AFMT_U24_LE:
	case AFMT_U24_BE:
	    _format = AFMT_S24_LE;
	    samplesize=3;
	    break;
	case AFMT_S32_LE:
	case AFMT_S32_BE:
	case AFMT_U32_LE:
	case AFMT_U32_BE:
	    _format = AFMT_S32_LE;
	    samplesize=4;
	    break;
#endif
	default:
	    samplesize=2;
	    _format = AFMT_S16_LE;    /* artsd always expects little endian?*/
	    break;
    }

    stream=arts_play_stream(_samplerate, samplesize*8, _channels, "MPlayerXP");

    if(stream == NULL) {
	MSG_ERR("[aRts] Can't open stream\n");
	arts_free();
	return MPXP_False;
    }

    /* Set the stream to blocking: it will not block anyway, but it seems */
    /* to be working better */
    arts_stream_set(stream, ARTS_P_BLOCKING, 1);
    frag_spec = ARTS_PACKET_SIZE_LOG2 | ARTS_PACKETS << 16;
    arts_stream_set(stream, ARTS_P_PACKET_SETTINGS, frag_spec);
    _buffersize = arts_stream_get(stream, ARTS_P_BUFFER_SIZE);
    MSG_INFO("[aRts] Stream opened\n");

    MSG_V("[aRts] buffersize=%u\n",_buffersize);
    MSG_V("[aRts] buffersize=%u\n", arts_stream_get(stream, ARTS_P_PACKET_SIZE));

    return MPXP_Ok;
}

unsigned Arts_AO_Interface::play(const any_t* data,unsigned len,unsigned flags)
{
    UNUSED(flags);
    return arts_write(stream, data, len);
}

void Arts_AO_Interface::pause() {}
void Arts_AO_Interface::resume() {}
void Arts_AO_Interface::reset() {}
unsigned Arts_AO_Interface::get_space() {
    return arts_stream_get(stream, ARTS_P_BUFFER_SPACE);
}

float Arts_AO_Interface::get_delay() {
    return ((float) (_buffersize - arts_stream_get(stream,
		ARTS_P_BUFFER_SPACE))) / ((float) bps());
}

unsigned Arts_AO_Interface::samplerate() const { return _samplerate; }
unsigned Arts_AO_Interface::channels() const { return _channels; }
unsigned Arts_AO_Interface::format() const { return _format; }
unsigned Arts_AO_Interface::buffersize() const { return _buffersize; }
unsigned Arts_AO_Interface::outburst() const { return _outburst; }
MPXP_Rc  Arts_AO_Interface::test_channels(unsigned c) const { UNUSED(c); return MPXP_Ok; }
MPXP_Rc  Arts_AO_Interface::test_rate(unsigned r) const { UNUSED(r); return MPXP_Ok; }
MPXP_Rc  Arts_AO_Interface::test_format(unsigned f) const {
    switch (f) {
	case AFMT_U8:
	case AFMT_S8:
	case AFMT_U16_LE:
	case AFMT_S16_LE:
	case AFMT_U16_BE:
	case AFMT_S16_BE: return MPXP_Ok;
	default: break;
    }
    return MPXP_False;
}

static AO_Interface* query_interface(const std::string& sd) { return new Arts_AO_Interface(sd); }

extern const ao_info_t audio_out_arts =
{
    "aRts audio output",
    "arts",
    "Michele Balistreri <brain87@gmx.net>",
    "",
    query_interface
};
} // namespace mpxp
