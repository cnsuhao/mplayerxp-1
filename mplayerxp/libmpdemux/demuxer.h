#ifndef __DEMUXER_H
#define __DEMUXER_H 1
#include "libmpstream/stream.h"
#include "xmpcore/xmp_enums.h"
#include "libmpconf/cfgparser.h"
#include "demuxer_packet.h"
#include "demuxer_stream.h"
#include "demuxer_info.h"

#define MAX_PACK_BYTES (0x1024*0x1024*4)
enum {
    DEMUXER_TYPE_UNKNOWN=0,
    DEMUXER_TYPE_MPEG_ES,
    DEMUXER_TYPE_MPEG4_ES,
    DEMUXER_TYPE_H264_ES,
    DEMUXER_TYPE_MPEG_PS,
    DEMUXER_TYPE_MPEG_TS,
    DEMUXER_TYPE_AVI,
    DEMUXER_TYPE_ASF,
    DEMUXER_TYPE_MOV,
    DEMUXER_TYPE_REAL,
    DEMUXER_TYPE_VIVO,
    DEMUXER_TYPE_TV,
    DEMUXER_TYPE_FLI,
    DEMUXER_TYPE_Y4M,
    DEMUXER_TYPE_NUV,
    DEMUXER_TYPE_FILM,
    DEMUXER_TYPE_ROQ,
    DEMUXER_TYPE_AUDIO,
    DEMUXER_TYPE_OGG,
    DEMUXER_TYPE_BMP,
    DEMUXER_TYPE_RAWAUDIO,
    DEMUXER_TYPE_RAWVIDEO,
    DEMUXER_TYPE_PVA,
    DEMUXER_TYPE_SMJPEG,
    DEMUXER_TYPE_NSV,
    DEMUXER_TYPE_MATROSKA,
/*
 This should always match the higest demuxer type number.
 Unless you want to disallow users to force the demuxer to some types
*/
    DEMUXER_TYPE_MIN	=0,
    DEMUXER_TYPE_MAX	=DEMUXER_TYPE_MATROSKA,
    DEMUXER_TYPE_DEMUXERS=(1<<16),
/* A virtual demuxer type for the network code */
    DEMUXER_TYPE_PLAYLIST=(2<<16)
};
enum {
    DEMUXER_TIME_NONE	=0,
    DEMUXER_TIME_PTS	=1,
    DEMUXER_TIME_FILE	=2,
    DEMUXER_TIME_BPS	=3
};

/** Describes interface to stream associated with this demuxer */
enum {
    DS_PIN=RND_NUMBER1+RND_CHAR1
};

enum {
    MAX_A_STREAMS	=256,
    MAX_V_STREAMS	=256,
    MAX_S_STREAMS	=256
};
enum {
    DEMUXF_SEEKABLE	=0x00000001UL
};
/** Describes demuxer (demultiplexer) of movie */
enum {
    DEMUX_PIN=RND_NUMBER2+RND_CHAR2
};

struct demuxer_driver_t;
struct demuxer_t : public Opaque {
    public:
	demuxer_t();
	virtual ~demuxer_t();

	Demuxer_Info&	info() const { return *_info; }

	char		antiviral_hole[RND_CHAR3];
	unsigned	pin;		/**< personal identification number */
	stream_t*	stream;		/**< stream for movie reading */
	Demuxer_Stream*	audio;		/**< audio buffer/demuxer */
	Demuxer_Stream*	video;		/**< video buffer/demuxer */
	Demuxer_Stream*	sub;		/**< DVD's subtitle buffer/demuxer */
	any_t*		a_streams[MAX_A_STREAMS]; /**< audio streams (sh_audio_t) for multilanguage movies */
	any_t*		v_streams[MAX_V_STREAMS]; /**< video streams (sh_video_t) for multipicture movies  */
	char		s_streams[MAX_S_STREAMS]; /**< DVD's subtitles (flag) streams for multilanguage movies */
	off_t		filepos;	/**< current pos. of input stream */
	off_t		movi_start;	/**< real start of movie within of stream */
	off_t		movi_end;	/**< real end of movie within of stream */
	unsigned	movi_length;	/**< length of movie in secs. Optional!*/
	unsigned	flags;		/**< set of DEMUXF_* bits */
	unsigned	file_format;	/**< file format: DEMUXER_TYPE_*(mpeg/avi/asf). Will be replaced with properties in the further versions */
	int		synced;		/**< indicates stream synchronisation. TODO: mpg->priv */

	Opaque*		priv;		/**< private data of demuxer's driver.*/
	const demuxer_driver_t* driver;	/**< driver associated with this demuxer */
    private:
	LocalPtr<Demuxer_Info>	_info;	/**< human-readable info from stream/movie (like movie name,author,duration)*/
};

enum {
    DEMUX_SEEK_CUR	=0x00,
    DEMUX_SEEK_SET	=0x01,
    DEMUX_SEEK_SECONDS	=0x00,
    DEMUX_SEEK_PERCENTS	=0x02
};
typedef struct seek_args_s {
    float	secs;
    unsigned	flags;
}seek_args_t;

/* Commands for control interface */
enum {
    DEMUX_CMD_SWITCH_AUDIO	=1,
    DEMUX_CMD_SWITCH_VIDEO	=2,
    DEMUX_CMD_SWITCH_SUBS	=3
};

demuxer_t* new_demuxer(stream_t *stream,int a_id,int v_id,int s_id);
void free_demuxer(demuxer_t *demuxer);

int demux_fill_buffer(demuxer_t *demux,Demuxer_Stream *ds);

// This is defined here because demux_stream_t ins't defined in stream.h
stream_t* __FASTCALL__ new_ds_stream(Demuxer_Stream *ds);

int demux_seek(demuxer_t *demuxer,const seek_args_t* seeka);
demuxer_t*  new_demuxers_demuxer(demuxer_t* vd, demuxer_t* ad, demuxer_t* sd);

/* AVI demuxer params: */
extern int index_mode;  /**< -1=untouched  0=don't use index  1=use (geneate) index */

extern int demuxer_switch_audio(const demuxer_t *, int id);
extern int demuxer_switch_video(const demuxer_t *, int id);
extern int demuxer_switch_subtitle(const demuxer_t *, int id);

demuxer_t* demux_open(stream_t *stream,int aid,int vid,int sid);

#endif
