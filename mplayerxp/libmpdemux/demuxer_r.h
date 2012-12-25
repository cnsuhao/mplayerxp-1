#ifndef __INCLUDE_DEMUXER_R
#define __INCLUDE_DEMUXER_R 1

#include <pthread.h>
#include <stdint.h>

#include "demuxer.h"
#include "stheader.h"

typedef enum enc_frame_type {
    AudioFrame=RND_NUMBER0,
    VideoFrame=RND_NUMBER1
}enc_frame_type_e;

struct enc_frame_t {
    enc_frame_type_e	type;
    float		pts;
    float		duration;
    unsigned		len;
    uint8_t*		data;
    unsigned		flags; // codec specific flags. filled by video_decode
};

extern	enc_frame_t*	new_enc_frame(enc_frame_type_e type,unsigned len,float pts,float duration);
extern	void		free_enc_frame(enc_frame_t* frame);

static inline int ds_tell_pts_r(Demuxer_Stream& ds) { return ds.tell_pts(); }

extern int demux_getc_r(Demuxer_Stream& ds,float& pts);
extern enc_frame_t* video_read_frame_r(sh_video_t* sh_video,int force_fps);
extern int demux_read_data_r(Demuxer_Stream& ds,unsigned char* mem,int len,float& pts);
extern int ds_get_packet_r(Demuxer_Stream& ds,unsigned char **start,float& pts);
extern int ds_get_packet_sub_r(Demuxer_Stream& ds,unsigned char **start);

extern int demux_seek_r(Demuxer& demuxer,const seek_args_t* seeka);
extern void vobsub_seek_r(any_t* vobhandle,const seek_args_t* seeka);

extern int demuxer_switch_audio_r(Demuxer&, int id);
extern int demuxer_switch_video_r(Demuxer&, int id);
extern int demuxer_switch_subtitle_r(Demuxer&, int id);

#endif
