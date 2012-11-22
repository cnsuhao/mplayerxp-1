#include "mp_config.h"

#include <stdlib.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#ifndef HAVE_WINSOCK2
#include <sys/socket.h>
#define closesocket close
#else
#include <winsock2.h>
#endif

#include "stream.h"
#include "help_mp.h"
#include "udp.h"
#include "url.h"
#include "stream_msg.h"
#include "osdep/mplib.h"

using namespace mpxp;

static int __FASTCALL__ udp_read(stream_t *s,stream_packet_t*sp)
{
  return nop_streaming_read(s->fd,sp->buf,sp->len,s->streaming_ctrl);
}

static off_t __FASTCALL__ udp_seek(stream_t *s,off_t newpos)
{
    UNUSED(s);
    return newpos;
}

static off_t __FASTCALL__ udp_tell(const stream_t *stream)
{
    UNUSED(stream);
    return 0;
}

static MPXP_Rc __FASTCALL__ udp_ctrl(const stream_t *s,unsigned cmd,any_t*args)
{
    UNUSED(s);
    UNUSED(cmd);
    UNUSED(args);
    return MPXP_Unknown;
}

static void __FASTCALL__ udp_close(stream_t*stream)
{
    url_free(stream->streaming_ctrl->url);
    streaming_ctrl_free (stream->streaming_ctrl);
}

static int __FASTCALL__ udp_streaming_start (stream_t *stream)
{
  streaming_ctrl_t *streaming_ctrl;
  int fd;

  if (!stream)
    return -1;

  streaming_ctrl = stream->streaming_ctrl;
  fd = stream->fd;

  if (fd < 0)
  {
    fd = udp_open_socket (streaming_ctrl->url);
    if (fd < 0)
      return -1;
    stream->fd = fd;
  }

  streaming_ctrl->streaming_read = nop_streaming_read;
  streaming_ctrl->streaming_seek = nop_streaming_seek;
  streaming_ctrl->prebuffer_size = 64 * 1024; /* 64 KBytes */
  streaming_ctrl->buffering = 0;
  streaming_ctrl->status = streaming_playing_e;

  return 0;
}

extern int network_bandwidth;
static MPXP_Rc __FASTCALL__ udp_open (any_t* libinput,stream_t *stream,const char *filename,unsigned flags)
{
    URL_t *url;
    UNUSED(flags);
    MSG_V("STREAM_UDP, URL: %s\n", filename);
    stream->streaming_ctrl = streaming_ctrl_new (libinput);
    if (!stream->streaming_ctrl) return MPXP_False;

    stream->streaming_ctrl->bandwidth = network_bandwidth;
    url = url_new (filename);
    stream->streaming_ctrl->url = check4proxies (url);
    if (url->port == 0) {
	MSG_ERR("You must enter a port number for UDP streams!\n");
	streaming_ctrl_free (stream->streaming_ctrl);
	stream->streaming_ctrl = NULL;
	return MPXP_False;
    }
    if (udp_streaming_start (stream) < 0) {
	MSG_ERR("udp_streaming_start failed\n");
	streaming_ctrl_free (stream->streaming_ctrl);
	stream->streaming_ctrl = NULL;
	return MPXP_False;
    }
    stream->type = STREAMTYPE_STREAM;
    fixup_network_stream_cache (stream);
    check_pin("stream",stream->pin,STREAM_PIN);
    return MPXP_Ok;
}

/* "reuse a bit of code from ftplib written by Thomas Pfau", */
const stream_driver_t udp_stream =
{
    "udp://",
    "reads multimedia stream directly from User Datagram Protocol (UDP)",
    udp_open,
    udp_read,
    udp_seek,
    udp_tell,
    udp_close,
    udp_ctrl
};
