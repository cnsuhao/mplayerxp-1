#ifndef __STREAM_H
#define __STREAM_H

#include "../mp_config.h"

#include "demux_msg.h"
#include <inttypes.h>
#include <sys/types.h>

#define STREAM_BUFFER_SIZE 4096

#define STREAMTYPE_STREAM	0x00000000 /**< Stream resides on remote filesystem (doesn't provide seek functionality). */
#define STREAMTYPE_SEEKABLE	0x00000001 /**< Stream is seekable (resides on local filesystem). */
#define STREAMTYPE_MEMORY	0x00000002 /**< Stream is memory cache (doesn't provide seek/read functionality). */
#define STREAMTYPE_TEXT		0x00000004 /**< Stream is non-media stream (redirector, playlist, ...). */
#define STREAMTYPE_DS		0x00000008 /**< Stream is mapped on the other demuxer. */
#define STREAMTYPE_RAWAUDIO	0x00000010 /**< Stream contains raw audio without headers. */
#define STREAMTYPE_RAWVIDEO	0x00000020 /**< Stream contains raw video without headers. */
#define STREAMTYPE_PROGRAM	0x00000040 /**< Stream contains program (non-media) headers (VCD,DVD,http,...) */
#define STREAMTYPE_MENU		0x00000080 /**< Stream contains DVD menu... */

#define VCD_SECTOR_SIZE 2352
#define VCD_SECTOR_OFFS 24
#define VCD_SECTOR_DATA 2324

#ifdef HAVE_STREAMING
#include "network.h"
#endif

/** Stream packet description */
typedef struct {
    int	type;	/**< 0 - means raw data; other values depends on stream type */
    char *buf;  /**< buffer to be read */
    int len;	/**< length of buffer */
}stream_packet_t;

struct stream_s;
typedef void (* __FASTCALL__ stream_callback)(struct stream_s *s,const stream_packet_t *);

/** Stream description */
typedef struct stream_s{
  int fd;		/**< file handler */
  off_t pos;		/**< absolute offset from begin of stream */
  int eof;		/**< indicates EOF */
  int type;		/**< properties of the stream (see STREAMTYPE_ for detail) */
  int file_format;	/**< detected file format (by http:// protocol for example) */
  int _Errno;		/**< keeps value of errno from low-level operations */
  unsigned int buf_pos; /**< position whitin of small cache */
  unsigned int buf_len; /**< length of small cache */
  unsigned char *buffer;/**< buffer of small cache */
  off_t start_pos;	/**< real start of stream (without internet's headers) */
  off_t end_pos;	/**< real end of stream (media may be not fully filled) */
  unsigned sector_size; /**< alignment of read operations (1 for file, VCD_SECTOR_SIZE for VCDs) */
  void* cache_data;	/**< large cache */
  void* priv;		/**< private data used by stream driver */
  float stream_pts;	/**< PTS correction for idiotics DVD's discontinuities */
#ifdef HAVE_STREAMING
  streaming_ctrl_t *streaming_ctrl; /**< callback for internet streaming control */
#endif
  const struct stream_driver_s *driver; /**< low-level stream driver */
  stream_callback event_handler;  /**< callback for streams which provide events */
} stream_t;

int stream_enable_cache(stream_t *stream,int size,int min,int prefill);
void stream_disable_cache(stream_t *stream);

#include <string.h>

/* this block describes interface to non-cache stream functions */
extern int __FASTCALL__ nc_stream_read_cbuffer(stream_t *s);
extern int __FASTCALL__ nc_stream_seek_long(stream_t *s,off_t pos);
extern void __FASTCALL__ nc_stream_reset(stream_t *s);
extern int __FASTCALL__ nc_stream_read_char(stream_t *s);
extern int __FASTCALL__ nc_stream_read(stream_t *s,char* mem,int total);
extern off_t __FASTCALL__ nc_stream_tell(stream_t *s);
extern int __FASTCALL__ nc_stream_seek(stream_t *s,off_t pos);
extern int __FASTCALL__ nc_stream_skip(stream_t *s,off_t len);


/* this block describes interface to cache/non-cache stream functions */
extern int __FASTCALL__ stream_read_char(stream_t *s);
extern int __FASTCALL__ stream_read(stream_t *s,char* mem,int total);
extern off_t __FASTCALL__ stream_tell(stream_t *s);
extern int __FASTCALL__ stream_seek(stream_t *s,off_t pos);
extern int __FASTCALL__ stream_skip(stream_t *s,off_t len);
extern int __FASTCALL__ stream_eof(stream_t *s);
extern void __FASTCALL__ stream_set_eof(stream_t *s,int eof);

void __FASTCALL__ stream_reset(stream_t *s);
stream_t* __FASTCALL__ new_stream(int type);
void __FASTCALL__ free_stream(stream_t *s);
stream_t* __FASTCALL__ new_memory_stream(const unsigned char* data,int len);
stream_t* __FASTCALL__ open_stream(const char* filename,int* file_format,stream_callback event_handler);

#define stream_read_fourcc stream_read_dword_le
extern unsigned int __FASTCALL__ stream_read_word(stream_t *s);
extern unsigned int __FASTCALL__ stream_read_dword(stream_t *s);
extern unsigned int __FASTCALL__ stream_read_word_le(stream_t *s);
extern unsigned int __FASTCALL__ stream_read_dword_le(stream_t *s);
extern uint64_t     __FASTCALL__ stream_read_qword(stream_t *s);
extern uint64_t     __FASTCALL__ stream_read_qword_le(stream_t *s);
extern unsigned int __FASTCALL__ stream_read_int24(stream_t *s);

#include "../cfgparser.h"

/*
    Stream control definitions
*/
typedef struct {
  uint16_t sx, sy;
  uint16_t ex, ey;
} rect_highlight_t;

/* return codes: */
#define SCTRL_FALSE		0
#define SCTRL_OK		1
#define SCTRL_UNKNOWN		-1

/* These controls extracts text info from stream */
#define SCTRL_TXT_GET_STREAM_AUTHOR	1 /**< Returns author of stream. Accepts char *name as pointer on 256 bytes array  */
#define SCTRL_TXT_GET_STREAM_NAME	2 /**< Returns name of stream. Accepts char *name as pointer on 256 bytes array  */
#define SCTRL_TXT_GET_STREAM_SUBJECT	3 /**< Returns subject of stream. Accepts char *name as pointer on 256 bytes array  */
#define SCTRL_TXT_GET_STREAM_COPYRIGHT	4 /**< Returns copyright of stream. Accepts char *name as pointer on 256 bytes array  */
#define SCTRL_TXT_GET_STREAM_DESCRIPTION 5 /**< Returns description of stream. Accepts char *name as pointer on 256 bytes array  */
#define SCTRL_TXT_GET_STREAM_ALBUM	6 /**< Returns album of stream. Accepts char *name as pointer on 256 bytes array  */
#define SCTRL_TXT_GET_STREAM_DATE	7 /**< Returns date of stream. Accepts char *name as pointer on 256 bytes array  */
#define SCTRL_TXT_GET_STREAM_TRACK	8 /**< Returns track of stream. Accepts char *name as pointer on 256 bytes array  */
#define SCTRL_TXT_GET_STREAM_GENRE	9 /**< Returns genre of stream. Accepts char *name as pointer on 256 bytes array  */
#define SCTRL_TXT_GET_STREAM_ENCODER	10 /**< Returns encoder of stream. Accepts char *name as pointer on 256 bytes array  */
#define SCTRL_TXT_GET_STREAM_SOURCE_MEDIA 11 /**< Returns mediatype of stream. Accepts char *name as pointer on 256 bytes array  */
#define SCTRL_TXT_GET_STREAM_RATING	12 /**< Returns rating of stream. Accepts char *name as pointer on 256 bytes array  */
#define SCTRL_TXT_GET_STREAM_COMMENT	13 /**< Returns comments for stream. Accepts char *name as pointer on 256 bytes array  */
#define SCTRL_TXT_GET_STREAM_MIME	14 /**< Returns mimetype of stream. Accepts char *name as pointer on 256 bytes array  */

/* These controls extracts videospecific info from stream */
#define SCTRL_VID_GET_PALETTE		1000 /**< Returns palette array. Accepts unsigned** as pointer to palette array */
#define SCTRL_VID_GET_WIDTH		1001 /**< Returns width of raw video in pixels. Accepts unsigned* as pointer to storage area */
#define SCTRL_VID_GET_HEIGHT		1002 /**< Returns height of raw video in pixels. Accepts unsigned* as pointer to storage area */
#define SCTRL_VID_GET_FORMAT		1003 /**< Returns fourcc of raw video in pixels. Accepts unsigned* as pointer to storage area */
#define SCTRL_VID_GET_FPS		1004 /**< Returns Frames Per Seconds of raw video. Accepts float* as pointer to storage area */
#define SCTRL_VID_GET_HILIGHT		1005 /**< Returns HighLight area. Accepts (rect_hilight_t*) as argument */

/* These controls extracts audiospecific info from stream */
#define SCTRL_AUD_GET_CHANNELS		2000 /**< Returns number of channels. Accepts unsigned* as pointer to channels storage */
#define SCTRL_AUD_GET_SAMPLERATE	2001 /**< Returns rate of samples in Hz. Accepts unsigned* as pointer to storage area */
#define SCTRL_AUD_GET_SAMPLESIZE	2002 /**< Returns size fo samples in bits. Accepts unsigned* as pointer to storage area */
#define SCTRL_AUD_GET_FORMAT		2003 /**< Returns format of samples. Accepts unsigned* as pointer to storage area */

/* These controls extracts language specific info from stream */
#define SCTRL_LNG_GET_AID		3000 /**< Returns audio id from language. Accepts char* as input language name. Stores int* id into this pointer as output. */
#define SCTRL_LNG_GET_SID		3001 /**< Returns subtitle id from language. Accepts char* as input language name. Stores int* id into this pointer as output. */

/* These controls provide event handlig by stream driver */
#define SCRTL_EVT_HANDLE		4000 /**< Informs driver about stream specific event. Accepts const stream_packet_t* as input argument */
#define SCRTL_MPXP_CMD			4001 /**< Informs driver about input action (menu selection,tv channels switching,...) . Accepts int cmd as input argument */

#define SCTRL_EOF			10000

/** Stream-driver interface */
typedef struct stream_driver_s
{
    const char		*mrl;	/**< MRL of stream driver */
    const char		*descr;	/**< description of the driver */
		/** Opens stream with given name
		  * @param _this	points structure to be filled by driver
		  * @param filename	points MRL of stream (vcdnav://, file://, http://, ...)
		  * @param flags	currently unused and filled as 0
		  * @return		0 if stream can't be opened by this driver
		  *			1 - if stream was successfully opened
		**/
    int		(* __FASTCALL__ open)(stream_t *_this,const char *filename,unsigned flags);

		/** Reads next packet from stream
		  * @param _this	points structure which identifies stream
		  * @param sp		points to packet where stream data should be stored
		  * @return		length of readed information
		**/
    int		(* __FASTCALL__ read)(stream_t *_this,stream_packet_t * sp);

		/** Seeks on new stream position
		  * @param _this	points structure which identifies stream
		  * @param off		absolute offset from begin of stream
		  * @return		real offset after seeking
		**/
    off_t	(* __FASTCALL__ seek)(stream_t *_this,off_t off);

		/** Tells stream position
		  * @param _this	points structure which identifies stream
		  * @return		current offset from begin of stream
		**/
    off_t	(* __FASTCALL__ tell)(stream_t *_this);

		/** Closes stream
		  * @param _this	points structure which identifies stream
		**/
    void	(* __FASTCALL__ close)(stream_t *_this);

		/** Pass to driver player's commands (like ioctl)
		  * @param _this	points structure which identifies stream
		  * @param cmd		contains the command (for detail see SCTRL_* definitions)
		  * @return		result of command processing (SCTRL_OK, SCTRL_FALSE, ...)
		**/
    int		(* __FASTCALL__ control)(stream_t *_this,unsigned cmd,void *param);
}stream_driver_t;

void print_stream_drivers(void);

#define MAX_STREAM_PROTOCOLS 10
typedef struct stream_info_st {
  const char *info;
  const char *name;
  const char *author;
  const char *comment;
  /// mode isn't used atm (ie always READ) but it shouldn't be ignored
  /// opts is at least in it's defaults settings and may have been
  /// altered by url parsing if enabled and the options string parsing.
  int (*open)(struct stream_s* st, int mode, void* opts, int* file_format);
  char* protocols[MAX_STREAM_PROTOCOLS];
  void* opts;
  int opts_url; /* If this is 1 we will parse the url as an option string
		 * too. Otherwise options are only parsed from the
		 * options string given to open_stream_plugin */
} stream_info_t;


#endif // __STREAM_H
