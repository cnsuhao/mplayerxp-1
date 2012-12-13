#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
/*
 * HTTP Helper
 * by Bertrand Baudet <bertrand_baudet@yahoo.com>
 * (C) 2001, MPlayer team.
 */
#include <algorithm>
#include <ctype.h>
#include <limits>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "http.h"
#include "url.h"
#include "stream_msg.h"

namespace mpxp {
HTTP_Header::HTTP_Header() {}
HTTP_Header::~HTTP_Header() {
    if( buffer!=NULL ) delete buffer ;
}

int HTTP_Header::response_append(const uint8_t* response, size_t length ) {
    if( response==NULL) return -1;

    if( (unsigned)length > std::numeric_limits<size_t>::max() - buffer_size - 1) {
	MSG_FATAL("Bad size in memory (re)allocation\n");
	return -1;
    }
    buffer = (uint8_t*)mp_realloc( buffer, buffer_size+length+1 );
    if(buffer ==NULL ) {
	MSG_FATAL("Memory allocation failed\n");
	return -1;
    }
    memcpy( buffer+buffer_size, response, length );
    buffer_size += length;
    buffer[buffer_size]=0; // close the string!
    return buffer_size;
}

int HTTP_Header::is_header_entire() const {
    if( buffer==NULL ) return 0; // empty

    if( strstr((char*)buffer, "\r\n\r\n")==NULL &&
	strstr((char*)buffer, "\n\n")==NULL ) return 0;
    return 1;
}

int HTTP_Header::response_parse( ) {
    char *hdr_ptr, *ptr;
    char *field=NULL;
    int pos_hdr_sep, hdr_sep_len;
    size_t len;
    if( is_parsed ) return 0;

    // Get the protocol
    hdr_ptr = strstr( (char*)buffer, " " );
    if( hdr_ptr==NULL ) {
	MSG_FATAL("Malformed answer. No space separator found.\n");
	return -1;
    }
    len = hdr_ptr-(char*)buffer;
    protocol.assign((char*)buffer,len);
    std::string sstr=protocol.substr(0,4);
    std::transform(sstr.begin(), sstr.end(),sstr.begin(), ::toupper);
    if(sstr=="HTTP") {
	if( sscanf( &protocol.c_str()[5],"1.%d", &(http_minor_version) )!=1 ) {
	    MSG_FATAL("Malformed answer. Unable to get HTTP minor version.\n");
	    return -1;
	}
    }

    // Get the status code
    if( sscanf( ++hdr_ptr, "%d", &status_code )!=1 ) {
	MSG_FATAL("Malformed answer. Unable to get status code.\n");
	return -1;
    }
    hdr_ptr += 4;

    // Get the reason phrase
    ptr = strstr( hdr_ptr, "\n" );
    if( hdr_ptr==NULL ) {
	MSG_FATAL("Malformed answer. Unable to get the reason phrase.\n");
	return -1;
    }
    len = ptr-hdr_ptr;
    reason_phrase.assign(hdr_ptr, len);
    if( reason_phrase[len-1]=='\r' ) {
	len--;
	reason_phrase.resize(len);
    }
    reason_phrase[len]='\0';

    // Set the position of the header separator: \r\n\r\n
    hdr_sep_len = 4;
    ptr = strstr( (char*)buffer, "\r\n\r\n" );
    if( ptr==NULL ) {
	ptr = strstr( (char*)buffer, "\n\n" );
	if( ptr==NULL ) {
	    MSG_ERR("Header may be incomplete. No CRLF CRLF found.\n");
	    return -1;
	}
	hdr_sep_len = 2;
    }
    pos_hdr_sep = ptr-(char*)buffer;

    // Point to the first line after the method line.
    hdr_ptr = strstr( (char*)buffer, "\n" )+1;
    do {
	ptr = hdr_ptr;
	while( *ptr!='\r' && *ptr!='\n' ) ptr++;
	len = ptr-hdr_ptr;
	if( len==0 ) break;
	field = (char*)mp_realloc(field, len+1);
	if( field==NULL ) {
	    MSG_FATAL("Memory allocation failed\n");
	    return -1;
	}
	strncpy( field, hdr_ptr, len );
	field[len]='\0';
	set_field( field );
	hdr_ptr = ptr+((*ptr=='\r')?2:1);
    } while( hdr_ptr<((char*)buffer+pos_hdr_sep) );

    if( field!=NULL ) delete field ;

    if( pos_hdr_sep+hdr_sep_len<buffer_size ) {
	// Response has data!
	body.assign((char*)buffer+pos_hdr_sep+hdr_sep_len,buffer_size-(pos_hdr_sep+hdr_sep_len));
    }

    is_parsed = 1;
    return 0;
}

const char* HTTP_Header::build_request() {
    char *ptr;
    int len;

    if( method.empty() ) set_method( "GET");
    if( uri.empty() ) set_uri( "/");

    //**** Compute the request length
    // Add the Method line
    len = method.length()+uri.length()+12;
    // Add the fields
    std::vector<std::string>::size_type sz = fields.size();
    for(unsigned i=0;i<sz;i++) len+= fields[i].length()+2;
    // Add the CRLF
    len += 2;
    // Add the body
    if( !body.empty() ) {
	len += body.length();
    }
    // Free the buffer if it was previously used
    if( buffer!=NULL ) {
	delete buffer ;
	buffer = NULL;
    }
    buffer = new uint8_t [len+1];
    if( buffer==NULL ) {
	MSG_FATAL("Memory allocation failed\n");
	return NULL;
    }
    buffer_size = len;

    //*** Building the request
    ptr = (char*)buffer;
    // Add the method line
    ptr += sprintf( ptr, "%s %s HTTP/1.%d\r\n", method.c_str(),uri.c_str(), http_minor_version );
    // Add the field
    for(unsigned i=0;i<sz;i++) ptr += sprintf( ptr, "%s\r\n", fields[i].c_str());
    ptr += sprintf( ptr, "\r\n" );
    // Add the body
    if( !body.empty()) {
	memcpy( ptr, body.c_str(), body.size());
    }

    return (char *)buffer;
}

const char* HTTP_Header::get_field(const std::string& field_name ) {
    search_pos=0;
    field_search=field_name;
    return get_next_field();
}

const char* HTTP_Header::get_next_field() {
    std::vector<std::string>::size_type sz = fields.size();
    for(unsigned i=search_pos;i<sz;i++) {
	size_t pos,epos;
	const std::string& str = fields[i];
	pos=0;
	epos=str.find(':',pos);
	if(epos!=std::string::npos) {
	    if(str.substr(pos,epos)==field_search) {
		epos++; // skip the column
		pos=epos;
		while(::isspace(str[epos])) epos++;
		search_pos=i+1;
		return &str[epos];
	    }
	}
    }
    return NULL;
}

void HTTP_Header::set_field(const std::string& field_name ) {
    if(field_name.empty()) return;
    fields.push_back(field_name);
}

void HTTP_Header::set_method( const std::string& _method ) {
    method=_method;
}

void HTTP_Header::set_uri(const std::string& _uri ) {
    uri=_uri;
}

int HTTP_Header::add_basic_authentication( const std::string& username, const std::string& password ) {
    char *auth=NULL, *usr_pass=NULL, *b64_usr_pass=NULL;
    int encoded_len, pass_len=0, out_len;
    int res = -1;
    if( username.empty() ) return -1;

    if( !password.empty() ) pass_len = password.length();

    usr_pass = new char [username.length()+pass_len+2];
    if( usr_pass==NULL ) {
	MSG_FATAL("Memory allocation failed\n");
	goto out;
    }

    sprintf( usr_pass, "%s:%s", username.c_str(), password.c_str() );

    // Base 64 encode with at least 33% more data than the original size
    encoded_len = strlen(usr_pass)*2;
    b64_usr_pass = new char [encoded_len];
    if( b64_usr_pass==NULL ) {
	MSG_FATAL("Memory allocation failed\n");
	goto out;
    }

    out_len = base64_encode( usr_pass, strlen(usr_pass), b64_usr_pass, encoded_len);
    if( out_len<0 ) {
	MSG_FATAL("Base64 out overflow\n");
	goto out;
    }

    b64_usr_pass[out_len]='\0';

    auth = new char [encoded_len+22];
    if( auth==NULL ) {
	MSG_FATAL("Memory allocation failed\n");
	goto out;
    }

    sprintf( auth, "Authorization: Basic %s", b64_usr_pass);
    set_field( auth );
    res = 0;
out:
    delete usr_pass ;
    delete b64_usr_pass ;
    delete auth ;

    return res;
}

void HTTP_Header::erase_body() {
    body.clear();
}

void HTTP_Header::debug_hdr( ) {
    unsigned i = 0;

    MSG_V(	"--- HTTP DEBUG HEADER --- START ---\n"
		"protocol:           [%s]\n"
		"http minor version: [%d]\n"
		"uri:                [%s]\n"
		"method:             [%s]\n"
		"status code:        [%d]\n"
		"reason phrase:      [%s]\n"
		"body size:          [%d]\n"
		,protocol.c_str()
		,http_minor_version
		,uri.c_str()
		,method.c_str()
		,status_code
		,reason_phrase.c_str()
		,body.length() );

    MSG_V("Fields:\n");
    std::vector<std::string>::size_type sz = fields.size();
    for(i=0;i<sz;i++) MSG_V(" %d - %s\n", i, fields[i].c_str());
    MSG_V("--- HTTP DEBUG HEADER --- END ---\n");
}

int
base64_encode(const any_t*enc, int encLen, char *out, int outMax) {
	static const char	b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

	unsigned char		*encBuf;
	int			outLen;
	unsigned int		bits;
	unsigned int		shift;

	encBuf = (unsigned char*)enc;
	outLen = 0;
	bits = 0;
	shift = 0;
	outMax &= ~3;

	while( 1 ) {
		if( encLen>0 ) {
			// Shift in byte
			bits <<= 8;
			bits |= *encBuf;
			shift += 8;
			// Next byte
			encBuf++;
			encLen--;
		} else if( shift>0 ) {
			// Pad last bits to 6 bits - will end next loop
			bits <<= 6 - shift;
			shift = 6;
		} else {
			// As per RFC 2045, section 6.8,
			// pad output as necessary: 0 to 2 '=' chars.
			while( outLen & 3 ){
				*out++ = '=';
				outLen++;
			}

			return outLen;
		}

		// Encode 6 bit segments
		while( shift>=6 ) {
			if (outLen >= outMax)
				return -1;
			shift -= 6;
			*out = b64[ (bits >> shift) & 0x3F ];
			out++;
			outLen++;
		}
	}

	// Output overflow
	return -1;
}
}// namespace mpxp
