/*
 * Network layer for MPlayer
 * by Bertrand BAUDET <bertrand_baudet@yahoo.com>
 * (C) 2001, MPlayer team.
 */
#ifndef __NETWORK_H
#define __NETWORK_H
#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;

#include <string>

#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include "mp_config.h"

#ifndef HAVE_WINSOCK2
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#endif

#include "url.h"
#include "http.h"
#include "stream.h"

#define BUFFER_SIZE		2048

namespace mpxp {
    struct Stream;
    struct libinput_t;
    class Tcp;

    struct net_config_t {
	public:
	    net_config_t();
	    virtual ~net_config_t();

	    char*	username;
	    char*	password;
	    int		bandwidth;
	    int		cookies_enabled;
	    char*	cookies_file;
	    char*	useragent;
/* IPv6 options */
	    int		prefer_ipv4;
	    int		ipv4_only_proxy;
    };
    extern net_config_t net_conf;

    enum networking_status {
	networking_stopped_e,
	networking_playing_e
    };

    struct network_protocol_t {
	URL	url;
	std::string mime;
	Opaque*	data;
    };

    struct Networking : public Opaque {
	public:
	    virtual ~Networking();

	    static Networking*	start(Tcp& tcp,const URL& url);
	    virtual int		stop();
	    virtual void	fixup_cache();
	    virtual int		bufferize(unsigned char *buffer, int size);

	    virtual int read( Tcp& fd, char *buffer, int buffer_size) = 0;
	    virtual int seek( Tcp& fd, off_t pos) = 0;

	    std::string		mime;
	    URL			url;
	    networking_status	status;
	    unsigned int	bandwidth;	// The downstream available
	protected:
	    Networking();
	    unsigned int	prebuffer_size;
	    int			buffering; // boolean
	    char*		buffer;
	    unsigned int	buffer_size;
	    unsigned int	buffer_pos;
	    Opaque*		data;
	private:
	    static MPXP_Rc	autodetectProtocol(network_protocol_t& protocol,Tcp& tcp);
    };

    MPXP_Rc http_send_request(Tcp& tcp,URL& url, off_t pos);
    HTTP_Header* http_read_response(Tcp& fd);

/*
 * Joey Parrish <joey@yunamusic.com>:
 *
 * This define is to allow systems without inet_pton() to fallback on
 * inet_aton().  The difference between the two is that inet_aton() is
 * strictly for IPv4 networking, while inet_pton() is for IPv4 and IPv6
 * both.  Slightly limited network functionality seems better than no
 * network functionality to me, and as all systems (Cygwin) start to
 * implement inet_pton(), configure will decide not to use this code.
 */
#ifdef USE_ATON
    inline int inet_pton(int a, const char* b, any_t* c) { return inet_aton(b, c); }
#endif
} // namespace mpxp
#endif
