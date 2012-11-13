#ifdef __AD_ACM /* simply ugly hack */
#include "loader/wine/msacm.h"
#endif
#include "libmpconf/codec-cfg.h"

#include "libmpdemux/stream.h"
#include "libmpdemux/demuxer.h"
#include "libmpdemux/demuxer_r.h"
#include "libmpdemux/stheader.h"

#include "ad.h"

#include "ad_msg.h"

static const audio_probe_t* __FASTCALL__ probe(sh_audio_t* sh,uint32_t wtag);
static MPXP_Rc __FASTCALL__ init(sh_audio_t *sh);
static MPXP_Rc __FASTCALL__  preinit(sh_audio_t *sh);
static void __FASTCALL__  uninit(sh_audio_t *sh);
static MPXP_Rc control(sh_audio_t *sh,int cmd,any_t* arg, ...);
static unsigned __FASTCALL__  decode(sh_audio_t *sh_audio,unsigned char *buf,unsigned minlen,unsigned maxlen,float *pts);

#define LIBAD_EXTERN(x) const ad_functions_t mpcodecs_ad_##x = {\
	&info,\
	&options,\
	probe, \
	preinit,\
	init,\
	uninit,\
	control,\
	decode\
};

