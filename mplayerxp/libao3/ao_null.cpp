#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/time.h>

#include <limits>

#include "osdep/bswap.h"

#include "libmpstream2/mrl.h"
#include "afmt.h"
#include "audio_out.h"
#include "audio_out_internal.h"
#include "ao_msg.h"

namespace mpxp {
#define WAV_ID_RIFF FOURCC_TAG(0x46,0x46,0x49,0x52) /* "RIFF" */
#define WAV_ID_WAVE FOURCC_TAG(0x45,0x56,0x41,0x57) /* "WAVE" */
#define WAV_ID_FMT  FOURCC_TAG(0x20,0x74,0x6d,0x66) /* "fmt " */
#define WAV_ID_DATA FOURCC_TAG(0x61,0x74,0x61,0x64) /* "data" */
#define WAV_ID_PCM  TWOCC_TAG (0x00,0x01)
struct WaveHeader {
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

/* init with default values */
static struct WaveHeader wavhdr = {
    WAV_ID_RIFF,
    /* same conventions than in sox/wav.c/wavwritehdr() */
    0, //le2me_32(0x7ffff024),
    WAV_ID_WAVE,
    WAV_ID_FMT,
    16,
    WAV_ID_PCM,
    2,
    44100,
    192000,
    4,
    16,
    WAV_ID_DATA,
    0, //le2me_32(0x7ffff000)
};

class Null_AO_Interface : public AO_Interface {
    public:
	Null_AO_Interface(const std::string& subdevice);
	virtual ~Null_AO_Interface();

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
	void		drain();

	struct		timeval last_tv;
	int		buffer;
	FILE*		fd;
	int		fast_mode;
	int		wav_mode;
};

Null_AO_Interface::Null_AO_Interface(const std::string& _subdevice)
		:AO_Interface(_subdevice) {}
Null_AO_Interface::~Null_AO_Interface() {
    if(fd && wav_mode && fseeko(fd, 0, SEEK_SET) == 0){ /* Write wave header */
	wavhdr.file_length = wavhdr.data_length + sizeof(wavhdr) - 8;
	wavhdr.file_length = le2me_32(wavhdr.file_length);
	wavhdr.data_length = le2me_32(wavhdr.data_length);
	::fwrite(&wavhdr,sizeof(wavhdr),1,fd);
    }
    if(fd) ::fclose(fd);
}

void Null_AO_Interface::drain() {
    struct timeval now_tv;
    int temp, temp2;

    ::gettimeofday(&now_tv, 0);
    temp = now_tv.tv_sec - last_tv.tv_sec;
    temp *= bps();

    temp2 = now_tv.tv_usec - last_tv.tv_usec;
    temp2 /= 1000;
    temp2 *= bps();
    temp2 /= 1000;
    temp += temp2;

    buffer-=temp;
    if (buffer<0) buffer=0;

    if(temp>0) last_tv = now_tv;//mplayer is fast
}

// to set/get/query special features/parameters
MPXP_Rc Null_AO_Interface::ctrl(int cmd,long arg) const {
    UNUSED(cmd);
    UNUSED(arg);
    return MPXP_True;
}

// open & setup audio device
// return: 1=success 0=fail
MPXP_Rc Null_AO_Interface::open(unsigned flags) {
    char *null_dev=NULL,*mode=NULL;
    UNUSED(flags);
    if (!subdevice.empty()) {
	mrl_parse_line(subdevice,NULL,NULL,&null_dev,&mode);
	fd=NULL;
	if(null_dev) fd = ::fopen(null_dev, "wb");
	if(::strcmp(mode,"wav")==0) wav_mode=1;
    } //end parsing subdevice
    return MPXP_Ok;
}

MPXP_Rc Null_AO_Interface::configure(unsigned r,unsigned c,unsigned f){
    unsigned bits;
    _buffersize= 0xFFFFF;
    _outburst=0xFFFF;//4096;
    _channels=c;
    _samplerate=r;
    _format=f;
    bits=8;
    switch(_format) {
	case AFMT_S16_LE:
	case AFMT_U16_LE:
	case AFMT_S16_BE:
	case AFMT_U16_BE:
	    bits=16;
	    break;
	case AFMT_S32_LE:
	case AFMT_S32_BE:
	case AFMT_U32_LE:
	case AFMT_U32_BE:
	case AFMT_FLOAT32:
	    bits=32;
	    break;
	case AFMT_S24_LE:
	case AFMT_S24_BE:
	case AFMT_U24_LE:
	case AFMT_U24_BE:
	    bits=24;
	    break;
	default:
	    break;
    }
    buffer=0;
    gettimeofday(&last_tv, 0);
    if(fd && wav_mode) {
	wavhdr.channels = le2me_16(_channels);
	wavhdr.sample_rate = le2me_32(_samplerate);
	wavhdr.bytes_per_second = le2me_32(afmt2bps(_format));
	wavhdr.bits = le2me_16(bits);
	wavhdr.block_align = le2me_16(_channels * (bits / 8));
	wavhdr.data_length=le2me_32(0x7ffff000);
	wavhdr.file_length = wavhdr.data_length + sizeof(wavhdr) - 8;

	::fwrite(&wavhdr,sizeof(wavhdr),1,fd);
	wavhdr.file_length=wavhdr.data_length=0;
    }
    return MPXP_Ok;
}

// stop playing and empty priv->buffers (for seeking/pause)
void Null_AO_Interface::reset() {
    buffer=0;
}

// stop playing, keep priv->buffers (for pause)
void Null_AO_Interface::pause() {
    // for now, just call reset();
    reset();
}

// resume playing, after audio_pause()
void Null_AO_Interface::resume() { }

// return: how many bytes can be played without blocking
unsigned Null_AO_Interface::get_space() {
    drain();
    return fast_mode?std::numeric_limits<int>::max():_outburst-buffer;
}

// plays 'len' bytes of 'data'
// it should round it down to outburst*n
// return: number of bytes played
unsigned Null_AO_Interface::play(const any_t* data,unsigned len,unsigned flags) {
    unsigned maxbursts = (_buffersize - buffer) / _outburst;
    unsigned playbursts = len / _outburst;
    unsigned bursts = playbursts > maxbursts ? maxbursts : playbursts;
    buffer += bursts * _outburst;
    UNUSED(flags);
    if(fd && len) {
	MSG_DBG2("writing %u bytes into file\n",len);
	::fwrite(data,len,1,fd);
	wavhdr.data_length += len;
    }
    return fast_mode?bursts*_outburst:len;
}

// return: delay in seconds between first and last sample in priv->buffer
float Null_AO_Interface::get_delay() {
    drain();
    return fast_mode?0.0:(float) buffer / (float) bps();
}

unsigned Null_AO_Interface::samplerate() const { return _samplerate; }
unsigned Null_AO_Interface::channels() const { return _channels; }
unsigned Null_AO_Interface::format() const { return _format; }
unsigned Null_AO_Interface::buffersize() const { return _buffersize; }
unsigned Null_AO_Interface::outburst() const { return _outburst; }
MPXP_Rc  Null_AO_Interface::test_channels(unsigned c) const { UNUSED(c); return MPXP_Ok; }
MPXP_Rc  Null_AO_Interface::test_rate(unsigned r) const { UNUSED(r); return MPXP_Ok; }
MPXP_Rc  Null_AO_Interface::test_format(unsigned f) const { UNUSED(f); return MPXP_Ok; }

static AO_Interface* query_interface(const std::string& sd) { return new(zeromem) Null_AO_Interface(sd); }

extern const ao_info_t audio_out_null = {
    "Null audio output",
    "null",
    "Tobias Diedrich",
    "",
    query_interface
};
} // namespace mpxp
