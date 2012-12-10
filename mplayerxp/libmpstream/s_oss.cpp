#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
/*
    s_oss - stream interface for oss capturing.
*/
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#include "mplayerxp.h"
#include "xmpcore/mp_aframe.h"
#include "postproc/af.h"
#include "libao2/afmt.h"
#include "libao2/audio_out.h"
#include "loader/wine/mmreg.h"
#include "stream.h"
#include "stream_internal.h"
#include "mrl.h"
#include "stream_msg.h"

namespace mpxp {
    class Oss_Stream_Interface : public Stream_Interface {
	public:
	    Oss_Stream_Interface(libinput_t* libinput);
	    virtual ~Oss_Stream_Interface();

	    virtual MPXP_Rc	open(const char *filename,unsigned flags);
	    virtual int		read(stream_packet_t * sp);
	    virtual off_t	seek(off_t off);
	    virtual off_t	tell() const;
	    virtual void	close();
	    virtual MPXP_Rc	ctrl(unsigned cmd,any_t* param);
	    virtual Stream::type_e type() const;
	    virtual off_t	size() const;
	    virtual off_t	sector_size() const;
	    virtual std::string mime_type() const;
	private:
	    int		fd;
	    unsigned	nchannels; /* 1,2,6 */
	    unsigned	samplerate; /* 32000, 44100, 48000 */
	    mpaf_format_e sampleformat; /* S32_LE, S16_BE, ... */
	    off_t	spos;
	    unsigned	_sector_size;
    };

Oss_Stream_Interface::Oss_Stream_Interface(libinput_t* libinput)
		    :Stream_Interface(libinput) {}
Oss_Stream_Interface::~Oss_Stream_Interface() {}

MPXP_Rc Oss_Stream_Interface::open(const char *filename,unsigned flags)
{
    char *args;
    char *oss_device,*comma;
    unsigned tmp,param;
    int err;
    UNUSED(flags);
    if(strcmp(filename,"help") == 0) {
	MSG_HINT("Usage: oss://<@device>#<channels>,<samplerate>,<sampleformat>\n");
	return MPXP_False;
    }
    args=mp_strdup(mrl_parse_line(filename,NULL,NULL,&oss_device,NULL));
    comma=strchr(args,',');
    if(comma) *comma=0;
    nchannels=args[0]?atoi(args):2;
    if(comma) args=comma+1;
    comma=strchr(args,',');
    if(comma) *comma=0;
    samplerate=args[0]?atoi(args):44100;
    if(comma) args=comma+1;
    comma=strchr(args,',');
    if(comma) *comma=0;
    if(args[0])
	sampleformat=mpaf_str2fmt(args);
    else {
	/* Default to S16_NE */
	sampleformat=MPAF_NE|MPAF_SI|MPAF_I|MPAF_BPS_2;
    }
    fd = ::open(oss_device?oss_device:PATH_DEV_DSP,O_RDONLY);
    if(fd<0) { delete args; return MPXP_False; }
    ::ioctl(fd, SNDCTL_DSP_RESET, NULL);
//    ioctl(stream->fd, SNDCTL_DSP_SYNC, NULL);
    spos=0;
    /* Configure OSS */
    tmp = samplerate;
    err=0;
    if (::ioctl(fd, SNDCTL_DSP_SPEED, &samplerate)<0)
	MSG_ERR("[s_oss] Can't set samplerate to %u (will use %u)\n",tmp,samplerate);
    else
	MSG_DBG2("[o_oss] Did set samplerate to %u\n",samplerate);
    tmp = nchannels;
    if(tmp>2)
	err=::ioctl(fd, SNDCTL_DSP_CHANNELS, &nchannels);
    else {
	param=(nchannels==2?1:0);
	err=::ioctl(fd, SNDCTL_DSP_STEREO, &param);
	nchannels=param?2:1;
    }
    if(err<0) MSG_ERR("[s_oss] Can't set channels to %u (will use %u)\n",tmp,nchannels);
    else MSG_DBG2("[o_oss] Did set channels to %u\n",nchannels);
    mp_aframe_t afd;
    int oss_fmt;
    afd.rate=samplerate;
    afd.nch=nchannels;
    afd.format=sampleformat;
    oss_fmt=mpaf2afmt(sampleformat);
    tmp=oss_fmt;
    if(::ioctl(fd, SNDCTL_DSP_SETFMT, &oss_fmt)<0)
	MSG_ERR("[s_oss] Can't set format %s (will use %s)\n",ao_format_name(tmp),ao_format_name(oss_fmt));
    else
	MSG_DBG2("[o_oss] Did set format to %s\n",ao_format_name(oss_fmt));
    tmp = PCM_ENABLE_INPUT;
    if(::ioctl(fd, SNDCTL_DSP_SETTRIGGER, &tmp)<0)
	MSG_ERR("[s_oss] Can't enable input\n");
    else
	MSG_DBG2("[o_oss] Did set trigger to %u\n",tmp);
    _sector_size = 0;
    err = ::ioctl(fd, SNDCTL_DSP_GETBLKSIZE, &_sector_size);
    if (err < 0)
	MSG_ERR("[s_oss] Can't get blocksize\n");
    else
	MSG_DBG2("[o_oss] Did get blocksize as %u\n",_sector_size);
    // correct the blocksize to a reasonable value
    if (_sector_size <= 0) {
	_sector_size = 4096*nchannels*(sampleformat&MPAF_BPS_MASK);
    } else if (_sector_size < 4096*nchannels*(sampleformat&MPAF_BPS_MASK)) {
	_sector_size *= 4096*nchannels*(sampleformat&MPAF_BPS_MASK)/_sector_size;
    }
    MSG_DBG2("[o_oss] Correct blocksize as %u\n",_sector_size);
    delete args;
    return MPXP_Ok;
}
Stream::type_e Oss_Stream_Interface::type() const { return Stream::Type_Stream|Stream::Type_RawAudio; }
off_t	Oss_Stream_Interface::size() const { return -1; }
off_t	Oss_Stream_Interface::sector_size() const { return _sector_size; }
std::string Oss_Stream_Interface::mime_type() const { return "audio/PCMA"; }

#ifndef TEMP_FAILURE_RETRY
#define TEMP_FAILURE_RETRY(x) (x)
#endif

int Oss_Stream_Interface::read(stream_packet_t*sp)
{
/* Should we repeate read() again on these errno: `EAGAIN', `EIO' ??? */
    sp->type=0;
    sp->len = TEMP_FAILURE_RETRY(::read(fd,sp->buf,sp->len));
    if(!errno) spos+=sp->len;
    return sp->len;
}

off_t Oss_Stream_Interface::seek(off_t pos)
{
    UNUSED(pos);
    errno=ENOSYS;
    return spos;
}

off_t Oss_Stream_Interface::tell() const
{
    return spos;
}

void Oss_Stream_Interface::close()
{
    ::ioctl(fd, SNDCTL_DSP_RESET, NULL);
    ::close(fd);
}

MPXP_Rc Oss_Stream_Interface::ctrl(unsigned cmd,any_t*args)
{
    int rval;
    if(args) *(int *)args=0;
    switch(cmd) {
	case SCTRL_AUD_GET_CHANNELS:
	    rval = nchannels;
	    if (rval > 2) {
		if ( ::ioctl(fd, SNDCTL_DSP_CHANNELS, &rval) == -1 ||
		(unsigned)rval != nchannels ) return MPXP_False;
		*(int *)args=rval;
		return MPXP_Ok;
	    }
	    else {
		int c = rval-1;
		if (::ioctl (fd, SNDCTL_DSP_STEREO, &c) == -1) return MPXP_False;
		*(int *)args=c+1;
		return MPXP_Ok;
	    }
	    break;
	case SCTRL_AUD_GET_SAMPLERATE:
	    rval=samplerate;
	    if (::ioctl(fd, SNDCTL_DSP_SPEED, &rval) != -1) {
		*(int *)args = rval;
		return MPXP_Ok;
	    }
	    return MPXP_False;
	    break;
	case SCTRL_AUD_GET_SAMPLESIZE:
	    *(int *)args=2;
	    if (::ioctl (fd, SNDCTL_DSP_GETFMTS, &rval) != -1) {
		switch(rval) {
		    case AFMT_MU_LAW:
		    case AFMT_A_LAW:
		    case AFMT_IMA_ADPCM:
		    case AFMT_MPEG:
		    case AFMT_AC3:
		    case AFMT_U8:
		    case AFMT_S8:
			*(int *)args=1;
			return MPXP_Ok;
		    default:
		    case AFMT_S16_LE:
		    case AFMT_S16_BE:
		    case AFMT_U16_LE:
		    case AFMT_U16_BE:
			*(int *)args=2;
			return MPXP_Ok;
		    case AFMT_S24_LE:
		    case AFMT_S24_BE:
		    case AFMT_U24_LE:
		    case AFMT_U24_BE:
			*(int *)args=3;
			return MPXP_Ok;
		    case AFMT_S32_LE:
		    case AFMT_S32_BE:
		    case AFMT_U32_LE:
		    case AFMT_U32_BE:
			*(int *)args=4;
			return MPXP_Ok;
		}
		break;
	    }
	    return MPXP_False;
	case SCTRL_AUD_GET_FORMAT:
	    *(int *)args=0x01; /* Raw PCM */
	    if (::ioctl (fd, SNDCTL_DSP_GETFMTS, &rval) != -1) {
		switch(rval) {
		    case AFMT_MU_LAW: *(int *)args=WAVE_FORMAT_MULAW; return MPXP_Ok;
		    case AFMT_A_LAW:  *(int *)args=WAVE_FORMAT_ALAW; return MPXP_Ok;
		    case AFMT_IMA_ADPCM: *(int *)args=WAVE_FORMAT_ADPCM; return MPXP_Ok;
		    case AFMT_MPEG: *(int *)args=WAVE_FORMAT_MPEG; return MPXP_Ok; /* 0x55? */
		    case AFMT_AC3:  *(int *)args=0x2000; return MPXP_Ok;
		    default:
		    case AFMT_U8:
		    case AFMT_S8:
		    case AFMT_S16_LE:
		    case AFMT_S16_BE:
		    case AFMT_U16_LE:
		    case AFMT_U16_BE:
		    case AFMT_S24_LE:
		    case AFMT_S24_BE:
		    case AFMT_U24_LE:
		    case AFMT_U24_BE:
		    case AFMT_S32_LE:
		    case AFMT_S32_BE:
		    case AFMT_U32_LE:
		    case AFMT_U32_BE: *(int *)args=0x01;/* Raw PCM */ return MPXP_Ok;
		}
		break;
	    }
	    return MPXP_False;
	default:
	    break;
    }
    return MPXP_Unknown;
}

static Stream_Interface* query_interface(libinput_t* libinput) { return new(zeromem) Oss_Stream_Interface(libinput); }

extern const stream_interface_info_t oss_stream =
{
    "oss://",
    "reads multimedia stream from OSS audio capturing interface",
    query_interface
};
} // namespace mpxp
