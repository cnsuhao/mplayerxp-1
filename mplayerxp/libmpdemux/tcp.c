/*
 * Network layer for MPlayer
 * by Bertrand BAUDET <bertrand_baudet@yahoo.com>
 * (C) 2001, MPlayer team.
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <errno.h>
#include <ctype.h>

#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>

#include "mp_config.h"

#include "help_mp.h"
#include "input2/input.h"

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

#include "tcp.h"
#include "demux_msg.h"

/* IPv6 options */
extern int   network_prefer_ipv4;

#define IP_NAME (network_prefer_ipv4?"ipv4":"ipv6")

// Converts an address family constant to a string

static const char *af2String(int af) {
	switch (af) {
		case AF_INET:	return "AF_INET";
		
#ifdef HAVE_AF_INET6
		case AF_INET6:	return "AF_INET6";
#endif
		default:	return "Unknown address family!";
	}
}


// Connect to a server using a TCP connection, with specified address family
// return -2 for fatal error, like unable to resolve name, connection timeout...
// return -1 is unable to connect to a particular port

static int
connect2Server_with_af(any_t* libinput,const char *host, int port, int af,int verb) {
	int socket_server_fd;
	int err;
	socklen_t err_len;
	int ret,count = 0;
	fd_set set;
	struct timeval tv;
	union {
		struct sockaddr_in four;
#ifdef HAVE_AF_INET6
		struct sockaddr_in6 six;
#endif
	} server_address;
	size_t server_address_size;
	any_t*our_s_addr;	// Pointer to sin_addr or sin6_addr
	struct hostent *hp=NULL;
	char buf[255];

#ifdef HAVE_WINSOCK2
	u_long val;
	int to;
#else
	struct timeval to;
#endif
	
	MSG_V("[tcp%s] Trying to resolv host '%s' For AF %s\n", IP_NAME, host, af2String(af));
	socket_server_fd = socket(af, SOCK_STREAM, 0);

	if( socket_server_fd==-1 ) {
		return TCP_ERROR_FATAL;
	}

#if defined(SO_RCVTIMEO) && defined(SO_SNDTIMEO)
#ifdef HAVE_WINSOCK2
	/* timeout in milliseconds */
	to = 10 * 1000;
#else
	to.tv_sec = 10;
	to.tv_usec = 0;
#endif
	setsockopt(socket_server_fd, SOL_SOCKET, SO_RCVTIMEO, &to, sizeof(to));
	setsockopt(socket_server_fd, SOL_SOCKET, SO_SNDTIMEO, &to, sizeof(to));
#endif

	switch (af) {
		case AF_INET:  our_s_addr = (any_t*) &server_address.four.sin_addr; break;
#ifdef HAVE_AF_INET6
		case AF_INET6: our_s_addr = (any_t*) &server_address.six.sin6_addr; break;
#endif
		default:
			MSG_ERR("[tcp%s] UnknownAF: %i\n", IP_NAME, af);
			return TCP_ERROR_FATAL;
	}

	memset(&server_address, 0, sizeof(server_address));

	MSG_V("[tcp%s] PreResolving Host '%s' For AF %s\n", IP_NAME, host, af2String(af));
#ifndef HAVE_WINSOCK2
#ifdef USE_ATON
	if (inet_aton(host, our_s_addr)!=1)
#else
	if (inet_pton(af, host, our_s_addr)!=1)
#endif
#else
	if ( inet_addr(host)==INADDR_NONE )
#endif
	{
		MSG_V("[tcp%s] Resolving Host '%s' For AF %s\n", IP_NAME, host, af2String(af));

#ifdef HAVE_GETHOSTBYNAME2
		hp=(struct hostent*)gethostbyname2( host, af );
#else
		hp=(struct hostent*)gethostbyname( host );
#endif
		if( hp==NULL ) {
			MSG_V("[tcp%s] Can't resolv: %s '%s'\n", IP_NAME, af2String(af), host);
			return TCP_ERROR_FATAL;
		}
		
		memcpy( our_s_addr, (any_t*)hp->h_addr_list[0], hp->h_length );
	}
#ifdef HAVE_WINSOCK2
	else {
		unsigned long addr = inet_addr(host);
		memcpy( our_s_addr, (any_t*)&addr, sizeof(addr) );
	}
#endif

	switch (af) {
		case AF_INET:
			server_address.four.sin_family=af;
			server_address.four.sin_port=htons(port);
			server_address_size = sizeof(server_address.four);
			break;
#ifdef HAVE_AF_INET6
		case AF_INET6:
			server_address.six.sin6_family=af;
			server_address.six.sin6_port=htons(port);
			server_address_size = sizeof(server_address.six);
			break;
#endif
		default:
			MSG_ERR("[tcp%s] UnknownAF: %i\n", IP_NAME, af);
			return TCP_ERROR_FATAL;
	}

#if defined(USE_ATON) || defined(HAVE_WINSOCK2)
	strncpy( buf, inet_ntoa( *((struct in_addr*)our_s_addr) ), 255);
#else
	inet_ntop(af, our_s_addr, buf, 255);
#endif
	MSG_INFO("[tcp%s] Connecting to server: %s (%s:%i)\n", IP_NAME, host, buf , port );

	// Turn the socket as non blocking so we can timeout on the connection
#ifndef HAVE_WINSOCK2
	fcntl( socket_server_fd, F_SETFL, fcntl(socket_server_fd, F_GETFL) | O_NONBLOCK );
#else
	val = 1;
	ioctlsocket( socket_server_fd, FIONBIO, &val );
#endif
	if( connect( socket_server_fd, (struct sockaddr*)&server_address, server_address_size )==-1 ) {
#ifndef HAVE_WINSOCK2
		if( errno!=EINPROGRESS ) {
#else
		if( (WSAGetLastError() != WSAEINPROGRESS) && (WSAGetLastError() != WSAEWOULDBLOCK) ) {
#endif
			MSG_V("[tcp%s] Can't connect to server: %s\n", IP_NAME, af2String(af));
			closesocket(socket_server_fd);
			return TCP_ERROR_PORT;
		}
	}
	tv.tv_sec = 0;
	tv.tv_usec = 500000;
	FD_ZERO( &set );
	FD_SET( socket_server_fd, &set );
	// When the connection will be made, we will have a writeable fd
	while((ret = select(FD_SETSIZE, NULL, &set, NULL, &tv)) == 0) {
	      if(count > 30 || mp_input_check_interrupt(libinput,500)==MPXP_Ok) {
		if(count > 30)
		  MSG_ERR("[tcp%s] Connecting timeout\n",IP_NAME);
		else
		  MSG_V("[tcp%s] Connection interrupted by user\n",IP_NAME);
		return TCP_ERROR_TIMEOUT;
	      }
	      count++;
	      FD_ZERO( &set );
	      FD_SET( socket_server_fd, &set );
	      tv.tv_sec = 0;
	      tv.tv_usec = 500000;
	}
	if (ret < 0) MSG_ERR("[tcp%s] Select failed\n",IP_NAME);

	// Turn back the socket as blocking
#ifndef HAVE_WINSOCK2
	fcntl( socket_server_fd, F_SETFL, fcntl(socket_server_fd, F_GETFL) & ~O_NONBLOCK );
#else
	val = 0;
	ioctlsocket( socket_server_fd, FIONBIO, &val );
#endif
	// Check if there were any errors
	err_len = sizeof(int);
	ret =  getsockopt(socket_server_fd,SOL_SOCKET,SO_ERROR,&err,&err_len);
	if(ret < 0) {
		MSG_ERR("[tcp%s] Get socket option failed: %s\n",IP_NAME,strerror(errno));
		return TCP_ERROR_FATAL;
	}
	if(err > 0) {
		MSG_ERR("[tcp%s] Connection error: %s\n",IP_NAME,strerror(err));
		return TCP_ERROR_PORT;
	}

    return socket_server_fd;
}

// Connect to a server using a TCP connection
// return -2 for fatal error, like unable to resolve name, connection timeout...
// return -1 is unable to connect to a particular port


int tcp_connect2Server(any_t* libinput,const char *host, int  port, int verb) {
#ifdef HAVE_AF_INET6
    return connect2Server_with_af(libinput,host, port, network_prefer_ipv4 ? AF_INET:AF_INET6,verb);
#else
    network_prefer_ipv4=1;
    return connect2Server_with_af(libinput,host, port, AF_INET,verb);
#endif

}
