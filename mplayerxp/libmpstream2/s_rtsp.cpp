#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;

#include <stdlib.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#ifndef HAVE_WINSOCK2
#include <sys/socket.h>
#define closesocket close
#else
#include <winsock2.h>
#endif

#include "stream.h"
#include "stream_internal.h"
#include "help_mp.h"
#include "url.h"
#include "tcp.h"
#include "network_rtsp.h"
#include "librtsp/rtsp.h"
#include "librtsp/rtsp_session.h"
#include "stream_msg.h"

namespace mpxp {
    class Rtsp_Stream_Interface : public Stream_Interface {
	public:
	    Rtsp_Stream_Interface(libinput_t& libinput);
	    virtual ~Rtsp_Stream_Interface();

	    virtual MPXP_Rc	open(const std::string& filename,unsigned flags);
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
	    Networking*		networking;
	    Tcp			tcp;
    };

Rtsp_Stream_Interface::Rtsp_Stream_Interface(libinput_t& _libinput)
			:Stream_Interface(_libinput),
			tcp(_libinput,-1) {}
Rtsp_Stream_Interface::~Rtsp_Stream_Interface() {}

int Rtsp_Stream_Interface::read(stream_packet_t*sp)
{
    return networking->read(tcp,sp->buf,sp->len);
}

off_t Rtsp_Stream_Interface::seek(off_t newpos) { return newpos; }
off_t Rtsp_Stream_Interface::tell() const { return 0; }

MPXP_Rc Rtsp_Stream_Interface::ctrl(unsigned cmd,any_t*args)
{
    UNUSED(cmd);
    UNUSED(args);
    return MPXP_Unknown;
}

void Rtsp_Stream_Interface::close()
{
    delete networking;
    networking=NULL;
}

MPXP_Rc Rtsp_Stream_Interface::open(const std::string& filename,unsigned flags)
{
    URL url;
    UNUSED(flags);
    if(filename.substr(0,7)!="rtsp://") return MPXP_False;

    MSG_V("STREAM_RTSP, URL: %s\n", filename.c_str());

    url.redirect (filename);
    url.check4proxies ();

    tcp.close();
    if ((networking=Rtsp_Networking::start(tcp,url,net_conf.bandwidth)) == NULL) {
	return MPXP_False;
    }

    networking->fixup_cache ();
    return MPXP_Ok;
}
Stream::type_e Rtsp_Stream_Interface::type() const { return Stream::Type_Stream; }
off_t	Rtsp_Stream_Interface::size() const { return 0; }
off_t	Rtsp_Stream_Interface::sector_size() const { return 1; }
std::string Rtsp_Stream_Interface::mime_type() const { return "application/octet-stream"; }

static Stream_Interface* query_interface(libinput_t& libinput) { return new(zeromem) Rtsp_Stream_Interface(libinput); }

/* "reuse a bit of code from ftplib written by Thomas Pfau", */
extern const stream_interface_info_t rtsp_stream =
{
    "rtsp",
    "reads multimedia stream from Real Time Streaming Protocol (RTSP)",
    query_interface
};
} // namespace mpxp
