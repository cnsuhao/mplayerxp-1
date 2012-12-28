#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
/*
 * raw DV file parser
 * copyright (c) 2002 Alexander Neundorf <neundorf@kde.org>
 * based on the fli demuxer
 *
 * This file is part of MPlayer.
 *
 * MPlayer is mp_free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with MPlayer; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#define __STDC_FORMAT_MACROS
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <libdv/dv.h>
#include <libdv/dv_types.h>

#include "demux_msg.h"
#include "libmpstream2/stream.h"
#include "demuxer.h"
#include "demuxer_internal.h"
#include "stheader.h"

static const int DV_PAL_FRAME_SIZE =144000;
static const int DV_NTSC_FRAME_SIZE=122000;

struct rawdv_frames_t : public Opaque
{
    public:
	rawdv_frames_t() {}
	virtual ~rawdv_frames_t() {};

	int current_frame;
	int frame_size;
	off_t current_filepos;
	int frame_number;
	dv_decoder_t *decoder;
};

static void dv_seek(Demuxer *demuxer,const seek_args_t* seeka)
{
   rawdv_frames_t *frames = reinterpret_cast<rawdv_frames_t*>(demuxer->priv);
   sh_video_t *sh_video = reinterpret_cast<sh_video_t*>(demuxer->video->sh);
   off_t newpos=(seeka->flags&DEMUX_SEEK_SET)?0:frames->current_frame;
   if(seeka->flags&DEMUX_SEEK_PERCENTS) {
      // float 0..1
      newpos+=seeka->secs*frames->frame_number;
   } else {
      // secs
      newpos+=seeka->secs*sh_video->fps;
   }
   if(newpos<0)
      newpos=0;
   else if(newpos>frames->frame_number)
      newpos=frames->frame_number;
   frames->current_frame=newpos;
   frames->current_filepos=newpos*frames->frame_size;
}

static MPXP_Rc dv_probe(Demuxer *demuxer)
{
   unsigned char tmp_buffer[DV_PAL_FRAME_SIZE];
   int bytes_read=0;
   int result=0;
   dv_decoder_t *td;

   MSG_V("Checking for DV\n");

   bytes_read=demuxer->stream->read(tmp_buffer,DV_PAL_FRAME_SIZE);
   if ((bytes_read!=DV_PAL_FRAME_SIZE) && (bytes_read!=DV_NTSC_FRAME_SIZE))
      return MPXP_False;

   if(!(td=dv_decoder_new(TRUE,TRUE,FALSE))) return MPXP_False;

   td->quality=DV_QUALITY_BEST;
   if((result=dv_parse_header(td, tmp_buffer))<0) return MPXP_False;

   if ((( td->num_dif_seqs==10) || (td->num_dif_seqs==12))
       && (td->width==720)
       && ((td->height==576) || (td->height==480)))
      result=1;
   dv_decoder_free(td);
   if (result) return MPXP_Ok;
   return MPXP_False;
}

// return value:
//     0 = EOF or no stream found
//     1 = successfully read a packet
static int dv_demux(Demuxer *demuxer, Demuxer_Stream *ds)
{
   rawdv_frames_t *frames = static_cast<rawdv_frames_t*>(demuxer->priv);
   sh_video_t *sh_video = reinterpret_cast<sh_video_t*>(demuxer->video->sh);
   int bytes_read=0;
//   fprintf(stderr,"demux_rawdv_fill_buffer() seek to %qu, size: %d\n",frames->current_filepos,frames->frame_size);
   // fetch the frame from the file
   // first, position the file properly since ds_read_packet() doesn't
   // seem to do it, even though it takes a file offset as a parameter
   demuxer->stream->seek(frames->current_filepos);

   Demuxer_Packet* dp_video=new(zeromem) Demuxer_Packet(frames->frame_size);
   bytes_read=demuxer->stream->read(dp_video->buffer(),frames->frame_size);
   if (bytes_read<frames->frame_size)
      return 0;
   dp_video->pts=frames->current_frame/sh_video->fps;
   dp_video->pos=frames->current_filepos;
   dp_video->flags=DP_NONKEYFRAME;

   if (demuxer->audio && demuxer->audio->id>=-1)
   {
      Demuxer_Packet* dp_audio=dp_video->clone();
      demuxer->audio->add_packet(dp_audio);
   }
   demuxer->video->add_packet(dp_video);
   // get the next frame ready
   frames->current_filepos+=frames->frame_size;
   frames->current_frame++;
//   fprintf(stderr," audio->packs: %d , video->packs: %d \n",demuxer->audio->packs, demuxer->video->packs);
   return 1;
}

static Opaque* dv_open(Demuxer* demuxer)
{
   unsigned char dv_frame[DV_PAL_FRAME_SIZE];
   sh_video_t *sh_video = NULL;
   rawdv_frames_t *frames = new rawdv_frames_t;
   dv_decoder_t *dv_decoder=NULL;

   MSG_V("demux_open_rawdv() end_pos %" PRId64"\n",(int64_t)demuxer->stream->end_pos());

   // go back to the beginning
   demuxer->stream->reset();
   demuxer->stream->seek(0);

   //get the first frame
   demuxer->stream->read( dv_frame, DV_PAL_FRAME_SIZE);

   //read params from this frame
   dv_decoder=dv_decoder_new(TRUE,TRUE,FALSE);
   dv_decoder->quality=DV_QUALITY_BEST;

   if (dv_parse_header(dv_decoder, dv_frame) == -1)
	   return NULL;

   // create a new video stream header
   sh_video = demuxer->new_sh_video();
   if (!sh_video)
	   return NULL;

   // make sure the demuxer knows about the new video stream header
   // (even though new_sh_video() ought to take care of it)
   demuxer->flags |= Demuxer::Seekable;
   demuxer->video->sh = sh_video;

   // make sure that the video demuxer stream header knows about its
   // parent video demuxer stream (this is getting wacky), or else
   // video_read_properties() will choke
   sh_video->ds = demuxer->video;

   // custom fourcc for internal MPlayer use
//   sh_video->wtag = mmioFOURCC('R', 'A', 'D', 'V');
   sh_video->fourcc = mmioFOURCC('D', 'V', 'S', 'D');

   sh_video->src_w = dv_decoder->width;
   sh_video->src_h = dv_decoder->height;
   MSG_V("demux_open_rawdv() frame_size: %d w: %d h: %d dif_seq: %d system: %d\n",dv_decoder->frame_size,dv_decoder->width, dv_decoder->height,dv_decoder->num_dif_seqs,dv_decoder->system);

   sh_video->fps= (dv_decoder->system==e_dv_system_525_60?29.97:25);

  // emulate BITMAPINFOHEADER for win32 decoders:
  sh_video->bih=new(zeromem) BITMAPINFOHEADER;
  sh_video->bih->biSize=40;
  sh_video->bih->biWidth = dv_decoder->width;
  sh_video->bih->biHeight = dv_decoder->height;
  sh_video->bih->biPlanes=1;
  sh_video->bih->biBitCount=24;
  sh_video->bih->biCompression=sh_video->fourcc; // "DVSD"
  sh_video->bih->biSizeImage=sh_video->bih->biWidth*sh_video->bih->biHeight*3;


   frames->current_filepos=0;
   frames->current_frame=0;
   frames->frame_size=dv_decoder->frame_size;
   frames->frame_number=demuxer->stream->end_pos()/frames->frame_size;

   MSG_V("demux_open_rawdv() seek to %qu, size: %d, dv_dec->frame_size: %d\n",frames->current_filepos,frames->frame_size, dv_decoder->frame_size);
    if (dv_decoder->audio != NULL && demuxer->audio->id>=-1){
	sh_audio_t *sh_audio = demuxer->new_sh_audio();
	demuxer->audio->id = 0;
	    demuxer->audio->sh = sh_audio;
	    sh_audio->ds = demuxer->audio;
	MSG_V("demux_open_rawdv() chan: %d samplerate: %d\n",dv_decoder->audio->num_channels,dv_decoder->audio->frequency );
	// custom fourcc for internal MPlayer use
	sh_audio->wtag = mmioFOURCC('R', 'A', 'D', 'V');

	sh_audio->wf = new(zeromem) WAVEFORMATEX;
	sh_audio->wf->wFormatTag = sh_audio->wtag;
	sh_audio->wf->nChannels = dv_decoder->audio->num_channels;
	sh_audio->wf->wBitsPerSample = 16;
	sh_audio->wf->nSamplesPerSec = dv_decoder->audio->frequency;
	// info about the input stream:
	sh_audio->wf->nAvgBytesPerSec = sh_video->fps*dv_decoder->frame_size;
	sh_audio->wf->nBlockAlign = dv_decoder->frame_size;

//       sh_audio->context=(any_t*)dv_decoder;
    }
    demuxer->stream->reset();
    demuxer->stream->seek(0);
    dv_decoder_free(dv_decoder);  //we keep this in the context of both stream headers
    check_pin("demuxer",demuxer->pin,DEMUX_PIN);
    return frames;
}

static void dv_close(Demuxer* demuxer)
{
    rawdv_frames_t *frames = static_cast<rawdv_frames_t*>(demuxer->priv);

    if(frames==0) return;
    delete frames;
}

static MPXP_Rc dv_control(const Demuxer *demuxer,int cmd, any_t*arg) {
    return MPXP_Unknown;
}

extern const demuxer_driver_t demux_dv = {
  "dv",
  "DV video: IEC 61834 and SMPTE 314M",
  ".dv",
  NULL, // no options
  dv_probe,
  dv_open,
  dv_demux,
  dv_seek,
  dv_close,
  dv_control
};
