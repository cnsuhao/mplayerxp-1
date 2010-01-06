
// dec_video.c:
extern int video_read_properties(sh_video_t *sh_video);

extern int init_video(sh_video_t *sh_video, const char *codec_name,const char *family,int status);
void uninit_video(sh_video_t *sh_video);

extern int decode_video(sh_video_t *sh_video,unsigned char *start,int in_size,int drop_frame,float pts);

extern int get_video_quality_max(sh_video_t *sh_video);
extern void set_video_quality(sh_video_t *sh_video,int quality);

int set_video_colors(sh_video_t *sh_video,char *item,int value);
extern void resync_video_stream(sh_video_t *sh_video);

extern int divx_quality;

extern void vfm_help(void);
