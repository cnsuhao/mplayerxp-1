/*
 * Network layer for MPlayer
 * by Bertrand BAUDET <bertrand_baudet@yahoo.com>
 * (C) 2001, MPlayer team.
 */

#ifndef __NETWORK_H
#define __NETWORK_H

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
    struct stream_t;
    struct libinput_t;
}
typedef int net_fd_t;
enum networking_status {
    networking_stopped_e,
    networking_playing_e
};

struct networking_t {
    URL_t *url;
    networking_status status;
    int buffering;	// boolean
    unsigned int prebuffer_size;
    char *buffer;
    unsigned int buffer_size;
    unsigned int buffer_pos;
    unsigned int bandwidth;	// The downstream available
    int (*networking_read)( net_fd_t fd, char *buffer, int buffer_size, networking_t *stream_ctrl );
    int (*networking_seek)( net_fd_t fd, off_t pos, networking_t *stream_ctrl );
    any_t*data;
    libinput_t* libinput;   /**< provides possibility to inperrupt network streams */
};

extern void fixup_network_stream_cache(networking_t *s);
extern int networking_start(net_fd_t* fd,networking_t *n, URL_t *url);
extern int networking_bufferize(networking_t *networking,unsigned char *buffer, int size);
extern networking_t *new_networking(libinput_t* libinput);
extern void free_networking( networking_t *networking );
extern URL_t* check4proxies( URL_t *url );

int nop_networking_read( net_fd_t fd, char *buffer, int size, networking_t *stream_ctrl );
int nop_networking_seek( net_fd_t fd, off_t pos, networking_t *stream_ctrl );

int http_send_request(libinput_t* libinput,URL_t *url, off_t pos);
HTTP_header_t *http_read_response(net_fd_t fd);

int http_authenticate(HTTP_header_t *http_hdr, URL_t *url, int *auth_retry);

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
