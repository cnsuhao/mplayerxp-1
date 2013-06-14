#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
/*=============================================================================
//
//  This software has been released under the terms of the GNU General Public
//  license. See http://www.gnu.org/copyleft/gpl.html for details.
//
//  Copyright 1991 Lance Norskog And Sundry Contributors (sox plugin for xmms)
//  Imported by Nickols_K 2004
//=============================================================================
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <inttypes.h>
#include <math.h>
#include <limits.h>

#include "mplayerxp.h"
#include "af.h"
#include "af_internal.h"

struct af_dyn_t
{
    float gain;
};

static MPXP_Rc __FASTCALL__ config_af(af_instance_t* af,const af_conf_t* arg)
{
    // Sanity check
    if(!arg) return MPXP_Error;

    af->conf.rate   = arg->rate;
    af->conf.nch    = arg->nch;
    af->conf.format = MPAF_F|MPAF_NE|MPAF_BPS_4;

    return af_test_output(af,arg);
}

// Data for specific instances of this filter
// Initialization and runtime control_af
static MPXP_Rc __FASTCALL__ control_af(af_instance_t* af, int cmd, any_t* arg)
{
  af_dyn_t* s   = (af_dyn_t*)af->setup;
  switch(cmd){
  case AF_CONTROL_COMMAND_LINE:{
    float f;
    sscanf((char*)arg,"%f", &f);
    s->gain = f;
    return MPXP_Ok;
  }
  default: break;
  }
  return MPXP_Unknown;
}

// Deallocate memory
static void __FASTCALL__ uninit(af_instance_t* af)
{
    if(af->setup) delete af->setup;
}

// Filter data through filter
static mp_aframe_t __FASTCALL__ play(af_instance_t* af,const mp_aframe_t& ind)
{
    unsigned	i = 0;
    float*	in = (float*)ind.audio;// Audio data
    af_dyn_t*	s=reinterpret_cast<af_dyn_t*>(af->setup);
    unsigned	nsamples = ind.len/4;	// Number of samples
    float	d,l;
    int sign;
    mp_aframe_t outd= ind.genome();
    outd.alloc();
    float*	out = (float*)outd.audio;// Audio data

    for(i = 0; i < nsamples; i++) {
	d = *in;
	if (d == 0.0) l = 0;
	else {
	    if (d < 0.0) {
		d *= -1.0;
		sign = -1;
	    } else	sign = 1;
	    l = pow(s->gain, log10(d))*sign;
	}
	*out++ = l;
    }

    return outd;
}

// Allocate memory and set function pointers
static MPXP_Rc __FASTCALL__ af_open(af_instance_t* af){
  af->config_af=config_af;
  af->control_af=control_af;
  af->uninit=uninit;
  af->play=play;
  af->mul.n=1;
  af->mul.d=1;
  af->setup=new(zeromem) af_dyn_t;

  ((af_dyn_t *)(af->setup))->gain=8.;
    check_pin("afilter",af->pin,AF_PIN);
  return MPXP_Ok;
}

// Description of this filter
extern const af_info_t af_info_dyn = {
    "Dynamic compander",
    "dyn",
    "Nickols_K",
    "",
    AF_FLAGS_REENTRANT,
    af_open
};
