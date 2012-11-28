#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
#include "mplayerxp.h"

#include <dlfcn.h>
#include "mp_conf_lavc.h"
#include "libmpcodecs/codecs_ld.h"
#include "stream.h"
#include "stream_msg.h"

struct lavc_priv_t : public Opaque {
    public:
	lavc_priv_t() {}
	virtual ~lavc_priv_t();

	URLContext *ctx;
	off_t spos;
};

lavc_priv_t::~lavc_priv_t() {
    if(ctx) ffurl_close(ctx);
}

static int lavc_int_cb(any_t*op) { return 0; } /* non interrupt blicking */
static AVIOInterruptCB int_cb = { lavc_int_cb, NULL };

static int __FASTCALL__ lavc_read(stream_t *s, stream_packet_t*sp)
{
    lavc_priv_t*p=static_cast<lavc_priv_t*>(s->priv);
    sp->len = ffurl_read_complete(p->ctx, reinterpret_cast<unsigned char*>(sp->buf), sp->len);
    if(sp->len>0) p->spos += sp->len;
    return sp->len;
}

static off_t __FASTCALL__ lavc_seek(stream_t *s, off_t newpos)
{
    lavc_priv_t*p=static_cast<lavc_priv_t*>(s->priv);
    p->spos = newpos;
    p->spos = ffurl_seek(p->ctx, newpos, SEEK_SET);
    return p->spos;
}

static off_t lavc_tell(const stream_t *s)
{
    lavc_priv_t*p=static_cast<lavc_priv_t*>(s->priv);
    return p->spos;
}

static MPXP_Rc __FASTCALL__ lavc_ctrl(const stream_t *s, unsigned cmd, any_t*arg)
{
    UNUSED(s);
    UNUSED(cmd);
    UNUSED(arg);
    return MPXP_Unknown;
}

static void __FASTCALL__ lavc_close(stream_t *stream)
{
    lavc_priv_t*p=static_cast<lavc_priv_t*>(stream->priv);
    delete p;
}

static const char prefix[] = "lavc://";

static MPXP_Rc __FASTCALL__ lavc_open(any_t*libinput,stream_t *stream,const char *filename,unsigned flags)
{
    URLContext *ctx = NULL;
    lavc_priv_t *p;
    int64_t size;

    UNUSED(flags);
    UNUSED(libinput);
    av_register_all();
    MSG_V("[lavc] Opening %s\n", filename);

    if (ffurl_open(&ctx, filename, 0, &int_cb, NULL) < 0) return MPXP_False;
    p = new(zeromem) lavc_priv_t;
    p->ctx = ctx;
    p->spos = 0;
    size = ffurl_size(ctx);
    if (size >= 0)
	stream->end_pos = size;
    stream->type = STREAMTYPE_SEEKABLE;
    stream->priv = p;
    if (ctx->is_streamed) stream->type = STREAMTYPE_STREAM;
    check_pin("stream",stream->pin,STREAM_PIN);
    return MPXP_Ok;
}

extern const stream_driver_t lavc_stream =
{
    "lavc:",
    "reads multimedia stream through lavc library",
    lavc_open,
    lavc_read,
    lavc_seek,
    lavc_tell,
    lavc_close,
    lavc_ctrl
};
