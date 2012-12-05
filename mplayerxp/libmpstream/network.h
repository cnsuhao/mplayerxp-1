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

typedef enum {
    streaming_stopped_e,
    streaming_playing_e
} streaming_status;

typedef struct streaming_control {
    URL_t *url;
    streaming_status status;
    int buffering;	// boolean
    unsigned int prebuffer_size;
    char *buffer;
    unsigned int buffer_size;
    unsigned int buffer_pos;
    unsigned int bandwidth;	// The downstream available
    int (*streaming_read)( int fd, char *buffer, int buffer_size, struct streaming_control *stream_ctrl );
    int (*streaming_seek)( int fd, off_t pos, struct streaming_control *stream_ctrl );
    any_t*data;
    libinput_t* libinput;   /**< provides possibility to inperrupt network streams */
} streaming_ctrl_t;

struct stream_t;
extern void fixup_network_stream_cache(stream_t *s);
extern int streaming_start(libinput_t* libinput,stream_t *stream, int *demuxer_type, URL_t *url);
extern int streaming_bufferize( streaming_ctrl_t *streaming_ctrl,unsigned char *buffer, int size);
extern streaming_ctrl_t *streaming_ctrl_new(libinput_t* libinput);
extern void streaming_ctrl_free( streaming_ctrl_t *streaming_ctrl );
extern URL_t* check4proxies( URL_t *url );

int nop_streaming_read( int fd, char *buffer, int size, streaming_ctrl_t *stream_ctrl );
int nop_streaming_seek( int fd, off_t pos, streaming_ctrl_t *stream_ctrl );

int http_send_request(libinput_t* libinput,URL_t *url, off_t pos);
HTTP_header_t *http_read_response(int fd);

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
