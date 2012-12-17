#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
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

#include "mplayerxp.h"
#include "network_asf.h"
#include "network_asf_mmst.h"
#include "network_nop.h"
#include "network_pnm.h"
#include "network_real_rtsp.h"
#include "network_rtp.h"
#include "network_rtsp.h"
#ifndef HAVE_WINSOCK2
#define closesocket close
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include "libmpconf/cfgparser.h"
#include "stream.h"
#include "help_mp.h"

#include "tcp.h"
#include "network.h"
#include "http.h"
#include "cookies.h"
#include "url.h"
#include "udp.h"
#include "version.h"
#include "stream_msg.h"

namespace mpxp {
/* Variables for the command line option -user, -passwd & -bandwidth */
net_config_t::net_config_t()
	    :username(NULL),
	    password(NULL),
	    bandwidth(0),
	    cookies_enabled(0),
	    cookies_file(NULL),
	    useragent(NULL),
	    prefer_ipv4(1),
	    ipv4_only_proxy(0)
{
}
net_config_t::~net_config_t() {}
net_config_t net_conf;

URL*
check4proxies( URL *url ) {
	URL *url_out = NULL;
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
			URL *tmp_url;
			URL *proxy_url = url_new( proxy );

			if( proxy_url==NULL ) {
				MSG_WARN("Invalid proxy setting...Trying without proxy.\n");
				return url_out;
			}

#ifdef HAVE_AF_INET6
			if (net_conf.ipv4_only_proxy && (gethostbyname(url->hostname)==NULL)) {
				MSG_WARN(
					"Could not find resolve remote hostname for AF_INET. Trying without proxy.\n");
				return url_out;
			}
#endif

			MSG_V("Using HTTP proxy: %s\n", proxy_url->url );
			len = strlen( proxy_url->hostname ) + strlen( url->url ) + 20;	// 20 = http_proxy:// + port
			new_url = new char [len+1];
			if( new_url==NULL ) {
				MSG_FATAL(MSGTR_OutOfMemory);
				return url_out;
			}
			sprintf(new_url, "http_proxy://%s:%d/%s", proxy_url->hostname, proxy_url->port, url->url );
			tmp_url = url_new( new_url );
			if( tmp_url==NULL ) {
				return url_out;
			}
			delete url_out;
			url_out = tmp_url;
			delete new_url ;
			delete proxy_url;
		}
	}
	return url_out;
}

MPXP_Rc http_send_request(Tcp& tcp, URL *url, off_t pos ) {
	HTTP_Header& http_hdr = *new(zeromem) HTTP_Header;
	URL *server_url;
	char str[256];
	int ret;
	int proxy = 0;		// Boolean

	if( !strcasecmp(url->protocol, "http_proxy") ) {
		proxy = 1;
		server_url = url_new( (url->file)+1 );
		http_hdr.set_uri(server_url->url );
	} else {
		server_url = url;
		http_hdr.set_uri( server_url->file );
	}
	if (server_url->port && server_url->port != 80)
	    snprintf(str, 256, "Host: %s:%d", server_url->hostname, server_url->port );
	else
	    snprintf(str, 256, "Host: %s", server_url->hostname );
	http_hdr.set_field(str);
	if (net_conf.useragent)
	{
	    snprintf(str, 256, "User-Agent: %s", net_conf.useragent);
	    http_hdr.set_field(str);
	}
	else
	    http_hdr.set_field("User-Agent: MPlayerXP/"VERSION);

	http_hdr.set_field("Icy-MetaData: 1");

	if(pos>0) {
	// Extend http_send_request with possibility to do partial content retrieval
	    snprintf(str, 256, "Range: bytes=%d-", (int)pos);
	    http_hdr.set_field(str);
	}

	if (net_conf.cookies_enabled) http_hdr.cookies_set( server_url->hostname, server_url->url );

	http_hdr.set_field( "Connection: closed");
	http_hdr.add_basic_authentication( url->username?url->username:"", url->password?url->password:"");
	if( http_hdr.build_request( )==NULL ) {
		goto err_out;
	}

	if( proxy ) {
		if( url->port==0 ) url->port = 8080;			// Default port for the proxy server
		tcp.close();
		tcp.open(url->hostname, url->port, Tcp::IP4);
		delete server_url;
		server_url = NULL;
	} else {
		if( server_url->port==0 ) server_url->port = 80;	// Default port for the web server
		tcp.close();
		tcp.open(server_url->hostname, server_url->port, Tcp::IP4);
	}
	if(!tcp.established()) { MSG_ERR("Cannot establish connection\n"); goto err_out; }
	MSG_DBG2("Request: [%s]\n", http_hdr.get_buffer() );

	ret = tcp.write((uint8_t*)(http_hdr.get_buffer()), http_hdr.get_buffer_size());
	if( ret!=(int)http_hdr.get_buffer_size() ) {
		MSG_ERR("Error while sending HTTP request: didn't sent all the request\n");
		goto err_out;
	}

	delete &http_hdr;

	return MPXP_Ok;
err_out:
	delete &http_hdr;
	if (proxy && server_url) delete server_url;
	return MPXP_False;
}

HTTP_Header* http_read_response( Tcp& tcp ) {
	HTTP_Header* http_hdr = new(zeromem) HTTP_Header;
	uint8_t response[BUFFER_SIZE];
	int i;

	if( http_hdr==NULL ) return NULL;

	do {
		i = tcp.read(response, BUFFER_SIZE);
		if( i<0 ) {
			MSG_ERR("Read failed\n");
			delete http_hdr;
			return NULL;
		}
		if( i==0 ) {
			MSG_ERR("http_read_response read 0 -ie- EOF\n");
			delete http_hdr;
			return NULL;
		}
		http_hdr->response_append(response,i);
	} while( !http_hdr->is_header_entire() );
	http_hdr->response_parse();
	return http_hdr;
}

off_t http_seek(Tcp& tcp, Networking& networking, off_t pos ) {
    HTTP_Header* http_hdr = NULL;

    tcp.close();
    if(http_send_request(tcp,networking.url, pos)==MPXP_Ok) return 0;

    http_hdr = http_read_response(tcp);

    if( http_hdr==NULL ) return 0;

    switch( http_hdr->get_status() ) {
	case 200:
	case 206: // OK
	    MSG_V("Content-Type: [%s]\n", http_hdr->get_field("Content-Type") );
	    MSG_V("Content-Length: [%s]\n", http_hdr->get_field("Content-Length") );
	    if( http_hdr->get_body_size()>0 ) {
		if( networking.bufferize((unsigned char *)http_hdr->get_body(), http_hdr->get_body_size() )<0 ) {
		    delete http_hdr;
		    return 0;
		}
	    }
	    break;
	default:
	    MSG_ERR("Server return %d: %s\n", http_hdr->get_status(), http_hdr->get_reason_phrase());
	    tcp.close();
    }

    if( http_hdr ) {
	delete http_hdr;
	networking.data = NULL;
    }

    return pos;
}

Networking::Networking()
	    :mime("application/octet-stream") {}

Networking::~Networking() {
    if( url ) delete url;
    if( buffer ) delete buffer ;
    if( data ) delete data ;
}

// By using the protocol, the extension of the file or the content-type
// we might be able to guess the networking type.
MPXP_Rc Networking::autodetectProtocol(network_protocol_t& networking, Tcp& tcp) {
    HTTP_Header *http_hdr=NULL;
    int redirect;
    int auth_retry=0;
    MPXP_Rc seekable=MPXP_False;
    const char *extension;
    const char *content_type;
    const char *next_url;

    URL *url = networking.url;

    do {
	next_url = NULL;
	extension = NULL;
	content_type = NULL;
	redirect = 0;

	if( url==NULL ) {
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
	    return MPXP_Ok;
	}
#endif
	// HTTP based protocol
	if( !strcasecmp(url->protocol, "http") || !strcasecmp(url->protocol, "http_proxy") ) {
	    http_send_request(tcp, url, 0 );
	    if(!tcp.established()) goto err_out;

	    http_hdr = http_read_response(tcp);
	    if( http_hdr==NULL ) goto err_out;
	    if( mp_conf.verbose ) http_hdr->debug_hdr();
	    networking.data = http_hdr;

	    // Check if we can make partial content requests and thus seek in http-streams
	    if( http_hdr->get_status()==200 ) {
		    const char *accept_ranges;
		    if( (accept_ranges = http_hdr->get_field("Accept-Ranges")) != NULL )
			seekable = strncmp(accept_ranges,"bytes",5)==0?MPXP_Ok:MPXP_False;
	    }
	    // Check if the response is an ICY get_status() reason_phrase
	    if( !strcasecmp(http_hdr->get_protocol(), "ICY") ) {
		switch( http_hdr->get_status() ) {
		    case 200: { // OK
			const char *field_data = NULL;
			// note: I skip icy-notice1 and 2, as they contain html <BR>
			// and are IMHO useless info ::atmos
			if( (field_data = http_hdr->get_field("icy-name")) != NULL )
			    MSG_INFO("Name   : %s\n", field_data); field_data = NULL;
			if( (field_data = http_hdr->get_field("icy-genre")) != NULL )
			    MSG_INFO("Genre  : %s\n", field_data); field_data = NULL;
			if( (field_data = http_hdr->get_field("icy-url")) != NULL )
			    MSG_INFO("Website: %s\n", field_data); field_data = NULL;
			// XXX: does this really mean public server? ::atmos
			if( (field_data = http_hdr->get_field("icy-pub")) != NULL )
			    MSG_INFO("Public : %s\n", atoi(field_data)?"yes":"no"); field_data = NULL;
			if( (field_data = http_hdr->get_field("icy-br")) != NULL )
			    MSG_INFO("Bitrate: %skbit/s\n", field_data); field_data = NULL;
			if ( (field_data = http_hdr->get_field("content-type")) != NULL )
			    networking.mime = field_data;
			return MPXP_Ok;
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
	    switch( http_hdr->get_status() ) {
		case 200: // OK
		    // Look if we can use the Content-Type
		    content_type = http_hdr->get_field("Content-Type" );
		    if( content_type!=NULL ) {
			const char *content_length = NULL;
			MSG_V("Content-Type: [%s]\n", content_type );
			if( (content_length = http_hdr->get_field("Content-Length")) != NULL)
			    MSG_V("Content-Length: [%s]\n", http_hdr->get_field("Content-Length"));
		    }
		    // Not found in the mime type table, don't fail,
		    // we should try raw HTTP
		    return MPXP_Ok;
		// Redirect
		    case 301: // Permanently
		    case 302: // Temporarily
			// TODO: RFC 2616, recommand to detect infinite redirection loops
			next_url = http_hdr->get_field("Location" );
			if( next_url!=NULL ) {
			    networking.url = url = url_redirect( &url, next_url );
			    if (!strcasecmp(url->protocol, "mms")) goto err_out;
			    if (strcasecmp(url->protocol, "http")) {
				MSG_WARN("Unsupported http %d redirect to %s protocol\n", http_hdr->get_status(), url->protocol);
				goto err_out;
			    }
			    redirect = 1;
			}
			break;
		    case 401: // Authentication required
			if( http_hdr->authenticate(url, &auth_retry)<0 ) goto err_out;
			redirect = 1;
			break;
		    default:
			MSG_ERR("Server returned %d: %s\n", http_hdr->get_status(), http_hdr->get_reason_phrase());
			goto err_out;
	    }
	} else {
	    MSG_ERR("Unknown protocol '%s'\n", url->protocol );
	    goto err_out;
	}
    } while( redirect );
err_out:
    delete http_hdr;

    return MPXP_False;
}

int Networking::bufferize(unsigned char *_buffer, int size) {
//printf("networking_bufferize\n");
    buffer = new char [size];
    if( buffer==NULL ) {
	MSG_FATAL(MSGTR_OutOfMemory);
	return -1;
    }
    memcpy( buffer, _buffer, size );
    buffer_size = size;
    return size;
}

void Networking::fixup_cache() {
  if(buffering) {
    if(mp_conf.s_cache_size<0) {
      // cache option not set, will use our computed value.
      // buffer in KBytes, *5 because the prefill is 20% of the buffer.
      mp_conf.s_cache_size = (prebuffer_size/1024)*5;
      if( mp_conf.s_cache_size<64 ) mp_conf.s_cache_size = 64;	// 16KBytes min buffer
    }
    MSG_INFO("[network] cache size set to: %i\n", mp_conf.s_cache_size);
  }
}

Networking* Networking::start(Tcp& tcp, URL *_url) {
    Networking* rc;
    network_protocol_t net_protocol;
    URL* url = check4proxies( _url );

    net_protocol.url=url;

    if( autodetectProtocol(net_protocol,tcp)!=MPXP_Ok ) return NULL;
    rc = NULL;
    url=net_protocol.url;

    // For RTP streams, we usually don't know the stream type until we open it.
    if( !strcasecmp( url->protocol, "rtp")) {
	if(tcp.established()) tcp.close();
	rc = Rtp_Networking::start(tcp, net_protocol, 0);
    } else if( !strcasecmp( url->protocol, "pnm")) {
	tcp.close();
	rc = Pnm_Networking::start(tcp, net_protocol);
	if (!rc) {
	    MSG_INFO("Can't connect with pnm, retrying with http.\n");
	    return NULL;
	}
    }
    else if( !strcasecmp( url->protocol, "rtsp")) {
	if ((rc = RealRtsp_Networking::start( tcp, net_protocol )) == NULL) {
	    MSG_INFO("Not a Realmedia rtsp url. Trying standard rtsp protocol.\n");
#ifdef STREAMING_LIVE_DOT_COM
	    rc = Rtsp_Networking::start( tcp, net_protocol );
	    if(!rc) MSG_ERR("rtsp_networking_start failed\n");
	    return rc;
#else
	    MSG_ERR("RTSP support requires the \"LIVE.COM Streaming Media\" libraries!\n");
	    return NULL;
#endif
	}
    }
    else if(!strcasecmp( url->protocol, "udp")) {
	tcp.close();
	rc = Rtp_Networking::start(tcp, net_protocol, 1);
	if(!rc) {
	    MSG_ERR("rtp_networking_start(udp) failed\n");
	    return NULL;
	}
    } else {
	// Send the appropriate HTTP request
	// Need to filter the network stream.
	// ASF raw stream is encapsulated.
	// It can also be a playlist (redirector)
	// so we need to pass demuxer_type too
	rc = Asf_Networking::start(tcp,net_protocol);
	if( !rc ) {
	    //sometimes a file is just on a webserver and it is not streamed.
	    //try loading them default method as last resort for http protocol
	    if ( !strcasecmp(url->protocol, "http") ) {
		MSG_STATUS("Trying default networking for http protocol\n ");
		//reset stream
		tcp.close();
		rc=Nop_Networking::start(tcp,net_protocol);
	    }
	    if (!rc) {
		MSG_ERR("asf_networking_start failed\n");
		MSG_STATUS("Check if this is a playlist which requires -playlist option\nExample: mplayer -playlist <url>\n");
	    }
	}
    }
    if( rc ) {
	// Get the bandwidth available
	rc->bandwidth = net_conf.bandwidth;
	rc->fixup_cache();
	rc->url=net_protocol.url;
	rc->mime=net_protocol.mime;
	rc->data=net_protocol.data;
    }
    return rc;
}

int Networking::stop() {
    status = networking_stopped_e;
    return 0;
}
} // namespace mpxp
