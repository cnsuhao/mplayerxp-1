#ifndef __DEMUX_PACKET_H_INCLUDED
#define __DEMUX_PACKET_H_INCLUDED 1
#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
#include <stdint.h>
#include <unistd.h>

namespace	usr {
    enum dp_flags_e {
	DP_NONKEYFRAME		=0x00000000UL,
	DP_KEYFRAME		=0x00000001UL,

	DP_FULL_FRAME		=0x00000000UL,
	DP_START_OF_FRAME	=0x10000000UL,
	DP_PART_OF_FRAME	=0x20000000UL,
	DP_EOF_OF_FRAME		=0x30000000UL,
    };
    inline dp_flags_e operator~(dp_flags_e a) { return static_cast<dp_flags_e>(~static_cast<unsigned>(a)); }
    inline dp_flags_e operator|(dp_flags_e a, dp_flags_e b) { return static_cast<dp_flags_e>(static_cast<unsigned>(a)|static_cast<unsigned>(b)); }
    inline dp_flags_e operator&(dp_flags_e a, dp_flags_e b) { return static_cast<dp_flags_e>(static_cast<unsigned>(a)&static_cast<unsigned>(b)); }
    inline dp_flags_e operator^(dp_flags_e a, dp_flags_e b) { return static_cast<dp_flags_e>(static_cast<unsigned>(a)^static_cast<unsigned>(b)); }
    inline dp_flags_e operator|=(dp_flags_e& a, dp_flags_e b) { return (a=static_cast<dp_flags_e>(static_cast<unsigned>(a)|static_cast<unsigned>(b))); }
    inline dp_flags_e operator&=(dp_flags_e& a, dp_flags_e b) { return (a=static_cast<dp_flags_e>(static_cast<unsigned>(a)&static_cast<unsigned>(b))); }
    inline dp_flags_e operator^=(dp_flags_e& a, dp_flags_e b) { return (a=static_cast<dp_flags_e>(static_cast<unsigned>(a)^static_cast<unsigned>(b))); }

    /** Describes demuxer's packet */
    class Demuxer_Packet : public Opaque {
	public:
	    Demuxer_Packet(unsigned len);
	    virtual ~Demuxer_Packet();

	    void		resize(unsigned newlen);
	    Demuxer_Packet*	clone() const;

	    unsigned		length() const { return _len; }
	    uint8_t*		buffer() { return _buf; }
	    const uint8_t*	buffer() const { return _buf; }

	    enum dp_type_e {
		Generic=0,
		Video,
		Audio,
		Sub
	    };
	    dp_type_e		type;	/**< type of packet's data */
	    float		pts;	/**< Presentation Time-Stamp (PTS) of data */
	    off_t		pos;	/**< Position in index (AVI) or file (MPG) */
	    dp_flags_e		flags;	/**< 1 - indicates keyframe, 0 - regular frame */
	    int			lang_id;/**< language of this packet */
	    Demuxer_Packet*	next; /**< pointer to the next packet in chain */
	private:
	    unsigned		_len;	/**< length of packet's data */
	    uint8_t*		_buf;	/**< buffer of packet's data */
};
} // namespacee mpxp
#endif
