#include <stdlib.h> /* mp_malloc */
#include <string.h>

namespace	usr {
    struct priv_t;
}
static int init(priv_t *priv);
static int uninit(priv_t *priv);
static int control(priv_t *priv, int cmd, any_t*arg);
static int start(priv_t *priv);
static double grab_video_frame(priv_t *priv, unsigned char *buffer, int len);
#ifdef HAVE_TV_BSDBT848
static double grabimmediate_video_frame(priv_t *priv, unsigned char *buffer, int len);
#endif
static int get_video_framesize(priv_t *priv);
static double grab_audio_frame(priv_t *priv, unsigned char *buffer, int len);
static int get_audio_framesize(priv_t *priv);

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

namespace	usr {
    tvi_handle_t *new_handle();
    void free_handle(tvi_handle_t *h);
} // namespace	usr
