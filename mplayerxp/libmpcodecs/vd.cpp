#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "help_mp.h"

#include "xmpcore/xmp_core.h"
#include "libmpconf/codec-cfg.h"

#include "libvo/img_format.h"

#include "libmpstream/stream.h"
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

void libmpcodecs_vd_register_options(m_config_t* cfg)
{
    unsigned i;
    for(i=0;i<nddrivers;i++) {
	if(mpcodecs_vd_drivers[i])
	    if(mpcodecs_vd_drivers[i]->options)
		m_config_register_options(cfg,mpcodecs_vd_drivers[i]->options);
	if(mpcodecs_vd_drivers[i]==&mpcodecs_vd_null) break;
    }
}

const vd_functions_t* vfm_find_driver(const char *name) {
    unsigned i;
    for (i=0; mpcodecs_vd_drivers[i] != &mpcodecs_vd_null; i++)
	if(strcmp(mpcodecs_vd_drivers[i]->info->driver_name,name)==0)
	    return mpcodecs_vd_drivers[i];
    return NULL;
}

const video_probe_t* vfm_driver_probe(vd_private_t* ctx,sh_video_t *sh) {
    unsigned i;
    const video_probe_t* probe;
    for (i=0; mpcodecs_vd_drivers[i] != &mpcodecs_vd_null; i++) {
	MSG_V("Probing: %s\n",mpcodecs_vd_drivers[i]->info->driver_name);
	if((probe=mpcodecs_vd_drivers[i]->probe(ctx,sh->fourcc))!=NULL) {
	    const char* pfcc = reinterpret_cast<const char*>(&sh->fourcc);
	    MSG_V("Driver: %s supports these outfmt for %c%c%c%c fourcc:\n"
		,mpcodecs_vd_drivers[i]->info->driver_name
		,pfcc[0],pfcc[1],pfcc[2],pfcc[3],probe->flags[i]);
	    for(i=0;i<Video_MaxOutFmt;i++) {
		pfcc = reinterpret_cast<const char*>(&probe->pix_fmt[i]);
		MSG_V("%c%c%c%c (flg=%X) ",pfcc[0],pfcc[1],pfcc[2],pfcc[3],probe->flags[i]);
		if(probe->pix_fmt[i]==0||probe->pix_fmt[i]==-1) break;
	    }
	    MSG_V("\n");
	    return probe;
	}
    }
    return NULL;
}

void vfm_help(void) {
    unsigned i;
    MSG_INFO("Available video codec families/drivers:\n");
    for(i=0;i<nddrivers;i++) {
	if(mpcodecs_vd_drivers[i])
	    if(mpcodecs_vd_drivers[i]->options)
		MSG_INFO("\t%-10s %s\n",mpcodecs_vd_drivers[i]->info->driver_name,mpcodecs_vd_drivers[i]->info->descr);
    }
    MSG_INFO("\n");
}
