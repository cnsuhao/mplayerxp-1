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

#include "mp_conf_lavc.h"

#include "af.h"
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
    unsigned bps=fmt&MPAF_BPS_MASK;
    switch(bps) {
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

static MPXP_Rc __FASTCALL__ af_config(struct af_instance_s* af,const af_conf_t* arg)
{
    af_resample_t* s   = (af_resample_t*)af->setup;
    enum AVSampleFormat avfmt;
    uint64_t		nch;
    MPXP_Rc		rv  = MPXP_Ok;

    if(s->ctx) { swr_free(&s->ctx); s->ctx=NULL; }
    // Make sure this filter isn't redundant
    if((af->conf.rate == arg->rate) || (af->conf.rate == 0)) {
	MSG_V("[af_resample] detach due: %i -> %i Hz\n",
		af->conf.rate,arg->rate);
	return MPXP_Detach;
    }
    avfmt=get_sample_format(arg->format);
    nch=get_ch_layout(arg->nch);
    if(avfmt==AV_SAMPLE_FMT_NONE) rv=MPXP_Error;
    if(nch==0) rv=MPXP_Error;
    if(rv!=MPXP_Ok) {
	char buff[256];
	MSG_V("[af_resample] doesn't work with '%s' x %i\n"
	,mpaf_fmt2str(arg->format,buff,sizeof(buff))
	,arg->nch);
    }
    s->ctx = swr_alloc_set_opts(NULL,
			      nch, avfmt,af->conf.rate,
			      nch, avfmt,arg->rate,
			      0, NULL);
    if(swr_init(s->ctx)<0) {
	MSG_ERR("[af_resample] Cannot init swr_init\n");
	rv=MPXP_Error;
    }

    af->conf.format = arg->format;
    af->conf.nch = arg->nch;

    s->irate=arg->rate;
    s->inch=arg->nch;
    s->ifmt=arg->format;
    // Set multiplier and delay
    af->delay = (double)swr_get_delay(s->ctx,1000);
    af->mul.n = af->conf.rate;
    af->mul.d = arg->rate;
    return rv;
}
// Initialization and runtime control
static MPXP_Rc __FASTCALL__ control(struct af_instance_s* af, int cmd, any_t* arg)
{
    af_resample_t* s   = (af_resample_t*)af->setup;
    switch(cmd){
	case AF_CONTROL_SHOWCONF:
	    MSG_INFO("[af_resample] New filter designed (%i -> %i Hz)\n", s->irate,af->conf.rate);
	    return MPXP_Ok;
	case AF_CONTROL_COMMAND_LINE:{
	    int rate=0;
	    sscanf((char*)arg,"%i", &rate);
	    return af->control(af,AF_CONTROL_RESAMPLE_RATE | AF_CONTROL_SET, &rate);
	}
	case AF_CONTROL_POST_CREATE: return MPXP_Ok;
	case AF_CONTROL_RESAMPLE_RATE | AF_CONTROL_SET: {
	    af->conf.rate = ((int*)arg)[0];
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
    if(s->ctx) swr_free(&s->ctx);
    s->ctx=NULL;
    delete s;
}

// Filter data through filter
static mp_aframe_t* __FASTCALL__ play(struct af_instance_s* af,const mp_aframe_t* in)
{
    int rc;
    mp_aframe_t* out = new_mp_aframe_genome(in);
    out->len=af_lencalc(af->mul,in);
    mp_alloc_aframe(out);

    af_resample_t*	s = (af_resample_t*)af->setup;

    const uint8_t*	ain[SWR_CH_MAX];
    uint8_t*		aout[SWR_CH_MAX];

    aout[0]=reinterpret_cast<uint8_t*>(out->audio);
    ain[0]=reinterpret_cast<uint8_t*>(in->audio);

    rc=swr_convert(s->ctx,aout,out->len/(out->nch*(out->format&MPAF_BPS_MASK)),ain,in->len/(in->nch*(in->format&MPAF_BPS_MASK)));
    if(rc<0)	MSG_ERR("%i=swr_convert\n",rc);
    else	out->len=rc*out->nch*(out->format&MPAF_BPS_MASK);

    out->rate = af->conf.rate;
    return out;
}

// Allocate memory and set function pointers
static MPXP_Rc __FASTCALL__ af_open(af_instance_t* af){
    af->config=af_config;
    af->control=control;
    af->uninit=uninit;
    af->play=play;
    af->mul.n=1;
    af->mul.d=1;
    af->setup=mp_calloc(1,sizeof(af_resample_t));
    if(af->setup == NULL) return MPXP_Error;
    check_pin("afilter",af->pin,AF_PIN);
    return MPXP_Ok;
}

// Description of this plugin
extern const af_info_t af_info_resample = {
    "Sample frequency conversion",
    "resample",
    "nickols_k",
    "",
    AF_FLAGS_REENTRANT,
    af_open
};

