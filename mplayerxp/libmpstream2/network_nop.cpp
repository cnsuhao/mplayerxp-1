#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;

#include <errno.h>
#include "tcp.h"
#include "network_nop.h"
#include "stream.h"
#include "help_mp.h"
#include "stream_msg.h"

namespace mpxp {
int Nop_Networking::read(Tcp& tcp, char *_buffer, int size) {
    int len=0;
//printf("nop_networking_read\n");
    if( buffer_size!=0 ) {
	int buffer_len = buffer_size-buffer_pos;
//printf("%d bytes in buffer\n", stream_ctrl.buffer_size);
	len = (size<buffer_len)?size:buffer_len;
	memcpy( _buffer, buffer+buffer_pos, len );
	buffer_pos += len;
//printf("buffer_pos = %d\n", stream_ctrl.buffer_pos );
	if( buffer_pos>=buffer_size ) {
	    delete buffer ;
	    buffer = NULL;
	    buffer_size = 0;
	    buffer_pos = 0;
//printf("buffer cleaned\n");
	}
//printf("read %d bytes from buffer\n", len );
    }
    if( len<size ) {
	int ret;
	ret = tcp.read((uint8_t*)(_buffer+len), size-len);
	if( ret<0 ) {
	    MSG_ERR("nop_networking_read error : %s\n",strerror(errno));
	}
	len += ret;
//printf("read %d bytes from network\n", len );
    }
    return len;
}

int Nop_Networking::seek(Tcp& tcp, off_t pos) {
    UNUSED(tcp);
    UNUSED(pos);
    return -1;
}

Networking* Nop_Networking::start(Tcp& tcp,network_protocol_t& protocol) {
    HTTP_Header *http_hdr = NULL;
    const char *next_url=NULL;
    URL *rd_url=NULL;

    Nop_Networking* rv = new(zeromem) Nop_Networking;
    if( !tcp.established() ) {
	http_send_request(tcp, protocol.url,0);
	if( !tcp.established() ) {
	    delete rv;
	    return NULL;
	}
	http_hdr = http_read_response(tcp);
	if( http_hdr==NULL ) {
	    delete rv;
	    return NULL;
	}

	switch( http_hdr->get_status() ) {
	    case 200: // OK
		MSG_V("Content-Type: [%s]\n", http_hdr->get_field("Content-Type") );
		MSG_V("Content-Length: [%s]\n", http_hdr->get_field("Content-Length") );
		if( http_hdr->get_body_size()>0 ) {
		    if( rv->bufferize((unsigned char *)http_hdr->get_body(), http_hdr->get_body_size() )<0 ) {
			delete http_hdr;
			delete rv;
			return NULL;
		    }
		}
		break;
	    // Redirect
	    case 301: // Permanently
	    case 302: // Temporarily
		next_url = http_hdr->get_field("Location" );

		if (next_url != NULL)
		    rd_url=url_new(next_url);

		if (next_url != NULL && rd_url != NULL) {
		    MSG_STATUS("Redirected: Using this url instead %s\n",next_url);
		    protocol.url=check4proxies(rd_url);
		    delete rv;
		    rv=static_cast<Nop_Networking*>(Nop_Networking::start(tcp,protocol)); //recursively get networking started
		} else {
		    MSG_ERR("Redirection failed\n");
		    tcp.close();
		}
		return rv;
		break;
	    case 401: //Authorization required
	    case 403: //Forbidden
	    case 404: //Not found
	    case 500: //Server Error
	    default:
		MSG_ERR("Server return %d: %s\n", http_hdr->get_status(), http_hdr->get_reason_phrase());
		tcp.close();
		delete rv;
		return NULL;
	}
    } else {
	http_hdr = (HTTP_Header*)protocol.data;
	if( http_hdr->get_body_size()>0 ) {
	    if( rv->bufferize((unsigned char*)http_hdr->get_body(), http_hdr->get_body_size() )<0 ) {
		delete http_hdr;
		delete rv;
		return NULL;
	    }
	}
    }

    if( http_hdr ) delete http_hdr;

    rv->data = NULL;
    rv->prebuffer_size = 64*1024;	// KBytes
    rv->buffering = 1;
    rv->status = networking_playing_e;
    return rv;
}

Nop_Networking::Nop_Networking() {}
Nop_Networking::~Nop_Networking() {}
} // namespace mpxp
