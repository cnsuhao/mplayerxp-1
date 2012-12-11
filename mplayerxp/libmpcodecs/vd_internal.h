
#include "libmpconf/codec-cfg.h"
#include "libvo/img_format.h"

#include "vd.h"
#include "vd_msg.h"
// prototypes:
//static vd_info_t info;
//static const config_t options[];

static const video_probe_t* __FASTCALL__ probe(uint32_t fourcc);
static MPXP_Rc control_vd(vd_private_t *ctx,int cmd,any_t* arg,...);
static vd_private_t* __FASTCALL__ preinit(const video_probe_t* probe,sh_video_t *sh,put_slice_info_t* psi);
static MPXP_Rc __FASTCALL__ init(vd_private_t *ctx,video_decoder_t*opaque);
static void __FASTCALL__ uninit(vd_private_t *ctx);
static mp_image_t* __FASTCALL__ decode(vd_private_t *ctx,const enc_frame_t* frame);

#define LIBVD_EXTERN(x) extern const vd_functions_t mpcodecs_vd_##x = {\
	&info,\
	(const config_t*)options,\
	probe, \
	preinit, \
	init,\
	uninit,\
	control_vd,\
	decode\
};

