/*
    s_null - not a driver.
*/
#include "stream.h"

static MPXP_Rc __FASTCALL__ null_open(stream_t *stream,const char *filename,unsigned flags) {
    UNUSED(filename);
    return MPXP_False;
}

static int __FASTCALL__ null_read(stream_t*stream,stream_packet_t*sp)
{
    return 0;
}

static off_t __FASTCALL__ null_seek(stream_t*stream,off_t pos)
{
    return pos;
}

static off_t __FASTCALL__ null_tell(const stream_t*stream)
{
    return 0;
}

static void __FASTCALL__ null_close(stream_t *stream) {}

static MPXP_Rc __FASTCALL__ null_ctrl(const stream_t *s,unsigned cmd,any_t*args) {
    UNUSED(s);
    UNUSED(cmd);
    UNUSED(args);
    return MPXP_Unknown;
}

const stream_driver_t null_stream =
{
    "null://",
    "not a driver",
    null_open,
    null_read,
    null_seek,
    null_tell,
    null_close,
    null_ctrl
};
