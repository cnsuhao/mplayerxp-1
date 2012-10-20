#ifndef DEC_VIDEO_H_INCLUDED
#define DEC_VIDEO_H_INCLUDED 1

// dec_video.c:
extern int mpcv_init(sh_video_t *sh_video, const char *codec_name,const char *family,int status);
extern void mpcv_uninit(sh_video_t *sh_video);

extern int mpcv_decode(sh_video_t *sh_video,unsigned char *start,int in_size,int drop_frame,float pts);

extern int mpcv_get_quality_max(sh_video_t *sh_video);
extern void mpcv_set_quality(sh_video_t *sh_video,int quality);

extern int mpcv_set_colors(sh_video_t *sh_video,char *item,int value);
extern void mpcv_resync_stream(sh_video_t *sh_video);

extern int divx_quality;

extern void vfm_help(void);
#endif
