#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
/*
	FLI file parser for the MPlayer program
	by Mike Melanson

	TODO: demuxer->movi_length
	TODO: DP_KEYFRAME
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "help_mp.h"

#include "libmpstream/stream.h"
#include "demuxer.h"
#include "stheader.h"
#include "demux_msg.h"

typedef struct _fli_frames_t {
  int num_frames;
  int current_frame;
  off_t *filepos;
  unsigned int *frame_size;
} fli_frames_t;

static void fli_seek(demuxer_t *demuxer,const seek_args_t* seeka){
  fli_frames_t *frames = reinterpret_cast<fli_frames_t*>(demuxer->priv);
  sh_video_t *sh_video = reinterpret_cast<sh_video_t*>(demuxer->video->sh);
  int newpos=(seeka->flags&DEMUX_SEEK_SET)?0:frames->current_frame;
  newpos+=seeka->secs*(seeka->flags&DEMUX_SEEK_PERCENTS?frames->num_frames:sh_video->fps);
  if(newpos<0) newpos=0; else
  if(newpos>frames->num_frames) newpos=frames->num_frames;
  frames->current_frame=newpos;
}

// return value:
//     0 = EOF or no stream found
//     1 = successfully read a packet
static int fli_demux(demuxer_t *demuxer,demux_stream_t *__ds){
  fli_frames_t *frames = reinterpret_cast<fli_frames_t*>(demuxer->priv);
  sh_video_t *sh_video = reinterpret_cast<sh_video_t*>(demuxer->video->sh);

  // see if the end has been reached
  if (frames->current_frame >= frames->num_frames)
    return 0;

  // fetch the frame from the file
  // first, position the file properly since ds_read_packet() doesn't
  // seem to do it, even though it takes a file offset as a parameter
  stream_seek(demuxer->stream, frames->filepos[frames->current_frame]);
  ds_read_packet(demuxer->video,
    demuxer->stream,
    frames->frame_size[frames->current_frame],
    frames->current_frame/sh_video->fps,
    frames->filepos[frames->current_frame],
    DP_NONKEYFRAME /* what flags? -> demuxer.h (alex) */
  );

  // get the next frame ready
  frames->current_frame++;

  return 1;
}

static MPXP_Rc fli_probe(demuxer_t* demuxer){
  unsigned magic_number;
  demuxer->movi_end = stream_skip(demuxer->stream,4);
  magic_number = stream_read_word_le(demuxer->stream);
  if ((magic_number != 0xAF11) && (magic_number != 0xAF12)) return MPXP_False;
  demuxer->file_format=DEMUXER_TYPE_FLI;
  return MPXP_Ok;
}

static demuxer_t* fli_open(demuxer_t* demuxer){
  sh_video_t *sh_video = NULL;
  fli_frames_t *frames = (fli_frames_t *)mp_malloc(sizeof(fli_frames_t));
  int frame_number;
  int speed;
  unsigned int frame_size;
  int magic_number;
  unsigned char * header;

  // go back to the beginning
  stream_reset(demuxer->stream);
  stream_seek(demuxer->stream, 0);

  header = (unsigned char*)mp_malloc(sizeof(BITMAPINFOHEADER) + 128);
  stream_read(demuxer->stream, header + sizeof(BITMAPINFOHEADER), 128);
  stream_seek(demuxer->stream, 0);

  demuxer->movi_start = 128;
  demuxer->movi_end = stream_read_dword_le(demuxer->stream);

  magic_number = stream_read_word_le(demuxer->stream);

  if ((magic_number != 0xAF11) && (magic_number != 0xAF12))
  {
    MSG_ERR("Bad/unknown magic number (%04x)\n",
	magic_number);
    delete header;
    delete frames;
    return(NULL);
  }

  // fetch the number of frames
  frames->num_frames = stream_read_word_le(demuxer->stream);
  frames->current_frame = 0;

  // allocate enough entries for the indices
  frames->filepos = (off_t *)mp_malloc(frames->num_frames * sizeof(off_t));
  frames->frame_size = new unsigned int [frames->num_frames];

  // create a new video stream header
  sh_video = new_sh_video(demuxer, 0);

  // make sure the demuxer knows about the new video stream header
  // (even though new_sh_video() ought to take care of it)
  demuxer->video->sh = sh_video;

  // make sure that the video demuxer stream header knows about its
  // parent video demuxer stream (this is getting wacky), or else
  // video_read_properties() will choke
  sh_video->ds = demuxer->video;

  // custom fourcc for internal MPlayer use
  sh_video->fourcc = mmioFOURCC('F', 'L', 'I', 'C');

  sh_video->src_w = stream_read_word_le(demuxer->stream);
  sh_video->src_h = stream_read_word_le(demuxer->stream);

  // pass extradata to codec
  sh_video->bih = (BITMAPINFOHEADER*)header;
  sh_video->bih->biSize = sizeof(BITMAPINFOHEADER) + 128;
  sh_video->bih->biCompression=sh_video->fourcc;
  sh_video->bih->biWidth=sh_video->src_w;
  sh_video->bih->biPlanes=0;
  sh_video->bih->biBitCount=0; /* depth */
  sh_video->bih->biHeight=sh_video->src_h;
  sh_video->bih->biSizeImage=sh_video->bih->biWidth*sh_video->bih->biHeight;
  // skip the video depth and flags
  stream_skip(demuxer->stream, 4);

  // get the speed
  speed = stream_read_word_le(demuxer->stream);
  if (speed == 0)
    speed = 1;
  if (magic_number == 0xAF11)
    speed *= 1000.0f/70.0f;
  sh_video->fps = 1000.0f / speed;

  // build the frame index
  stream_seek(demuxer->stream, demuxer->movi_start);
  frame_number = 0;
  while ((!stream_eof(demuxer->stream)) && (frame_number < frames->num_frames))
  {
    frames->filepos[frame_number] = stream_tell(demuxer->stream);
    frame_size = stream_read_dword_le(demuxer->stream);
    magic_number = stream_read_word_le(demuxer->stream);
    stream_skip(demuxer->stream, frame_size - 6);

    // if this chunk has the right magic number, index it
    if ((magic_number == 0xF1FA) || (magic_number == 0xF5FA))
    {
      frames->frame_size[frame_number] = frame_size;
      frame_number++;
    }
  }

    // save the actual number of frames indexed
    frames->num_frames = frame_number;

    demuxer->priv = frames;
    check_pin("demuxer",demuxer->pin,DEMUX_PIN);
    return demuxer;
}

static void fli_close(demuxer_t* demuxer) {
  fli_frames_t *frames = reinterpret_cast<fli_frames_t*>(demuxer->priv);

  if(!frames)
    return;

  if(frames->filepos)
    delete frames->filepos;
  if(frames->frame_size)
    delete frames->frame_size;

  delete frames;
}

static MPXP_Rc fli_control(const demuxer_t *demuxer,int cmd,any_t*args)
{
    return MPXP_Unknown;
}

extern const demuxer_driver_t demux_fli =
{
    "FLI parser",
    ".fli",
    NULL,
    fli_probe,
    fli_open,
    fli_demux,
    fli_seek,
    fli_close,
    fli_control
};
