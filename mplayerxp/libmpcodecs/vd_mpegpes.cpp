#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
#include <stdio.h>
#include <stdlib.h>

#include "osdep/bswap.h"
#include "libvo2/img_format.h"
#include "vd.h"
#include "vd_msg.h"

namespace	usr {
static const video_probe_t probes[] = {
    { "mpegpes", "libmpegpes", 0x10000001,                  VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", 0x10000002,                  VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('A','V','M','P'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('D','V','R',' '), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('L','M','P','2'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('M','1','V',' '), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('M','1','V','1'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('M','2','V','1'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('M','7','0','1'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('M','M','E','S'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('M','P','2','V'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('M','P','G','V'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('M','P','E','G'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('M','P','G','1'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('M','P','G','2'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('M','X','5','P'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('P','I','M','1'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('P','I','M','2'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('H','D','V','1'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('H','D','V','2'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('H','D','V','3'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('H','D','V','4'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('H','D','V','5'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('H','D','V','6'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('H','D','V','7'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('H','D','V','8'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('H','D','V','9'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('H','D','V','A'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('M','X','3','N'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('M','X','3','P'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('M','X','4','N'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('M','X','4','P'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('M','X','5','N'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','5','1'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','5','2'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','5','3'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','5','4'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','5','5'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','5','6'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','5','7'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','5','8'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','5','9'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','5','A'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','5','B'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','5','C'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','5','D'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','5','E'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','5','F'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','V','1'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','V','2'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','V','3'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','V','4'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','V','5'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','V','6'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','V','7'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','V','8'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','V','9'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','V','A'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','V','B'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','V','C'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','V','D'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','V','E'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { "mpegpes", "libmpegpes", FOURCC_TAG('X','D','V','F'), VCodecStatus_Working, {IMGFMT_MPEGPES}, {VideoFlag_None} },
    { NULL, NULL, 0x0, VCodecStatus_NotWorking, {0x0}, { VideoFlag_None }}
};

    class mpegpes_decoder : public Video_Decoder {
	public:
	    mpegpes_decoder(VD_Interface&,sh_video_t&,put_slice_info_t&,uint32_t fourcc);
	    virtual ~mpegpes_decoder();

	    virtual MPXP_Rc		ctrl(int cmd,any_t* arg,long arg2=0);
	    virtual mp_image_t*		run(const enc_frame_t& frame);
	    virtual video_probe_t	get_probe_information() const;
	private:
	    VD_Interface&		parent;
	    sh_video_t&			sh;
	    const video_probe_t*	probe;
    };

video_probe_t mpegpes_decoder::get_probe_information() const { return *probe; }

// to set/get/query special features/parameters
MPXP_Rc mpegpes_decoder::ctrl(int cmd,any_t* arg,long arg2){
    UNUSED(arg2);
    UNUSED(cmd);
    UNUSED(arg);
    return MPXP_Unknown;
}

mpegpes_decoder::mpegpes_decoder(VD_Interface& p,sh_video_t& _sh,put_slice_info_t& psi,uint32_t fourcc)
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
mpegpes_decoder::~mpegpes_decoder() { }

// decode a frame
mp_image_t* mpegpes_decoder::run(const enc_frame_t& frame) {
    mp_image_t* mpi;
    static vo_mpegpes_t packet;
    mpi=parent.get_image(MP_IMGTYPE_EXPORT, 0,sh.src_w, sh.src_h);
    if(mpi->flags&MP_IMGFLAG_DIRECT) mpi->flags|=MP_IMGFLAG_RENDERED;
    packet.data=frame.data;
    packet.size=frame.len-4;
    packet.timestamp=sh.ds->pts;
    packet.id=0x1E0; //+sh_video->ds->id;
    mpi->planes[0]=(uint8_t*)(&packet);
    return mpi;
}

static const mpxp_option_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

static Video_Decoder* query_interface(VD_Interface& p,sh_video_t& sh,put_slice_info_t& psi,uint32_t fourcc) { return new(zeromem) mpegpes_decoder(p,sh,psi,fourcc); }

extern const vd_info_t vd_mpegpes_info = {
    "MPEG 1/2 Video passthrough",
    "mpegpes",
    "A'rpi",
    "build-in",
    query_interface,
    options
};
} // namespace	usr