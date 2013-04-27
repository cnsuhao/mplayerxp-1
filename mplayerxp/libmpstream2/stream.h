#ifndef __STREAM_H_INCLUDED
#define __STREAM_H_INCLUDED
#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;

#include <string>
#include <inttypes.h>
#include <sys/types.h>
#include <string.h>

#include "xmpcore/xmp_enums.h"

namespace mpxp {
    struct libinput_t;
    enum {
	STREAM_BUFFER_SIZE	=4096,
	VCD_SECTOR_SIZE	=2352,
	VCD_SECTOR_OFFS	=24,
	VCD_SECTOR_DATA	=2324,
	MAX_STREAM_PROTOCOLS=10
    };

    /** Stream packet description */
    struct stream_packet_t {
	int	type;	/**< 0 - means raw data; other values depends on stream type */
	char *buf;  /**< buffer to be read */
	int len;	/**< length of buffer */
    };

    enum {
	STREAM_PIN=RND_NUMBER2+RND_CHAR3
    };

    struct cache_vars_t;
    class Stream_Interface;
    struct stream_interface_info_t;
    /** Stream description */
    struct Stream : public Opaque {
	public:
	    enum type_e {
		Type_Unknown	=0x00000000, /**< Stream resides on remote filesystem (doesn't provide seek functionality). */
		Type_Stream	=0x00000001, /**< Stream resides on remote filesystem (doesn't provide seek functionality). */
		Type_Seekable	=0x00000002, /**< Stream is seekable (resides on local filesystem). */
		Type_Memory	=0x00000004, /**< Stream is memory cache (doesn't provide seek/read functionality). */
		Type_Text	=0x00000008, /**< Stream is non-media stream (redirector, playlist, ...). */
		Type_RawAudio	=0x00000010, /**< Stream contains raw audio without headers. */
		Type_RawVideo	=0x00000020, /**< Stream contains raw video without headers. */
		Type_Program	=0x00000040, /**< Stream contains program (non-media) headers (VCD,DVD,http,...) */
		Type_Menu	=0x00000080, /**< Stream contains DVD menu... */
	    };
	    Stream(type_e type=Stream::Type_Unknown);
	    virtual ~Stream();

	    static void		print_drivers();

	    virtual MPXP_Rc	open(libinput_t&libinput,const std::string& filename,int* file_format);
	    virtual void	close();

	    virtual int		read(any_t* mem,int total);
	    virtual off_t	seek(off_t off);
	    virtual int		skip(off_t len);
	    virtual off_t	tell() const;
	    virtual MPXP_Rc	ctrl(unsigned cmd,any_t* param);

	    virtual void	reset();
	    virtual void	type(type_e);/**< assign new propertie for the stream (see STREAMTYPE_ for detail) */
	    virtual type_e	type();		/**< properties of the stream (see STREAMTYPE_ for detail) */
	    virtual int		eof() const;	/**< indicates EOF */
	    virtual void	eof(int);	/**< set EOF */
	    virtual off_t	start_pos() const;	/**< real start of stream (without internet's headers) */
	    virtual off_t	end_pos() const;	/**< real end of stream (media may be not fully filled) */
	    virtual unsigned	sector_size() const; /**< alignment of read operations (1 for file, VCD_SECTOR_SIZE for VCDs) */
	    virtual float	stream_pts() const;	/**< PTS correction for idiotics DVD's discontinuities */
	    virtual std::string mime_type() const;

	    virtual int		read_char();
	    virtual unsigned	read_word();
	    virtual unsigned	read_dword();
	    virtual unsigned	read_word_le();
	    virtual unsigned	read_dword_le();
	    virtual uint64_t	read_qword();
	    virtual uint64_t	read_qword_le();
	    virtual unsigned	read_int24();
	    uint32_t		read_fourcc() { return read_dword_le(); }

	    Opaque		unusable;
	    unsigned		pin;		/**< personal identification number */
	    int			file_format;	/**< detected file format (by http:// protocol for example) */
	    const stream_interface_info_t* driver_info;
	private:
	    int			read(stream_packet_t* sp);
	    Stream_Interface*	driver; /**< low-level stream driver */
	    type_e		_type;
	    int			_eof;	/**< indicates EOF */
    };
    inline Stream::type_e operator~(Stream::type_e a) { return static_cast<Stream::type_e>(~static_cast<unsigned>(a)); }
    inline Stream::type_e operator|(Stream::type_e a, Stream::type_e b) { return static_cast<Stream::type_e>(static_cast<unsigned>(a)|static_cast<unsigned>(b)); }
    inline Stream::type_e operator&(Stream::type_e a, Stream::type_e b) { return static_cast<Stream::type_e>(static_cast<unsigned>(a)&static_cast<unsigned>(b)); }
    inline Stream::type_e operator^(Stream::type_e a, Stream::type_e b) { return static_cast<Stream::type_e>(static_cast<unsigned>(a)^static_cast<unsigned>(b)); }
    inline Stream::type_e operator|=(Stream::type_e& a, Stream::type_e b) { return (a=static_cast<Stream::type_e>(static_cast<unsigned>(a)|static_cast<unsigned>(b))); }
    inline Stream::type_e operator&=(Stream::type_e& a, Stream::type_e b) { return (a=static_cast<Stream::type_e>(static_cast<unsigned>(a)&static_cast<unsigned>(b))); }
    inline Stream::type_e operator^=(Stream::type_e& a, Stream::type_e b) { return (a=static_cast<Stream::type_e>(static_cast<unsigned>(a)^static_cast<unsigned>(b))); }

    struct Memory_Stream : public Stream {
	public:
	    Memory_Stream(const unsigned char* data,unsigned len);
	    virtual ~Memory_Stream();

	    virtual int		read(any_t* mem,int total);
	    virtual off_t	tell() const;
	    virtual off_t	seek(off_t pos);
	    virtual int		skip(off_t len);
	    virtual int		eof() const;
	    virtual void	eof(int eof);
	    virtual void	reset();

	    virtual off_t	start_pos() const;	/**< real start of stream (without internet's headers) */
	    virtual off_t	end_pos() const;	/**< real end of stream (media may be not fully filled) */
	    virtual unsigned	sector_size() const; /**< alignment of read operations (1 for file, VCD_SECTOR_SIZE for VCDs) */
	    virtual std::string	mime_type() const; /**< alignment of read operations (1 for file, VCD_SECTOR_SIZE for VCDs) */
	private:
	    off_t		_pos;
	    unsigned		_len;
	    uint8_t*		buffer;
    };

    struct Cached_Stream : public Stream {
	public:
	    Cached_Stream(libinput_t& libinput,int size,int _min,int prefill,Stream::type_e type=Stream::Type_Unknown);
	    virtual ~Cached_Stream();

	    virtual int		read(any_t* mem,int total);
	    virtual off_t	tell() const;
	    virtual off_t	seek(off_t pos);
	    virtual int		skip(off_t len);
	    virtual int		eof() const;
	    virtual void	eof(int eof);
	    virtual void	reset();
	private:
	    cache_vars_t*	cache_data;	/**< large cache */
    };

/*
    Stream control definitions
*/
    struct rect_highlight_t {
	uint16_t sx, sy;
	uint16_t ex, ey;
    };

    /* These controls extracts text info from stream */
    enum {
	SCTRL_TXT_GET_STREAM_AUTHOR=1,	/**< Returns author of stream. Accepts char *name as pointer on 256 bytes array  */
	SCTRL_TXT_GET_STREAM_NAME,	/**< Returns name of stream. Accepts char *name as pointer on 256 bytes array  */
	SCTRL_TXT_GET_STREAM_SUBJECT,	/**< Returns subject of stream. Accepts char *name as pointer on 256 bytes array  */
	SCTRL_TXT_GET_STREAM_COPYRIGHT,	/**< Returns copyright of stream. Accepts char *name as pointer on 256 bytes array  */
	SCTRL_TXT_GET_STREAM_DESCRIPTION,/**< Returns description of stream. Accepts char *name as pointer on 256 bytes array  */
	SCTRL_TXT_GET_STREAM_ALBUM,	/**< Returns album of stream. Accepts char *name as pointer on 256 bytes array  */
	SCTRL_TXT_GET_STREAM_DATE,	/**< Returns date of stream. Accepts char *name as pointer on 256 bytes array  */
	SCTRL_TXT_GET_STREAM_TRACK,	/**< Returns track of stream. Accepts char *name as pointer on 256 bytes array  */
	SCTRL_TXT_GET_STREAM_GENRE,	/**< Returns genre of stream. Accepts char *name as pointer on 256 bytes array  */
	SCTRL_TXT_GET_STREAM_ENCODER,	/**< Returns encoder of stream. Accepts char *name as pointer on 256 bytes array  */
	SCTRL_TXT_GET_STREAM_SOURCE_MEDIA,/**< Returns mediatype of stream. Accepts char *name as pointer on 256 bytes array  */
	SCTRL_TXT_GET_STREAM_RATING,	/**< Returns rating of stream. Accepts char *name as pointer on 256 bytes array  */
	SCTRL_TXT_GET_STREAM_COMMENT,	/**< Returns comments for stream. Accepts char *name as pointer on 256 bytes array  */
/* These controls extracts videospecific info from stream */
	SCTRL_VID_GET_PALETTE=1000,	/**< Returns palette array. Accepts unsigned** as pointer to palette array */
	SCTRL_VID_GET_WIDTH,		/**< Returns width of raw video in pixels. Accepts unsigned* as pointer to storage area */
	SCTRL_VID_GET_HEIGHT,		/**< Returns height of raw video in pixels. Accepts unsigned* as pointer to storage area */
	SCTRL_VID_GET_FORMAT,		/**< Returns fourcc of raw video in pixels. Accepts unsigned* as pointer to storage area */
	SCTRL_VID_GET_FPS,		/**< Returns Frames Per Seconds of raw video. Accepts float* as pointer to storage area */
	SCTRL_VID_GET_HILIGHT,		/**< Returns HighLight area. Accepts (rect_hilight_t*) as argument */
/* These controls extracts audiospecific info from stream */
	SCTRL_AUD_GET_CHANNELS=2000,	/**< Returns number of channels. Accepts unsigned* as pointer to channels storage */
	SCTRL_AUD_GET_SAMPLERATE,	/**< Returns rate of samples in Hz. Accepts unsigned* as pointer to storage area */
	SCTRL_AUD_GET_SAMPLESIZE,	/**< Returns size fo samples in bits. Accepts unsigned* as pointer to storage area */
	SCTRL_AUD_GET_FORMAT,		/**< Returns format of samples. Accepts unsigned* as pointer to storage area */
/* These controls extracts language specific info from stream */
	SCTRL_LNG_GET_AID=3000,		/**< Returns audio id from language. Accepts char* as input language name. Stores int* id into this pointer as output. */
	SCTRL_LNG_GET_SID,		/**< Returns subtitle id from language. Accepts char* as input language name. Stores int* id into this pointer as output. */
/* These controls provide event handlig by stream driver */
	SCRTL_EVT_HANDLE=4000,		/**< Informs driver about stream specific event. Accepts const stream_packet_t* as input argument */
	SCRTL_MPXP_CMD,			/**< Informs driver about input action (menu selection,tv channels switching,...) . Accepts int cmd as input argument */

	SCTRL_EOF=10000
    };
} // namespace mpxp
#endif // __STREAM_H
