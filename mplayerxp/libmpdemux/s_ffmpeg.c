#include "../mp_config.h"

#include <dlfcn.h>
#include "libavformat/avformat.h"
#include "libavformat/avio.h"
#include "libavformat/url.h"
#include "../libmpcodecs/codecs_ld.h"
#include "stream.h"
#include "demux_msg.h"

typedef struct ffmpeg_priv_s
{
    URLContext *ctx;
    off_t spos;
}ffmpeg_priv_t;

static int ffmpeg_int_cb(void *op) { return 0; } /* non interrupt blicking */
static AVIOInterruptCB int_cb = { ffmpeg_int_cb, NULL };

static int __FASTCALL__ ffmpeg_read(stream_t *s, stream_packet_t*sp)
{
    ffmpeg_priv_t*p=s->priv;
    sp->len = ffurl_read_complete(p->ctx, sp->buf, sp->len);
    if(sp->len>0) p->spos += sp->len;
    else	  s->_Errno=errno;
    return sp->len;
}

static off_t __FASTCALL__ ffmpeg_seek(stream_t *s, off_t newpos)
{
    ffmpeg_priv_t*p=s->priv;
    p->spos = newpos;
    if ((p->spos = ffurl_seek(p->ctx, newpos, SEEK_SET)) < 0) {
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
    ffurl_close(p->ctx);
    free(p);
}

static const char prefix[] = "ffmpeg://";

static int __FASTCALL__ ffmpeg_open(stream_t *stream,const char *filename,unsigned flags)
{
    URLContext *ctx = NULL;
    ffmpeg_priv_t *p;
    int64_t size;

    UNUSED(flags);
    av_register_all();
    MSG_V("[ffmpeg] Opening %s\n", filename);

    if (ffurl_open(&ctx, filename, 0, &int_cb, NULL) < 0) return 0;
    p = malloc(sizeof(ffmpeg_priv_t));
    p->ctx = ctx;
    p->spos = 0;
    size = ffurl_size(ctx);
    if (size >= 0)
        stream->end_pos = size;
    stream->type = STREAMTYPE_SEEKABLE;
    stream->priv = p;
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
