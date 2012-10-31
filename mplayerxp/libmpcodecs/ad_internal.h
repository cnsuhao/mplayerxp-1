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

static const config_t options[];
static int __FASTCALL__ init(sh_audio_t *sh);
static int __FASTCALL__  preinit(sh_audio_t *sh);
static void __FASTCALL__  uninit(sh_audio_t *sh);
static ControlCodes __FASTCALL__  control(sh_audio_t *sh,int cmd,any_t* arg, ...);
static unsigned __FASTCALL__  decode(sh_audio_t *sh_audio,unsigned char *buf,unsigned minlen,unsigned maxlen,float *pts);

#define LIBAD_EXTERN(x) const ad_functions_t mpcodecs_ad_##x = {\
	&info,\
	&options,\
	preinit,\
	init,\
        uninit,\
	control,\
	decode\
};

