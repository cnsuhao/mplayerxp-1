#include "mpxp_config.h"
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
#include "mpxp_help.h"
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
	    URL			url;
	    off_t		spos;
	    Tcp			tcp;
	    Networking*	networking;
    };

Network_Stream_Interface::Network_Stream_Interface(libinput_t& libinput)
			:Stream_Interface(libinput),
			tcp(libinput,-1) {}
Network_Stream_Interface::~Network_Stream_Interface() {}

MPXP_Rc Network_Stream_Interface::open(const std::string& filename,unsigned flags)
{
    UNUSED(flags);
    if(url.redirect(filename)==MPXP_Ok) {
	if((networking=Networking::start(tcp,url))==NULL){
	    mpxp_err<<MSGTR_UnableOpenURL<<":"<<filename<<std::endl;
	    return MPXP_False;
	}
	mpxp_info<<MSGTR_ConnToServer<<":"<<url.host()<<std::endl;
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
    if(networking!=NULL)sp->len=networking->read(tcp,sp->buf,STREAM_BUFFER_SIZE);
    else		sp->len=TEMP_FAILURE_RETRY(tcp.read((uint8_t*)sp->buf,STREAM_BUFFER_SIZE));
    spos += sp->len;
    return sp->len;
}

off_t Network_Stream_Interface::seek(off_t pos)
{
    off_t newpos=0;
    if(networking!=NULL) {
	newpos=networking->seek(tcp, pos);
	if( newpos<0 ) {
	    mpxp_warn<<"Stream not seekable!"<<std::endl;
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
