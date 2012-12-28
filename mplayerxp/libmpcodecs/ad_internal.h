#ifdef __AD_ACM /* simply ugly hack */
#include "win32loader/wine/msacm.h"
#endif
#include "libmpconf/codec-cfg.h"
#include "libmpdemux/demuxer_r.h"

#include "ad.h"

#include "ad_msg.h"

static const audio_probe_t* __FASTCALL__ probe(uint32_t wtag);
static MPXP_Rc __FASTCALL__ init(Opaque& ctx);
static Opaque* __FASTCALL__  preinit(const audio_probe_t& probe,sh_audio_t *ctx,audio_filter_info_t& afi);
static void __FASTCALL__  uninit(Opaque& ctx);
static MPXP_Rc control_ad(Opaque& ctx,int cmd,any_t* arg, ...);
static unsigned __FASTCALL__  decode(Opaque& ctx,unsigned char *buf,unsigned minlen,unsigned maxlen,float& pts);

#define LIBAD_EXTERN(x) extern const ad_functions_t mpcodecs_ad_##x = {\
	&info,\
	options,\
	probe, \
	preinit,\
	init,\
	uninit,\
	control_ad,\
	decode\
};

