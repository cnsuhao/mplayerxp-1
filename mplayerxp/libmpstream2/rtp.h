/* Imported from the dvbstream project
 *
 * Modified for use with MPlayer, for details see the changelog at
 * http://svn.mplayerhq.hu/mplayer/trunk/
 * $Id: rtp.h,v 1.2 2007/11/17 12:43:37 nickols_k Exp $
 */
#ifndef __RTP_H_INCLUDED
#define __RTP_H_INCLUDED 1
#include "network.h"
#include "tcp.h"
#include <stdint.h>

namespace mpxp {
// RTP reorder routines
// Also handling of repeated UDP packets (a bug of ExtremeNetworks switches firmware)
// rtpreord procedures
// write rtp packets in cache
// get rtp packets reordered

#define MAXRTPPACKETSIN 32   // The number of max packets being reordered

    class Rtp : public Opaque {
	public:
	    Rtp(Tcp& tcp);
	    virtual ~Rtp();

	    virtual int		read_from_server(char *buffer, int length);
	private:
	    void	cache_reset(unsigned short seq);
	    int		cache(char *buffer, int length);
	    int		get_next(char *buffer, int length);
	    int		getrtp2(struct rtpheader *rh, char** data, int* lengthData) const;

	    Tcp&	tcp;
	    uint8_t	rtp_data[MAXRTPPACKETSIN][STREAM_BUFFER_SIZE];
	    uint16_t	rtp_seq[MAXRTPPACKETSIN];
	    uint16_t	rtp_len[MAXRTPPACKETSIN];
	    uint16_t	rtp_first;
    };
} // namespace mpxp
#endif
