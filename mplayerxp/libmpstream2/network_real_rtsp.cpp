#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
#include <stdio.h>
#include "tcp.h"
#include "network_real_rtsp.h"
#include "librtsp/rtsp_session.h"

namespace mpxp {
int RealRtsp_Networking::seek(Tcp& tcp, off_t pos) {
    UNUSED(tcp);
    UNUSED(pos);
    return -1;
}

int RealRtsp_Networking::read( Tcp& tcp, char *_buffer, int size) {
    Rtsp_Session& rtsp=*static_cast<Rtsp_Session*>(data);
    return rtsp.read(tcp, _buffer, size);
}

Networking* RealRtsp_Networking::start( Tcp& tcp, network_protocol_t& protocol ) {
    Rtsp_Session* rtsp;
    char *mrl;
    char *file;
    int port;
    int redirected, temp;

    temp = 5; // counter so we don't get caught in infinite redirections (you never know)

    do {
	redirected = 0;
	port = protocol.url->port ? protocol.url->port : 554;
	tcp.close();
	tcp.open( protocol.url->hostname, port, Tcp::IP4);
	if(!tcp.established() && !protocol.url->port)
	    tcp.open( protocol.url->hostname,port = 7070, Tcp::IP4);
	if(!tcp.established()) return NULL;

	file = protocol.url->file;
	if (file[0] == '/') file++;
	mrl = new char [strlen(protocol.url->hostname)+strlen(file)+16];
	sprintf(mrl,"rtsp://%s:%i/%s",protocol.url->hostname,port,file);
	rtsp = rtsp_session_start(tcp,&mrl, file,
			protocol.url->hostname, port, &redirected,
			net_conf.bandwidth,protocol.url->username,
			protocol.url->password);

	if ( redirected == 1 ) {
	    delete protocol.url;
	    protocol.url = url_new(mrl);
	    tcp.close();
	}
	delete mrl;
	temp--;

    } while( (redirected != 0) && (temp > 0) );

    if(!rtsp) return NULL;

    RealRtsp_Networking* rv = new(zeromem) RealRtsp_Networking;
    rv->data=rtsp;

    rv->prebuffer_size = 128*1024;  // 8 KBytes
    rv->buffering = 1;
    rv->status = networking_playing_e;
    return rv;
}
RealRtsp_Networking::RealRtsp_Networking() {}
RealRtsp_Networking::~RealRtsp_Networking() {}
} // namespace mpxp
