#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
/*
  VIVO file parser by A'rpi
  VIVO text header parser and audio support by alex
  TODO: demuxer->movi_length
  TODO: DP_KEYFRAME
*/
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h> /* strtok */

#include "mpxp_help.h"
#include "libmpstream2/stream.h"
#include "demuxer.h"
#include "demuxer_internal.h"
#include "stheader.h"
#include "osdep/bswap.h"
#include "demux_msg.h"
#include "libmpconf/cfgparser.h"

/* parameters ! */
struct vivo_param : public Opaque {
    public:
	vivo_param();
	virtual ~vivo_param() {}

	int version;
	char *acodec;
	int abitrate;
	int samplerate;
	int bytesperblock;
	int width;
	int height;
	int vformat;
};
vivo_param::vivo_param() {
    version = -1;
    acodec = NULL;
    abitrate = -1;
    samplerate = -1;
    bytesperblock = -1;
    width = -1;
    height = -1;
    vformat = -1;
}
static vivo_param vivo_param;
/* VIVO audio standards from vivog723.acm:

    G.723:
	FormatTag = 0x111
	Channels = 1 - mono
	SamplesPerSec = 8000 - 8khz
	AvgBytesPerSec = 800
	BlockAlign (bytes per block) = 24
	BitsPerSample = 8

    Siren:
	FormatTag = 0x112
	Channels = 1 - mono
	SamplesPerSec = 16000 - 16khz
	AvgBytesPerSec = 2000
	BlockAlign (bytes per block) = 40
	BitsPerSample = 8
*/

//enum { VIVO_AUDIO_G723, VIVO_AUDIO_SIREN };

enum {
    VIVO_AUDIO_G723=1,
    VIVO_AUDIO_SIREN
};

struct vivo_priv_t : public Opaque {
    public:
	vivo_priv_t() {}
	virtual ~vivo_priv_t();

	/* generic */
	char	version;
	int	supported;
	/* info */
	char	*title;
	char	*author;
	char	*copyright;
	char	*producer;
	/* video */
	float	fps;
	int	width;
	int	height;
	int	disp_width;
	int	disp_height;
	/* audio */
	int	audio_codec;
	int	audio_bitrate;
	int	audio_samplerate;
	int	audio_bytesperblock;
};

vivo_priv_t::~vivo_priv_t() {
    if (title) delete title;
    if (author) delete author;
    if (copyright) delete copyright;
    if (producer) delete producer;
}

static const mpxp_option_t vivoopts_conf[]={
    {"version", &vivo_param.version, CONF_TYPE_INT, 0, 0, 0, "forces version of VIVO stream"},
    /* audio options */
    {"acodec", &vivo_param.acodec, CONF_TYPE_STRING, 0, 0, 0, "specifies audio-codec of VIVO stream"},
    {"abitrate", &vivo_param.abitrate, CONF_TYPE_INT, 0, 0, 0, "specifies audio bitrate of VIVO stream"},
    {"samplerate", &vivo_param.samplerate, CONF_TYPE_INT, 0, 0, 0, "specifies audio samplerate of VIVO stream"},
    {"bytesperblock", &vivo_param.bytesperblock, CONF_TYPE_INT, 0, 0, 0, "specifies number of bytes per audio-block in VIVO stream"},
    /* video options */
    {"width", &vivo_param.width, CONF_TYPE_INT, 0, 0, 0, "specifies width of video in VIVO stream" },
    {"height", &vivo_param.height, CONF_TYPE_INT, 0, 0, 0, "specifies height of video in VIVO stream"},
    {"vformat", &vivo_param.vformat, CONF_TYPE_INT, 0, 0, 0, "specifies video-codec of VIVO stream"},
    {NULL, NULL, 0, 0, 0, 0, NULL}
};

static const mpxp_option_t vivo_conf[] = {
    { "vivo", (any_t*)&vivoopts_conf, CONF_TYPE_SUBCONFIG, 0, 0, 0, "Vivo specific options"},
    { NULL,NULL, 0, 0, 0, 0, NULL}
};

/* parse all possible extra headers */
/* (audio headers are seperate - mostly with recordtype=3 or 4) */

#define TEXTPARSE_ALL
static void vivo_parse_text_header(Demuxer *demux, int header_len)
{
    vivo_priv_t* priv = static_cast<vivo_priv_t*>(demux->priv);
    char *buf;
    int i;
    char *token;
    char *opt, *param;
    int parser_in_audio_block = 0;

    if (!demux->priv)
    {
	priv = new(zeromem) vivo_priv_t;
	demux->priv = priv;
	priv->supported = 0;
    }

    buf = new char [header_len];
    opt = new char [header_len];
    param = new char [header_len];
    demux->stream->read( buf, header_len);
    i=0;
    while(i<header_len && buf[i]==0x0D && buf[i+1]==0x0A) i+=2; // skip empty lines

    token = strtok(buf, (char *)&("\x0d\x0a"));
    while (token && (header_len>2))
    {
	header_len -= strlen(token)+2;
	if (sscanf(token, "%[^:]:%[^\n]", opt, param) != 2)
	{
	    MSG_V( "viv_text_header_parser: bad line: '%s' at ~%p\n",
		token, demux->stream->tell());
	    break;
	}
	MSG_DBG3( "token: '%s' (%d bytes/%d bytes left)\n",
	    token, strlen(token), header_len);
	MSG_DBG3( "token => o: '%s', p: '%s'\n",
	    opt, param);

	/* checking versions: only v1 or v2 is suitable (or known?:) */
	if (!strcmp(opt, "Version"))
	{
	    MSG_DBG2( "Version: %s\n", param);
	    if (!strncmp(param, "Vivo/1", 6) || !strncmp(param, "Vivo/2", 6))
	    {
		priv->supported = 1;
		/* save major version for fourcc */
		priv->version = param[5];
	    }
	}

	/* video specific */
	if (!strcmp(opt, "FPS"))
	{
	    MSG_DBG2( "FPS: %f\n", atof(param));
	    priv->fps = atof(param);
	}
	if (!strcmp(opt, "Width"))
	{
	    MSG_DBG2( "Width: %d\n", atoi(param));
	    priv->width = atoi(param);
	}
	if (!strcmp(opt, "Height"))
	{
	    MSG_DBG2( "Height: %d\n", atoi(param));
	    priv->height = atoi(param);
	}
	if (!strcmp(opt, "DisplayWidth"))
	{
	    MSG_DBG2( "Display Width: %d\n", atoi(param));
	    priv->disp_width = atoi(param);
	}
	if (!strcmp(opt, "DisplayHeight"))
	{
	    MSG_DBG2( "Display Height: %d\n", atoi(param));
	    priv->disp_height = atoi(param);
	}

	/* audio specific */
	if (!strcmp(opt, "RecordType"))
	{
	    /* no audio recordblock by Vivo/1.00, 3 and 4 by Vivo/2.00 */
	    if ((atoi(param) == 3) || (atoi(param) == 4))
		parser_in_audio_block = 1;
	    else
		parser_in_audio_block = 0;
	}
	if (!strcmp(opt, "NominalBitrate"))
	{
	    priv->audio_bitrate = atoi(param);
	    if (priv->audio_bitrate == 2000)
		priv->audio_codec = VIVO_AUDIO_SIREN;
	    if (priv->audio_bitrate == 800)
		priv->audio_codec = VIVO_AUDIO_G723;
	}
	if (!strcmp(opt, "SamplingFrequency"))
	{
	    priv->audio_samplerate = atoi(param);
	    if (priv->audio_samplerate == 16000)
		priv->audio_codec = VIVO_AUDIO_SIREN;
	    if (priv->audio_samplerate == 8000)
		priv->audio_codec = VIVO_AUDIO_G723;
	}
	if (!strcmp(opt, "Length") && (parser_in_audio_block == 1))
	{
	    priv->audio_bytesperblock = atoi(param); /* 24 or 40 kbps */
	    if (priv->audio_bytesperblock == 40)
		priv->audio_codec = VIVO_AUDIO_SIREN;
	    if (priv->audio_bytesperblock == 24)
		priv->audio_codec = VIVO_AUDIO_G723;
	}

	/* only for displaying some informations about movie*/
	if (!strcmp(opt, "Title"))
	{
	    demux->info().add( INFOT_NAME, param);
	    priv->title = mp_strdup(param);
	}
	if (!strcmp(opt, "Author"))
	{
	    demux->info().add( INFOT_AUTHOR, param);
	    priv->author = mp_strdup(param);
	}
	if (!strcmp(opt, "Copyright"))
	{
	    demux->info().add( INFOT_COPYRIGHT, param);
	    priv->copyright = mp_strdup(param);
	}
	if (!strcmp(opt, "Producer"))
	{
	    demux->info().add( INFOT_ENCODER, param);
	    priv->producer = mp_strdup(param);
	}

	/* get next token */
	token = strtok(NULL, (char *)&("\x0d\x0a"));
    }

    if (buf)
	delete buf;
    if (opt)
	delete opt;
    if (param)
	delete param;
}

static MPXP_Rc vivo_probe(Demuxer* demuxer){
    int i=0;
    int len;
    int c;
    char buf[2048+256];
    vivo_priv_t* priv;
    int orig_pos = demuxer->stream->tell();

    MSG_V("Checking for VIVO\n");

    c=demuxer->stream->read_char();
    if(c==-256) return MPXP_False;
    len=0;
    while((c=demuxer->stream->read_char())>=0x80){
	len+=0x80*(c-0x80);
	if(len>1024) return MPXP_False;
    }
    len+=c;
    MSG_DBG2("header block 1 size: %d\n",len);
    //stream_skip(demuxer->stream,len);

    priv=new(zeromem) vivo_priv_t;
    demuxer->priv=priv;

#if 0
    vivo_parse_text_header(demuxer, len);
    if (priv->supported == 0)
	return MPXP_False;
#else
    /* this is enought for check (for now) */
    demuxer->stream->read(buf,len);
    buf[len]=0;

    // parse header:
    i=0;
    while(i<len && buf[i]==0x0D && buf[i+1]==0x0A) i+=2; // skip empty lines
    if(strncmp(buf+i,"Version:Vivo/",13)) return MPXP_False; // bad version/type!
#endif

#if 0
    c=demuxer->stream->read_char();
    if(c) return MPXP_False;
    len2=0;
    while((c=demuxer->stream->read_char())>=0x80){
	len2+=0x80*(c-0x80);
	if(len+len2>2048) return MPXP_False;
    }
    len2+=c;
    MSG_DBG2("header block 2 size: %d\n",len2);
    stream_skip(demuxer->stream,len2);
//    stream_read(demuxer->stream,buf+len,len2);
#endif
//    c=demuxer->stream->read_char();
    demuxer->stream->seek( orig_pos);
    demuxer->file_format=Demuxer::Type_VIVO;
    return MPXP_Ok;
}

static int audio_pos=0;
static int audio_rate=0;

// return value:
//     0 = EOF or no stream found
//     1 = successfully read a packet
static int vivo_demux(Demuxer *demux,Demuxer_Stream *__ds){
  Demuxer_Stream *ds=NULL;
  int c;
  int len=0;
  int seq;
  int prefix=0;
  demux->filepos=demux->stream->tell();

  c=demux->stream->read_char();
  if (c == -256) /* EOF */
    return 0;
  if (c == 0x82)
  {
      /* ok, this works, but pts calculating from header is required! */
#warning "Calculate PTS from picture header!"
      prefix = 1;
      c = demux->stream->read_char();
      MSG_DBG2("packet 0x82(pos=%llu) chunk=%x\n",
	demux->stream->tell(), c);
  }
  switch(c&0xF0){
  case 0x00: // header - skip it!
  {
      len=demux->stream->read_char();
      if(len>=0x80) len=0x80*(len-0x80)+demux->stream->read_char();
      MSG_DBG2("vivo extra header: %d bytes\n",len);
#ifdef TEXTPARSE_ALL
{
      int pos;
      /* also try to parse all headers */
      pos = demux->stream->tell();
      vivo_parse_text_header(demux, len);
      demux->stream->seek( pos);
}
#endif
      break;
  }
  case 0x10:  // video packet
      if (prefix == 1)
	len = demux->stream->read_char();
      else
	len=128;
      ds=demux->video;
      break;
  case 0x20:  // video packet
      len=demux->stream->read_char();
      ds=demux->video;
      break;
  case 0x30:  // audio packet
      if (prefix == 1)
	len = demux->stream->read_char();
      else
	len=40;	/* 40kbps */
      ds=demux->audio;
      audio_pos+=len;
      break;
  case 0x40:  // audio packet
      if (prefix == 1)
	len = demux->stream->read_char();
      else
	len=24;	/* 24kbps */
      ds=demux->audio;
      audio_pos+=len;
      break;
  default:
      MSG_WARN("VIVO - unknown ID found: %02X at pos %lu contact author!\n",
	c, demux->stream->tell());
      return 0;
  }

  if(!ds || ds->id<-1){
      if(len) demux->stream->skip(len);
      return 1;
  }

  seq=c&0x0F;

    if(ds->asf_packet){
      if(ds->asf_seq!=seq){
	// closed segment, finalize packet:
	ds->add_packet(ds->asf_packet);
	ds->asf_packet=NULL;
      } else {
	// append data to it!
	Demuxer_Packet* dp=ds->asf_packet;
	dp->resize(dp->length()+len);
	//memcpy(dp->buffer+dp->len,data,len);
	demux->stream->read(dp->buffer()+dp->length(),len);
	MSG_DBG3("data appended! %d+%d\n",dp->length(),len);
	// we are ready now.
	if((c&0xF0)==0x20) --ds->asf_seq; // hack!
	return 1;
      }
    }
    // create new packet:
      Demuxer_Packet* dp=new(zeromem) Demuxer_Packet(len);
      //memcpy(dp->buffer,data,len);
      len=demux->stream->read(dp->buffer(),len);
      dp->resize(len);
      dp->pts=audio_rate?((float)audio_pos/(float)audio_rate):0;
      dp->flags=DP_NONKEYFRAME;
      dp->pos=demux->filepos;
      ds->asf_packet=dp;
      ds->asf_seq=seq;
      // we are ready now.
      return 1;
}

static const short h263_format[8][2] = {
    { 0, 0 },
    { 128, 96 },
    { 176, 144 },
    { 352, 288 },
    { 704, 576 },
    { 1408, 1152 },
    { 320, 240 }   // ??????? or 240x180 (found in vivo2) ?
};

static unsigned char* buffer;
static int bufptr=0;
static int bitcnt=0;
static unsigned char buf=0;
static int format, width, height;

static unsigned int x_get_bits(int n){
    unsigned int x=0;
    while(n-->0){
	if(!bitcnt){
	    // fill buff
	    buf=buffer[bufptr++];
	    bitcnt=8;
	}
	//x=(x<<1)|(buf&1);buf>>=1;
	x=(x<<1)|(buf>>7);buf<<=1;
	--bitcnt;
    }
    return x;
}

#define get_bits(xxx,n) x_get_bits(n)
#define get_bits1(xxx) x_get_bits(1)
#define skip_bits(xxx,n) x_get_bits(n)
#define skip_bits1(xxx) x_get_bits(1)

/* most is hardcoded. should extend to handle all h263 streams */
static int h263_decode_picture_header(const unsigned char *b_ptr)
{
//    int i;
    const unsigned char *buffer;
    buffer=b_ptr;
    bufptr=bitcnt=buf=0;

    /* picture header */
    if (get_bits(&s->gb, 22) != 0x20){
	MSG_ERR("bad picture header\n");
	return -1;
    }
    skip_bits(&s->gb, 8); /* picture timestamp */

    if (get_bits1(&s->gb) != 1){
	MSG_ERR("bad marker\n");
	return -1;	/* marker */
    }
    if (get_bits1(&s->gb) != 0){
	MSG_ERR("bad h263 id\n");
	return -1;	/* h263 id */
    }
    skip_bits1(&s->gb);	/* split screen off */
    skip_bits1(&s->gb);	/* camera  off */
    skip_bits1(&s->gb);	/* freeze picture release off */

    format = get_bits(&s->gb, 3);

    if (format != 7) {
	MSG_ERR("h263_plus = 0  format = %d\n",format);
	/* H.263v1 */
	width = h263_format[format][0];
	height = h263_format[format][1];
	MSG_ERR("%d x %d\n",width,height);
//        if (!width) return -1;

	MSG_V("pict_type=%d\n",get_bits1(&s->gb));
	MSG_V("unrestricted_mv=%d\n",get_bits1(&s->gb));
#if 1
	MSG_V("SAC: %d\n",get_bits1(&s->gb));
	MSG_V("advanced prediction mode: %d\n",get_bits1(&s->gb));
	MSG_V("PB frame: %d\n",get_bits1(&s->gb));
#else
	if (get_bits1(&s->gb) != 0)
	    return -1;	/* SAC: off */
	if (get_bits1(&s->gb) != 0)
	    return -1;	/* advanced prediction mode: off */
	if (get_bits1(&s->gb) != 0)
	    return -1;	/* not PB frame */
#endif
	MSG_V("qscale=%d\n",get_bits(&s->gb, 5));
	skip_bits1(&s->gb);	/* Continuous Presence Multipoint mode: off */
    } else {
	MSG_V("h263_plus = 1\n");
	/* H.263v2 */
	if (get_bits(&s->gb, 3) != 1){
	    MSG_ERR("H.263v2 A error\n");
	    return -1;
	}
	if (get_bits(&s->gb, 3) != 6){ /* custom source format */
	    MSG_ERR("custom source format\n");
	    return -1;
	}
	skip_bits(&s->gb, 12);
	skip_bits(&s->gb, 3);
	MSG_V("pict_type=%d\n",get_bits(&s->gb, 3) + 1);
//        if (s->pict_type != I_TYPE &&
//            s->pict_type != P_TYPE)
//            return -1;
	skip_bits(&s->gb, 7);
	skip_bits(&s->gb, 4); /* aspect ratio */
	width = (get_bits(&s->gb, 9) + 1) * 4;
	skip_bits1(&s->gb);
	height = get_bits(&s->gb, 9) * 4;
	MSG_V("%d x %d\n",width,height);
	//if (height == 0)
	//    return -1;
	MSG_V("qscale=%d\n",get_bits(&s->gb, 5));
    }

    /* PEI */
    while (get_bits1(&s->gb) != 0) {
	skip_bits(&s->gb, 8);
    }
//    s->f_code = 1;
//    s->width = width;
//    s->height = height;
    return 0;
}

static Opaque* vivo_open(Demuxer* demuxer){
    vivo_priv_t* priv=static_cast<vivo_priv_t*>(demuxer->priv);

  if(!demuxer->video->fill_buffer()){
    MSG_ERR("VIVO: " MSGTR_MissingVideoStreamBug);
    return NULL;
  }

    audio_pos=0;

  h263_decode_picture_header(demuxer->video->buffer());

  if (vivo_param.version != -1)
    priv->version = '0' + vivo_param.version;

{		sh_video_t* sh=demuxer->new_sh_video();

		/* viv1, viv2 (for better codecs.conf) */
		sh->fourcc = mmioFOURCC('v', 'i', 'v', priv->version);
		if(!sh->fps)
		{
		    if (priv->fps)
			sh->fps=priv->fps;
		    else
			sh->fps=15.0f;
		}

		/* XXX: FIXME: can't scale image. */
		/* hotfix to disable: */
		priv->disp_width = priv->width;
		priv->disp_height = priv->height;

		if (vivo_param.width != -1)
		    priv->disp_width = priv->width = vivo_param.width;

		if (vivo_param.height != -1)
		    priv->disp_height = priv->height = vivo_param.height;

		if (vivo_param.vformat != -1)
		{
		    priv->disp_width = priv->width = h263_format[vivo_param.vformat][0];
		    priv->disp_height = priv->height = h263_format[vivo_param.vformat][1];
		}

		if (priv->disp_width)
		    sh->src_w = priv->disp_width;
		else
		    sh->src_w = width;
		if (priv->disp_height)
		    sh->src_h = priv->disp_height;
		else
		    sh->src_h = height;

		// emulate BITMAPINFOHEADER:
		sh->bih=new(zeromem) BITMAPINFOHEADER;
		sh->bih->biSize=40;
		if (priv->width)
		    sh->bih->biWidth = priv->width;
		else
		    sh->bih->biWidth = width;
		if (priv->height)
		    sh->bih->biHeight = priv->height;
		else
		    sh->bih->biHeight = height;
		sh->bih->biPlanes=1;
		sh->bih->biBitCount=24;
		sh->bih->biCompression=sh->fourcc;
		sh->bih->biSizeImage=sh->bih->biWidth*sh->bih->biHeight*3;

		/* insert as stream */
		demuxer->video->sh=sh;
		sh->ds=demuxer->video;
		demuxer->video->id=0;

		/* disable seeking */
		demuxer->flags &= ~Demuxer::Seekable;

		MSG_V("VIVO Video stream %d size: display: %dx%d, codec: %ux%u\n",
		    demuxer->video->id, sh->src_w, sh->src_h, sh->bih->biWidth,
		    sh->bih->biHeight);
}

/* AUDIO init */
if (demuxer->audio->id >= -1){
  if(!demuxer->audio->fill_buffer()){
    MSG_ERR("VIVO: " MSGTR_MissingAudioStream);
  } else
{		sh_audio_t* sh=demuxer->new_sh_audio();

		/* Select audio codec */
		if (priv->audio_codec == 0)
		{
		    if (priv->version == '2')
			priv->audio_codec = VIVO_AUDIO_SIREN;
		    else
			priv->audio_codec = VIVO_AUDIO_G723;
		}
		if (vivo_param.acodec != NULL)
		{
		    if (!strcasecmp(vivo_param.acodec, "g723"))
			priv->audio_codec = VIVO_AUDIO_G723;
		    if (!strcasecmp(vivo_param.acodec, "siren"))
			priv->audio_codec = VIVO_AUDIO_SIREN;
		}

		sh->wtag = UINT_MAX;
		if (priv->audio_codec == VIVO_AUDIO_G723)
		    sh->wtag = 0x111;
		if (priv->audio_codec == VIVO_AUDIO_SIREN)
		    sh->wtag = 0x112;
		if (sh->wtag == UINT_MAX)
		{
		    MSG_ERR( "VIVO: Not support audio codec (%d)\n",
			priv->audio_codec);
		    goto nosound;
		}

		// Emulate WAVEFORMATEX struct:
		sh->wf=new(zeromem) WAVEFORMATEX;
		sh->wf->wFormatTag=sh->wtag;
		sh->wf->nChannels=1; /* 1 channels for both Siren and G.723 */

		/* Set bits per sample */
		if (priv->audio_codec == VIVO_AUDIO_SIREN)
		    sh->wf->wBitsPerSample = 16;
		else
		if (priv->audio_codec == VIVO_AUDIO_G723)
		    sh->wf->wBitsPerSample = 8;

		/* Set sampling rate */
		if (priv->audio_samplerate) /* got from header */
		    sh->wf->nSamplesPerSec = priv->audio_samplerate;
		else
		{
		    if (priv->audio_codec == VIVO_AUDIO_SIREN)
			sh->wf->nSamplesPerSec = 16000;
		    if (priv->audio_codec == VIVO_AUDIO_G723)
			sh->wf->nSamplesPerSec = 8000;
		}
		if (vivo_param.samplerate != -1)
		    sh->wf->nSamplesPerSec = vivo_param.samplerate;

		/* Set audio bitrate */
		if (priv->audio_bitrate) /* got from header */
		    sh->wf->nAvgBytesPerSec = priv->audio_bitrate;
		else
		{
		    if (priv->audio_codec == VIVO_AUDIO_SIREN)
			sh->wf->nAvgBytesPerSec = 2000;
		    if (priv->audio_codec == VIVO_AUDIO_G723)
			sh->wf->nAvgBytesPerSec = 800;
		}
		if (vivo_param.abitrate != -1)
		    sh->wf->nAvgBytesPerSec = vivo_param.abitrate;
		audio_rate=sh->wf->nAvgBytesPerSec;

		if (!priv->audio_bytesperblock)
		{
		    if (priv->audio_codec == VIVO_AUDIO_SIREN)
			sh->wf->nBlockAlign = 40;
		    if (priv->audio_codec == VIVO_AUDIO_G723)
			sh->wf->nBlockAlign = 24;
		}
		else
		    sh->wf->nBlockAlign = priv->audio_bytesperblock;
		if (vivo_param.bytesperblock != -1)
		    sh->wf->nBlockAlign = vivo_param.bytesperblock;

		/* insert as stream */
		demuxer->audio->sh=sh;
		sh->ds=demuxer->audio;
		demuxer->audio->id=1;
nosound:
		;
}
}
    check_pin("demuxer",demuxer->pin,DEMUX_PIN);
    return priv;
}

static void vivo_close(Demuxer *demuxer)
{
    vivo_priv_t* priv=static_cast<vivo_priv_t*>(demuxer->priv);

    if (priv) delete priv;
    return;
}

static MPXP_Rc vivo_control(const Demuxer *demuxer,int cmd,any_t*args)
{
    UNUSED(demuxer);
    UNUSED(cmd);
    UNUSED(args);
    return MPXP_Unknown;
}

extern const demuxer_driver_t demux_vivo =
{
    "vivo",
    "VIVO parser",
    ".vivo",
    vivo_conf,
    vivo_probe,
    vivo_open,
    vivo_demux,
    NULL,
    vivo_close,
    vivo_control
};
