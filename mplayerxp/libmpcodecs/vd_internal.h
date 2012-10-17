
#include "codec-cfg.h"
#include "../libvo/img_format.h"

#include "stream.h"
#include "demuxer.h"
#include "demuxer_r.h"
#include "stheader.h"

#include "vd.h"
#include "vd_msg.h"
extern int divx_quality;

// prototypes:
//static vd_info_t info;
static const config_t options[];
static int control(sh_video_t *sh,int cmd,any_t* arg,...);
static int init(sh_video_t *sh);
static void uninit(sh_video_t *sh);
static mp_image_t* decode(sh_video_t *sh,any_t* data,int len,int flags);

#define LIBVD_EXTERN(x) const vd_functions_t mpcodecs_vd_##x = {\
	&info,\
	&options,\
	init,\
	uninit,\
	control,\
	decode\
};

