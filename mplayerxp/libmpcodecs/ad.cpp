#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
/*
   ad.c - audio decoder interface
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libmpstream2/stream.h"
#include "libmpdemux/demuxer.h"
#include "libmpdemux/stheader.h"
#include "dec_audio.h"
#include "ad.h"
#include "ad_msg.h"

/* Missed vorbis, mad, dshow */
namespace	usr {

extern const ad_info_t ad_null_info;
extern const ad_info_t ad_mp3_info;
extern const ad_info_t ad_lavc_info;
extern const ad_info_t ad_a52_info;
extern const ad_info_t ad_dca_info;
extern const ad_info_t ad_pcm_info;
extern const ad_info_t ad_libdv_info;
extern const ad_info_t ad_dvdpcm_info;
extern const ad_info_t ad_dshow_info;
extern const ad_info_t ad_msacm_info;
extern const ad_info_t ad_faad_info;
extern const ad_info_t ad_vorbis_info;
extern const ad_info_t ad_real_info;
extern const ad_info_t ad_twin_info;
extern const ad_info_t ad_dmo_info;
extern const ad_info_t ad_qtaudio_info;

static const ad_info_t* mpcodecs_ad_drivers[] = {
    &ad_mp3_info,
    &ad_a52_info,
    &ad_dca_info,
    &ad_pcm_info,
    &ad_dvdpcm_info,
    &ad_faad_info,
#ifdef HAVE_LIBVORBIS
    &ad_vorbis_info,
#endif
#ifdef HAVE_LIBDV
    &ad_libdv_info,
#endif
#ifndef ENABLE_GPL_ONLY
    &ad_real_info,
#endif
    &ad_lavc_info,
    &ad_null_info
};

void libmpcodecs_ad_register_options(M_Config& cfg)
{
    unsigned i;
    for (i=0; mpcodecs_ad_drivers[i] != &ad_null_info; i++) {
	if(mpcodecs_ad_drivers[i]->options)
	    cfg.register_options(mpcodecs_ad_drivers[i]->options);
    }
}

const ad_info_t* AD_Interface::find_driver(const std::string& name) const {
    unsigned i;
    for (i=0; mpcodecs_ad_drivers[i] != &ad_null_info; i++) {
	if(name==mpcodecs_ad_drivers[i]->driver_name)
	    return mpcodecs_ad_drivers[i];
    }
    return NULL;
}

Audio_Decoder* AD_Interface::probe_driver(sh_audio_t& sh,audio_filter_info_t& afi) const {
    unsigned i;
    Audio_Decoder* drv=NULL;
    for (i=0; mpcodecs_ad_drivers[i] != &ad_null_info; i++) {
	mpxp_v<<"Probing: "<<mpcodecs_ad_drivers[i]->driver_name;
	try {
	    /* Set up some common usefull defaults. ad->preinit() can override these: */
#ifdef WORDS_BIGENDIAN
	    sh.afmt=AFMT_S16_BE;
#else
	    sh.afmt=AFMT_S16_LE;
#endif
	    sh.rate=0;
	    sh.o_bps=0;
	    drv=mpcodecs_ad_drivers[i]->query_interface(sh,afi,sh.wtag);
	    mpxp_v<<"ok"<<std::endl;
	    mpxp_v<<"Driver: "<<mpcodecs_ad_drivers[i]->driver_name<<" supports these outfmt for ";
	    fourcc(mpxp_v,sh.wtag);
	    mpxp_v<<std::endl;
	    audio_probe_t probe=drv->get_probe_information();
	    for(i=0;i<Audio_MaxOutSample;i++) {
		mpxp_v<<std::hex<<probe.sample_fmt[i]<<" ";
		if(probe.sample_fmt[i]==unsigned(-1)||probe.sample_fmt[i]==0) break;
	    }
	    mpxp_v<<std::endl;
	    break;
	} catch (const bad_format_exception& ) {
	    mpxp_v<<"failed"<<std::endl;
	    delete drv; drv=NULL;
	    continue;
	}
    }
    return drv;
}

void AD_Interface::print_help() {
    unsigned i;
    mpxp_info<<"Available audio codec families/drivers:"<<std::endl;
    for (i=0; mpcodecs_ad_drivers[i] != &ad_null_info; i++) {
	if(mpcodecs_ad_drivers[i])
	    if(mpcodecs_ad_drivers[i]->options)
		mpxp_info<<"\t"<<std::left<<std::setw(10)<<mpcodecs_ad_drivers[i]->driver_name<<" "<<mpcodecs_ad_drivers[i]->descr<<std::endl;
    }
    mpxp_info<<std::endl;
}
} //namespace	usr