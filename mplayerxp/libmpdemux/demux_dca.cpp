#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
#include <algorithm>

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#ifdef MP_DEBUG
#include <assert.h>
#endif

#include "libmpstream/stream.h"
#include "demuxer.h"
#include "demuxer_internal.h"
#include "stheader.h"
#include "libmpcodecs/dec_audio.h"
#include "libao2/afmt.h"
#include "aviprint.h"
#include "osdep/bswap.h"
#include "mp3_hdr.h"
#include "demux_msg.h"

#define HDR_SIZE 4

struct dca_priv_t : public Opaque {
    public:
	dca_priv_t() {}
	virtual ~dca_priv_t() {}

	int	frmt;
	float	last_pts,pts_per_packet,length;
	uint32_t dword;
	int	pos;
	/* Xing's VBR specific extensions */
	int	is_xing;
	unsigned nframes;
	unsigned nbytes;
	int	scale;
	unsigned srate;
	int	lsf;
	unsigned char	toc[100]; /* like AVI's indexes */
};

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

static int read_ddca_tags(Demuxer *demuxer,uint8_t *hdr, off_t pos,unsigned *bitrate,unsigned *samplerate,unsigned *channels)
{
    uint8_t b[12];
    unsigned n;
    Stream *s=demuxer->stream;
    demuxer->movi_end = s->end_pos();
    memcpy(b,hdr,4);
    s->seek(pos+4);
    s->read(&b[4],8);
    for(n = 0; n < 5 ; n++) {
      MSG_DBG2("read_ddca_tags\n");
      pos = ddca_decode_header(b,bitrate,samplerate,channels);
      if(pos < 0)
	return 0;
      s->skip(pos-12);
      if(s->eof())
	return 0;
      s->read(hdr,12);
      if(s->eof())
	return 0;
    }
    return 1;
}

static int dca_get_raw_id(Demuxer *demuxer,off_t fptr,unsigned *brate,unsigned *samplerate,unsigned *channels)
{
  uint32_t fcc,fcc1;
  uint8_t *p,b[32];
  Stream *s;
  *brate=*samplerate=*channels=0;
  s = demuxer->stream;
  s->seek(fptr);
  fcc=fcc1=s->read_dword();
  fcc1=me2be_32(fcc1);
  p = (uint8_t *)&fcc1;
  s->seek(fptr);
  s->read(b,sizeof(b));
  if(ddca_decode_header(b,samplerate,brate,channels)>0) return 1;
  s->seek(fptr);
  return 0;
}

static MPXP_Rc dca_probe(Demuxer* demuxer)
{
  uint32_t fcc1,fcc2;
  Stream *s;
  uint8_t *p;
  s = demuxer->stream;
  fcc1=s->read_dword();
  fcc1=me2be_32(fcc1);
  p = (uint8_t *)&fcc1;
  if(dca_get_raw_id(demuxer,0,&fcc1,&fcc2,&fcc2)) return MPXP_Ok;
  return MPXP_False;
}

static Opaque* dca_open(Demuxer* demuxer) {
  Stream *s;
  sh_audio_t* sh_audio;
  uint8_t hdr[HDR_SIZE];
  uint32_t fcc;
  int frmt = 0, n = 0, pos = 0, step;
  off_t st_pos = 0;
  dca_priv_t* priv;
  const unsigned char *pfcc;
#ifdef MP_DEBUG
  assert(demuxer != NULL);
  assert(demuxer->stream != NULL);
#endif

  priv = new(zeromem) dca_priv_t;
  s = demuxer->stream;
  s->reset();
  s->seek(s->start_pos());
  while(n < 5 && !s->eof())
  {
    st_pos = s->tell();
    step = 1;

    if(pos < HDR_SIZE) {
      s->read(&hdr[pos],HDR_SIZE-pos);
      pos = HDR_SIZE;
    }

    fcc = le2me_32(*(uint32_t *)hdr);
    pfcc = (const unsigned char *)&fcc;
    MSG_DBG2("AUDIO initial fcc=%c%c%c%c\n",pfcc[0],pfcc[1],pfcc[2],pfcc[3]);
	unsigned fmt;
	uint8_t b[21];
	MSG_DBG2("initial mp3_header: 0x%08X at %lu\n",*(uint32_t *)hdr,st_pos);
	memcpy(b,hdr,HDR_SIZE);
	s->read(&b[HDR_SIZE],12-HDR_SIZE);
	if((n = ddca_decode_header(b,&fmt,&fmt,&fmt)) > 0)
	{
	    demuxer->movi_start = st_pos;
	    break;
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
  sh_audio = demuxer->new_sh_audio();
  MSG_DBG2("mp3_header off: st_pos=%lu n=%lu HDR_SIZE=%u\n",st_pos,n,HDR_SIZE);
  demuxer->movi_start = s->tell();
  demuxer->movi_end = s->end_pos();

    sh_audio->wtag = 0x2001;
    if(!read_ddca_tags(demuxer,hdr,pos,&sh_audio->i_bps,&sh_audio->rate,&sh_audio->nch)) return 0;
    sh_audio->i_bps/=8;

  priv->frmt = frmt;
  priv->last_pts = -1;
  demuxer->priv = priv;
  demuxer->audio->id = 0;
  demuxer->audio->sh = sh_audio;
  sh_audio->ds = demuxer->audio;

  MSG_V("demux_audio: audio data 0x%llX - 0x%llX  \n",demuxer->movi_start,demuxer->movi_end);
  if(s->tell() != demuxer->movi_start)
    s->seek(demuxer->movi_start);
  if(mp_conf.verbose && sh_audio->wf) print_wave_header(sh_audio->wf,sizeof(WAVEFORMATEX));
  if(demuxer->movi_length==UINT_MAX && sh_audio->i_bps)
    demuxer->movi_length=(unsigned)(((float)demuxer->movi_end-(float)demuxer->movi_start)/(float)sh_audio->i_bps);
    check_pin("demuxer",demuxer->pin,DEMUX_PIN);
  return priv;
}

static int dca_demux(Demuxer *demuxer,Demuxer_Stream *ds) {
  sh_audio_t* sh_audio;
  Demuxer* demux;
  dca_priv_t* priv;
  Stream* s;
  int seof;
#ifdef MP_DEBUG
  assert(ds != NULL);
  assert(ds->sh != NULL);
  assert(ds->demuxer != NULL);
#endif
  sh_audio = reinterpret_cast<sh_audio_t*>(demuxer->audio->sh);
  demux = demuxer;
  priv = static_cast<dca_priv_t*>(demux->priv);
  s = demux->stream;

  seof=s->eof();
  if(seof || (demux->movi_end && s->tell() >= demux->movi_end)) {
    MSG_DBG2("dca_demux: EOF due: %s\n",
	    seof?"s->eof()":"s->tell() >= demux->movi_end");
    if(!seof) {
	MSG_DBG2("dca_demux: stream_pos=%llu movi_end=%llu\n",
		s->tell(),
		demux->movi_end);
    }
    return 0;
  }

    while(!s->eof() || (demux->movi_end && s->tell() >= demux->movi_end) ) {
      uint8_t hdr[16];
      int len;
      unsigned dummy;
      s->read(hdr,16);
      len = ddca_decode_header(hdr,&dummy,&dummy,&dummy);
      MSG_DBG2("dca_fillbuffer %u bytes\n",len);
      if(s->eof()) return 0; /* workaround for dead-lock (skip(-7)) below */
      if(len < 0) {
	s->skip(-15);
      } else {
	if(s->eof() || (demux->movi_end && s->tell() >= demux->movi_end) )
	  return 0;
	if(len>16)
	{
	    Demuxer_Packet* dp = new(zeromem) Demuxer_Packet(len);
	    dp->resize(len+16);
	    memcpy(dp->buffer(),hdr,16);
	    len=s->read(dp->buffer()+16,len-16);
	    priv->last_pts = priv->last_pts < 0 ? 0 : priv->last_pts + len/(float)sh_audio->i_bps;
	    dp->pts = priv->last_pts - (demux->audio->tell_pts()-sh_audio->a_in_buffer_len)/(float)sh_audio->i_bps;
	    dp->flags=DP_NONKEYFRAME;
	    ds->add_packet(dp);
	}
	else s->skip(len);
	return 1;
      }
    }
  return 0;
}

static void high_res_ddca_seek(Demuxer *demuxer,float _time) {
  uint8_t hdr[12];
  int len,nf;
  unsigned tmp;
  dca_priv_t* priv = static_cast<dca_priv_t*>(demuxer->priv);
  sh_audio_t* sh = (sh_audio_t*)demuxer->audio->sh;

  nf = _time*sh->rate/1152;
  while(nf > 0) {
    demuxer->stream->read(hdr,12);
    MSG_DBG2("high_res_ddca_seek\n");
    len = ddca_decode_header(hdr,&tmp,&tmp,&tmp);
    if(len < 0) {
      demuxer->stream->skip(-11);
      continue;
    }
    demuxer->stream->skip(len-12);
    priv->last_pts += 1152/(float)sh->rate;
    nf--;
  }
}

static void dca_seek(Demuxer *demuxer,const seek_args_t* seeka){
  sh_audio_t* sh_audio;
  Stream* s;
  int base,pos;
  float len;
  dca_priv_t* priv;

  if(!(sh_audio = reinterpret_cast<sh_audio_t*>(demuxer->audio->sh))) return;
  s = demuxer->stream;
  priv = static_cast<dca_priv_t*>(demuxer->priv);

  base = seeka->flags&DEMUX_SEEK_SET ? demuxer->movi_start : s->tell();
  pos=base+(seeka->flags&DEMUX_SEEK_PERCENTS?(demuxer->movi_end - demuxer->movi_start):sh_audio->i_bps)*seeka->secs;

  if(demuxer->movi_end && pos >= demuxer->movi_end) {
    sh_audio->timer = (s->tell() - demuxer->movi_start)/(float)sh_audio->i_bps;
    return;
  } else if(pos < demuxer->movi_start)
    pos = demuxer->movi_start;

  priv->last_pts = (pos-demuxer->movi_start)/(float)sh_audio->i_bps;
  sh_audio->timer = priv->last_pts - (demuxer->audio->tell_pts()-sh_audio->a_in_buffer_len)/(float)sh_audio->i_bps;

    len = (seeka->flags & DEMUX_SEEK_SET) ? seeka->secs - priv->last_pts : seeka->secs;
    if(len < 0) {
      s->seek(demuxer->movi_start);
      len = priv->last_pts + len;
      priv->last_pts = 0;
    }
    if(len > 0)
      high_res_ddca_seek(demuxer,len);
    sh_audio->timer = priv->last_pts - (demuxer->audio->tell_pts()-sh_audio->a_in_buffer_len)/(float)sh_audio->i_bps;
}

static void dca_close(Demuxer* demuxer) {
    dca_priv_t* priv = static_cast<dca_priv_t*>(demuxer->priv);

    if(!priv) return;
    delete priv;
}

static MPXP_Rc dca_control(const Demuxer *demuxer,int cmd,any_t*args)
{
    UNUSED(demuxer);
    UNUSED(cmd);
    UNUSED(args);
    return MPXP_Unknown;
}

/****************** Options stuff ******************/

#include "libmpconf/cfgparser.h"

static const config_t dca_opts[] = {
  {NULL, NULL, 0, 0, 0, 0, NULL}
};

extern const demuxer_driver_t demux_dca =
{
    "dca",
    "DCA parser",
    ".dca",
    dca_opts,
    dca_probe,
    dca_open,
    dca_demux,
    dca_seek,
    dca_close,
    dca_control
};
