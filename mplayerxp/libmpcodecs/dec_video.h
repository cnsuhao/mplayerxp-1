#ifndef DEC_VIDEO_H_INCLUDED
#define DEC_VIDEO_H_INCLUDED 1
#include "xmpcore/xmp_enums.h"

// dec_video.c:
extern MPXP_Rc	__FASTCALL__ mpcv_init(sh_video_t *sh_video, const char *codec_name,const char *family,int status,any_t*libinput);
extern void	__FASTCALL__ mpcv_uninit(sh_video_t *sh_video);
extern MPXP_Rc	__FASTCALL__ mpcv_ffmpeg_init(sh_video_t*,any_t* libinput);

extern int	__FASTCALL__ mpcv_decode(sh_video_t *sh_video,const enc_frame_t* frame);

extern MPXP_Rc	__FASTCALL__ mpcv_get_quality_max(sh_video_t *sh_video,unsigned *qual);
extern MPXP_Rc	__FASTCALL__ mpcv_set_quality(sh_video_t *sh_video,int quality);

extern MPXP_Rc	__FASTCALL__ mpcv_set_colors(sh_video_t *sh_video,char *item,int value);
extern void	__FASTCALL__ mpcv_resync_stream(sh_video_t *sh_video);

extern void vfm_help(void);
#endif
