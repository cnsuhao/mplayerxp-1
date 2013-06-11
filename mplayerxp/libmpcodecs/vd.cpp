#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mpxp_help.h"

#include "xmpcore/xmp_core.h"
#include "libmpconf/codec-cfg.h"

#include "libvo2/img_format.h"

#include "libmpstream2/stream.h"
#include "libmpdemux/demuxer.h"
#include "libmpdemux/stheader.h"
#include "vd.h"
#include "postproc/vf.h"
#include "vd_msg.h"

namespace	usr {
extern const vd_info_t vd_null_info;
extern const vd_info_t vd_lavc_info;
extern const vd_info_t vd_dshow_info;
extern const vd_info_t vd_vfw_info;
extern const vd_info_t vd_vfwex_info;
extern const vd_info_t vd_divx4_info;
extern const vd_info_t vd_raw_info;
extern const vd_info_t vd_libdv_info;
extern const vd_info_t vd_xanim_info;
extern const vd_info_t vd_fli_info;
extern const vd_info_t vd_nuv_info;
extern const vd_info_t vd_mpng_info;
extern const vd_info_t vd_ijpg_info;
extern const vd_info_t vd_libmpeg2_info;
extern const vd_info_t vd_xvid_info;
extern const vd_info_t vd_mpegpes_info;
extern const vd_info_t vd_huffyuv_info;
extern const vd_info_t vd_xanim_info;
extern const vd_info_t vd_real_info;
extern const vd_info_t vd_dmo_info;
extern const vd_info_t vd_qtvideo_info;
extern const vd_info_t vd_theora_info;

static const vd_info_t* mpcodecs_vd_drivers[] = {
    &vd_raw_info,
    &vd_nuv_info,
    &vd_libmpeg2_info,
    &vd_xvid_info,
    &vd_mpegpes_info,
    &vd_huffyuv_info,
#ifndef ENABLE_GPL_ONLY
    &vd_divx4_info,
    &vd_real_info,
    &vd_xanim_info,
#endif
#ifdef HAVE_LIBTHEORA
    &vd_theora_info,
#endif
#ifdef HAVE_LIBDV
    &vd_libdv_info,
#endif
    &vd_lavc_info,
    &vd_null_info
};

void libmpcodecs_vd_register_options(M_Config& cfg)
{
    unsigned i;
    for (i=0; mpcodecs_vd_drivers[i] != &vd_null_info; i++)
	if(mpcodecs_vd_drivers[i]->options)
	    cfg.register_options(mpcodecs_vd_drivers[i]->options);
}

const vd_info_t* VD_Interface::find_driver(const std::string& name) {
    unsigned i;
    for (i=0; mpcodecs_vd_drivers[i] != &vd_null_info; i++)
	if(name==mpcodecs_vd_drivers[i]->driver_name) {
	    return mpcodecs_vd_drivers[i];
	}
    return NULL;
}

Video_Decoder* VD_Interface::probe_driver(sh_video_t& sh,put_slice_info_t& psi) {
    unsigned i;
    Video_Decoder* drv=NULL;
    for (i=0; mpcodecs_vd_drivers[i] != &vd_null_info; i++) {
	mpxp_v<<"Probing: "<<mpcodecs_vd_drivers[i]->driver_name<<std::endl;
	try {
	    drv = mpcodecs_vd_drivers[i]->query_interface(*this,sh,psi,sh.fourcc);
	    mpxp_v<<"ok"<<std::endl;
	    mpxp_v<<"Driver: "<<mpcodecs_vd_drivers[i]->driver_name<<" supports these outfmt for ";
		fourcc(mpxp_v,sh.fourcc);
	    mpxp_v<<" fourcc:"<<std::endl;
	    video_probe_t probe=drv->get_probe_information();
	    for(i=0;i<Video_MaxOutFmt;i++) {
		fourcc(mpxp_v,probe.pix_fmt[i]);
		mpxp_v<<" (flg="<<probe.flags[i]<<")"<<std::endl;
		if(probe.pix_fmt[i]==0||probe.pix_fmt[i]==unsigned(-1)) break;
	    }
	    mpxp_v<<std::endl;
	    break;
	} catch(const bad_format_exception&) {
	    mpxp_v<<"failed"<<std::endl;
	    delete drv; drv = NULL;
	    continue;
	}
    }
    return drv;
}

void VD_Interface::print_help() {
    unsigned i;
    mpxp_info<<"Available video codec families/drivers:"<<std::endl;
    for (i=0; mpcodecs_vd_drivers[i] != &vd_null_info; i++) {
	if(mpcodecs_vd_drivers[i])
	    if(mpcodecs_vd_drivers[i]->options)
		mpxp_info<<"\t"<<std::setw(10)<<std::left<<mpcodecs_vd_drivers[i]->driver_name<<" "<<mpcodecs_vd_drivers[i]->descr<<std::endl;
    }
    mpxp_info<<std::endl;
}
} // namespace	usr