#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
#include <stdio.h>
#include "tcp.h"
#include "network_rtsp.h"
#include "librtsp/rtsp_session.h"

namespace mpxp {
Networking* Rtsp_Networking::start(Tcp& tcp, URL& url,unsigned bandwidth)
{
    Rtsp_Session *rtsp;
    char *mrl;
    const char *file;
    int redirected, temp;

    /* counter so we don't get caught in infinite redirections */
    temp = 5;

    do {
	redirected = 0;

	url.assign_port(554);
	tcp.open(url);
	if (!tcp.established())  {
	    url.assign_port(7070);
	    tcp.open(url);
	}
	if (!tcp.established()) return NULL;
	file = url.file().c_str();
	if (file[0] == '/') file++;

	mrl = new char [url.host().length() + strlen (file) + 16];

	sprintf (mrl, "rtsp://%s:%i/%s",url.host().c_str(), url.port(), file);

	rtsp = Rtsp_Session::start (tcp, &mrl, file,
			url.host(),
			url.port(), &redirected,
			bandwidth,
			url.user(),
			url.password());
	if (redirected == 1) {
	    url.redirect(mrl);
	    tcp.close();
	}
	delete mrl;
	temp--;
    } while ((redirected != 0) && (temp > 0));

    if (!rtsp) return NULL;

    Rtsp_Networking* rv = new(zeromem) Rtsp_Networking;
    rv->data = rtsp;

    rv->url = url;
    rv->prebuffer_size = 128*1024;  // 640 KBytes
    rv->buffering = 1;
    rv->status = networking_playing_e;

    return NULL;
}

int Rtsp_Networking::seek(Tcp& tcp, off_t pos) {
    UNUSED(tcp);
    UNUSED(pos);
    return -1;
}

int Rtsp_Networking::read(Tcp& tcp, char *_buffer, int size) {
    Rtsp_Session& rtsp = *static_cast<Rtsp_Session*>(data);
    return rtsp.read (buffer, size);
}

Rtsp_Networking::Rtsp_Networking() {}
Rtsp_Networking::~Rtsp_Networking() {
    Rtsp_Session* rtsp = static_cast<Rtsp_Session*>(data);
    if (rtsp) rtsp->end();
}
} // namespace mpxp
