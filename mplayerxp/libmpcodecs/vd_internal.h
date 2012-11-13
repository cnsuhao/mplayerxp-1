
#include "libmpconf/codec-cfg.h"
#include "libvo/img_format.h"

#include "libmpdemux/stream.h"
#include "libmpdemux/demuxer.h"
#include "libmpdemux/demuxer_r.h"
#include "libmpdemux/stheader.h"

#include "vd.h"
#include "vd_msg.h"
// prototypes:
//static vd_info_t info;
static const config_t options[];

static video_probe_t* __FASTCALL__ probe(uint32_t fourcc);
static MPXP_Rc control(sh_video_t *sh,int cmd,any_t* arg,...);
static int __FASTCALL__ init(sh_video_t *sh,any_t* libinput);
static void __FASTCALL__ uninit(sh_video_t *sh);
static mp_image_t* __FASTCALL__ decode(sh_video_t *sh,const enc_frame_t* frame);

#define LIBVD_EXTERN(x) const vd_functions_t mpcodecs_vd_##x = {\
	&info,\
	&options,\
	probe, \
	init,\
	uninit,\
	control,\
	decode\
};

