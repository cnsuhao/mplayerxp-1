#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
/*
 * PCM audio output driver
 *
 * This file is part of MPlayerXP.
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xmpcore/xmp_core.h"

#include "osdep/bswap.h"
#include "postproc/af.h"
#include "afmt.h"
#include "audio_out.h"
#include "audio_out_internal.h"
#include "libvo2/video_out.h"
#include "help_mp.h"
#include "ao_msg.h"

#ifdef __MINGW32__
// for GetFileType to detect pipes
#include <windows.h>
#endif

namespace mpxp {
#define WAV_ID_RIFF 0x46464952 /* "RIFF" */
#define WAV_ID_WAVE 0x45564157 /* "WAVE" */
#define WAV_ID_FMT  0x20746d66 /* "fmt " */
#define WAV_ID_DATA 0x61746164 /* "data" */
#define WAV_ID_PCM  0x0001
#define WAV_ID_FLOAT_PCM  0x0003

struct WaveHeader
{
    uint32_t riff;
    uint32_t file_length;
    uint32_t wave;
    uint32_t fmt;
    uint32_t fmt_length;
    uint16_t fmt_tag;
    uint16_t channels;
    uint32_t sample_rate;
    uint32_t bytes_per_second;
    uint16_t block_align;
    uint16_t bits;
    uint32_t data;
    uint32_t data_length;
};
class Wave_AO_Interface : public AO_Interface {
    public:
	Wave_AO_Interface(const std::string& subdevice);
	virtual ~Wave_AO_Interface();

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
	unsigned	bps() const { return _channels*_samplerate*afmt2bps(_format); }
	std::string	out_filename;
	int		pcm_waveheader;
	int		fast;

	uint64_t	data_length;
	FILE*		fp;
	WaveHeader	wavhdr;
	unsigned	_channels,_samplerate,_format;
	unsigned	_buffersize,_outburst;
};

Wave_AO_Interface::Wave_AO_Interface(const std::string& _subdevice)
		:AO_Interface(_subdevice) {}
Wave_AO_Interface::~Wave_AO_Interface() {
    if(pcm_waveheader){ /* Rewrite wave header */
	int broken_seek = 0;
#ifdef __MINGW32__
	// Windows, in its usual idiocy "emulates" seeks on pipes so it always looks
	// like they work. So we have to detect them brute-force.
	broken_seek = GetFileType((HANDLE)_get_osfhandle(_fileno(fp))) != FILE_TYPE_DISK;
#endif
	if (broken_seek || fseek(fp, 0, SEEK_SET) != 0)
	    MSG_ERR("Could not seek to start, WAV size headers not updated!\n");
	else if (data_length > 0x7ffff000)
	    MSG_ERR("File larger than allowed for WAV files, may play truncated!\n");
	else {
	    wavhdr.file_length = data_length + sizeof(wavhdr) - 8;
	    wavhdr.file_length = le2me_32(wavhdr.file_length);
	    wavhdr.data_length = le2me_32(data_length);
	    ::fwrite(&wavhdr,sizeof(wavhdr),1,fp);
	}
    }
    ::fclose(fp);
}

// to set/get/query special features/parameters
MPXP_Rc Wave_AO_Interface::ctrl(int cmd,long arg) const {
    UNUSED(cmd);
    UNUSED(arg);
    return MPXP_False;
}

// open & setup audio device
// return: 1=success 0=fail
MPXP_Rc Wave_AO_Interface::open(unsigned flags) {
    // set defaults
    UNUSED(flags);
    pcm_waveheader=1;
    return MPXP_Ok;
}

MPXP_Rc Wave_AO_Interface::configure(unsigned r,unsigned c,unsigned f){
    unsigned bits;

    if(!subdevice.empty())	out_filename = subdevice;
    else			out_filename = "mpxp_adump.wav";

    bits=8;
    switch(f){
    case AFMT_S32_BE:
	f=AFMT_S32_LE;
    case AFMT_S32_LE:
	bits=32;
	break;
    case AFMT_FLOAT32:
	bits=32;
	break;
    case AFMT_S8:
	f=AFMT_U8;
    case AFMT_U8:
	break;
    case AFMT_AC3:
	bits=16;
	break;
    default:
	f=AFMT_S16_LE;
	bits=16;
	break;
    }

    _outburst = 65536;
    _buffersize= 2*65536;
    _channels=c;
    _samplerate=r;
    _format=f;

    wavhdr.riff = le2me_32(WAV_ID_RIFF);
    wavhdr.wave = le2me_32(WAV_ID_WAVE);
    wavhdr.fmt = le2me_32(WAV_ID_FMT);
    wavhdr.fmt_length = le2me_32(16);
    wavhdr.fmt_tag = le2me_16(_format == AFMT_FLOAT32 ? WAV_ID_FLOAT_PCM : WAV_ID_PCM);
    wavhdr.channels = le2me_16(_channels);
    wavhdr.sample_rate = le2me_32(_samplerate);
    wavhdr.bytes_per_second = le2me_32(bps());
    wavhdr.bits = le2me_16(bits);
    wavhdr.block_align = le2me_16(_channels * (bits / 8));

    wavhdr.data = le2me_32(WAV_ID_DATA);
    wavhdr.data_length=le2me_32(0x7ffff000);
    wavhdr.file_length = wavhdr.data_length + sizeof(wavhdr) - 8;

    MSG_INFO("ao_wav: %s %d-%s %s\n"
		,out_filename.c_str()
		,_samplerate
		,(_channels > 1) ? "Stereo" : "Mono"
		,afmt2str(_format));

    fp = ::fopen(out_filename.c_str(), "wb");
    if(fp) {
	if(pcm_waveheader){ /* Reserve space for wave header */
	    ::fwrite(&wavhdr,sizeof(wavhdr),1,fp);
	}
	return MPXP_Ok;
    }
    MSG_ERR("ao_wav: can't open output file: %s\n", out_filename.c_str());
    return MPXP_False;
}

// stop playing and empty buffers (for seeking/pause)
void Wave_AO_Interface::reset(){}

// stop playing, keep buffers (for pause)
void Wave_AO_Interface::pause() { reset(); }

// resume playing, after audio_pause()
void Wave_AO_Interface::resume() {}

// return: how many bytes can be played without blocking
unsigned Wave_AO_Interface::get_space() {
    float pts=dae_played_frame(mpxp_context().engine().xp_core->video).v_pts;
    if(pts)
	return mpxp_context().audio().output->pts < pts + fast * 30000 ? _outburst : 0;
    return _outburst;
}

// plays 'len' bytes of 'data'
// it should round it down to outburst*n
// return: number of bytes played
unsigned Wave_AO_Interface::play(const any_t* data,unsigned len,unsigned flags){
    UNUSED(flags);
    ::fwrite(data,len,1,fp);
    if(pcm_waveheader) data_length += len;
    return len;
}

// return: delay in seconds between first and last sample in buffer
float Wave_AO_Interface::get_delay() { return 0.0f; }

unsigned Wave_AO_Interface::samplerate() const { return _samplerate; }
unsigned Wave_AO_Interface::channels() const { return _channels; }
unsigned Wave_AO_Interface::format() const { return _format; }
unsigned Wave_AO_Interface::buffersize() const { return _buffersize; }
unsigned Wave_AO_Interface::outburst() const { return _outburst; }
MPXP_Rc  Wave_AO_Interface::test_channels(unsigned c) const { UNUSED(c); return MPXP_Ok; }
MPXP_Rc  Wave_AO_Interface::test_rate(unsigned r) const { UNUSED(r); return MPXP_Ok; }
MPXP_Rc  Wave_AO_Interface::test_format(unsigned f) const { UNUSED(f); return MPXP_Ok; }

static AO_Interface* query_interface(const std::string& sd) { return new(zeromem) Wave_AO_Interface(sd); }

extern const ao_info_t audio_out_wav =
{
    "RAW WAVE file writer audio output",
    "wav",
    "Atmosfear",
    "",
    query_interface
};
} // namesapce mpxp

