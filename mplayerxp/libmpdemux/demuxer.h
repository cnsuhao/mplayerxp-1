#ifndef __DEMUXER_H
#define __DEMUXER_H 1

#define MAX_PACK_BYTES (0x1024*0x1024*4)

#define DEMUXER_TYPE_UNKNOWN	0
#define DEMUXER_TYPE_MPEG_ES	1
#define DEMUXER_TYPE_MPEG4_ES	2
#define DEMUXER_TYPE_H264_ES	3
#define DEMUXER_TYPE_MPEG_PS	4
#define DEMUXER_TYPE_MPEG_TS	5
#define DEMUXER_TYPE_AVI	6
#define DEMUXER_TYPE_ASF	7
#define DEMUXER_TYPE_MOV	8
#define DEMUXER_TYPE_REAL	9
#define DEMUXER_TYPE_VIVO	10
#define DEMUXER_TYPE_TV		11
#define DEMUXER_TYPE_FLI	12
#define DEMUXER_TYPE_Y4M	13
#define DEMUXER_TYPE_NUV	14
#define DEMUXER_TYPE_FILM	15
#define DEMUXER_TYPE_ROQ	16
#define DEMUXER_TYPE_AUDIO	17
#define DEMUXER_TYPE_OGG	18
#define DEMUXER_TYPE_BMP	19
#define DEMUXER_TYPE_RAWAUDIO	20
#define DEMUXER_TYPE_RAWVIDEO	21
#define DEMUXER_TYPE_PVA	22
#define DEMUXER_TYPE_SMJPEG	23
#define DEMUXER_TYPE_NSV	24
#define DEMUXER_TYPE_MATROSKA   25
/*
 This should always match the higest demuxer type number.
 Unless you want to disallow users to force the demuxer to some types
*/
#define DEMUXER_TYPE_MIN	0
#define DEMUXER_TYPE_MAX	25


#define DEMUXER_TYPE_DEMUXERS (1<<16)
/* A virtual demuxer type for the network code */
#define DEMUXER_TYPE_PLAYLIST (2<<16)


#define DEMUXER_TIME_NONE 0
#define DEMUXER_TIME_PTS 1
#define DEMUXER_TIME_FILE 2
#define DEMUXER_TIME_BPS 3

#define DP_NONKEYFRAME	0x00000000UL
#define DP_KEYFRAME	0x00000001UL

/** Describes demuxer's packet */
typedef struct demux_packet_st {
    int			len;	/**< length of packet's data */
    float		pts;	/**< Presentation Time-Stamp (PTS) of data */
    off_t		pos;	/**< Position in index (AVI) or file (MPG) */
    unsigned char*	buffer; /**< buffer of packet's data */
    int			flags;	/**< 1 - indicates keyframe, 0 - regular frame */
    struct demux_packet_st* next; /**< pointer to the next packet in chain */
} demux_packet_t;

/** Describes interface to stream associated with this demuxer */
typedef struct demux_stream_s {
    int			id;		/**< stream ID  (for multiple audio/video streams) */
    int			buffer_pos;	/**< current buffer position */
    int			buffer_size;	/**< current buffer size */
    unsigned char*	buffer;		/**< current buffer */
    float		pts;		/**< current buffer's PTS */
    int			pts_bytes;	/**< number of bytes read after last pts stamp */
    int			eof;		/**< end of demuxed stream? (true if all buffer empty) */
    off_t		pos;		/**< position in the input stream (file) */
    off_t		dpos;		/**< position in the demuxed stream */
    int			pack_no;	/**< serial number of packet */
    int			flags;		/**< flags of current packet (keyframe etc) */
/*---------------*/
    int			packs;		/**< number of packets in buffer */
    int			bytes;		/**< total bytes of packets in buffer */
    demux_packet_t*	first;		/**< read to current buffer from here */
    demux_packet_t*	last;		/**< append new packets from input stream to here */
    demux_packet_t*	current;	/**< needed for refcounting of the buffer */
    struct demuxer_s*	demuxer;	/**< parent demuxer structure (stream handler) */
/* ---- asf ----- */
    demux_packet_t*	asf_packet;	/**< read asf fragments here */
    int			asf_seq;	/**< sequence id associated with asf_packet */
/*---------------*/
    any_t*		sh;		/**< Stream header associated with this stream (@see st_header.h for detail) */
/*---------------*/
    float		prev_pts;	/**< PTS of previous packet (DVD's PTS correction) */
    float		pts_corr;	/**< PTS correction (DVD's PTS correction) */
    int			pts_flags;	/**< PTS flags like trigger for correction applying (DVD's PTS correction) */
} demux_stream_t;

extern demux_stream_t *d_audio;
extern demux_stream_t *d_dvdsub;

#define MAX_A_STREAMS 256
#define MAX_V_STREAMS 256
#define MAX_S_STREAMS 256


#define DEMUXF_SEEKABLE 0x00000001UL
/** Describes demuxer (demultiplexer) of movie */
typedef struct demuxer_s {
    stream_t*		stream;		/**< stream for movie reading */
    demux_stream_t*	audio;		/**< audio buffer/demuxer */
    demux_stream_t*	video;		/**< video buffer/demuxer */
    demux_stream_t*	sub;		/**< DVD's subtitle buffer/demuxer */
    any_t*		a_streams[MAX_A_STREAMS]; /**< audio streams (sh_audio_t) for multilanguage movies */
    any_t*		v_streams[MAX_V_STREAMS]; /**< video streams (sh_video_t) for multipicture movies  */
    char		s_streams[MAX_S_STREAMS]; /**< DVD's subtitles (flag) streams for multilanguage movies */
    off_t		filepos;	/**< current pos. of input stream */
    off_t		movi_start;	/**< real start of movie within of stream */
    off_t		movi_end;	/**< real end of movie within of stream */
    unsigned		movi_length;	/**< length of movie in secs. Optional!*/
    unsigned		flags;		/**< set of DEMUXF_* bits */
    unsigned		file_format;	/**< file format: DEMUXER_TYPE_*(mpeg/avi/asf). Will be replaced with properties in the further versions */
    int			synced;		/**< indicates stream synchronisation. TODO: mpg->priv */

    any_t*		priv;		/**< private data of demuxer's driver.*/
    any_t*		info;		/**< human-readable info from stream/movie (like movie name,author,duration)*/
    struct demuxer_driver_s* driver;	/**< driver associated with this demuxer */
} demuxer_t;

/* Return values for control interface */
#define DEMUX_UNKNOWN		-1
#define DEMUX_FALSE		0
#define DEMUX_OK		1

#define DEMUX_SEEK_CUR		0x00
#define DEMUX_SEEK_SET		0x01
#define DEMUX_SEEK_SECONDS	0x00
#define DEMUX_SEEK_PERCENTS	0x02
typedef struct seek_args_s {
    float	secs;
    unsigned	flags;
}seek_args_t;

/* Commands for control interface */
#define DEMUX_CMD_SWITCH_AUDIO		1
#define DEMUX_CMD_SWITCH_VIDEO		2
#define DEMUX_CMD_SWITCH_SUBS		3
/** Demuxer's driver interface */
typedef struct demuxer_driver_s
{
    const char *	name;	/**< Name of driver ("Matroska MKV parser") */
    const char *	defext; /**< Default file extension for this movie type */
    const any_t*	options;/**< Optional: MPlayerXP's option related */
			/** Probing stream.
			  * @param d	_this demuxer
			  * @return	0 - fail; 1 if driver may handle this stream
			 **/
    int			(*probe)(demuxer_t *d);
			/** Opens stream.
			  * @param d	_this demxuer
			  * @return	0 - fail; 1 - successful
			 **/
    demuxer_t*		(*open)(demuxer_t *d);
			/** Reads and demuxes stream.
			 * @param d	_this demuxer
			 * @param ds	pointer to stream associated with demuxer
			 * @return	0 - EOF or no stream found; 1 - if packet was successfully readed */
    int			(*demux)(demuxer_t *d,demux_stream_t *ds);
			/** Seeks within of stream.
			 * @param d 		_thid demuxer
			 * @param rel_seek_secs	position in seconds from begin of stream
			 * @param flags		0x01 - seek from start else seek_cur, 0x02 - rel_seek_secs indicates pos in percents/100 else in seconds
			 * @note		this function is optional and maybe NULL
			**/
    void		(*seek)(demuxer_t *d,const seek_args_t* seeka);
			/** Closes driver
			  * @param d	_this demuxer
			 **/
    void		(*close)(demuxer_t *d);
			/** Control interface to demuxer
			  * @param d	_this demuxer
			  * @param cmd	command to be execute (one of DEMUX_CMD_*)
			  * @param arg	optional arguments for thsis command
			  * @return	one of DEMUX_* states
			 **/
    int			(*control)(demuxer_t *d,int cmd,any_t*arg);
}demuxer_driver_t;

inline static demux_packet_t* new_demux_packet(int len){
  demux_packet_t* dp=malloc(sizeof(demux_packet_t));
  dp->len=len;
  dp->buffer=malloc(len);
  dp->next=NULL;
  dp->pts=0;
  dp->pos=0;
  dp->flags=0;
  return dp;
}

inline static void free_demux_packet(demux_packet_t* dp){
  free(dp->buffer);
  free(dp);
}

demux_packet_t* clone_demux_packet(demux_packet_t* pack);
demux_stream_t* new_demuxer_stream(struct demuxer_s *demuxer,int id);
demuxer_t* new_demuxer(stream_t *stream,int type,int a_id,int v_id,int s_id);
void free_demuxer_stream(demux_stream_t *ds);
#define FREE_DEMUXER_STREAM(d) { free_demuxer_stream(d); d=NULL; }
void free_demuxer(demuxer_t *demuxer);
#define FREE_DEMUXER(d) { free_demuxer(d); d=NULL; }

void ds_add_packet(demux_stream_t *ds,demux_packet_t* dp);
void ds_read_packet(demux_stream_t *ds,stream_t *stream,int len,float pts,off_t pos,int flags);

int demux_fill_buffer(demuxer_t *demux,demux_stream_t *ds);
int ds_fill_buffer(demux_stream_t *ds);

inline static off_t ds_tell(demux_stream_t *ds){
  return (ds->dpos-ds->buffer_size)+ds->buffer_pos;
}

inline static int ds_tell_pts(demux_stream_t *ds){
  return (ds->pts_bytes-ds->buffer_size)+ds->buffer_pos;
}

int demux_read_data(demux_stream_t *ds,unsigned char* mem,int len);

#if 1
#define demux_getc(ds) (\
     (ds->buffer_pos<ds->buffer_size) ? ds->buffer[ds->buffer_pos++] \
     :((!ds_fill_buffer(ds))? (-1) : ds->buffer[ds->buffer_pos++] ) )
#else
inline static int demux_getc(demux_stream_t *ds){
  if(ds->buffer_pos>=ds->buffer_size){
    if(!ds_fill_buffer(ds)){
      return -1; // EOF
    }
  }
  return ds->buffer[ds->buffer_pos++];
}
#endif

void ds_free_packs(demux_stream_t *ds);
void ds_free_packs_until_pts(demux_stream_t *ds,float pts);
int ds_get_packet(demux_stream_t *ds,unsigned char **start);
int ds_get_packet_sub(demux_stream_t *ds,unsigned char **start);
float ds_get_next_pts(demux_stream_t *ds);

// This is defined here because demux_stream_t ins't defined in stream.h
stream_t* __FASTCALL__ new_ds_stream(demux_stream_t *ds);

inline static void resize_demux_packet(demux_packet_t* dp, int len)
{
  if(dp->len!=len)
  {
    if(len)
    {
	dp->buffer=(unsigned char *)realloc(dp->buffer,len+8);
	memset(dp->buffer+len,0,8);
    }
    else
    {
	if(dp->buffer) free(dp->buffer);
	dp->buffer=NULL;
    }
    dp->len=len;
  }
}

demuxer_t* demux_open(stream_t *stream,int file_format,int aid,int vid,int sid);
int demux_seek(demuxer_t *demuxer,const seek_args_t* seeka);
demuxer_t*  new_demuxers_demuxer(demuxer_t* vd, demuxer_t* ad, demuxer_t* sd);

/* AVI demuxer params: */
extern int index_mode;  /**< -1=untouched  0=don't use index  1=use (geneate) index */
extern int force_ni;
extern int pts_from_bps;
extern int demux_aid_vid_mismatch;

#define INFOT_NULL		0
#define INFOT_AUTHOR		1
#define INFOT_NAME		2
#define INFOT_SUBJECT		3
#define INFOT_COPYRIGHT		4
#define INFOT_DESCRIPTION	5
#define INFOT_ALBUM		6
#define INFOT_DATE		7
#define INFOT_TRACK		8
#define INFOT_GENRE		9
#define INFOT_ENCODER		10
#define INFOT_SOURCE_MEDIA	11
#define INFOT_WWW		12
#define INFOT_MAIL		13
#define INFOT_RATING		14
#define INFOT_COMMENTS		15
#define INFOT_MIME		16

#define INFOT_MAX		16
int demux_info_add(demuxer_t *demuxer, unsigned opt, const char *param);
const char* demux_info_get(demuxer_t *demuxer, unsigned opt);
int demux_info_print(demuxer_t *demuxer,const char *filename);
void demux_info_free(demuxer_t *demuxer);

extern int demuxer_switch_audio(demuxer_t *, int id);
extern int demuxer_switch_video(demuxer_t *, int id);
extern int demuxer_switch_subtitle(demuxer_t *, int id);

#endif
