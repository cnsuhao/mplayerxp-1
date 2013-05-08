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
#include "ad.h"
#include "ad_msg.h"

/* Missed vorbis, mad, dshow */

extern const ad_functions_t mpcodecs_ad_null;
extern const ad_functions_t mpcodecs_ad_mp3;
extern const ad_functions_t mpcodecs_ad_lavc;
extern const ad_functions_t mpcodecs_ad_a52;
extern const ad_functions_t mpcodecs_ad_dca;
extern const ad_functions_t mpcodecs_ad_hwac3;
extern const ad_functions_t mpcodecs_ad_pcm;
extern const ad_functions_t mpcodecs_ad_libdv;
extern const ad_functions_t mpcodecs_ad_dvdpcm;
extern const ad_functions_t mpcodecs_ad_dshow;
extern const ad_functions_t mpcodecs_ad_msacm;
extern const ad_functions_t mpcodecs_ad_faad;
extern const ad_functions_t mpcodecs_ad_vorbis;
extern const ad_functions_t mpcodecs_ad_real;
extern const ad_functions_t mpcodecs_ad_twin;
extern const ad_functions_t mpcodecs_ad_dmo;
extern const ad_functions_t mpcodecs_ad_qtaudio;

static const ad_functions_t* mpcodecs_ad_drivers[] = {
    &mpcodecs_ad_mp3,
    &mpcodecs_ad_a52,
    &mpcodecs_ad_dca,
    &mpcodecs_ad_hwac3,
    &mpcodecs_ad_pcm,
    &mpcodecs_ad_dvdpcm,
    &mpcodecs_ad_faad,
#ifdef HAVE_LIBVORBIS
    &mpcodecs_ad_vorbis,
#endif
#ifdef HAVE_LIBDV
    &mpcodecs_ad_libdv,
#endif
#ifndef ENABLE_GPL_ONLY
    &mpcodecs_ad_real,
#endif
#ifdef ENABLE_WIN32LOADER
    &mpcodecs_ad_dshow,
    &mpcodecs_ad_twin,
    &mpcodecs_ad_msacm,
    &mpcodecs_ad_dmo,
    &mpcodecs_ad_qtaudio,
#endif
    &mpcodecs_ad_lavc,
    &mpcodecs_ad_null,

};

static unsigned int nddrivers=sizeof(mpcodecs_ad_drivers)/sizeof(ad_functions_t*);

void libmpcodecs_ad_register_options(M_Config& cfg)
{
    unsigned i;
    for(i=0;i<nddrivers;i++) {
	if(mpcodecs_ad_drivers[i])
	    if(mpcodecs_ad_drivers[i]->options)
		cfg.register_options(mpcodecs_ad_drivers[i]->options);
	if(mpcodecs_ad_drivers[i]==&mpcodecs_ad_null) break;
    }
}

const ad_functions_t* afm_find_driver(const std::string& name) {
    unsigned i;
    for (i=0; mpcodecs_ad_drivers[i] != &mpcodecs_ad_null; i++) {
	if(name==mpcodecs_ad_drivers[i]->info->driver_name){
	    return mpcodecs_ad_drivers[i];
	}
    }
    return NULL;
}

const audio_probe_t* afm_probe_driver(Opaque& ctx,sh_audio_t *sh,audio_filter_info_t& afi) {
    unsigned i;
    const audio_probe_t* probe;
    for (i=0; mpcodecs_ad_drivers[i] != &mpcodecs_ad_null; i++) {
	mpxp_v<<"Probing: "<<mpcodecs_ad_drivers[i]->info->driver_name<<std::endl;
	if((probe=mpcodecs_ad_drivers[i]->probe(sh->wtag))!=NULL) {
	    Opaque* priv=mpcodecs_ad_drivers[i]->preinit(*probe,sh,afi);
	    mpxp_v<<"Driver: "<<mpcodecs_ad_drivers[i]->info->driver_name<<" supports these outfmt for ";
	    fourcc(mpxp_v,sh->wtag);
	    mpxp_v<<std::endl;
	    for(i=0;i<Audio_MaxOutSample;i++) {
		    mpxp_v<<std::hex<<probe->sample_fmt[i]<<" ";
		    if(probe->sample_fmt[i]==-1||probe->sample_fmt[i]==0) break;
	    }
	    mpxp_v<<std::endl;
	    mpcodecs_ad_drivers[i]->uninit(*priv);
	    delete priv;
	    return probe;
	}
    }
    return NULL;
}

void afm_help(void) {
  unsigned i;
  mpxp_info<<"Available audio codec families/drivers:"<<std::endl;
  for(i=0;i<nddrivers;i++) {
    if(mpcodecs_ad_drivers[i])
	if(mpcodecs_ad_drivers[i]->options)
	    mpxp_info<<"\t"<<std::left<<std::setw(10)<<mpcodecs_ad_drivers[i]->info->driver_name<<" "<<mpcodecs_ad_drivers[i]->info->descr<<std::endl;
  }
  mpxp_info<<std::endl;
}
