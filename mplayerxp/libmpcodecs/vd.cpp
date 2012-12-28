#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
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

extern const vd_functions_t mpcodecs_vd_null;
extern const vd_functions_t mpcodecs_vd_lavc;
extern const vd_functions_t mpcodecs_vd_dshow;
extern const vd_functions_t mpcodecs_vd_vfw;
extern const vd_functions_t mpcodecs_vd_vfwex;
extern const vd_functions_t mpcodecs_vd_divx4;
extern const vd_functions_t mpcodecs_vd_raw;
extern const vd_functions_t mpcodecs_vd_libdv;
extern const vd_functions_t mpcodecs_vd_xanim;
extern const vd_functions_t mpcodecs_vd_fli;
extern const vd_functions_t mpcodecs_vd_nuv;
extern const vd_functions_t mpcodecs_vd_mpng;
extern const vd_functions_t mpcodecs_vd_ijpg;
extern const vd_functions_t mpcodecs_vd_libmpeg2;
extern const vd_functions_t mpcodecs_vd_xvid;
extern const vd_functions_t mpcodecs_vd_mpegpes;
extern const vd_functions_t mpcodecs_vd_huffyuv;
extern const vd_functions_t mpcodecs_vd_xanim;
extern const vd_functions_t mpcodecs_vd_real;
extern const vd_functions_t mpcodecs_vd_dmo;
extern const vd_functions_t mpcodecs_vd_qtvideo;
extern const vd_functions_t mpcodecs_vd_theora;

static const vd_functions_t* mpcodecs_vd_drivers[] = {
#ifdef ENABLE_WIN32LOADER
    &mpcodecs_vd_dshow,
    &mpcodecs_vd_vfw,
    &mpcodecs_vd_vfwex,
    &mpcodecs_vd_dmo,
    &mpcodecs_vd_qtvideo,
#endif
    &mpcodecs_vd_raw,
    &mpcodecs_vd_nuv,
    &mpcodecs_vd_libmpeg2,
    &mpcodecs_vd_xvid,
    &mpcodecs_vd_mpegpes,
    &mpcodecs_vd_huffyuv,
#ifndef ENABLE_GPL_ONLY
    &mpcodecs_vd_divx4,
    &mpcodecs_vd_real,
    &mpcodecs_vd_xanim,
#endif
#ifdef HAVE_LIBTHEORA
    &mpcodecs_vd_theora,
#endif
#ifdef HAVE_LIBDV
    &mpcodecs_vd_libdv,
#endif
    &mpcodecs_vd_lavc,
    &mpcodecs_vd_null,
    NULL
};
static unsigned int nddrivers=sizeof(mpcodecs_vd_drivers)/sizeof(vd_functions_t*);

void libmpcodecs_vd_register_options(M_Config& cfg)
{
    unsigned i;
    for(i=0;i<nddrivers;i++) {
	if(mpcodecs_vd_drivers[i])
	    if(mpcodecs_vd_drivers[i]->options)
		cfg.register_options(mpcodecs_vd_drivers[i]->options);
	if(mpcodecs_vd_drivers[i]==&mpcodecs_vd_null) break;
    }
}

const vd_functions_t* vfm_find_driver(const std::string& name) {
    unsigned i;
    for (i=0; mpcodecs_vd_drivers[i] != &mpcodecs_vd_null; i++)
	if(name==mpcodecs_vd_drivers[i]->info->driver_name) {
	    return mpcodecs_vd_drivers[i];
	}
    return NULL;
}

const video_probe_t* vfm_driver_probe(Opaque& ctx,sh_video_t *sh,put_slice_info_t& psi) {
    unsigned i;
    const video_probe_t* probe;
    for (i=0; mpcodecs_vd_drivers[i] != &mpcodecs_vd_null; i++) {
	mpxp_v<<"Probing: "<<mpcodecs_vd_drivers[i]->info->driver_name<<std::endl;
	if((probe=mpcodecs_vd_drivers[i]->probe(sh->fourcc))!=NULL) {
	    Opaque* priv=mpcodecs_vd_drivers[i]->preinit(*probe,sh,psi);
	    if(priv) {
		mpxp_v<<"Driver: "<<mpcodecs_vd_drivers[i]->info->driver_name<<" supports these outfmt for ";
		fourcc(mpxp_v,sh->fourcc);
		mpxp_v<<" fourcc:"<<std::endl;
		for(i=0;i<Video_MaxOutFmt;i++) {
		    fourcc(mpxp_v,probe->pix_fmt[i]);
		    mpxp_v<<" (flg="<<probe->flags[i]<<")"<<std::endl;
		    if(probe->pix_fmt[i]==0||probe->pix_fmt[i]==-1) break;
		}
		mpxp_v<<std::endl;
		mpcodecs_vd_drivers[i]->uninit(*priv);
		delete priv;
		return probe;
	    }
	}
    }
    return NULL;
}

void vfm_help(void) {
    unsigned i;
    mpxp_info<<"Available video codec families/drivers:"<<std::endl;
    for(i=0;i<nddrivers;i++) {
	if(mpcodecs_vd_drivers[i])
	    if(mpcodecs_vd_drivers[i]->options)
		mpxp_info<<"\t"<<std::setw(10)<<std::left<<mpcodecs_vd_drivers[i]->info->driver_name<<" "<<mpcodecs_vd_drivers[i]->info->descr<<std::endl;
    }
    mpxp_info<<std::endl;
}
