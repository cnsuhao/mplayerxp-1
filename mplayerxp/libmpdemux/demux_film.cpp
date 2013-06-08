#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
/*
    FILM file parser for the MPlayer program
      by Mike Melanson

    This demuxer handles FILM (a.k.a. CPK) files commonly found on
    Sega Saturn CD-ROM games. FILM files have also been found on 3DO
    games.

    Details of the FILM file format can be found at:
      http://www.pcisys.net/~melanson/codecs/

    TODO: demuxer->movi_length
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "mpxp_help.h"
#include "osdep/bswap.h"
#include "libmpstream2/stream.h"
#include "demuxer.h"
#include "demuxer_internal.h"
#include "stheader.h"
#include "demux_msg.h"

// chunk types found in a FILM file
static const uint32_t CHUNK_FILM=mmioFOURCC('F', 'I', 'L', 'M');
static const uint32_t CHUNK_FDSC=mmioFOURCC('F', 'D', 'S', 'C');
static const uint32_t CHUNK_STAB=mmioFOURCC('S', 'T', 'A', 'B');

typedef struct _film_chunk_t
{
    off_t chunk_offset;
    int chunk_size;
    unsigned int syncinfo1;
    unsigned int syncinfo2;

    float pts;
} film_chunk_t;

struct film_data_t : public Opaque {
    public:
	film_data_t() {}
	virtual ~film_data_t();

	unsigned int total_chunks;
	unsigned int current_chunk;
	film_chunk_t *chunks;
	unsigned int chunks_per_second;
	unsigned int film_version;
};

film_data_t::~film_data_t() {
  if(chunks) delete chunks;
}

static void film_seek(Demuxer *demuxer, const seek_args_t* seeka)
{
  film_data_t *film_data = static_cast<film_data_t*>(demuxer->priv);
  int new_current_chunk=(seeka->flags&DEMUX_SEEK_SET)?0:film_data->current_chunk;

  new_current_chunk += seeka->secs *(seeka->flags&DEMUX_SEEK_PERCENTS?film_data->total_chunks:film_data->chunks_per_second);

MSG_V("current, total chunks = %d, %d; seek %5.3f sec, new chunk guess = %d\n",
  film_data->current_chunk, film_data->total_chunks,
  seeka->secs, new_current_chunk);

  // check if the new chunk number is valid
  if (new_current_chunk < 0)
    new_current_chunk = 0;
  if ((unsigned int)new_current_chunk > film_data->total_chunks)
    new_current_chunk = film_data->total_chunks - 1;

  while (((film_data->chunks[new_current_chunk].syncinfo1 == 0xFFFFFFFF) ||
    (film_data->chunks[new_current_chunk].syncinfo1 & 0x80000000)) &&
    (new_current_chunk > 0))
    new_current_chunk--;

  film_data->current_chunk = new_current_chunk;

MSG_V("  (flags = %X)  actual new chunk = %d (syncinfo1 = %08X)\n",
  seeka->flags, film_data->current_chunk, film_data->chunks[film_data->current_chunk].syncinfo1);
  demuxer->video->pts=film_data->chunks[film_data->current_chunk].pts;

}

// return value:
//     0 = EOF or no stream found
//     1 = successfully read a packet
static int film_demux(Demuxer *demuxer,Demuxer_Stream *__ds)
{
  UNUSED(__ds);
  int i;
  unsigned char byte_swap;
  int cvid_size;
  sh_video_t *sh_video = reinterpret_cast<sh_video_t*>(demuxer->video->sh);
  sh_audio_t *sh_audio = reinterpret_cast<sh_audio_t*>(demuxer->audio->sh);
  film_data_t *film_data = static_cast<film_data_t*>(demuxer->priv);
  film_chunk_t film_chunk;
  int length_fix_bytes;
  Demuxer_Packet* dp;

  // see if the end has been reached
  if (film_data->current_chunk >= film_data->total_chunks)
    return 0;

  film_chunk = film_data->chunks[film_data->current_chunk];

  // position stream and fetch chunk
  demuxer->stream->seek( film_chunk.chunk_offset);

  // load the chunks manually (instead of using ds_read_packet()), since
  // they require some adjustment
  // (all ones in syncinfo1 indicates an audio chunk)
  binary_packet bp(1);
  if (film_chunk.syncinfo1 == 0xFFFFFFFF)
  {
   if(demuxer->audio->id>=-1){   // audio not disabled
    dp = new(zeromem) Demuxer_Packet(film_chunk.chunk_size);
    bp=demuxer->stream->read(film_chunk.chunk_size);
    if(bp.size() != size_t(film_chunk.chunk_size)) return 0;
    memcpy(dp->buffer(),bp.data(),bp.size());
    dp->pts = film_chunk.pts;
    dp->pos = film_chunk.chunk_offset;
    dp->flags = DP_NONKEYFRAME;

    // adjust the data before queuing it:
    //   8-bit: signed -> unsigned
    //  16-bit: big-endian -> little-endian
    if (sh_audio->wf->wBitsPerSample == 8)
      for (i = 0; i < film_chunk.chunk_size; i++)
	dp->buffer()[i] += 128;
    else
      for (i = 0; i < film_chunk.chunk_size; i += 2)
      {
	byte_swap = dp->buffer()[i];
	dp->buffer()[i] = dp->buffer()[i + 1];
	dp->buffer()[i + 1] = byte_swap;
      }

    // append packet to DS stream
    demuxer->audio->add_packet(dp);
   }
  }
  else
  {
    // if the demuxer is dealing with CVID data, deal with it a special way
    if (sh_video->fourcc == mmioFOURCC('c', 'v', 'i', 'd'))
    {
      if (film_data->film_version)
	length_fix_bytes = 2;
      else
	length_fix_bytes = 6;

      // account for the fix bytes when allocating the buffer
      dp = new(zeromem) Demuxer_Packet(film_chunk.chunk_size - length_fix_bytes);

      // these CVID data chunks have a few extra bytes; skip them
      bp=demuxer->stream->read(10);
      if (bp.size() != 10) return 0;
      memcpy(dp->buffer(),bp.data(),bp.size());
      demuxer->stream->skip( length_fix_bytes);

      bp=demuxer->stream->read(film_chunk.chunk_size - (10 + length_fix_bytes));
      if(bp.size()!=size_t(film_chunk.chunk_size - (10 + length_fix_bytes))) return 0;
      memcpy(dp->buffer(),bp.data(),bp.size());

      dp->pts = film_chunk.pts;
      dp->pos = film_chunk.chunk_offset;
      dp->flags = (film_chunk.syncinfo1 & 0x80000000) ? DP_KEYFRAME : DP_NONKEYFRAME;

      // fix the CVID chunk size
      cvid_size = film_chunk.chunk_size - length_fix_bytes;
      dp->buffer()[1] = (cvid_size >> 16) & 0xFF;
      dp->buffer()[2] = (cvid_size >>  8) & 0xFF;
      dp->buffer()[3] = (cvid_size >>  0) & 0xFF;

      // append packet to DS stream
      demuxer->video->add_packet(dp);
    }
    else
    {
      demuxer->video->read_packet(demuxer->stream, film_chunk.chunk_size,
				film_chunk.pts,
				film_chunk.chunk_offset,
				(film_chunk.syncinfo1 & 0x80000000) ? DP_KEYFRAME : DP_NONKEYFRAME);
    }
  }
  film_data->current_chunk++;

  return 1;
}

static MPXP_Rc film_probe(Demuxer* demuxer)
{
  uint32_t chunk_type;

  // read the master chunk type
  chunk_type = le2me_32(demuxer->stream->read_fourcc());
  // validate the chunk type
  if (chunk_type != CHUNK_FILM) return MPXP_False;
  demuxer->file_format=Demuxer::Type_FILM;
  return MPXP_Ok;
}

static Opaque* film_open(Demuxer* demuxer)
{
  sh_video_t *sh_video = NULL;
  sh_audio_t *sh_audio = NULL;
  film_data_t *film_data;
  film_chunk_t film_chunk;
  int header_size;
  unsigned int chunk_type;
  unsigned int chunk_size;
  unsigned int i;
  unsigned int video_format;
  int audio_channels;
  int counting_chunks;
  unsigned int total_audio_bytes = 0;

  film_data = new(zeromem) film_data_t;
  film_data->total_chunks = 0;
  film_data->current_chunk = 0;
  film_data->chunks = NULL;
  film_data->chunks_per_second = 0;

  // go back to the beginning
  demuxer->stream->reset();
  demuxer->stream->seek( demuxer->stream->start_pos());

  // read the master chunk type
  chunk_type = demuxer->stream->read_fourcc();
  // validate the chunk type
  if (chunk_type != CHUNK_FILM)
  {
    MSG_ERR( "Not a FILM file\n");
    delete film_data;
    return NULL;
  }

  // get the header size, which implicitly points past the header and
  // to the start of the data
  header_size = demuxer->stream->read_dword();
  film_data->film_version = demuxer->stream->read_fourcc();
  demuxer->movi_start = header_size;
  demuxer->movi_end = demuxer->stream->end_pos();
  header_size -= 16;

  MSG_HINT( "FILM version %.4s\n",
    &film_data->film_version);

  // skip to where the next chunk should be
  demuxer->stream->skip( 4);

  // traverse through the header
  while (header_size > 0)
  {
    // fetch the chunk type and size
    chunk_type = demuxer->stream->read_fourcc();
    chunk_size = demuxer->stream->read_dword();
    header_size -= chunk_size;

    switch (chunk_type)
    {
    case CHUNK_FDSC:
      MSG_V( "parsing FDSC chunk\n");

      // fetch the video codec fourcc to see if there's any video
      video_format = demuxer->stream->read_fourcc();
      if (video_format)
      {
	// create and initialize the video stream header
	sh_video = demuxer->new_sh_video();
	demuxer->video->sh = sh_video;
	sh_video->ds = demuxer->video;

	sh_video->fourcc= video_format;
	sh_video->src_h = demuxer->stream->read_dword();
	sh_video->src_w = demuxer->stream->read_dword();
	MSG_V(
	  "  FILM video: %d x %d\n", sh_video->src_w,
	  sh_video->src_h);
      }
      else
	// skip height and width if no video
	demuxer->stream->skip( 8);

      if(demuxer->audio->id<-1){
	  MSG_V("chunk size = 0x%X \n",chunk_size);
	demuxer->stream->skip( chunk_size-12-8);
	break; // audio disabled (or no soundcard)
      }

      // skip over unknown byte, but only if file had non-NULL version
      if (film_data->film_version)
	demuxer->stream->skip( 1);

      // fetch the audio channels to see if there's any audio
      // don't do this if the file is a quirky file with NULL version
      if (film_data->film_version)
      {
	audio_channels = demuxer->stream->read_char();
	if (audio_channels > 0)
	{
	  // create and initialize the audio stream header
	  sh_audio = demuxer->new_sh_audio();
	  demuxer->audio->sh = sh_audio;
	  sh_audio->ds = demuxer->audio;

	  sh_audio->wf = new WAVEFORMATEX;

	  // uncompressed PCM format
	  sh_audio->wf->wFormatTag = 1;
	  sh_audio->wtag = 1;
	  sh_audio->wf->nChannels = audio_channels;
	  sh_audio->wf->wBitsPerSample = demuxer->stream->read_char();
	  demuxer->stream->skip( 1);  // skip unknown byte
	  sh_audio->wf->nSamplesPerSec = demuxer->stream->read_word();
	  sh_audio->wf->nAvgBytesPerSec =
	    sh_audio->wf->nSamplesPerSec * sh_audio->wf->wBitsPerSample
	    * sh_audio->wf->nChannels / 8;
	  demuxer->stream->skip( 6);  // skip the rest of the unknown

	  MSG_V(
	    "  FILM audio: %d channels, %d bits, %d Hz\n",
	    sh_audio->wf->nChannels, 8 * sh_audio->wf->wBitsPerSample,
	    sh_audio->wf->nSamplesPerSec);
	}
	else
	  demuxer->stream->skip( 10);
      }
      else
      {
	// otherwise, make some assumptions about the audio

	// create and initialize the audio stream header
	sh_audio = demuxer->new_sh_audio();
	demuxer->audio->sh = sh_audio;
	sh_audio->ds = demuxer->audio;

	sh_audio->wf = new WAVEFORMATEX;

	// uncompressed PCM format
	sh_audio->wf->wFormatTag = 1;
	sh_audio->wtag = 1;
	sh_audio->wf->nChannels = 1;
	sh_audio->wf->wBitsPerSample = 8;
	sh_audio->wf->nSamplesPerSec = 22050;
	sh_audio->wf->nAvgBytesPerSec =
	  sh_audio->wf->nSamplesPerSec * sh_audio->wf->wBitsPerSample
	  * sh_audio->wf->nChannels / 8;

	MSG_V(
	  "  FILM audio: %d channels, %d bits, %d Hz\n",
	  sh_audio->wf->nChannels, sh_audio->wf->wBitsPerSample,
	  sh_audio->wf->nSamplesPerSec);
      }
      break;

    case CHUNK_STAB:
      MSG_V( "parsing STAB chunk\n");

      if (sh_video)
      {
	sh_video->fps = demuxer->stream->read_dword();
      }

      // fetch the number of chunks
      film_data->total_chunks = demuxer->stream->read_dword();
      film_data->current_chunk = 0;
      MSG_V(
	"  STAB chunk contains %d chunks\n", film_data->total_chunks);

      // allocate enough entries for the chunk
      film_data->chunks =new film_chunk_t[film_data->total_chunks];

      // build the chunk index
      counting_chunks = 1;
      for (i = 0; i < film_data->total_chunks; i++)
      {
	film_chunk = film_data->chunks[i];
	film_chunk.chunk_offset =
	  demuxer->movi_start + demuxer->stream->read_dword();
	film_chunk.chunk_size = demuxer->stream->read_dword();
	film_chunk.syncinfo1 = demuxer->stream->read_dword();
	film_chunk.syncinfo2 = demuxer->stream->read_dword();

	// count chunks for the purposes of seeking
	if (counting_chunks)
	{
	  // if we're counting chunks, always count an audio chunk
	  if (film_chunk.syncinfo1 == 0xFFFFFFFF)
	    film_data->chunks_per_second++;
	  // if it's a video chunk, check if it's time to stop counting
	  else if ((film_chunk.syncinfo1 & 0x7FFFFFFF) >= sh_video->fps)
	    counting_chunks = 0;
	  else
	    film_data->chunks_per_second++;
	}

	// precalculate PTS
	if (film_chunk.syncinfo1 == 0xFFFFFFFF)
	{
	  if(demuxer->audio->id>=-1)
	  film_chunk.pts =
	    (float)total_audio_bytes / (float)sh_audio->wf->nAvgBytesPerSec;
	  total_audio_bytes += film_chunk.chunk_size;
	}
	else
	  film_chunk.pts =
	    (film_chunk.syncinfo1 & 0x7FFFFFFF) / sh_video->fps;

	film_data->chunks[i] = film_chunk;
      }

      // in some FILM files (notably '1.09'), the length of the FDSC chunk
      // follows different rules
      if (chunk_size == (film_data->total_chunks * 16))
	header_size -= 16;
      break;

    default:
      MSG_ERR( "Unrecognized FILM header chunk: %08X\n",
	chunk_type);
      return NULL;
      break;
    }
  }

    demuxer->priv = film_data;
    check_pin("demuxer",demuxer->pin,DEMUX_PIN);
    return film_data;
}

static void film_close(Demuxer* demuxer) {
    film_data_t *film_data = reinterpret_cast<film_data_t*>(demuxer->priv);

    if(!film_data) return;
    delete film_data;
}

static MPXP_Rc film_control(const Demuxer *demuxer,int cmd,any_t*args)
{
    UNUSED(demuxer);
    UNUSED(cmd);
    UNUSED(args);
    return MPXP_Unknown;
}

extern const demuxer_driver_t demux_film =
{
    "film",
    "FILM (a.k.a. CPK) parser",
    ".cpk",
    NULL,
    film_probe,
    film_open,
    film_demux,
    film_seek,
    film_close,
    film_control
};
