#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
#include <stdio.h>
#include <stdlib.h>
#include "libnuppelvideo/nuppelvideo.h"
#include "codecs_ld.h"
#include "osdep/bswap.h"

#include "libvo2/img_format.h"
#include "vd.h"
#include "vd_msg.h"

namespace	usr {
    class nuv_decoder : public Video_Decoder {
	public:
	    nuv_decoder(VD_Interface&,sh_video_t&,put_slice_info_t&,uint32_t fourcc);
	    virtual ~nuv_decoder();

	    virtual MPXP_Rc		ctrl(int cmd,any_t* arg,long arg2=0);
	    virtual mp_image_t*		run(const enc_frame_t& frame);
	    virtual video_probe_t	get_probe_information() const;
	private:
	    VD_Interface&		parent;
	    sh_video_t&			sh;
	    const video_probe_t*	probe;
    };

video_probe_t nuv_decoder::get_probe_information() const { return *probe; }

static const video_probe_t probes[] = {
    { "nuv", "nuv", FOURCC_TAG('N','U','V','1'), VCodecStatus_Working, {IMGFMT_I420}, {VideoFlag_None, VideoFlag_None } },
    { "nuv", "nuv", FOURCC_TAG('R','J','P','G'), VCodecStatus_Working, {IMGFMT_I420}, {VideoFlag_None, VideoFlag_None } },
    { NULL, NULL, 0x0, VCodecStatus_NotWorking, {0x0}, { VideoFlag_None }}
};

nuv_decoder::nuv_decoder(VD_Interface& p,sh_video_t& _sh,put_slice_info_t& psi,uint32_t fourcc)
	    :Video_Decoder(p,_sh,psi,fourcc)
	    ,parent(p)
	    ,sh(_sh)
{
    unsigned i;
    for(i=0;probes[i].driver;i++)
	if(fourcc==probes[i].fourcc)
	    probe=&probes[i];
    if(!probe) throw bad_format_exception();
    if(parent.config_vf(sh.src_w,sh.src_h)!=MPXP_Ok) throw bad_format_exception();
}

// uninit driver
nuv_decoder::~nuv_decoder() { }

// to set/get/query special features/parameters
MPXP_Rc nuv_decoder::ctrl(int cmd,any_t* arg,long arg2){
    UNUSED(arg2);
    switch(cmd) {
	case VDCTRL_QUERY_FORMAT:
	    if (*((int*)arg) == IMGFMT_I420 ||
		*((int*)arg) == IMGFMT_IYUV)
			return MPXP_True;
	    else 	return MPXP_False;
	default: break;
    }
    return MPXP_Unknown;
}

// decode a frame
mp_image_t* nuv_decoder::run(const enc_frame_t& frame){
    mp_image_t* mpi;
    if(frame.len<=0) return NULL; // skipped frame

    mpi=parent.get_image(MP_IMGTYPE_TEMP, 0, sh.src_w, sh.src_h);
    if(mpi->flags&MP_IMGFLAG_DIRECT) mpi->flags|=MP_IMGFLAG_RENDERED;

    decode_nuv(reinterpret_cast<unsigned char*>(frame.data), frame.len, mpi->planes[0], sh.src_w, sh.src_h);

    return mpi;
}

static const mpxp_option_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

static Video_Decoder* query_interface(VD_Interface& p,sh_video_t& sh,put_slice_info_t& psi,uint32_t fourcc) { return new(zeromem) nuv_decoder(p,sh,psi,fourcc); }

extern const vd_info_t vd_nuv_info = {
    "NuppelVideo decoder",
    "nuv",
    "A'rpi (Alex & Panagiotis Issaris <takis@lumumba.luc.ac.be>)",
    "build-in",
    query_interface,
    options
};
} // namespace	usr