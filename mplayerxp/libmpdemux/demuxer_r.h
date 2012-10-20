#ifndef __INCLUDE_DEMUXER_R
#define __INCLUDE_DEMUXER_R 1

#include <pthread.h>

#include "stream.h"
#include "demuxer.h"
#include "stheader.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ds_tell_pts_r(ds) ds_tell_pts(ds)

extern int demux_getc_r(demux_stream_t *ds,float *pts);
extern int video_read_frame_r(sh_video_t* sh_video,float* frame_time_ptr,float *v_pts,unsigned char** start,int force_fps);
extern int demux_read_data_r(demux_stream_t *ds,unsigned char* mem,int len,float *pts);
extern int ds_get_packet_r(demux_stream_t *ds,unsigned char **start,float *pts);

extern int demux_seek_r(demuxer_t *demuxer,const seek_args_t* seeka);
extern void vobsub_seek_r(any_t* vobhandle, float pts);

extern int demuxer_switch_audio_r(demuxer_t *, int id);
extern int demuxer_switch_video_r(demuxer_t *, int id);
extern int demuxer_switch_subtitle_r(demuxer_t *, int id);
#ifdef __cplusplus
}
#endif

#endif
