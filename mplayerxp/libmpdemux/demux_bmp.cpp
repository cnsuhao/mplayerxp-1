#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
/*
    BMP file parser for the MPlayer program
    by Mike Melanson
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>

#include "help_mp.h"

#include "libmpstream/stream.h"
#include "demuxer.h"
#include "demuxer_internal.h"
#include "stheader.h"

#include "libvo/img_format.h"
#include "osdep/fastmemcpy.h"
#include "demux_msg.h"

#ifdef HAVE_SDL_IMAGE
#include <SDL/SDL_image.h>


static int demux_rw_seek(struct SDL_RWops *context, int offset, int whence)
{
    unsigned long newpos=-1;
    demuxer_t *demux = reinterpret_cast<demuxer_t*>(context->hidden.unknown.data1);
    switch(whence)
    {
	case SEEK_SET:
		newpos = offset;
		break;
	case SEEK_CUR:
		newpos = stream_tell(demux->stream)+offset;
		break;
	case SEEK_END:
		newpos = -1; /* TODO !!!*/
		MSG_ERR("demux_bmp: unsupported syscall\n");
		break;
    }
    stream_reset(demux->stream);
    stream_seek(demux->stream,newpos);
    return newpos;
}

static int demux_rw_read(struct SDL_RWops *context, any_t*ptr, int size, int maxnum)
{
    int retval;
    demuxer_t *demux = reinterpret_cast<demuxer_t*>(context->hidden.unknown.data1);
    retval = stream_read(demux->stream,ptr,size*maxnum);
    return retval;
}

static int demux_rw_write(struct SDL_RWops *context, const any_t*ptr, int size, int num)
{
    return 0;
}

static int demux_rw_close(struct SDL_RWops *context)
{
    return 0;
}

static void demux_dup_rw_stream(demuxer_t* demuxer, struct SDL_RWops*rwops)
{
    /* it's not a danger to mp_malloc 8 bytes */
    rwops->hidden.unknown.data1=demuxer;
    rwops->type=0;
    rwops->close=demux_rw_close;
    rwops->write=demux_rw_write;
    rwops->read=demux_rw_read;
    rwops->seek=demux_rw_seek;
}

static void demux_rw_close_stream(struct SDL_RWops*rwops)
{
}

static SDL_RWops my_rw;
static SDL_Surface *img;

static MPXP_Rc bmp_probe(demuxer_t *demuxer)
{
    demux_dup_rw_stream(demuxer,&my_rw);
    stream_reset(demuxer->stream);
    img = IMG_Load_RW(&my_rw,0);
    if(img) demuxer->file_format=DEMUXER_TYPE_BMP;
    return img ? MPXP_Ok : MPXP_False;
}

static int bmp_demux(demuxer_t *demuxer,Demuxer_Stream *__ds)
{
  UNUSED(__ds);
  unsigned lsize=((img->format->BitsPerPixel+7)/8)*img->w;
  unsigned j,len=lsize*img->h;
  unsigned npal_colors;
  int fake_24;
  uint8_t *dst,*src;
  npal_colors = img->format->palette ? img->format->palette->ncolors : 0;
  fake_24 = img->format->BitsPerPixel == 8 && npal_colors > 0;
  Demuxer_Packet* dp = new(zeromem) Demuxer_Packet(fake_24 ? len*3 : len);
  dst = dp->buffer();
  src =reinterpret_cast<uint8_t*>(img->pixels);
  if(fake_24)
  {
    for(j=0;j<img->h;j++)
    {
	unsigned i;
	for(i=0;i<img->w;i++)
	{
	    uint8_t idx;
	    idx = src[i];
	    dst[i*3+0]=img->format->palette->colors[idx].b;
	    dst[i*3+1]=img->format->palette->colors[idx].g;
	    dst[i*3+2]=img->format->palette->colors[idx].r;
	}
	dst+=img->w*3;
	src+=img->pitch;
    }
  }
  else
  {
    if(img->pitch == lsize)
	memcpy(dst,src,len);
    else
    for(j=0;j<img->h;j++)
    {
	memcpy(dst,src,lsize);
	dst+=lsize;
	src+=img->pitch;
    }
  }
  demuxer->video->add_packet(dp);
/* return value:
     0 = EOF or no stream found
     1 = successfully read a packet */
  return 1;
}

static demuxer_t* bmp_open(demuxer_t* demuxer)
{
  sh_video_t *sh_video = NULL;
  unsigned npal_colors;
  // create a new video stream header
  sh_video = new_sh_video(demuxer, 0);
  // make sure the demuxer knows about the new video stream header
  demuxer->video->sh = sh_video;
  // make sure that the video demuxer stream header knows about its
  // parent video demuxer stream
  sh_video->ds = demuxer->video;
  npal_colors = img->format->palette ? img->format->palette->ncolors : 0;
    switch(img->format->BitsPerPixel)
    {
	default:
	case 8:  sh_video->fourcc = npal_colors > 0 ? IMGFMT_BGR24 :
				    img->format->Bshift < img->format->Rshift ? IMGFMT_BGR8 : IMGFMT_RGB8; break;
	case 15: sh_video->fourcc = img->format->Bshift < img->format->Rshift ? IMGFMT_BGR15 : IMGFMT_RGB15; break;
	case 16: sh_video->fourcc = img->format->Bshift < img->format->Rshift ? IMGFMT_BGR16 : IMGFMT_RGB16; break;
	case 24: sh_video->fourcc = img->format->Bshift < img->format->Rshift ? IMGFMT_BGR24 : IMGFMT_RGB24; break;
	case 32: sh_video->fourcc = img->format->Bshift < img->format->Rshift ? IMGFMT_BGR32 : IMGFMT_RGB32; break;
    }
  // custom fourcc for internal MPlayer use
  MSG_V("demux_bmp: bpp=%u w=%u h=%u RGB_fmt(loss: %u %u %u %u shift: %u %u %u %u mask %u %u %u %u) palette=%u\n"
  ,img->format->BitsPerPixel,img->w,img->h
  ,img->format->Aloss
  ,img->format->Bloss
  ,img->format->Gloss
  ,img->format->Rloss
  ,img->format->Ashift
  ,img->format->Bshift
  ,img->format->Gshift
  ,img->format->Rshift
  ,img->format->Amask
  ,img->format->Bmask
  ,img->format->Gmask
  ,img->format->Rmask
  ,npal_colors);
  sh_video->src_w = img->w;
  sh_video->src_h = img->h;
  sh_video->is_static = 1;
  // get the speed
  sh_video->fps = 2;

//  demuxer->priv = bmp_image;
    check_pin("demuxer",demuxer->pin,DEMUX_PIN);
    return demuxer;
}

static void bmp_close(demuxer_t* demuxer)
{
    UNUSED(demuxer);
    if(img) SDL_FreeSurface(img);
    demux_rw_close_stream(&my_rw);
}

#else

struct bmp_image_t : public Opaque {
    public:
	bmp_image_t() {}
	virtual ~bmp_image_t() {}

	int image_size;
	int image_offset;
};

// Check if a file is a BMP file depending on whether starts with 'BM'
static MPXP_Rc bmp_probe(demuxer_t *demuxer)
{
  if (stream_read_word(demuxer->stream) == (('B' << 8) | 'M'))
    return MPXP_Ok;
  else
    return MPXP_False;
}

// return value:
//     0 = EOF or no stream found
//     1 = successfully read a packet
static int bmp_demux(demuxer_t *demuxer,Demuxer_Stream *__ds)
{
  bmp_image_t *bmp_image = static_cast<bmp_image_t*>(demuxer->priv);

  stream_reset(demuxer->stream);
  stream_seek(demuxer->stream, bmp_image->image_offset);
  ds_read_packet(demuxer->video, demuxer->stream, bmp_image->image_size,
    0, bmp_image->image_offset, DP_KEYFRAME);
  return 1;
}

static demuxer_t* bmp_open(demuxer_t* demuxer)
{
  sh_video_t *sh_video = NULL;
  unsigned int filesize;
  unsigned int data_offset;
  bmp_image_t *bmp_image;

  // go back to the beginning
  stream_reset(demuxer->stream);
  stream_seek(demuxer->stream, demuxer->stream->start_pos+2);
  filesize = stream_read_dword_le(demuxer->stream);
  stream_skip(demuxer->stream, 4);
  data_offset = stream_read_word_le(demuxer->stream);
  stream_skip(demuxer->stream, 2);

  // create a new video stream header
  sh_video = new_sh_video(demuxer, 0);

  // make sure the demuxer knows about the new video stream header
  demuxer->video->sh = sh_video;

  // make sure that the video demuxer stream header knows about its
  // parent video demuxer stream
  sh_video->ds = demuxer->video;

  // load the BITMAPINFOHEADER
  // allocate size and take the palette table into account
  sh_video->bih = (BITMAPINFOHEADER *)mp_malloc(data_offset - 12);
  sh_video->bih->biSize = stream_read_dword_le(demuxer->stream);
  sh_video->bih->biWidth = stream_read_dword_le(demuxer->stream);
  sh_video->bih->biHeight = stream_read_dword_le(demuxer->stream);
  sh_video->bih->biPlanes = stream_read_word_le(demuxer->stream);
  sh_video->bih->biBitCount = stream_read_word_le(demuxer->stream);
  sh_video->bih->biCompression = stream_read_dword_le(demuxer->stream);
  sh_video->bih->biSizeImage = stream_read_dword_le(demuxer->stream);
  sh_video->bih->biXPelsPerMeter = stream_read_dword_le(demuxer->stream);
  sh_video->bih->biYPelsPerMeter = stream_read_dword_le(demuxer->stream);
  sh_video->bih->biClrUsed = stream_read_dword_le(demuxer->stream);
  sh_video->bih->biClrImportant = stream_read_dword_le(demuxer->stream);
  // fetch the palette
  stream_read(demuxer->stream, (unsigned char *)(sh_video->bih) + 40,
    sh_video->bih->biClrUsed * 4);

  // load the data
  bmp_image = new(zeromem) bmp_image_t;
  bmp_image->image_size = filesize - data_offset;
  bmp_image->image_offset = data_offset;

  // custom fourcc for internal MPlayer use
  sh_video->fourcc = sh_video->bih->biCompression;

  sh_video->src_w = sh_video->bih->biWidth;
  sh_video->src_h = sh_video->bih->biHeight;

  // get the speed
  sh_video->is_static = 1;
  sh_video->fps = 1;

  demuxer->priv = bmp_image;

  return demuxer;
}

static void bmp_close(demuxer_t* demuxer) {
  bmp_image_t *bmp_image = static_cast<bmp_image_t*>(demuxer->priv);

  if(!bmp_image)
    return;
  delete bmp_image;
}
#endif

static MPXP_Rc bmp_control(const demuxer_t *demuxer,int cmd,any_t*args)
{
    UNUSED(demuxer);
    UNUSED(cmd);
    UNUSED(args);
    return MPXP_Unknown;
}

extern const demuxer_driver_t demux_bmp =
{
    "bmp",
    "BMP - Bitmap amd other pictures parser",
    ".bmp",
    NULL,
    bmp_probe,
    bmp_open,
    bmp_demux,
    NULL,
    bmp_close,
    bmp_control
};
