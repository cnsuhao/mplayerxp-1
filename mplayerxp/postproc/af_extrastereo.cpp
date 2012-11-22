/*=============================================================================
//
//  This software has been released under the terms of the GNU General Public
//  license. See http://www.gnu.org/copyleft/gpl.html for details.
//
//  Copyright 2004 Alex Beregszaszi & Pierre Lombard
//
//=============================================================================
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <inttypes.h>
#include <math.h>
#include <limits.h>

#include "af.h"
#include "osdep/mplib.h"
#include "pp_msg.h"

using namespace mpxp;

// Data for specific instances of this filter
typedef struct af_extrastereo_s
{
    float mul;
}af_extrastereo_t;

static MPXP_Rc __FASTCALL__ config(struct af_instance_s* af,const af_conf_t* arg)
{
    // Sanity check
    if(!arg) return MPXP_Error;

    if(!mpaf_testa(arg->format,MPAF_SI|MPAF_NE) || (arg->nch != 2))
       return MPXP_Error;

    af->conf.rate   = arg->rate;
    af->conf.nch    = 2;
    af->conf.format = MPAF_SI|MPAF_NE|MPAF_BPS_2;

    return af_test_output(af,arg);
}
// Initialization and runtime control
static MPXP_Rc __FASTCALL__ control(struct af_instance_s* af, int cmd, any_t* arg)
{
  af_extrastereo_t* s   = (af_extrastereo_t*)af->setup;

  switch(cmd){
  case AF_CONTROL_SHOWCONF:
    MSG_INFO("[af_extrastereo] %f\n",s->mul);
    return MPXP_Ok;
  case AF_CONTROL_COMMAND_LINE:{
    float f;
    sscanf((char*)arg,"%f", &f);
    s->mul = f;
    return MPXP_Ok;
  }
  case AF_CONTROL_ES_MUL | AF_CONTROL_SET:
    s->mul = *(float*)arg;
    return MPXP_Ok;
  case AF_CONTROL_ES_MUL | AF_CONTROL_GET:
    *(float*)arg = s->mul;
    return MPXP_Ok;
  default: break;
  }
  return MPXP_Unknown;
}

// Deallocate memory
static void __FASTCALL__ uninit(struct af_instance_s* af)
{
    if(af->setup) delete af->setup;
}

// Filter data through filter
static mp_aframe_t* __FASTCALL__ play(struct af_instance_s* af,const mp_aframe_t* ind)
{
    af_extrastereo_t *s = reinterpret_cast<af_extrastereo_t*>(af->setup);
    unsigned	i = 0;
    int16_t*	a = (int16_t*)ind->audio;	// Audio data
    unsigned	len = ind->len/2;		// Number of samples
    int		avg, l, r;
    mp_aframe_t*outd = new_mp_aframe_genome(ind);
    mp_alloc_aframe(outd);

    for (i = 0; i < len; i+=2) {
	avg = (a[i] + a[i + 1]) / 2;

	l = avg + (int)(s->mul * (a[i] - avg));
	r = avg + (int)(s->mul * (a[i + 1] - avg));

	((int16_t*)outd->audio)[i] = clamp(l, SHRT_MIN, SHRT_MAX);
	((int16_t*)outd->audio)[i + 1] = clamp(r, SHRT_MIN, SHRT_MAX);
  }

  return outd;
}

// Allocate memory and set function pointers
static MPXP_Rc __FASTCALL__ af_open(af_instance_t* af){
  af->config=config;
  af->control=control;
  af->uninit=uninit;
  af->play=play;
  af->mul.n=1;
  af->mul.d=1;
  af->setup=mp_calloc(1,sizeof(af_extrastereo_t));
  if(af->setup == NULL) return MPXP_Error;
  ((af_extrastereo_t*)af->setup)->mul = 2.5;
    check_pin("afilter",af->pin,AF_PIN);
  return MPXP_Ok;
}

// Description of this filter
extern const af_info_t af_info_extrastereo = {
    "Extra stereo",
    "extrastereo",
    "Alex Beregszaszi & Pierre Lombard",
    "",
    AF_FLAGS_NOT_REENTRANT,
    af_open
};
