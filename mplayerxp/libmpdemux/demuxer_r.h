#ifndef __INCLUDE_DEMUXER_R
#define __INCLUDE_DEMUXER_R 1

#include <pthread.h>

#include "libmpstream/stream.h"
#include "demuxer.h"
#include "stheader.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum enc_frame_type {
    AudioFrame=RND_NUMBER0,
    VideoFrame=RND_NUMBER1
}enc_frame_type_e;

typedef struct enc_frame_s {
    enc_frame_type_e	type;
    float		pts;
    float		duration;
    unsigned		len;
    any_t*		data;
    unsigned		flags; // codec specific flags. filled by video_decode
}enc_frame_t;

extern	enc_frame_t*	new_enc_frame(enc_frame_type_e type,unsigned len,float pts,float duration);
extern	void		free_enc_frame(enc_frame_t* frame);

static inline int ds_tell_pts_r(demux_stream_t *ds) { return ds_tell_pts(ds); }

extern int demux_getc_r(demux_stream_t *ds,float *pts);
extern enc_frame_t* video_read_frame_r(sh_video_t* sh_video,int force_fps);
extern int demux_read_data_r(demux_stream_t *ds,unsigned char* mem,int len,float *pts);
extern int ds_get_packet_r(demux_stream_t *ds,unsigned char **start,float *pts);
extern int ds_get_packet_sub_r(demux_stream_t *ds,unsigned char **start);

extern int demux_seek_r(demuxer_t *demuxer,const seek_args_t* seeka);
extern void vobsub_seek_r(any_t* vobhandle,const seek_args_t* seeka);

extern int demuxer_switch_audio_r(demuxer_t *, int id);
extern int demuxer_switch_video_r(demuxer_t *, int id);
extern int demuxer_switch_subtitle_r(demuxer_t *, int id);
#ifdef __cplusplus
}
#endif

#endif
