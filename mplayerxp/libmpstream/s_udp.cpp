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
#include "udp.h"
#include "url.h"
#include "stream_msg.h"

namespace mpxp {
    class Udp_Stream_Interface : public Stream_Interface {
	public:
	    Udp_Stream_Interface();
	    virtual ~Udp_Stream_Interface();

	    virtual MPXP_Rc	open(libinput_t* libinput,const char *filename,unsigned flags);
	    virtual int		read(stream_packet_t * sp);
	    virtual off_t	seek(off_t off);
	    virtual off_t	tell() const;
	    virtual void	close();
	    virtual MPXP_Rc	ctrl(unsigned cmd,any_t* param);
	    virtual stream_type_e type() const;
	    virtual off_t	size() const;
	    virtual off_t	sector_size() const;
	private:
	    int 		start ();

	    networking_t*	networking;
	    net_fd_t		fd;
    };

Udp_Stream_Interface::Udp_Stream_Interface() {}
Udp_Stream_Interface::~Udp_Stream_Interface() {}

int Udp_Stream_Interface::read(stream_packet_t*sp)
{
  return nop_networking_read(fd,sp->buf,sp->len,networking);
}

off_t Udp_Stream_Interface::seek(off_t newpos) { return newpos; }
off_t Udp_Stream_Interface::tell() const { return 0; }

MPXP_Rc Udp_Stream_Interface::ctrl(unsigned cmd,any_t*args)
{
    UNUSED(cmd);
    UNUSED(args);
    return MPXP_Unknown;
}

void Udp_Stream_Interface::close()
{
    url_free(networking->url);
    free_networking(networking);
}

int Udp_Stream_Interface::start ()
{
    if (fd < 0) {
	fd = udp_open_socket (networking->url);
	if (fd < 0) return -1;
    }
    networking->networking_read = nop_networking_read;
    networking->networking_seek = nop_networking_seek;
    networking->prebuffer_size = 64 * 1024; /* 64 KBytes */
    networking->buffering = 0;
    networking->status = networking_playing_e;
    return 0;
}

extern int network_bandwidth;
MPXP_Rc Udp_Stream_Interface::open(libinput_t* libinput,const char *filename,unsigned flags)
{
    URL_t *url;
    UNUSED(flags);
    MSG_V("STREAM_UDP, URL: %s\n", filename);
    networking = new_networking(libinput);
    if (!networking) return MPXP_False;

    networking->bandwidth = network_bandwidth;
    url = url_new (filename);
    networking->url = check4proxies (url);
    if (url->port == 0) {
	MSG_ERR("You must enter a port number for UDP streams!\n");
	free_networking (networking);
	networking = NULL;
	return MPXP_False;
    }
    if (start () < 0) {
	MSG_ERR("udp_networking_start failed\n");
	free_networking (networking);
	networking = NULL;
	return MPXP_False;
    }
    fixup_network_stream_cache (networking);
    return MPXP_Ok;
}
stream_type_e Udp_Stream_Interface::type() const { return STREAMTYPE_STREAM; }
off_t	Udp_Stream_Interface::size() const { return 0; }
off_t	Udp_Stream_Interface::sector_size() const { return 1; }

static Stream_Interface* query_interface() { return new(zeromem) Udp_Stream_Interface; }

/* "reuse a bit of code from ftplib written by Thomas Pfau", */
extern const stream_interface_info_t rtsp_stream =
{
    "udp://",
    "reads multimedia stream directly from User Datagram Protocol (UDP)",
    query_interface
};
} // namespace mpxp

