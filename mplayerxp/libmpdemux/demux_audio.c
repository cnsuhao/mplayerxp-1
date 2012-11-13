#include "mp_config.h"

#include <stdlib.h>
#include <stdio.h>
#include "stream.h"
#include "demuxer.h"
#include "stheader.h"
#include "genres.h"
#include <limits.h>
#include "libmpcodecs/dec_audio.h"
#include "libao2/afmt.h"
#include "aviprint.h"
#include "osdep/bswap.h"
#include "mp3_hdr.h"
#include <string.h>
#ifdef MP_DEBUG
#include <assert.h>
#endif
#include "demux_msg.h"
#include "osdep/mplib.h"

#define RAW_MP1 1
#define RAW_MP2 2
#define RAW_MP3 3
#define RAW_WAV 4
#define RAW_FLAC 5
#define RAW_SND_AU 6
#define RAW_AC3 7
#define RAW_DCA 8
#define RAW_VOC 9
#define RAW_MUSEPACK 10

#define HDR_SIZE 4

typedef struct da_priv {
  int frmt;
  float last_pts,pts_per_packet,length;
  uint32_t dword;
  int pos;
  /* Xing's VBR specific extensions */
  int	   is_xing;
  unsigned nframes;
  unsigned nbytes;
  int	   scale;
  unsigned srate;
  int	   lsf;
  unsigned char	toc[100]; /* like AVI's indexes */
} da_priv_t;

static int hr_mp3_seek = 0;

#define DDCA_MONO 0
#define DDCA_CHANNEL 1
#define DDCA_STEREO 2
#define DDCA_STEREO_SUMDIFF 3
#define DDCA_STEREO_TOTAL 4
#define DDCA_3F 5
#define DDCA_2F1R 6
#define DDCA_3F1R 7
#define DDCA_2F2R 8
#define DDCA_3F2R 9
#define DDCA_4F2R 10

#define DDCA_DOLBY 101 /* FIXME */

#define DDCA_CHANNEL_MAX  DDCA_3F2R /* We don't handle anything above that */
#define DDCA_CHANNEL_BITS 6
#define DDCA_CHANNEL_MASK 0x3F

#define DDCA_LFE 0x80
#define DDCA_ADJUST_LEVEL 0x100

typedef struct ddca_state_s
{
    const uint32_t *buffer_start;
    uint32_t bits_left,current_word;
    int word_mode,bigendian_mode;
}ddca_state_t;

#ifdef WORDS_BIGENDIAN
#   define ddca_swab32(x) (x)
#else
#   define ddca_swab32(x)\
((((uint8_t*)&x)[0] << 24) | (((uint8_t*)&x)[1] << 16) |  \
 (((uint8_t*)&x)[2] << 8)  | (((uint8_t*)&x)[3]))
#endif

#ifdef WORDS_BIGENDIAN
#   define ddca_swable32(x)\
((((uint8_t*)&x)[0] << 16) | (((uint8_t*)&x)[1] << 24) |  \
 (((uint8_t*)&x)[2])  | (((uint8_t*)&x)[3] << 8))
#else
#   define ddca_swable32(x)\
((((uint16_t*)&x)[0] << 16) | (((uint16_t*)&x)[1]))
#endif

static uint32_t ddca_bitstream_get_bh (ddca_state_t * state, uint32_t num_bits);

static uint32_t ddca_bitstream_get (ddca_state_t * state, uint32_t num_bits)
{
    uint32_t result;

    if (num_bits < state->bits_left) {
	result = (state->current_word << (32 - state->bits_left))
				      >> (32 - num_bits);

	state->bits_left -= num_bits;
	return result;
    }

    return ddca_bitstream_get_bh (state, num_bits);
}

static void ddca_bitstream_init (ddca_state_t * state,const uint8_t * buf, int word_mode,
			 int bigendian_mode)
{
    intptr_t align;

    align = (uintptr_t)buf & 3;
    state->buffer_start = (uint32_t *) (buf - align);
    state->bits_left = 0;
    state->current_word = 0;
    state->word_mode = word_mode;
    state->bigendian_mode = bigendian_mode;
    ddca_bitstream_get (state, align * 8);
}

static void ddca_bitstream_fill_current (ddca_state_t * state)
{
    uint32_t tmp;

    tmp = *(state->buffer_start++);

    if (state->bigendian_mode)
	state->current_word = ddca_swab32 (tmp);
    else
	state->current_word = ddca_swable32 (tmp);

    if (!state->word_mode)
    {
	state->current_word = (state->current_word & 0x00003FFF) |
	    ((state->current_word & 0x3FFF0000 ) >> 2);
    }
}

static uint32_t ddca_bitstream_get_bh (ddca_state_t * state, uint32_t num_bits)
{
    uint32_t result;

    num_bits -= state->bits_left;

    result = ((state->current_word << (32 - state->bits_left)) >>
	      (32 - state->bits_left));

    if ( !state->word_mode && num_bits > 28 ) {
	ddca_bitstream_fill_current (state);
	result = (result << 28) | state->current_word;
	num_bits -= 28;
    }

    ddca_bitstream_fill_current (state);

    if ( state->word_mode )
    {
	if (num_bits != 0)
	    result = (result << num_bits) |
		     (state->current_word >> (32 - num_bits));

	state->bits_left = 32 - num_bits;
    }
    else
    {
	if (num_bits != 0)
	    result = (result << num_bits) |
		     (state->current_word >> (28 - num_bits));

	state->bits_left = 28 - num_bits;
    }

    return result;
}

static int ddca_syncinfo (ddca_state_t * state, unsigned * flags,
		     unsigned * sample_rate, unsigned * bit_rate, unsigned * frame_length)
{
static const int ddca_sample_rates[] =
{
    0, 8000, 16000, 32000, 0, 0, 11025, 22050, 44100, 0, 0,
    12000, 24000, 48000, 96000, 192000
};

static const int ddca_bit_rates[] =
{
    32000, 56000, 64000, 96000, 112000, 128000,
    192000, 224000, 256000, 320000, 384000,
    448000, 512000, 576000, 640000, 768000,
    896000, 1024000, 1152000, 1280000, 1344000,
    1408000, 1411200, 1472000, 1536000, 1920000,
    2048000, 3072000, 3840000, 1/*open*/, 2/*variable*/, 3/*lossless*/
};

#if 0
static const uint8_t ddca_channels[] =
{
    1, 2, 2, 2, 2, 3, 3, 4, 4, 5, 6, 6, 6, 7, 8, 8
};

static const uint8_t ddca_bits_per_sample[] =
{
    16, 16, 20, 20, 0, 24, 24
};
#endif
    int frame_size;

    /* Sync code */
    ddca_bitstream_get (state, 32);
    /* Frame type */
    ddca_bitstream_get (state, 1);
    /* Samples deficit */
    ddca_bitstream_get (state, 5);
    /* CRC present */
    ddca_bitstream_get (state, 1);

    *frame_length = (ddca_bitstream_get (state, 7) + 1) * 32;
    frame_size = ddca_bitstream_get (state, 14) + 1;
    if (!state->word_mode) frame_size = frame_size * 8 / 14 * 2;

    /* Audio channel arrangement */
    *flags = ddca_bitstream_get (state, 6);
    if (*flags > 63) return 0;

    *sample_rate = ddca_bitstream_get (state, 4);
    if (*sample_rate >= sizeof (ddca_sample_rates) / sizeof (int)) return 0;
    *sample_rate = ddca_sample_rates[ *sample_rate ];
    if (!*sample_rate) return 0;

    *bit_rate = ddca_bitstream_get (state, 5);
    if (*bit_rate >= sizeof (ddca_bit_rates) / sizeof (int)) return 0;
    *bit_rate = ddca_bit_rates[ *bit_rate ];
    if (!*bit_rate) return 0;

    /* LFE */
    ddca_bitstream_get (state, 10);
    if (ddca_bitstream_get (state, 2)) *flags |= DDCA_LFE;

    return frame_size;
}

static int ddca_decode_header (const uint8_t * buf, unsigned* sample_rate, unsigned* bit_rate,unsigned*channels)
{
    ddca_state_t state;
    unsigned flags,frame_length,frame_size=0;
    /* 14 bits and little endian bitstream */
    if (buf[0] == 0xff && buf[1] == 0x1f &&
	buf[2] == 0x00 && buf[3] == 0xe8 &&
	(buf[4] & 0xf0) == 0xf0 && buf[5] == 0x07)
    {
	MSG_DBG2("DCA: 14 bits and little endian bitstream\n");
	ddca_bitstream_init (&state, buf, 0, 0);
	frame_size = ddca_syncinfo (&state, &flags, sample_rate,
			       bit_rate, &frame_length);
    }
    else
    /* 14 bits and big endian bitstream */
    if (buf[0] == 0x1f && buf[1] == 0xff &&
	buf[2] == 0xe8 && buf[3] == 0x00 &&
	buf[4] == 0x07 && (buf[5] & 0xf0) == 0xf0)
    {
	MSG_DBG2("DCA: 14 bits and big endian bitstream\n");
	ddca_bitstream_init (&state, buf, 0, 1);
	frame_size = ddca_syncinfo (&state, &flags, sample_rate,
			       bit_rate, &frame_length);
    }
    else
    /* 16 bits and little endian bitstream */
    if (buf[0] == 0xfe && buf[1] == 0x7f &&
	buf[2] == 0x01 && buf[3] == 0x80)
    {
	MSG_DBG2("DCA: 16 bits and little endian bitstream\n");
	ddca_bitstream_init (&state, buf, 1, 0);
	frame_size = ddca_syncinfo (&state, &flags, sample_rate,
			       bit_rate, &frame_length);
    }
    else
    /* 16 bits and big endian bitstream */
    if (buf[0] == 0x7f && buf[1] == 0xfe &&
	buf[2] == 0x80 && buf[3] == 0x01)
    {
	MSG_DBG2("DCA: 16 bits and big endian bitstream\n");
	ddca_bitstream_init (&state, buf, 1, 1);
	frame_size = ddca_syncinfo (&state, &flags, sample_rate,
			       bit_rate, &frame_length);
    }
    *channels=0;
    if(frame_size)
    {
	switch(flags&DDCA_CHANNEL_MASK)
	{
	    case DDCA_MONO: *channels=1; break;
	    case DDCA_CHANNEL:
	    case DDCA_STEREO:
	    case DDCA_STEREO_SUMDIFF:
	    case DDCA_STEREO_TOTAL: *channels=2; break;
	    case DDCA_3F:
	    case DDCA_2F1R: *channels=3; break;
	    case DDCA_3F1R:
	    case DDCA_2F2R: *channels=4; break;
	    case DDCA_3F2R: *channels=5; break;
	    case DDCA_4F2R: *channels=6; break;
	    default: break;
	}
//	if(flags&DDCA_DOLBY) (*channels)++;
	if(flags&DDCA_LFE) (*channels)++;
    }
    return frame_size;
}

#define AC3_CHANNEL 0
#define AC3_MONO 1
#define AC3_STEREO 2
#define AC3_3F 3
#define AC3_2F1R 4
#define AC3_3F1R 5
#define AC3_2F2R 6
#define AC3_3F2R 7
#define AC3_CHANNEL1 8
#define AC3_CHANNEL2 9
#define AC3_DOLBY 10
#define AC3_CHANNEL_MASK 15
#define AC3_LFE 16
#define AC3_ADJUST_LEVEL 32
static int ac3_decode_header (const uint8_t * buf,unsigned* sample_rate,unsigned* bit_rate,unsigned* channels)
{
    static int rate[] = { 32,  40,  48,  56,  64,  80,  96, 112,
			 128, 160, 192, 224, 256, 320, 384, 448,
			 512, 576, 640};
    static uint8_t lfeon[8] = {0x10, 0x10, 0x04, 0x04, 0x04, 0x01, 0x04, 0x01};
    static uint8_t halfrate[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3};
    int frmsizecod;
    int bitrate;
    int half;
    int acmod;
    int flags;

    if ((buf[0] != 0x0b) || (buf[1] != 0x77))	/* syncword */
	return 0;

    if (buf[5] >= 0x60)		/* bsid >= 12 */
	return 0;
    half = halfrate[buf[5] >> 3];

    /* acmod, dsurmod and lfeon */
    acmod = buf[6] >> 5;
    flags = ((((buf[6] & 0xf8) == 0x50) ? AC3_DOLBY : acmod) |
	      ((buf[6] & lfeon[acmod]) ? AC3_LFE : 0));

    switch(flags & AC3_CHANNEL_MASK)
    {
	default:
	case AC3_CHANNEL1:
	case AC3_CHANNEL2:
	case AC3_CHANNEL:
	case AC3_MONO:
	    *channels=1;
	    break;
	case AC3_STEREO:
	    *channels=2;
	    break;
	case AC3_3F:
	case AC3_2F1R:
	    *channels=3;
	    break;
	case AC3_3F1R:
	case AC3_2F2R:
	    *channels=4;
	    break;
	case AC3_3F2R:
	    *channels=5;
	    break;
	case AC3_DOLBY:
	    *channels=6;
	    break;
    }
    if((flags & AC3_LFE)==AC3_LFE) (*channels)++;
    frmsizecod = buf[4] & 63;
    if (frmsizecod >= 38)
	return 0;
    bitrate = rate [frmsizecod >> 1];
    *bit_rate = (bitrate * 1000) >> half;

    switch (buf[4] & 0xc0) {
    case 0:
	*sample_rate = 48000 >> half;
	return 4 * bitrate;
    case 0x40:
	*sample_rate = 44100 >> half;
	return 2 * (320 * bitrate / 147 + (frmsizecod & 1));
    case 0x80:
	*sample_rate = 32000 >> half;
	return 6 * bitrate;
    default:
	return 0;
    }
}

static void find_next_mp3_hdr(demuxer_t *demuxer,uint8_t *hdr) {
  int len;
  off_t spos;
  while(!stream_eof(demuxer->stream)) {
    spos=stream_tell(demuxer->stream);
    stream_read(demuxer->stream,hdr,4);
    len = mp_decode_mp3_header(hdr,NULL,NULL,NULL,NULL);
    if(len < 0) {
      stream_skip(demuxer->stream,-3);
      continue;
    }
    stream_seek(demuxer->stream,spos);
    break;
  }
}


static int read_mp3v1_tags(demuxer_t *demuxer,uint8_t *hdr, off_t pos )
{
    unsigned n;
    stream_t *s=demuxer->stream;
    for(n = 0; n < 5 ; n++) {
      MSG_DBG2("read_mp3v1_tags\n");
      pos = mp_decode_mp3_header(hdr,NULL,NULL,NULL,NULL);
      if(pos < 0)
	return 0;
      stream_skip(s,pos-4);
      if(stream_eof(s))
	return 0;
      stream_read(s,hdr,4);
      if(stream_eof(s))
	return 0;
    }
    if(s->end_pos) {
      char tag[4];
      stream_seek(s,s->end_pos-128);
      stream_read(s,tag,3);
      tag[3] = '\0';
      if(strcmp(tag,"TAG"))
	demuxer->movi_end = s->end_pos;
      else {
	char buf[31];
	uint8_t g;
	demuxer->movi_end = stream_tell(s)-3;
	stream_read(s,buf,30);
	buf[30] = '\0';
	demux_info_add(demuxer,INFOT_NAME,buf);
	stream_read(s,buf,30);
	buf[30] = '\0';
	demux_info_add(demuxer,INFOT_AUTHOR,buf);
	stream_read(s,buf,30);
	buf[30] = '\0';
	demux_info_add(demuxer,INFOT_ALBUM,buf);
	stream_read(s,buf,4);
	buf[4] = '\0';
	demux_info_add(demuxer,INFOT_DATE,buf);
	stream_read(s,buf,30);
	buf[30] = '\0';
	demux_info_add(demuxer,INFOT_COMMENTS,buf);
	if(buf[28] == 0 && buf[29] != 0) {
	  uint8_t trk = (uint8_t)buf[29];
	  sprintf(buf,"%d",trk);
	  demux_info_add(demuxer,INFOT_TRACK,buf);
	}
	g = stream_read_char(s);
	demux_info_add(demuxer,INFOT_GENRE,genres[g]);
      }
    }
    return 1;
}

static int read_ac3_tags(demuxer_t *demuxer,uint8_t *hdr, off_t pos,unsigned *bitrate,unsigned *samplerate,unsigned *channels)
{
    char b[8];
    unsigned n;
    stream_t *s=demuxer->stream;
    demuxer->movi_end = s->end_pos;
    memcpy(b,hdr,4);
    stream_seek(s,pos+4);
    stream_read(s,&b[4],4);
    for(n = 0; n < 5 ; n++) {
      MSG_DBG2("read_ac3_tags\n");
      pos = ac3_decode_header(b,bitrate,samplerate,channels);
      if(pos < 0)
	return 0;
      stream_skip(s,pos-8);
      if(stream_eof(s))
	return 0;
      stream_read(s,b,8);
      if(stream_eof(s))
	return 0;
    }
    return 1;
}

static int read_ddca_tags(demuxer_t *demuxer,uint8_t *hdr, off_t pos,unsigned *bitrate,unsigned *samplerate,unsigned *channels)
{
    char b[12];
    unsigned n;
    stream_t *s=demuxer->stream;
    demuxer->movi_end = s->end_pos;
    memcpy(b,hdr,4);
    stream_seek(s,pos+4);
    stream_read(s,&b[4],8);
    for(n = 0; n < 5 ; n++) {
      MSG_DBG2("read_ddca_tags\n");
      pos = ddca_decode_header(b,bitrate,samplerate,channels);
      if(pos < 0)
	return 0;
      stream_skip(s,pos-12);
      if(stream_eof(s))
	return 0;
      stream_read(s,hdr,12);
      if(stream_eof(s))
	return 0;
    }
    return 1;
}
/* id3v2 */
#define FOURCC_TAG BE_FOURCC
#define ID3V22_TAG FOURCC_TAG('I', 'D', '3', 2)  /* id3 v2.2 tags */
#define ID3V23_TAG FOURCC_TAG('I', 'D', '3', 3)  /* id3 v2.3 tags */
#define ID3V24_TAG FOURCC_TAG('I', 'D', '3', 4)  /* id3 v2.4 tags */

/*
 *  ID3 v2.2
 */
/* tag header */
#define ID3V22_UNSYNCH_FLAG               0x80
#define ID3V22_COMPRESS_FLAG              0x40
#define ID3V22_ZERO_FLAG                  0x3F

/* frame header */
#define ID3V22_FRAME_HEADER_SIZE             6
static int read_id3v22_tags(demuxer_t *demuxer,unsigned flags,unsigned hsize)
{
    off_t pos,epos;
    stream_t *s=demuxer->stream;
    if(	flags==ID3V22_ZERO_FLAG ||
	flags==ID3V22_UNSYNCH_FLAG ||
	flags==ID3V22_COMPRESS_FLAG) return 0;
    pos=stream_tell(s);
    epos=pos+hsize;
    while(pos<epos)
    {
	uint32_t id;
	unsigned len;
	unsigned char buf[ID3V22_FRAME_HEADER_SIZE];
	char data[4096];
	stream_read(s,buf,ID3V22_FRAME_HEADER_SIZE);
	id=(buf[2] << 16) + (buf[1] << 8) + buf[0];
	len=(buf[3] << 14) + (buf[4] << 7) + buf[5];
	stream_read(s,data,min(len,4096));
	data[min(len,4096)]=0;
	switch(id)
	{
	    case mmioFOURCC(0,'T','T','1'): if(len>1) demux_info_add(demuxer,INFOT_DESCRIPTION,data+1); break;
	    case mmioFOURCC(0,'T','T','2'): if(len>1) demux_info_add(demuxer,INFOT_NAME,data+1); break;
	    case mmioFOURCC(0,'T','T','3'): if(len>1) demux_info_add(demuxer,INFOT_SUBJECT,data+1); break;
	    case mmioFOURCC(0,'C','O','M'): if(len>4) demux_info_add(demuxer,INFOT_COMMENTS,data+4); break;
	    case mmioFOURCC(0,'T','C','O'): if(len>1) demux_info_add(demuxer,INFOT_GENRE,genres[data[1]]); break;
	    case mmioFOURCC(0,'T','C','R'): if(len>1) demux_info_add(demuxer,INFOT_COPYRIGHT,genres[data[1]]); break;
	    case mmioFOURCC(0,'T','P','1'): if(len>1) demux_info_add(demuxer,INFOT_AUTHOR,data+1); break;
	    case mmioFOURCC(0,'T','A','L'): if(len>1) demux_info_add(demuxer,INFOT_ALBUM,data+1); break;
	    case mmioFOURCC(0,'T','R','K'): if(len>1) demux_info_add(demuxer,INFOT_TRACK,data+1); break;
	    case mmioFOURCC(0,'T','Y','E'): if(len>1) demux_info_add(demuxer,INFOT_DATE,data+1); break;
	    case mmioFOURCC(0,'T','E','N'): if(len>1) demux_info_add(demuxer,INFOT_ENCODER,data+1); break;
	    case mmioFOURCC(0,'T','M','T'): if(len>1) demux_info_add(demuxer,INFOT_SOURCE_MEDIA,data+1); break;
	    case mmioFOURCC(0,'T','F','T'): if(len>1) demux_info_add(demuxer,INFOT_MIME,data+1); break;
	    case mmioFOURCC(0,'P','O','P'): if(len>1) demux_info_add(demuxer,INFOT_RATING,data+1); break;
	    case mmioFOURCC(0,'W','X','X'): if(len>1) demux_info_add(demuxer,INFOT_WWW,data+1); break;
	    case 0: goto end;
	    default: MSG_WARN("Unhandled frame: %3s\n",buf); break;
	}
	pos=stream_tell(s);
    }
    end:
    return 1;
}

/*
 *  ID3 v2.3
 */
/* tag header */
#define ID3V23_UNSYNCH_FLAG               0x80
#define ID3V23_EXT_HEADER_FLAG            0x40
#define ID3V23_EXPERIMENTAL_FLAG          0x20
#define ID3V23_ZERO_FLAG                  0x1F

/* frame header */
#define ID3V23_FRAME_HEADER_SIZE            10
#define ID3V23_FRAME_TAG_PRESERV_FLAG   0x8000
#define ID3V23_FRAME_FILE_PRESERV_FLAG  0x4000
#define ID3V23_FRAME_READ_ONLY_FLAG     0x2000
#define ID3V23_FRAME_COMPRESS_FLAG      0x0080
#define ID3V23_FRAME_ENCRYPT_FLAG       0x0040
#define ID3V23_FRAME_GROUP_ID_FLAG      0x0020
#define ID3V23_FRAME_ZERO_FLAG          0x1F1F

static int read_id3v23_tags(demuxer_t *demuxer,unsigned flags,unsigned hsize)
{
    off_t pos,epos;
    stream_t *s=demuxer->stream;
    if(	flags==ID3V23_ZERO_FLAG ||
	flags==ID3V23_UNSYNCH_FLAG) return 0;
    if( flags==ID3V23_EXT_HEADER_FLAG )
    {
	char buf[4];
	unsigned ehsize;
	stream_read(demuxer->stream,buf,4);
	ehsize=(buf[0] << 21) + (buf[1] << 14) + (buf[2] << 7) + buf[3];
	stream_skip(demuxer->stream,ehsize);
    }
    pos=stream_tell(s);
    epos=pos+hsize;
    while(pos<epos)
    {
	uint32_t id;
	unsigned len;
	unsigned char buf[ID3V23_FRAME_HEADER_SIZE];
	char data[4096];
	stream_read(s,buf,ID3V23_FRAME_HEADER_SIZE);
	id=*((uint32_t *)buf);
	len=(buf[4] << 21) + (buf[5] << 14) + (buf[6] << 7) + buf[7];
	stream_read(s,data,min(len,4096));
	data[min(len,4096)]=0;
	MSG_V("ID3: %4s len %u\n",buf,len);
	switch(id)
	{
	    case mmioFOURCC('T','I','T','1'): if(len>1) demux_info_add(demuxer,INFOT_DESCRIPTION,data+1); break;
	    case mmioFOURCC('T','I','T','2'): if(len>1) demux_info_add(demuxer,INFOT_NAME,data+1); break;
	    case mmioFOURCC('T','I','T','3'): if(len>1) demux_info_add(demuxer,INFOT_SUBJECT,data+1); break;
	    case mmioFOURCC('C','O','M','M'): if(len>4) demux_info_add(demuxer,INFOT_COMMENTS,data+4); break;
	    case mmioFOURCC('T','C','O','N'): if(len>1) demux_info_add(demuxer,INFOT_GENRE,genres[data[1]]); break;
	    case mmioFOURCC('T','P','E','1'): if(len>1) demux_info_add(demuxer,INFOT_AUTHOR,data+1); break;
	    case mmioFOURCC('T','A','L','B'): if(len>1) demux_info_add(demuxer,INFOT_ALBUM,data+1); break;
	    case mmioFOURCC('T','R','C','K'): if(len>1) demux_info_add(demuxer,INFOT_TRACK,data+1); break;
	    case mmioFOURCC('T','Y','E','R'): if(len>1) demux_info_add(demuxer,INFOT_DATE,data+1); break;
	    case mmioFOURCC('T','E','N','C'): if(len>1) demux_info_add(demuxer,INFOT_ENCODER,data+1); break;
	    case mmioFOURCC('T','C','O','P'): if(len>1) demux_info_add(demuxer,INFOT_COPYRIGHT,data+1); break;
	    case mmioFOURCC('T','M','E','D'): if(len>1) demux_info_add(demuxer,INFOT_SOURCE_MEDIA,data+1); break;
	    case mmioFOURCC('T','F','L','T'): if(len>1) demux_info_add(demuxer,INFOT_MIME,data+1); break;
	    case mmioFOURCC('P','O','P','M'): if(len>1) demux_info_add(demuxer,INFOT_RATING,data+1); break;
	    case mmioFOURCC('W','X','X','X'): if(len>1) demux_info_add(demuxer,INFOT_WWW,data+1); break;
	    case 0: goto end;
	    default: MSG_V("Unhandled frame: %4s\n",buf); break;
	}
	pos=stream_tell(s);
    }
    end:
    return 1;
}

/*
 *  ID3 v2.4
 */
/* tag header */
#define ID3V24_UNSYNCH_FLAG               0x80
#define ID3V24_EXT_HEADER_FLAG            0x40
#define ID3V24_EXPERIMENTAL_FLAG          0x20
#define ID3V24_FOOTER_FLAG                0x10
#define ID3V24_ZERO_FLAG                  0x0F

/* frame header */
#define ID3V24_FRAME_HEADER_SIZE            10
#define ID3V24_FRAME_TAG_PRESERV_FLAG   0x4000
#define ID3V24_FRAME_FILE_PRESERV_FLAG  0x2000
#define ID3V24_FRAME_READ_ONLY_FLAG     0x1000
#define ID3V24_FRAME_GROUP_ID_FLAG      0x0040
#define ID3V24_FRAME_COMPRESS_FLAG      0x0008
#define ID3V24_FRAME_ENCRYPT_FLAG       0x0004
#define ID3V24_FRAME_UNSYNCH_FLAG       0x0002
#define ID3V24_FRAME_DATA_LEN_FLAG      0x0001
#define ID3V24_FRAME_ZERO_FLAG          0x8FB0

static int read_id3v24_tags(demuxer_t *demuxer,unsigned flags,unsigned hsize)
{
    off_t pos,epos;
    stream_t *s=demuxer->stream;
    if(	flags==ID3V24_ZERO_FLAG ||
	flags==ID3V24_UNSYNCH_FLAG) return 0;
    if( flags==ID3V24_EXT_HEADER_FLAG )
    {
	char buf[4];
	unsigned ehsize;
	stream_read(demuxer->stream,buf,4);
	ehsize=(buf[0] << 21) + (buf[1] << 14) + (buf[2] << 7) + buf[3];
	stream_skip(demuxer->stream,ehsize);
    }
    pos=stream_tell(s);
    epos=pos+hsize;
    while(pos<epos)
    {
	uint32_t id;
	unsigned len;
	unsigned char buf[ID3V23_FRAME_HEADER_SIZE];
	char data[4096];
	stream_read(s,buf,ID3V23_FRAME_HEADER_SIZE);
	id=*((uint32_t *)buf);
	len=(buf[4] << 21) + (buf[5] << 14) + (buf[6] << 7) + buf[7];
	stream_read(s,data,min(len,4096));
	data[min(len,4096)]=0;
	MSG_V("ID3: %4s len %u\n",buf,len);
	switch(id)
	{
	    /* first byte of data indicates encoding type: 0-ASCII (1-2)-UTF16(LE,BE) 3-UTF8 */
	    case mmioFOURCC('T','I','T','1'): if(len>1) demux_info_add(demuxer,INFOT_DESCRIPTION,data+1); break;
	    case mmioFOURCC('T','I','T','2'): if(len>1) demux_info_add(demuxer,INFOT_NAME,data+1); break;
	    case mmioFOURCC('T','I','T','3'): if(len>1) demux_info_add(demuxer,INFOT_SUBJECT,data+1); break;
	    case mmioFOURCC('C','O','M','M'): if(len>4) demux_info_add(demuxer,INFOT_COMMENTS,data+4); break;
	    case mmioFOURCC('T','C','O','N'): if(len>1) demux_info_add(demuxer,INFOT_GENRE,genres[data[1]]); break;
	    case mmioFOURCC('T','P','E','1'): if(len>1) demux_info_add(demuxer,INFOT_AUTHOR,data+1); break;
	    case mmioFOURCC('T','A','L','B'): if(len>1) demux_info_add(demuxer,INFOT_ALBUM,data+1); break;
	    case mmioFOURCC('T','R','C','K'): if(len>1) demux_info_add(demuxer,INFOT_TRACK,data+1); break;
/*!*/	    case mmioFOURCC('T','D','R','C'): if(len>1) demux_info_add(demuxer,INFOT_DATE,data+1); break;
	    case mmioFOURCC('T','E','N','C'): if(len>1) demux_info_add(demuxer,INFOT_ENCODER,data+1); break;
	    case mmioFOURCC('T','C','O','P'): if(len>1) demux_info_add(demuxer,INFOT_COPYRIGHT,data+1); break;
	    case mmioFOURCC('T','M','E','D'): if(len>1) demux_info_add(demuxer,INFOT_SOURCE_MEDIA,data+1); break;
	    case mmioFOURCC('T','F','L','T'): if(len>1) demux_info_add(demuxer,INFOT_MIME,data+1); break;
	    case mmioFOURCC('P','O','P','M'): if(len>1) demux_info_add(demuxer,INFOT_RATING,data+1); break;
	    case mmioFOURCC('W','X','X','X'): if(len>1) demux_info_add(demuxer,INFOT_WWW,data+1); break;
	    case 0: goto end;
	    default: MSG_V("Unhandled frame: %4s\n",buf); break;
	}
	pos=stream_tell(s);
    }
    end:
    return 1;
}

static int read_id3v2_tags(demuxer_t *demuxer)
{
    char buf[4];
    stream_t* s=demuxer->stream;
    unsigned vers,rev,flags,hsize;
    stream_seek(s,3); /* skip 'ID3' */
    vers=stream_read_char(s);
    rev=stream_read_char(s);
    flags=stream_read_char(s);
    stream_read(s,buf,4);
    hsize=(buf[0] << 21) + (buf[1] << 14) + (buf[2] << 7) + buf[3];
    MSG_V("Found ID3v2.%d.%d flags %d size %d\n",vers,rev,flags,hsize);
    if(vers==2) return read_id3v22_tags(demuxer,flags,hsize);
    else
    if(vers==3) return read_id3v23_tags(demuxer,flags,hsize);
    else
    if(vers==4) return read_id3v24_tags(demuxer,flags,hsize);
    else
    return 1;
}

static int audio_get_raw_id(demuxer_t *demuxer,off_t fptr,unsigned *brate,unsigned *samplerate,unsigned *channels)
{
  int retval=0;
  uint32_t fcc,fcc1,fmt;
  uint8_t *p,b[32];
  stream_t *s;
  *brate=*samplerate=*channels=0;
  s = demuxer->stream;
  stream_seek(s,fptr);
  fcc=fcc1=stream_read_dword(s);
  fcc1=me2be_32(fcc1);
  p = (uint8_t *)&fcc1;
  stream_seek(s,fptr);
  stream_read(s,b,sizeof(b));
  if(p[0] == 'M' && p[1] == 'P' && p[2] == '+' && (p[3] >= 4 && p[3] <= 0x20)) retval = RAW_MUSEPACK;
  else
  if(fcc1 == mmioFOURCC('f','L','a','C')) retval = RAW_FLAC;
  else
  if(fcc1 == mmioFOURCC('.','s','n','d')) retval = RAW_SND_AU;
  else
  if(mp_check_mp3_header(fcc1,&fmt,brate,samplerate,channels))
  {
    if(fmt==1)	retval = RAW_MP1;
    else
    if(fmt==2)	retval = RAW_MP2;
    else	retval = RAW_MP3;
  }
  else
  /* ac3 header check */
  if(ac3_decode_header(b,samplerate,brate,channels)>0) retval = RAW_AC3;
  else
  if(ddca_decode_header(b,samplerate,brate,channels)>0) retval = RAW_DCA;
  else
  if(memcmp(b,"Creative Voice File\x1A",20)==0) retval = RAW_VOC;
  stream_seek(s,fptr);
  return retval;
}

static MPXP_Rc audio_probe(demuxer_t* demuxer)
{
  uint32_t fcc1,fcc2;
  stream_t *s;
  uint8_t *p;
  s = demuxer->stream;
  fcc1=stream_read_dword(s);
  fcc1=me2be_32(fcc1);
  p = (uint8_t *)&fcc1;
  if(fcc1 == mmioFOURCC('R','I','F','F'))
  {
    stream_skip(s,4);
    fcc2 = stream_read_fourcc(s);
    if(fcc2 == mmioFOURCC('W','A','V','E')) return MPXP_Ok;
  }
  else
  if(p[0] == 'I' && p[1] == 'D' && p[2] == '3' && (p[3] >= 2)) return MPXP_Ok;
  else
  if(audio_get_raw_id(demuxer,0,&fcc1,&fcc2,&fcc2)) return MPXP_Ok;
  return MPXP_False;
}

#define FRAMES_FLAG     0x0001
#define BYTES_FLAG      0x0002
#define TOC_FLAG        0x0004
#define VBR_SCALE_FLAG  0x0008
#define FRAMES_AND_BYTES (FRAMES_FLAG | BYTES_FLAG)
#define MPG_MD_MONO     3

static void  Xing_test(stream_t *s,uint8_t *hdr,da_priv_t *priv)
{
    off_t fpos;
    unsigned mpeg1, mode, sr_index;
    unsigned off,head_flags;
    char buf[4];
    const int sr_table[4] = { 44100, 48000, 32000, 99999 };
    priv->scale=-1;
    mpeg1    = (hdr[1]>>3)&1;
    sr_index = (hdr[2]>>2)&3;
    mode     = (hdr[3]>>6)&3;
    if(mpeg1)	off=mode!=MPG_MD_MONO?32:17;
    else	off=mode!=MPG_MD_MONO?17:9;/* mpeg2 */
    fpos = stream_tell(s);
    stream_skip(s,off);
    stream_read(s,buf,4);
    if(memcmp(buf,"Xing",4) == 0 || memcmp(buf,"Info",4) == 0)
    {
	priv->is_xing=1;
	priv->lsf=mpeg1?0:1;
	priv->srate=sr_table[sr_index&0x3];
	head_flags = stream_read_dword(s);
	if(head_flags & FRAMES_FLAG)	priv->nframes=stream_read_dword(s);
	if(head_flags & BYTES_FLAG)		priv->nbytes=stream_read_dword(s);
	if(head_flags & TOC_FLAG)		stream_read(s,priv->toc,100);
	if(head_flags & VBR_SCALE_FLAG)	priv->scale = stream_read_dword(s);
	MSG_DBG2("Found Xing VBR header: flags=%08X nframes=%u nbytes=%u scale=%i srate=%u\n"
	,head_flags,priv->nframes,priv->nbytes,priv->scale,priv->srate);
	stream_seek(s,fpos);
    }
    else stream_seek(s,fpos);
}

extern demuxer_driver_t demux_audio;

static demuxer_t* audio_open(demuxer_t* demuxer) {
  stream_t *s;
  sh_audio_t* sh_audio;
  uint8_t hdr[HDR_SIZE];
  uint32_t fcc,fcc2;
  int frmt = 0, n = 0, pos = 0, step;
  unsigned mp3_brate,mp3_samplerate,mp3_channels;
  off_t st_pos = 0;
  da_priv_t* priv;
  const unsigned char *pfcc;
#ifdef MP_DEBUG
  assert(demuxer != NULL);
  assert(demuxer->stream != NULL);
#endif

  priv = (da_priv_t*)mp_mallocz(sizeof(da_priv_t));
  s = demuxer->stream;
  stream_reset(s);
  stream_seek(s,s->start_pos);
  while(n < 5 && !stream_eof(s))
  {
    st_pos = stream_tell(s);
    step = 1;

    if(pos < HDR_SIZE) {
      stream_read(s,&hdr[pos],HDR_SIZE-pos);
      pos = HDR_SIZE;
    }

    fcc = le2me_32(*(uint32_t *)hdr);
    pfcc = (const unsigned char *)&fcc;
    MSG_DBG2("AUDIO initial fcc=%c%c%c%c\n",pfcc[0],pfcc[1],pfcc[2],pfcc[3]);
    if(fcc == mmioFOURCC('R','I','F','F'))
    {
	MSG_DBG2("Found RIFF\n");
	stream_skip(s,4);
	if(stream_eof(s)) break;
	stream_read(s,hdr,4);
	if(stream_eof(s)) break;
	fcc2 = le2me_32(*(uint32_t *)hdr);
	pfcc= (const unsigned char *)&fcc2;
	MSG_DBG2("RIFF fcc=%c%c%c%c\n",pfcc[0],pfcc[1],pfcc[2],pfcc[3]);
	if(fcc2!=mmioFOURCC('W','A','V','E')) stream_skip(s,-8);
	else
	{
	    /* We found wav header. Now we should find 'fmt '*/
	    off_t fpos;
	    fpos=stream_tell(s);
	    MSG_DBG2("RIFF WAVE found. Start detection from %llu\n",fpos);
	    step = 4;
	    while(1)
	    {
		unsigned chunk_len;
		fcc=stream_read_fourcc(s);
		pfcc= (const unsigned char *)&fcc;
		MSG_DBG2("fmt fcc=%c%c%c%c\n",pfcc[0],pfcc[1],pfcc[2],pfcc[3]);
		if(fcc==mmioFOURCC('f','m','t',' '))
		{
		    MSG_DBG2("RIFF WAVE fmt found\n");
		    frmt = RAW_WAV;
		    break;
		}
		if(stream_eof(s)) break;
		chunk_len=stream_read_dword_le(s);
		stream_skip(s,chunk_len);
	    }
	    MSG_DBG2("Restore stream pos %llu\n",fpos);
	    stream_seek(s,fpos);
	    if(frmt==RAW_WAV) break;
	}
    }
    else
    if( hdr[0] == 'I' && hdr[1] == 'D' && hdr[2] == '3' && (hdr[3] >= 2))
    {
	unsigned len,fmt;
	stream_skip(s,2);
	stream_read(s,hdr,4);
	len = (hdr[0]<<21) | (hdr[1]<<14) | (hdr[2]<<7) | hdr[3];
	read_id3v2_tags(demuxer);
	stream_seek(s,len+10);
	find_next_mp3_hdr(demuxer,hdr);
	Xing_test(s,hdr,priv);
	mp_decode_mp3_header(hdr,&fmt,&mp3_brate,&mp3_samplerate,&mp3_channels);
	step = 4;
	frmt=RAW_MP3;
    }
    else
    if( hdr[0] == 'f' && hdr[1] == 'L' && hdr[2] == 'a' && hdr[3] == 'C' )
    {
	frmt=RAW_FLAC;
	break;
    }
    else
    if( hdr[0] == '.' && hdr[1] == 's' && hdr[2] == 'n' && hdr[3] == 'd' )
    {
	frmt=RAW_SND_AU;
	break;
    }
    else
    if( hdr[0] == 'M' && hdr[1] == 'P' && hdr[2] == '+' && (hdr[3] >= 4 && hdr[3] <= 0x20))
    {
	frmt=RAW_MUSEPACK;
	break;
    }
    else
    {
	unsigned fmt;
	uint8_t b[21];
	MSG_DBG2("initial mp3_header: 0x%08X at %lu\n",*(uint32_t *)hdr,st_pos);
	if((n = mp_decode_mp3_header(hdr,&fmt,&mp3_brate,&mp3_samplerate,&mp3_channels)) > 0)
	{
	    /* A Xing header may be present in stream as the first frame of an mp3 bitstream */
	    Xing_test(s,hdr,priv);
	    demuxer->movi_start = st_pos;
	    frmt = fmt;
	    break;
	}
	memcpy(b,hdr,HDR_SIZE);
	stream_read(s,&b[HDR_SIZE],12-HDR_SIZE);
	if((n = ac3_decode_header(b,&fmt,&fmt,&fmt)) > 0)
	{
	    demuxer->movi_start = st_pos;
	    frmt = RAW_AC3;
	    break;
	}
	if((n = ddca_decode_header(b,&fmt,&fmt,&fmt)) > 0)
	{
	    demuxer->movi_start = st_pos;
	    frmt = RAW_DCA;
	    break;
	}
	if(memcmp(b,"Creative Voice File\x1A",20)==0)
	{
	    frmt = RAW_VOC;
	    break;
	}
    }
    /* Add here some other audio format detection */
    if(step < HDR_SIZE) memmove(hdr,&hdr[step],HDR_SIZE-step);
    pos -= step;
  }

  if(!frmt)
  {
    MSG_ERR("Can't detect audio format\n");
    return NULL;
  }
  sh_audio = new_sh_audio(demuxer,0);
  MSG_DBG2("mp3_header off: st_pos=%lu n=%lu HDR_SIZE=%u\n",st_pos,n,HDR_SIZE);
  switch(frmt) {
  case RAW_FLAC:
  {
    uint8_t chunk[4];
    uint32_t block_size;
	sh_audio->wtag = mmioFOURCC('f', 'L', 'a', 'C');
	/* loop through the metadata blocks; use a do-while construct since there
	* will always be 1 metadata block */
	do {
	    if(stream_read(s,chunk,4)!=4) return NULL;
	    block_size=(chunk[1]<<16)|(chunk[2]<<8)|chunk[3];
	    switch (chunk[0] & 0x7F) {
		/* STREAMINFO */
		case 0:
		{
		    char sinfo[block_size];
		    WAVEFORMATEX* w;
		    unsigned long long int total_samples;
		    sh_audio->wf = w = (WAVEFORMATEX*)mp_mallocz(sizeof(WAVEFORMATEX));
		    MSG_V("STREAMINFO metadata\n");
		    if (block_size != 34) {
			MSG_V("expected STREAMINFO chunk of %d bytes\n",block_size);
			return 0;
		    }
		    if(stream_read(s,sinfo,block_size)!=(int)block_size) return NULL;
		    sh_audio->rate=be2me_32(*(uint32_t *)&sinfo[10]);
		    sh_audio->nch=w->nChannels=((sh_audio->rate>>9)&0x07)+1;
		    w->wBitsPerSample=((sh_audio->rate>>4)&0x1F)+1;
		    sh_audio->afmt=bps2afmt((w->wBitsPerSample+7)/8);
		    sh_audio->rate>>=12;
		    w->nSamplesPerSec=sh_audio->rate;
		    w->nAvgBytesPerSec = sh_audio->rate*afmt2bps(sh_audio->afmt)*sh_audio->nch;
		    w->nBlockAlign = sh_audio->nch*afmt2bps(sh_audio->afmt);
		    w->wBitsPerSample = 8*afmt2bps(sh_audio->afmt);
		    w->cbSize = 0;
		    total_samples = be2me_64(*(uint64_t *)&sinfo[10]) & 0x0FFFFFFFFFLL;  /* 36 bits */
		    MSG_V("Total fLaC samples: %llu (%llu secs)\n",total_samples,total_samples/afmt2bps(sh_audio->afmt));
		    /*many streams have incorrectly computed this field. So ignore it for now! */
		    demuxer->movi_end=0;//total_samples*sh_audio->samplesize;
		    break;
		}
		/* VORBIS_COMMENT */
		case 4:
		/* CUESHEET */
		case 5:
		/* 6-127 are presently reserved */
		default:
		/* PADDING */
		case 1:
		/* SEEKTABLE */
		case 3:
		/* APPLICATION */
		case 2:
		    MSG_V("metadata %i size %u\n",chunk[0] & 0x7F,block_size);
		    stream_skip(s,block_size);
		    break;
	    }
	} while ((chunk[0] & 0x80) == 0);
	/* We have reached 1st FRAME_HEADER here. */
	demuxer->movi_start = 0;//stream_tell(s); (ffmpeg.flac requires STREAM_HEADER to be proceed too!!!)
	demuxer->movi_end += demuxer->movi_start;
	demux_audio.name="FLAC parser";
	break;
  }
  case RAW_SND_AU: {
	unsigned hsize,dsize;
	uint32_t id;
	WAVEFORMATEX* w;
	sh_audio->wf = w = (WAVEFORMATEX*)mp_malloc(sizeof(WAVEFORMATEX));
	hsize=stream_read_dword(s);
	dsize=stream_read_dword(s);
	id = stream_read_dword(s);
	sh_audio->afmt=bps2afmt(2);
	if(id == 1) id = WAVE_FORMAT_MULAW;
	else
	if(id == 27) id=WAVE_FORMAT_ALAW;
	else
	if(id == 3) id=0x1;
	w->wFormatTag = sh_audio->wtag = id;
	/* Trickly mplayerxp will threat 'raw ' as big-endian */
	if(id == 0x1) sh_audio->wtag=mmioFOURCC('r','a','w',' ');
	w->nSamplesPerSec = sh_audio->rate = stream_read_dword(s);
	w->nChannels = sh_audio->nch = stream_read_dword(s);
	w->nAvgBytesPerSec = sh_audio->rate*afmt2bps(sh_audio->afmt)*sh_audio->nch;
	w->nBlockAlign = sh_audio->nch*afmt2bps(sh_audio->afmt);
	w->wBitsPerSample = 8*afmt2bps(sh_audio->afmt);
	w->cbSize = 0;
	demuxer->movi_start = demuxer->stream->start_pos+hsize;
	demuxer->movi_end = demuxer->movi_start+hsize+dsize;
	demuxer->movi_length = (demuxer->movi_end-demuxer->movi_start)/w->nAvgBytesPerSec;
	demux_audio.name="Sun Audio (AU) and NeXT parser";
    }
	break;
  case RAW_MP1:
  case RAW_MP2:
    sh_audio->wtag = 0x50;
    sh_audio->i_bps=mp3_brate;
    sh_audio->rate=mp3_samplerate;
    sh_audio->nch=mp3_channels;
    if(!read_mp3v1_tags(demuxer,hdr,pos)) return 0; /* id3v1 may coexist with id3v2 */
    break;
  case RAW_MP3:
    sh_audio->wtag = 0x55;
    sh_audio->i_bps=mp3_brate;
    sh_audio->rate=mp3_samplerate;
    sh_audio->nch=mp3_channels;
    if(!read_mp3v1_tags(demuxer,hdr,pos)) return 0; /* id3v1 may coexist with id3v2 */
    break;
  case RAW_AC3:
    sh_audio->wtag = 0x2000;
    if(!read_ac3_tags(demuxer,hdr,pos,&sh_audio->i_bps,&sh_audio->rate,&sh_audio->nch)) return 0;
    break;
  case RAW_DCA:
    sh_audio->wtag = 0x2001;
    if(!read_ddca_tags(demuxer,hdr,pos,&sh_audio->i_bps,&sh_audio->rate,&sh_audio->nch)) return 0;
    sh_audio->i_bps/=8;
    break;
  case RAW_MUSEPACK:
  {
    const unsigned freqs[4]={ 44100, 48000, 37800, 32000 };
    uint32_t frames;
    unsigned char bt;
    sh_audio->wtag = mmioFOURCC('M','P','C',' ');
    stream_seek(s,4);
    frames = stream_read_dword(s);
    stream_skip(s,2);
    bt=stream_read_char(s);
    sh_audio->wf = (WAVEFORMATEX *)mp_malloc(sizeof(WAVEFORMATEX));
    sh_audio->wf->wFormatTag = sh_audio->wtag;
    sh_audio->wf->nChannels = 2;
    sh_audio->wf->nSamplesPerSec = freqs[bt & 3];
    sh_audio->wf->nBlockAlign = 32 * 36;
    sh_audio->wf->wBitsPerSample = 16;
    sh_audio->i_bps = sh_audio->wf->nAvgBytesPerSec;
    sh_audio->rate = sh_audio->wf->nSamplesPerSec;
    sh_audio->audio.dwSampleSize = 0;
    sh_audio->audio.dwScale = 32 * 36;
    sh_audio->audio.dwRate = sh_audio->rate;
    priv->pts_per_packet = (32 * 36) / (float)sh_audio->wf->nSamplesPerSec;
    priv->dword = 0;
    priv->pos = 32; // empty bit buffer
    priv->length = 1152 * frames / (float)sh_audio->wf->nSamplesPerSec;
    demuxer->movi_start = 24; /* skip header */
    demuxer->movi_end = s->end_pos;
    if (demuxer->movi_end > demuxer->movi_start && priv->length > 1)
      sh_audio->wf->nAvgBytesPerSec = (demuxer->movi_end - demuxer->movi_start) / priv->length;
    else
      sh_audio->wf->nAvgBytesPerSec = 32 * 1024; // dummy to make mplayerxp not hang
    sh_audio->wf->cbSize = 24;
    break;
  }
  case RAW_VOC:
  {
    char chunk[4];
    unsigned size;
    WAVEFORMATEX* w;
    stream_seek(s,0x14);
    stream_read(s,chunk,2);
    size=le2me_16(*(uint16_t *)&chunk[0]);
    stream_seek(s,size);
    stream_read(s,chunk,4);
    if(chunk[0]!=0x01) { MSG_V("VOC unknown block type %02X\n",chunk[0]); return NULL; }
    size=chunk[1]|(chunk[2]<<8)|(chunk[3]<<16);
    sh_audio->wtag = 0x01; /* PCM */
    stream_read(s,chunk,2);
    if(chunk[1]!=0) { MSG_V("VOC unknown compression type %02X\n",chunk[1]); return NULL; }
    demuxer->movi_start=stream_tell(s);
    demuxer->movi_end=demuxer->movi_start+size;
    sh_audio->rate=256-(1000000/chunk[0]);
    sh_audio->nch=1;
    sh_audio->afmt=bps2afmt(1);
    sh_audio->wf = w = (WAVEFORMATEX*)mp_malloc(sizeof(WAVEFORMATEX));
    w->wFormatTag = sh_audio->wtag;
    w->nChannels = sh_audio->nch;
    w->nSamplesPerSec = sh_audio->rate;
    w->nAvgBytesPerSec = sh_audio->rate*afmt2bps(sh_audio->afmt)*sh_audio->nch;
    w->nBlockAlign = 1024;
    w->wBitsPerSample = (afmt2bps(sh_audio->afmt)+7)/8;
    w->cbSize = 0;
    break;
  }
  case RAW_WAV: {
    off_t fpos,data_off=-1;
    unsigned int chunk_type;
    unsigned int chunk_size;
    WAVEFORMATEX* w;
    int l;
    sh_audio->wf = w = (WAVEFORMATEX*)mp_malloc(sizeof(WAVEFORMATEX));
    do
    {
      chunk_type = stream_read_fourcc(s);
      chunk_size = stream_read_dword_le(s);
      fpos=stream_tell(s);
      switch(chunk_type)
      {
	case mmioFOURCC('f','m','t',' '):
		{
		    l = chunk_size;
		    MSG_DBG2("Found %u bytes WAVEFORMATEX\n",l);
		    if(l < 16) {
			MSG_ERR("Bad wav header length : too short !!!\n");
			free_sh_audio(sh_audio);
			return NULL;
		    }
		    w->wFormatTag = sh_audio->wtag = stream_read_word_le(s);
		    w->nChannels = sh_audio->nch = stream_read_word_le(s);
		    w->nSamplesPerSec = sh_audio->rate = stream_read_dword_le(s);
		    w->nAvgBytesPerSec = stream_read_dword_le(s);
		    w->nBlockAlign = stream_read_word_le(s);
		    w->wBitsPerSample =  stream_read_word_le(s);
		    sh_audio->afmt = bps2afmt((w->wBitsPerSample+7)/8);
		    w->cbSize = 0;
		    l -= 16;
		    if(l) stream_skip(s,l);
		}
		break;
	case mmioFOURCC('d', 'a', 't', 'a'):
		MSG_DBG2("Found data chunk at %llu\n",fpos);
		data_off=fpos;
		stream_skip(s,chunk_size);
		break;
	case mmioFOURCC('l', 'i', 's', 't'):
	    {
		uint32_t cfcc;
		MSG_DBG2("RIFF 'list' found\n");
		cfcc=stream_read_fourcc(s);
		if(cfcc!=mmioFOURCC('a', 'd', 't', 'l')) { stream_seek(s,fpos); break; }
		do
		{
		    unsigned int subchunk_type;
		    unsigned int subchunk_size;
		    unsigned int subchunk_id;
		    unsigned slen,rlen;
		    char note[256];
		    MSG_DBG2("RIFF 'list' accepted\n");
		    subchunk_type = stream_read_fourcc(s);
		    subchunk_size = stream_read_dword_le(s);
		    subchunk_id = stream_read_dword_le(s);
		    if(subchunk_type==mmioFOURCC('l','a','b','l'))
		    {
			slen=subchunk_size-4;
			rlen=min(sizeof(note),slen);
			stream_read(s,note,rlen);
			note[rlen]=0;
			if(slen>rlen) stream_skip(s,slen-rlen);
			demux_info_add(demuxer,INFOT_NAME,note);
			MSG_DBG2("RIFF 'labl' %u %s accepted\n",slen,note);
		    }
		    else
		    if(subchunk_type==mmioFOURCC('n','o','t','e'))
		    {
			slen=subchunk_size-4;
			rlen=min(sizeof(note),slen);
			stream_read(s,note,rlen);
			note[rlen]=0;
			if(slen>rlen) stream_skip(s,slen-rlen);
			demux_info_add(demuxer,INFOT_COMMENTS,note);
			MSG_DBG2("RIFF 'note' %u %s accepted\n",slen,note);
		    }
		    else stream_skip(s,subchunk_size);
		}while(stream_tell(s)<fpos+chunk_size);
		stream_seek(s,fpos+chunk_size);
	    }
	    break;
	default:
	    stream_skip(s, chunk_size);
	    pfcc=(unsigned char *)&chunk_type;
	    MSG_DBG2("RIFF unhandled '%c%c%c%c' chunk skipped\n",pfcc[0],pfcc[1],pfcc[2],pfcc[3]);
	    break;
      }
    } while (!stream_eof(s));
    if(data_off==-1)
    {
	MSG_ERR("RIFF WAVE - no 'data' chunk found\n");
	return NULL;
    }
    stream_seek(s,data_off);
    demuxer->movi_start = stream_tell(s);
    if(w->wFormatTag==0x01) /* PCM */
    {
	int raw_id;
	unsigned brate,samplerate,channels;
	if((raw_id=audio_get_raw_id(demuxer,data_off,&brate,&samplerate,&channels))!=0) {
	switch(raw_id)
	{
	    case RAW_MP1:
	    case RAW_MP2: sh_audio->wtag=w->wFormatTag=0x50; break;
	    case RAW_MP3: sh_audio->wtag=w->wFormatTag=0x55; break;
	    case RAW_FLAC:sh_audio->wtag = mmioFOURCC('f', 'L', 'a', 'C'); break;
	    case RAW_SND_AU: {
		unsigned hsize,dsize;
		uint32_t id;
		hsize=stream_read_dword(s);
		dsize=stream_read_dword(s);
		id = stream_read_dword(s);
		if(id == 1) id = WAVE_FORMAT_MULAW;
		else
		if(id == 27) id=WAVE_FORMAT_ALAW;
		else
		if(id == 3) id=0x1;
		w->wFormatTag = sh_audio->wtag = id;
		/* Trickly mplayerxp will threat 'raw ' as big-endian */
		if(id == 0x1) sh_audio->wtag=mmioFOURCC('r','a','w',' ');
		break;
	    }
	    case RAW_AC3:   sh_audio->wtag=w->wFormatTag=0x2000; break;
	    case RAW_DCA:   sh_audio->wtag=w->wFormatTag=0x2001;
			    if(brate) brate/=8;
			    break;
	    default:
	    case RAW_VOC: break;
	    case RAW_MUSEPACK: sh_audio->wtag = mmioFOURCC('M','P','+',' '); break;
	}
	if(brate)	sh_audio->i_bps=brate;
	if(channels)	w->nChannels = sh_audio->nch = channels;
	if(samplerate)	w->nSamplesPerSec = sh_audio->rate = samplerate;
	}
    }
    stream_seek(s,data_off);
    /* id3v1 tags may exist within WAV */
    if(sh_audio->wtag==0x50 || sh_audio->wtag==0x55)
    {
	stream_seek(s,data_off);
	stream_read(s,hdr,4);
	MSG_DBG2("Trying id3v1 at %llX\n",data_off);
	if(!read_mp3v1_tags(demuxer,hdr,data_off)) demuxer->movi_end = s->end_pos;
    }
    else
	demuxer->movi_end = s->end_pos;
    stream_seek(s,data_off);
  } break;
  }

  priv->frmt = frmt;
  priv->last_pts = -1;
  demuxer->priv = priv;
  demuxer->audio->id = 0;
  demuxer->audio->sh = sh_audio;
  sh_audio->ds = demuxer->audio;

  if(priv->is_xing)
  {
    float framesize;
    float bitrate;
    framesize=(float)(demuxer->movi_end-demuxer->movi_start)/priv->nframes;
    bitrate=(framesize*(float)(priv->srate<<priv->lsf))/144000.;
    sh_audio->i_bps=(unsigned)(bitrate*(1000./8.));
    demuxer->movi_length= (unsigned)((float)(demuxer->movi_end-demuxer->movi_start)/(float)sh_audio->i_bps);
    MSG_DBG2("stream length by xing header=%u secs\n",demuxer->movi_length);
    demux_audio.name = "Xing's VBR MP3 parser";
  }
  MSG_V("demux_audio: audio data 0x%llX - 0x%llX  \n",demuxer->movi_start,demuxer->movi_end);
  if(stream_tell(s) != demuxer->movi_start)
    stream_seek(s,demuxer->movi_start);
  if(mp_conf.verbose && sh_audio->wf) print_wave_header(sh_audio->wf,sizeof(WAVEFORMATEX));
  if(demuxer->movi_length==UINT_MAX && sh_audio->i_bps)
    demuxer->movi_length=(unsigned)(((float)demuxer->movi_end-(float)demuxer->movi_start)/(float)sh_audio->i_bps);
    check_pin("demuxer",demuxer->pin,DEMUX_PIN);
  return demuxer;
}

static uint32_t mpc_get_bits(da_priv_t* priv, stream_t* s, int bits) {
  uint32_t out = priv->dword;
  uint32_t mask = (1 << bits) - 1;
  priv->pos += bits;
  if (priv->pos < 32) {
    out >>= (32 - priv->pos);
  }
  else {
    stream_read(s, (any_t*)&priv->dword, 4);
    priv->dword = le2me_32(priv->dword);
    priv->pos -= 32;
    if (priv->pos) {
      out <<= priv->pos;
      out |= priv->dword >> (32 - priv->pos);
    }
  }
  return out & mask;
}

static int audio_demux(demuxer_t *demuxer,demux_stream_t *ds) {
  sh_audio_t* sh_audio;
  demuxer_t* demux;
  da_priv_t* priv;
  stream_t* s;
  int frmt,seof;
#ifdef MP_DEBUG
  assert(ds != NULL);
  assert(ds->sh != NULL);
  assert(ds->demuxer != NULL);
#endif
  sh_audio = demuxer->audio->sh;
  demux = demuxer;
  priv = demux->priv;
  s = demux->stream;

  seof=stream_eof(s);
  if(seof || (demux->movi_end && stream_tell(s) >= demux->movi_end)) {
    MSG_DBG2("audio_demux: EOF due: %s\n",
	    seof?"stream_eof(s)":"stream_tell(s) >= demux->movi_end");
    if(!seof) {
	MSG_DBG2("audio_demux: stream_pos=%llu movi_end=%llu\n",
		stream_tell(s),
		demux->movi_end);
    }
    return 0;
  }
  frmt=priv->frmt;
  if(frmt==RAW_WAV)
  {
    switch(sh_audio->wtag)
    {
	case 0x50:
	case 0x55: frmt=RAW_MP3; break;
	case 0x2000: frmt=RAW_AC3; break;
	case 0x2001: frmt=RAW_DCA; break;
	default: break;
    }
  }
  switch(frmt) {
  case RAW_MP1:
  case RAW_MP2:
  case RAW_MP3:
    while(!stream_eof(s) || (demux->movi_end && stream_tell(s) >= demux->movi_end) ) {
      uint8_t hdr[4];
      int len;
      stream_read(s,hdr,4);
      MSG_DBG2("audio_fillbuffer\n");
      len = mp_decode_mp3_header(hdr,NULL,NULL,NULL,NULL);
      if(stream_eof(s)) return 0; /* workaround for dead-lock (skip(-3)) below */
      if(len < 0) {
	stream_skip(s,-3);
      } else {
	demux_packet_t* dp;
	if(stream_eof(s) || (demux->movi_end && stream_tell(s) >= demux->movi_end) )
	  return 0;
	if(len>4)
	{
	    dp = new_demux_packet(len);
	    memcpy(dp->buffer,hdr,4);
	    len=stream_read(s,dp->buffer + 4,len-4);
	    resize_demux_packet(dp,len+4);
	    priv->last_pts = priv->last_pts < 0 ? 0 : priv->last_pts + len/(float)sh_audio->i_bps;
	    dp->pts = priv->last_pts - (ds_tell_pts(demux->audio)-sh_audio->a_in_buffer_len)/(float)sh_audio->i_bps;
	    dp->flags=DP_NONKEYFRAME;
	    ds_add_packet(ds,dp);
	}
	else stream_skip(s,len);
	return 1;
      }
    } break;
  case RAW_AC3:
    while(!stream_eof(s) || (demux->movi_end && stream_tell(s) >= demux->movi_end) ) {
      uint8_t hdr[8];
      int len;
      unsigned dummy;
      stream_read(s,hdr,8);
      len = ac3_decode_header(hdr,&dummy,&dummy,&dummy);
      MSG_DBG2("audio_fillbuffer %u bytes\n",len);
      if(stream_eof(s)) return 0; /* workaround for dead-lock (skip(-7)) below */
      if(len < 0) {
	stream_skip(s,-7);
      } else {
	demux_packet_t* dp;
	if(stream_eof(s) || (demux->movi_end && stream_tell(s) >= demux->movi_end) )
	  return 0;
	if(len>8)
	{
	    dp = new_demux_packet(len);
	    memcpy(dp->buffer,hdr,8);
	    len=stream_read(s,dp->buffer+8,len-8);
	    resize_demux_packet(dp,len+8);
	    priv->last_pts = priv->last_pts < 0 ? 0 : priv->last_pts + len/(float)sh_audio->i_bps;
	    dp->pts = priv->last_pts - (ds_tell_pts(demux->audio)-sh_audio->a_in_buffer_len)/(float)sh_audio->i_bps;
	    dp->flags=DP_NONKEYFRAME;
	    ds_add_packet(ds,dp);
	}
	else stream_skip(s,len);
	return 1;
      }
    } break;
  case RAW_DCA:
    while(!stream_eof(s) || (demux->movi_end && stream_tell(s) >= demux->movi_end) ) {
      uint8_t hdr[16];
      int len;
      unsigned dummy;
      stream_read(s,hdr,16);
      len = ddca_decode_header(hdr,&dummy,&dummy,&dummy);
      MSG_DBG2("audio_fillbuffer %u bytes\n",len);
      if(stream_eof(s)) return 0; /* workaround for dead-lock (skip(-7)) below */
      if(len < 0) {
	stream_skip(s,-15);
      } else {
	demux_packet_t* dp;
	if(stream_eof(s) || (demux->movi_end && stream_tell(s) >= demux->movi_end) )
	  return 0;
	if(len>16)
	{
	    dp = new_demux_packet(len);
	    memcpy(dp->buffer,hdr,16);
	    len=stream_read(s,dp->buffer+16,len-16);
	    resize_demux_packet(dp,len+16);
	    priv->last_pts = priv->last_pts < 0 ? 0 : priv->last_pts + len/(float)sh_audio->i_bps;
	    dp->pts = priv->last_pts - (ds_tell_pts(demux->audio)-sh_audio->a_in_buffer_len)/(float)sh_audio->i_bps;
	    dp->flags=DP_NONKEYFRAME;
	    ds_add_packet(ds,dp);
	}
	else stream_skip(s,len);
	return 1;
      }
    } break;
  case RAW_FLAC :
  case RAW_SND_AU:
  case RAW_WAV : {
    int l = sh_audio->wf->nAvgBytesPerSec;
    demux_packet_t*  dp = new_demux_packet(l);
    l=stream_read(s,dp->buffer,l);
    resize_demux_packet(dp, l);
    priv->last_pts = priv->last_pts < 0 ? 0 : priv->last_pts + l/(float)sh_audio->i_bps;
    dp->pts = priv->last_pts - (ds_tell_pts(demux->audio)-sh_audio->a_in_buffer_len)/(float)sh_audio->i_bps;
    dp->flags=DP_NONKEYFRAME;
    ds_add_packet(ds,dp);
    return 1;
  }
  case RAW_VOC : {
    int l = 65536;
    demux_packet_t*  dp = new_demux_packet(l);
    l=stream_read(s,dp->buffer,l);
    resize_demux_packet(dp, l);
    priv->last_pts = priv->last_pts < 0 ? 0 : priv->last_pts + l/(float)sh_audio->i_bps;
    dp->pts = priv->last_pts - (ds_tell_pts(demux->audio)-sh_audio->a_in_buffer_len)/(float)sh_audio->i_bps;
    dp->flags=DP_NONKEYFRAME;
    ds_add_packet(ds,dp);
    return 1;
  }
  case RAW_MUSEPACK: {
    int l;
    int bit_len;
    demux_packet_t* dp;
    priv = demux->priv;
    s = demux->stream;
    sh_audio = ds->sh;

    if (s->eof) return 0;

    bit_len = mpc_get_bits(priv, s, 20);
    dp = new_demux_packet((bit_len + 7) / 8);
    for (l = 0; l < (bit_len / 8); l++)
	dp->buffer[l] = mpc_get_bits(priv, s, 8);
    bit_len %= 8;
    if (bit_len)
	dp->buffer[l] = mpc_get_bits(priv, s, bit_len) << (8 - bit_len);
    if (priv->last_pts < 0)
	priv->last_pts = 0;
    else
	priv->last_pts += priv->pts_per_packet;
    dp->pts = priv->last_pts - (ds_tell_pts(demux->audio) -
	      sh_audio->a_in_buffer_len)/(float)sh_audio->i_bps;
    dp->flags=DP_NONKEYFRAME;
    ds_add_packet(ds, dp);
    return 1;
  }
  default:
    MSG_ERR("Audio demuxer : unknown format %d\n",priv->frmt);
  }
  return 0;
}

static void high_res_mp3_seek(demuxer_t *demuxer,float _time) {
  uint8_t hdr[4];
  int len,nf;
  da_priv_t* priv = demuxer->priv;
  sh_audio_t* sh = (sh_audio_t*)demuxer->audio->sh;

  nf = _time*sh->rate/1152;
  while(nf > 0) {
    stream_read(demuxer->stream,hdr,4);
    MSG_DBG2("high_res_mp3_seek\n");
    len = mp_decode_mp3_header(hdr,NULL,NULL,NULL,NULL);
    if(len < 0) {
      stream_skip(demuxer->stream,-3);
      continue;
    }
    stream_skip(demuxer->stream,len-4);
    priv->last_pts += 1152/(float)sh->rate;
    nf--;
  }
}

static void high_res_ac3_seek(demuxer_t *demuxer,float _time) {
  uint8_t hdr[8];
  int len,nf;
  unsigned tmp;
  da_priv_t* priv = demuxer->priv;
  sh_audio_t* sh = (sh_audio_t*)demuxer->audio->sh;

  nf = _time*sh->rate/1152;
  while(nf > 0) {
    stream_read(demuxer->stream,hdr,8);
    MSG_DBG2("high_res_mp3_seek\n");
    len = ac3_decode_header(hdr,&tmp,&tmp,&tmp);
    if(len < 0) {
      stream_skip(demuxer->stream,-7);
      continue;
    }
    stream_skip(demuxer->stream,len-8);
    priv->last_pts += 1152/(float)sh->rate;
    nf--;
  }
}

static void high_res_ddca_seek(demuxer_t *demuxer,float _time) {
  uint8_t hdr[12];
  int len,nf;
  unsigned tmp;
  da_priv_t* priv = demuxer->priv;
  sh_audio_t* sh = (sh_audio_t*)demuxer->audio->sh;

  nf = _time*sh->rate/1152;
  while(nf > 0) {
    stream_read(demuxer->stream,hdr,12);
    MSG_DBG2("high_res_ddca_seek\n");
    len = ddca_decode_header(hdr,&tmp,&tmp,&tmp);
    if(len < 0) {
      stream_skip(demuxer->stream,-11);
      continue;
    }
    stream_skip(demuxer->stream,len-12);
    priv->last_pts += 1152/(float)sh->rate;
    nf--;
  }
}

static void audio_seek(demuxer_t *demuxer,const seek_args_t* seeka){
  sh_audio_t* sh_audio;
  stream_t* s;
  int base,pos,frmt;
  float len;
  da_priv_t* priv;

  if(!(sh_audio = demuxer->audio->sh)) return;
  s = demuxer->stream;
  priv = demuxer->priv;
  if(priv->frmt==RAW_MUSEPACK)
  {
    priv = demuxer->priv;
    s = demuxer->stream;
    float target = seeka->secs;
    if (seeka->flags & DEMUX_SEEK_PERCENTS) target *= priv->length;
    if (!(seeka->flags & DEMUX_SEEK_SET)) target += priv->last_pts;
    if (target < priv->last_pts) {
	stream_seek(s, demuxer->movi_start);
	priv->pos = 32; // empty bit buffer
	mpc_get_bits(priv, s, 8); // discard first 8 bits
	priv->last_pts = 0;
    }
    while (target > priv->last_pts) {
	int bit_len = mpc_get_bits(priv, s, 20);
	if (bit_len > 32) {
	    stream_skip(s, bit_len / 32 * 4 - 4);
	    mpc_get_bits(priv, s, 32); // make sure dword is reloaded
	}
	mpc_get_bits(priv, s, bit_len % 32);
	priv->last_pts += priv->pts_per_packet;
	if (s->eof) break;
    }
    return;
  }

  if(priv->is_xing)
  {
    unsigned percents,cpercents,npercents;
    off_t newpos,spos;
    percents = (unsigned)(seeka->secs*100.)/(float)demuxer->movi_length;
    spos=stream_tell(demuxer->stream);
    cpercents=(unsigned)((float)spos*100./(float)priv->nbytes);
    npercents=(seeka->flags&DEMUX_SEEK_SET)?percents:cpercents+percents;
    if(npercents>100) npercents=100;
    newpos=demuxer->movi_start+(off_t)(((float)priv->toc[npercents]/256.0)*priv->nbytes);
    MSG_DBG2("xing seeking: secs=%f prcnts=%u cprcnts=%u spos=%llu newpos=%llu\n",seeka->secs,npercents,cpercents,spos,newpos);
    stream_seek(demuxer->stream,newpos);
    priv->last_pts=(((float)demuxer->movi_length*npercents)/100.)*1000.;
    return;
  }
  if((priv->frmt == RAW_MP3 || priv->frmt == RAW_MP2 || priv->frmt == RAW_MP1) && hr_mp3_seek && !(seeka->flags & DEMUX_SEEK_PERCENTS)) {
    len = (seeka->flags & DEMUX_SEEK_SET) ? seeka->secs - priv->last_pts : seeka->secs;
    if(len < 0) {
      stream_seek(s,demuxer->movi_start);
      len = priv->last_pts + len;
      priv->last_pts = 0;
    }
    if(len > 0)
      high_res_mp3_seek(demuxer,len);
    sh_audio->timer = priv->last_pts - (ds_tell_pts(demuxer->audio)-sh_audio->a_in_buffer_len)/(float)sh_audio->i_bps;
    mpca_resync_stream(sh_audio);
    return;
  }

  base = seeka->flags&DEMUX_SEEK_SET ? demuxer->movi_start : stream_tell(s);
  pos=base+(seeka->flags&DEMUX_SEEK_PERCENTS?(demuxer->movi_end - demuxer->movi_start):sh_audio->i_bps)*seeka->secs;

  if(demuxer->movi_end && pos >= demuxer->movi_end) {
    sh_audio->timer = (stream_tell(s) - demuxer->movi_start)/(float)sh_audio->i_bps;
    return;
  } else if(pos < demuxer->movi_start)
    pos = demuxer->movi_start;

  priv->last_pts = (pos-demuxer->movi_start)/(float)sh_audio->i_bps;
  sh_audio->timer = priv->last_pts - (ds_tell_pts(demuxer->audio)-sh_audio->a_in_buffer_len)/(float)sh_audio->i_bps;

  frmt=priv->frmt;
  if(frmt==RAW_WAV)
  {
    switch(sh_audio->wtag)
    {
	case 0x2000: frmt=RAW_AC3; break;
	case 0x2001: frmt=RAW_DCA; break;
	default: break;
    }
  }
  switch(frmt) {
  case RAW_AC3: {
    len = (seeka->flags & DEMUX_SEEK_SET) ? seeka->secs - priv->last_pts : seeka->secs;
    if(len < 0) {
      stream_seek(s,demuxer->movi_start);
      len = priv->last_pts + len;
      priv->last_pts = 0;
    }
    if(len > 0)
      high_res_ac3_seek(demuxer,len);
    sh_audio->timer = priv->last_pts - (ds_tell_pts(demuxer->audio)-sh_audio->a_in_buffer_len)/(float)sh_audio->i_bps;
    mpca_resync_stream(sh_audio);
    return;
  }
  case RAW_DCA: {
    len = (seeka->flags & DEMUX_SEEK_SET) ? seeka->secs - priv->last_pts : seeka->secs;
    if(len < 0) {
      stream_seek(s,demuxer->movi_start);
      len = priv->last_pts + len;
      priv->last_pts = 0;
    }
    if(len > 0)
      high_res_ddca_seek(demuxer,len);
    sh_audio->timer = priv->last_pts - (ds_tell_pts(demuxer->audio)-sh_audio->a_in_buffer_len)/(float)sh_audio->i_bps;
    mpca_resync_stream(sh_audio);
    return;
  }
  case RAW_VOC:
  case RAW_FLAC:
  case RAW_SND_AU:
  case RAW_WAV:
    pos -= (pos % (sh_audio->nch * afmt2bps(sh_audio->afmt)));
    // We need to decrease the pts by one step to make it the "last one"
    priv->last_pts -= sh_audio->wf->nAvgBytesPerSec/(float)sh_audio->i_bps;
    break;
  }

  stream_seek(s,pos);

  mpca_resync_stream(sh_audio);

}

static void audio_close(demuxer_t* demuxer) {
  da_priv_t* priv = demuxer->priv;

  if(!priv)
    return;
  mp_free(priv);
}

static MPXP_Rc audio_control(demuxer_t *demuxer,int cmd,any_t*args)
{
    return MPXP_Unknown;
}

/****************** Options stuff ******************/

#include "libmpconf/cfgparser.h"

static const config_t mp3_opts[] = {
  { "hr-seek", &hr_mp3_seek, CONF_TYPE_FLAG, 0, 0, 1, "enables hight-resolution mp3 seeking" },
  { "nohr-seek", &hr_mp3_seek, CONF_TYPE_FLAG, 0, 1, 0, "disables hight-resolution mp3 seeking"},
  {NULL, NULL, 0, 0, 0, 0, NULL}
};

static const config_t audio_opts[] = {
  { "mp3", &mp3_opts, CONF_TYPE_SUBCONFIG, 0, 0, 0, "MP3 related options" },
  {NULL, NULL, 0, 0, 0, 0, NULL}
};

demuxer_driver_t demux_audio =
{
    "WAV/MP3 parser",
    ".wav",
    audio_opts,
    audio_probe,
    audio_open,
    audio_demux,
    audio_seek,
    audio_close,
    audio_control
};
