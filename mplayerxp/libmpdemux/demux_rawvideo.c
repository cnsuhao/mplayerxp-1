#include "../mp_config.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>


#include "stream.h"
#include "demuxer.h"
#include "stheader.h"
#include "../cfgparser.h"

#include "../libvo/img_format.h"
#include "demux_msg.h"

int use_rawvideo = 0;
static int format = IMGFMT_I420;
static int size_id = 0;
static int width = 0;
static int height = 0;
static float fps = 25;
static int imgsize=0;

static const config_t demux_rawvideo_opts[] = {
  { "on", &use_rawvideo, CONF_TYPE_FLAG, 0,0, 1, NULL, "forces treating stream as raw-vidio" },
  // size:
  { "w", &width, CONF_TYPE_INT,CONF_RANGE,1,8192, NULL, "specifies image width of raw-video stream" },
  { "h", &height, CONF_TYPE_INT,CONF_RANGE,1,8192, NULL, "specifies image height of raw-video stream" },
  { "sqcif", &size_id, CONF_TYPE_FLAG,0,0,1, NULL, "sets image size to SQCIF standard" },
  { "qcif", &size_id, CONF_TYPE_FLAG,0,0,2, NULL, "sets image size to QCIF standard" },
  { "cif", &size_id, CONF_TYPE_FLAG,0,0,3, NULL, "sets image size to CIF standard" },
  { "4cif", &size_id, CONF_TYPE_FLAG,0,0,4, NULL, "sets image size to 4CIF standard" },
  { "pal", &size_id, CONF_TYPE_FLAG,0,0,5, NULL, "sets image size to PAL standard" },
  { "ntsc", &size_id, CONF_TYPE_FLAG,0,0,6, NULL, "sets image size to NTSC standard" },
  { "16cif", &size_id, CONF_TYPE_FLAG,0,0,7, NULL, "sets image size to 16CIF standard" },
  { "sif", &size_id, CONF_TYPE_FLAG,0,0,8, NULL, "sets image size to SIF standard" },
  // format:
  { "format", &format, CONF_TYPE_INT, 0, 0 , 0, NULL, "specifies colorspace (fourcc) in hex or string constant" },
  { "i420", &format, CONF_TYPE_FLAG, 0, 0 , IMGFMT_I420, NULL, "treats raw-video as I420 fourcc" },
  { "yv12", &format, CONF_TYPE_FLAG, 0, 0 , IMGFMT_YV12, NULL, "treats raw-video as YV12 fourcc" },
  { "yuy2", &format, CONF_TYPE_FLAG, 0, 0 , IMGFMT_YUY2, NULL, "treats raw-video as YUY2 fourcc" },
  { "uyvy", &format, CONF_TYPE_FLAG, 0, 0 , IMGFMT_UYVY, NULL, "treats raw-video as UYVY fourcc" },
  { "y8", &format, CONF_TYPE_FLAG, 0, 0 , IMGFMT_Y8, NULL, "treats raw-video as Y8 fourcc" },
  // misc:
  { "fps", &fps, CONF_TYPE_FLOAT,CONF_RANGE,0.001,1000, NULL, "specifies rate in frames per second of raw-video stream" },
  { "size", &imgsize, CONF_TYPE_INT, CONF_RANGE, 1 , 8192*8192*4, NULL, "specifies frame size in bytes" },
  {NULL, NULL, 0, 0, 0, 0, NULL, NULL}
};

static const config_t rawvideo_conf[] = {
  { "rawvideo", &demux_rawvideo_opts, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL, "Raw-video specific options"},
  { NULL,NULL, 0, 0, 0, 0, NULL, NULL}
};

static int rawvideo_probe(demuxer_t* demuxer) 
{
 if(demuxer->stream->type & STREAMTYPE_RAWVIDEO || use_rawvideo)
 {
    demuxer->file_format=DEMUXER_TYPE_RAWVIDEO;
    return 1;
 }
 return 0;
}


static demuxer_t* rawvideo_open(demuxer_t* demuxer) {
  sh_video_t* sh_video;

  switch(size_id){
  case 1: width=128; height=96; break;
  case 2: width=176; height=144; break;
  case 3: width=352; height=288; break;
  case 4: width=704; height=576; break;
  case 5: width=720; height=576; break;
  case 6: width=720; height=480; break;
  case 7: width=1408;height=1152;break;
  case 8: width=352; height=240; break;
  }
  demuxer->stream->driver->control(demuxer->stream,SCTRL_VID_GET_WIDTH,&width);
  demuxer->stream->driver->control(demuxer->stream,SCTRL_VID_GET_HEIGHT,&height);
  demuxer->stream->driver->control(demuxer->stream,SCTRL_VID_GET_FORMAT,&format);
  demuxer->stream->driver->control(demuxer->stream,SCTRL_VID_GET_FPS,&fps);

  if(!width || !height){
      MSG_ERR("rawvideo: width or height not specified!\n");
      return NULL;
  }

  if(!imgsize)
  switch(format){
  case IMGFMT_I420:
  case IMGFMT_IYUV:
  case IMGFMT_YV12: imgsize=width*height+2*(width>>1)*(height>>1);break;
  case IMGFMT_YUY2: imgsize=width*height*2;break;
  case IMGFMT_UYVY: imgsize=width*height*2;break;
  case IMGFMT_Y8: imgsize=width*height;break;
  default:
      MSG_ERR("rawvideo: img size not specified and unknown format!\n");
      return NULL;
  }

  sh_video = new_sh_video(demuxer,0);
  sh_video->format=format;
  sh_video->fps=fps;
  sh_video->frametime=1.0/fps;
  sh_video->disp_w=width;
  sh_video->disp_h=height;
  sh_video->i_bps=fps*imgsize;

  demuxer->movi_start = demuxer->stream->start_pos;
  demuxer->movi_end = demuxer->stream->end_pos;
  demuxer->movi_length = (demuxer->movi_end-demuxer->movi_start)/sh_video->i_bps;

  demuxer->video->sh = sh_video;
  sh_video->ds = demuxer->video;

  return demuxer;
}

static int rawvideo_demux(demuxer_t* demuxer, demux_stream_t *ds) {

  if(stream_eof(demuxer->stream)) return 0;
  if(ds!=demuxer->video) return 0;

  ds_read_packet(ds,demuxer->stream,imgsize,0,stream_tell(demuxer->stream),DP_KEYFRAME);

  return 1;
}

static void rawvideo_seek(demuxer_t *demuxer,float rel_seek_secs,int flags){
  stream_t* s = demuxer->stream;
  sh_video_t* sh_video = demuxer->video->sh;
  off_t pos;

  pos =(flags & DEMUX_SEEK_SET)?demuxer->movi_start:stream_tell(s);
  pos+=(flags&DEMUX_SEEK_PERCENTS?demuxer->movi_end-demuxer->movi_start:sh_video->i_bps)*rel_seek_secs;
  pos/=imgsize;
  stream_seek(s,pos*imgsize);
  sh_video->timer=pos * sh_video->frametime;
//  printf("demux_rawvideo: streamtell=%d\n",(int)stream_tell(demuxer->stream));
}

static void rawvideo_close(demuxer_t *demuxer) {}

static int rawvideo_control(demuxer_t *demuxer,int cmd,any_t*args)
{
    return DEMUX_UNKNOWN;
}

demuxer_driver_t demux_rawvideo =
{
    "RAW video parser",
    ".rawvideo",
    rawvideo_conf,
    rawvideo_probe,
    rawvideo_open,
    rawvideo_demux,
    rawvideo_seek,
    rawvideo_close,
    rawvideo_control
};
