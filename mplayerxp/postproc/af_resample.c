/*=============================================================================
//
//  This software has been released under the terms of the GNU General Public
//  license. See http://www.gnu.org/copyleft/gpl.html for details.
//
//  Copyright 2002 Anders Johansson ajh@atri.curtin.edu.au
//
//=============================================================================
*/

/* This audio filter changes the sample rate. */
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <inttypes.h>

#include "libavutil/audioconvert.h"
#include "libswresample/swresample.h"

#include "af.h"
#include "dsp.h"
#include "osdep/mplib.h"
#include "pp_msg.h"

uint64_t layouts[]={
    0,
    AV_CH_LAYOUT_MONO,
    AV_CH_LAYOUT_STEREO,
    AV_CH_LAYOUT_SURROUND,
    AV_CH_LAYOUT_4POINT0,
    AV_CH_LAYOUT_5POINT0,
    AV_CH_LAYOUT_5POINT1,
    AV_CH_LAYOUT_7POINT0,
    AV_CH_LAYOUT_7POINT1
};

static uint64_t get_ch_layout(unsigned nch) {
    if(nch < sizeof(layouts)/sizeof(uint64_t))
	return layouts[nch];
    return 0;
}

static enum AVSampleFormat get_sample_format(mpaf_format_e fmt) {
    switch(fmt&MPAF_BPS_MASK) {
	case 1: if((fmt&MPAF_SIGN_MASK) == MPAF_US) return AV_SAMPLE_FMT_U8;
		break;
	case 2: if((fmt&MPAF_SIGN_MASK) == MPAF_SI &&
		   (fmt&MPAF_POINT_MASK) == MPAF_I &&
		   (fmt&MPAF_END_MASK) == MPAF_NE) return AV_SAMPLE_FMT_S16;
		break;
	case 4: if((fmt&MPAF_POINT_MASK) == MPAF_I) {
		    if((fmt&MPAF_SIGN_MASK) == MPAF_SI &&
			(fmt&MPAF_POINT_MASK) == MPAF_I &&
			(fmt&MPAF_END_MASK) == MPAF_NE) return AV_SAMPLE_FMT_S32;
		}
		else return AV_SAMPLE_FMT_FLT;
		break;
	case 8: if((fmt&MPAF_POINT_MASK) == MPAF_F) return AV_SAMPLE_FMT_DBL;
		break;
    }
    return AV_SAMPLE_FMT_NONE;
}

// local data
typedef struct af_resample_s {
    struct SwrContext* ctx;
    unsigned irate;
    unsigned inch;
    unsigned ifmt;
} af_resample_t;

// Initialization and runtime control
static MPXP_Rc __FASTCALL__ control(struct af_instance_s* af, int cmd, any_t* arg)
{
    af_resample_t* s   = (af_resample_t*)af->setup;
    switch(cmd){
	case AF_CONTROL_REINIT: {
	    enum AVSampleFormat avfmt;
	    uint64_t		nch;
	    mp_aframe_t*		n   = (mp_aframe_t*)arg; // New configuration
	    int			rv  = MPXP_Ok;

	    if(s->ctx) { swr_free(&s->ctx); s->ctx=NULL; }
	    // Make sure this filter isn't redundant
	    if((af->data->rate == n->rate) || (af->data->rate == 0)) {
		MSG_V("[af_resample] detach due: %i -> %i Hz\n",
			af->data->rate,n->rate);
		return MPXP_Detach;
	    }
	    avfmt=get_sample_format(n->format);
	    nch=get_ch_layout(n->nch);
	    if(avfmt==AV_SAMPLE_FMT_NONE) rv=MPXP_Error;
	    if(nch==0) rv=MPXP_Error;
	    if(rv!=MPXP_Ok) {
		char buff[256];
		MSG_V("[af_resample] doesn't work with '%s' x %i\n"
		,mpaf_fmt2str(n->format,buff,sizeof(buff))
		,n->nch);
	    }
	    s->ctx = swr_alloc_set_opts(NULL,
				      nch, avfmt,af->data->rate,
				      nch, avfmt,n->rate,
				      0, NULL);
	    if(swr_init(s->ctx)<0) {
		MSG_ERR("[af_resample] Cannot init swr_init\n");
		rv=MPXP_Error;
	    }

	    af->data->format = n->format;
	    af->data->nch = n->nch;

	    s->irate=n->rate;
	    s->inch=n->nch;
	    s->ifmt=n->format;
	    // Set multiplier and delay
	    af->delay = (double)swr_get_delay(s->ctx,1000);
	    af->mul.n = af->data->rate;
	    af->mul.d = n->rate;
	    return rv;
	}
	case AF_CONTROL_SHOWCONF:
	    MSG_INFO("[af_resample] New filter designed (%i -> %i Hz)\n", s->irate,af->data->rate);
	    return MPXP_Ok;
	case AF_CONTROL_COMMAND_LINE:{
	    int rate=0;
	    sscanf((char*)arg,"%i", &rate);
	    return af->control(af,AF_CONTROL_RESAMPLE_RATE | AF_CONTROL_SET, &rate);
	}
	case AF_CONTROL_POST_CREATE: return MPXP_Ok;
	case AF_CONTROL_RESAMPLE_RATE | AF_CONTROL_SET: {
	    af->data->rate = ((int*)arg)[0];
	    return MPXP_Ok;
	}
	default: break;
    }
    return MPXP_Unknown;
}

// Deallocate memory
static void __FASTCALL__ uninit(struct af_instance_s* af)
{
    af_resample_t* s = (af_resample_t*)af->setup;
    if(af->data) {
	if(af->data->audio) mp_free(af->data->audio);
	mp_free(af->data);
    }
    if(s->ctx) swr_free(&s->ctx);
    s->ctx=NULL;
}

// Filter data through filter
static mp_aframe_t* __FASTCALL__ play(struct af_instance_s* af, mp_aframe_t* data,int final)
{
    int rc;
    mp_aframe_t*	c = data; // Current working data
    af_resample_t*	s = (af_resample_t*)af->setup;

    if (MPXP_Ok != RESIZE_LOCAL_BUFFER(af, data)) return NULL;
    mp_aframe_t*	l = af->data; // Local data
    uint8_t*		ain[SWR_CH_MAX];
    const uint8_t*	aout[SWR_CH_MAX];

    aout[0]=l->audio;
    ain[0]=c->audio;

    rc=swr_convert(s->ctx,aout,l->len/(l->nch*(l->format&MPAF_BPS_MASK)),ain,c->len/(c->nch*(c->format&MPAF_BPS_MASK)));
    if(rc<0)	MSG_ERR("%i=swr_convert\n",rc);
    else	l->len=rc*l->nch*(l->format&MPAF_BPS_MASK);

    return l;
}

// Allocate memory and set function pointers
static MPXP_Rc __FASTCALL__ open(af_instance_t* af){
    af->control=control;
    af->uninit=uninit;
    af->play=play;
    af->mul.n=1;
    af->mul.d=1;
    af->data=mp_calloc(1,sizeof(mp_aframe_t));
    af->setup=mp_calloc(1,sizeof(af_resample_t));
    if(af->data == NULL || af->setup == NULL) return MPXP_Error;
    return MPXP_Ok;
}

// Description of this plugin
const af_info_t af_info_resample = {
    "Sample frequency conversion",
    "resample",
    "Anders",
    "",
    AF_FLAGS_REENTRANT,
    open
};

