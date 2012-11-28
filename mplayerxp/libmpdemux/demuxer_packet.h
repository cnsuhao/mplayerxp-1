#ifndef __DEMUX_PACKET_H_INCLUDED
#define __DEMUX_PACKET_H_INCLUDED 1
#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
#include <unistd.h>

enum {
    DP_NONKEYFRAME	=0x00000000UL,
    DP_KEYFRAME		=0x00000001UL
};

/** Describes demuxer's packet */
class Demux_Packet : public Opaque {
    public:
	Demux_Packet(unsigned len);
	virtual ~Demux_Packet();

	void resize(unsigned newlen);
	Demux_Packet* clone() const;

	unsigned	len;	/**< length of packet's data */
	float		pts;	/**< Presentation Time-Stamp (PTS) of data */
	off_t		pos;	/**< Position in index (AVI) or file (MPG) */
	unsigned char*	buffer; /**< buffer of packet's data */
	unsigned	flags;	/**< 1 - indicates keyframe, 0 - regular frame */
	Demux_Packet*	next; /**< pointer to the next packet in chain */
};

#endif
