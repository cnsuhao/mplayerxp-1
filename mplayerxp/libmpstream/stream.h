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

namespace mpxp {
    struct libinput_t;
    enum {
	STREAM_BUFFER_SIZE	=4096,
	VCD_SECTOR_SIZE	=2352,
	VCD_SECTOR_OFFS	=24,
	VCD_SECTOR_DATA	=2324,
	MAX_STREAM_PROTOCOLS=10
    };

    enum stream_type_e {
	STREAMTYPE_Unknown	=0x00000000, /**< Stream resides on remote filesystem (doesn't provide seek functionality). */
	STREAMTYPE_STREAM	=0x00000001, /**< Stream resides on remote filesystem (doesn't provide seek functionality). */
	STREAMTYPE_SEEKABLE	=0x00000002, /**< Stream is seekable (resides on local filesystem). */
	STREAMTYPE_MEMORY	=0x00000004, /**< Stream is memory cache (doesn't provide seek/read functionality). */
	STREAMTYPE_TEXT		=0x00000008, /**< Stream is non-media stream (redirector, playlist, ...). */
	STREAMTYPE_DS		=0x00000010, /**< Stream is mapped on the other demuxer. */
	STREAMTYPE_RAWAUDIO	=0x00000020, /**< Stream contains raw audio without headers. */
	STREAMTYPE_RAWVIDEO	=0x00000040, /**< Stream contains raw video without headers. */
	STREAMTYPE_PROGRAM	=0x00000080, /**< Stream contains program (non-media) headers (VCD,DVD,http,...) */
	STREAMTYPE_MENU		=0x00000100, /**< Stream contains DVD menu... */
    };
    inline stream_type_e operator~(stream_type_e a) { return static_cast<stream_type_e>(~static_cast<unsigned>(a)); }
    inline stream_type_e operator|(stream_type_e a, stream_type_e b) { return static_cast<stream_type_e>(static_cast<unsigned>(a)|static_cast<unsigned>(b)); }
    inline stream_type_e operator&(stream_type_e a, stream_type_e b) { return static_cast<stream_type_e>(static_cast<unsigned>(a)&static_cast<unsigned>(b)); }
    inline stream_type_e operator^(stream_type_e a, stream_type_e b) { return static_cast<stream_type_e>(static_cast<unsigned>(a)^static_cast<unsigned>(b)); }
    inline stream_type_e operator|=(stream_type_e a, stream_type_e b) { return (a=static_cast<stream_type_e>(static_cast<unsigned>(a)|static_cast<unsigned>(b))); }
    inline stream_type_e operator&=(stream_type_e a, stream_type_e b) { return (a=static_cast<stream_type_e>(static_cast<unsigned>(a)&static_cast<unsigned>(b))); }
    inline stream_type_e operator^=(stream_type_e a, stream_type_e b) { return (a=static_cast<stream_type_e>(static_cast<unsigned>(a)^static_cast<unsigned>(b))); }

    /** Stream packet description */
    struct stream_packet_t {
	int	type;	/**< 0 - means raw data; other values depends on stream type */
	char *buf;  /**< buffer to be read */
	int len;	/**< length of buffer */
    };

    struct stream_t;
    typedef void (* __FASTCALL__ stream_callback)(stream_t *s,const stream_packet_t*);

    enum {
	STREAM_PIN=RND_NUMBER2+RND_CHAR3
    };

    struct cache_vars_s;
    struct Demuxer;
    class Stream_Interface;
    struct stream_interface_info_t;
    /** Stream description */
    struct stream_t : public Opaque {
	public:
	    stream_t():_type(STREAMTYPE_Unknown) {}
	    virtual ~stream_t() {}

	    char		antiviral_hole[RND_CHAR3];
	    unsigned		pin;		/**< personal identification number */
	    off_t		pos;		/**< SOF offset from begin of stream */
	    int			eof;		/**< indicates EOF */
	    void		type(stream_type_e);/**< assign new propertie for the stream (see STREAMTYPE_ for detail) */
	    stream_type_e	type();		/**< properties of the stream (see STREAMTYPE_ for detail) */
	    int			file_format;	/**< detected file format (by http:// protocol for example) */
	    unsigned int	buf_pos; /**< position whitin of small cache */
	    unsigned int	buf_len; /**< length of small cache */
	    unsigned char*	buffer;/**< buffer of small cache */
	    off_t		start_pos() const;	/**< real start of stream (without internet's headers) */
	    off_t		end_pos() const;	/**< real end of stream (media may be not fully filled) */
	    unsigned		sector_size() const; /**< alignment of read operations (1 for file, VCD_SECTOR_SIZE for VCDs) */
	    Demuxer*		demuxer; /* parent demuxer */
	    struct cache_vars_s*cache_data;	/**< large cache */
	    Opaque*		priv;	/**< private data used by stream driver */
	    float		stream_pts() const;	/**< PTS correction for idiotics DVD's discontinuities */
#ifdef HAVE_STREAMING
	    networking_t*	networking; /**< callback for internet networking control */
#endif
	    Stream_Interface *driver; /**< low-level stream driver */
	    const stream_interface_info_t* driver_info;
	    stream_callback	event_handler;  /**< callback for streams which provide events */
	private:
	    stream_type_e	_type;
    } __attribute__ ((packed));

    int stream_enable_cache(stream_t *stream,libinput_t* libinput,int size,int min,int prefill);
    void stream_disable_cache(stream_t *stream);

/* this block describes interface to non-cache stream functions */
    extern int __FASTCALL__ nc_stream_read_cbuffer(stream_t *s);
    extern int __FASTCALL__ nc_stream_seek_long(stream_t *s,off_t pos);
    extern void __FASTCALL__ nc_stream_reset(stream_t *s);
    extern int __FASTCALL__ nc_stream_read_char(stream_t *s);
    extern int __FASTCALL__ nc_stream_read(stream_t *s,any_t* mem,int total);
    extern off_t __FASTCALL__ nc_stream_tell(stream_t *s);
    extern int __FASTCALL__ nc_stream_seek(stream_t *s,off_t pos);
    extern int __FASTCALL__ nc_stream_skip(stream_t *s,off_t len);

    extern MPXP_Rc __FASTCALL__ nc_stream_control(const stream_t *s,unsigned cmd,any_t*param);

/* this block describes interface to cache/non-cache stream functions */
    extern int __FASTCALL__ stream_read_char(stream_t *s);
    extern int __FASTCALL__ stream_read(stream_t *s,any_t* mem,int total);
    extern off_t __FASTCALL__ stream_tell(stream_t *s);
    extern int __FASTCALL__ stream_seek(stream_t *s,off_t pos);
    extern int __FASTCALL__ stream_skip(stream_t *s,off_t len);
    extern int __FASTCALL__ stream_eof(stream_t *s);
    extern void __FASTCALL__ stream_set_eof(stream_t *s,int eof);
    extern MPXP_Rc __FASTCALL__ stream_control(const stream_t *s,unsigned cmd,any_t*param);

    void __FASTCALL__ stream_reset(stream_t *s);
    stream_t* __FASTCALL__ new_stream(stream_type_e type);
    void __FASTCALL__ free_stream(stream_t *s);
    stream_t* __FASTCALL__ new_memory_stream(const unsigned char* data,int len);
    stream_t* __FASTCALL__ open_stream(libinput_t*libinput,const char* filename,int* file_format,stream_callback event_handler);

    extern unsigned int __FASTCALL__ stream_read_word(stream_t *s);
    extern unsigned int __FASTCALL__ stream_read_dword(stream_t *s);
    extern unsigned int __FASTCALL__ stream_read_word_le(stream_t *s);
    extern unsigned int __FASTCALL__ stream_read_dword_le(stream_t *s);
    extern uint64_t     __FASTCALL__ stream_read_qword(stream_t *s);
    extern uint64_t     __FASTCALL__ stream_read_qword_le(stream_t *s);
    extern unsigned int __FASTCALL__ stream_read_int24(stream_t *s);
    static inline uint32_t stream_read_fourcc(stream_t* s) { return stream_read_dword_le(s); }

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
