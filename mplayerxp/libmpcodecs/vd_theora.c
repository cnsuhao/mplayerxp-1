#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#include <dlfcn.h>

#include <theora/theora.h>

#include "mp_config.h"
#include "help_mp.h"
#include "codecs_ld.h"

#include "vd_internal.h"
#include "vd_msg.h"
#include "osdep/mplib.h"

static const vd_info_t info = {
   "Theora/VP3 video decoder",
   "theora",
   "David Kuehling (www.theora.org)",
   "build-in"
};

static const config_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

LIBVD_EXTERN(theora)

#define THEORA_NUM_HEADER_PACKETS 3


typedef struct priv_s {
    theora_state st;
    theora_comment cc;
    theora_info inf;
} priv_t;

// to set/get/query special features/parameters
static MPXP_Rc control(sh_video_t *sh,int cmd,any_t* arg,...){
    switch(cmd) {
	case VDCTRL_QUERY_FORMAT:
	    if (*((int*)arg) == IMGFMT_YV12)
			return MPXP_True;
	    else	return MPXP_False;
	default: break;
    }
    return MPXP_Unknown;
}

/*
 * init driver
 */
static MPXP_Rc init(sh_video_t *sh,any_t* libinput){
    priv_t *priv = NULL;
    int failed = 1;
    int errorCode = 0;
    ogg_packet op;
    int i;
    float pts;

    /* check whether video output format is supported */
    switch(sh->codec->outfmt[sh->outfmtidx]) {
	case IMGFMT_YV12: /* well, this should work... */ break;
	default:
	    MSG_ERR("Unsupported out_fmt: 0x%X\n", sh->codec->outfmt[sh->outfmtidx]);
	return MPXP_False;
    }

    /* this is not a loop, just a context, from which we can break on error */
    do {
	priv = (priv_t *)mp_calloc (sizeof (priv_t), 1);
	sh->context = priv;
	if (!priv) break;

	theora_info_init(&priv->inf);
	theora_comment_init(&priv->cc);

	/* Read all header packets, pass them to theora_decode_header. */
	for (i = 0; i < THEORA_NUM_HEADER_PACKETS; i++) {
	    op.bytes = ds_get_packet_r (sh->ds, &op.packet,&pts);
	    op.b_o_s = 1;
	    if ( (errorCode = theora_decode_header (&priv->inf, &priv->cc, &op))) {
		MSG_ERR("Broken Theora header; errorCode=%i!\n", errorCode);
		break;
	    }
	}
	if (errorCode) break;

	/* now init codec */
	errorCode = theora_decode_init (&priv->st, &priv->inf);
	if (errorCode) {
	    MSG_ERR("Theora decode init failed: %i \n", errorCode);
	    break;
	}
	failed = 0;
    } while (0);

    if (failed) {
	if (priv) {
	    mp_free (priv);
	    sh->context = NULL;
	}
	return MPXP_False;
    }

    if(sh->aspect==0.0 && priv->inf.aspect_denominator!=0) {
	sh->aspect = (float)(priv->inf.aspect_numerator * priv->inf.frame_width)/
		(priv->inf.aspect_denominator * priv->inf.frame_height);
    }

    MSG_V("INFO: Theora video init ok!\n");

    return mpcodecs_config_vo (sh,sh->src_w,sh->src_h,NULL,libinput);
}

/*
 * uninit driver
 */
static void uninit(sh_video_t *sh)
{
    priv_t *priv = (priv_t *)sh->context;
    if (priv) {
	theora_clear (&priv->st);
	mp_free (priv);
    }
}

/*
 * decode frame
 */
static mp_image_t* decode(sh_video_t *sh,const enc_frame_t* frame,int flags)
{
    priv_t *priv = (priv_t *)sh->context;
    int errorCode = 0;
    ogg_packet op;
    yuv_buffer yuv;
    mp_image_t* mpi;

    bzero (&op, sizeof (op));
    op.bytes = frame->len;
    op.packet = frame->data;
    op.granulepos = -1;

    errorCode = theora_decode_packetin (&priv->st, &op);
    if (errorCode) {
	MSG_ERR("Theora decode packetin failed: %i \n",
	     errorCode);
	return NULL;
    }

    errorCode = theora_decode_YUVout (&priv->st, &yuv);
    if (errorCode) {
	MSG_ERR("Theora decode YUVout failed: %i \n",
	     errorCode);
        return NULL;
    }

    mpi = mpcodecs_get_image(sh, MP_IMGTYPE_EXPORT, 0,sh->src_w, sh->src_h);

    mpi->planes[0]=yuv.y;
    mpi->stride[0]=yuv.y_stride;
    mpi->planes[1]=yuv.u;
    mpi->stride[1]=yuv.uv_stride;
    mpi->planes[2]=yuv.v;
    mpi->stride[2]=yuv.uv_stride;

    return mpi;
}
