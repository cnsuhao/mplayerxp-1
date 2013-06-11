#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
#include <stdio.h>
#include <stdlib.h>

#include "vd_internal.h"

namespace	usr {
    class vnull_decoder : public Video_Decoder {
	public:
	    vnull_decoder(video_decoder_t&,sh_video_t&,put_slice_info_t&,uint32_t fourcc);
	    virtual ~vnull_decoder();

	    virtual MPXP_Rc		ctrl(int cmd,any_t* arg,long arg2=0);
	    virtual mp_image_t*		run(const enc_frame_t& frame);
	    virtual video_probe_t	get_probe_information() const;
	private:
	    sh_video_t& sh;
    };

video_probe_t vnull_decoder::get_probe_information() const { video_probe_t probe = { NULL, NULL, 0, VCodecStatus_NotWorking, {0}, {VideoFlag_None}}; return probe; }

// to set/get/query special features/parameters
MPXP_Rc vnull_decoder::ctrl(int cmd,any_t* arg,long arg2){
    UNUSED(arg2);
    UNUSED(cmd);
    UNUSED(arg);
    return MPXP_Unknown;
}

vnull_decoder::vnull_decoder(video_decoder_t& p,sh_video_t& _sh,put_slice_info_t& psi,uint32_t fourcc)
	    :Video_Decoder(p,_sh,psi,fourcc)
	    ,sh(_sh)
{
    throw bad_format_exception();
}

// uninit driver
vnull_decoder::~vnull_decoder() {}

// decode a frame
mp_image_t* vnull_decoder::run(const enc_frame_t& frame){
    UNUSED(frame);
    return NULL;
}

static const mpxp_option_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

static Video_Decoder* query_interface(video_decoder_t& p,sh_video_t& sh,put_slice_info_t& psi,uint32_t fourcc) { return new(zeromem) vnull_decoder(p,sh,psi,fourcc); }

extern const vd_info_t vd_null_info = {
    "Null video decoder",
    "null",
    "A'rpi",
    "build-in",
    query_interface,
    options
};
} // namespace	usr
