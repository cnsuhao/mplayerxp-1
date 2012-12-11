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
}

struct net_config_t {
    public:
	net_config_t();
	virtual ~net_config_t();

	char*	username;
	char*	password;
	int	bandwidth;
	int	cookies_enabled;
	char*	cookies_file;
	char*	useragent;
/* IPv6 options */
	int	prefer_ipv4;
	int	ipv4_only_proxy;
};
extern net_config_t net_conf;

enum networking_status {
    networking_stopped_e,
    networking_playing_e
};

struct networking_t {
    URL *url;
    std::string mime;
    networking_status status;
    int buffering;	// boolean
    unsigned int prebuffer_size;
    char *buffer;
    unsigned int buffer_size;
    unsigned int buffer_pos;
    unsigned int bandwidth;	// The downstream available
    int (*networking_read)( Tcp& fd, char *buffer, int buffer_size, networking_t *stream_ctrl );
    int (*networking_seek)( Tcp& fd, off_t pos, networking_t *stream_ctrl );
    Opaque* data;
};

extern void fixup_network_stream_cache(networking_t *s);
extern MPXP_Rc networking_start(Tcp& fd,networking_t *n, URL *url);
extern int networking_bufferize(networking_t *networking,unsigned char *buffer, int size);
extern networking_t *new_networking();
extern void free_networking( networking_t *networking );
extern URL* check4proxies( URL* url );

int nop_networking_read(Tcp& fd, char *buffer, int size, networking_t *stream_ctrl );
int nop_networking_seek(Tcp& fd, off_t pos, networking_t *stream_ctrl );

MPXP_Rc http_send_request(Tcp& tcp,URL* url, off_t pos);
HTTP_Header* http_read_response(Tcp& fd);

int http_authenticate(HTTP_Header& http_hdr, URL* url, int *auth_retry);

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
# define inet_pton(a, b, c) inet_aton(b, c)
#endif

#endif
