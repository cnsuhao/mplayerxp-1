#ifndef __ST_HEADER_H
#define __ST_HEADER_H 1

// Stream headers:
#ifdef __cplusplus
extern "C" {
#endif
#include "loader/wine/mmreg.h"
#include "loader/wine/avifmt.h"
#include "loader/wine/vfw.h"
#ifdef __cplusplus
}
#endif
#include "xmpcore/mp_image.h"
#include "demuxer_stream.h"

struct ImageDescription;
struct af_stream_t;
struct codecs_st;
namespace mpxp {
    struct standard_header : public Opaque {
	public:
	    standard_header() {}
	    virtual ~standard_header() {}

	int			id;
	Demuxer_Stream*		ds;
	struct codecs_st*	codec;
	int			inited;
    };

    struct sh_audio_t : public standard_header  {
	public:
	    sh_audio_t() {}
	    virtual ~sh_audio_t();

    // input format
	uint32_t	wtag;  // analogue of fourcc for sound
	unsigned	i_bps; // == bitrate  (compressed bytes/sec)

    // output format:
	float		timer;  // value of old a_frame
	unsigned	rate;  // sample rate
	unsigned	afmt;  // sample format
	unsigned	nch;   // number of chanels
	unsigned	o_bps; // == rate*afmt2bps*nch   (uncompr. bytes/sec)
    // in buffers:
	char*		a_in_buffer;
	int		a_in_buffer_len;
	unsigned	a_in_buffer_size;

    // out buffers:
	char*		a_buffer;
	int		a_buffer_len;
	unsigned	a_buffer_size;

    /* filter buffer */
	unsigned	af_bps; // == samplerate*samplesize*channels   (after filters bytes/sec)
	char*		af_buffer;
	unsigned	af_buffer_len;
	float		af_pts;

	float		a_pts;
	int		a_pts_pos;
	int		chapter_change;
// win32 codec stuff:
	AVIStreamHeader	audio;
	WAVEFORMATEX*	wf;
//  char wf_ext[64];     // in format
	unsigned	audio_in_minsize;
	unsigned	audio_out_minsize;
// other codecs:
	unsigned char*	codecdata;
	unsigned	codecdata_len;
    };

    struct sh_video_t : public standard_header {
	public:
	    sh_video_t() {}
	    virtual ~sh_video_t();

    // input format
	uint32_t	fourcc;
	int		is_static; /* default: 0 - means movie; 1 - means picture (.jpg ...)*/

// output format:
	float		fps;
	int		chapter_change;
	unsigned	src_w,src_h;// source picture size (filled by fileformat parser)
	float		aspect;
	unsigned int	outfmtidx; // TODO: replace with out_fourcc

// win32 codec stuff:
	AVIStreamHeader	video;
	BITMAPINFOHEADER*bih;		// in format
	ImageDescription*ImageDesc;	// for quicktime codecs
    };
    int video_read_properties(sh_video_t *sh_video);
    int video_read_frame(sh_video_t* sh_video,float* frame_time_ptr,float *v_pts,unsigned char** start,int force_fps);
} //namespace mpxp
#endif

