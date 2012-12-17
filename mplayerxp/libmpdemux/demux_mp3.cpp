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

#include "libmpstream2/stream.h"
#include "demuxer.h"
#include "demuxer_internal.h"
#include "stheader.h"
#include "genres.h"
#include "libmpcodecs/dec_audio.h"
#include "libao3/afmt.h"
#include "aviprint.h"
#include "osdep/bswap.h"
#include "mp3_hdr.h"
#include "demux_msg.h"

#define RAW_MP1 1
#define RAW_MP2 2
#define RAW_MP3 3
#define HDR_SIZE 4

struct mp3_priv_t : public Opaque {
    public:
	mp3_priv_t() {}
	virtual ~mp3_priv_t() {}

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

static int hr_mp3_seek = 0;

static void find_next_mp3_hdr(Demuxer *demuxer,uint8_t *hdr) {
  int len;
  off_t spos;
  while(!demuxer->stream->eof()) {
    spos=demuxer->stream->tell();
    demuxer->stream->read(hdr,4);
    len = mp_decode_mp3_header(hdr,NULL,NULL,NULL,NULL);
    if(len < 0) {
      demuxer->stream->skip(-3);
      continue;
    }
    demuxer->stream->seek(spos);
    break;
  }
}


static int read_mp3v1_tags(Demuxer *demuxer,uint8_t *hdr, off_t pos )
{
    unsigned n;
    Stream *s=demuxer->stream;
    for(n = 0; n < 5 ; n++) {
      MSG_DBG2("read_mp3v1_tags\n");
      pos = mp_decode_mp3_header(hdr,NULL,NULL,NULL,NULL);
      if(pos < 0)
	return 0;
      s->skip(pos-4);
      if(s->eof())
	return 0;
      s->read(hdr,4);
      if(s->eof())
	return 0;
    }
    if(s->end_pos()) {
      char tag[4];
      s->seek(s->end_pos()-128);
      s->read(tag,3);
      tag[3] = '\0';
      if(strcmp(tag,"TAG"))
	demuxer->movi_end = s->end_pos();
      else {
	char buf[31];
	uint8_t g;
	demuxer->movi_end = s->tell()-3;
	s->read(buf,30);
	buf[30] = '\0';
	demuxer->info().add(INFOT_NAME,buf);
	s->read(buf,30);
	buf[30] = '\0';
	demuxer->info().add(INFOT_AUTHOR,buf);
	s->read(buf,30);
	buf[30] = '\0';
	demuxer->info().add(INFOT_ALBUM,buf);
	s->read(buf,4);
	buf[4] = '\0';
	demuxer->info().add(INFOT_DATE,buf);
	s->read(buf,30);
	buf[30] = '\0';
	demuxer->info().add(INFOT_COMMENTS,buf);
	if(buf[28] == 0 && buf[29] != 0) {
	  uint8_t trk = (uint8_t)buf[29];
	  sprintf(buf,"%d",trk);
	  demuxer->info().add(INFOT_TRACK,buf);
	}
	g = s->read_char();
	demuxer->info().add(INFOT_GENRE,genres[g]);
      }
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
static int read_id3v22_tags(Demuxer *demuxer,unsigned flags,unsigned hsize)
{
    off_t pos,epos;
    Stream *s=demuxer->stream;
    if(	flags==ID3V22_ZERO_FLAG ||
	flags==ID3V22_UNSYNCH_FLAG ||
	flags==ID3V22_COMPRESS_FLAG) return 0;
    pos=s->tell();
    epos=pos+hsize;
    while(pos<epos)
    {
	uint32_t id;
	unsigned len;
	unsigned char buf[ID3V22_FRAME_HEADER_SIZE];
	char data[4096];
	s->read(buf,ID3V22_FRAME_HEADER_SIZE);
	id=(buf[2] << 16) + (buf[1] << 8) + buf[0];
	len=(buf[3] << 14) + (buf[4] << 7) + buf[5];
	s->read(data,std::min(len,unsigned(4096)));
	data[std::min(len,unsigned(4096))]=0;
	switch(id)
	{
	    case mmioFOURCC(0,'T','T','1'): if(len>1) demuxer->info().add(INFOT_DESCRIPTION,data+1); break;
	    case mmioFOURCC(0,'T','T','2'): if(len>1) demuxer->info().add(INFOT_NAME,data+1); break;
	    case mmioFOURCC(0,'T','T','3'): if(len>1) demuxer->info().add(INFOT_SUBJECT,data+1); break;
	    case mmioFOURCC(0,'C','O','M'): if(len>4) demuxer->info().add(INFOT_COMMENTS,data+4); break;
	    case mmioFOURCC(0,'T','C','O'): if(len>1) demuxer->info().add(INFOT_GENRE,genres[data[1]]); break;
	    case mmioFOURCC(0,'T','C','R'): if(len>1) demuxer->info().add(INFOT_COPYRIGHT,genres[data[1]]); break;
	    case mmioFOURCC(0,'T','P','1'): if(len>1) demuxer->info().add(INFOT_AUTHOR,data+1); break;
	    case mmioFOURCC(0,'T','A','L'): if(len>1) demuxer->info().add(INFOT_ALBUM,data+1); break;
	    case mmioFOURCC(0,'T','R','K'): if(len>1) demuxer->info().add(INFOT_TRACK,data+1); break;
	    case mmioFOURCC(0,'T','Y','E'): if(len>1) demuxer->info().add(INFOT_DATE,data+1); break;
	    case mmioFOURCC(0,'T','E','N'): if(len>1) demuxer->info().add(INFOT_ENCODER,data+1); break;
	    case mmioFOURCC(0,'T','M','T'): if(len>1) demuxer->info().add(INFOT_SOURCE_MEDIA,data+1); break;
//	    case mmioFOURCC(0,'T','F','T'): if(len>1) demuxer->info().add(INFOT_MIME,data+1); break;
	    case mmioFOURCC(0,'P','O','P'): if(len>1) demuxer->info().add(INFOT_RATING,data+1); break;
	    case mmioFOURCC(0,'W','X','X'): if(len>1) demuxer->info().add(INFOT_WWW,data+1); break;
	    case 0: goto end;
	    default: MSG_WARN("Unhandled frame: %3s\n",buf); break;
	}
	pos=s->tell();
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

static int read_id3v23_tags(Demuxer *demuxer,unsigned flags,unsigned hsize)
{
    off_t pos,epos;
    Stream *s=demuxer->stream;
    if(	flags==ID3V23_ZERO_FLAG ||
	flags==ID3V23_UNSYNCH_FLAG) return 0;
    if( flags==ID3V23_EXT_HEADER_FLAG )
    {
	char buf[4];
	unsigned ehsize;
	demuxer->stream->read(buf,4);
	ehsize=(buf[0] << 21) + (buf[1] << 14) + (buf[2] << 7) + buf[3];
	demuxer->stream->skip(ehsize);
    }
    pos=s->tell();
    epos=pos+hsize;
    while(pos<epos)
    {
	uint32_t id;
	unsigned len;
	unsigned char buf[ID3V23_FRAME_HEADER_SIZE];
	char data[4096];
	s->read(buf,ID3V23_FRAME_HEADER_SIZE);
	id=*((uint32_t *)buf);
	len=(buf[4] << 21) + (buf[5] << 14) + (buf[6] << 7) + buf[7];
	s->read(data,std::min(len,unsigned(4096)));
	data[std::min(len,unsigned(4096))]=0;
	MSG_V("ID3: %4s len %u\n",buf,len);
	switch(id)
	{
	    case mmioFOURCC('T','I','T','1'): if(len>1) demuxer->info().add(INFOT_DESCRIPTION,data+1); break;
	    case mmioFOURCC('T','I','T','2'): if(len>1) demuxer->info().add(INFOT_NAME,data+1); break;
	    case mmioFOURCC('T','I','T','3'): if(len>1) demuxer->info().add(INFOT_SUBJECT,data+1); break;
	    case mmioFOURCC('C','O','M','M'): if(len>4) demuxer->info().add(INFOT_COMMENTS,data+4); break;
	    case mmioFOURCC('T','C','O','N'): if(len>1) demuxer->info().add(INFOT_GENRE,genres[data[1]]); break;
	    case mmioFOURCC('T','P','E','1'): if(len>1) demuxer->info().add(INFOT_AUTHOR,data+1); break;
	    case mmioFOURCC('T','A','L','B'): if(len>1) demuxer->info().add(INFOT_ALBUM,data+1); break;
	    case mmioFOURCC('T','R','C','K'): if(len>1) demuxer->info().add(INFOT_TRACK,data+1); break;
	    case mmioFOURCC('T','Y','E','R'): if(len>1) demuxer->info().add(INFOT_DATE,data+1); break;
	    case mmioFOURCC('T','E','N','C'): if(len>1) demuxer->info().add(INFOT_ENCODER,data+1); break;
	    case mmioFOURCC('T','C','O','P'): if(len>1) demuxer->info().add(INFOT_COPYRIGHT,data+1); break;
	    case mmioFOURCC('T','M','E','D'): if(len>1) demuxer->info().add(INFOT_SOURCE_MEDIA,data+1); break;
//	    case mmioFOURCC('T','F','L','T'): if(len>1) demuxer->info().add(INFOT_MIME,data+1); break;
	    case mmioFOURCC('P','O','P','M'): if(len>1) demuxer->info().add(INFOT_RATING,data+1); break;
	    case mmioFOURCC('W','X','X','X'): if(len>1) demuxer->info().add(INFOT_WWW,data+1); break;
	    case 0: goto end;
	    default: MSG_V("Unhandled frame: %4s\n",buf); break;
	}
	pos=s->tell();
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

static int read_id3v24_tags(Demuxer *demuxer,unsigned flags,unsigned hsize)
{
    off_t pos,epos;
    Stream *s=demuxer->stream;
    if(	flags==ID3V24_ZERO_FLAG ||
	flags==ID3V24_UNSYNCH_FLAG) return 0;
    if( flags==ID3V24_EXT_HEADER_FLAG )
    {
	char buf[4];
	unsigned ehsize;
	demuxer->stream->read(buf,4);
	ehsize=(buf[0] << 21) + (buf[1] << 14) + (buf[2] << 7) + buf[3];
	demuxer->stream->skip(ehsize);
    }
    pos=s->tell();
    epos=pos+hsize;
    while(pos<epos)
    {
	uint32_t id;
	unsigned len;
	unsigned char buf[ID3V23_FRAME_HEADER_SIZE];
	char data[4096];
	s->read(buf,ID3V23_FRAME_HEADER_SIZE);
	id=*((uint32_t *)buf);
	len=(buf[4] << 21) + (buf[5] << 14) + (buf[6] << 7) + buf[7];
	s->read(data,std::min(len,unsigned(4096)));
	data[std::min(len,unsigned(4096))]=0;
	MSG_V("ID3: %4s len %u\n",buf,len);
	switch(id)
	{
	    /* first byte of data indicates encoding type: 0-ASCII (1-2)-UTF16(LE,BE) 3-UTF8 */
	    case mmioFOURCC('T','I','T','1'): if(len>1) demuxer->info().add(INFOT_DESCRIPTION,data+1); break;
	    case mmioFOURCC('T','I','T','2'): if(len>1) demuxer->info().add(INFOT_NAME,data+1); break;
	    case mmioFOURCC('T','I','T','3'): if(len>1) demuxer->info().add(INFOT_SUBJECT,data+1); break;
	    case mmioFOURCC('C','O','M','M'): if(len>4) demuxer->info().add(INFOT_COMMENTS,data+4); break;
	    case mmioFOURCC('T','C','O','N'): if(len>1) demuxer->info().add(INFOT_GENRE,genres[data[1]]); break;
	    case mmioFOURCC('T','P','E','1'): if(len>1) demuxer->info().add(INFOT_AUTHOR,data+1); break;
	    case mmioFOURCC('T','A','L','B'): if(len>1) demuxer->info().add(INFOT_ALBUM,data+1); break;
	    case mmioFOURCC('T','R','C','K'): if(len>1) demuxer->info().add(INFOT_TRACK,data+1); break;
/*!*/	    case mmioFOURCC('T','D','R','C'): if(len>1) demuxer->info().add(INFOT_DATE,data+1); break;
	    case mmioFOURCC('T','E','N','C'): if(len>1) demuxer->info().add(INFOT_ENCODER,data+1); break;
	    case mmioFOURCC('T','C','O','P'): if(len>1) demuxer->info().add(INFOT_COPYRIGHT,data+1); break;
	    case mmioFOURCC('T','M','E','D'): if(len>1) demuxer->info().add(INFOT_SOURCE_MEDIA,data+1); break;
//	    case mmioFOURCC('T','F','L','T'): if(len>1) demuxer->info().add(INFOT_MIME,data+1); break;
	    case mmioFOURCC('P','O','P','M'): if(len>1) demuxer->info().add(INFOT_RATING,data+1); break;
	    case mmioFOURCC('W','X','X','X'): if(len>1) demuxer->info().add(INFOT_WWW,data+1); break;
	    case 0: goto end;
	    default: MSG_V("Unhandled frame: %4s\n",buf); break;
	}
	pos=s->tell();
    }
    end:
    return 1;
}

static int read_id3v2_tags(Demuxer *demuxer)
{
    char buf[4];
    Stream* s=demuxer->stream;
    unsigned vers,rev,flags,hsize;
    s->seek(3); /* skip 'ID3' */
    vers=s->read_char();
    rev=s->read_char();
    flags=s->read_char();
    s->read(buf,4);
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

static int mp3_get_raw_id(Demuxer *demuxer,off_t fptr,unsigned *brate,unsigned *samplerate,unsigned *channels)
{
  int retval=0;
  uint32_t fcc,fcc1,fmt;
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
  if(mp_check_mp3_header(fcc1,&fmt,brate,samplerate,channels)) {
    if(fmt==1)	retval = RAW_MP1;
    else
    if(fmt==2)	retval = RAW_MP2;
    else	retval = RAW_MP3;
  }
  s->seek(fptr);
  return retval;
}

static MPXP_Rc mp3_probe(Demuxer* demuxer)
{
  uint32_t fcc1,fcc2;
  Stream *s;
  uint8_t *p;
  s = demuxer->stream;
  fcc1=s->read_dword();
  fcc1=me2be_32(fcc1);
  p = (uint8_t *)&fcc1;
  if(p[0] == 'I' && p[1] == 'D' && p[2] == '3' && (p[3] >= 2)) return MPXP_Ok;
  else
  if(mp3_get_raw_id(demuxer,0,&fcc1,&fcc2,&fcc2)) return MPXP_Ok;
  return MPXP_False;
}

#define FRAMES_FLAG     0x0001
#define BYTES_FLAG      0x0002
#define TOC_FLAG        0x0004
#define VBR_SCALE_FLAG  0x0008
#define FRAMES_AND_BYTES (FRAMES_FLAG | BYTES_FLAG)
#define MPG_MD_MONO     3

static void  Xing_test(Stream *s,uint8_t *hdr,mp3_priv_t *priv)
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
    fpos = s->tell();
    s->skip(off);
    s->read(buf,4);
    if(memcmp(buf,"Xing",4) == 0 || memcmp(buf,"Info",4) == 0)
    {
	priv->is_xing=1;
	priv->lsf=mpeg1?0:1;
	priv->srate=sr_table[sr_index&0x3];
	head_flags = s->read_dword();
	if(head_flags & FRAMES_FLAG)	priv->nframes=s->read_dword();
	if(head_flags & BYTES_FLAG)		priv->nbytes=s->read_dword();
	if(head_flags & TOC_FLAG)		s->read(priv->toc,100);
	if(head_flags & VBR_SCALE_FLAG)	priv->scale = s->read_dword();
	MSG_DBG2("Found Xing VBR header: flags=%08X nframes=%u nbytes=%u scale=%i srate=%u\n"
	,head_flags,priv->nframes,priv->nbytes,priv->scale,priv->srate);
	s->seek(fpos);
    }
    else s->seek(fpos);
}

static Opaque* mp3_open(Demuxer* demuxer) {
  Stream *s;
  sh_audio_t* sh_audio;
  uint8_t hdr[HDR_SIZE];
  uint32_t fcc;
  int frmt = 0, n = 0, pos = 0, step;
  unsigned mp3_brate,mp3_samplerate,mp3_channels;
  off_t st_pos = 0;
  mp3_priv_t* priv;
  const unsigned char *pfcc;
#ifdef MP_DEBUG
  assert(demuxer != NULL);
  assert(demuxer->stream != NULL);
#endif

  priv = new(zeromem) mp3_priv_t;
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
    if( hdr[0] == 'I' && hdr[1] == 'D' && hdr[2] == '3' && (hdr[3] >= 2)) {
	unsigned len,fmt;
	s->skip(2);
	s->read(hdr,4);
	len = (hdr[0]<<21) | (hdr[1]<<14) | (hdr[2]<<7) | hdr[3];
	read_id3v2_tags(demuxer);
	s->seek(len+10);
	find_next_mp3_hdr(demuxer,hdr);
	Xing_test(s,hdr,priv);
	mp_decode_mp3_header(hdr,&fmt,&mp3_brate,&mp3_samplerate,&mp3_channels);
	step = 4;
	frmt=RAW_MP3;
    } else {
	unsigned fmt;
	uint8_t b[21];
	MSG_DBG2("initial mp3_header: 0x%08X at %lu\n",*(uint32_t *)hdr,st_pos);
	if((n = mp_decode_mp3_header(hdr,&fmt,&mp3_brate,&mp3_samplerate,&mp3_channels)) > 0) {
	    /* A Xing header may be present in stream as the first frame of an mp3 bitstream */
	    Xing_test(s,hdr,priv);
	    demuxer->movi_start = st_pos;
	    frmt = fmt;
	    break;
	}
	memcpy(b,hdr,HDR_SIZE);
	s->read(&b[HDR_SIZE],12-HDR_SIZE);
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
  switch(frmt) {
  case RAW_MP1:
  case RAW_MP2:
    sh_audio->wtag = 0x50;
    sh_audio->i_bps=mp3_brate;
    sh_audio->rate=mp3_samplerate;
    sh_audio->nch=mp3_channels;
    demuxer->movi_start-=HDR_SIZE;
    if(!read_mp3v1_tags(demuxer,hdr,pos)) return 0; /* id3v1 may coexist with id3v2 */
    break;
  case RAW_MP3:
    sh_audio->wtag = 0x55;
    sh_audio->i_bps=mp3_brate;
    sh_audio->rate=mp3_samplerate;
    sh_audio->nch=mp3_channels;
    demuxer->movi_start-=HDR_SIZE;
    if(!read_mp3v1_tags(demuxer,hdr,pos)) return 0; /* id3v1 may coexist with id3v2 */
    break;
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
  }
  MSG_V("demux_audio: audio data 0x%llX - 0x%llX  \n",demuxer->movi_start,demuxer->movi_end);
  if(s->tell() != demuxer->movi_start)
    s->seek(demuxer->movi_start);
  if(mp_conf.verbose && sh_audio->wf) print_wave_header(sh_audio->wf,sizeof(WAVEFORMATEX));
  if(demuxer->movi_length==UINT_MAX && sh_audio->i_bps)
    demuxer->movi_length=(unsigned)(((float)demuxer->movi_end-(float)demuxer->movi_start)/(float)sh_audio->i_bps);
    check_pin("demuxer",demuxer->pin,DEMUX_PIN);
  return priv;
}

static int mp3_demux(Demuxer *demuxer,Demuxer_Stream *ds) {
  sh_audio_t* sh_audio;
  Demuxer* demux;
  mp3_priv_t* priv;
  Stream* s;
  int frmt,seof;
#ifdef MP_DEBUG
  assert(ds != NULL);
  assert(ds->sh != NULL);
  assert(ds->demuxer != NULL);
#endif
  sh_audio = reinterpret_cast<sh_audio_t*>(demuxer->audio->sh);
  demux = demuxer;
  priv = static_cast<mp3_priv_t*>(demux->priv);
  s = demux->stream;

  seof=s->eof();
  if(seof || (demux->movi_end && s->tell() >= demux->movi_end)) {
    MSG_DBG2("mp3_demux: EOF due: %s\n",
	    seof?"s->eof()":"s->tell() >= demux->movi_end");
    if(!seof) {
	MSG_DBG2("mp3_demux: stream_pos=%llu movi_end=%llu\n",
		s->tell(),
		demux->movi_end);
    }
    return 0;
  }
  frmt=priv->frmt;
  switch(frmt) {
  case RAW_MP1:
  case RAW_MP2:
  case RAW_MP3:
    while(!s->eof() || (demux->movi_end && s->tell() >= demux->movi_end) ) {
      uint8_t hdr[4];
      int len;
      s->read(hdr,4);
      MSG_DBG2("mp3_fillbuffer\n");
      len = mp_decode_mp3_header(hdr,NULL,NULL,NULL,NULL);
      if(s->eof()) return 0; /* workaround for dead-lock (skip(-3)) below */
      if(len < 0) {
	s->skip(-3);
      } else {
	if(s->eof() || (demux->movi_end && s->tell() >= demux->movi_end) )
	  return 0;
	if(len>4)
	{
	    Demuxer_Packet* dp = new(zeromem) Demuxer_Packet(len);
	    memcpy(dp->buffer(),hdr,4);
	    dp->resize(len+4);
	    len=s->read(dp->buffer() + 4,len-4);
	    priv->last_pts = priv->last_pts < 0 ? 0 : priv->last_pts + len/(float)sh_audio->i_bps;
	    dp->pts = priv->last_pts - (demux->audio->tell_pts()-sh_audio->a_in_buffer_len)/(float)sh_audio->i_bps;
	    dp->flags=DP_NONKEYFRAME;
	    ds->add_packet(dp);
	}
	else s->skip(len);
	return 1;
      }
    } break;
  default:
    MSG_ERR("Audio demuxer : unknown format %d\n",priv->frmt);
  }
  return 0;
}

static void high_res_mp3_seek(Demuxer *demuxer,float _time) {
  uint8_t hdr[4];
  int len,nf;
  mp3_priv_t* priv = static_cast<mp3_priv_t*>(demuxer->priv);
  sh_audio_t* sh = reinterpret_cast<sh_audio_t*>(demuxer->audio->sh);

  nf = _time*sh->rate/1152;
  while(nf > 0) {
    demuxer->stream->read(hdr,4);
    MSG_DBG2("high_res_mp3_seek\n");
    len = mp_decode_mp3_header(hdr,NULL,NULL,NULL,NULL);
    if(len < 0) {
      demuxer->stream->skip(-3);
      continue;
    }
    demuxer->stream->skip(len-4);
    priv->last_pts += 1152/(float)sh->rate;
    nf--;
  }
}

static void mp3_seek(Demuxer *demuxer,const seek_args_t* seeka){
  sh_audio_t* sh_audio;
  Stream* s;
  int base,pos;
  float len;
  mp3_priv_t* priv;

  if(!(sh_audio = reinterpret_cast<sh_audio_t*>(demuxer->audio->sh))) return;
  s = demuxer->stream;
  priv = static_cast<mp3_priv_t*>(demuxer->priv);
  if(priv->is_xing)
  {
    unsigned percents,cpercents,npercents;
    off_t newpos,spos;
    percents = (unsigned)(seeka->secs*100.)/(float)demuxer->movi_length;
    spos=demuxer->stream->tell();
    cpercents=(unsigned)((float)spos*100./(float)priv->nbytes);
    npercents=(seeka->flags&DEMUX_SEEK_SET)?percents:cpercents+percents;
    if(npercents>100) npercents=100;
    newpos=demuxer->movi_start+(off_t)(((float)priv->toc[npercents]/256.0)*priv->nbytes);
    MSG_DBG2("xing seeking: secs=%f prcnts=%u cprcnts=%u spos=%llu newpos=%llu\n",seeka->secs,npercents,cpercents,spos,newpos);
    demuxer->stream->seek(newpos);
    priv->last_pts=(((float)demuxer->movi_length*npercents)/100.)*1000.;
    return;
  }
  if((priv->frmt == RAW_MP3 || priv->frmt == RAW_MP2 || priv->frmt == RAW_MP1) && hr_mp3_seek && !(seeka->flags & DEMUX_SEEK_PERCENTS)) {
    len = (seeka->flags & DEMUX_SEEK_SET) ? seeka->secs - priv->last_pts : seeka->secs;
    if(len < 0) {
      s->seek(demuxer->movi_start);
      len = priv->last_pts + len;
      priv->last_pts = 0;
    }
    if(len > 0)
      high_res_mp3_seek(demuxer,len);
    sh_audio->timer = priv->last_pts - (demuxer->audio->tell_pts()-sh_audio->a_in_buffer_len)/(float)sh_audio->i_bps;
    return;
  }

  base = seeka->flags&DEMUX_SEEK_SET ? demuxer->movi_start : s->tell();
  pos=base+(seeka->flags&DEMUX_SEEK_PERCENTS?(demuxer->movi_end - demuxer->movi_start):sh_audio->i_bps)*seeka->secs;

  if(demuxer->movi_end && pos >= demuxer->movi_end) {
    sh_audio->timer = (s->tell() - demuxer->movi_start)/(float)sh_audio->i_bps;
    return;
  } else if(pos < demuxer->movi_start)
    pos = demuxer->movi_start;

  priv->last_pts = (pos-demuxer->movi_start)/(float)sh_audio->i_bps;
  sh_audio->timer = priv->last_pts - (demuxer->audio->tell_pts()-sh_audio->a_in_buffer_len)/(float)sh_audio->i_bps;

  s->seek(pos);
}

static void mp3_close(Demuxer* demuxer) {
    mp3_priv_t* priv = static_cast<mp3_priv_t*>(demuxer->priv);

    if(!priv) return;
    delete priv;
}

static MPXP_Rc mp3_control(const Demuxer *demuxer,int cmd,any_t*args)
{
    UNUSED(demuxer);
    UNUSED(cmd);
    UNUSED(args);
    return MPXP_Unknown;
}

/****************** Options stuff ******************/

#include "libmpconf/cfgparser.h"

static const config_t mp3_opts[] = {
  { "hr-seek", &hr_mp3_seek, CONF_TYPE_FLAG, 0, 0, 1, "enables hight-resolution mp3 seeking" },
  { "nohr-seek", &hr_mp3_seek, CONF_TYPE_FLAG, 0, 1, 0, "disables hight-resolution mp3 seeking"},
  {NULL, NULL, 0, 0, 0, 0, NULL}
};

static const config_t mp3_options[] = {
  { "mp3", (any_t*)&mp3_opts, CONF_TYPE_SUBCONFIG, 0, 0, 0, "MP3 related options" },
  {NULL, NULL, 0, 0, 0, 0, NULL}
};

extern const demuxer_driver_t demux_mp3 =
{
    "mp3",
    "MP3 parser",
    ".mp3",
    mp3_options,
    mp3_probe,
    mp3_open,
    mp3_demux,
    mp3_seek,
    mp3_close,
    mp3_control
};
