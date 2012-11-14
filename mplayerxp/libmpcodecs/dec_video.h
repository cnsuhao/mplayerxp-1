#ifndef DEC_VIDEO_H_INCLUDED
#define DEC_VIDEO_H_INCLUDED 1
#include "xmpcore/xmp_enums.h"

struct priv_s;
// dec_video.c:
extern struct priv_s*	__FASTCALL__ RND_RENAME3(mpcv_init)(sh_video_t *sh_video, const char *codec_name,const char *family,int status,any_t*libinput);
extern void		__FASTCALL__ mpcv_uninit(struct priv_s *handle);
extern struct priv_s*	__FASTCALL__ mpcv_ffmpeg_init(sh_video_t*,any_t* libinput);
extern int		__FASTCALL__ RND_RENAME4(mpcv_decode)(struct priv_s *handle,const enc_frame_t* frame);

extern MPXP_Rc	__FASTCALL__ mpcv_get_quality_max(struct priv_s *handle,unsigned *qual);
extern MPXP_Rc	__FASTCALL__ mpcv_set_quality(struct priv_s *handle,int quality);
extern MPXP_Rc	__FASTCALL__ mpcv_set_colors(struct priv_s *handle,char *item,int value);
extern void	__FASTCALL__ mpcv_resync_stream(struct priv_s *handle);

extern void vfm_help(void);
#endif
