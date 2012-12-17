#ifndef __NETWORK_PNM_H_INCLUDED
#define __NETWORK_PNM_H_INCLUDED 1

#include "network.h"

namespace mpxp {
    struct Pnm_Networking : public Networking {
    public:
	virtual ~Pnm_Networking();

	static Networking*	start(Tcp& tcp, network_protocol_t& protocol);
	virtual int read( Tcp& fd, char *buffer, int buffer_size);
	virtual int seek( Tcp& fd, off_t pos);
    private:
	Pnm_Networking();
    };
} // namespace mpxp
#endif

