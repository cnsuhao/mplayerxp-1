/*
    Only a sample!
*/

#include "mp_config.h"
#include "osdep/mplib.h"

#ifdef USE_TV

#include <stdio.h>
#include "libvo/img_format.h"
#include "tv.h"

/* information about this file */
static const tvi_info_t info = {
	"NULL-TV",
	"dummy",
	"alex",
	NULL
};

/* private data's */
struct priv_s {
    int width;
    int height;
};

#include "tvi_def.h"

/* handler creator - entry point ! */
tvi_handle_t *tvi_init_dummy(const char *device)
{
    UNUSED(device);
    return(new_handle());
}

/* initialisation */
static int init(struct priv_s *priv)
{
    priv->width = 320;
    priv->height = 200;
    return(1);
}

/* that's the real start, we'got the format parameters (checked with control) */
static int start(struct priv_s *priv)
{
    UNUSED(priv);
    return 1;
}

static int uninit(struct priv_s *priv)
{
    UNUSED(priv);
    return 1;
}

static int control(struct priv_s *priv, int cmd, any_t*arg)
{
    switch(cmd)
    {
	case TVI_CONTROL_IS_VIDEO:
	    return(TVI_CONTROL_TRUE);
	case TVI_CONTROL_VID_GET_FORMAT:
	    *(int *)arg = IMGFMT_YV12;
	    return(TVI_CONTROL_TRUE);
	case TVI_CONTROL_VID_SET_FORMAT:
	{
	    int req_fmt = (long)*(any_t**)arg;
	    if (req_fmt != IMGFMT_YV12)
		return(TVI_CONTROL_FALSE);
	    return(TVI_CONTROL_TRUE);
	}
	case TVI_CONTROL_VID_SET_WIDTH:
	    priv->width = (long)*(any_t**)arg;
	    return(TVI_CONTROL_TRUE);
	case TVI_CONTROL_VID_GET_WIDTH:
	    *(int *)arg = priv->width;
	    return(TVI_CONTROL_TRUE);
	case TVI_CONTROL_VID_SET_HEIGHT:
	    priv->height = (long)*(any_t**)arg;
	    return(TVI_CONTROL_TRUE);
	case TVI_CONTROL_VID_GET_HEIGHT:
	    *(int *)arg = priv->height;
	    return(TVI_CONTROL_TRUE);
	case TVI_CONTROL_VID_CHK_WIDTH:
	case TVI_CONTROL_VID_CHK_HEIGHT:
	    return(TVI_CONTROL_TRUE);
    }
    return(TVI_CONTROL_UNKNOWN);
}

#ifdef HAVE_TV_BSDBT848
static double grabimmediate_video_frame(struct priv_s *priv,unsigned char *buffer, int len)
{
    UNUSED(priv);
    memset(buffer, 0xCC, len);
    return 1;
}
#endif

static double grab_video_frame(struct priv_s *priv,unsigned char *buffer, int len)
{
    UNUSED(priv);
    memset(buffer, 0x42, len);
    return 1;
}

static int get_video_framesize(struct priv_s *priv)
{
    /* YV12 */
    return(priv->width*priv->height*12/8);
}

static double grab_audio_frame(struct priv_s *priv,unsigned char *buffer, int len)
{
    UNUSED(priv);
    memset(buffer, 0x42, len);
    return 1;
}

static int get_audio_framesize(struct priv_s *priv)
{
    UNUSED(priv);
    return 1;
}

#endif /* USE_TV */

tvi_handle_t *new_handle()
{
    tvi_handle_t *h = (tvi_handle_t *)mp_malloc(sizeof(tvi_handle_t));

    if (!h)
	return(NULL);
    h->priv = (struct priv_s *)mp_mallocz(sizeof(struct priv_s));
    if (!h->priv)
    {
	mp_free(h);
	return(NULL);
    }
    h->info = &info;
    h->functions = &functions;
    h->seq = 0;
    h->chanlist = -1;
    h->chanlist_s = NULL;
    h->norm = -1;
    h->channel = -1;
    return(h);
}

void free_handle(tvi_handle_t *h)
{
    if (h) {
	if (h->priv)
	    mp_free(h->priv);
	mp_free(h);
    }
}
