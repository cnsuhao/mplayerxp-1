#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
/*
    s_network - network stream inetrface
*/
#ifdef HAVE_STREAMING
#include <errno.h>
#include <stdlib.h>
#ifndef __USE_GNU
#define __USE_GNU
#endif
#include <unistd.h>
#include "stream.h"
#include "stream_internal.h"
#include "help_mp.h"
#include "stream_msg.h"

#include "url.h"
#include "tcp.h"
#include "network.h"

namespace mpxp {
    class Network_Stream_Interface : public Stream_Interface {
	public:
	    Network_Stream_Interface(libinput_t& libinput);
	    virtual ~Network_Stream_Interface();

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
	    URL*		url;
	    off_t		spos;
	    Tcp			tcp;
	    networking_t*	networking;
    };

Network_Stream_Interface::Network_Stream_Interface(libinput_t& libinput)
			:Stream_Interface(libinput),
			tcp(libinput,-1) {}
Network_Stream_Interface::~Network_Stream_Interface() {
    if(url) delete url;
}

MPXP_Rc Network_Stream_Interface::open(const std::string& filename,unsigned flags)
{
    UNUSED(flags);
    url = url_new(filename);
    if(url) {
	networking=new_networking();
	if(networking_start(tcp,networking,url)!=MPXP_Ok){
	    MSG_ERR(MSGTR_UnableOpenURL, filename.c_str());
	    delete url;
	    url=NULL;
	    free_networking(networking);
	    networking=NULL;
	    return MPXP_False;
	}
	MSG_INFO(MSGTR_ConnToServer, url->hostname);
	spos = 0;
	return MPXP_Ok;
    }
    return MPXP_False;
}
Stream::type_e Network_Stream_Interface::type() const { return Stream::Type_Stream; }
off_t	Network_Stream_Interface::size() const { return 0; }
off_t	Network_Stream_Interface::sector_size() const { return 1; }
std::string Network_Stream_Interface::mime_type() const { return networking->mime; }

int Network_Stream_Interface::read(stream_packet_t*sp)
{
    sp->type=0;
    if(networking!=NULL)sp->len=networking->networking_read(tcp,sp->buf,STREAM_BUFFER_SIZE, networking);
    else		sp->len=TEMP_FAILURE_RETRY(tcp.read((uint8_t*)sp->buf,STREAM_BUFFER_SIZE));
    spos += sp->len;
    return sp->len;
}

off_t Network_Stream_Interface::seek(off_t pos)
{
    off_t newpos=0;
    if(networking!=NULL) {
	newpos=networking->networking_seek(tcp, pos, networking );
	if( newpos<0 ) {
	    MSG_WARN("Stream not seekable!\n");
	    return 1;
	}
    }
    spos=newpos;
    return newpos;
}

off_t Network_Stream_Interface::tell() const
{
    return spos;
}

void Network_Stream_Interface::close()
{
    tcp.close();
}

MPXP_Rc Network_Stream_Interface::ctrl(unsigned cmd,any_t*args) {
    UNUSED(cmd);
    UNUSED(args);
    return MPXP_Unknown;
}

static Stream_Interface* query_interface(libinput_t& libinput) { return new(zeromem) Network_Stream_Interface(libinput); }

extern const stream_interface_info_t network_stream =
{
    "*://",
    "reads multimedia stream from any known network protocol. Example: inet:http://myserver.com",
    query_interface
};
} // namespace mpxp
#endif
