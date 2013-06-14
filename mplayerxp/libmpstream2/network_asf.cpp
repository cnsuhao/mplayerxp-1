#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
#include <algorithm>
#include <limits>

#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "mplayerxp.h"
#ifndef HAVE_WINSOCK2
#define closesocket close
#else
#include <winsock2.h>
#endif

#include "url.h"
#include "tcp.h"
#include "http.h"

#include "stream.h"
#include "network_asf.h"
#include "libmpdemux/asf.h"
#include "network.h"
#include "stream_msg.h"

namespace	usr {
#if defined( ARCH_X86 ) || defined(ARCH_X86_64)
static inline uint32_t ASF_LOAD_GUID_PREFIX(uint8_t* guid) { return *(uint32_t*)guid; }
#else
static inline uint32_t ASF_LOAD_GUID_PREFIX(uint8_t* guid) { return bswap_32(*(uint32_t*)guid); }
#endif

// ASF networking support several network protocol.
// One use UDP, not known, yet!
// Another is HTTP, this one is known.
// So for now, we use the HTTP protocol.
//
// We can try several protocol for asf networking
// * first the UDP protcol, if there is a firewall, UDP
//   packets will not come back, so the mmsu will failed.
// * Then we can try TCP, but if there is a proxy for
//   internet connection, the TCP connection will not get
//   through
// * Then we can try HTTP.
//
// Note: 	MMS/HTTP support is now a "well known" support protocol,
// 		it has been tested for while, not like MMST support.
// 		WMP sequence is MMSU then MMST and then HTTP.
// 		In MPlayer case since HTTP support is more reliable,
// 		we are doing HTTP first then we try MMST if HTTP fail.

static int
asf_networking(ASF_stream_chunck_t *stream_chunck, int *drop_packet ) {
    if( drop_packet!=NULL ) *drop_packet = 0;

    if( stream_chunck->size<8 ) {
	mpxp_err<<"Ahhhh, stream_chunck size is too small: "<<stream_chunck->size<<std::endl;
	return -1;
    }
    if( stream_chunck->size!=stream_chunck->size_confirm ) {
	mpxp_err<<"size_confirm mismatch!: "<<stream_chunck->size<<" "<<stream_chunck->size_confirm<<std::endl;
	return -1;
    }
    switch(stream_chunck->type) {
	case ASF_STREAMING_CLEAR:	// $C	Clear ASF configuration
	    mpxp_v<<"=====> Clearing ASF stream configuration!"<<std::endl;
	    if( drop_packet!=NULL ) *drop_packet = 1;
	    return stream_chunck->size;
	    break;
	case ASF_STREAMING_DATA:	// $D	Data follows
	    break;
	case ASF_STREAMING_END_TRANS:	// $E	Transfer complete
	    mpxp_v<<"=====> Transfer complete"<<std::endl;
	    if( drop_packet!=NULL ) *drop_packet = 1;
	    return stream_chunck->size;
	    break;
	case ASF_STREAMING_HEADER:	// $H	ASF header chunk follows
	    mpxp_v<<"=====> ASF header chunk follows"<<std::endl;
	    break;
	default:
	    mpxp_v<<"=====> Unknown stream type 0x"<<std::hex<<stream_chunck->type<<std::endl;
    }
    return stream_chunck->size+4;
}

static const uint8_t asf_stream_header_guid[16] = {0x91, 0x07, 0xdc, 0xb7,
  0xb7, 0xa9, 0xcf, 0x11, 0x8e, 0xe6, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65};
static const uint8_t asf_file_header_guid[16] = {0xa1, 0xdc, 0xab, 0x8c,
  0x47, 0xa9, 0xcf, 0x11, 0x8e, 0xe4, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65};
static const uint8_t asf_content_desc_guid[16] = {0x33, 0x26, 0xb2, 0x75,
  0x8e, 0x66, 0xcf, 0x11, 0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c};
static const uint8_t asf_stream_group_guid[16] = {0xce, 0x75, 0xf8, 0x7b,
  0x8d, 0x46, 0xd1, 0x11, 0x8d, 0x82, 0x00, 0x60, 0x97, 0xc9, 0xa2, 0xb2};
static const uint8_t asf_data_chunk_guid[16] = {0x36, 0x26, 0xb2, 0x75,
  0x8e, 0x66, 0xcf, 0x11, 0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c};
static int find_asf_guid(char *buf, const uint8_t *guid, int cur_pos, int buf_len)
{
  int i;
  for (i = cur_pos; i < buf_len - 19; i++) {
    if (memcmp(&buf[i], guid, 16) == 0)
      return i + 16 + 8; // point after guid + length
  }
  return -1;
}

static int max_idx(int s_count, int *s_rates, int bound) {
  int i, best = -1, rate = -1;
  for (i = 0; i < s_count; i++) {
    if (s_rates[i] > rate && s_rates[i] <= bound) {
      rate = s_rates[i];
      best = i;
    }
  }
  return best;
}

MPXP_Rc Asf_Networking::parse_header(Tcp& tcp) {
    ASF_header_t asfh;
    ASF_stream_chunck_t chunk;
    char* _buffer=NULL, *chunk_buffer=NULL;
    int i,r,size,pos = 0;
    int _start;
    int _buffer_size = 0;
    int chunk_size2read = 0;
    int bw = bandwidth;
    int *v_rates = NULL, *a_rates = NULL;
    int v_rate = 0, a_rate = 0, a_idx = -1, v_idx = -1;

    // The ASF header can be in several network chunks. For example if the content description
    // is big, the ASF header will be split in 2 network chunk.
    // So we need to retrieve all the chunk before starting to parse the header.
    do {
	for( r=0; r < (int)sizeof(ASF_stream_chunck_t) ; ) {
	    i = Nop_Networking::read(tcp,((char*)&chunk)+r,sizeof(ASF_stream_chunck_t) - r);
	    if(i <= 0) return MPXP_False;
	    r += i;
	}
	// Endian handling of the stream chunk
	le2me_ASF_stream_chunck_t(&chunk);
	size = asf_networking( &chunk, &r) - sizeof(ASF_stream_chunck_t);
	if(r) mpxp_warn<<"Warning : drop header ????"<<std::endl;
	if(size < 0){
	    mpxp_err<<"Error while parsing chunk header"<<std::endl;
	    return MPXP_False;
	}
	if (chunk.type != ASF_STREAMING_HEADER) {
	    mpxp_err<<"Don't got a header as first chunk !!!!"<<std::endl;
	    return MPXP_False;
	}
	// audit: do not overflow buffer_size
	if (unsigned(size) > std::numeric_limits<size_t>::max() - _buffer_size) return MPXP_False;
	_buffer = new char[size+_buffer_size];
	if( chunk_buffer!=NULL ) {
	    memcpy( _buffer, chunk_buffer, _buffer_size );
	    delete chunk_buffer ;
	}
	chunk_buffer = _buffer;
	_buffer += _buffer_size;
	_buffer_size += size;

	for(r = 0; r < size;) {
	    i = Nop_Networking::read(tcp,_buffer+r,size-r);
	    if(i < 0) {
		mpxp_err<<"Error while reading network stream"<<std::endl;
		return MPXP_False;
	    }
	    r += i;
	}

	if( chunk_size2read==0 ) {
	    if(size < (int)sizeof(asfh)) {
		mpxp_err<<"Error chunk is too small"<<std::endl;
		return MPXP_False;
	    } else mpxp_dbg2<<"Got chunk"<<std::endl;
	    memcpy(&asfh,_buffer,sizeof(asfh));
	    le2me_ASF_header_t(&asfh);
	    chunk_size2read = asfh.objh.size;
	    mpxp_dbg2<<"Size 2 read="<<chunk_size2read<<std::endl;
	}
    } while( _buffer_size<chunk_size2read);
    _buffer = chunk_buffer;
    size = _buffer_size;

    if(asfh.cno > 256) {
	mpxp_err<<"Error sub chunks number is invalid"<<std::endl;
	return MPXP_False;
    }

    _start = sizeof(asfh);

    pos = find_asf_guid(_buffer, asf_file_header_guid, _start, size);
    if (pos >= 0) {
	ASF_file_header_t *fileh = (ASF_file_header_t *) &_buffer[pos];
	pos += sizeof(ASF_file_header_t);
	if (pos > size) goto len_err_out;
	le2me_ASF_file_header_t(fileh);
	packet_size = fileh->max_packet_size;
	// before playing.
	// preroll: time in ms to bufferize before playing
	prebuffer_size = (unsigned int)(((double)fileh->preroll/1000.0)*((double)fileh->max_bitrate/8.0));
    }

    pos = _start;
    while ((pos = find_asf_guid(_buffer, asf_stream_header_guid, pos, size)) >= 0) {
	ASF_stream_header_t *streamh = (ASF_stream_header_t *)&_buffer[pos];
	pos += sizeof(ASF_stream_header_t);
	if (pos > size) goto len_err_out;
	le2me_ASF_stream_header_t(streamh);
	switch(ASF_LOAD_GUID_PREFIX(streamh->type)) {
	    case 0xF8699E40 : // audio stream
		if(audio_streams == NULL){
		    audio_streams = new int;
		    n_audio = 1;
		} else {
		    n_audio++;
		    audio_streams = (int*)mp_realloc(audio_streams,
							n_audio*sizeof(int));
		}
		audio_streams[n_audio-1] = streamh->stream_no;
		break;
	    case 0xBC19EFC0 : // video stream
		if(video_streams == NULL){
		    video_streams = new int;
		    n_video = 1;
		} else {
		    n_video++;
		    video_streams = (int*)mp_realloc(video_streams,
						     n_video*sizeof(int));
		}
		video_streams[n_video-1] = streamh->stream_no;
		break;
	}
    }

    // always allocate to avoid lots of ifs later
    v_rates =new(zeromem) int [n_video];
    a_rates =new(zeromem) int [n_audio];

    pos = find_asf_guid(_buffer, asf_stream_group_guid, _start, size);
    if (pos >= 0) {
	// stream bitrate properties object
	int stream_count;
	char *ptr = &_buffer[pos];

	mpxp_v<<"Stream bitrate properties object"<<std::endl;
	stream_count = le2me_16(*(uint16_t*)ptr);
	ptr += sizeof(uint16_t);
	if (ptr > &_buffer[size]) goto len_err_out;
	mpxp_v<<" stream count=[0x"<<std::hex<<stream_count<<"]["<<stream_count<<"]"<<std::endl;
	for( i=0 ; i<stream_count ; i++ ) {
	    uint32_t rate;
	    int id;
	    int j;
	    id = le2me_16(*(uint16_t*)ptr);
	    ptr += sizeof(uint16_t);
	    if (ptr > &_buffer[size]) goto len_err_out;
	    memcpy(&rate, ptr, sizeof(uint32_t));// workaround unaligment bug on sparc
	    ptr += sizeof(uint32_t);
	    if (ptr > &_buffer[size]) goto len_err_out;
	    rate = le2me_32(rate);
	    mpxp_v<<"  stream id=[0x"<<std::hex<<id<<"]["<<id<<"]"<<std::endl;
	    mpxp_v<<"  max bitrate=[0x"<<std::hex<<id<<"]["<<id<<"]"<<std::endl;
	    for (j = 0; j < n_video; j++) {
		if (id == video_streams[j]) {
		    mpxp_v<<"  is video stream"<<std::endl;
		    v_rates[j] = rate;
		    break;
		}
	    }
	    for (j = 0; j < n_audio; j++) {
		if (id == audio_streams[j]) {
		    mpxp_v<<"  is audio stream"<<std::endl;
		    a_rates[j] = rate;
		    break;
		}
	    }
	}
    }
    delete _buffer;

    // automatic stream selection based on bandwidth
    if (bw == 0) bw = INT_MAX;
    mpxp_v<<"Max bandwidth set to "<<bw<<std::endl;

    if (n_audio) {
	// find lowest-bitrate audio stream
	a_rate = a_rates[0];
	a_idx = 0;
	for (i = 0; i < n_audio; i++) {
	    if (a_rates[i] < a_rate) {
		a_rate = a_rates[i];
		a_idx = i;
	    }
	}
	if (max_idx(n_video, v_rates, bw - a_rate) < 0) {
	    // both audio and video are not possible, try video only next
	    a_idx = -1;
	    a_rate = 0;
	}
    }
    // find best video stream
    v_idx = max_idx(n_video, v_rates, bw - a_rate);
    if (v_idx >= 0) v_rate = v_rates[v_idx];
    // find best audio stream
    a_idx = max_idx(n_audio, a_rates, bw - v_rate);

    delete v_rates;
    delete a_rates;

    if (a_idx < 0 && v_idx < 0) {
	mpxp_fatal<<"bandwidth too small, file cannot be played!"<<std::endl;
	return MPXP_False;
    }

    // a audio stream was forced
    if (mp_conf.audio_id > 0) audio_id = mp_conf.audio_id;
    else if (a_idx >= 0) audio_id = audio_streams[a_idx];
    else if (n_audio) {
	mpxp_warn<<"bandwidth too small, deselected audio stream"<<std::endl;
	mp_conf.audio_id = -2;
    }

    // a video stream was forced
    if (mp_conf.video_id > 0) video_id = mp_conf.video_id;
    else if (v_idx >= 0) video_id = video_streams[v_idx];
    else if (n_video) {
	mpxp_warn<<"bandwidth too small, deselected video stream"<<std::endl;
	mp_conf.video_id = -2;
    }
    return MPXP_Ok;

len_err_out:
    mpxp_fatal<<"Invalid length in ASF header!"<<std::endl;
    if (_buffer) delete _buffer;
    if (v_rates) delete v_rates;
    if (a_rates) delete a_rates;
    return MPXP_False;
}

int Asf_Networking::read( Tcp& tcp, char *_buffer, int size) {
  static ASF_stream_chunck_t chunk;
  int _read,chunk_size = 0;
  static int rest = 0, drop_chunk = 0, waiting = 0;

  while(1) {
    if (rest == 0 && waiting == 0) {
      _read = 0;
      while(_read < (int)sizeof(ASF_stream_chunck_t)){
	int r = Nop_Networking::read( tcp, ((char*)&chunk) + _read,
				    sizeof(ASF_stream_chunck_t)-_read);
	if(r <= 0){
	  if( r < 0)
	    mpxp_err<<"Error while reading chunk header"<<std::endl;
	  return -1;
	}
	_read += r;
      }

      // Endian handling of the stream chunk
      le2me_ASF_stream_chunck_t(&chunk);
      chunk_size = asf_networking( &chunk, &drop_chunk );
      if(chunk_size < 0) {
	mpxp_err<<"Error while parsing chunk header"<<std::endl;
	return -1;
      }
      chunk_size -= sizeof(ASF_stream_chunck_t);

      if(chunk.type != ASF_STREAMING_HEADER && (!drop_chunk)) {
	if (packet_size < chunk_size) {
	  mpxp_err<<"Error chunk_size > packet_size"<<std::endl;
	  return -1;
	}
	waiting = packet_size;
      } else {
	waiting = chunk_size;
      }

    } else if (rest){
      chunk_size = rest;
      rest = 0;
    }

    _read = 0;
    if ( waiting >= chunk_size) {
      if (chunk_size > size){
	rest = chunk_size - size;
	chunk_size = size;
      }
      while(_read < chunk_size) {
	int got = Nop_Networking::read( tcp,_buffer+_read,chunk_size-_read);
	if(got <= 0) {
	  if(got < 0)
	    mpxp_err<<"Error while reading chunk"<<std::endl;
	  return -1;
	}
	_read += got;
      }
      waiting -= _read;
      if (drop_chunk) continue;
    }
    if (rest == 0 && waiting > 0 && size-_read > 0) {
      int s = std::min(waiting,size-_read);
      memset(_buffer+_read,0,s);
      waiting -= s;
      _read += s;
    }
    break;
  }

  return _read;
}

int Asf_Networking::seek( Tcp& tcp, off_t pos) {
    UNUSED(tcp);
    UNUSED(pos);
    return -1;
}

static int
asf_header_check( HTTP_Header& http_hdr ) {
    ASF_obj_header_t *objh;
    if( http_hdr.get_body()==NULL || http_hdr.get_body_size()<sizeof(ASF_obj_header_t) ) return -1;

    objh = (ASF_obj_header_t*)http_hdr.get_body();
    if( ASF_LOAD_GUID_PREFIX(objh->guid)==0x75B22630 ) return 0;
    return -1;
}

static ASF_StreamType_e
asf_http_networking_type(const char *content_type,const char *features, HTTP_Header& http_hdr ) {
    if(content_type==NULL ) return ASF_Unknown_e;
    if(!strcasecmp(content_type, "application/octet-stream") ||
	!strcasecmp(content_type, "application/vnd.ms.wms-hdr.asfv1") ||        // New in Corona, first request
	!strcasecmp(content_type, "application/x-mms-framed") ||                // New in Corana, second request
	!strcasecmp(content_type, "video/x-ms-asf")) {

	if( strstr(features, "broadcast") ) {
	    mpxp_v<<"=====> ASF Live stream"<<std::endl;
	    return ASF_Live_e;
	} else {
	    mpxp_v<<"=====> ASF Prerecorded"<<std::endl;
	    return ASF_Prerecorded_e;
	}
    } else {
	// Ok in a perfect world, web servers should be well configured
	// so we could used mime type to know the stream type,
	// but guess what? All of them are not well configured.
	// So we have to check for an asf header :(, but it works :p
	if( http_hdr.get_body_size()>sizeof(ASF_obj_header_t) ) {
	    if( asf_header_check( http_hdr )==0 ) {
		mpxp_v<<"=====> ASF Plain text"<<std::endl;
		return ASF_PlainText_e;
	    } else if( (!strcasecmp(content_type, "text/html")) ) {
		mpxp_v<<"=====> HTML, mplayer is not a browser...yet!"<<std::endl;
		return ASF_Unknown_e;
	    } else {
		mpxp_v<<"=====> ASF Redirector"<<std::endl;
		return ASF_Redirector_e;
	    }
	} else {
	    if((!strcasecmp(content_type, "audio/x-ms-wax")) ||
		(!strcasecmp(content_type, "audio/x-ms-wma")) ||
		(!strcasecmp(content_type, "video/x-ms-asf")) ||
		(!strcasecmp(content_type, "video/x-ms-afs")) ||
		(!strcasecmp(content_type, "video/x-ms-wvx")) ||
		(!strcasecmp(content_type, "video/x-ms-wmv")) ||
		(!strcasecmp(content_type, "video/x-ms-wma")) ) {
		    mpxp_err<<"=====> ASF Redirector"<<std::endl;
		    return ASF_Redirector_e;
	    } else if( !strcasecmp(content_type, "text/plain") ) {
		mpxp_v<<"=====> ASF Plain text"<<std::endl;
		return ASF_PlainText_e;
	    } else {
		mpxp_v<<"=====> ASF unknown content-type: "<<content_type<<std::endl;
		return ASF_Unknown_e;
	    }
	}
    }
    return ASF_Unknown_e;
}

HTTP_Header* Asf_Networking::http_request() const {
    HTTP_Header* http_hdr = new(zeromem) HTTP_Header;
//	URL *url = NULL;
    URL server_url;
    char str[250];
    char *ptr;
    int i, enable;

    int offset_hi=0, offset_lo=0, length=0;
    int asf_nb_stream=0, stream_id;

    // Common header for all requests.
    http_hdr->set_field("Accept: */*" );
    http_hdr->set_field("User-Agent: NSPlayer/4.1.0.3856" );
    http_hdr->add_basic_authentication(url.user(), url.password());

    // Check if we are using a proxy
    if( url.protocol2lower()=="http_proxy") {
	server_url.redirect(url.file());
	http_hdr->set_uri(server_url.url());
	sprintf( str, "Host: %.220s:%d", server_url.host().c_str(), server_url.port());
    } else {
	http_hdr->set_uri(url.file());
	sprintf( str, "Host: %.220s:%d", url.host().c_str(), url.port());
    }

    http_hdr->set_field(str );
    http_hdr->set_field("Pragma: xClientGUID={c77e7400-738a-11d2-9add-0020af0a3278}" );
    sprintf(str,"Pragma: no-cache,rate=1.000000,stream-time=0,stream-offset=%u:%u,request-context=%d,max-duration=%u",
		offset_hi, offset_lo, request, length );
    http_hdr->set_field( str );

    switch( networking_type ) {
	case ASF_Live_e:
	case ASF_Prerecorded_e:
	    http_hdr->set_field("Pragma: xPlayStrm=1" );
	    ptr = str;
	    ptr += sprintf( ptr, "Pragma: stream-switch-entry=");
	    if(n_audio > 0) {
		for( i=0; i<n_audio ; i++ ) {
		    stream_id = audio_streams[i];
		    if(stream_id == audio_id) enable = 0;
		    else {
			enable = 2;
			continue;
		    }
		    asf_nb_stream++;
		    ptr += sprintf(ptr, "ffff:%d:%d ", stream_id, enable);
		}
	    }
	    if(n_video > 0) {
		for( i=0; i<n_video ; i++ ) {
		    stream_id = video_streams[i];
		    if(stream_id == video_id) enable = 0;
		    else {
			enable = 2;
			continue;
		    }
		    asf_nb_stream++;
		    ptr += sprintf(ptr, "ffff:%d:%d ", stream_id, enable);
		}
	    }
	    http_hdr->set_field(str );
	    sprintf( str, "Pragma: stream-switch-count=%d", asf_nb_stream );
	    http_hdr->set_field( str );
	    break;
	case ASF_Redirector_e: break;
	case ASF_Unknown_e: // First request goes here.
	    break;
	default:
	    mpxp_err<<"Unknown asf stream type"<<std::endl;
    }

    http_hdr->set_field("Connection: Close" );
    http_hdr->build_request( );

    return http_hdr;
}

int Asf_Networking::parse_response(HTTP_Header& http_hdr ) {
    const char *content_type, *pragma;
    char features[64] = "\0";
    size_t len;
    if( http_hdr.response_parse()<0 ) {
	mpxp_err<<"Failed to parse HTTP response"<<std::endl;
	return -1;
    }
    switch( http_hdr.get_status()) {
	case 200:
	    break;
	case 401: // Authentication required
	    return ASF_Authenticate_e;
	default:
	    mpxp_err<<"Server return "<<http_hdr.get_status()<<":"<<http_hdr.get_reason_phrase()<<std::endl;
	    return -1;
    }

    content_type = http_hdr.get_field("Content-Type");

    pragma = http_hdr.get_field("Pragma");
    while( pragma!=NULL ) {
	const char *comma_ptr=NULL;
	const char *end;
	// The pragma line can get severals attributes
	// separeted with a comma ','.
	do {
	    if( !strncasecmp( pragma, "features=", 9) ) {
		pragma += 9;
		end = strstr( pragma, "," );
		if( end==NULL ) {
		    size_t s = strlen(pragma);
		    if(s > sizeof(features)) {
			mpxp_warn<<"ASF HTTP PARSE WARNING : Pragma "<<pragma<<" cuted from "<<s<<" bytes to "<<sizeof(features)<<std::endl;
			len = sizeof(features);
		    } else len = s;
		} else len = std::min(unsigned(end-pragma),unsigned(sizeof(features)));
		strncpy( features, pragma, len );
		features[len]='\0';
		break;
	    }
	    comma_ptr = strstr( pragma, "," );
	    if( comma_ptr!=NULL ) {
		pragma = comma_ptr+1;
		if( pragma[0]==' ' ) pragma++;
	    }
	} while( comma_ptr!=NULL );
	pragma = http_hdr.get_next_field();
    }
    networking_type = asf_http_networking_type( content_type, features, http_hdr );
    return 0;
}

Networking* Asf_Networking::start(Tcp& tcp, network_protocol_t& protocol) {
    HTTP_Header *http_hdr=NULL;
    URL& url = protocol.url;
    uint8_t buffer[BUFFER_SIZE];
    int i, ret;
    int done;
    int auth_retry = 0;
    const char *proto = protocol.url.protocol().c_str();

    // sanity check
    if (!(protocol.url.protocol2lower()=="http_proxy" ||
	protocol.url.protocol2lower()=="http")) {
	mpxp_err<<"Unknown protocol: "<<proto<<std::endl;
	return NULL;
    }
    mpxp_v<<"Trying ASF/HTTP..."<<std::endl;

    Asf_Networking* rv = new(zeromem) Asf_Networking;
    rv->url=protocol.url;
    rv->mime=protocol.mime;

    rv->networking_type = ASF_Unknown_e;
    rv->request = 1;
    rv->audio_streams = rv->video_streams = NULL;
    rv->n_audio = rv->n_video = 0;
    rv->data = NULL;

    do {
	done = 1;
	tcp.close();

	if( url.protocol2lower()=="http_proxy") url.assign_port(8080);
	else url.assign_port(80);
	tcp.open(url, Tcp::IP4);
	if( !tcp.established()) {
	    delete rv;
	    return NULL;
	}

	http_hdr = rv->http_request();
	mpxp_dbg2<<"Request ["<<http_hdr->get_buffer()<<"]"<<std::endl;
	for(i=0; i < (int)http_hdr->get_buffer_size() ; ) {
	    int r = tcp.write((uint8_t*)(http_hdr->get_buffer()+i), http_hdr->get_buffer_size()-i);
	    if(r<0) {
		mpxp_err<<"Socket write error: "<<strerror(errno)<<std::endl;
		delete rv;
		return NULL;
	    }
	    i += r;
	}
	delete http_hdr;
	http_hdr = new(zeromem) HTTP_Header;
	do {
	    i = tcp.read(buffer, BUFFER_SIZE);
	    if( i<=0 ) {
		perror("read");
		delete http_hdr;
		delete rv;
		return NULL;
	    }
	    http_hdr->response_append(buffer,i);
	} while( !http_hdr->is_header_entire());
	if( mp_conf.verbose>0 ) {
	    mpxp_dbg2<<"Response ["<<http_hdr->get_buffer()<<"]"<<std::endl;
	}
	ret = rv->parse_response(*http_hdr);
	if( ret<0 ) {
	    mpxp_err<<"Failed to parse header"<<std::endl;
	    delete http_hdr;
	    delete rv;
	    return NULL;
	}
	switch( rv->networking_type ) {
	    case ASF_Live_e:
	    case ASF_Prerecorded_e:
	    case ASF_PlainText_e:
		if( http_hdr->get_body_size()>0 ) {
		    if( rv->bufferize((unsigned char *)(http_hdr->get_body()), http_hdr->get_body_size())<0 ) {
			delete http_hdr;
			delete rv;
			return NULL;
		    }
		}
		if( rv->request==1 ) {
		    if( rv->networking_type!=ASF_PlainText_e ) {
			// First request, we only got the ASF header.
			ret = rv->parse_header(tcp);
			if(ret < 0) {
			    delete rv;
			    return NULL;
			}
			if(rv->n_audio == 0 && rv->n_video == 0) {
			    mpxp_err<<"No stream found"<<std::endl;
			    delete http_hdr;
			    delete rv;
			    return NULL;
			}
			rv->request++;
			done = 0;
		    } else done = 1;
		}
		break;
	    case ASF_Redirector_e:
		if( http_hdr->get_body_size()>0 ) {
		    if( rv->bufferize((unsigned char*)http_hdr->get_body(), http_hdr->get_body_size())<0 ) {
			delete http_hdr;
			delete rv;
			return NULL;
		    }
		}
//		type |= STREAMTYPE_TEXT;
		done = 1;
		break;
	    case ASF_Authenticate_e:
		if( http_hdr->authenticate(url, &auth_retry)<0 ) {
		    delete http_hdr;
		    delete rv;
		    return NULL;
		}
		rv->networking_type = ASF_Unknown_e;
		done = 0;
		break;
	    case ASF_Unknown_e:
	    default:
		mpxp_err<<"Unknown ASF networking type"<<std::endl;
		tcp.close();
		delete http_hdr;
		delete rv;
		return NULL;
	}
    // Check if we got a redirect.
    } while(!done);

    if( rv->networking_type==ASF_PlainText_e || rv->networking_type==ASF_Redirector_e ) {
	delete rv;
	return Nop_Networking::start(tcp,protocol);
    } else rv->buffering = 1;
    rv->status = networking_playing_e;

    delete http_hdr;
    return rv;
}

Asf_Networking::Asf_Networking() {}
Asf_Networking::~Asf_Networking() {}
} // namespace	usr
