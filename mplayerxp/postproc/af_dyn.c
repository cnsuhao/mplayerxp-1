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
#include "osdep/mplib.h"

typedef struct af_dyn_s
{
    float gain;
}af_dyn_t;
// Data for specific instances of this filter
// Initialization and runtime control
static MPXP_Rc __FASTCALL__ control(struct af_instance_s* af, int cmd, any_t* arg)
{
  af_dyn_t* s   = (af_dyn_t*)af->setup;
  switch(cmd){
  case AF_CONTROL_REINIT:
    // Sanity check
    if(!arg) return MPXP_Error;

    af->data->rate   = ((mp_aframe_t*)arg)->rate;
    af->data->nch    = ((mp_aframe_t*)arg)->nch;
    af->data->format = MPAF_F|MPAF_NE|4;

    return af_test_output(af,(mp_aframe_t*)arg);
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
static void __FASTCALL__ uninit(struct af_instance_s* af)
{
  if(af->data)
    mp_free(af->data);
  if(af->setup)
    mp_free(af->setup);
}

// Filter data through filter
static mp_aframe_t* __FASTCALL__ play(struct af_instance_s* af, mp_aframe_t* data,int final)
{
  register unsigned i = 0;
  float *in = (float*)data->audio;	// Audio data
  af_dyn_t *s=af->setup;
  unsigned nsamples = data->len/4;		// Number of samples
  float d,l;
  int sign;
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
	*in++ = l;
  }

  return data;
}

// Allocate memory and set function pointers
static MPXP_Rc __FASTCALL__ open(af_instance_t* af){
  af->control=control;
  af->uninit=uninit;
  af->play=play;
  af->mul.n=1;
  af->mul.d=1;
  af->data=mp_calloc(1,sizeof(mp_aframe_t));
  af->setup=mp_calloc(1,sizeof(af_dyn_t));
  if(af->data == NULL || af->setup==NULL) return MPXP_Error;
  ((af_dyn_t *)(af->setup))->gain=8.;
    check_pin("afilter",af->pin,AF_PIN);
  return MPXP_Ok;
}

// Description of this filter
const af_info_t af_info_dyn = {
    "Dynamic compander",
    "dyn",
    "Nickols_K",
    "",
    AF_FLAGS_REENTRANT,
    open
};
