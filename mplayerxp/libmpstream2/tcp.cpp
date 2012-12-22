#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
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

#include "mpxp_help.h"
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
#include "stream_msg.h"

namespace mpxp {

// Converts an address family constant to a string
static const char *af2String(Tcp::tcp_af_e af) {
	switch (af) {
		case Tcp::IP4:	return "IPv4";
		case Tcp::IP6:	return "IPv6";
		default:	return "Unknown address family!";
	}
}


// Connect to a server using a TCP connection, with specified address family
// return -2 for fatal error, like unable to resolve name, connection timeout...
// return -1 is unable to connect to a particular port

void Tcp::open(const URL& __url, tcp_af_e af) {
    socklen_t err_len;
    int ret,count = 0;
    fd_set set;
    struct timeval tv;
    union {
	struct sockaddr_in four;
	struct sockaddr_in6 six;
    } server_address;
    size_t server_address_size;
    any_t*our_s_addr;	// Pointer to sin_addr or sin6_addr
    struct hostent *hp=NULL;
    char buf[255];
    _url=__url;

#ifdef HAVE_WINSOCK2
    u_long val;
    int to;
#else
    struct timeval to;
#endif

    buf[0]=0;
    mpxp_v<<"[tcp"<<af2String(af)<<"] Trying to resolv host: "<<_url.host()<<std::endl;
    _fd = ::socket(af==Tcp::IP4?AF_INET:AF_INET6, SOCK_STREAM, 0);

    if( _fd==-1 ) {
	_error=Tcp::Err_Fatal;
	return;
    }

#if defined(SO_RCVTIMEO) && defined(SO_SNDTIMEO)
#ifdef HAVE_WINSOCK2
    /* timeout in milliseconds */
    to = 10 * 1000;
#else
    to.tv_sec = 10;
    to.tv_usec = 0;
#endif
    ::setsockopt(_fd, SOL_SOCKET, SO_RCVTIMEO, &to, sizeof(to));
    ::setsockopt(_fd, SOL_SOCKET, SO_SNDTIMEO, &to, sizeof(to));
#endif

    switch (af) {
	case Tcp::IP4: our_s_addr = (any_t*) &server_address.four.sin_addr; break;
	case Tcp::IP6: our_s_addr = (any_t*) &server_address.six.sin6_addr; break;
	default:
	    mpxp_err<<"[tcp"<<af2String(af)<<"] UnknownAF: "<<af<<std::endl;
	    _error=Tcp::Err_Fatal;
	    return;
    }

    memset(&server_address, 0, sizeof(server_address));

    mpxp_v<<"[tcp"<<af2String(af)<<"] PreResolving Host "<<_url.host()<<std::endl;
#ifndef HAVE_WINSOCK2
#ifdef USE_ATON
    if (::inet_aton(_url.host().c_str(), our_s_addr)!=1)
#else
    if (::inet_pton(af==Tcp::IP4?AF_INET:AF_INET6, _url.host().c_str(), our_s_addr)!=1)
#endif
#else
    if (::inet_addr(_url.host().c_str())==INADDR_NONE )
#endif
    {
	mpxp_v<<"[tcp"<<af2String(af)<<"] Resolving Host: "<<_url.host()<<std::endl;

#ifdef HAVE_GETHOSTBYNAME2
	hp=(struct hostent*)::gethostbyname2( _url.host().c_str(), af==Tcp::IP4?AF_INET:AF_INET6 );
#else
	hp=(struct hostent*)::gethostbyname( _url.host().c_str() );
#endif
	if( hp==NULL ) {
	    mpxp_v<<"[tcp"<<af2String(af)<<"] Can't resolv: "<<_url.host()<<std::endl;
	    _error=Tcp::Err_Fatal;
	    return;
	}

	memcpy( our_s_addr, (any_t*)hp->h_addr_list[0], hp->h_length );
    }
#ifdef HAVE_WINSOCK2
    else {
	unsigned long addr = inet_addr(_url.host().c_str());
	memcpy( our_s_addr, (any_t*)&addr, sizeof(addr) );
    }
#endif

    switch (af) {
	case Tcp::IP4:
	    server_address.four.sin_family=AF_INET;
	    server_address.four.sin_port=htons(_url.port());
	    server_address_size = sizeof(server_address.four);
	    break;
	case Tcp::IP6:
	    server_address.six.sin6_family=AF_INET6;
	    server_address.six.sin6_port=htons(_url.port());
	    server_address_size = sizeof(server_address.six);
	    break;
	default:
	    mpxp_err<<"[tcp"<<af2String(af)<<"] UnknownAF: "<<af<<std::endl;
	    _error = Tcp::Err_Fatal;
	    return;
    }

#if defined(USE_ATON) || defined(HAVE_WINSOCK2)
    strncpy( buf, ::inet_ntoa( *((struct in_addr*)our_s_addr) ), 255);
#else
    ::inet_ntop(af==Tcp::IP4?AF_INET:AF_INET6, our_s_addr, buf, 255);
#endif
    mpxp_info<<"[tcp"<<af2String(af)<<"] Connecting to server: "<<_url.host()<<" ("<<(const char*)buf<<":"<<_url.port()<<")"<<std::endl;

    // Turn the socket as non blocking so we can timeout on the connection
#ifndef HAVE_WINSOCK2
    ::fcntl( _fd, F_SETFL, ::fcntl(_fd, F_GETFL) | O_NONBLOCK );
#else
    val = 1;
    ::ioctlsocket( _fd, FIONBIO, &val );
#endif
    if(::connect( _fd, (struct sockaddr*)&server_address, server_address_size )==-1 ) {
#ifndef HAVE_WINSOCK2
	if( errno!=EINPROGRESS ) {
#else
	if( (WSAGetLastError() != WSAEINPROGRESS) && (WSAGetLastError() != WSAEWOULDBLOCK) ) {
#endif
	    mpxp_v<<"[tcp"<<af2String(af)<<"] Can't connect to server: "<<_url.host()<<std::endl;
	    ::closesocket(_fd);
	    _fd=-1;
	    _error=Tcp::Err_Port;
	    return;
	}
    }
    tv.tv_sec = 0;
    tv.tv_usec = 500000;
    FD_ZERO( &set );
    FD_SET( _fd, &set );
    // When the connection will be made, we will have a writeable fd
    while((ret = ::select(FD_SETSIZE, NULL, &set, NULL, &tv)) == 0) {
	if(count > 30 || mp_input_check_interrupt(libinput,500)==MPXP_Ok) {
	    if(count > 30)
		mpxp_err<<"[tcp"<<af2String(af)<<"] Connecting timeout"<<std::endl;
	    else
		mpxp_v<<"[tcp"<<af2String(af)<<"] Connection interrupted by user"<<std::endl;
	    _error=Tcp::Err_Timeout;
	    return;
	}
	count++;
	FD_ZERO( &set );
	FD_SET( _fd, &set );
	tv.tv_sec = 0;
	tv.tv_usec = 500000;
    }
    if (ret < 0) mpxp_err<<"[tcp"<<af2String(af)<<"] Select failed"<<std::endl;

    // Turn back the socket as blocking
#ifndef HAVE_WINSOCK2
    ::fcntl( _fd, F_SETFL, ::fcntl(_fd, F_GETFL) & ~O_NONBLOCK );
#else
    val = 0;
    ::ioctlsocket( _fd, FIONBIO, &val );
#endif
    // Check if there were any errors
    err_len = sizeof(int);
    ret = ::getsockopt(_fd,SOL_SOCKET,SO_ERROR,&_error,&err_len);
    if(ret < 0) {
	mpxp_err<<"[tcp"<<af2String(af)<<"] Get socket option failed: "<<strerror(errno)<<std::endl;
	_error=Tcp::Err_Fatal;
	return;
    }
    if(_error > 0) {
	mpxp_err<<"[tcp"<<af2String(af)<<"] Connection error: "<<strerror(_error)<<std::endl;
	_error=Tcp::Err_Port;
	return;
    }
}

Tcp::Tcp(libinput_t& _libinput,const URL& url,tcp_af_e af)
    :_fd(-1),
    _error(Tcp::Err_None),
    libinput(_libinput)
{
    open(url,af);
}

Tcp::Tcp(libinput_t& _libinput,net_fd_t fd)
    :_fd(fd),
    _error(Tcp::Err_None),
    libinput(_libinput)
{
}

Tcp::~Tcp() { if(_fd>0) close(); }

void Tcp::close() { if(_fd>0) ::closesocket(_fd); _fd=-1; }

int Tcp::has_data(int timeout) const {
    fd_set fds;
    struct timeval tv;

    FD_ZERO(&fds);
    FD_SET(_fd,&fds);
    tv.tv_sec = timeout;
    tv.tv_usec = 0;

    return (::select(_fd+1, &fds, NULL, NULL, &tv) > 0);
}

Tcp& Tcp::operator=(Tcp& other) {
    _fd = other._fd; _error=other._error;
    _url=other._url;
    other._fd = 0;
    return *this;
}
Tcp& Tcp::operator=(net_fd_t fd) { _url.redirect(""); _fd=fd; _error=Tcp::Err_None; return *this; }

int Tcp::read(uint8_t* buf,unsigned len,int flags) { return ::recv(_fd,buf,len,flags); }
int Tcp::write(const uint8_t* buf,unsigned len,int flags) const { return ::send(_fd,buf,len,flags); }
int Tcp::established() const { return _fd > 0; }
Tcp::tcp_error_e Tcp::error() const { return _error; }
libinput_t& Tcp::get_libinput() const { return libinput; }
const URL& Tcp::url() const { return _url; }
} // namespace mpxp
