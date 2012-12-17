#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;

#include "tcp.h"
#include "pnm.h"
#include "network_pnm.h"

namespace mpxp {
int Pnm_Networking::seek(Tcp& tcp, off_t pos) {
    UNUSED(tcp);
    UNUSED(pos);
    return -1;
}

int Pnm_Networking::read(Tcp& tcp, char *_buffer, int size) {
    Pnm& pnm=*static_cast<Pnm*>(data);
    UNUSED(tcp);
    return pnm.read(_buffer, size);
}

Networking* Pnm_Networking::start(Tcp& tcp,network_protocol_t& protocol ) {
    Pnm* pnm = new(zeromem) Pnm(tcp);

    tcp.open(protocol.url->hostname,
	    protocol.url->port ? protocol.url->port : 7070);
    if(!tcp.established()) return NULL;

    if(pnm->connect(protocol.url->file)!=MPXP_Ok) {
	delete pnm;
	return NULL;
    }
    Pnm_Networking* rv = new(zeromem) Pnm_Networking;
    rv->data=pnm;
    rv->prebuffer_size = 8*1024;  // 8 KBytes
    rv->buffering = 1;
    rv->status = networking_playing_e;
    return rv;
}

Pnm_Networking::Pnm_Networking() {}
Pnm_Networking::~Pnm_Networking() {}
} // namespace mpxp
