#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
/*
    s_cdd - cdda & cddb streams interface
*/
#include "mplayerxp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stream.h"
#include "stream_internal.h"
#include "stream_msg.h"

#ifdef HAVE_LIBCDIO
#include "cdd.h"
#include "mrl.h"

namespace mpxp {
    class Cdda_Stream_Interface : public Stream_Interface {
	public:
	    Cdda_Stream_Interface(libinput_t* libinput);
	    virtual ~Cdda_Stream_Interface();

	    virtual MPXP_Rc	open(const char *filename,unsigned flags);
	    virtual int		read(stream_packet_t * sp);
	    virtual off_t	seek(off_t off);
	    virtual off_t	tell() const;
	    virtual void	close();
	    virtual MPXP_Rc	ctrl(unsigned cmd,any_t* param);
	    virtual Stream::type_e type() const;
	    virtual off_t	start_pos() const;
	    virtual off_t	size() const;
	    virtual off_t	sector_size() const;
	    virtual std::string mime_type() const;
	protected:
	    cdda_priv*		priv;
	private:
	    track_t		track_idx;
    };

Cdda_Stream_Interface::Cdda_Stream_Interface(libinput_t* libinput)
			:Stream_Interface(libinput),
			track_idx(255) {}
Cdda_Stream_Interface::~Cdda_Stream_Interface() {}

MPXP_Rc Cdda_Stream_Interface::open(const char *filename,unsigned flags)
{
    const char *param;
    char *device;
    UNUSED(flags);
    if(strcmp(filename,"help") == 0) {
	MSG_HINT("Usage: cdda://<@device><#trackno>\n");
	return MPXP_False;
    }
    param=mrl_parse_line(filename,NULL,NULL,&device,NULL);
    priv = open_cdda(device ? device : DEFAULT_CDROM_DEVICE,param);
    if(device) delete device;
    return priv?MPXP_Ok:MPXP_False;
}

Stream::type_e Cdda_Stream_Interface::type() const { return Stream::Type_RawAudio|Stream::Type_Seekable; }
off_t	Cdda_Stream_Interface::start_pos() const { return cdda_start(priv); }
off_t	Cdda_Stream_Interface::size() const { return cdda_size(priv); }
off_t	Cdda_Stream_Interface::sector_size() const { return CD_FRAMESIZE_RAW; }
std::string Cdda_Stream_Interface::mime_type() const { return "audio/PCMA"; }

int Cdda_Stream_Interface::read(stream_packet_t*sp)
{
    sp->type=0;
    sp->len=read_cdda(priv,sp->buf,&track_idx);
    return sp->len;
}

off_t Cdda_Stream_Interface::seek(off_t pos)
{
    seek_cdda(priv,pos,&track_idx);
    return pos;
}

off_t Cdda_Stream_Interface::tell() const
{
    return tell_cdda(priv);
}

void Cdda_Stream_Interface::close()
{
    close_cdda(priv);
}

MPXP_Rc Cdda_Stream_Interface::ctrl(unsigned cmd,any_t*args)
{
    switch(cmd) {
	case SCTRL_TXT_GET_STREAM_NAME: {
	    if(track_idx!=255)
		sprintf((char *)args,"Track %d",track_idx);
	    return MPXP_Ok;
	}
	break;
	case SCTRL_AUD_GET_CHANNELS:
	    *(int *)args=cdio_cddap_track_channels(priv->cd, track_idx);
	    if(*(int *)args<=0) *(int *)args=2;
	    MSG_V("cdda channels: %u\n",*(int *)args);
	    return MPXP_Ok;
	case SCTRL_AUD_GET_SAMPLERATE:
	    *(int *)args = 44100;
	    return MPXP_Ok;
	case SCTRL_AUD_GET_SAMPLESIZE:
	    *(int *)args=2;
	    return MPXP_Ok;
	case SCTRL_AUD_GET_FORMAT:
	    *(int *)args=0x01; /* Raw PCM */
	    return MPXP_Ok;
	default: break;
    }
    return MPXP_False;
}

static Stream_Interface* query_cdda_interface(libinput_t* libinput) { return new(zeromem) Cdda_Stream_Interface(libinput); }

extern const stream_interface_info_t cdda_stream =
{
    "cdda://",
    "reads multimedia stream directly from Digital Audio Compact Disc [CD-DA]",
    query_cdda_interface
};

    class Cddb_Stream_Interface : public Cdda_Stream_Interface {
	public:
	    Cddb_Stream_Interface(libinput_t* libinput);
	    virtual ~Cddb_Stream_Interface();

	    virtual MPXP_Rc	open(const char *filename,unsigned flags);
	private:
	    libinput_t*		libinput;
    };
Cddb_Stream_Interface::Cddb_Stream_Interface(libinput_t*_libinput)
			:Cdda_Stream_Interface(_libinput),
			libinput(_libinput) {}
Cddb_Stream_Interface::~Cddb_Stream_Interface() {}

MPXP_Rc Cddb_Stream_Interface::open(const char *filename,unsigned flags)
{
#ifdef HAVE_STREAMING
    const char *param;
    char *device;
    MPXP_Rc retval;
    UNUSED(flags);
    if(strcmp(filename,"help") == 0) {
	MSG_HINT("Usage: cddb://<@device><#trackno>\n");
	return MPXP_False;
    }
    param=mrl_parse_line(filename,NULL,NULL,&device,NULL);
    priv = open_cddb(libinput,device ? device : DEFAULT_CDROM_DEVICE,param);
    if(device) delete device;
    return priv?MPXP_Ok:MPXP_False;
#else
    return MPXP_False;
#endif
}

static Stream_Interface* query_cddb_interface(libinput_t* libinput) { return new(zeromem) Cddb_Stream_Interface(libinput); }
extern const stream_interface_info_t cddb_stream =
{
    "cddb://",
    "reads multimedia stream from CD-DA but tracks names from CDDB servers",
    query_cddb_interface
};
#endif
} // namespace mpxp