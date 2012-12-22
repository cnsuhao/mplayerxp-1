#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
/*
 *  Copyright (C) 2006 Benjamin Zores
 *   Network helpers for UDP connections (originally borrowed from rtp.c).
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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <ctype.h>

#ifndef HAVE_WINSOCK2
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#define closesocket close
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include "url.h"
#include "udp.h"
#include "stream_msg.h"

namespace mpxp {
/* Start listening on a UDP port. If multicast, join the group. */
void Udp::open(const URL& url,int reuse_socket)
{
    int socket_server_fd, rxsockbufsz;
    int err;
    socklen_t err_len;
    fd_set set;
    struct sockaddr_in server_address;
    struct ip_mreq mcast;
    struct timeval tv;
    struct hostent *hp;
    int reuse=reuse_socket;

    mpxp_v<<"[udp] Listening for traffic on "<<url.host()<<":"<<url.port()<<std::endl;

    _fd = ::socket (AF_INET, SOCK_DGRAM, 0);
    if (socket_server_fd == -1) {
	mpxp_err<<"[udp] Failed to create socket"<<std::endl;
	return;
    }

    if (::isalpha (url.host()[0])) {
#ifndef HAVE_WINSOCK2
	hp = (struct hostent *) ::gethostbyname (url.host().c_str());
	if (!hp) {
	    mpxp_err<<"[udp] Counldn't resolve name: "<<url.host()<<std::endl;
	    ::closesocket (_fd);
	    _fd = -1;
	    return;
	}
	memcpy ((any_t*) &server_address.sin_addr.s_addr,
		(any_t*) hp->h_addr_list[0], hp->h_length);
#else
	server_address.sin_addr.s_addr = htonl (INADDR_ANY);
#endif /* HAVE_WINSOCK2 */
    } else {
#ifndef HAVE_WINSOCK2
#ifdef USE_ATON
	inet_aton (url.host().c_str(), &server_address.sin_addr);
#else
	inet_pton (AF_INET, url.host().c_str(), &server_address.sin_addr);
#endif /* USE_ATON */
#else
	server_address.sin_addr.s_addr = htonl(INADDR_ANY);
#endif /* HAVE_WINSOCK2 */
    }
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons (url.port());

    if(reuse_socket && ::setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)))
	mpxp_err<<"[udp] SO_REUSEADDR failed! ignore"<<std::endl;

    if (::bind (_fd, (struct sockaddr *) &server_address,
		sizeof (server_address)) == -1)  {
#ifndef HAVE_WINSOCK2
	if (errno != EINPROGRESS) {
#else
	if (WSAGetLastError () != WSAEINPROGRESS) {
#endif /* HAVE_WINSOCK2 */
	    mpxp_err<<"[udp] Failed to connect to server"<<std::endl;
	    ::closesocket (_fd);
	    _fd = -1;
	    return;
	}
    }

#ifdef HAVE_WINSOCK2
    if (::isalpha (url.host()[0])) {
	hp = (struct hostent *) ::gethostbyname (url.host().c_str());
	if (!hp) {
	    mpxp_err<<"[udp] Could not resolve name: "<<url.host()<<std::endl;
	    ::closesocket (_fd);
	    _fd = -1;
	    return;
	}
	memcpy ((any_t*) &server_address.sin_addr.s_addr,
		(any_t*) hp->h_addr, hp->h_length);
    } else {
	unsigned int addr = inet_addr (url.host().c_str());
	memcpy ((any_t*) &server_address.sin_addr, (any_t*) &addr, sizeof (addr));
    }
#endif /* HAVE_WINSOCK2 */
    /* Increase the socket rx buffer size to maximum -- this is UDP */
    rxsockbufsz = 240 * 1024;
    if (setsockopt (socket_server_fd, SOL_SOCKET, SO_RCVBUF,
		  &rxsockbufsz, sizeof (rxsockbufsz))) {
	mpxp_err<<"[udp] Couldn't set receive socket buffer size"<<std::endl;
    }
    if ((ntohl (server_address.sin_addr.s_addr) >> 28) == 0xe) {
	mcast.imr_multiaddr.s_addr = server_address.sin_addr.s_addr;
	mcast.imr_interface.s_addr = 0;

	if (::setsockopt (_fd, IPPROTO_IP,
			IP_ADD_MEMBERSHIP, &mcast, sizeof (mcast))) {
	    mpxp_err<<"[udp] IP_ADD_MEMBERSHIP failed (do you have multicasting enabled in your kernel?)"<<std::endl;
	    ::closesocket (_fd);
	    _fd = -1;
	    return;
	}
    }

    tv.tv_sec = 1; /* 1 second timeout */
    tv.tv_usec = 0;

    FD_ZERO (&set);
    FD_SET (_fd, &set);

    err = ::select (_fd + 1, &set, NULL, NULL, &tv);
    if (err < 0) {
	mpxp_fatal<<"[udp] Select failed: "<<strerror (errno)<<std::endl;
	::closesocket (_fd);
	_fd = -1;
	return;
    }

    if (err == 0) {
	mpxp_err<<"[udp] Timeout! No data from host "<<url.host()<<std::endl;
	::closesocket (_fd);
	_fd = -1;
	return;
    }

    err_len = sizeof (err);
    ::getsockopt (_fd, SOL_SOCKET, SO_ERROR, &err, &err_len);
    if (err) {
	mpxp_dbg2<<"[udp] Socket error: "<<err<<std::endl;
	::closesocket (_fd);
	_fd = -1;
	return;
    }
}
Udp::Udp(const URL& url,int reuse_socket)
    :_fd(-1)
    ,_error(0)
{
    open(url,reuse_socket);
}
Udp::Udp(net_fd_t fd)
    :_fd(fd)
    ,_error(0)
{
}
Udp::~Udp() {}
int	Udp::established() const { return _fd>0; }
} // namespace mpxp