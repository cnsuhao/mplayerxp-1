/*
 *  Copyright (C) 2006 Benjamin Zores
 *   heavily base on the Freebox patch for xine by Vincent Mussard
 *   but with many enhancements for better RTSP RFC compliance.
 *
 *   This program is mp_free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software Foundation,
 *  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef HAVE_RTSP_RTP_H
#define HAVE_RTSP_RTP_H

#include "rtsp.h"

namespace mpxp {
#define MAX_PREVIEW_SIZE 4096
    class Rtp_Rtsp_Session : public Opaque {
	public:
	    Rtp_Rtsp_Session();
	    virtual ~Rtp_Rtsp_Session();

	    static Rtp_Rtsp_Session*	setup_and_play(Rtsp& rtsp_session);
	    virtual void		rtcp_send_rr (Rtsp& s);
	    virtual int			get_rtp_socket() const;
	private:
	    void	set_fd (int rtp_sock, int rtcp_sock);
	    int		parse_port (const char *line, const char *param, int *rtp_port, int *rtcp_port) const;
	    char*	parse_destination (const char *line) const;
	    int		rtcp_connect (int client_port, int server_port, const char* server_hostname) const;
	    int		rtp_connect (const char *hostname, int port) const;
	    int		is_multicast_address (const char *addr) const;

	    int		rtp_socket;
	    int		rtcp_socket;
	    char*	control_url;
	    int		count;
	    int		rtsp_port;
	    char*	rtsp_destination;
    };
} // namespace mpxp
#endif /* HAVE_RTSP_RTP_H */

