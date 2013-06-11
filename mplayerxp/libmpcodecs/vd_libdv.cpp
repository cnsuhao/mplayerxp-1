#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <math.h>

#include "libvo2/img_format.h"
#include "osdep/bswap.h"

#include <libdv/dv.h>
#include <libdv/dv_types.h>

#include "libmpstream2/stream.h"
#include "libmpdemux/demuxer.h"
#include "libmpdemux/stheader.h"

#include "libvo2/img_format.h"
#include "vd.h"
#include "vd_msg.h"

namespace	usr {
    class libvdv_decoder : public Video_Decoder {
	public:
	    libvdv_decoder(VD_Interface&,sh_video_t&,put_slice_info_t&,uint32_t fourcc);
	    virtual ~libvdv_decoder();

	    virtual MPXP_Rc		ctrl(int cmd,any_t* arg,long arg2=0);
	    virtual mp_image_t*		run(const enc_frame_t& frame);
	    virtual video_probe_t	get_probe_information() const;
	private:
	    dv_decoder_t*		init_global_rawdv_decoder();

	    VD_Interface&		parent;
	    sh_video_t&			sh;
	    const video_probe_t*	probe;

	    dv_decoder_t*		dvd;
};

video_probe_t libvdv_decoder::get_probe_information() const { return *probe; }

static const video_probe_t probes[] = {
    { "libdv", "libdv", FOURCC_TAG('A','V','d','1'), VCodecStatus_Working, {IMGFMT_YUY2}, {VideoFlag_None, VideoFlag_None } },
    { "libdv", "libdv", FOURCC_TAG('A','V','d','v'), VCodecStatus_Working, {IMGFMT_YUY2}, {VideoFlag_None, VideoFlag_None } },
    { "libdv", "libdv", FOURCC_TAG('D','V','C',' '), VCodecStatus_Working, {IMGFMT_YUY2}, {VideoFlag_None, VideoFlag_None } },
    { "libdv", "libdv", FOURCC_TAG('D','V','C','P'), VCodecStatus_Working, {IMGFMT_YUY2}, {VideoFlag_None, VideoFlag_None } },
    { "libdv", "libdv", FOURCC_TAG('D','V','5','0'), VCodecStatus_Working, {IMGFMT_YUY2}, {VideoFlag_None, VideoFlag_None } },
    { "libdv", "libdv", FOURCC_TAG('D','V','5','N'), VCodecStatus_Working, {IMGFMT_YUY2}, {VideoFlag_None, VideoFlag_None } },
    { "libdv", "libdv", FOURCC_TAG('D','V','5','P'), VCodecStatus_Working, {IMGFMT_YUY2}, {VideoFlag_None, VideoFlag_None } },
    { "libdv", "libdv", FOURCC_TAG('D','V','H','3'), VCodecStatus_Working, {IMGFMT_YUY2}, {VideoFlag_None, VideoFlag_None } },
    { "libdv", "libdv", FOURCC_TAG('D','V','H','5'), VCodecStatus_Working, {IMGFMT_YUY2}, {VideoFlag_None, VideoFlag_None } },
    { "libdv", "libdv", FOURCC_TAG('D','V','H','6'), VCodecStatus_Working, {IMGFMT_YUY2}, {VideoFlag_None, VideoFlag_None } },
    { "libdv", "libdv", FOURCC_TAG('D','V','H','Q'), VCodecStatus_Working, {IMGFMT_YUY2}, {VideoFlag_None, VideoFlag_None } },
    { "libdv", "libdv", FOURCC_TAG('D','V','H','P'), VCodecStatus_Working, {IMGFMT_YUY2}, {VideoFlag_None, VideoFlag_None } },
    { "libdv", "libdv", FOURCC_TAG('D','V','P','P'), VCodecStatus_Working, {IMGFMT_YUY2}, {VideoFlag_None, VideoFlag_None } },
    { "libdv", "libdv", FOURCC_TAG('D','V','S','C'), VCodecStatus_Working, {IMGFMT_YUY2}, {VideoFlag_None, VideoFlag_None } },
    { "libdv", "libdv", FOURCC_TAG('D','V','S','D'), VCodecStatus_Working, {IMGFMT_YUY2}, {VideoFlag_None, VideoFlag_None } },
    { NULL, NULL, 0x0, VCodecStatus_NotWorking, {0x0}, { VideoFlag_None }}
};

dv_decoder_t* libvdv_decoder::init_global_rawdv_decoder()
{
    dv_decoder_t* global_rawdv_decoder;
    global_rawdv_decoder=dv_decoder_new(TRUE,TRUE,FALSE);
    global_rawdv_decoder->quality=DV_QUALITY_BEST;
    global_rawdv_decoder->prev_frame_decoded = 0;
    return global_rawdv_decoder;
}

libvdv_decoder::libvdv_decoder(VD_Interface& p,sh_video_t& _sh,put_slice_info_t& psi,uint32_t fourcc)
	    :Video_Decoder(p,_sh,psi,fourcc)
	    ,parent(p)
	    ,sh(_sh)
{
    unsigned i;
    for(i=0;probes[i].driver;i++)
	if(fourcc==probes[i].fourcc)
	    probe=&probes[i];
    if(!probe) throw bad_format_exception();

    dvd = init_global_rawdv_decoder();
    if(!parent.config_vf(sh.src_w,sh.src_h)!=MPXP_Ok) throw bad_format_exception();
}

// uninit driver
libvdv_decoder::~libvdv_decoder() {}

// to set/get/query special features/parameters
MPXP_Rc libvdv_decoder::ctrl(int cmd,any_t* arg,long arg2){
    UNUSED(arg2);
    UNUSED(cmd);
    UNUSED(arg);
    return MPXP_Unknown;
}

// decode a frame
mp_image_t* libvdv_decoder::run(const enc_frame_t& frame)
{
    mp_image_t* mpi;

    if(frame.len<=0 || (frame.flags&3)){
//      fprintf(stderr,"decode() (rawdv) SKIPPED\n");
	return NULL; // skipped frame
    }

    dv_parse_header(dvd, reinterpret_cast<uint8_t*>(frame.data));

    mpi=parent.get_image(MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE, sh.src_w, sh.src_h);

    if(!mpi){	// temporary!
	mpxp_err<<"couldn't allocate image for codec"<<std::endl;
	return NULL;
    }

    dv_decode_full_frame(dvd, reinterpret_cast<uint8_t*>(frame.data), e_dv_color_yuv, mpi->planes, reinterpret_cast<int*>(mpi->stride));

    return mpi;
}

static const mpxp_option_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

static Video_Decoder* query_interface(VD_Interface& p,sh_video_t& sh,put_slice_info_t& psi,uint32_t fourcc) { return new(zeromem) libvdv_decoder(p,sh,psi,fourcc); }

extern const vd_info_t vd_libdv_info = {
    "Raw DV Video Decoder",
    "libdv",
    "Alexander Neundorf <neundorf@kde.org>",
    "http://libdv.sourceforge.net",
    query_interface,
    options
};
} // namespace	usr