#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
/*
 * HTTP Helper
 * by Bertrand Baudet <bertrand_baudet@yahoo.com>
 * (C) 2001, MPlayer team.
 */
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
    HTTP_field_t *field, *field2free;
    if( protocol!=NULL ) delete protocol ;
    if( uri!=NULL ) delete uri ;
    if( reason_phrase!=NULL ) delete reason_phrase ;
    if( field_search!=NULL ) delete field_search ;
    if( method!=NULL ) delete method ;
    if( buffer!=NULL ) delete buffer ;
    field = first_field;
    while( field!=NULL ) {
	field2free = field;
	if (field->field_name) delete field->field_name;
	field = field->next;
	delete field2free;
    }
}

int HTTP_Header::response_append(const char *response, int length ) {
    if( response==NULL || length<0 ) return -1;

    if( (unsigned)length > std::numeric_limits<size_t>::max() - buffer_size - 1) {
	MSG_FATAL("Bad size in memory (re)allocation\n");
	return -1;
    }
    buffer = (char*)mp_realloc( buffer, buffer_size+length+1 );
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

    if( strstr(buffer, "\r\n\r\n")==NULL &&
	strstr(buffer, "\n\n")==NULL ) return 0;
    return 1;
}

int HTTP_Header::response_parse( ) {
    char *hdr_ptr, *ptr;
    char *field=NULL;
    int pos_hdr_sep, hdr_sep_len;
    size_t len;
    if( is_parsed ) return 0;

    // Get the protocol
    hdr_ptr = strstr( buffer, " " );
    if( hdr_ptr==NULL ) {
	MSG_FATAL("Malformed answer. No space separator found.\n");
	return -1;
    }
    len = hdr_ptr-buffer;
    protocol = new char [len+1];
    if( protocol==NULL ) {
	MSG_FATAL("Memory allocation failed\n");
	return -1;
    }
    strncpy( protocol, buffer, len );
    protocol[len]='\0';
    if( !strncasecmp( protocol, "HTTP", 4) ) {
	if( sscanf( protocol+5,"1.%d", &(http_minor_version) )!=1 ) {
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
    reason_phrase = new char[len+1];
    if( reason_phrase==NULL ) {
	MSG_FATAL("Memory allocation failed\n");
	return -1;
    }
    strncpy( reason_phrase, hdr_ptr, len );
    if( reason_phrase[len-1]=='\r' ) {
	len--;
    }
    reason_phrase[len]='\0';

    // Set the position of the header separator: \r\n\r\n
    hdr_sep_len = 4;
    ptr = strstr( buffer, "\r\n\r\n" );
    if( ptr==NULL ) {
	ptr = strstr( buffer, "\n\n" );
	if( ptr==NULL ) {
	    MSG_ERR("Header may be incomplete. No CRLF CRLF found.\n");
	    return -1;
	}
	hdr_sep_len = 2;
    }
    pos_hdr_sep = ptr-buffer;

    // Point to the first line after the method line.
    hdr_ptr = strstr( buffer, "\n" )+1;
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
    } while( hdr_ptr<(buffer+pos_hdr_sep) );

    if( field!=NULL ) delete field ;

    if( pos_hdr_sep+hdr_sep_len<buffer_size ) {
	// Response has data!
	body = (unsigned char*)buffer+pos_hdr_sep+hdr_sep_len;
	body_size = buffer_size-(pos_hdr_sep+hdr_sep_len);
    }

    is_parsed = 1;
    return 0;
}

char* HTTP_Header::build_request() {
    char *ptr;
    int len;
    HTTP_field_t *field;

    if( method==NULL ) set_method( "GET");
    if( uri==NULL ) set_uri( "/");

    //**** Compute the request length
    // Add the Method line
    len = strlen(method)+strlen(uri)+12;
    // Add the fields
    field = first_field;
    while( field!=NULL ) {
	len += strlen(field->field_name)+2;
	field = field->next;
    }
    // Add the CRLF
    len += 2;
    // Add the body
    if( body!=NULL ) {
	len += body_size;
    }
    // Free the buffer if it was previously used
    if( buffer!=NULL ) {
	delete buffer ;
	buffer = NULL;
    }
    buffer = new char [len+1];
    if( buffer==NULL ) {
	MSG_FATAL("Memory allocation failed\n");
	return NULL;
    }
    buffer_size = len;

    //*** Building the request
    ptr = buffer;
    // Add the method line
    ptr += sprintf( ptr, "%s %s HTTP/1.%d\r\n", method,uri, http_minor_version );
    field = first_field;
    // Add the field
    while( field!=NULL ) {
	ptr += sprintf( ptr, "%s\r\n", field->field_name );
	field = field->next;
    }
    ptr += sprintf( ptr, "\r\n" );
    // Add the body
    if( body!=NULL ) {
	memcpy( ptr, body, body_size );
    }

    return buffer;
}

char* HTTP_Header::get_field(const char *field_name ) {
    if( field_name==NULL ) return NULL;
    field_search_pos = first_field;
    field_search = (char*)mp_realloc( field_search, strlen(field_name)+1 );
    if( field_search==NULL ) {
	MSG_FATAL("Memory allocation failed\n");
	return NULL;
    }
    strcpy( field_search, field_name );
    return get_next_field();
}

char* HTTP_Header::get_next_field() {
    char *ptr;
    HTTP_field_t *field;

    field = field_search_pos;
    while( field!=NULL ) {
	ptr = strstr( field->field_name, ":" );
	if( ptr==NULL ) return NULL;
	if( !strncasecmp( field->field_name, field_search, ptr-(field->field_name) ) ) {
	    ptr++;	// Skip the column
	    while( ptr[0]==' ' ) ptr++; // Skip the spaces if there is some
	    field_search_pos = field->next;
	    return ptr;	// return the value without the field name
	}
	field = field->next;
    }
    return NULL;
}

void HTTP_Header::set_field(const char *field_name ) {
    HTTP_field_t *new_field;
    if( field_name==NULL ) return;

    new_field = new(zeromem) HTTP_field_t;
    if( new_field==NULL ) {
	MSG_FATAL("Memory allocation failed\n");
	return;
    }
    new_field->next = NULL;
    new_field->field_name = new char [strlen(field_name)+1];
    if( new_field->field_name==NULL ) {
	MSG_FATAL("Memory allocation failed\n");
	delete new_field;
	return;
    }
    strcpy( new_field->field_name, field_name );

    if( last_field==NULL ) {
	first_field = new_field;
    } else {
	last_field->next = new_field;
    }
    last_field = new_field;
    field_nb++;
}

void HTTP_Header::set_method( const char *_method ) {
    if( _method==NULL ) return;
    method=mp_strdup(_method);
}

void HTTP_Header::set_uri(const char *_uri ) {
    if(_uri==NULL ) return;
    uri=mp_strdup(_uri);
}

int HTTP_Header::add_basic_authentication( const char *username, const char *password ) {
    char *auth=NULL, *usr_pass=NULL, *b64_usr_pass=NULL;
    int encoded_len, pass_len=0, out_len;
    int res = -1;
    if( username==NULL ) return -1;

    if( password!=NULL ) pass_len = strlen(password);

    usr_pass = new char [strlen(username)+pass_len+2];
    if( usr_pass==NULL ) {
	MSG_FATAL("Memory allocation failed\n");
	goto out;
    }

    sprintf( usr_pass, "%s:%s", username, (password==NULL)?"":password );

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

void HTTP_Header::debug_hdr( ) {
    HTTP_field_t *field;
    int i = 0;

    MSG_V(	"--- HTTP DEBUG HEADER --- START ---\n"
		"protocol:           [%s]\n"
		"http minor version: [%d]\n"
		"uri:                [%s]\n"
		"method:             [%s]\n"
		"status code:        [%d]\n"
		"reason phrase:      [%s]\n"
		"body size:          [%d]\n"
		,protocol
		,http_minor_version
		,uri
		,method
		,status_code
		,reason_phrase
		,body_size );

    MSG_V("Fields:\n");
    field = first_field;
    while( field!=NULL ) {
	MSG_V(" %d - %s\n", i++, field->field_name );
	field = field->next;
    }
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
