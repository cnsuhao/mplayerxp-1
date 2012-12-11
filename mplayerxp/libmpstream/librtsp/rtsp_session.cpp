#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
/*
 * This file was ported to MPlayer from xine CVS rtsp_session.c,v 1.9 2003/02/11 16:20:40
 */

/*
 * Copyright (C) 2000-2002 the xine project
 *
 * This file is part of xine, a mp_free video player.
 *
 * xine is mp_free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * xine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 *
 * high level interface to rtsp servers.
 *
 *    2006, Benjamin Zores and Vincent Mussard
 *      Support for MPEG-TS streaming through RFC compliant RTSP servers
 */

#include <sys/types.h>
#include "mplayerxp.h"
#ifndef HAVE_WINSOCK2
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#else
#include <winsock2.h>
#endif
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "tcp.h"
#include "url.h"
#include "rtp.h"
#include "rtsp.h"
#include "rtsp_rtp.h"
#include "rtsp_session.h"
#include "realrtsp/real.h"
#include "realrtsp/rmff.h"
#include "realrtsp/asmrp.h"
#include "realrtsp/xbuffer.h"
#include "stream_msg.h"

namespace mpxp {
/*
#define LOG
*/

#define RTSP_OPTIONS_PUBLIC "Public"
#define RTSP_OPTIONS_SERVER "Server"
#define RTSP_OPTIONS_LOCATION "Location"
#define RTSP_OPTIONS_REAL "RealChallenge1"
#define RTSP_SERVER_TYPE_REAL "Real"
#define RTSP_SERVER_TYPE_HELIX "Helix"
#define RTSP_SERVER_TYPE_UNKNOWN "unknown"

//Rtsp_Session *rtsp_session_start(char *mrl) {
Rtsp_Session *rtsp_session_start(Tcp& tcp, char **mrl, char *path, char *host,
  int port, int *redir, uint32_t bandwidth, char *user, char *pass) {

  Rtsp_Session *rtsp_session = NULL;
  char *server;
  char *mrl_line = NULL;
  rmff_header_t *h;

  rtsp_session = new Rtsp_Session;
  rtsp_session->s = NULL;
  rtsp_session->real_session = NULL;
  rtsp_session->rtp_session = NULL;

//connect:
  *redir = 0;

  /* connect to server */
  rtsp_session->s=rtsp_connect(tcp,*mrl,path,host,port,NULL);
  if (!rtsp_session->s)
  {
    MSG_ERR("rtsp_session: failed to connect to server %s\n", path);
    delete rtsp_session;
    return NULL;
  }

  /* looking for server type */
  if (rtsp_search_answers(rtsp_session->s,RTSP_OPTIONS_SERVER))
    server=mp_strdup(rtsp_search_answers(rtsp_session->s,RTSP_OPTIONS_SERVER));
  else {
    if (rtsp_search_answers(rtsp_session->s,RTSP_OPTIONS_REAL))
      server=mp_strdup(RTSP_SERVER_TYPE_REAL);
    else
      server=mp_strdup(RTSP_SERVER_TYPE_UNKNOWN);
  }
  if (strstr(server,RTSP_SERVER_TYPE_REAL) || strstr(server,RTSP_SERVER_TYPE_HELIX))
  {
    /* we are talking to a real server ... */

    h=real_setup_and_get_header(rtsp_session->s, bandwidth, user, pass);
    if (!h) {
      /* got an redirect? */
      if (rtsp_search_answers(rtsp_session->s, RTSP_OPTIONS_LOCATION))
      {
	delete mrl_line;
	mrl_line=mp_strdup(rtsp_search_answers(rtsp_session->s, RTSP_OPTIONS_LOCATION));
	MSG_INFO("rtsp_session: redirected to %s\n", mrl_line);
	rtsp_close(rtsp_session->s);
	delete server;
	delete *mrl;
	delete rtsp_session;
	/* tell the caller to redirect, return url to redirect to in mrl */
	*mrl = mrl_line;
	*redir = 1;
	return NULL;
//	goto connect; /* *shudder* i made a design mistake somewhere */
      } else
      {
	MSG_ERR("rtsp_session: session can not be established.\n");
	rtsp_close(rtsp_session->s);
	delete server;
	delete rtsp_session;
	return NULL;
      }
    }

    rtsp_session->real_session = init_real_rtsp_session ();
    if(!strncmp(h->streams[0]->mime_type, "application/vnd.rn-rmadriver", h->streams[0]->mime_type_size) ||
       !strncmp(h->streams[0]->mime_type, "application/smil", h->streams[0]->mime_type_size)) {
      rtsp_session->real_session->header_len = 0;
      rtsp_session->real_session->recv_size = 0;
      rtsp_session->real_session->rdt_rawdata = 1;
      MSG_V("smil-over-realrtsp playlist, switching to raw rdt mode\n");
    } else {
    rtsp_session->real_session->header_len =
      rmff_dump_header (h, (char *) rtsp_session->real_session->header, HEADER_SIZE);

      if (rtsp_session->real_session->header_len < 0) {
	MSG_ERR("rtsp_session: error while dumping RMFF headers, session can not be established.\n");
	free_real_rtsp_session(rtsp_session->real_session);
	rtsp_close(rtsp_session->s);
	delete server;
	delete mrl_line;
	delete rtsp_session;
	return NULL;
      }

    rtsp_session->real_session->recv =
      (uint8_t*)xbuffer_copyin (rtsp_session->real_session->recv, 0,
			rtsp_session->real_session->header,
			rtsp_session->real_session->header_len);

    rtsp_session->real_session->recv_size =
      rtsp_session->real_session->header_len;
    }
    rtsp_session->real_session->recv_read = 0;
  } else /* not a Real server : try RTP instead */
  {
    char *publics = NULL;

    /* look for the Public: field in response to RTSP OPTIONS */
    if (!(publics = rtsp_search_answers (rtsp_session->s, RTSP_OPTIONS_PUBLIC)))
    {
      rtsp_close (rtsp_session->s);
      delete server;
      delete mrl_line;
      delete rtsp_session;
      return NULL;
    }

    /* check for minimalistic RTSP RFC compliance */
    if (!strstr (publics, RTSP_METHOD_DESCRIBE)
	|| !strstr (publics, RTSP_METHOD_SETUP)
	|| !strstr (publics, RTSP_METHOD_PLAY)
	|| !strstr (publics, RTSP_METHOD_TEARDOWN))
    {
      MSG_ERR("Remote server does not meet minimal RTSP 1.0 compliance.\n");
      rtsp_close (rtsp_session->s);
      delete server;
      delete mrl_line;
      delete rtsp_session;
      return NULL;
    }

    rtsp_session->rtp_session = rtp_setup_and_play (rtsp_session->s);

    /* neither a Real or an RTP server */
    if (!rtsp_session->rtp_session)
    {
      MSG_ERR("rtsp_session: unsupported RTSP server. Server type is '%s'.\n", server);
      rtsp_close (rtsp_session->s);
      delete server;
      delete mrl_line;
      delete rtsp_session;
      return NULL;
    }
  }
  delete server;

  return rtsp_session;
}

int Rtsp_Session::read(Tcp& tcp,char *data, int len) {

    if (real_session) {
	int to_copy=len;
	char *dest=data;
	char *source =
	    (char *) (real_session->recv + real_session->recv_read);
	int fill = real_session->recv_size - real_session->recv_read;

	if(real_session->rdteof) return -1;
	if(len < 0) return 0;
	if(real_session->recv_size < 0) return -1;
	while (to_copy > fill) {
	    memcpy(dest, source, fill);
	    to_copy -= fill;
	    dest += fill;
	    real_session->recv_read = 0;
	    real_session->recv_size =
	    real_get_rdt_chunk (s, (char **)&(real_session->recv), real_session->rdt_rawdata);
	    if (real_session->recv_size < 0) {
		real_session->rdteof = 1;
		real_session->recv_size = 0;
	    }
	    source = (char *) real_session->recv;
	    fill = real_session->recv_size;
	    if (real_session->recv_size == 0) {
#ifdef LOG
		MSG_INFO("librtsp: %d of %d bytes provided\n", len-to_copy, len);
#endif
		return len-to_copy;
	    }
	}
	memcpy(dest, source, to_copy);
	real_session->recv_read += to_copy;
#ifdef LOG
	MSG_INFO("librtsp: %d bytes provided\n", len);
#endif
	return len;
    } else if (rtp_session) {
	int l = 0;
	Tcp _tcp(tcp.get_libinput(),rtp_session->rtp_socket);

	l = read_rtp_from_server (_tcp, data, len);
	/* send RTSP and RTCP keepalive  */
	rtcp_send_rr (s, rtp_session);
	if (l == 0)	end ();
	return l;
    }
    return 0;
}

void Rtsp_Session::end() {
    rtsp_close(s);
    if (real_session) free_real_rtsp_session (real_session);
    if (rtp_session)  rtp_session_free (rtp_session);
}

Rtsp_Session::Rtsp_Session() {}
Rtsp_Session::~Rtsp_Session() {}
} // namespace mpxp