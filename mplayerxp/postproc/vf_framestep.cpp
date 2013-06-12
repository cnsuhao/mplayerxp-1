#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
/*
 * vf_fstep.c - filter to ouput only 1 every n frame, or only the I (key)
 *              frame
 *
 * The parameters are:
 *
 *    [I] | [i]num
 *
 * if you call the filter with I (uppercase) as the parameter
 *    ... -vf framestep=I ...
 * then ONLY the keyframes are outputted.
 * For DVD it means, generally, one every 15 frames (IBBPBBPBBPBBPBB), for avi it means
 * every scene change or every keyint value (see -lavcopts).
 *
 * if you call the filter with the i (lowercase)
 *    ... -vf framestep=i ...
 * then a I! followed by a cr is printed when a key frame (eg Intra frame) is
 * found, leaving the current line of mplayer/mencoder, where you got the
 * time, in seconds, and frame of the key. Use this information to split the
 * AVI.
 *
 * After the i or alone you can put a positive number and only one frame every
 * x (the number you set) is passed on the filter chain, limiting the output
 * of the frame.
 *
 * Example
 *    ... -vf framestep=i20 ...
 * Dump one every 20 frames, printing on the console when a I-Frame is encounter.
 *
 *    ... -vf framestep=25
 * Dump one every 25 frames.
 *
 * If you call the filter without parameter it does nothing (except using memory
 * and resource of your system,. of course).
 *
 * This filter doesn' t work like the option -sstep seconds.
 *
 * The -sstep seek to the new position, without decoding all frames but,
 * expecially on avi file coded whith mpeg4 (lavc or xvid or divx), the
 * seek is not always too much precise.
 *
 * This filter simply discard the unwanted frames, so you are very precise in
 * counting the frame but sometime you use a lot of CPU for nothing.
 *
 * As usual it depends on what you're doing.
 *
 *     Daniele Forghieri ( guru@digitalfantasy.it )
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mpxp_help.h"
#include "osdep/cpudetect.h"

#include "libvo2/img_format.h"
#include "xmpcore/xmp_image.h"
#include "vf.h"
#include "vf_internal.h"
#include "pp_msg.h"

/* Uncomment if you want to print some info on the format */
// #define DUMP_FORMAT_DATA

/* Private data */
struct vf_priv_t {
    /* Current frame */
    int  frame_cur;
    /* Frame output step, 0 = all */
    int  frame_step;
    /* Only I-Frame (2), print on I-Frame (1) */
    int  dump_iframe;
};

/* Filter handler */
static int __FASTCALL__ put_slice(vf_instance_t* vf,const mp_image_t& smpi)
{
    mp_image_t        *dmpi;
    vf_priv_t  *priv;
    int               skip;

    priv = vf->priv;

    /* Print the 'I' if is a intra frame. The \n advance the current line so you got the
     * current file time (in second) and the frame number on the console ;-)
     */
    if (priv->dump_iframe) {
	if (smpi.pict_type == 1) {
		MSG_INFO("I!\n");
	}
    }

    /* decide if frame must be shown */
    if (priv->dump_iframe == 2) {
	/* Only key frame */
	skip = smpi.pict_type == 1 ? 0 : 1;
    }
    else {
	/* Only 1 every frame_step */
	skip = 0;
	if ((priv->frame_step != 0) && ((priv->frame_cur % priv->frame_step) != 0)) {
	    skip = 1;
	}
    }
    /* Increment current frame */
    ++priv->frame_cur;

    if (skip == 0) {
	/* Get image, export type (we don't modify tghe image) */
	dmpi=vf_get_new_exportable_genome(vf->next, MP_IMGTYPE_EXPORT, 0, smpi);
	/* Copy only the pointer ( MP_IMGTYPE_EXPORT ! ) */
	dmpi->planes[0] = smpi.planes[0];
	dmpi->planes[1] = smpi.planes[1];
	dmpi->planes[2] = smpi.planes[2];

	dmpi->stride[0] = smpi.stride[0];
	dmpi->stride[1] = smpi.stride[1];
	dmpi->stride[2] = smpi.stride[2];

	dmpi->width     = smpi.width;
	dmpi->height    = smpi.height;

	/* Chain to next filter / output ... */
	return vf_next_put_slice(vf,*dmpi);
    }

    /* Skip the frame */
    return 0;
}

static void __FASTCALL__ uninit(vf_instance_t* vf)
{
    /* Free private data */
    delete vf->priv;
}

/* Main entry funct for the filter */
static MPXP_Rc __FASTCALL__ vf_open(vf_instance_t *vf,const char* args)
{
    vf_priv_t *p;

    vf->put_slice = put_slice;
    vf->uninit = uninit;
    vf->default_reqs = VFCAP_ACCEPT_STRIDE;
    vf->priv = p = new(zeromem) vf_priv_t;
    if (p == NULL) return MPXP_False;

    if (args != NULL) {
#ifdef DUMP_FORMAT_DATA
	if (*args == 'd') {
	    p->dump_iframe = 3;
	} else
#endif
	if (*args == 'I') {
	    /* Dump only KEY (ie INTRA) frame */
	    p->dump_iframe = 2;
	} else {
	    if (*args == 'i') {
		/* Print a 'I!' when a i-frame is encounter */
		p->dump_iframe = 1;
		++args;
	    }

	    if (*args != '\0') {
		p->frame_step = atoi(args);
		if (p->frame_step <= 0) {
		    MSG_WARN("[vf_framestep] Error parsing of arguments\n");
		    return MPXP_False;
		}
	    }
	}
    }
    check_pin("vfilter",vf->pin,VF_PIN);
    return MPXP_Ok;
}

extern const vf_info_t vf_info_framestep = {
    "Dump one every n / key frames",
    "framestep",
    "Daniele Forghieri",
    "",
    VF_FLAGS_THREADS,
    vf_open
};
