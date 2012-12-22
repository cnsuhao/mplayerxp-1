#include "mpxp_config.h"
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
#include "mpxp_help.h"

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

MPXP_Rc http_send_request(Tcp& tcp, URL& url, off_t pos ) {
    HTTP_Header& http_hdr = *new(zeromem) HTTP_Header;
    URL server_url("");
    char str[256];
    int ret;
    int proxy = 0;		// Boolean

    if( url.protocol2lower()=="http_proxy") {
	proxy = 1;
	server_url.redirect(url.file());
	http_hdr.set_uri(server_url.url());
    } else {
	    server_url = url;
	    http_hdr.set_uri( server_url.file());
    }
    if (server_url.port() && server_url.port() != 80)
	snprintf(str, 256, "Host: %s:%d", server_url.host().c_str(), server_url.port());
    else
	snprintf(str, 256, "Host: %s", server_url.host().c_str());
    http_hdr.set_field(str);
    if (net_conf.useragent) {
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

    if (net_conf.cookies_enabled) http_hdr.cookies_set( server_url.host(), server_url.url());

    http_hdr.set_field( "Connection: closed");
    http_hdr.add_basic_authentication( url.user(), url.password());
    if( http_hdr.build_request( )==NULL ) {
	goto err_out;
    }

    if( proxy ) {
	tcp.close();
	url.assign_port(8080);
	tcp.open(url, Tcp::IP4);
    } else {
	tcp.close();
	server_url.assign_port(80);
	tcp.open(server_url, Tcp::IP4);
    }
    if(!tcp.established()) { mpxp_err<<"Cannot establish connection"<<std::endl; goto err_out; }
    mpxp_dbg2<<"Request: ["<<http_hdr.get_buffer()<<"]"<<std::endl;

    ret = tcp.write((uint8_t*)(http_hdr.get_buffer()), http_hdr.get_buffer_size());
    if( ret!=(int)http_hdr.get_buffer_size() ) {
	mpxp_err<<"Error while sending HTTP request: didn't sent all the request"<<std::endl;
	goto err_out;
    }

    delete &http_hdr;
    return MPXP_Ok;
err_out:
    delete &http_hdr;
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
	    mpxp_err<<"Read failed"<<std::endl;
	    delete http_hdr;
	    return NULL;
	}
	if( i==0 ) {
	    mpxp_err<<"http_read_response read 0 -ie- EOF"<<std::endl;
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
	    mpxp_v<<"Content-Type: ["<<http_hdr->get_field("Content-Type")<<"]"<<std::endl;
	    mpxp_v<<"Content-Length: ["<<http_hdr->get_field("Content-Length")<<"]"<<std::endl;
	    if( http_hdr->get_body_size()>0 ) {
		if( networking.bufferize((unsigned char *)http_hdr->get_body(), http_hdr->get_body_size() )<0 ) {
		    delete http_hdr;
		    return 0;
		}
	    }
	    break;
	default:
	    mpxp_err<<"Server return "<<http_hdr->get_status()<<": "<<http_hdr->get_reason_phrase()<<std::endl;
	    tcp.close();
    }

    if( http_hdr ) delete http_hdr;

    return pos;
}

Networking::Networking()
	    :mime("application/octet-stream"),
	    url("") {}

Networking::~Networking() {
    if( buffer ) delete buffer;
    if( data ) delete data;
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

    URL& url = networking.url;

    do {
	next_url = NULL;
	extension = NULL;
	content_type = NULL;
	redirect = 0;

#ifndef STREAMING_LIVE_DOT_COM
	// Old, hacked RTP support, which works for MPEG Program Streams
	//   RTP streams only:
	// Checking for RTP
	if( url.protocol2lower()=="rtp") {
	    if( url.port()==0 ) {
		mpxp_err<<"You must enter a port number for RTP streams!"<<std::endl;
		goto err_out;
	    }
	    return MPXP_Ok;
	}
#endif
	// HTTP based protocol
	if( url.protocol2lower()=="http" || url.protocol2lower()=="http_proxy") {
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
			    mpxp_info<<"Name   : "<<field_data<<std::endl; field_data = NULL;
			if( (field_data = http_hdr->get_field("icy-genre")) != NULL )
			    mpxp_info<<"Genre  : "<<field_data<<std::endl; field_data = NULL;
			if( (field_data = http_hdr->get_field("icy-url")) != NULL )
			    mpxp_info<<"Website: "<<field_data<<std::endl; field_data = NULL;
			// XXX: does this really mean public server? ::atmos
			if( (field_data = http_hdr->get_field("icy-pub")) != NULL )
			    mpxp_info<<"Public : "<<(atoi(field_data)?"yes":"no")<<std::endl; field_data = NULL;
			if( (field_data = http_hdr->get_field("icy-br")) != NULL )
			    mpxp_info<<"Bitrate: "<<field_data<<"kbit/s"<<std::endl; field_data = NULL;
			if ( (field_data = http_hdr->get_field("content-type")) != NULL )
			    networking.mime = field_data;
			return MPXP_Ok;
		    }
		    case 400: // Server Full
			mpxp_err<<"Error: ICY-Server is full, skipping!"<<std::endl;
			goto err_out;
		    case 401: // Service Unavailable
			mpxp_err<<"Error: ICY-Server return service unavailable, skipping!"<<std::endl;
			goto err_out;
		    case 403: // Service Forbidden
			mpxp_err<<"Error: ICY-Server return 'Service Forbidden'"<<std::endl;
			goto err_out;
		    case 404: // Resource Not Found
			mpxp_err<<"Error: ICY-Server couldn't find requested stream, skipping!"<<std::endl;
			goto err_out;
		    default:
			mpxp_err<<"Error: unhandled ICY-Errorcode, contact MPlayer developers!"<<std::endl;
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
			mpxp_v<<"Content-Type: ["<<content_type<<"]"<<std::endl;
			if( (content_length = http_hdr->get_field("Content-Length")) != NULL)
			    mpxp_v<<"Content-Length: ["<<http_hdr->get_field("Content-Length")<<"]"<<std::endl;
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
			    url.redirect(next_url);
			    if (url.protocol2lower()=="mms") goto err_out;
			    if (url.protocol2lower()!="http") {
				mpxp_warn<<"Unsupported http "<<http_hdr->get_status()<<" redirect to "<<url.protocol()<<" protocol"<<std::endl;
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
			mpxp_err<<"Server returned "<<http_hdr->get_status()<<": "<<http_hdr->get_reason_phrase()<<std::endl;
			goto err_out;
	    }
	} else {
	    mpxp_err<<"Unknown protocol: "<<url.protocol()<<std::endl;
	    goto err_out;
	}
    } while( redirect );
err_out:
    delete http_hdr;

    return MPXP_False;
}

int Networking::bufferize(unsigned char *_buffer, int size) {
    buffer = new char [size];
    if( buffer==NULL ) {
	mpxp_fatal<<MSGTR_OutOfMemory<<std::endl;
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
    mpxp_info<<"[network] cache size set to: "<<mp_conf.s_cache_size<<std::endl;
  }
}

Networking* Networking::start(Tcp& tcp, const URL& _url) {
    Networking* rc;
    network_protocol_t net_protocol;
    net_protocol.url=_url;
    net_protocol.url.check4proxies();

    if( autodetectProtocol(net_protocol,tcp)!=MPXP_Ok ) return NULL;
    rc = NULL;
    URL url=net_protocol.url;

    // For RTP streams, we usually don't know the stream type until we open it.
    if( url.protocol2lower()=="rtp") {
	if(tcp.established()) tcp.close();
	rc = Rtp_Networking::start(tcp, net_protocol, 0);
    } else if( url.protocol2lower()=="pnm") {
	tcp.close();
	rc = Pnm_Networking::start(tcp, net_protocol);
	if (!rc) {
	    mpxp_info<<"Can't connect with pnm, retrying with http"<<std::endl;
	    return NULL;
	}
    } else if( url.protocol2lower()=="rtsp") {
	if ((rc = RealRtsp_Networking::start( tcp, net_protocol )) == NULL) {
	    mpxp_info<<"Not a Realmedia rtsp url. Trying standard rtsp protocol"<<std::endl;
#ifdef STREAMING_LIVE_DOT_COM
	    rc = Rtsp_Networking::start( tcp, net_protocol );
	    if(!rc) mpxp_err<<"rtsp_networking_start failed"<<std::endl;
	    return rc;
#else
	    mpxp_err<<"RTSP support requires the \"LIVE.COM Streaming Media\" libraries!"<<std::endl;
	    return NULL;
#endif
	}
    } else if(url.protocol2lower()=="udp") {
	tcp.close();
	rc = Rtp_Networking::start(tcp, net_protocol, 1);
	if(!rc) {
	    mpxp_err<<"rtp_networking_start(udp) failed"<<std::endl;
	    return NULL;
	}
    } else if(url.protocol2lower()=="mms" ||
	      url.protocol2lower()=="mmst" ||
	      url.protocol2lower()=="mmsu") {
	rc=Asf_Mmst_Networking::start(tcp,net_protocol);
	if(!rc) {
	    mpxp_err<<"asf_mmst_networking_start() failed"<<std::endl;
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
	    if (url.protocol2lower()=="http") {
		mpxp_status<<"Trying default networking for http protocol"<<std::endl;
		//reset stream
		tcp.close();
		rc=Nop_Networking::start(tcp,net_protocol);
	    }
	    if (!rc) {
		mpxp_err<<"asf_networking_start failed"<<std::endl;
		mpxp_status<<"Check if this is a playlist which requires -playlist option"<<std::endl<<"Example: mplayer -playlist <url>"<<std::endl;
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
