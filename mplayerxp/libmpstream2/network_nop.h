#ifndef __NETWORK_NOP_H_INCLUDED
#define __NETWORK_NOP_H_INCLUDED

#include "network.h"

namespace mpxp {
    struct Nop_Networking : public Networking {
	public:
	    Nop_Networking();
	    virtual ~Nop_Networking();

	    static Networking*	start(Tcp& tcp, network_protocol_t& protocol);
	    virtual int read( Tcp& fd, char *buffer, int buffer_size);
	    virtual int seek( Tcp& fd, off_t pos);
    };
} // namespace mpxp
#endif

