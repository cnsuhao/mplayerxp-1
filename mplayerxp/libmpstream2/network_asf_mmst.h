#ifndef __NETWORK_ASF_MMST_H_INCLUDED
#define __NETWORK_ASF_MMST_H_INCLUDED 1

#include "network_nop.h"

namespace mpxp {
    struct Asf_Mmst_Networking : public Nop_Networking {
	public:
	    virtual ~Asf_Mmst_Networking();

	    static Networking*	start(Tcp& tcp, network_protocol_t& protocol);
	    virtual int read( Tcp& fd, char *buffer, int buffer_size);
	    virtual int seek( Tcp& fd, off_t pos);
	private:
	    Asf_Mmst_Networking();
	    int		get_header (Tcp& tcp, uint8_t *header);
    };
} // namespace mpxp
#endif

