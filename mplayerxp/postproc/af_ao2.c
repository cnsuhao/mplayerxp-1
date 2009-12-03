/* The name speaks for itself this filter is a dummy and will not blow
   up regardless of what you do with it. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../libao2/audio_out.h"

#include "af.h"

static unsigned rates[] =
{ 4000, 5512, 8000, 9600, 11025, 16000, 19200, 22050, 24000, 32000, 38400, 44100, 48000, 64000, 76800, 88200, 96000, 128000, 153600, 176400, 192000 };
static unsigned __FASTCALL__ find_best_rate(unsigned irate)
{
    unsigned i,ii;
    int rval;
    rval=ao_control(AOCONTROL_QUERY_RATE,irate);
    if(rval == CONTROL_TRUE) return irate;
    for(i=0;i<sizeof(rates)/sizeof(unsigned)-1;i++)
    {
	if(irate >= rates[i] && irate < rates[i+1]) break;
    }
    ii=i;
    for(;i<sizeof(rates)/sizeof(unsigned);i++)
    {
	rval=ao_control(AOCONTROL_QUERY_RATE,rates[i]);
	if(rval == CONTROL_TRUE) return rates[i];
    }
    i=ii;
    for(;i<sizeof(rates)/sizeof(unsigned);i--)
    {
	rval=ao_control(AOCONTROL_QUERY_RATE,rates[i]);
	if(rval == CONTROL_TRUE) return rates[i];
    }
    for(i=0;i<sizeof(rates)/sizeof(unsigned);i++)
    {
	rval=ao_control(AOCONTROL_QUERY_RATE,rates[i]);
	if(rval == CONTROL_TRUE) return rates[i];
    }
    return 44100;
}

static unsigned __FASTCALL__ find_best_ch(unsigned ich)
{
    unsigned i;
    int rval;
    rval=ao_control(AOCONTROL_QUERY_CHANNELS,ich);
    if(rval == CONTROL_TRUE) return ich;
    for(i=ich>1?ich:1;i<AF_NCH;i++)
    {
	rval=ao_control(AOCONTROL_QUERY_CHANNELS,i);
	if(rval == CONTROL_TRUE) return i;
    }
    for(i=1;i<AF_NCH;i++)
    {
	rval=ao_control(AOCONTROL_QUERY_CHANNELS,i);
	if(rval == CONTROL_TRUE) return i;
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
    rval=ao_control(AOCONTROL_QUERY_FORMAT,ifmt);
    if(rval == CONTROL_TRUE) return ifmt;
    rval=-1;
    for(i=0;i<sizeof(cvt_list)/sizeof(fmt_cvt_t);i++)
    {
	if(ifmt==cvt_list[i].base_fourcc) { rval=i; break; }
    }
    if(rval==-1) return 0; /* unknown format */
    i=rval;
    for(j=0;j<20;j++)
    {
	if(cvt_list[i].cvt_fourcc[j]==0) break;
	rval=ao_control(AOCONTROL_QUERY_FORMAT,cvt_list[i].cvt_fourcc[j]);
	if(rval == CONTROL_TRUE) return cvt_list[i].cvt_fourcc[j];
    }
    return AFMT_S16_LE;
}

typedef struct af_ao2_s{
  int rate;
  int nch;
  int format;
  int bps;
}af_ao2_t;

// Initialization and runtime control
static int __FASTCALL__ control(struct af_instance_s* af, int cmd, void* arg)
{
  af_ao2_t* s = af->setup;
  switch(cmd){
  case AF_CONTROL_REINIT:
    /* Sanity check */
    if(!arg) return AF_ERROR;
    s->rate = af->data->rate   = find_best_rate(((af_data_t*)arg)->rate);
    s->nch = af->data->nch    = find_best_ch(((af_data_t*)arg)->nch);
    s->format = af->data->format = af_format_decode(find_best_fmt(af_format_encode(((af_data_t*)arg))),&af->data->bps);
    s->bps = af->data->bps;
    return af_test_output(af,(af_data_t*)arg);
  case AF_CONTROL_SHOWCONF: {
    char sbuf[256];
    const ao_info_t*info=ao_get_info();
    MSG_INFO("AO-CONF: [%s] %uHz nch=%u %s (%3.1f-kbit)\n"
	    ,info->short_name,s->rate,s->nch,fmt2str(s->format,s->bps,sbuf,sizeof(sbuf))
	    ,(s->rate*s->nch*s->bps*8)*0.001f);
    return AF_OK;
  }
  default: break;
  }
  return AF_UNKNOWN;
}

// Deallocate memory 
static void __FASTCALL__ uninit(struct af_instance_s* af)
{
  if(af->data)
    free(af->data);
  if(af->setup)
    free(af->setup);
}

// Filter data through filter
static af_data_t* __FASTCALL__ play(struct af_instance_s* af, af_data_t* data)
{
  // Do something necessary to get rid of annoying warning during compile
  if(!af)
    MSG_ERR("EEEK: Argument af == NULL in af_dummy.c play().");
  return data;
}

// Allocate memory and set function pointers
static int __FASTCALL__ open(af_instance_t* af){
  af->control=control;
  af->uninit=uninit;
  af->play=play;
  af->mul.d=1;
  af->mul.n=1;
  af->data=malloc(sizeof(af_data_t));
  af->setup=calloc(1,sizeof(af_ao2_t));
  if((af->data == NULL) || (af->setup == NULL)) return AF_ERROR;
  return AF_OK;
}

// Description of this filter
const af_info_t af_info_ao = {
    "libao wrapper",
    "ao2",
    "Nickols_K",
    "",
    AF_FLAGS_REENTRANT,
    open
};
