/*
 * Network layer for MPlayer
 * by Bertrand BAUDET <bertrand_baudet@yahoo.com>
 * (C) 2001, MPlayer team.
 */

//#define DUMP2FILE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

#include <errno.h>
#include <ctype.h>

#include "config.h"
#include "../mplayer.h"
#ifndef HAVE_WINSOCK2
#define closesocket close
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include "stream.h"
#include "demuxer.h"
#include "../cfgparser.h"
#include "mpdemux.h"
#include "../help_mp.h"

#include "tcp.h"
#include "network.h"
#include "http.h"
#include "cookies.h"
#include "url.h"
#include "udp.h"
#include "asf.h"
#include "pnm.h"
#ifndef STREAMING_LIVE_DOT_COM
#include "rtp.h"
#endif
#include "demux_msg.h"
#include "../version.h"

/* Variables for the command line option -user, -passwd & -bandwidth */
char *network_username=NULL;
char *network_password=NULL;
int   network_bandwidth=0;
int   network_cookies_enabled = 0;
char *network_useragent=NULL;

/* IPv6 options */
int   network_prefer_ipv4 = 1;
int   network_ipv4_only_proxy = 0;

static struct {
	char *mime_type;
	int demuxer_type;
} mime_type_table[] = {
	// MP3 streaming, some MP3 streaming server answer with audio/mpeg
	{ "audio/mpeg", DEMUXER_TYPE_AUDIO },
	// MPEG streaming
	{ "video/mpeg", DEMUXER_TYPE_UNKNOWN },
	{ "video/x-mpeg", DEMUXER_TYPE_UNKNOWN },
	{ "video/x-mpeg2", DEMUXER_TYPE_UNKNOWN },
	// AVI ??? => video/x-msvideo
	{ "video/x-msvideo", DEMUXER_TYPE_AVI },
	// MOV => video/quicktime
	{ "video/quicktime", DEMUXER_TYPE_MOV },
	// ASF
        { "audio/x-ms-wax", DEMUXER_TYPE_ASF },
	{ "audio/x-ms-wma", DEMUXER_TYPE_ASF },
	{ "video/x-ms-asf", DEMUXER_TYPE_ASF },
	{ "video/x-ms-afs", DEMUXER_TYPE_ASF },
	{ "video/x-ms-wvx", DEMUXER_TYPE_ASF },
	{ "video/x-ms-wmv", DEMUXER_TYPE_ASF },
	{ "video/x-ms-wma", DEMUXER_TYPE_ASF },
	// Playlists
	{ "video/x-ms-wmx", DEMUXER_TYPE_PLAYLIST },
	{ "audio/x-scpls", DEMUXER_TYPE_PLAYLIST },
	{ "audio/x-mpegurl", DEMUXER_TYPE_PLAYLIST },
	{ "audio/x-pls", DEMUXER_TYPE_PLAYLIST },
	// Real Media
	{ "audio/x-pn-realaudio", DEMUXER_TYPE_REAL },
	// OGG Streaming
	{ "application/x-ogg", DEMUXER_TYPE_OGG },
	// NullSoft Streaming Video
	{ "video/nsv", DEMUXER_TYPE_NSV},
	{ "misc/ultravox", DEMUXER_TYPE_NSV}
};

streaming_ctrl_t * streaming_ctrl_new(void) {
	streaming_ctrl_t *streaming_ctrl;
	streaming_ctrl = (streaming_ctrl_t*)malloc(sizeof(streaming_ctrl_t));
	if( streaming_ctrl==NULL ) {
		MSG_FATAL(MSGTR_OutOfMemory);
		return NULL;
	}
	memset( streaming_ctrl, 0, sizeof(streaming_ctrl_t) );
	return streaming_ctrl;
}

void streaming_ctrl_free( streaming_ctrl_t *streaming_ctrl ) {
	if( streaming_ctrl==NULL ) return;
	if( streaming_ctrl->url ) url_free( streaming_ctrl->url );
	if( streaming_ctrl->buffer ) free( streaming_ctrl->buffer );
	if( streaming_ctrl->data ) free( streaming_ctrl->data );
	free( streaming_ctrl );
}

URL_t*
check4proxies( URL_t *url ) {
	URL_t *url_out = NULL;
	if( url==NULL ) return NULL;
	url_out = url_new( url->url );
	if( !strcasecmp(url->protocol, "http_proxy") ) {
		MSG_V("Using HTTP proxy: http://%s:%d\n", url->hostname, url->port );
		return url_out;
	}
	// Check if the http_proxy environment variable is set.
	if( !strcasecmp(url->protocol, "http") ) {
		char *proxy;
		proxy = getenv("http_proxy");
		if( proxy!=NULL ) {
			// We got a proxy, build the URL to use it
			int len;
			char *new_url;
			URL_t *tmp_url;
			URL_t *proxy_url = url_new( proxy );

			if( proxy_url==NULL ) {
				MSG_WARN("Invalid proxy setting...Trying without proxy.\n");
				return url_out;
			}

#ifdef HAVE_AF_INET6
			if (network_ipv4_only_proxy && (gethostbyname(url->hostname)==NULL)) {
				MSG_WARN(
					"Could not find resolve remote hostname for AF_INET. Trying without proxy.\n");
				return url_out;
			}
#endif

			MSG_V("Using HTTP proxy: %s\n", proxy_url->url );
			len = strlen( proxy_url->hostname ) + strlen( url->url ) + 20;	// 20 = http_proxy:// + port
			new_url = malloc( len+1 );
			if( new_url==NULL ) {
				MSG_FATAL(MSGTR_OutOfMemory);
				return url_out;
			}
			sprintf(new_url, "http_proxy://%s:%d/%s", proxy_url->hostname, proxy_url->port, url->url );
			tmp_url = url_new( new_url );
			if( tmp_url==NULL ) {
				return url_out;
			}
			url_free( url_out );
			url_out = tmp_url;
			free( new_url );
			url_free( proxy_url );
		}
	}
	return url_out;
}

int
http_send_request( URL_t *url, off_t pos ) {
	HTTP_header_t *http_hdr;
	URL_t *server_url;
	char str[256];
	int fd=-1;
	int ret;
	int proxy = 0;		// Boolean

	http_hdr = http_new_header();

	if( !strcasecmp(url->protocol, "http_proxy") ) {
		proxy = 1;
		server_url = url_new( (url->file)+1 );
		http_set_uri( http_hdr, server_url->url );
	} else {
		server_url = url;
		http_set_uri( http_hdr, server_url->file );
	}
	if (server_url->port && server_url->port != 80)
	    snprintf(str, 256, "Host: %s:%d", server_url->hostname, server_url->port );
	else
	    snprintf(str, 256, "Host: %s", server_url->hostname );
	http_set_field( http_hdr, str);
	if (network_useragent)
	{
	    snprintf(str, 256, "User-Agent: %s", network_useragent);
	    http_set_field(http_hdr, str);
	}
	else
	    http_set_field( http_hdr, "User-Agent: MPlayerXP/"VERSION);

	http_set_field(http_hdr, "Icy-MetaData: 1");

	if(pos>0) { 
	// Extend http_send_request with possibility to do partial content retrieval
	    snprintf(str, 256, "Range: bytes=%d-", (int)pos);
	    http_set_field(http_hdr, str);
	}
	    
	if (network_cookies_enabled) cookies_set( http_hdr, server_url->hostname, server_url->url );

	http_set_field( http_hdr, "Connection: closed");
	http_add_basic_authentication( http_hdr, url->username, url->password );
	if( http_build_request( http_hdr )==NULL ) {
		goto err_out;
	}

	if( proxy ) {
		if( url->port==0 ) url->port = 8080;			// Default port for the proxy server
		fd = tcp_connect2Server( url->hostname, url->port, 0);
		url_free( server_url );
		server_url = NULL;
	} else {
		if( server_url->port==0 ) server_url->port = 80;	// Default port for the web server
		fd = tcp_connect2Server( server_url->hostname, server_url->port, 0);
	}
	if( fd<0 ) {
		goto err_out; 
	}
	MSG_DBG2("Request: [%s]\n", http_hdr->buffer );
	
	ret = send( fd, http_hdr->buffer, http_hdr->buffer_size, 0);
	if( ret!=(int)http_hdr->buffer_size ) {
		MSG_ERR("Error while sending HTTP request: didn't sent all the request\n");
		goto err_out;
	}
	
	http_free( http_hdr );

	return fd;
err_out:
	if (fd > 0) closesocket(fd);
	http_free(http_hdr);
	if (proxy && server_url)
		url_free(server_url);
	return -1;
}

HTTP_header_t *
http_read_response( int fd ) {
	HTTP_header_t *http_hdr;
	char response[BUFFER_SIZE];
	int i;

	http_hdr = http_new_header();
	if( http_hdr==NULL ) {
		return NULL;
	}

	do {
		i = recv( fd, response, BUFFER_SIZE, 0); 
		if( i<0 ) {
			MSG_ERR("Read failed\n");
			http_free( http_hdr );
			return NULL;
		}
		if( i==0 ) {
			MSG_ERR("http_read_response read 0 -ie- EOF\n");
			http_free( http_hdr );
			return NULL;
		}
		http_response_append( http_hdr, response, i );
	} while( !http_is_header_entire( http_hdr ) ); 
	http_response_parse( http_hdr );
	return http_hdr;
}

int
http_authenticate(HTTP_header_t *http_hdr, URL_t *url, int *auth_retry) {
	char *aut;

	if( *auth_retry==1 ) {
		MSG_ERR(MSGTR_ConnAuthFailed);
		return -1;
	}
	if( *auth_retry>0 ) {
		if( url->username ) {
			free( url->username );
			url->username = NULL;
		}
		if( url->password ) {
			free( url->password );
			url->password = NULL;
		}
	}

	aut = http_get_field(http_hdr, "WWW-Authenticate");
	if( aut!=NULL ) {
		char *aut_space;
		aut_space = strstr(aut, "realm=");
		if( aut_space!=NULL ) aut_space += 6;
		MSG_INFO("Authentication required for %s\n", aut_space);
	} else {
		MSG_INFO("Authentication required\n");
	}
	if( network_username ) {
		url->username = strdup(network_username);
		if( url->username==NULL ) {
			MSG_FATAL(MSGTR_OutOfMemory);
			return -1;
		}
	} else {
		MSG_ERR(MSGTR_ConnAuthFailed);
		return -1;
	}
	if( network_password ) {
		url->password = strdup(network_password);
		if( url->password==NULL ) {
			MSG_FATAL(MSGTR_OutOfMemory);
			return -1;
		}
	} else {
		MSG_INFO("No password provided, trying blank password\n");
	}
	(*auth_retry)++;
	return 0;
}

int
http_seek( stream_t *stream, off_t pos ) {
	HTTP_header_t *http_hdr = NULL;
	int fd;
	if( stream==NULL ) return 0;

	if( stream->fd>0 ) closesocket(stream->fd); // need to reconnect to seek in http-stream
	fd = http_send_request( stream->streaming_ctrl->url, pos ); 
	if( fd<0 ) return 0;

	http_hdr = http_read_response( fd );

	if( http_hdr==NULL ) return 0;

	switch( http_hdr->status_code ) {
		case 200:
		case 206: // OK
			MSG_V("Content-Type: [%s]\n", http_get_field(http_hdr, "Content-Type") );
			MSG_V("Content-Length: [%s]\n", http_get_field(http_hdr, "Content-Length") );
			if( http_hdr->body_size>0 ) {
				if( streaming_bufferize( stream->streaming_ctrl, http_hdr->body, http_hdr->body_size )<0 ) {
					http_free( http_hdr );
					return -1;
				}
			}
			break;
		default:
			MSG_ERR("Server return %d: %s\n", http_hdr->status_code, http_hdr->reason_phrase );
			close( fd );
			fd = -1;
	}
	stream->fd = fd;

	if( http_hdr ) {
		http_free( http_hdr );
		stream->streaming_ctrl->data = NULL;
	}

	stream->pos=pos;

	return 1;
}

// By using the protocol, the extension of the file or the content-type
// we might be able to guess the streaming type.
int
autodetectProtocol(streaming_ctrl_t *streaming_ctrl, int *fd_out, int *file_format) {
	HTTP_header_t *http_hdr=NULL;
	unsigned int i;
	int fd=-1;
	int redirect;
	int auth_retry=0;
	int seekable=0;
	char *extension;
	char *content_type;
	char *next_url;

	URL_t *url = streaming_ctrl->url;
	*file_format = DEMUXER_TYPE_UNKNOWN;

	do {
		*fd_out = -1;
		next_url = NULL;
		extension = NULL;
		content_type = NULL;
		redirect = 0;

		if( url==NULL ) {
			goto err_out;
		}

		// Checking for PNM://
		if( !strcasecmp(url->protocol, "pnm") ) {
			*file_format = DEMUXER_TYPE_REAL;
			return 0;
		}
		// Checking for RTSP
		if( !strcasecmp(url->protocol, "rtsp") ) {
			MSG_ERR("RTSP protocol support requires the \"LIVE.COM Streaming Media\" libraries!\n");
			goto err_out;
		}

#ifndef STREAMING_LIVE_DOT_COM
	// Old, hacked RTP support, which works for MPEG Program Streams
	//   RTP streams only:
		// Checking for RTP
		if( !strcasecmp(url->protocol, "rtp") ) {
			if( url->port==0 ) {
				MSG_ERR("You must enter a port number for RTP streams!\n");
				goto err_out;
			}
			return 0;
		}
#endif

		// Checking for ASF
		if( !strncasecmp(url->protocol, "mms", 3) ) {
			*file_format = DEMUXER_TYPE_ASF;
			return 0;
		}

		if(!strcasecmp(url->protocol, "udp") ) {
			*file_format = DEMUXER_TYPE_UNKNOWN;
			return 0;
		}

		// HTTP based protocol
		if( !strcasecmp(url->protocol, "http") || !strcasecmp(url->protocol, "http_proxy") ) {
			fd = http_send_request( url, 0 );
			if( fd<0 ) {
				goto err_out;
			}

			http_hdr = http_read_response( fd );
			if( http_hdr==NULL ) {
				goto err_out;
			}

			*fd_out=fd;
			if( verbose ) {
				http_debug_hdr( http_hdr );
			}
			
			streaming_ctrl->data = (void*)http_hdr;
			
			// Check if we can make partial content requests and thus seek in http-streams
		        if( http_hdr!=NULL && http_hdr->status_code==200 ) {
			    char *accept_ranges;
			    if( (accept_ranges = http_get_field(http_hdr,"Accept-Ranges")) != NULL )
				seekable = strncmp(accept_ranges,"bytes",5)==0;
			} 
			// Check if the response is an ICY status_code reason_phrase
			if( !strcasecmp(http_hdr->protocol, "ICY") ) {
				switch( http_hdr->status_code ) {
					case 200: { // OK
						char *field_data = NULL;
						// note: I skip icy-notice1 and 2, as they contain html <BR>
						// and are IMHO useless info ::atmos
						if( (field_data = http_get_field(http_hdr, "icy-name")) != NULL )
							MSG_INFO("Name   : %s\n", field_data); field_data = NULL;
						if( (field_data = http_get_field(http_hdr, "icy-genre")) != NULL )
							MSG_INFO("Genre  : %s\n", field_data); field_data = NULL;
						if( (field_data = http_get_field(http_hdr, "icy-url")) != NULL )
							MSG_INFO("Website: %s\n", field_data); field_data = NULL;
						// XXX: does this really mean public server? ::atmos
						if( (field_data = http_get_field(http_hdr, "icy-pub")) != NULL )
							MSG_INFO("Public : %s\n", atoi(field_data)?"yes":"no"); field_data = NULL;
						if( (field_data = http_get_field(http_hdr, "icy-br")) != NULL )
							MSG_INFO("Bitrate: %skbit/s\n", field_data); field_data = NULL;
						// Ok, we have detected an mp3 stream
						// If content-type == video/nsv we most likely have a winamp video stream 
						// otherwise it should be mp3. if there are more types consider adding mime type 
						// handling like later
				                if ( (field_data = http_get_field(http_hdr, "content-type")) != NULL && (!strcmp(field_data, "video/nsv") || !strcmp(field_data, "misc/ultravox")))
							*file_format = DEMUXER_TYPE_NSV;
						else
							*file_format = DEMUXER_TYPE_AUDIO;
						return 0;
					}
					case 400: // Server Full
						MSG_ERR("Error: ICY-Server is full, skipping!\n");
						goto err_out;
					case 401: // Service Unavailable
						MSG_ERR("Error: ICY-Server return service unavailable, skipping!\n");
						goto err_out;
					case 403: // Service Forbidden
						MSG_ERR("Error: ICY-Server return 'Service Forbidden'\n");
						goto err_out;
					case 404: // Resource Not Found
						MSG_ERR("Error: ICY-Server couldn't find requested stream, skipping!\n");
						goto err_out;
					default:
						MSG_ERR("Error: unhandled ICY-Errorcode, contact MPlayer developers!\n");
						goto err_out;
				}
			}

			// Assume standard http if not ICY			
			switch( http_hdr->status_code ) {
				case 200: // OK
					// Look if we can use the Content-Type
					content_type = http_get_field( http_hdr, "Content-Type" );
					if( content_type!=NULL ) {
						char *content_length = NULL;
						MSG_V("Content-Type: [%s]\n", content_type );
						if( (content_length = http_get_field(http_hdr, "Content-Length")) != NULL)
							MSG_V("Content-Length: [%s]\n", http_get_field(http_hdr, "Content-Length"));
						// Check in the mime type table for a demuxer type
						for( i=0 ; i<(sizeof(mime_type_table)/sizeof(mime_type_table[0])) ; i++ ) {
							if( !strcasecmp( content_type, mime_type_table[i].mime_type ) ) {
								*file_format = mime_type_table[i].demuxer_type;
								return seekable; 
							}
						}
					}
					// Not found in the mime type table, don't fail,
					// we should try raw HTTP
					return 0;
				// Redirect
				case 301: // Permanently
				case 302: // Temporarily
					// TODO: RFC 2616, recommand to detect infinite redirection loops
					next_url = http_get_field( http_hdr, "Location" );
					if( next_url!=NULL ) {
						streaming_ctrl->url = url = url_redirect( &url, next_url );
						if (!strcasecmp(url->protocol, "mms")) {
						    goto err_out;
						}
						if (strcasecmp(url->protocol, "http")) {
						    MSG_WARN("Unsupported http %d redirect to %s protocol\n", http_hdr->status_code, url->protocol);
						    goto err_out;
						}
						redirect = 1;	
					}
					break;
				case 401: // Authentication required
					if( http_authenticate(http_hdr, url, &auth_retry)<0 ) goto err_out;
					redirect = 1;
					break;
				default:
					MSG_ERR("Server returned %d: %s\n", http_hdr->status_code, http_hdr->reason_phrase );
					goto err_out;
			}
		} else {
			MSG_ERR("Unknown protocol '%s'\n", url->protocol );
			goto err_out;
		}
	} while( redirect );
err_out:
	if (fd > 0) closesocket( fd );
	fd = -1;
	http_free( http_hdr );
	http_hdr = NULL;

	return -1;
}

int
streaming_bufferize( streaming_ctrl_t *streaming_ctrl, char *buffer, int size) {
//printf("streaming_bufferize\n");
	streaming_ctrl->buffer = (char*)malloc(size);
	if( streaming_ctrl->buffer==NULL ) {
		MSG_FATAL(MSGTR_OutOfMemory);
		return -1;
	}
	memcpy( streaming_ctrl->buffer, buffer, size );
	streaming_ctrl->buffer_size = size;
	return size;
}

int
nop_streaming_read( int fd, char *buffer, int size, streaming_ctrl_t *stream_ctrl ) {
	int len=0;
//printf("nop_streaming_read\n");
	if( stream_ctrl->buffer_size!=0 ) {
		int buffer_len = stream_ctrl->buffer_size-stream_ctrl->buffer_pos;
//printf("%d bytes in buffer\n", stream_ctrl->buffer_size);
		len = (size<buffer_len)?size:buffer_len;
		memcpy( buffer, (stream_ctrl->buffer)+(stream_ctrl->buffer_pos), len );
		stream_ctrl->buffer_pos += len;
//printf("buffer_pos = %d\n", stream_ctrl->buffer_pos );
		if( stream_ctrl->buffer_pos>=stream_ctrl->buffer_size ) {
			free( stream_ctrl->buffer );
			stream_ctrl->buffer = NULL;
			stream_ctrl->buffer_size = 0;
			stream_ctrl->buffer_pos = 0;
//printf("buffer cleaned\n");
		}
//printf("read %d bytes from buffer\n", len );
	}

	if( len<size ) {
		int ret;
		ret = read( fd, buffer+len, size-len );
		if( ret<0 ) {
			MSG_ERR("nop_streaming_read error : %s\n",strerror(errno));
		}
		len += ret;
//printf("read %d bytes from network\n", len );
	}
	
	return len;
}

int
nop_streaming_seek( int fd, off_t pos, streaming_ctrl_t *stream_ctrl ) {
	return -1;
}

int
nop_streaming_start( stream_t *stream ) {
	HTTP_header_t *http_hdr = NULL;
	char *next_url=NULL;
	URL_t *rd_url=NULL;
	int fd,ret;
	if( stream==NULL ) return -1;

	fd = stream->fd;
	if( fd<0 ) {
		fd = http_send_request( stream->streaming_ctrl->url,0); 
		if( fd<0 ) return -1;
		http_hdr = http_read_response( fd );
		if( http_hdr==NULL ) return -1;

		switch( http_hdr->status_code ) {
			case 200: // OK
				MSG_V("Content-Type: [%s]\n", http_get_field(http_hdr, "Content-Type") );
				MSG_V("Content-Length: [%s]\n", http_get_field(http_hdr, "Content-Length") );
				if( http_hdr->body_size>0 ) {
					if( streaming_bufferize( stream->streaming_ctrl, http_hdr->body, http_hdr->body_size )<0 ) {
						http_free( http_hdr );
						return -1;
					}
				}
				break;
			// Redirect
			case 301: // Permanently
			case 302: // Temporarily
				ret=-1;
				next_url = http_get_field( http_hdr, "Location" );

				if (next_url != NULL)
					rd_url=url_new(next_url);

				if (next_url != NULL && rd_url != NULL) {
					MSG_STATUS("Redirected: Using this url instead %s\n",next_url);
							stream->streaming_ctrl->url=check4proxies(rd_url);
					ret=nop_streaming_start(stream); //recursively get streaming started 
				} else {
					MSG_ERR("Redirection failed\n");
					closesocket( fd );
					fd = -1;
				}
				return ret;
				break;
			case 401: //Authorization required
			case 403: //Forbidden
			case 404: //Not found
			case 500: //Server Error
			default:
				MSG_ERR("Server return %d: %s\n", http_hdr->status_code, http_hdr->reason_phrase );
				closesocket( fd );
				fd = -1;
				return -1;
				break;
		}
		stream->fd = fd;
	} else {
		http_hdr = (HTTP_header_t*)stream->streaming_ctrl->data;
		if( http_hdr->body_size>0 ) {
			if( streaming_bufferize( stream->streaming_ctrl, http_hdr->body, http_hdr->body_size )<0 ) {
				http_free( http_hdr );
				stream->streaming_ctrl->data = NULL;
				return -1;
			}
		}
	}

	if( http_hdr ) {
		http_free( http_hdr );
		stream->streaming_ctrl->data = NULL;
	}

	stream->streaming_ctrl->streaming_read = nop_streaming_read;
	stream->streaming_ctrl->streaming_seek = nop_streaming_seek;
	stream->streaming_ctrl->prebuffer_size = 64*1024;	// KBytes
	stream->streaming_ctrl->buffering = 1;
	stream->streaming_ctrl->status = streaming_playing_e;
	return 0;
}

void fixup_network_stream_cache(stream_t *stream) {
  if(stream->streaming_ctrl->buffering) {
    if(stream_cache_size<0) {
      // cache option not set, will use our computed value.
      // buffer in KBytes, *5 because the prefill is 20% of the buffer.
      stream_cache_size = (stream->streaming_ctrl->prebuffer_size/1024)*5;
      if( stream_cache_size<64 ) stream_cache_size = 64;	// 16KBytes min buffer
    }
    MSG_INFO("[network] cache size set to: %i\n", stream_cache_size);
  }
}

int
pnm_streaming_read( int fd, char *buffer, int size, streaming_ctrl_t *stream_ctrl ) {
	return pnm_read(stream_ctrl->data, buffer, size);
}


int
pnm_streaming_start( stream_t *stream ) {
	int fd;
	pnm_t *pnm;
	if( stream==NULL ) return -1;

	fd = tcp_connect2Server( stream->streaming_ctrl->url->hostname,
	    stream->streaming_ctrl->url->port ? stream->streaming_ctrl->url->port : 7070, 0);
	printf("PNM:// fd=%d\n",fd);
	if(fd<0) return -1;
	
	pnm = pnm_connect(fd,stream->streaming_ctrl->url->file);
	if(!pnm) return -2;

	stream->fd=fd;
	stream->streaming_ctrl->data=pnm;

	stream->streaming_ctrl->streaming_read = pnm_streaming_read;
//	stream->streaming_ctrl->streaming_seek = nop_streaming_seek;
	stream->streaming_ctrl->prebuffer_size = 8*1024;  // 8 KBytes
	stream->streaming_ctrl->buffering = 1;
	stream->streaming_ctrl->status = streaming_playing_e;
	return 0;
}

#ifdef HAVE_RTSP_SESSION_H
int
realrtsp_streaming_read( int fd, char *buffer, int size, streaming_ctrl_t *stream_ctrl ) {
	return rtsp_session_read(stream_ctrl->data, buffer, size);
}


int
realrtsp_streaming_start( stream_t *stream ) {
	int fd;
	rtsp_session_t *rtsp;
	char *mrl;
	char *file;
	int port;
	int redirected, temp;
	if( stream==NULL ) return -1;
	
	temp = 5; // counter so we don't get caught in infinite redirections (you never know)
	
	do {
	
		redirected = 0;
		port = stream->streaming_ctrl->url->port ? stream->streaming_ctrl->url->port : 554;
		fd = tcp_connect2Server( stream->streaming_ctrl->url->hostname, port, 1);
		if(fd<0 && !stream->streaming_ctrl->url->port)
			fd = tcp_connect2Server( stream->streaming_ctrl->url->hostname,	port = 7070, 1 );
		if(fd<0) return -1;
		
		file = stream->streaming_ctrl->url->file;
		if (file[0] == '/')
		    file++;
		mrl = malloc(sizeof(char)*(strlen(stream->streaming_ctrl->url->hostname)+strlen(file)+16));
		sprintf(mrl,"rtsp://%s:%i/%s",stream->streaming_ctrl->url->hostname,port,file);
		rtsp = rtsp_session_start(fd,&mrl, file,
			stream->streaming_ctrl->url->hostname, port, &redirected);

		if ( redirected == 1 ) {
			url_free(stream->streaming_ctrl->url);
			stream->streaming_ctrl->url = url_new(mrl);
			closesocket(fd);
		}

		free(mrl);
		temp--;

	} while( (redirected != 0) && (temp > 0) );	

	if(!rtsp) return -1;

	stream->fd=fd;
	stream->streaming_ctrl->data=rtsp;

	stream->streaming_ctrl->streaming_read = realrtsp_streaming_read;
//	stream->streaming_ctrl->streaming_seek = nop_streaming_seek;
	stream->streaming_ctrl->prebuffer_size = 128*1024;  // 8 KBytes
	stream->streaming_ctrl->buffering = 1;
	stream->streaming_ctrl->status = streaming_playing_e;
	return 0;
}
#endif // HAVE_RTSP_SESSION_H


#ifndef STREAMING_LIVE_DOT_COM

static int
rtp_streaming_read( int fd, char *buffer, int size, streaming_ctrl_t *streaming_ctrl ) {
    return read_rtp_from_server( fd, buffer, size );
}

static int
rtp_streaming_start( stream_t *stream, int raw_udp ) {
	streaming_ctrl_t *streaming_ctrl;
	int fd;

	if( stream==NULL ) return -1;
	streaming_ctrl = stream->streaming_ctrl;
	fd = stream->fd;
	
	if( fd<0 ) {
		fd = udp_open_socket( (streaming_ctrl->url) ); 
		if( fd<0 ) return -1;
		stream->fd = fd;
	}

	if(raw_udp)
		streaming_ctrl->streaming_read = nop_streaming_read;
	else
		streaming_ctrl->streaming_read = rtp_streaming_read;
	streaming_ctrl->streaming_read = rtp_streaming_read;
	streaming_ctrl->streaming_seek = nop_streaming_seek;
	streaming_ctrl->prebuffer_size = 64*1024;	// KBytes	
	streaming_ctrl->buffering = 0;
	streaming_ctrl->status = streaming_playing_e;
	return 0;
}
#endif
extern int asf_streaming_start( stream_t *stream, int *demuxer_type);
int
streaming_start(stream_t *stream, int *demuxer_type, URL_t *url) {
	int ret;
	if( stream==NULL ) return -1;

	stream->streaming_ctrl = streaming_ctrl_new();
	if( stream->streaming_ctrl==NULL ) {
		return -1;
	}
	stream->streaming_ctrl->url = check4proxies( url );

        if (*demuxer_type != DEMUXER_TYPE_PLAYLIST){ 
	ret = autodetectProtocol( stream->streaming_ctrl, &stream->fd, demuxer_type );
        } else {
	  ret=0;
	}

	if( ret<0 ) {
		return -1;
	}
	if( ret==1 ) {
//		stream->flags |= STREAM_SEEK;
//		stream->seek = http_seek;
	}

	ret = -1;
	
	// Get the bandwidth available
	stream->streaming_ctrl->bandwidth = network_bandwidth;
	
	// For RTP streams, we usually don't know the stream type until we open it.
	if( !strcasecmp( stream->streaming_ctrl->url->protocol, "rtp")) {
		if(stream->fd >= 0) {
			if(closesocket(stream->fd) < 0)
				MSG_ERR("streaming_start : Closing socket %d failed %s\n",stream->fd,strerror(errno));
		}
		stream->fd = -1;
		ret = rtp_streaming_start( stream, 0);
	} else

	if( !strcasecmp( stream->streaming_ctrl->url->protocol, "pnm")) {
		stream->fd = -1;
		ret = pnm_streaming_start( stream );
		if (ret == -1) {
		    MSG_INFO("Can't connect with pnm, retrying with http.\n");
		    goto stream_switch;
		}
	} else
	
#ifdef HAVE_RTSP_SESSION_H
	if( (!strcasecmp( stream->streaming_ctrl->url->protocol, "rtsp")) &&
			(*demuxer_type == DEMUXER_TYPE_REAL)) {
		stream->fd = -1;
		if ((ret = realrtsp_streaming_start( stream )) < 0) {
		    MSG_INFO("Not a Realmedia rtsp url. Trying standard rtsp protocol.\n");
#ifdef STREAMING_LIVE_DOT_COM
		    *demuxer_type =  DEMUXER_TYPE_RTP;
		    goto stream_switch;
#else
		    MSG_ERR("RTSP support requires the \"LIVE.COM Streaming Media\" libraries!\n");
		    return -1;
#endif
		}
	} else
#endif
	if(!strcasecmp( stream->streaming_ctrl->url->protocol, "udp")) {
		stream->fd = -1;
		ret = rtp_streaming_start(stream, 1);
		if(ret<0) {
			MSG_ERR("rtp_streaming_start(udp) failed\n");
			return -1;
		}
		*demuxer_type =  DEMUXER_TYPE_UNKNOWN;
	} else

	// For connection-oriented streams, we can usually determine the streaming type.
stream_switch:
	switch( *demuxer_type ) {
		case DEMUXER_TYPE_ASF:
			// Send the appropriate HTTP request
			// Need to filter the network stream.
			// ASF raw stream is encapsulated.
			// It can also be a playlist (redirector)
			// so we need to pass demuxer_type too
			ret = asf_streaming_start( stream, demuxer_type );
			if( ret<0 ) {
                                //sometimes a file is just on a webserver and it is not streamed.
				//try loading them default method as last resort for http protocol
                                if ( !strcasecmp(stream->streaming_ctrl->url->protocol, "http") ) {
                                MSG_STATUS("Trying default streaming for http protocol\n ");
                                //reset stream
                                close(stream->fd);
		                stream->fd=-1;
                                ret=nop_streaming_start(stream);
                                }

                         if (ret<0) {
				MSG_ERR("asf_streaming_start failed\n");
                                MSG_STATUS("Check if this is a playlist which requires -playlist option\nExample: mplayer -playlist <url>\n");
                               }
			}
			break;
#ifdef STREAMING_LIVE_DOT_COM
		case DEMUXER_TYPE_RTP:
			// RTSP/RTP streaming is handled separately:
			ret = rtsp_streaming_start( stream );
			if( ret<0 ) {
				MSG_ERR("rtsp_streaming_start failed\n");
			}
			break;
#endif
		case DEMUXER_TYPE_MPEG_ES:
		case DEMUXER_TYPE_MPEG_PS:
		case DEMUXER_TYPE_AVI:
		case DEMUXER_TYPE_MOV:
		case DEMUXER_TYPE_VIVO:
		case DEMUXER_TYPE_FLI:
		case DEMUXER_TYPE_REAL:
		case DEMUXER_TYPE_Y4M:
		case DEMUXER_TYPE_FILM:
		case DEMUXER_TYPE_ROQ:
		case DEMUXER_TYPE_AUDIO:
		case DEMUXER_TYPE_OGG:
		case DEMUXER_TYPE_PLAYLIST:
		case DEMUXER_TYPE_UNKNOWN:
		case DEMUXER_TYPE_NSV: 
			// Generic start, doesn't need to filter
			// the network stream, it's a raw stream
			ret = nop_streaming_start( stream );
			if( ret<0 ) {
			    MSG_ERR("nop_streaming_start failed\n");
			}
			break;
		default:
			MSG_ERR("Unable to detect the streaming type\n");
			ret = -1;
	}

	if( ret<0 ) {
		streaming_ctrl_free( stream->streaming_ctrl );
		stream->streaming_ctrl = NULL;
	} else if( stream->streaming_ctrl->buffering ) {
		if(stream_cache_size<0) {
			// cache option not set, will use our computed value.
			// buffer in KBytes, *5 because the prefill is 20% of the buffer.
			stream_cache_size = (stream->streaming_ctrl->prebuffer_size/1024)*5;
			if( stream_cache_size<64 ) stream_cache_size = 64;	// 16KBytes min buffer
		}
		MSG_INFO("Cache size set to %d KBytes\n", stream_cache_size);
	}

	return ret;
}

int
streaming_stop( stream_t *stream ) {
	stream->streaming_ctrl->status = streaming_stopped_e;
	return 0;
}
