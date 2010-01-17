#ifndef __ST_HEADER_H
#define __ST_HEADER_H 1

// Stream headers:

#include "loader/wine/mmreg.h"
#include "loader/wine/avifmt.h"
#include "loader/wine/vfw.h"

#include "../mp_image.h"

typedef struct {
  int aid;
  demux_stream_t *ds;
  unsigned int format;
  struct codecs_st *codec;
  int inited;
  // output format:
  float timer;		   // value of old a_frame
  unsigned samplerate;
  unsigned samplesize;
  unsigned channels;
  unsigned o_bps; // == samplerate*samplesize*channels   (uncompr. bytes/sec)
  unsigned i_bps; // == bitrate  (compressed bytes/sec)
  // in buffers:
  char* a_in_buffer;
  int a_in_buffer_len;
  unsigned a_in_buffer_size;

  // out buffers:
  char* a_buffer;
  int a_buffer_len;
  unsigned a_buffer_size;

  /* filter buffer */
  void *   afilter;
  int      afilter_inited;
  unsigned af_bps; // == samplerate*samplesize*channels   (after filters bytes/sec)
  char *   af_buffer;
  unsigned af_buffer_len;
  float    af_pts;

  int sample_format;
  float a_pts;
  int a_pts_pos;
  int chapter_change;
  // win32 codec stuff:
  AVIStreamHeader audio;
  WAVEFORMATEX *wf;
//  char wf_ext[64];     // in format
  unsigned audio_in_minsize;
  unsigned audio_out_minsize;
  // other codecs:
  void* context; // codec-specific stuff (usually HANDLE or struct pointer)
  unsigned char *codecdata;
  unsigned codecdata_len;
} sh_audio_t;

typedef struct {
  int vid;
  demux_stream_t *ds;
  unsigned int format;
  struct codecs_st *codec;
  int inited;
  int is_static; /* default: 0 - means movie; 1 - means picture (.jpg ...)*/
  // output format:
  float timer;		   // value of old v_frame
  float fps;
  float frametime;  // 1/fps
  int chapter_change;
  unsigned i_bps; // == bitrate  (compressed bytes/sec)
  unsigned disp_w,disp_h;// display size (filled by fileformat parser)
//  int coded_w,coded_h; // coded size (filled by video codec)
  float aspect;
  unsigned int outfmtidx;
  /* vfilter chan */
  void *vfilter;
  int   vfilter_inited;
  int   vf_flags;
  pthread_mutex_t mutex;
  unsigned active_slices;
//  unsigned int bitrate;
  // buffers:
  float num_frames;       // number of frames played
  int num_frames_decoded; // number of frames decoded
  mp_image_t *image;
  // win32 codec stuff:
  AVIStreamHeader video;
  BITMAPINFOHEADER *bih;   // in format
  void* context; // codec-specific stuff (usually HANDLE or struct pointer)
  void* ImageDesc; // for quicktime codecs
} sh_video_t;

sh_audio_t* get_sh_audio(demuxer_t *demuxer,int id);
sh_video_t* get_sh_video(demuxer_t *demuxer,int id);
#define new_sh_audio(d, i) new_sh_audio_aid(d, i, i)
sh_audio_t* new_sh_audio_aid(demuxer_t *demuxer,int id,int aid);
#define new_sh_video(d, i) new_sh_video_vid(d, i, i)
sh_video_t* new_sh_video_vid(demuxer_t *demuxer,int id,int vid);
void free_sh_audio(sh_audio_t *sh);
void free_sh_video(sh_video_t *sh);

int video_read_properties(sh_video_t *sh_video);
int video_read_frame(sh_video_t* sh_video,float* frame_time_ptr,float *v_pts,unsigned char** start,int force_fps);

#endif
