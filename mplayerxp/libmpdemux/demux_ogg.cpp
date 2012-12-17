#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
#include <algorithm>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <dlfcn.h>
#include <ogg/ogg.h>
#include <vorbis/codec.h>
#ifdef HAVE_LIBTHEORA
#include <theora/theora.h>
#endif

#include "mplayerxp.h"
#include "osdep/bswap.h"
#include "help_mp.h"
#include "libmpstream2/stream.h"
#include "demuxer.h"
#include "demuxer_internal.h"
#include "stheader.h"
#include "aviprint.h"
#include "libmpcodecs/codecs_ld.h"
#include "libmpcodecs/dec_audio.h"
#include "libvo2/video_out.h"
#include "libao3/afmt.h"
#include "demux_msg.h"

#define BLOCK_SIZE 4096
#define FOURCC_VORBIS mmioFOURCC('v', 'r', 'b', 's')
#define FOURCC_THEORA mmioFOURCC('t', 'h', 'e', 'o')

/// Vorbis decoder context : we need the vorbis_info for vorbis timestamping
/// Shall we put this struct def in a common header ?
struct ogg_vorbis_struct {
  int inited;
  vorbis_info      vi; /* struct that stores all the static vorbis bitstream
			  settings */
  vorbis_comment   vc; /* struct that stores all the bitstream user comments */
#ifdef HAVE_LIBTHEORA
  theora_info           t_info;
  theora_comment        t_comment;
#endif
//  vorbis_dsp_state vd; /* central working state for the packet->PCM decoder */
//  vorbis_block     vb; /* local working space for packet->PCM decode */
//  float            rg_scale; /* replaygain scale */
#ifdef TREMOR
//  int              rg_scale_int;
#endif
};

//// OggDS headers
// Header for the new header format
typedef struct stream_header_video
{
	ogg_int32_t	width;
	ogg_int32_t	height;
} stream_header_video;

typedef struct stream_header_audio
{
	ogg_int16_t	channels;
	ogg_int16_t	blockalign;
	ogg_int32_t	avgbytespersec;
} stream_header_audio;

typedef struct __attribute__((__packed__)) stream_header
{
	char	streamtype[8];
	char	subtype[4];

	ogg_int32_t	size;				// size of the structure

	ogg_int64_t	time_unit;			// in reference time
	ogg_int64_t	samples_per_unit;
	ogg_int32_t default_len;		// in media time

	ogg_int32_t buffersize;
	ogg_int16_t	bits_per_sample;

	ogg_int16_t padding;

	union
	{
		// Video specific
		stream_header_video	video;
		// Audio specific
		stream_header_audio	audio;
	} sh;
} stream_header;

/// Our private datas

typedef struct ogg_syncpoint {
  int64_t granulepos;
  off_t page_pos;
} ogg_syncpoint_t;

/// A logical stream
typedef struct ogg_stream {
  /// Timestamping stuff
  float samplerate; /// granulpos 2 time
  int64_t lastpos;
  int32_t lastsize;

  // Logical stream state
  ogg_stream_state stream;
  int hdr_packets;
  int vorbis;
  int theora;
  int flac;
  int text;
  int id;
} ogg_stream_t;

struct ogg_demuxer_t : public Opaque {
    public:
	ogg_demuxer_t();
	virtual ~ogg_demuxer_t();

	/// Physical stream state
	ogg_sync_state sync;
	/// Current page
	ogg_page page;
	/// Logical streams
	ogg_stream_t *subs;
	int num_sub;
	ogg_syncpoint_t* syncpoints;
	int num_syncpoint;
	off_t pos, last_size;
	int64_t final_granulepos;
	ogg_vorbis_struct* ov;

	/* Used for subtitle switching. */
	int n_text;
	int *text_ids;
	char **text_langs;
};

ogg_demuxer_t::ogg_demuxer_t()
	    :ov(new(zeromem) ogg_vorbis_struct)
{
}

ogg_demuxer_t::~ogg_demuxer_t() {
    unsigned i;
    ogg_sync_clear(&sync);
    if(subs) {
	for (i = 0; i < num_sub; i++)
	    ogg_stream_clear(&subs[i].stream);
	delete subs;
    }
    if(syncpoints) delete syncpoints;
    if (text_ids) delete text_ids;
    if (text_langs) {
	for (i = 0; i < n_text; i++)
	    if (text_langs[i]) delete text_langs[i];
	delete text_langs;
    }
    delete ov;
}

#define NUM_VORBIS_HDR_PACKETS 3

/// Some defines from OggDS
#define PACKET_TYPE_HEADER   0x01
#define PACKET_TYPE_BITS         0x07
#define PACKET_LEN_BITS01       0xc0
#define PACKET_LEN_BITS2         0x02
#define PACKET_IS_SYNCPOINT  0x08

//-------- subtitle support - should be moved to decoder layer, and queue
//                          - subtitles up in demuxer buffer...

#include "libmpsub/subreader.h"
#include "libvo2/sub.h"
#define OGG_SUB_MAX_LINE 128

static subtitle ogg_sub;
static float clear_sub;
//FILE* subout;

static MPXP_Rc ogg_probe(Demuxer *demuxer)
{
    uint32_t fcc;
    fcc=me2be_32(demuxer->stream->read_dword());
    if(fcc != mmioFOURCC('O','g','g','S')) return MPXP_False;
    demuxer->file_format=Demuxer::Type_OGG;
    demuxer->stream->seek(0);
    return MPXP_Ok;
}

static inline uint16_t get_uint16(uint16_t* val) { return le2me_16(*val); }
static inline uint32_t get_uint32(uint32_t* val) { return le2me_32(*val); }
static inline uint64_t get_uint64(uint64_t* val) { return le2me_64(*val); }
#if 0
static
uint16_t get_uint16 (const any_t*buf)
{
  uint16_t      ret;
  unsigned char *tmp;

  tmp = (unsigned char *) buf;

  ret = tmp[1] & 0xff;
  ret = (ret << 8) + (tmp[0] & 0xff);

  return (ret);
}

static
uint32_t get_uint32 (const any_t*buf)
{
  uint32_t      ret;
  unsigned char *tmp;

  tmp = (unsigned char *) buf;

  ret = tmp[3] & 0xff;
  ret = (ret << 8) + (tmp[2] & 0xff);
  ret = (ret << 8) + (tmp[1] & 0xff);
  ret = (ret << 8) + (tmp[0] & 0xff);

  return (ret);
}

static
uint64_t get_uint64 (const any_t*buf)
{
  uint64_t      ret;
  unsigned char *tmp;

  tmp = (unsigned char *) buf;

  ret = tmp[7] & 0xff;
  ret = (ret << 8) + (tmp[6] & 0xff);
  ret = (ret << 8) + (tmp[5] & 0xff);
  ret = (ret << 8) + (tmp[4] & 0xff);
  ret = (ret << 8) + (tmp[3] & 0xff);
  ret = (ret << 8) + (tmp[2] & 0xff);
  ret = (ret << 8) + (tmp[1] & 0xff);
  ret = (ret << 8) + (tmp[0] & 0xff);

  return (ret);
}
#endif

static void demux_ogg_init_sub () {
  int lcv;
  if(!ogg_sub.text[0]) // not yet allocated
  for (lcv = 0; lcv < SUB_MAX_TEXT; lcv++) {
    ogg_sub.text[lcv] = (char*)mp_malloc(OGG_SUB_MAX_LINE);
  }
}

static void demux_ogg_add_sub (ogg_stream_t* os,ogg_packet* pack) {
  int lcv;
  int line_pos = 0;
  int ignoring = 0;
  char *packet = reinterpret_cast<char*>(pack->packet);

  MSG_DBG2("\ndemux_ogg_add_sub %02X %02X %02X '%s'\n",
      (unsigned char)packet[0],
      (unsigned char)packet[1],
      (unsigned char)packet[2],
      &packet[3]);

  ogg_sub.lines = 0;
  if (((unsigned char)packet[0]) == 0x88) { // some subtitle text
    // Find data start
    int32_t duration = 0;
    int16_t hdrlen = (*packet & PACKET_LEN_BITS01)>>6, i;
    hdrlen |= (*packet & PACKET_LEN_BITS2) <<1;
    lcv = 1 + hdrlen;
    for (i = hdrlen; i > 0; i--) {
      duration <<= 8;
      duration |= (unsigned char)packet[i];
    }
    if ((hdrlen > 0) && (duration > 0)) {
      float pts;
      if(pack->granulepos == -1)
	pack->granulepos = os->lastpos + os->lastsize;
      pts = (float)pack->granulepos/(float)os->samplerate;
      clear_sub = 1.0 + pts + (float)duration/1000.0;
    }
    while (1) {
      int c = packet[lcv++];
      if(c=='\n' || c==0 || line_pos >= OGG_SUB_MAX_LINE-1){
	  ogg_sub.text[ogg_sub.lines][line_pos] = 0; // close sub
	  if(line_pos) ogg_sub.lines++;
	  if(!c || ogg_sub.lines>=SUB_MAX_TEXT) break; // EOL or TooMany
	  line_pos = 0;
      }
      switch (c) {
	case '\r':
	case '\n': // just ignore linefeeds for now
		   // their placement seems rather haphazard
	  break;
	case '<': // some html markup, ignore for now
	  ignoring = 1;
	  break;
	case '>':
	  ignoring = 0;
	  break;
	default:
	  if(!ignoring)
	  ogg_sub.text[ogg_sub.lines][line_pos++] = c;
	  break;
      }
    }
  }

  MSG_DBG2("Ogg sub lines: %d  first: '%s'\n",
      ogg_sub.lines, ogg_sub.text[0]);
#ifdef USE_ICONV
  subcp_recode1(&ogg_sub);
#endif
  mpxp_context().video().output->sub = &ogg_sub;
  vo_osd_changed(OSDTYPE_SUBTITLE);
}


// get the logical stream of the current page
// fill os if non NULL and return the stream id
static  int demux_ogg_get_page_stream(ogg_demuxer_t* ogg_d,ogg_stream_state** os) {
  int id,s_no;
  ogg_page* page = &ogg_d->page;

  s_no = ogg_page_serialno(page);

  for(id= 0; id < ogg_d->num_sub ; id++) {
    if(s_no == ogg_d->subs[id].stream.serialno)
      break;
  }

  if(id == ogg_d->num_sub) {
    // If we have only one vorbis stream allow the stream id to change
    // it's normal on radio stream (each song have an different id).
    // But we (or the codec?) should check that the samplerate, etc
    // doesn't change (for radio stream it's ok)
    if(ogg_d->num_sub == 1 && ogg_d->subs[0].vorbis) {
      ogg_stream_reset(&ogg_d->subs[0].stream);
      ogg_stream_init(&ogg_d->subs[0].stream,s_no);
      id = 0;
    } else
      return -1;
  }

  if(os)
    *os = &ogg_d->subs[id].stream;

  return id;

}

static unsigned char* demux_ogg_read_packet(ogg_demuxer_t* ogg_d,ogg_stream_t* os,ogg_packet* pack,float* pts,int* flags, int samplesize) {
  unsigned char* data=NULL;

  *pts = 0;
  *flags = 0;

  if(os->vorbis) {
    data = pack->packet;
    if(*pack->packet & PACKET_TYPE_HEADER)  os->hdr_packets++;
    else {
	int32_t blocksize;
	vorbis_info *vi = &ogg_d->ov->vi;
	vorbis_comment *vc = &ogg_d->ov->vc;

	if (ogg_d->ov->inited) {
	// When we dump the audio, there is no vi, but we don't care of timestamp in this case
	    blocksize = vorbis_packet_blocksize(vi,pack) / samplesize;
	    // Calculate the timestamp if the packet don't have any
	    if(pack->granulepos == -1) {
		pack->granulepos = os->lastpos;
		if(os->lastsize > 0)
		    pack->granulepos += os->lastsize;
	    }
	    *pts = pack->granulepos / (float)vi->rate;
	    os->lastsize = blocksize;
	    os->lastpos = pack->granulepos;
	}
    }
  } else if (os->theora) {
     /* we pass complete packets to theora, mustn't strip the header! */
     data = pack->packet;
     os->lastsize = 1;

     /* header packets beginn on 1-bit: thus check (*data&0x80).  We don't
	have theora_state st, until all header packets were passed to the
	decoder. */
     if (ogg_d->ov->inited && !(*data&0x80))
     {
	int keyframe_granule_shift=logf(ogg_d->ov->t_info.keyframe_frequency_force-1);
	int64_t iframemask = (1 << keyframe_granule_shift) - 1;

	if (pack->granulepos >= 0)
	{
	   os->lastpos = pack->granulepos >> keyframe_granule_shift;
	   os->lastpos += pack->granulepos & iframemask;
	   *flags = ((pack->granulepos & iframemask) == 0);
	}
	else
	{
	   os->lastpos++;
	}
	pack->granulepos = os->lastpos;
	*pts = (double)os->lastpos / (double)os->samplerate;
     }
  } else if (os->flac) {
     /* we pass complete packets to flac, mustn't strip the header! */
     data = pack->packet;
  } else {
    if(*pack->packet & PACKET_TYPE_HEADER)
      os->hdr_packets++;
    else {
    // Find data start
    int16_t hdrlen = (*pack->packet & PACKET_LEN_BITS01)>>6;
    hdrlen |= (*pack->packet & PACKET_LEN_BITS2) <<1;
    data = pack->packet + 1 + hdrlen;
    // Calculate the timestamp
    if(pack->granulepos == -1)
      pack->granulepos = os->lastpos + (os->lastsize ? os->lastsize : 1);
    // If we alredy have a timestamp it can be a syncpoint
    if(*pack->packet & PACKET_IS_SYNCPOINT)
      *flags = 1;
    *pts =  pack->granulepos/os->samplerate;
    // Save the packet length and timestamp
    os->lastsize = 0;
    while(hdrlen) {
      os->lastsize <<= 8;
      os->lastsize |= pack->packet[hdrlen];
      hdrlen--;
    }
    os->lastpos = pack->granulepos;
  }
  }
  return data;
}

// check if clang has substring from comma separated langlist
static int demux_ogg_check_lang(const char *clang,const char *langlist)
{
  const char *c;

  if (!langlist || !*langlist)
    return 0;
  while ((c = strchr(langlist, ',')))
  {
    if (!strncasecmp(clang, langlist, c - langlist))
      return 1;
    langlist = &c[1];
  }
  if (!strncasecmp(clang, langlist, strlen(langlist)))
    return 1;
  return 0;
}

/** \brief Translate the ogg track number into the subtitle number.
 *  \param demuxer The demuxer about whose subtitles we are inquiring.
 *  \param id The ogg track number of the subtitle track.
 */
static int demux_ogg_sub_reverse_id(Demuxer *demuxer, int id) {
  ogg_demuxer_t *ogg_d = static_cast<ogg_demuxer_t*>(demuxer->priv);
  int i;
  for (i = 0; i < ogg_d->n_text; i++)
    if (ogg_d->text_ids[i] == id) return i;
  return -1;
}

/// Try to print out comments and also check for LANGUAGE= tag
static void demux_ogg_check_comments(Demuxer *d, ogg_stream_t *os, int id, vorbis_comment *vc)
{
  const char *hdr;
  char *val=NULL;
  char **cmt = vc->user_comments;
  int index;
  ogg_demuxer_t *ogg_d = static_cast<ogg_demuxer_t*>(d->priv);

  while(*cmt)
  {
    hdr = NULL;
    if (!strncasecmp(*cmt, "ENCODED_USING=", 14))
    {
      hdr = "Software";
      val = *cmt + 14;
    }
    else if (!strncasecmp(*cmt, "LANGUAGE=", 9))
    {
      val = *cmt + 9;
      if (ogg_d->subs[id].text)
	MSG_V("[Ogg] Language for -sid %d is '-slang \"%s\"'\n", ogg_d->subs[id].id, val);
      // copy this language name into the array
      index = demux_ogg_sub_reverse_id(d, id);
      if (index >= 0) {
	// in case of malicious files with more than one lang per track:
	if (ogg_d->text_langs[index]) delete ogg_d->text_langs[index];
	ogg_d->text_langs[index] = mp_strdup(val);
      }
      // check for -slang if subs are uninitialized yet
      if (os->text && d->sub->id == -1 && demux_ogg_check_lang(val, mp_conf.dvdsub_lang))
      {
	d->sub->id = id;
#if 0
	dvdsub_id = index;
#endif
	MSG_V( "Ogg demuxer: Displaying subtitle stream id %d which matched -slang %s\n", id, val);
      }
      else
	hdr = "Language";
    }
    else if (!strncasecmp(*cmt, "ENCODER_URL=", 12))
    {
      hdr = "Encoder URL";
      val = *cmt + 12;
    }
    else if (!strncasecmp(*cmt, "TITLE=", 6))
    {
      hdr = "Name";
      val = *cmt + 6;
    }
    if (hdr)
      MSG_V( " %s: %s\n", hdr, val);
    cmt++;
  }
}

/// Calculate the timestamp and add the packet to the demux stream
// return 1 if the packet was added, 0 otherwise
static int demux_ogg_add_packet(Demuxer_Stream* ds,ogg_stream_t* os,int id,ogg_packet* pack) {
  Demuxer* d = ds->demuxer;
  ogg_demuxer_t *ogg_d = static_cast<ogg_demuxer_t*>(d->priv);
  Demuxer_Packet* dp;
  unsigned char* data;
  float pts = 0;
  int flags = 0;
  int samplesize = 1;

  // If packet is an comment header then we try to get comments at first
  if (pack->bytes >= 7 && !memcmp(pack->packet, "\003vorbis", 7))
  {
    ogg_d->ov->vi.rate = 1L; // it's checked by vorbis_synthesis_headerin()
    if(vorbis_synthesis_headerin(&ogg_d->ov->vi, &ogg_d->ov->vc, pack) == 0) // if no errors
      demux_ogg_check_comments(d, os, id, &ogg_d->ov->vc);
  }
  if (os->text) {
    if (id == d->sub->id) // don't want to add subtitles to the demuxer for now
      demux_ogg_add_sub(os,pack);
    return 0;
  }
  // If packet is an header we jump it except for vorbis and theora
  // (PACKET_TYPE_HEADER bit doesn't even exist for theora ?!)
  // We jump nothing for FLAC. Ain't this great? Packet contents have to be
  // handled differently for each and every stream type. The joy! The joy!
  if(!os->flac && ((*pack->packet & PACKET_TYPE_HEADER) &&
     (ds != d->audio || ( ((sh_audio_t*)ds->sh)->wtag != FOURCC_VORBIS || os->hdr_packets >= NUM_VORBIS_HDR_PACKETS ) ) &&
     (ds != d->video || (((sh_video_t*)ds->sh)->fourcc != FOURCC_THEORA))))
    return 0;

  // For vorbis packet the packet is the data, for other codec we must jump
  // the header
  if(ds == d->audio && ((sh_audio_t*)ds->sh)->wtag == FOURCC_VORBIS) {
     samplesize = afmt2bps(((sh_audio_t *)ds->sh)->afmt);
  }
  data = demux_ogg_read_packet(ogg_d,os,pack,&pts,&flags,samplesize);

  if(d->video->id < 0)
      ((sh_audio_t*)ds->sh)->a_pts = pts;

  /// Clear subtitles if necessary (for broken files)
  if ((clear_sub > 0) && (pts >= clear_sub)) {
    ogg_sub.lines = 0;
    mpxp_context().video().output->sub = &ogg_sub;
    vo_osd_changed(OSDTYPE_SUBTITLE);
    clear_sub = -1;
  }
  /// Send the packet
  dp = new(zeromem) Demuxer_Packet(pack->bytes-(data-pack->packet));
  memcpy(dp->buffer(),data,pack->bytes-(data-pack->packet));
  dp->pts = pts;
  dp->flags = flags?DP_KEYFRAME:DP_NONKEYFRAME;
  ds->add_packet(dp);
  MSG_DBG2("New dp: %p  ds=%p  pts=%5.3f  len=%d  flag=%d  \n",
      dp, ds, pts, dp->length(), flags);
  return 1;
}

/// if -forceidx build a table of all syncpoints to make seeking easier
/// otherwise try to get at least the final_granulepos
static void demux_ogg_scan_stream(Demuxer* demuxer) {
  ogg_demuxer_t* ogg_d = static_cast<ogg_demuxer_t*>(demuxer->priv);
  Stream *s = demuxer->stream;
  ogg_sync_state* sync = &ogg_d->sync;
  ogg_page* page= &ogg_d->page;
  ogg_stream_state* oss;
  ogg_stream_t* os;
  ogg_packet op;
  int np,sid,p,samplesize=1;
  off_t pos, last_pos;
  pos = last_pos = demuxer->movi_start;

  // Reset the stream
  if(index_mode == 2) {
  s->seek(demuxer->movi_start);
  } else {
    //the 270000 are just a wild guess
    s->seek(std::max(ogg_d->pos,demuxer->movi_end-270000));
  }
  ogg_sync_reset(sync);

  // Get the serial number of the stream we use
  if(demuxer->video->id >= 0) {
    sid = demuxer->video->id;
  }
  else {
    sid = demuxer->audio->id;
    /* demux_ogg_read_packet needs decoder context for Vorbis streams */
    if(((sh_audio_t*)demuxer->audio->sh)->wtag == FOURCC_VORBIS) {
      samplesize = afmt2bps(((sh_audio_t*)demuxer->audio->sh)->afmt);
    }
  }
  os = &ogg_d->subs[sid];
  oss = &os->stream;

  while(1) {
    np = ogg_sync_pageseek(sync,page);
    if(np < 0) { // We had to skip some bytes
      if(index_mode == 2) MSG_ERR("Bad page sync while building syncpoints table (%d)\n",-np);
      pos += -np;
      continue;
    }
    if(np <= 0) { // We need more data
      char* buf = ogg_sync_buffer(sync,BLOCK_SIZE);
      int len = s->read(buf,BLOCK_SIZE);
      if(len == 0 && s->eof())
	break;
      ogg_sync_wrote(sync,len);
      continue;
    }
    // The page is ready
    //ogg_sync_pageout(sync,page);
    if(ogg_page_serialno(page) != os->stream.serialno) { // It isn't a page from the stream we want
      pos += np;
      continue;
    }
    if(ogg_stream_pagein(oss,page) != 0) {
      MSG_ERR("Pagein error ????\n");
      pos += np;
      continue;
    }
    p = 0;
    while(ogg_stream_packetout(oss,&op) == 1) {
      float pts;
      int flags;
      demux_ogg_read_packet(ogg_d,os,&op,&pts,&flags,samplesize);
      if(op.granulepos >= 0) ogg_d->final_granulepos = op.granulepos;
      if(index_mode == 2 && (flags || (os->vorbis && op.granulepos >= 0))) {
	ogg_d->syncpoints = (ogg_syncpoint_t*)mp_realloc(ogg_d->syncpoints,(ogg_d->num_syncpoint+1)*sizeof(ogg_syncpoint_t));
	ogg_d->syncpoints[ogg_d->num_syncpoint].granulepos = op.granulepos;
	ogg_d->syncpoints[ogg_d->num_syncpoint].page_pos = (ogg_page_continued(page) && p == 0) ? last_pos : pos;
	ogg_d->num_syncpoint++;
      }
      p++;
    }
    if(p > 1 || (p == 1 && ! ogg_page_continued(page)))
      last_pos = pos;
    pos += np;
    if(index_mode == 2) MSG_V("Building syncpoint table %d%%\r",(int)(pos*100/s->end_pos()));
  }
  if(index_mode == 2) MSG_V("\n");

  if(index_mode == 2) MSG_V("Ogg syncpoints table builed: %d syncpoints\n",ogg_d->num_syncpoint);
  MSG_V("Ogg stream length (granulepos): %lld\n",ogg_d->final_granulepos);

  s->reset();
  s->seek(demuxer->movi_start);
  ogg_sync_reset(sync);
  for(np = 0 ; np < ogg_d->num_sub ; np++) {
    ogg_stream_reset(&ogg_d->subs[np].stream);
    ogg_d->subs[np].lastpos = ogg_d->subs[np].lastsize = ogg_d->subs[np].hdr_packets = 0;
  }


  // Get the first page
  while(1) {
    np = ogg_sync_pageout(sync,page);
    if(np <= 0) { // We need more data
      char* buf = ogg_sync_buffer(sync,BLOCK_SIZE);
      int len = s->read(buf,BLOCK_SIZE);
      if(len == 0 && s->eof()) {
	MSG_ERR("EOF while trying to get the first page !!!!\n");
	break;
      }

      ogg_sync_wrote(sync,len);
      continue;
    }
    demux_ogg_get_page_stream(ogg_d,&oss);
    ogg_stream_pagein(oss,page);
    break;
  }

}

/** \brief Return the number of subtitle tracks in the file.

  \param demuxer The demuxer for which the number of subtitle tracks
  should be returned.
*/
static int demux_ogg_num_subs(Demuxer *demuxer) {
  ogg_demuxer_t *ogg_d = static_cast<ogg_demuxer_t*>(demuxer->priv);
  return ogg_d->n_text;
}

/** \brief Change the current subtitle stream and return its ID.

  \param demuxer The demuxer whose subtitle stream will be changed.
  \param new_num The number of the new subtitle track. The number must be
  between 0 and ogg_d->n_text - 1.

  \returns The Ogg stream number ( = page serial number) of the newly selected
  track.
*/
static int demux_ogg_sub_id(Demuxer *demuxer, int index) {
  ogg_demuxer_t *ogg_d = static_cast<ogg_demuxer_t*>(demuxer->priv);
  return (index < 0) ? index : (index >= ogg_d->n_text) ? -1 : ogg_d->text_ids[index];
}

/** \brief Lookup the subtitle language by the subtitle number.  Returns NULL on out-of-bounds input.
 *  \param demuxer The demuxer about whose subtitles we are inquiring.
 *  \param index The subtitle number.
 */
char *demux_ogg_sub_lang(Demuxer *demuxer, int index) {
  ogg_demuxer_t *ogg_d = static_cast<ogg_demuxer_t*>(demuxer->priv);
  return (index < 0) ? NULL : (index >= ogg_d->n_text) ? NULL : ogg_d->text_langs[index];
}

static void ogg_close(Demuxer* demuxer);

/// Open an ogg physical stream
static Opaque* ogg_open(Demuxer* demuxer) {
  ogg_demuxer_t* ogg_d;
  Stream *s;
  char* buf;
  int np,s_no, n_audio = 0, n_video = 0;
  int audio_id = -1, video_id = -1, text_id = -1;
  ogg_sync_state* sync;
  ogg_page* page;
  ogg_packet pack;
  sh_audio_t* sh_a;
  sh_video_t* sh_v;

#ifdef USE_ICONV
  subcp_open();
#endif

  clear_sub = -1;
  s = demuxer->stream;

  ogg_d = new(zeromem) ogg_demuxer_t;
  sync = &ogg_d->sync;
  page = &ogg_d->page;

  ogg_sync_init(sync);
  s->reset();

  vorbis_info_init(&ogg_d->ov->vi);
  vorbis_comment_init(&ogg_d->ov->vc);
  ogg_d->ov->inited=1;

  while(1) {
    /// Try to get a page
    ogg_d->pos += ogg_d->last_size;
    np = ogg_sync_pageseek(sync,page);
    /// Error
    if(np < 0) {
      MSG_DBG2("Ogg demuxer : Bad page sync\n");
      goto err_out;
    }
    /// Need some more data
    if(np == 0) {
      int len;
      buf = ogg_sync_buffer(sync,BLOCK_SIZE);
      len = s->read(buf,BLOCK_SIZE);
      if(len == 0 && s->eof()) {
	goto err_out;
      }
      ogg_sync_wrote(sync,len);
      continue;
    }
    ogg_d->last_size = np;
    // We got one page now

    if( ! ogg_page_bos(page) ) { // It's not a begining page
      // Header parsing end here, we need to get the page otherwise it will be lost
      int id = demux_ogg_get_page_stream(ogg_d,NULL);
      if(id >= 0)
	ogg_stream_pagein(&ogg_d->subs[id].stream,page);
      else
	MSG_ERR("Ogg : Warning found none bos page from unknown stream %d\n",ogg_page_serialno(page));
      break;
    }

    /// Init  the data structure needed for a logical stream
    ogg_d->subs = (ogg_stream_t*)mp_realloc(ogg_d->subs,(ogg_d->num_sub+1)*sizeof(ogg_stream_t));
    memset(&ogg_d->subs[ogg_d->num_sub],0,sizeof(ogg_stream_t));
    /// Get the stream serial number
    s_no = ogg_page_serialno(page);
    ogg_stream_init(&ogg_d->subs[ogg_d->num_sub].stream,s_no);
    MSG_DBG2("Ogg : Found a stream with serial=%d\n",s_no);
    // Take the first page
    ogg_stream_pagein(&ogg_d->subs[ogg_d->num_sub].stream,page);
    // Get first packet of the page
    ogg_stream_packetout(&ogg_d->subs[ogg_d->num_sub].stream,&pack);

    // Reset our vars
    sh_a = NULL;
    sh_v = NULL;

#ifdef HAVE_LIBTHEORA
    theora_info_init (&ogg_d->ov->t_info);
    theora_comment_init (&ogg_d->ov->t_comment);
#endif
    // Check for Vorbis
    if(pack.bytes >= 7 && ! strncmp(reinterpret_cast<char*>(&pack.packet[1]),"vorbis", 6) ) {
      sh_a = demuxer->new_sh_audio(ogg_d->num_sub);
      sh_a->wtag = FOURCC_VORBIS;
      ogg_d->subs[ogg_d->num_sub].vorbis = 1;
      ogg_d->subs[ogg_d->num_sub].id = n_audio;
      n_audio++;
      MSG_V("Ogg : stream %d is vorbis\n",ogg_d->num_sub);

      // check for Theora
    } else
#ifdef HAVE_LIBTHEORA
    if (pack.bytes >= 7 && !strncmp (reinterpret_cast<char*>(&pack.packet[1]), "theora", 6)) {
	int errorCode = 0;

	errorCode = theora_decode_header (&ogg_d->ov->t_info, &ogg_d->ov->t_comment, &pack);
	if (errorCode)
	    MSG_ERR("Theora header parsing failed: %i \n",
		   errorCode);
	else {
	    sh_v = demuxer->new_sh_video(ogg_d->num_sub);

	    sh_v->bih = new(zeromem) BITMAPINFOHEADER;
	    sh_v->bih->biSize=sizeof(BITMAPINFOHEADER);
	    sh_v->bih->biCompression= sh_v->fourcc = FOURCC_THEORA;
	    sh_v->fps = ((double)ogg_d->ov->t_info.fps_numerator)/
		(double)ogg_d->ov->t_info.fps_denominator;
	    sh_v->src_w = sh_v->bih->biWidth = ogg_d->ov->t_info.frame_width;
	    sh_v->src_h = sh_v->bih->biHeight = ogg_d->ov->t_info.frame_height;
	    sh_v->bih->biBitCount = 24;
	    sh_v->bih->biPlanes = 3;
	    sh_v->bih->biSizeImage = ((sh_v->bih->biBitCount/8) *
				      sh_v->bih->biWidth*sh_v->bih->biHeight);
	    ogg_d->subs[ogg_d->num_sub].samplerate = sh_v->fps;
	    ogg_d->subs[ogg_d->num_sub].theora = 1;
	    ogg_d->subs[ogg_d->num_sub].id = n_video;
	    n_video++;
	    MSG_V(
		   "Ogg : stream %d is theora v%i.%i.%i %i:%i, %.3f FPS,"
		   " aspect %i:%i\n", ogg_d->num_sub,
		   (int)ogg_d->ov->t_info.version_major,
		   (int)ogg_d->ov->t_info.version_minor,
		   (int)ogg_d->ov->t_info.version_subminor,
		   ogg_d->ov->t_info.width,
		   ogg_d->ov->t_info.height,
		   sh_v->fps,
		   ogg_d->ov->t_info.aspect_numerator,
		   ogg_d->ov->t_info.aspect_denominator);
	    if(mp_conf.verbose>0) print_video_header(sh_v->bih,sizeof(BITMAPINFOHEADER));
	}
    }
#endif
    else if (pack.bytes >= 4 && !strncmp (reinterpret_cast<char*>(&pack.packet[0]), "fLaC", 4)) {
	sh_a = demuxer->new_sh_audio(ogg_d->num_sub);
	sh_a->wtag =  mmioFOURCC('f', 'L', 'a', 'C');
	ogg_d->subs[ogg_d->num_sub].id = n_audio;
	n_audio++;
	ogg_d->subs[ogg_d->num_sub].flac = 1;
	sh_a->wf = NULL;
	MSG_V("Ogg : stream %d is FLAC\n",ogg_d->num_sub);

      /// Check for old header
    } else if(pack.bytes >= 142 && ! strncmp(reinterpret_cast<char*>(&pack.packet[1]),"Direct Show Samples embedded in Ogg",35) ) {

       // Old video header
      if(get_uint32 (reinterpret_cast<uint32_t*>(pack.packet+96)) == 0x05589f80 && pack.bytes >= 184) {
	sh_v = demuxer->new_sh_video(ogg_d->num_sub);
	sh_v->bih = (BITMAPINFOHEADER*)mp_calloc(1,sizeof(BITMAPINFOHEADER));
	sh_v->bih->biSize=sizeof(BITMAPINFOHEADER);
	sh_v->bih->biCompression=
	sh_v->fourcc = mmioFOURCC(pack.packet[68],pack.packet[69],
				pack.packet[70],pack.packet[71]);
	sh_v->fps = 1/(get_uint64(reinterpret_cast<uint64_t*>(pack.packet+164))*0.0000001);
	sh_v->src_w = sh_v->bih->biWidth = get_uint32(reinterpret_cast<uint32_t*>(pack.packet+176));
	sh_v->src_h = sh_v->bih->biHeight = get_uint32(reinterpret_cast<uint32_t*>(pack.packet+180));
	sh_v->bih->biBitCount = get_uint16(reinterpret_cast<uint16_t*>(pack.packet+182));
	if(!sh_v->bih->biBitCount) sh_v->bih->biBitCount=24; // hack, FIXME
	sh_v->bih->biPlanes=1;
	sh_v->bih->biSizeImage=(sh_v->bih->biBitCount>>3)*sh_v->bih->biWidth*sh_v->bih->biHeight;

	ogg_d->subs[ogg_d->num_sub].samplerate = sh_v->fps;
	ogg_d->subs[ogg_d->num_sub].id = n_video;
	n_video++;
	MSG_V("Ogg stream %d is video (old hdr)\n",ogg_d->num_sub);
	if(mp_conf.verbose>0) print_video_header(sh_v->bih,sizeof(BITMAPINFOHEADER));
	// Old audio header
      } else if(get_uint32(reinterpret_cast<uint32_t*>(pack.packet+96)) == 0x05589F81) {
	unsigned int extra_size;
	sh_a = demuxer->new_sh_audio(ogg_d->num_sub);
	extra_size = get_uint16(reinterpret_cast<uint16_t*>(pack.packet+140));
	sh_a->wf = (WAVEFORMATEX*)mp_calloc(1,sizeof(WAVEFORMATEX)+extra_size);
	sh_a->wtag = sh_a->wf->wFormatTag = get_uint16(reinterpret_cast<uint16_t*>(pack.packet+124));
	sh_a->nch = sh_a->wf->nChannels = get_uint16(reinterpret_cast<uint16_t*>(pack.packet+126));
	sh_a->rate = sh_a->wf->nSamplesPerSec = get_uint32(reinterpret_cast<uint32_t*>(pack.packet+128));
	sh_a->wf->nAvgBytesPerSec = get_uint32(reinterpret_cast<uint32_t*>(pack.packet+132));
	sh_a->wf->nBlockAlign = get_uint16(reinterpret_cast<uint16_t*>(pack.packet+136));
	sh_a->wf->wBitsPerSample = get_uint16(reinterpret_cast<uint16_t*>(pack.packet+138));
	sh_a->afmt = bps2afmt((sh_a->wf->wBitsPerSample+7)/8);
	sh_a->wf->cbSize = extra_size;
	if(extra_size > 0)
	  memcpy(sh_a->wf+sizeof(WAVEFORMATEX),pack.packet+142,extra_size);

	ogg_d->subs[ogg_d->num_sub].samplerate = sh_a->rate; // * sh_a->nch;
	ogg_d->subs[ogg_d->num_sub].id = n_audio;
	n_audio++;
	MSG_V("Ogg stream %d is audio (old hdr)\n",ogg_d->num_sub);
	if(mp_conf.verbose>0) print_wave_header(sh_a->wf,sizeof(WAVEFORMATEX)+extra_size);
      } else
	MSG_WARN("Ogg stream %d contains an old header but the header type is unknown: 0x%08X\n",ogg_d->num_sub,get_uint32 (reinterpret_cast<uint32_t*>(pack.packet+96)));

	// Check new header
    } else if ( (*pack.packet & PACKET_TYPE_BITS ) == PACKET_TYPE_HEADER &&
	      pack.bytes >= (int)sizeof(stream_header)+1) {
      stream_header *st = (stream_header*)(pack.packet+1);
      /// New video header
      if(strncmp(st->streamtype,"video",5) == 0) {
	sh_v = demuxer->new_sh_video(ogg_d->num_sub);
	sh_v->bih = (BITMAPINFOHEADER*)mp_calloc(1,sizeof(BITMAPINFOHEADER));
	sh_v->bih->biSize=sizeof(BITMAPINFOHEADER);
	sh_v->bih->biCompression=
	sh_v->fourcc = mmioFOURCC(st->subtype[0],st->subtype[1],
				  st->subtype[2],st->subtype[3]);
	sh_v->fps = 1.0/(get_uint64(reinterpret_cast<uint64_t*>(&st->time_unit))*0.0000001);
	sh_v->bih->biBitCount = get_uint16(reinterpret_cast<uint16_t*>(&st->bits_per_sample));
	sh_v->src_w = sh_v->bih->biWidth = get_uint32(reinterpret_cast<uint32_t*>(&st->sh.video.width));
	sh_v->src_h = sh_v->bih->biHeight = get_uint32(reinterpret_cast<uint32_t*>(&st->sh.video.height));
	if(!sh_v->bih->biBitCount) sh_v->bih->biBitCount=24; // hack, FIXME
	sh_v->bih->biPlanes=1;
	sh_v->bih->biSizeImage=(sh_v->bih->biBitCount>>3)*sh_v->bih->biWidth*sh_v->bih->biHeight;

	ogg_d->subs[ogg_d->num_sub].samplerate= sh_v->fps;
	ogg_d->subs[ogg_d->num_sub].id = n_video;
	n_video++;
	MSG_V("Ogg stream %d is video (new hdr)\n",ogg_d->num_sub);
	if(mp_conf.verbose>0) print_video_header(sh_v->bih,sizeof(BITMAPINFOHEADER));
	/// New audio header
      } else if(strncmp(st->streamtype,"audio",5) == 0) {
	char buffer[5];
	unsigned int extra_size = get_uint32 (reinterpret_cast<uint32_t*>(&st->size)) - sizeof(stream_header);
	memcpy(buffer,st->subtype,4);
	buffer[4] = '\0';
	sh_a = demuxer->new_sh_audio(ogg_d->num_sub);
	sh_a->wf = (WAVEFORMATEX*)mp_calloc(1,sizeof(WAVEFORMATEX)+extra_size);
	sh_a->wtag =  sh_a->wf->wFormatTag = strtol(buffer, NULL, 16);
	sh_a->nch = sh_a->wf->nChannels = get_uint16(reinterpret_cast<uint16_t*>(&st->sh.audio.channels));
	sh_a->rate = sh_a->wf->nSamplesPerSec = get_uint64(reinterpret_cast<uint64_t*>(&st->samples_per_unit));
	sh_a->wf->nAvgBytesPerSec = get_uint32(reinterpret_cast<uint32_t*>(&st->sh.audio.avgbytespersec));
	sh_a->wf->nBlockAlign = get_uint16(reinterpret_cast<uint16_t*>(&st->sh.audio.blockalign));
	sh_a->wf->wBitsPerSample = get_uint16(reinterpret_cast<uint16_t*>(&st->bits_per_sample));
	sh_a->afmt = bps2afmt((sh_a->wf->wBitsPerSample+7)/8);
	sh_a->wf->cbSize = extra_size;
	if(extra_size)
	  memcpy(sh_a->wf+sizeof(WAVEFORMATEX),st+1,extra_size);

	ogg_d->subs[ogg_d->num_sub].samplerate = sh_a->rate; // * sh_a->nch;
	ogg_d->subs[ogg_d->num_sub].id = n_audio;
	n_audio++;
	MSG_V("Ogg stream %d is audio (new hdr)\n",ogg_d->num_sub);
	if(mp_conf.verbose>0) print_wave_header(sh_a->wf,sizeof(WAVEFORMATEX)+extra_size);

	/// Check for text (subtitles) header
      } else if (strncmp(st->streamtype, "text", 4) == 0) {
	  MSG_V( "Ogg stream %d is text\n", ogg_d->num_sub);
	  ogg_d->subs[ogg_d->num_sub].samplerate= get_uint64(reinterpret_cast<uint64_t*>(&st->time_unit))/10;
	  ogg_d->subs[ogg_d->num_sub].text = 1;
	  ogg_d->subs[ogg_d->num_sub].id = ogg_d->n_text;
	  if (demuxer->sub->id == ogg_d->n_text)
	    text_id = ogg_d->num_sub;
	  ogg_d->n_text++;
	  ogg_d->text_ids = (int *)mp_realloc(ogg_d->text_ids, sizeof(int) * ogg_d->n_text);
	  ogg_d->text_ids[ogg_d->n_text - 1] = ogg_d->num_sub;
	  ogg_d->text_langs = (char **)mp_realloc(ogg_d->text_langs, sizeof(char *) * ogg_d->n_text);
	  ogg_d->text_langs[ogg_d->n_text - 1] = NULL;
	  demux_ogg_init_sub();
	//// Unknown header type
      } else
	MSG_ERR("Ogg stream %d has a header marker but is of an unknown type\n",ogg_d->num_sub);
      /// Unknown (invalid ?) header
    } else
      MSG_ERR("Ogg stream %d is of an unknown type\n",ogg_d->num_sub);

    if(sh_a || sh_v) {
      Demuxer_Stream* ds = NULL;
      if(sh_a) {
	// If the audio stream is not defined we took the first one
	if(demuxer->audio->id == -1) {
	  demuxer->audio->id = n_audio - 1;
	}
	/// Is it the stream we want
	if(demuxer->audio->id == (n_audio - 1)) {
	  demuxer->audio->sh = sh_a;
	  sh_a->ds = demuxer->audio;
	  ds = demuxer->audio;
	  audio_id = ogg_d->num_sub;
	}
      }
      if(sh_v) {
	/// Also for video
	if(demuxer->video->id == -1) {
	  demuxer->video->id = n_video - 1;
	}
	if(demuxer->video->id == (n_video - 1)) {
	  demuxer->video->sh = sh_v;
	  sh_v->ds = demuxer->video;
	  ds = demuxer->video;
	  video_id = ogg_d->num_sub;
	}
      }
      /// Add the header packets if the stream isn't seekable
      if(ds && !s->end_pos()) {
	/// Finish the page, otherwise packets will be lost
	do {
	  demux_ogg_add_packet(ds,&ogg_d->subs[ogg_d->num_sub],ogg_d->num_sub,&pack);
	} while(ogg_stream_packetout(&ogg_d->subs[ogg_d->num_sub].stream,&pack) == 1);
      }
    }
    ogg_d->num_sub++;
  }

  if(!n_video && !n_audio) {
    goto err_out;
  }

  /// Finish to setup the demuxer
  demuxer->priv = ogg_d;

  if(!n_video || (video_id < 0))
    demuxer->video->id = -2;
  else
    demuxer->video->id = video_id;
  if(!n_audio || (audio_id < 0))
    demuxer->audio->id = -2;
  else
    demuxer->audio->id = audio_id;
  /* Disable the subs only if there are no text streams at all.
     Otherwise the stream to display might be chosen later when the comment
     packet is encountered and the user used -slang instead of -sid. */
  if(!ogg_d->n_text)
    demuxer->sub->id = -2;
  else if (text_id >= 0) {
    demuxer->sub->id = text_id;
    MSG_V( "Ogg demuxer: Displaying subtitle stream id %d\n", text_id);
  }

  ogg_d->final_granulepos=0;
  if(!s->end_pos())
    demuxer->flags &= ~(Demuxer::Seekable);
  else {
    demuxer->movi_start = s->start_pos(); // Needed for XCD (Ogg written in MODE2)
    demuxer->movi_end = s->end_pos();
    demuxer->flags |= Demuxer::Seekable;
    demux_ogg_scan_stream(demuxer);
  }
    MSG_V("Ogg demuxer : found %d audio stream%s, %d video stream%s and %d text stream%s\n",n_audio,n_audio>1?"s":"",n_video,n_video>1?"s":"",ogg_d->n_text,ogg_d->n_text>1?"s":"");
    check_pin("demuxer",demuxer->pin,DEMUX_PIN);
    return ogg_d;

err_out:
  ogg_close(demuxer);
  return NULL;
}


static int ogg_demux(Demuxer *d,Demuxer_Stream *__ds) {
    UNUSED(__ds);
  ogg_demuxer_t* ogg_d;
  Stream *s;
  Demuxer_Stream *ds;
  ogg_sync_state* sync;
  ogg_stream_state* os;
  ogg_page* page;
  ogg_packet pack;
  int np = 0, id=0;

  s = d->stream;
  ogg_d = static_cast<ogg_demuxer_t*>(d->priv);
  sync = &ogg_d->sync;
  page = &ogg_d->page;

  /// Find the stream we are working on
  if ( (id = demux_ogg_get_page_stream(ogg_d,&os)) < 0) {
      MSG_ERR("Ogg demuxer : can't get current stream\n");
      return 0;
  }

  while(1) {
    np = 0;
    ds = NULL;
    /// Try to get some packet from the current page
    while( (np = ogg_stream_packetout(os,&pack)) != 1) {
      /// No packet we go the next page
      if(np == 0) {
	while(1) {
	  int pa,len;
	  char *buf;
	  ogg_d->pos += ogg_d->last_size;
	  /// Get the next page from the physical stream
	  while( (pa = ogg_sync_pageseek(sync,page)) <= 0) {
	    /// Error : we skip some bytes
	    if(pa < 0) {
	      MSG_WARN("Ogg : Page out not synced, we skip some bytes\n");
	      ogg_d->pos -= pa;
	      continue;
	    }
	    /// We need more data
	    buf = ogg_sync_buffer(sync,BLOCK_SIZE);
	    len = s->read(buf,BLOCK_SIZE);
	    if(len == 0 && s->eof()) {
	      MSG_DBG2("Ogg : Stream EOF !!!!\n");
	      return 0;
	    }
	    ogg_sync_wrote(sync,len);
	  } /// Page loop
	  ogg_d->last_size = pa;
	  /// Find the page's logical stream
	  if( (id = demux_ogg_get_page_stream(ogg_d,&os)) < 0) {
	    MSG_ERR("Ogg demuxer error : we met an unknown stream\n");
	    return 0;
	  }
	  /// Take the page
	  if(ogg_stream_pagein(os,page) == 0)
	    break;
	  /// Page was invalid => retry
	  MSG_WARN("Ogg demuxer : got invalid page !!!!!\n");
	  ogg_d->pos += ogg_d->last_size;
	}
      } else /// Packet was corrupted
	MSG_WARN("Ogg : bad packet in stream %d\n",id);
    } /// Packet loop

    /// Is the actual logical stream in use ?
    if(id == d->audio->id)
      ds = d->audio;
    else if(id == d->video->id)
      ds = d->video;
    else if (ogg_d->subs[id].text)
      ds = d->sub;

    if(ds) {
      if(!demux_ogg_add_packet(ds,&ogg_d->subs[id],id,&pack))
	continue; /// Unuseful packet, get another
      d->filepos = ogg_d->pos;
      return 1;
    }

  } /// while(1)

}

static void ogg_seek(Demuxer *demuxer,const seek_args_t* seeka) {
  ogg_demuxer_t* ogg_d = static_cast<ogg_demuxer_t*>(demuxer->priv);
  ogg_sync_state* sync = &ogg_d->sync;
  ogg_page* page= &ogg_d->page;
  ogg_stream_state* oss;
  ogg_stream_t* os;
  Demuxer_Stream* ds;
  ogg_packet op;
  float rate;
  int i,sp,first=0,precision=1,do_seek=1;
  vorbis_info* vi = NULL;
  int64_t gp = 0, old_gp;
  off_t pos, old_pos;
  int np;
  int is_gp_valid;
  float pts;
  int is_keyframe;
  int samplesize=1;

  if(demuxer->video->id >= 0) {
    ds = demuxer->video;
    rate = ogg_d->subs[ds->id].samplerate;
  } else {
    ds = demuxer->audio;
    vi = &ogg_d->ov->vi;
    rate = (float)vi->rate;
    samplesize = afmt2bps(((sh_audio_t*)ds->sh)->afmt);
  }

  os = &ogg_d->subs[ds->id];
  oss = &os->stream;

  old_gp = os->lastpos;
  old_pos = ogg_d->pos;

  //calculate the granulepos to seek to
    gp = seeka->flags & DEMUX_SEEK_SET ? 0 : os->lastpos;
  if(seeka->flags & DEMUX_SEEK_PERCENTS) {
    if (ogg_d->final_granulepos > 0)
      gp += ogg_d->final_granulepos * seeka->secs;
      else
      gp += seeka->secs * (demuxer->movi_end - demuxer->movi_start) * os->lastpos / ogg_d->pos;
  } else
      gp += seeka->secs * rate;
  if (gp < 0) gp = 0;

  //calculate the filepos to seek to
  if(ogg_d->syncpoints) {
    for(sp = 0; sp < ogg_d->num_syncpoint ; sp++) {
      if(ogg_d->syncpoints[sp].granulepos >= gp) break;
    }

    if(sp >= ogg_d->num_syncpoint) return;
    if (sp > 0 && ogg_d->syncpoints[sp].granulepos - gp > gp - ogg_d->syncpoints[sp-1].granulepos)
      sp--;
    if (ogg_d->syncpoints[sp].granulepos == os->lastpos) {
      if (sp > 0 && gp < os->lastpos) sp--;
      if (sp < ogg_d->num_syncpoint-1 && gp > os->lastpos) sp++;
    }
    pos = ogg_d->syncpoints[sp].page_pos;
    precision = 0;
  } else {
    pos = seeka->flags & DEMUX_SEEK_SET ? 0 : ogg_d->pos;
    if(seeka->flags & DEMUX_SEEK_PERCENTS)
      pos += (demuxer->movi_end - demuxer->movi_start) * seeka->secs;
    else {
      if (ogg_d->final_granulepos > 0) {
	pos += seeka->secs * (demuxer->movi_end - demuxer->movi_start) / (ogg_d->final_granulepos / rate);
      } else if (os->lastpos > 0) {
      pos += seeka->secs * ogg_d->pos / (os->lastpos / rate);
    }
  }
    if (pos < 0) pos = 0;
    if (pos > (demuxer->movi_end - demuxer->movi_start)) return;
  } // if(ogg_d->syncpoints)

  while(1) {
    if (do_seek) {
  demuxer->stream->seek(pos+demuxer->movi_start);
  ogg_sync_reset(sync);
  for(i = 0 ; i < ogg_d->num_sub ; i++) {
    ogg_stream_reset(&ogg_d->subs[i].stream);
    ogg_d->subs[i].lastpos = ogg_d->subs[i].lastsize = 0;
  }
  ogg_d->pos = pos;
  ogg_d->last_size = 0;
      /* we just guess that we reached correct granulepos, in case a
	 subsequent search occurs before we read a valid granulepos */
      os->lastpos = gp;
      first = !(ogg_d->syncpoints);
      do_seek=0;
    }
    ogg_d->pos += ogg_d->last_size;
    ogg_d->last_size = 0;
    np = ogg_sync_pageseek(sync,page);

    if(np < 0)
      ogg_d->pos -= np;
    if(np <= 0) { // We need more data
      char* buf = ogg_sync_buffer(sync,BLOCK_SIZE);
      int len = demuxer->stream->read(buf,BLOCK_SIZE);
       if(len == 0 && demuxer->stream->eof()) {
	MSG_ERR("EOF while trying to seek !!!!\n");
	break;
      }
      ogg_sync_wrote(sync,len);
      continue;
    }
    ogg_d->last_size = np;
    if(ogg_page_serialno(page) != oss->serialno)
      continue;

    if(ogg_stream_pagein(oss,page) != 0)
      continue;

     while(1) {
      np = ogg_stream_packetout(oss,&op);
      if(np < 0)
	continue;
      else if(np == 0)
	break;
      if (first) { /* Discard the first packet as it's probably broken,
	   and we don't have any other means to decide whether it is
	   complete or not. */
	first = 0;
	break;
      }
      is_gp_valid = (op.granulepos >= 0);
      demux_ogg_read_packet(ogg_d,os,&op,&pts,&is_keyframe,samplesize);
      if (precision && is_gp_valid) {
	precision--;
	if (abs(gp - op.granulepos) > rate && (op.granulepos != old_gp)) {
	  //prepare another seek because we are off by more than 1s
	  pos += (gp - op.granulepos) * (pos - old_pos) / (op.granulepos - old_gp);
	  if (pos < 0) pos = 0;
	  if (pos < (demuxer->movi_end - demuxer->movi_start)) {
	    do_seek=1;
	    break;
	  }
	}
      }
      if (is_gp_valid && (pos > 0) && (old_gp > gp)
	  && (2 * (old_gp - op.granulepos) < old_gp - gp)) {
	/* prepare another seek because looking for a syncpoint
	   destroyed the backward search */
	pos = old_pos - 1.5 * (old_pos - pos);
	if (pos < 0) pos = 0;
	if (pos < (demuxer->movi_end - demuxer->movi_start)) {
	  do_seek=1;
	  break;
	}
      }
      if(!precision && (is_keyframe || os->vorbis) ) {
	ogg_sub.lines = 0;
	mpxp_context().video().output->sub = &ogg_sub;
	vo_osd_changed(OSDTYPE_SUBTITLE);
	clear_sub = -1;
	demux_ogg_add_packet(ds,os,ds->id,&op);
	return;
      }
     }
  }

  MSG_ERR("Can't find the good packet :(\n");

}

static void ogg_close(Demuxer* demuxer) {
  ogg_demuxer_t* ogg_d = static_cast<ogg_demuxer_t*>(demuxer->priv);

  if(!ogg_d) return;

#ifdef USE_ICONV
  subcp_close();
#endif
#ifdef HAVE_LIBTHEORA
    theora_comment_clear (&ogg_d->ov->t_comment);
    theora_info_clear (&ogg_d->ov->t_info);
#endif
    vorbis_comment_clear(&ogg_d->ov->vc);
    vorbis_info_clear(&ogg_d->ov->vi);

    delete ogg_d;
}

static MPXP_Rc ogg_control(const Demuxer *demuxer,int cmd,any_t*args)
{
    UNUSED(demuxer);
    UNUSED(cmd);
    UNUSED(args);
    return MPXP_Unknown;
}

extern const demuxer_driver_t demux_ogg =
{
    "ogg",
    "OGG/Vorbis parser",
    ".ogg",
    NULL,
    ogg_probe,
    ogg_open,
    ogg_demux,
    ogg_seek,
    ogg_close,
    ogg_control
};
