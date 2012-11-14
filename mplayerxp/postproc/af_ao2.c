/* The name speaks for itself this filter is a dummy and will not blow
   up regardless of what you do with it. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libao2/audio_out.h"
#include "libao2/afmt.h"
#include "osdep/mplib.h"

#include "af.h"
#include "pp_msg.h"

extern ao_data_t* ao_data;

static unsigned rates[] =
{ 4000, 5512, 8000, 9600, 11025, 16000, 19200, 22050, 24000, 32000, 38400, 44100, 48000, 64000, 76800, 88200, 96000, 128000, 153600, 176400, 192000 };

static unsigned __FASTCALL__ find_best_rate(unsigned irate)
{
    unsigned i,ii;
    int rval;
    rval=RND_RENAME7(ao_control)(ao_data,AOCONTROL_QUERY_RATE,irate);
    if(rval == MPXP_True) return irate;
    for(i=0;i<sizeof(rates)/sizeof(unsigned)-1;i++) {
	if(irate >= rates[i] && irate < rates[i+1]) break;
    }
    ii=i;
    for(;i<sizeof(rates)/sizeof(unsigned);i++) {
	rval=RND_RENAME7(ao_control)(ao_data,AOCONTROL_QUERY_RATE,rates[i]);
	if(rval == MPXP_True) return rates[i];
    }
    i=ii;
    for(;i<sizeof(rates)/sizeof(unsigned);i--) {
	rval=RND_RENAME7(ao_control)(ao_data,AOCONTROL_QUERY_RATE,rates[i]);
	if(rval == MPXP_True) return rates[i];
    }
    for(i=0;i<sizeof(rates)/sizeof(unsigned);i++) {
	rval=RND_RENAME7(ao_control)(ao_data,AOCONTROL_QUERY_RATE,rates[i]);
	if(rval == MPXP_True) return rates[i];
    }
    return 44100;
}

static unsigned __FASTCALL__ find_best_ch(unsigned ich)
{
    unsigned i;
    int rval;
    rval=RND_RENAME7(ao_control)(ao_data,AOCONTROL_QUERY_CHANNELS,ich);
    if(rval == MPXP_True) return ich;
    for(i=ich>1?ich:1;i<AF_NCH;i++) {
	rval=RND_RENAME7(ao_control)(ao_data,AOCONTROL_QUERY_CHANNELS,i);
	if(rval == MPXP_True) return i;
    }
    for(i=1;i<AF_NCH;i++) {
	rval=RND_RENAME7(ao_control)(ao_data,AOCONTROL_QUERY_CHANNELS,i);
	if(rval == MPXP_True) return i;
    }
    return 2;
}

typedef struct fmt_cvs_s
{
    unsigned base_fourcc;
    unsigned cvt_fourcc[20];
}fmt_cvt_t;

static fmt_cvt_t cvt_list[] =
{
 { AFMT_FLOAT32, { AFMT_S32_LE, AFMT_S32_BE, AFMT_U32_BE, AFMT_U32_LE, AFMT_S24_LE, AFMT_S24_BE, AFMT_U24_LE, AFMT_U24_BE, AFMT_S16_LE, AFMT_S16_BE, AFMT_U16_LE, AFMT_U16_BE, AFMT_S8, AFMT_U8, 0 }},
 { AFMT_U32_BE, { AFMT_U32_LE, AFMT_S32_BE, AFMT_S32_LE, AFMT_FLOAT32, AFMT_U24_BE, AFMT_U24_LE, AFMT_S24_BE, AFMT_S24_LE, AFMT_U16_BE, AFMT_U16_LE, AFMT_S16_BE, AFMT_S16_LE, AFMT_U8, AFMT_S8, 0 }},
 { AFMT_U32_LE, { AFMT_U32_BE, AFMT_S32_LE, AFMT_S32_BE, AFMT_FLOAT32, AFMT_U24_LE, AFMT_U24_BE, AFMT_S24_LE, AFMT_S24_BE, AFMT_U16_LE, AFMT_U16_BE, AFMT_S16_LE, AFMT_S16_BE, AFMT_U8, AFMT_S8, 0 }},
 { AFMT_S32_BE, { AFMT_S32_LE, AFMT_U32_BE, AFMT_U32_LE, AFMT_FLOAT32, AFMT_S24_BE, AFMT_S24_LE, AFMT_U24_BE, AFMT_U24_LE, AFMT_S16_BE, AFMT_S16_LE, AFMT_U16_BE, AFMT_U16_LE, AFMT_S8, AFMT_U8, 0 }},
 { AFMT_S32_LE, { AFMT_S32_BE, AFMT_U32_LE, AFMT_U32_BE, AFMT_FLOAT32, AFMT_S24_LE, AFMT_S24_BE, AFMT_U24_LE, AFMT_U24_BE, AFMT_S16_LE, AFMT_S16_BE, AFMT_U16_LE, AFMT_U16_BE, AFMT_S8, AFMT_U8, 0 }},
 { AFMT_U24_BE, { AFMT_U24_LE, AFMT_S24_BE, AFMT_S24_LE, AFMT_U32_BE, AFMT_U32_LE, AFMT_S32_BE, AFMT_S32_LE, AFMT_FLOAT32, AFMT_U16_BE, AFMT_U16_LE, AFMT_S16_BE, AFMT_S16_LE, AFMT_U8, AFMT_S8, 0 }},
 { AFMT_U24_LE, { AFMT_U24_BE, AFMT_S24_LE, AFMT_S24_BE, AFMT_U32_LE, AFMT_U32_BE, AFMT_S32_LE, AFMT_S32_BE, AFMT_FLOAT32, AFMT_U16_LE, AFMT_U16_BE, AFMT_S16_LE, AFMT_S16_BE, AFMT_U8, AFMT_S8, 0 }},
 { AFMT_S24_BE, { AFMT_S24_LE, AFMT_U24_BE, AFMT_U24_LE, AFMT_S32_BE, AFMT_S32_LE, AFMT_U32_BE, AFMT_U32_LE, AFMT_FLOAT32, AFMT_S16_BE, AFMT_S16_LE, AFMT_U16_BE, AFMT_U16_LE, AFMT_S8, AFMT_U8, 0 }},
 { AFMT_S24_LE, { AFMT_S24_BE, AFMT_U24_LE, AFMT_U24_BE, AFMT_S32_LE, AFMT_S32_BE, AFMT_U32_LE, AFMT_U32_BE, AFMT_FLOAT32, AFMT_S16_LE, AFMT_S16_BE, AFMT_U16_LE, AFMT_U16_BE, AFMT_S8, AFMT_U8, 0 }},
 { AFMT_U16_BE, { AFMT_U16_LE, AFMT_S16_BE, AFMT_S16_LE, AFMT_U24_BE, AFMT_U24_LE, AFMT_S24_BE, AFMT_S24_LE, AFMT_U32_BE, AFMT_U32_LE, AFMT_S32_BE, AFMT_S32_LE, AFMT_FLOAT32, AFMT_U8, AFMT_S8, 0 }},
 { AFMT_U16_LE, { AFMT_U16_BE, AFMT_S16_LE, AFMT_S16_BE, AFMT_U24_LE, AFMT_U24_BE, AFMT_S24_LE, AFMT_S24_BE, AFMT_U32_LE, AFMT_U32_BE, AFMT_S32_LE, AFMT_S32_BE, AFMT_FLOAT32, AFMT_U8, AFMT_S8, 0 }},
 { AFMT_S16_BE, { AFMT_S16_LE, AFMT_U16_BE, AFMT_U16_LE, AFMT_S24_BE, AFMT_S24_LE, AFMT_U24_BE, AFMT_U24_LE, AFMT_S32_BE, AFMT_S32_LE, AFMT_U32_BE, AFMT_U32_LE, AFMT_FLOAT32, AFMT_S8, AFMT_U8, 0 }},
 { AFMT_S16_LE, { AFMT_S16_BE, AFMT_U16_LE, AFMT_U16_BE, AFMT_S24_LE, AFMT_S24_BE, AFMT_U24_LE, AFMT_U24_BE, AFMT_S32_LE, AFMT_S32_BE, AFMT_U32_LE, AFMT_U32_BE, AFMT_FLOAT32, AFMT_S8, AFMT_U8, 0 }},
 { AFMT_U8, { AFMT_S8, AFMT_U16_LE, AFMT_U16_BE, AFMT_S16_LE, AFMT_S16_BE, AFMT_U24_LE, AFMT_U24_BE, AFMT_S24_LE, AFMT_S24_BE, AFMT_U32_LE, AFMT_U32_BE, AFMT_S32_LE, AFMT_S32_BE, AFMT_FLOAT32, 0 }},
 { AFMT_S8, { AFMT_U8, AFMT_S16_LE, AFMT_S16_BE, AFMT_U16_LE, AFMT_U16_BE, AFMT_S24_LE, AFMT_S24_BE, AFMT_U24_LE, AFMT_U24_BE, AFMT_S32_LE, AFMT_S32_BE, AFMT_U32_LE, AFMT_U32_BE, AFMT_FLOAT32, 0 }},
};

static unsigned __FASTCALL__ find_best_fmt(unsigned ifmt)
{
    unsigned i,j;
    int rval;
    rval=RND_RENAME7(ao_control)(ao_data,AOCONTROL_QUERY_FORMAT,ifmt);
    if(rval == MPXP_True) return ifmt;
    rval=-1;
    for(i=0;i<sizeof(cvt_list)/sizeof(fmt_cvt_t);i++) {
	if(ifmt==cvt_list[i].base_fourcc) { rval=i; break; }
    }
    if(rval==-1) return 0; /* unknown format */
    i=rval;
    for(j=0;j<20;j++) {
	if(cvt_list[i].cvt_fourcc[j]==0) break;
	rval=RND_RENAME7(ao_control)(ao_data,AOCONTROL_QUERY_FORMAT,cvt_list[i].cvt_fourcc[j]);
	if(rval == MPXP_True) return cvt_list[i].cvt_fourcc[j];
    }
    return AFMT_S16_LE;
}

typedef struct af_ao2_s{
    unsigned		rate;
    unsigned		nch;
    mpaf_format_e	format;
}af_ao2_t;

// Initialization and runtime control
static MPXP_Rc __FASTCALL__ control(struct af_instance_s* af, int cmd, any_t* arg)
{
    af_ao2_t* s = af->setup;
    switch(cmd){
	case AF_CONTROL_REINIT:
	    /* Sanity check */
	    if(!arg) return MPXP_Error;
	    s->rate = af->data->rate = find_best_rate(((mp_aframe_t*)arg)->rate);
	    s->nch = af->data->nch  = find_best_ch(((mp_aframe_t*)arg)->nch);
	    s->format = af->data->format = mpaf_format_decode(find_best_fmt(mpaf_format_encode(((mp_aframe_t*)arg)->format)));
	    return af_test_output(af,(mp_aframe_t*)arg);
	case AF_CONTROL_SHOWCONF: {
	    char sbuf[256];
	    const ao_info_t*info=ao_get_info(ao_data);
	    MSG_INFO("AO-CONF: [%s] %uHz nch=%u %s (%3.1f-kbit)\n"
		,info->short_name,s->rate,s->nch,mpaf_fmt2str(s->format,sbuf,sizeof(sbuf))
		,(s->rate*s->nch*(s->format&MPAF_BPS_MASK)*8)*0.001f);
	    return MPXP_Ok;
	}
	default: break;
    }
    return MPXP_Unknown;
}

// Deallocate memory
static void __FASTCALL__ uninit(struct af_instance_s* af)
{
    if(af->data) mp_free(af->data);
    if(af->setup) mp_free(af->setup);
}

// Filter data through filter
static mp_aframe_t* __FASTCALL__ play(struct af_instance_s* af, mp_aframe_t* data,int final)
{
  // Do something necessary to get rid of annoying warning during compile
    if(!af) MSG_ERR("EEEK: Argument af == NULL in af_dummy.c play().");
    return data;
}

// Allocate memory and set function pointers
static MPXP_Rc __FASTCALL__ af_open(af_instance_t* af){
    af->control=control;
    af->uninit=uninit;
    af->play=play;
    af->mul.d=1;
    af->mul.n=1;
    af->data=mp_malloc(sizeof(mp_aframe_t));
    af->setup=mp_calloc(1,sizeof(af_ao2_t));
    if((af->data == NULL) || (af->setup == NULL)) return MPXP_Error;
    check_pin("afilter",af->pin,AF_PIN);
    return MPXP_Ok;
}

// Description of this filter
const af_info_t af_info_ao = {
    "libao wrapper",
    "ao2",
    "Nickols_K",
    "",
    AF_FLAGS_REENTRANT,
    af_open
};
