#ifndef __NETWORK_REAL_RTSP_H_INCLUDED
#define __NETWORK_REAL_RTSP_H_INCLUDED 1

#include "network.h"

namespace	usr {
    struct RealRtsp_Networking : public Networking {
	public:
	    virtual ~RealRtsp_Networking();

	    static Networking*	start(Tcp& tcp, network_protocol_t& protocol);
	    virtual int read( Tcp& fd, char *buffer, int buffer_size);
	    virtual int seek( Tcp& fd, off_t pos);
	private:
	    RealRtsp_Networking();
    };
}
#endif
