#ifndef __DEMUX_STREAM_H_INCLUDED
#define __DEMUX_STREAM_H_INCLUDED 1
#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;

#include <stdint.h>
#include "demuxer_packet.h"

namespace mpxp {
    struct Demuxer;
    struct Stream;
    class Demuxer_Stream : public Opaque {
	public:
	    Demuxer_Stream(Demuxer *demuxer,int id);
	    virtual ~Demuxer_Stream();

	    void	add_packet(Demuxer_Packet* dp);
	    void	read_packet(Stream *stream,int len,float pts,off_t pos,dp_flags_e flags);
	    int		read_data(unsigned char* mem,int len);
	    void	free_packs();
	    void	free_packs_until_pts(float pts);
	    int		get_packet(unsigned char **start);
	    int		get_packet_sub(unsigned char **start);
	    float	get_next_pts();
	    int		getch();
	    int		fill_buffer();
	    off_t	tell() const { return (dpos-_buffer_size)+_buffer_pos; }
	    int		tell_pts() const { return (_pts_bytes-_buffer_size)+_buffer_pos; }
	    int		packs() const { return _packs; }
	    int		bytes() const { return _bytes; }
	    const uint8_t*buffer() const { return _buffer; }
	    void	buffer_roll_back(int size); // deprecated (added for ad_lavc)

	    int			id;		/**< stream ID  (for multiple audio/video streams) */
	    char		antiviral_hole[RND_CHAR2];
	    unsigned		pin;		/**< personal identification number */
	    float		pts;		/**< current buffer's PTS */
	    int			eof;		/**< end of demuxed stream? (true if all buffer empty) */
	    off_t		pos;		/**< position in the input stream (file) */
	    off_t		dpos;		/**< position in the demuxed stream */
	    int			flags;		/**< flags of current packet (keyframe etc) */
/*---------------*/
/* ---- asf ----- */
	    Demuxer_Packet*	asf_packet;	/**< read asf fragments here */
	    int			asf_seq;	/**< sequence id associated with asf_packet */
/*---------------*/
	    any_t*		sh;		/**< Stream header associated with this stream (@see st_header.h for detail) */
/*---------------*/
	    float		prev_pts;	/**< PTS of previous packet (DVD's PTS correction) */
	    float		pts_corr;	/**< PTS correction (DVD's PTS correction) */
	    int			pts_flags;	/**< PTS flags like trigger for correction applying (DVD's PTS correction) */
	    Demuxer*		demuxer;	/**< parent demuxer structure (stream handler) */
	    int			pack_no;	/**< serial number of packet */
	private:
	    int			_pts_bytes;	/**< number of bytes read after last pts stamp */
	    int			_packs;		/**< number of packets in buffer */
	    int			_bytes;		/**< total bytes of packets in buffer */
	    Demuxer_Packet*	_first;		/**< read to current buffer from here */
	    Demuxer_Packet*	_last;		/**< append new packets from input stream to here */
	    Demuxer_Packet*	_current;	/**< needed for refcounting of the buffer */
	    int			_buffer_pos;	/**< current buffer position */
	    int			_buffer_size;	/**< current buffer size */
	    uint8_t*		_buffer;	/**< current buffer */
    };
} // namespace mpxp
#endif
