/*
    s_file - stream interface for file i/o.
*/
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifndef __USE_GNU
#define __USE_GNU
#endif
#include <unistd.h>
#include <fcntl.h>

#include "mplayerxp.h"
#include "stream.h"
#include "osdep/mplib.h"

typedef struct file_priv_s
{
    int was_open;
    off_t spos;
}file_priv_t;


static MPXP_Rc __FASTCALL__ file_open(any_t*libinput,stream_t *stream,const char *filename,unsigned flags)
{
    UNUSED(flags);
    UNUSED(libinput);
    if(!(stream->priv = mp_malloc(sizeof(file_priv_t)))) return MPXP_False;
    if(strcmp(filename,"-")==0) stream->fd=0;
    else stream->fd=open(filename,O_RDONLY);
    if(stream->fd<0) { delete stream->priv; return MPXP_False; }
    ((file_priv_t*)stream->priv)->was_open = stream->fd==0?0:1;
    stream->end_pos = lseek(stream->fd,0,SEEK_END);
    lseek(stream->fd,0,SEEK_SET);
    if(stream->end_pos == -1)	stream->type = STREAMTYPE_STREAM;
    else			stream->type = STREAMTYPE_SEEKABLE;
    /* decreasing number of packet from 256 to 10 speedups cache2 from 3.27% to 1.26%
       with full speed 1.04% for -nocache */
    /* Note: Please locate sector_size changinf after all read/write operations of open() function */
    stream->sector_size=mp_conf.s_cache_size?mp_conf.s_cache_size*1024/10:STREAM_BUFFER_SIZE;
    ((file_priv_t*)stream->priv)->spos = 0;
    check_pin("stream",stream->pin,STREAM_PIN);
    return MPXP_Ok;
}

static MPXP_Rc __FASTCALL__ stdin_open(any_t*libinput,stream_t *stream,const char *filename,unsigned flags) {
    UNUSED(filename);
    return file_open(NULL,stream,"-",flags);
}

#ifndef TEMP_FAILURE_RETRY
#define TEMP_FAILURE_RETRY(x) (x)
#endif

static int __FASTCALL__ file_read(stream_t*stream,stream_packet_t*sp)
{
/*
    Should we repeate read() again on these errno: `EAGAIN', `EIO' ???
*/
    file_priv_t*p=reinterpret_cast<file_priv_t*>(stream->priv);
    sp->type=0;
    sp->len = TEMP_FAILURE_RETRY(read(stream->fd,sp->buf,sp->len));
    if(sp->len>0) p->spos += sp->len;
    return sp->len;
}

# define TEMP_FAILURE_RETRY64(expression) \
  (__extension__					\
    ({ long long int __result;				\
       do __result = (long long int) (expression);	\
       while (__result == -1LL && errno == EINTR);	\
       __result; }))

static off_t __FASTCALL__ file_seek(stream_t*stream,off_t pos)
{
    file_priv_t*p=reinterpret_cast<file_priv_t*>(stream->priv);
    p->spos=TEMP_FAILURE_RETRY64(lseek(stream->fd,pos,SEEK_SET));
    return p->spos;
}

static off_t __FASTCALL__ file_tell(const stream_t*stream)
{
    file_priv_t*p=reinterpret_cast<file_priv_t*>(stream->priv);
    return p->spos;
}

static void __FASTCALL__ file_close(stream_t *stream)
{
    int was_open = reinterpret_cast<file_priv_t*>(stream->priv)->was_open;
    if(was_open) close(stream->fd);
    delete stream->priv;
}

static MPXP_Rc __FASTCALL__ file_ctrl(const stream_t *s,unsigned cmd,any_t*args) {
    UNUSED(s);
    UNUSED(cmd);
    UNUSED(args);
    return MPXP_Unknown;
}

extern const stream_driver_t stdin_stream =
{
    "stdin://",
    "reads multimedia stream from standard input",
    stdin_open,
    file_read,
    file_seek,
    file_tell,
    file_close,
    file_ctrl
};

extern const stream_driver_t file_stream =
{
    "file://",
    "reads multimedia stream from regular file",
    file_open,
    file_read,
    file_seek,
    file_tell,
    file_close,
    file_ctrl
};
