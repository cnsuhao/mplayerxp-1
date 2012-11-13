/*
    s_network - network stream inetrface
*/
#include "mp_config.h"
#ifdef HAVE_STREAMING

#include <errno.h>
#include <stdlib.h>
#ifndef __USE_GNU
#define __USE_GNU
#endif
#include <unistd.h>
#include "stream.h"
#include "help_mp.h"
#include "demux_msg.h"
#include "osdep/mplib.h"

#include "url.h"
#include "network.h"

extern int stream_open_mf(char * filename,stream_t * stream);

typedef struct network_priv_s
{
    URL_t *url;
    off_t  spos;
}network_priv_t;

static MPXP_Rc __FASTCALL__ network_open(any_t* libinput,stream_t *stream,const char *filename,unsigned flags)
{
    URL_t* url;
    UNUSED(flags);
    url = url_new(filename);
    if(url) {
	if(streaming_start(libinput,stream, &stream->file_format, url)<0){
	    MSG_ERR(MSGTR_UnableOpenURL, filename);
	    url_free(url);
	    return MPXP_False;
	}
	MSG_INFO(MSGTR_ConnToServer, url->hostname);
	stream->priv=mp_malloc(sizeof(network_priv_t));
	((network_priv_t*)stream->priv)->url = url;
	((network_priv_t*)stream->priv)->spos = 0;
	stream->type = STREAMTYPE_STREAM;
	stream->sector_size=STREAM_BUFFER_SIZE;
	return MPXP_Ok;
    }
    check_pin("stream",stream->pin,STREAM_PIN);
    return MPXP_False;
}

static int __FASTCALL__ network_read(stream_t *stream,stream_packet_t*sp)
{
    network_priv_t *p=stream->priv;
    sp->type=0;
    if( stream->streaming_ctrl!=NULL ) {
	    sp->len=stream->streaming_ctrl->streaming_read(stream->fd,sp->buf,STREAM_BUFFER_SIZE, stream->streaming_ctrl);
    } else {
      sp->len=TEMP_FAILURE_RETRY(read(stream->fd,sp->buf,STREAM_BUFFER_SIZE));
      if(sp->len<=0) stream->_Errno=errno;
    }
    p->spos += sp->len;
    return sp->len;
}

static off_t __FASTCALL__ network_seek(stream_t *stream,off_t pos)
{
    off_t newpos=0;
    network_priv_t *p=stream->priv;
    if( stream->streaming_ctrl!=NULL ) {
      newpos=stream->streaming_ctrl->streaming_seek( stream->fd, pos, stream->streaming_ctrl );
      if( newpos<0 ) {
	MSG_WARN("Stream not seekable!\n");
	return 1;
      }
    }
    p->spos=newpos;
    return newpos;
}

static off_t __FASTCALL__ network_tell(stream_t *stream)
{
    network_priv_t *p=stream->priv;
    return p->spos;
}

static void __FASTCALL__ network_close(stream_t *stream)
{
    mp_free(((network_priv_t *)stream->priv)->url);
    mp_free(stream->priv);
    if(stream->fd>0) close(stream->fd);
}

static MPXP_Rc __FASTCALL__ network_ctrl(stream_t *s,unsigned cmd,any_t*args) {
    UNUSED(s);
    UNUSED(cmd);
    UNUSED(args);
    return MPXP_Unknown;
}

const stream_driver_t network_stream =
{
    "inet:",
    "reads multimedia stream from any known network protocol. Example: inet:http://myserver.com",
    network_open,
    network_read,
    network_seek,
    network_tell,
    network_close,
    network_ctrl
};
#endif
