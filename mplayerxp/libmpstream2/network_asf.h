#ifndef __NETWORK_ASF_H_INCLUDED
#define __NETWORK_ASF_H_INCLUDED 1
#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;

#include "stream.h"
#include "network_nop.h"

struct Networking;
namespace mpxp {
    class Tcp;

    // Definition of the differents type of ASF streaming
    enum ASF_StreamType_e {
	ASF_Unknown_e,
	ASF_Live_e,
	ASF_Prerecorded_e,
	ASF_Redirector_e,
	ASF_PlainText_e,
	ASF_Authenticate_e
    };

    struct Asf_Networking : public Nop_Networking {
	public:
	    virtual ~Asf_Networking();

	    static Networking*	start(Tcp& tcp, network_protocol_t& protocol);
	    virtual int read( Tcp& fd, char *buffer, int buffer_size);
	    virtual int seek( Tcp& fd, off_t pos);
	private:
	    Asf_Networking();
	    int		parse_response(HTTP_Header& http_hdr );
	    MPXP_Rc	parse_header(Tcp& tcp);
	    HTTP_Header*http_request() const;

	    ASF_StreamType_e networking_type;
	    int request;
	    int packet_size;
	    int *audio_streams,n_audio,*video_streams,n_video;
	    int audio_id, video_id;
    };
} // namespace mpxp

#endif
