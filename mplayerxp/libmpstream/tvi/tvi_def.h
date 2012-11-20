#include <stdlib.h> /* mp_malloc */
#include <string.h>

struct priv_s;
static int init(struct priv_s *priv);
static int uninit(struct priv_s *priv);
static int control(struct priv_s *priv, int cmd, any_t*arg);
static int start(struct priv_s *priv);
static double grab_video_frame(struct priv_s *priv, unsigned char *buffer, int len);
#ifdef HAVE_TV_BSDBT848
static double grabimmediate_video_frame(struct priv_s *priv, unsigned char *buffer, int len);
#endif
static int get_video_framesize(struct priv_s *priv);
static double grab_audio_frame(struct priv_s *priv, unsigned char *buffer, int len);
static int get_audio_framesize(struct priv_s *priv);

static const tvi_functions_t functions =
{
    init,
    uninit,
    control,
    start,
    grab_video_frame,
#ifdef HAVE_TV_BSDBT848
    grabimmediate_video_frame,
#endif
    get_video_framesize,
    grab_audio_frame,
    get_audio_framesize
};

tvi_handle_t *new_handle();
void free_handle(tvi_handle_t *h);
