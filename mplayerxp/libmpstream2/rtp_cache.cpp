#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
/* Imported from the dvbstream-0.2 project
 *
 * Modified for use with MPlayer, for details see the changelog at
 * http://svn.mplayerhq.hu/mplayer/trunk/
 * $Id: rtp.c,v 1.4 2007/11/17 12:43:37 nickols_k Exp $
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <ctype.h>

#ifndef HAVE_WINSOCK2
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#define closesocket close
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#endif
#include <errno.h>
#include "stream.h"

/* MPEG-2 TS RTP stack */

#define DEBUG        1
#include "tcp.h"
#include "rtp_cache.h"
#include "stream_msg.h"

// RTP reorder routines
// Also handling of repeated UDP packets (a bug of ExtremeNetworks switches firmware)
// rtpreord procedures
// write rtp packets in cache
// get rtp packets reordered

namespace mpxp {
struct rtpbits {
  unsigned int v:2;           /* version: 2 */
  unsigned int p:1;           /* is there padding appended: 0 */
  unsigned int x:1;           /* number of extension headers: 0 */
  unsigned int cc:4;          /* number of CSRC identifiers: 0 */
  unsigned int m:1;           /* marker: 0 */
  unsigned int pt:7;          /* payload type: 33 for MPEG2 TS - RFC 1890 */
  unsigned int sequence:16;   /* sequence number: random */
};

struct rtpheader {	/* in network byte order */
  struct rtpbits b;
  int timestamp;	/* start: random */
  int ssrc;		/* random */
};

// RTP Reordering functions
// Algorithm works as follows:
// If next packet is in sequence just copy it to buffer
// Otherwise copy it in cache according to its sequence number
// Cache is a circular array where "rtpbuf.first" points to next sequence slot
// and keeps track of expected sequence

// Initialize rtp cache
void Rtp_Cache::cache_reset(unsigned short seq) {
    int i;

    rtp_first = 0;
    rtp_seq[0] = ++seq;

    for (i=0; i<MAXRTPPACKETSIN; i++) rtp_len[i] = 0;
}

// Write in a cache the rtp packet in right rtp sequence order
int Rtp_Cache::cache(char *buffer, int length) {
    struct rtpheader rh;
    int newseq;
    char *data;
    unsigned short seq;
    static int is_first = 1;

    getrtp2(&rh, &data, &length);
    if(!length) return 0;
    seq = rh.b.sequence;

    newseq = seq - rtp_seq[rtp_first];

    if ((newseq == 0) || is_first) {
	is_first = 0;

	rtp_first = ( 1 + rtp_first ) % MAXRTPPACKETSIN;
	rtp_seq[rtp_first] = ++seq;
	goto feed;
    }

    if (newseq > MAXRTPPACKETSIN) {
	mpxp_dbg2<<"Overrun(seq["<<rtp_first<<"]="<<rtp_seq[rtp_first]<<" seq="<<seq<<", newseq="<<newseq<<")"<<std::endl;
	cache_reset(seq);
	goto feed;
    }

    if (newseq < 0) {
	int i;

	// Is it a stray packet re-sent to network?
	for (i=0; i<MAXRTPPACKETSIN; i++) {
	    if (rtp_seq[i] == seq) {
		mpxp_err<<"Stray packet (seq["<<rtp_first<<"]="<<rtp_seq[rtp_first]<<" seq="<<seq<<", newseq="<<newseq<<" found at "<<i<<")"<<std::endl;
		return  0; // Yes, it is!
	    }
	}
	// Some heuristic to decide when to drop packet or to restart everything
	if (newseq > -(3 * MAXRTPPACKETSIN)) {
	    mpxp_err<<"Too Old packet (seq["<<rtp_first<<"]="<<rtp_seq[rtp_first]<<" seq="<<seq<<", newseq="<<newseq<<")"<<std::endl;
	    return  0; // Yes, it is!
	}

	mpxp_err<<"Underrun(seq["<<rtp_first<<"]="<<rtp_seq[rtp_first]<<" seq="<<seq<<", newseq="<<newseq<<")"<<std::endl;

	cache_reset(seq);
	goto feed;
    }

    mpxp_dbg3<<"Out of Seq (seq["<<rtp_first<<"]="<<rtp_seq[rtp_first]<<" seq="<<seq<<", newseq="<<newseq<<")"<<std::endl;
    newseq = ( newseq + rtp_first ) % MAXRTPPACKETSIN;
    memcpy (rtp_data[newseq], data, length);
    rtp_len[newseq] = length;
    rtp_seq[newseq] = seq;

    return 0;

feed:
    memcpy (buffer, data, length);
    return length;
}

// Get next packet in cache
// Look in cache to get first packet in sequence
int Rtp_Cache::get_next(char *buffer, int length) {
    int i;
    unsigned short nextseq;

    // If we have empty buffer we loop to fill it
    for (i=0; i < MAXRTPPACKETSIN -3; i++) {
	if (rtp_len[rtp_first] != 0) break;

	length = cache(buffer, length);

	// returns on first packet in sequence
	if (length > 0) return length;
	else if (length < 0) break;
	// Only if length == 0 loop continues!
    }

    i = rtp_first;
    while (rtp_len[i] == 0) {
	mpxp_err<<"Lost packet "<<std::hex<<rtp_seq[i]<<std::endl;
	i = ( 1 + i ) % MAXRTPPACKETSIN;
	if (rtp_first == i) break;
    }
    rtp_first = i;

    // Copy next non empty packet from cache
    mpxp_dbg3<<"Getting rtp from cache ["<<rtp_first<<"] "<<std::hex<<rtp_seq[rtp_first]<<std::endl;
    memcpy (buffer, rtp_data[rtp_first], rtp_len[rtp_first]);
    length = rtp_len[rtp_first]; // can be zero?

    // Reset fisrt slot and go next in cache
    rtp_len[rtp_first] = 0;
    nextseq = rtp_seq[rtp_first];
    rtp_first = ( 1 + rtp_first ) % MAXRTPPACKETSIN;
    rtp_seq[rtp_first] = nextseq + 1;

    return length;
}


// Read next rtp packet using cache
int Rtp_Cache::read_from_server(char *buffer, int length) {
    // Following test is ASSERT (i.e. uneuseful if code is correct)
    if(buffer==NULL || length<STREAM_BUFFER_SIZE) {
	mpxp_err<<"RTP buffer invalid; no data return from network"<<std::endl;
	return 0;
    }

    // loop just to skip empty packets
    while ((length = get_next(buffer, length)) == 0) {
	mpxp_err<<"Got empty packet from RTP cache!?"<<std::endl;
    }
    return length;
}

int Rtp_Cache::getrtp2(struct rtpheader *rh, char** data, int* lengthData) const {
    static char buf[1600];
    unsigned int intP;
    char* charP = (char*) &intP;
    int headerSize;
    int lengthPacket;
    lengthPacket=tcp.read((uint8_t*)(buf),1590);
    if (lengthPacket<0) mpxp_err<<"rtp: socket read error"<<std::endl;
    else if (lengthPacket<12) mpxp_err<<"rtp: packet too small ("<<lengthPacket<<") to be an rtp frame (>12bytes)"<<std::endl;
    if(lengthPacket<12) {
	*lengthData = 0;
	return 0;
    }
    rh->b.v  = (unsigned int) ((buf[0]>>6)&0x03);
    rh->b.p  = (unsigned int) ((buf[0]>>5)&0x01);
    rh->b.x  = (unsigned int) ((buf[0]>>4)&0x01);
    rh->b.cc = (unsigned int) ((buf[0]>>0)&0x0f);
    rh->b.m  = (unsigned int) ((buf[1]>>7)&0x01);
    rh->b.pt = (unsigned int) ((buf[1]>>0)&0x7f);
    intP = 0;
    memcpy(charP+2,&buf[2],2);
    rh->b.sequence = ntohl(intP);
    intP = 0;
    memcpy(charP,&buf[4],4);
    rh->timestamp = ntohl(intP);

    headerSize = 12 + 4*rh->b.cc; /* in bytes */

    *lengthData = lengthPacket - headerSize;
    *data = (char*) buf + headerSize;

    return 0;
}
Rtp_Cache::Rtp_Cache(Tcp& _tcp):tcp(_tcp) {}
Rtp_Cache::~Rtp_Cache() {}
} // namespace mpxp
