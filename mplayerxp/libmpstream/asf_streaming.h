#ifndef __ASF_STEAMING_H_INCLUDED
#define __ASF_STEAMING_H_INCLUDED 1
#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;

#include "stream.h"

namespace mpxp {
    class Tcp;
}

// Definition of the differents type of ASF streaming
enum ASF_StreamType_e {
    ASF_Unknown_e,
    ASF_Live_e,
    ASF_Prerecorded_e,
    ASF_Redirector_e,
    ASF_PlainText_e,
    ASF_Authenticate_e
};

struct asf_http_networking_t : public Opaque {
    public:
	asf_http_networking_t();
	virtual ~asf_http_networking_t();

	ASF_StreamType_e networking_type;
	int request;
	int packet_size;
	int *audio_streams,n_audio,*video_streams,n_video;
	int audio_id, video_id;
};

extern MPXP_Rc asf_networking_start(Tcp& fd, networking_t *networking);
extern MPXP_Rc asf_mmst_networking_start(Tcp& fd, networking_t *networking);

#endif
