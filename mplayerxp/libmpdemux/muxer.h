#ifndef MUXER_H_INCLUDED
#define MUXER_H_INCLUDED 1

enum {
    MUXER_MAX_STREAMS	=16,

    MUXER_TYPE_VIDEO	=0,
    MUXER_TYPE_AUDIO	=1,
    MUXER_TYPE_SUBS	=2,

    MUXER_TYPE_RAW	=0,
    MUXER_TYPE_MPXP64	=1,
    MUXER_TYPE_LAVF	=2,

    MUXER_MPEG_BLOCKSIZE=2048 // 2048 or 2324 - ?
};
#include "demuxer_r.h"

typedef struct muxer_packet_s{
    float pts;
    any_t*data;
    unsigned length;
    unsigned flags;
    struct muxer_packet_s *next;
}muxer_packet_t;

muxer_packet_t* new_muxer_packet(float pts,any_t*data,unsigned length,unsigned flags);
void free_muxer_packet(muxer_packet_t *packet);

typedef struct {
  // muxer data:
  int type;  // audio or video
  int id;    // stream no
  // to compute AveBitRate:
  off_t		size;
  double	timer;
  // buffering:
  unsigned char *buffer;
  unsigned int buffer_size;
  unsigned int buffer_len;

  // source stream:
  any_t* source; // sh_audio or sh_video
  int codec; // codec used for encoding. 0 means copy
  // avi stream header:
  AVIStreamHeader h;  // Rate/Scale and SampleSize must be filled by caller!
  // stream specific:
  WAVEFORMATEX *wf;
  BITMAPINFOHEADER *bih;   // in format
  ImageDescription* ImageDesc; // for quicktime codecs
  float aspect;
  // muxer of that stream
  struct muxer_t *muxer;
  muxer_packet_t *first;
  muxer_packet_t *last;
  any_t*priv;
} muxer_stream_t;

typedef struct {
  uint32_t id;
  const char *text;
} muxer_info_t;

typedef struct muxer_t{
  // encoding:
  MainAVIHeader avih;
  muxer_stream_t* def_v;  // default video stream (for general headers)
  muxer_stream_t* streams[MUXER_MAX_STREAMS];
  void (*fix_parameters)(struct muxer_t *);
  void (*cont_write_chunk)(muxer_stream_t *,size_t,unsigned int flags, float pts);
  void (*cont_write_header)(struct muxer_t *,Demuxer* dinfo);
  void (*cont_write_index)(struct muxer_t *);
  muxer_stream_t* (*cont_new_stream)(struct muxer_t *,int);
  FILE* file;
  any_t*priv;
} muxer_t;

muxer_t *muxer_new_muxer(const char *type,const char *subtype,FILE *f);
#define muxer_new_stream(muxer,a) muxer->cont_new_stream(muxer,a)
#define muxer_write_chunk(a,b,c,d) a->muxer->cont_write_chunk(a,b,c,d)
#define muxer_write_header(muxer,info) if(muxer->cont_write_header) muxer->cont_write_header(muxer,info)
#define muxer_write_index(muxer) if(muxer->cont_write_index) muxer->cont_write_index(muxer)
#define muxer_fix_parameters(muxer) if(muxer->fix_parameters) muxer->fix_parameters(muxer)
extern void muxer_init_muxer_raw(muxer_t *);
extern void muxer_init_muxer_mpxp64(muxer_t *);
extern int  muxer_init_muxer_lavf(muxer_t *,const char *);

#endif
