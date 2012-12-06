#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
#include "mplayerxp.h"

#include <dlfcn.h>
#include "mp_conf_lavc.h"
#include "libmpcodecs/codecs_ld.h"
#include "stream.h"
#include "stream_internal.h"
#include "stream_msg.h"

namespace mpxp {
    class Lavs_Stream_Interface : public Stream_Interface {
	public:
	    Lavs_Stream_Interface();
	    virtual ~Lavs_Stream_Interface();

	    virtual MPXP_Rc	open(libinput_t* libinput,const char *filename,unsigned flags);
	    virtual int		read(stream_packet_t * sp);
	    virtual off_t	seek(off_t off);
	    virtual off_t	tell() const;
	    virtual void	close();
	    virtual MPXP_Rc	ctrl(unsigned cmd,any_t* param);
	    virtual stream_type_e type() const;
	    virtual off_t	size() const;
	    virtual off_t	sector_size() const;
	private:
	    URLContext *ctx;
	    off_t spos;
	    off_t end_pos;
    };

Lavs_Stream_Interface::Lavs_Stream_Interface():ctx(NULL),end_pos(-1) {}
Lavs_Stream_Interface::~Lavs_Stream_Interface() {
    if(ctx) ffurl_close(ctx);
}

static int lavc_int_cb(any_t*op) { UNUSED(op); return 0; } /* non interrupt blocking */
static AVIOInterruptCB int_cb = { lavc_int_cb, NULL };

int Lavs_Stream_Interface::read(stream_packet_t*sp)
{
    sp->len = ffurl_read_complete(ctx, reinterpret_cast<unsigned char*>(sp->buf), sp->len);
    if(sp->len>0) spos += sp->len;
    return sp->len;
}

off_t Lavs_Stream_Interface::seek(off_t newpos)
{
    spos = newpos;
    spos = ffurl_seek(ctx, newpos, SEEK_SET);
    return spos;
}

off_t Lavs_Stream_Interface::tell() const { return spos; }

MPXP_Rc Lavs_Stream_Interface::ctrl(unsigned cmd, any_t*arg)
{
    UNUSED(cmd);
    UNUSED(arg);
    return MPXP_Unknown;
}

void Lavs_Stream_Interface::close() {}

MPXP_Rc Lavs_Stream_Interface::open(libinput_t*libinput,const char *filename,unsigned flags)
{
    int64_t _size;

    UNUSED(flags);
    UNUSED(libinput);
    av_register_all();
    MSG_V("[lavc] Opening %s\n", filename);

    if (ffurl_open(&ctx, filename, 0, &int_cb, NULL) < 0) return MPXP_False;
    spos = 0;
    _size = ffurl_size(ctx);
    if (_size >= 0) end_pos = _size;
    return MPXP_Ok;
}

stream_type_e Lavs_Stream_Interface::type() const { return (ctx->is_streamed)?STREAMTYPE_STREAM:STREAMTYPE_SEEKABLE; }
off_t	Lavs_Stream_Interface::size() const { return end_pos; }
off_t	Lavs_Stream_Interface::sector_size() const { return STREAM_BUFFER_SIZE; }

static Stream_Interface* query_interface() { return new(zeromem) Lavs_Stream_Interface; }

extern const stream_interface_info_t lavs_stream =
{
    "lavs://",
    "reads multimedia stream through lavc library",
    query_interface
};
} // namespace mpxp
