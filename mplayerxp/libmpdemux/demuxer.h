#ifndef __DEMUXER_H
#define __DEMUXER_H 1
#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;

#include "xmpcore/xmp_enums.h"
#include "libmpconf/cfgparser.h"
#include "demuxer_packet.h"
#include "demuxer_stream.h"
#include "demuxer_info.h"

struct demuxer_driver_t;
extern int index_mode;  /**< -1=untouched  0=don't use index  1=use (geneate) index */
namespace mpxp {
#define MAX_PACK_BYTES (0x1024*0x1024*4)
/** Describes interface to stream associated with this demuxer */
    enum {
	DS_PIN=RND_NUMBER1+RND_CHAR1
    };

    enum {
	MAX_A_STREAMS	=256,
	MAX_V_STREAMS	=256,
	MAX_S_STREAMS	=256
    };

/** Describes demuxer (demultiplexer) of movie */
    enum {
	DEMUX_PIN=RND_NUMBER2+RND_CHAR2
    };

    enum {
	DEMUX_SEEK_CUR		=0x00,
	DEMUX_SEEK_SET		=0x01,
	DEMUX_SEEK_SECONDS	=0x00,
	DEMUX_SEEK_PERCENTS	=0x02
    };

    struct seek_args_t {
	float		secs;
	unsigned	flags;
    };

    struct sh_audio_t;
    struct sh_video_t;
    struct Demuxer : public Opaque {
	public:
	    Demuxer();
	    Demuxer(Stream *stream,int a_id,int v_id,int s_id);
	    virtual ~Demuxer();

	    enum demuxer_type_e {
		Type_UNKNOWN=0,
		Type_MPEG_ES,
		Type_MPEG4_ES,
		Type_H264_ES,
		Type_MPEG_PS,
		Type_MPEG_TS,
		Type_AVI,
		Type_ASF,
		Type_MOV,
		Type_REAL,
		Type_VIVO,
		Type_TV,
		Type_FLI,
		Type_Y4M,
		Type_NUV,
		Type_FILM,
		Type_ROQ,
		Type_AUDIO,
		Type_OGG,
		Type_BMP,
		Type_RAWAUDIO,
		Type_RAWVIDEO,
		Type_PVA,
		Type_SMJPEG,
		Type_NSV,
		Type_MATROSKA,
/*
 This should always match the higest demuxer type number.
 Unless you want to disallow users to force the demuxer to some types
*/
		Type_MIN	=0,
		Type_MAX	=Type_MATROSKA,
		Type_DEMUXERS=(1<<16),
/* A virtual demuxer type for the network code */
		Type_PLAYLIST=(2<<16)
	    };

	    enum demuxer_flags_e {
		NonSeekable=0,
		Seekable
	    };

	    /* Commands for control interface */
	    enum demuxr_cmd_e {
		Switch_Audio	=1,
		Switch_Video	=2,
		Switch_Subs	=3
	    };

	    static Demuxer*		open(Stream* stream,libinput_t& libinput,int aid,int vid,int sid);

	    virtual MPXP_Rc		open();
	    virtual int			demux(Demuxer_Stream* ds);

	    virtual Demuxer_Info&	info() const { return *_info; }
	    virtual int			fill_buffer(Demuxer_Stream *ds);
	    virtual int			seek(const seek_args_t* seeka);

	    virtual sh_audio_t*		get_sh_audio(int id=0) const;
	    virtual sh_video_t*		get_sh_video(int id=0) const;
	    virtual char		get_sh_sub(int id=0) const;
	    virtual sh_audio_t*		new_sh_audio_aid(int id,int aid);
	    sh_audio_t*			new_sh_audio(int i=0) { return new_sh_audio_aid(i, i); }
	    virtual sh_video_t*		new_sh_video_vid(int id,int vid);
	    sh_video_t*			new_sh_video(int i=0) { return new_sh_video_vid(i, i); }
	    virtual char		new_sh_sub(int i=0);

	    virtual int			switch_audio(int id) const;
	    virtual int			switch_video(int id) const;
	    virtual int			switch_subtitle(int id) const;
	    virtual MPXP_Rc		ctrl(int cmd, any_t*arg) const;

	    Opaque		unusable;
	    unsigned		pin;		/**< personal identification number */
	    Stream*		stream;		/**< stream for movie reading */
	    Demuxer_Stream*	audio;		/**< audio buffer/demuxer */
	    Demuxer_Stream*	video;		/**< video buffer/demuxer */
	    Demuxer_Stream*	sub;		/**< DVD's subtitle buffer/demuxer */
	    off_t		filepos;	/**< current pos. of input stream */
	    off_t		movi_start;	/**< real start of movie within of stream */
	    off_t		movi_end;	/**< real end of movie within of stream */
	    unsigned		movi_length;	/**< length of movie in secs. Optional!*/
	    demuxer_flags_e	flags;		/**< set of DEMUXF_* bits */
	    demuxer_type_e	file_format;	/**< file format: Type_*(mpeg/avi/asf). Will be replaced with properties in the further versions */
	    int			synced;		/**< indicates stream synchronisation. TODO: mpg->priv */
	    Opaque*		priv;		/**< private data of demuxer's driver.*/
	private:
	    void _init(Stream *_stream,int a_id,int v_id,int s_id);

	    LocalPtr<Opaque>		demuxer_priv;
	    LocalPtr<Demuxer_Info>	_info;	/**< human-readable info from stream/movie (like movie name,author,duration)*/
    };
    inline Demuxer::demuxer_flags_e operator~(Demuxer::demuxer_flags_e a) { return static_cast<Demuxer::demuxer_flags_e>(~static_cast<unsigned>(a)); }
    inline Demuxer::demuxer_flags_e operator|(Demuxer::demuxer_flags_e a, Demuxer::demuxer_flags_e b) { return static_cast<Demuxer::demuxer_flags_e>(static_cast<unsigned>(a)|static_cast<unsigned>(b)); }
    inline Demuxer::demuxer_flags_e operator&(Demuxer::demuxer_flags_e a, Demuxer::demuxer_flags_e b) { return static_cast<Demuxer::demuxer_flags_e>(static_cast<unsigned>(a)&static_cast<unsigned>(b)); }
    inline Demuxer::demuxer_flags_e operator^(Demuxer::demuxer_flags_e a, Demuxer::demuxer_flags_e b) { return static_cast<Demuxer::demuxer_flags_e>(static_cast<unsigned>(a)^static_cast<unsigned>(b)); }
    inline Demuxer::demuxer_flags_e operator|=(Demuxer::demuxer_flags_e a, Demuxer::demuxer_flags_e b) { return (a=static_cast<Demuxer::demuxer_flags_e>(static_cast<unsigned>(a)|static_cast<unsigned>(b))); }
    inline Demuxer::demuxer_flags_e operator&=(Demuxer::demuxer_flags_e a, Demuxer::demuxer_flags_e b) { return (a=static_cast<Demuxer::demuxer_flags_e>(static_cast<unsigned>(a)&static_cast<unsigned>(b))); }
    inline Demuxer::demuxer_flags_e operator^=(Demuxer::demuxer_flags_e a, Demuxer::demuxer_flags_e b) { return (a=static_cast<Demuxer::demuxer_flags_e>(static_cast<unsigned>(a)^static_cast<unsigned>(b))); }

    Demuxer*			new_demuxers_demuxer(Demuxer* vd, Demuxer* ad, Demuxer* sd);
}// namespace mpxp
#endif
