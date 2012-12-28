#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
/*
	RoQ file demuxer for the MPlayer program
	by Mike Melanson
	based on Dr. Tim Ferguson's RoQ document found at:
	  http://www.csse.monash.edu.au/~timf/videocodec.html

	TODO: demuxer->movi_length
	TODO: DP_KEYFRAME
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "mpxp_help.h"

#include "libmpstream2/stream.h"
#include "demuxer.h"
#include "demuxer_internal.h"
#include "stheader.h"
#include "demux_msg.h"

enum {
    RoQ_INFO           =0x1001,
    RoQ_QUAD_CODEBOOK  =0x1002,
    RoQ_QUAD_VQ        =0x1011,
    RoQ_SOUND_MONO     =0x1020,
    RoQ_SOUND_STEREO   =0x1021
};

enum {
    CHUNK_TYPE_AUDIO=0,
    CHUNK_TYPE_VIDEO=1
};

typedef struct roq_chunk_t
{
  int chunk_type;
  off_t chunk_offset;
  int chunk_size;

  float video_chunk_number;  // in the case of a video chunk
  int running_audio_sample_count;  // for an audio chunk
} roq_chunk_t;

struct roq_data_t : public Opaque {
    public:
	roq_data_t() {}
	virtual ~roq_data_t() {}

	int total_chunks;
	int current_chunk;
	int total_video_chunks;
	int total_audio_sample_count;
	roq_chunk_t *chunks;
};

// Check if a stream qualifies as a RoQ file based on the magic numbers
// at the start of the file:
//  84 10 FF FF FF FF xx xx
static MPXP_Rc roq_probe(Demuxer *demuxer)
{
    demuxer->stream->reset();
    demuxer->stream->seek( demuxer->stream->start_pos());

    if ((demuxer->stream->read_dword() == 0x8410FFFF) &&
	((demuxer->stream->read_dword() & 0xFFFF0000) == 0xFFFF0000)) {
	demuxer->file_format=Demuxer::Type_ROQ;
	return MPXP_Ok;
    } else return MPXP_False;
}

// return value:
//     0 = EOF or no stream found
//     1 = successfully read a packet
static int roq_demux(Demuxer *demuxer,Demuxer_Stream *__ds)
{
  sh_video_t *sh_video = reinterpret_cast<sh_video_t*>(demuxer->video->sh);
  roq_data_t *roq_data = static_cast<roq_data_t*>(demuxer->priv);
  roq_chunk_t roq_chunk;

  if (roq_data->current_chunk >= roq_data->total_chunks)
    return 0;

  roq_chunk = roq_data->chunks[roq_data->current_chunk];

  // make sure we're at the right place in the stream and fetch the chunk
  demuxer->stream->seek( roq_chunk.chunk_offset);

  if (roq_chunk.chunk_type == CHUNK_TYPE_AUDIO)
    demuxer->audio->read_packet(demuxer->stream, roq_chunk.chunk_size,
      0,
      roq_chunk.chunk_offset, DP_NONKEYFRAME);
  else
    demuxer->video->read_packet(demuxer->stream, roq_chunk.chunk_size,
      roq_chunk.video_chunk_number / sh_video->fps,
      roq_chunk.chunk_offset, DP_NONKEYFRAME);

  roq_data->current_chunk++;
  return 1;
}

static Opaque* roq_open(Demuxer* demuxer)
{
  sh_video_t *sh_video = NULL;
  sh_audio_t *sh_audio = NULL;

  roq_data_t *roq_data = new(zeromem) roq_data_t;
  int chunk_id;
  int chunk_size;
  int chunk_arg;
  int last_chunk_id = 0;
  int largest_audio_chunk = 0;
  int fps;

  roq_data->total_chunks = 0;
  roq_data->current_chunk = 0;
  roq_data->total_video_chunks = 0;
  roq_data->chunks = NULL;

  // position the stream and start traversing
  demuxer->stream->seek( demuxer->stream->start_pos()+6);
  fps = demuxer->stream->read_word_le();
  while (!demuxer->stream->eof())
  {
    chunk_id = demuxer->stream->read_word_le();
    chunk_size = demuxer->stream->read_dword_le();
    chunk_arg = demuxer->stream->read_word_le();

    // this is the only useful header info in the file
    if (chunk_id == RoQ_INFO)
    {
      // there should only be one RoQ_INFO chunk per file
      if (sh_video)
      {
	MSG_WARN( "Found more than one RoQ_INFO chunk\n");
	demuxer->stream->skip( 8);
      }
      else
      {
	// this is a good opportunity to create a video stream header
	sh_video = demuxer->new_sh_video();
	// make sure the demuxer knows about the new stream header
	demuxer->video->sh = sh_video;
	// make sure that the video demuxer stream header knows about its
	// parent video demuxer stream
	sh_video->ds = demuxer->video;

	sh_video->src_w = demuxer->stream->read_word_le();
	sh_video->src_h = demuxer->stream->read_word_le();
	demuxer->stream->skip( 4);

	// custom fourcc for internal MPlayer use
	sh_video->fourcc = mmioFOURCC('R', 'o', 'Q', 'V');

	// constant frame rate
	sh_video->fps = fps;
      }
    }
    else if ((chunk_id == RoQ_SOUND_MONO) ||
      (chunk_id == RoQ_SOUND_STEREO))
    {
      // create the audio stream header if it hasn't been created it
      if (sh_audio == NULL)
      {
	// make the header first
	sh_audio = demuxer->new_sh_audio();
	// make sure the demuxer knows about the new stream header
	demuxer->audio->sh = sh_audio;
	// make sure that the audio demuxer stream header knows about its
	// parent audio demuxer stream
	sh_audio->ds = demuxer->audio;

	// go through the bother of making a WAVEFORMATEX structure
	sh_audio->wf = (WAVEFORMATEX *)mp_malloc(sizeof(WAVEFORMATEX));

	// custom fourcc for internal MPlayer use
	sh_audio->wtag = mmioFOURCC('R', 'o', 'Q', 'A');
	if (chunk_id == RoQ_SOUND_STEREO)
	  sh_audio->wf->nChannels = 2;
	else
	  sh_audio->wf->nChannels = 1;
	// always 22KHz, 16-bit
	sh_audio->wf->nSamplesPerSec = 22050;
	sh_audio->wf->wBitsPerSample = 16;
      }

      // index the chunk
      roq_data->chunks = (roq_chunk_t *)mp_realloc(roq_data->chunks,
	(roq_data->total_chunks + 1) * sizeof (roq_chunk_t));
      roq_data->chunks[roq_data->total_chunks].chunk_type = CHUNK_TYPE_AUDIO;
      roq_data->chunks[roq_data->total_chunks].chunk_offset =
	demuxer->stream->tell() - 8;
      roq_data->chunks[roq_data->total_chunks].chunk_size = chunk_size + 8;
      roq_data->chunks[roq_data->total_chunks].running_audio_sample_count =
	roq_data->total_audio_sample_count;

      // audio housekeeping
      if (chunk_size > largest_audio_chunk)
	largest_audio_chunk = chunk_size;
      roq_data->total_audio_sample_count +=
	(chunk_size / sh_audio->wf->nChannels);

      demuxer->stream->skip( chunk_size);
      roq_data->total_chunks++;
    }
    else if ((chunk_id == RoQ_QUAD_CODEBOOK) ||
      ((chunk_id == RoQ_QUAD_VQ) && (last_chunk_id != RoQ_QUAD_CODEBOOK)))
    {
      // index a new chunk if it's a codebook or quad VQ not following a
      // codebook
      roq_data->chunks = (roq_chunk_t *)mp_realloc(roq_data->chunks,
	(roq_data->total_chunks + 1) * sizeof (roq_chunk_t));
      roq_data->chunks[roq_data->total_chunks].chunk_type = CHUNK_TYPE_VIDEO;
      roq_data->chunks[roq_data->total_chunks].chunk_offset =
	demuxer->stream->tell() - 8;
      roq_data->chunks[roq_data->total_chunks].chunk_size = chunk_size + 8;
      roq_data->chunks[roq_data->total_chunks].video_chunk_number =
	roq_data->total_video_chunks++;

      demuxer->stream->skip( chunk_size);
      roq_data->total_chunks++;
    }
    else if ((chunk_id == RoQ_QUAD_VQ) && (last_chunk_id == RoQ_QUAD_CODEBOOK))
    {
      // if it's a quad VQ chunk following a codebook chunk, extend the last
      // chunk
      roq_data->chunks[roq_data->total_chunks - 1].chunk_size += (chunk_size + 8);
      demuxer->stream->skip( chunk_size);
    }
    else if (!demuxer->stream->eof())
    {
	MSG_WARN( "Unknown RoQ chunk ID: %04X\n", chunk_id);
    }

    last_chunk_id = chunk_id;
  }

  // minimum output buffer size = largest audio chunk * 2, since each byte
  // in the DPCM encoding effectively represents 1 16-bit sample
  // (store it in wf->nBlockAlign for the time being since init_audio() will
  // step on it anyway)
  if (sh_audio)
    sh_audio->wf->nBlockAlign = largest_audio_chunk * 2;

    roq_data->current_chunk = 0;
    demuxer->stream->reset();
    check_pin("demuxer",demuxer->pin,DEMUX_PIN);
    return roq_data;
}

static void roq_close(Demuxer* demuxer) {
  roq_data_t *roq_data = static_cast<roq_data_t*>(demuxer->priv);

  if(!roq_data) return;
  delete roq_data;
}

static MPXP_Rc roq_control(const Demuxer *demuxer,int cmd,any_t*args)
{
    UNUSED(demuxer);
    UNUSED(cmd);
    UNUSED(args);
    return MPXP_Unknown;
}

extern const demuxer_driver_t demux_roq =
{
    "roq",
    "RoQ parser",
    ".roq",
    NULL,
    roq_probe,
    roq_open,
    roq_demux,
    NULL,
    roq_close,
    roq_control
};
