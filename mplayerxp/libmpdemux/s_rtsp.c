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
#include "url.h"
#include "tcp.h"
#include "librtsp/rtsp.h"
#include "librtsp/rtsp_session.h"
#include "demux_msg.h"
#include "osdep/mplib.h"

#define RTSP_DEFAULT_PORT 554

static int __FASTCALL__ rtsp_stream_read(stream_t *s,stream_packet_t*sp)
{
  return rtsp_session_read (s->streaming_ctrl->data, sp->buf, sp->len);
}

static off_t __FASTCALL__ rtsp_seek(stream_t *s,off_t newpos)
{
    UNUSED(s);
    return newpos;
}

static off_t __FASTCALL__ rtsp_tell(stream_t *stream)
{
    UNUSED(stream);
    return 0;
}

static int __FASTCALL__ rtsp_ctrl(stream_t *s,unsigned cmd,any_t*args)
{
    UNUSED(s);
    UNUSED(cmd);
    UNUSED(args);
    return SCTRL_UNKNOWN;
}

static void __FASTCALL__ rtsp_stream_close(stream_t*s)
{
  rtsp_session_t *rtsp = NULL;

  rtsp = (rtsp_session_t *) s->streaming_ctrl->data;
  if (rtsp)
    rtsp_session_end (rtsp);
  url_free(s->streaming_ctrl->url);
  streaming_ctrl_free (s->streaming_ctrl);
}

static int __FASTCALL__ rtsp_streaming_start (any_t*libinput,stream_t *stream)
{
  int fd;
  rtsp_session_t *rtsp;
  char *mrl;
  char *file;
  int port;
  int redirected, temp;

  if (!stream)
    return -1;

  /* counter so we don't get caught in infinite redirections */
  temp = 5;

  do {
    redirected = 0;

    fd = tcp_connect2Server (libinput,stream->streaming_ctrl->url->hostname,
                         port = (stream->streaming_ctrl->url->port ?
                                 stream->streaming_ctrl->url->port :
                                 RTSP_DEFAULT_PORT), 1);

    if (fd < 0 && !stream->streaming_ctrl->url->port)
      fd = tcp_connect2Server (libinput,stream->streaming_ctrl->url->hostname,
                           port = 7070, 1);

    if (fd < 0)
      return -1;

    file = stream->streaming_ctrl->url->file;
    if (file[0] == '/')
      file++;

    mrl = mp_malloc (strlen (stream->streaming_ctrl->url->hostname)
                  + strlen (file) + 16);

    sprintf (mrl, "rtsp://%s:%i/%s",
             stream->streaming_ctrl->url->hostname, port, file);

    rtsp = rtsp_session_start (fd, &mrl, file,
                               stream->streaming_ctrl->url->hostname,
                               port, &redirected,
                               stream->streaming_ctrl->bandwidth,
                               stream->streaming_ctrl->url->username,
                               stream->streaming_ctrl->url->password);

    if (redirected == 1)
    {
      url_free (stream->streaming_ctrl->url);
      stream->streaming_ctrl->url = url_new (mrl);
      closesocket (fd);
    }

    mp_free (mrl);
    temp--;
  } while ((redirected != 0) && (temp > 0));

  if (!rtsp)
    return -1;

  stream->fd = fd;
  stream->streaming_ctrl->data = rtsp;

  stream->streaming_ctrl->prebuffer_size = 128*1024;  // 640 KBytes
  stream->streaming_ctrl->buffering = 1;
  stream->streaming_ctrl->status = streaming_playing_e;

  return 0;
}

extern int network_bandwidth;
extern int index_mode;
static int __FASTCALL__ rtsp_open (any_t* libinput,stream_t *stream,const char *filename,unsigned flags)
{
  URL_t *url;
  UNUSED(flags);
  if(strncmp(filename,"rtsp://",7)!=0) return 0;

  MSG_V("STREAM_RTSP, URL: %s\n", filename);
  stream->streaming_ctrl = streaming_ctrl_new (libinput);
  if (!stream->streaming_ctrl)
    return 0;

  stream->streaming_ctrl->bandwidth = network_bandwidth;
  url = url_new (filename);
  stream->streaming_ctrl->url = check4proxies (url);

  stream->fd = -1;
  index_mode = -1; /* prevent most RTSP streams from locking due to -idx */
  if (rtsp_streaming_start (libinput,stream) < 0)
  {
    streaming_ctrl_free (stream->streaming_ctrl);
    stream->streaming_ctrl = NULL;
    return 0;
  }

  fixup_network_stream_cache (stream);
  stream->type = STREAMTYPE_STREAM;

  return 1;
}

/* "reuse a bit of code from ftplib written by Thomas Pfau", */
const stream_driver_t rtsp_stream =
{
    "rtsp",
    "reads multimedia stream from Real Time Streaming Protocol (RTSP)",
    rtsp_open,
    rtsp_stream_read,
    rtsp_seek,
    rtsp_tell,
    rtsp_stream_close,
    rtsp_ctrl
};
