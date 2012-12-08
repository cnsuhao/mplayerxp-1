#ifndef __STREAM_H_INCLUDED
#define __STREAM_H_INCLUDED
#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;

#include <inttypes.h>
#include <sys/types.h>
#include <string.h>

#include "xmpcore/xmp_enums.h"
#ifdef HAVE_STREAMING
#include "network.h"
#endif

struct networking_t;
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

    struct Stream;
    typedef void (* __FASTCALL__ stream_callback)(Stream *s,const stream_packet_t*);

    enum {
	STREAM_PIN=RND_NUMBER2+RND_CHAR3
    };

    struct cache_vars_s;
    class Demuxer_Stream;
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
		Type_DS		=0x00000010, /**< Stream is mapped on the other demuxer. */
		Type_RawAudio	=0x00000020, /**< Stream contains raw audio without headers. */
		Type_RawVideo	=0x00000040, /**< Stream contains raw video without headers. */
		Type_Program	=0x00000080, /**< Stream contains program (non-media) headers (VCD,DVD,http,...) */
		Type_Menu	=0x00000100, /**< Stream contains DVD menu... */
	    };
	    Stream(type_e type=Stream::Type_Unknown);
	    Stream(Demuxer_Stream* ds);
	    virtual ~Stream();

	    virtual MPXP_Rc	open(libinput_t*libinput,const char* filename,int* file_format,stream_callback event_handler);
	    virtual int		read(stream_packet_t * sp);
	    virtual off_t	seek(off_t off);
	    virtual off_t	tell() const;
	    virtual void	close();
	    virtual MPXP_Rc	ctrl(unsigned cmd,any_t* param);

	    virtual void	reset();
	    virtual void	type(type_e);/**< assign new propertie for the stream (see STREAMTYPE_ for detail) */
	    virtual type_e	type();		/**< properties of the stream (see STREAMTYPE_ for detail) */
	    virtual off_t	pos() const;	/**< SOF offset from begin of stream */
	    virtual void	pos(off_t);	/**< SOF offset from begin of stream */
	    virtual int		eof() const;	/**< indicates EOF */
	    virtual void	eof(int);	/**< set EOF */
	    virtual off_t	start_pos() const;	/**< real start of stream (without internet's headers) */
	    virtual off_t	end_pos() const;	/**< real end of stream (media may be not fully filled) */
	    virtual unsigned	sector_size() const; /**< alignment of read operations (1 for file, VCD_SECTOR_SIZE for VCDs) */
	    virtual float	stream_pts() const;	/**< PTS correction for idiotics DVD's discontinuities */

	    char		antiviral_hole[RND_CHAR3];
	    unsigned		pin;		/**< personal identification number */
	    int			file_format;	/**< detected file format (by http:// protocol for example) */
	    unsigned int	buf_pos; /**< position whitin of small cache */
	    unsigned int	buf_len; /**< length of small cache */
	    unsigned char*	buffer;/**< buffer of small cache */
	    struct cache_vars_s*cache_data;	/**< large cache */
	    const stream_interface_info_t* driver_info;
	    stream_callback	event_handler;  /**< callback for streams which provide events */
	private:
	    Stream_Interface*	driver; /**< low-level stream driver */
	    Opaque*		priv;	/**< private data used by stream driver */
	    type_e		_type;
	    off_t		_pos;	/**< SOF offset from begin of stream */
	    int			_eof;	/**< indicates EOF */
    };
    inline Stream::type_e operator~(Stream::type_e a) { return static_cast<Stream::type_e>(~static_cast<unsigned>(a)); }
    inline Stream::type_e operator|(Stream::type_e a, Stream::type_e b) { return static_cast<Stream::type_e>(static_cast<unsigned>(a)|static_cast<unsigned>(b)); }
    inline Stream::type_e operator&(Stream::type_e a, Stream::type_e b) { return static_cast<Stream::type_e>(static_cast<unsigned>(a)&static_cast<unsigned>(b)); }
    inline Stream::type_e operator^(Stream::type_e a, Stream::type_e b) { return static_cast<Stream::type_e>(static_cast<unsigned>(a)^static_cast<unsigned>(b)); }
    inline Stream::type_e operator|=(Stream::type_e a, Stream::type_e b) { return (a=static_cast<Stream::type_e>(static_cast<unsigned>(a)|static_cast<unsigned>(b))); }
    inline Stream::type_e operator&=(Stream::type_e a, Stream::type_e b) { return (a=static_cast<Stream::type_e>(static_cast<unsigned>(a)&static_cast<unsigned>(b))); }
    inline Stream::type_e operator^=(Stream::type_e a, Stream::type_e b) { return (a=static_cast<Stream::type_e>(static_cast<unsigned>(a)^static_cast<unsigned>(b))); }

    int stream_enable_cache(Stream *stream,libinput_t* libinput,int size,int min,int prefill);
    void stream_disable_cache(Stream *stream);

/* this block describes interface to non-cache stream functions */
    extern int __FASTCALL__ nc_stream_read_cbuffer(Stream *s);
    extern int __FASTCALL__ nc_stream_seek_long(Stream *s,off_t pos);
    extern int __FASTCALL__ nc_stream_read_char(Stream *s);
    extern int __FASTCALL__ nc_stream_read(Stream *s,any_t* mem,int total);
    extern off_t __FASTCALL__ nc_stream_tell(Stream *s);
    extern int __FASTCALL__ nc_stream_seek(Stream *s,off_t pos);
    extern int __FASTCALL__ nc_stream_skip(Stream *s,off_t len);

/* this block describes interface to cache/non-cache stream functions */
    extern int __FASTCALL__ stream_read_char(Stream *s);
    extern int __FASTCALL__ stream_read(Stream *s,any_t* mem,int total);
    extern off_t __FASTCALL__ stream_tell(Stream *s);
    extern int __FASTCALL__ stream_seek(Stream *s,off_t pos);
    extern int __FASTCALL__ stream_skip(Stream *s,off_t len);
    extern int __FASTCALL__ stream_eof(Stream *s);
    extern void __FASTCALL__ stream_set_eof(Stream *s,int eof);

    void __FASTCALL__ stream_reset(Stream *s);
    Stream* __FASTCALL__ new_memory_stream(const unsigned char* data,int len);

    extern unsigned int __FASTCALL__ stream_read_word(Stream *s);
    extern unsigned int __FASTCALL__ stream_read_dword(Stream *s);
    extern unsigned int __FASTCALL__ stream_read_word_le(Stream *s);
    extern unsigned int __FASTCALL__ stream_read_dword_le(Stream *s);
    extern uint64_t     __FASTCALL__ stream_read_qword(Stream *s);
    extern uint64_t     __FASTCALL__ stream_read_qword_le(Stream *s);
    extern unsigned int __FASTCALL__ stream_read_int24(Stream *s);
    static inline uint32_t stream_read_fourcc(Stream* s) { return stream_read_dword_le(s); }

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
	SCTRL_TXT_GET_STREAM_MIME,	/**< Returns mimetype of stream. Accepts char *name as pointer on 256 bytes array  */
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

    void print_stream_drivers(void);
} // namespace mpxp
#endif // __STREAM_H
