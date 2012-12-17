#ifndef __NETWORK_SP_H_INCLUDED
#define __NETWORK_SP_H_INCLUDED 1

#include "network.h"

namespace mpxp {
    struct Rtsp_Networking : public Networking {
	public:
	    virtual ~Rtsp_Networking();

	    static Networking*	start(Tcp& tcp, URL* url,unsigned bandwidth);
	    virtual int read( Tcp& fd, char *buffer, int buffer_size);
	    virtual int seek( Tcp& fd, off_t pos);
	private:
	    Rtsp_Networking();
    };
} // namespace mpxp
#endif