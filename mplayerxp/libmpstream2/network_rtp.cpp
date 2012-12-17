#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;

#include "udp.h"
#include "tcp.h"
#include "network_rtp.h"
#include "rtp.h"

namespace mpxp {
int Rtp_Networking::seek(Tcp& tcp, off_t pos) {
    UNUSED(tcp);
    UNUSED(pos);
    return -1;
}

int Rtp_Networking::read(Tcp& tcp, char *_buffer, int size) {
    return read_rtp_from_server(tcp, _buffer, size );
}

Networking* Rtp_Networking::start(Tcp& tcp,network_protocol_t& protocol, int raw_udp ) {

    if( !tcp.established() ) {
	Udp* udp(new(zeromem) Udp(protocol.url));
	tcp = udp->socket();
	if( !tcp.established()) return NULL;
    }

    if(raw_udp) return Nop_Networking::start(tcp,protocol);
    Rtp_Networking* rv = new(zeromem) Rtp_Networking;
    rv->prebuffer_size = 64*1024;	// KBytes
    rv->buffering = 0;
    rv->status = networking_playing_e;
    return rv;
}

Rtp_Networking::Rtp_Networking() {}
Rtp_Networking::~Rtp_Networking() {}
} // namespace mpxp
