#include "config.h"

#include "../../codecs/ffmpeg/libavformat/avformat.h"
#include "../../codecs/ffmpeg/libavformat/avio.h"
#include "stream.h"
#include "demux_msg.h"

static void (*av_register_all_ptr)(void);
#define av_register_all() (*av_register_all_ptr)()
static int64_t (*url_seek_ptr)(URLContext *h, int64_t pos, int whence);
#define url_seek(a,b,c) (*url_seek_ptr)(a,b,c)
static int (*url_close_ptr)(URLContext *h);
#define url_close(a) (*url_close_ptr)(a)
static int (*url_read_complete_ptr)(URLContext *h, unsigned char *buf, int size);
#define url_read_complete(a,b,c) (*url_read_complete_ptr)(a,b,c)
static int (*url_open_ptr)(URLContext **h, const char *filename, int flags);
#define url_open(a,b,c) (*url_open_ptr)(a,b,c)
static int64_t (*url_filesize_ptr)(URLContext *h);
#define url_filesize(a) (*url_filesize_ptr)(a)

static void *dll_handle;
static int load_dll(const char *libname)
{
  if(!(dll_handle=ld_codec(libname,"http://ffmpeg.sf.net"))) return 0;
  av_register_all_ptr = ld_sym(dll_handle,"av_register_all");
  url_seek_ptr = ld_sym(dll_handle,"url_seek");
  url_close_ptr = ld_sym(dll_handle,"url_close");
  url_read_complete_ptr = ld_sym(dll_handle,"url_read_complete");
  url_open_ptr = ld_sym(dll_handle,"url_open");
  url_filesize_ptr = ld_sym(dll_handle,"url_filesize");
  return av_register_all_ptr && url_seek_ptr && url_close_ptr &&
	url_read_complete_ptr && url_open_ptr && url_filesize_ptr;
}

typedef struct ffmpeg_priv_s
{
    URLContext *ctx;
    off_t spos;
}ffmpeg_priv_t;

static int __FASTCALL__ ffmpeg_read(stream_t *s, stream_packet_t*sp)
{
    ffmpeg_priv_t*p=s->priv;
    sp->len = url_read_complete(p->ctx, sp->buf, sp->len);
    if(sp->len>0) p->spos += sp->len;
    else	  s->_Errno=errno;
    return sp->len;
}

static off_t __FASTCALL__ ffmpeg_seek(stream_t *s, off_t newpos)
{
    ffmpeg_priv_t*p=s->priv;
    p->spos = newpos;
    if ((p->spos = url_seek(p->ctx, newpos, SEEK_SET)) < 0) {
	s->_Errno=errno;
    }
    return p->spos;
}

static off_t ffmpeg_tell(stream_t *s)
{
    ffmpeg_priv_t*p=s->priv;
    return p->spos;
}

static int __FASTCALL__ ffmpeg_ctrl(stream_t *s, unsigned cmd, void *arg)
{
    UNUSED(s);
    UNUSED(cmd);
    UNUSED(arg);
    return SCTRL_UNKNOWN;
}

static void __FASTCALL__ ffmpeg_close(stream_t *stream)
{
    ffmpeg_priv_t*p=stream->priv;
    url_close(p->ctx);
    dlclose(dll_handle);
    dll_handle=NULL;
    free(p);
}

static const char prefix[] = "ffmpeg://";

static int __FASTCALL__ ffmpeg_open(stream_t *stream,const char *filename,unsigned flags)
{
    URLContext *ctx = NULL;
    ffmpeg_priv_t *p;
    int64_t size;

    UNUSED(flags);
    if(!load_dll(codec_name("libavformat"SLIBSUFFIX))) {
	MSG_ERR("Detected error during loading libavformat"SLIBSUFFIX"! Try to upgrade this library\n");
	return 0;
    }
    av_register_all();
    MSG_V("[ffmpeg] Opening %s\n", filename);

    if (url_open(&ctx, filename, URL_RDONLY) < 0) return 0;
    p = malloc(sizeof(ffmpeg_priv_t));
    p->ctx = ctx;
    p->spos = 0;
    size = url_filesize(ctx);
    if (size >= 0)
        stream->end_pos = size;
    stream->type = STREAMTYPE_SEEKABLE;
    if (ctx->is_streamed) stream->type = STREAMTYPE_STREAM;
    return 1;
}

const stream_driver_t ffmpeg_stream =
{
    "ffmpeg:",
    "reads multimedia stream through ffmpeg library",
    ffmpeg_open,
    ffmpeg_read,
    ffmpeg_seek,
    ffmpeg_tell,
    ffmpeg_close,
    ffmpeg_ctrl
};
