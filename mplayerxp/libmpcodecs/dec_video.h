#ifndef DEC_VIDEO_H_INCLUDED
#define DEC_VIDEO_H_INCLUDED 1
#include "xmpcore/xmp_enums.h"
#include "libmpdemux/demuxer_r.h"
#include "libmpdemux/stream.h"
#include "libmpdemux/stheader.h"

// dec_video.c:
extern any_t*	__FASTCALL__ RND_RENAME3(mpcv_init)(sh_video_t *sh_video, const char *codec_name,const char *family,int status,any_t*libinput);
extern void	__FASTCALL__ mpcv_uninit(any_t *handle);
extern any_t*	__FASTCALL__ mpcv_ffmpeg_init(sh_video_t*,any_t* libinput);
extern int	__FASTCALL__ RND_RENAME4(mpcv_decode)(any_t *handle,const enc_frame_t* frame);

extern MPXP_Rc	__FASTCALL__ mpcv_get_quality_max(any_t *handle,unsigned *qual);
extern MPXP_Rc	__FASTCALL__ mpcv_set_quality(any_t *handle,int quality);
extern MPXP_Rc	__FASTCALL__ mpcv_set_colors(any_t *handle,const char *item,int value);
extern void	__FASTCALL__ mpcv_resync_stream(any_t *handle);

extern void vfm_help(void);
#endif
