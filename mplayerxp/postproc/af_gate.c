/*=============================================================================
//
//  This software has been released under the terms of the GNU General Public
//  license. See http://www.gnu.org/copyleft/gpl.html for details.
//
//  Copyright 2002 Anders Johansson ajh@atri.curtin.edu.au
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

// Data for specific instances of this filter
typedef struct af_gate_s
{
  int   enable[AF_NCH];		// Enable/disable / channel
  float time[AF_NCH];		// Forgetting factor for power estimate
  float	pow[AF_NCH];		// Estimated power level [dB]
  float	tresh[AF_NCH];		// Threshold [dB]
  int	attack[AF_NCH];		// Attack time [ms]
  int	release[AF_NCH];	// Release time [ms]
  float	range[AF_NCH];		// Range level [dB]
}af_gate_t;

// Initialization and runtime control
static ControlCodes __FASTCALL__ control(struct af_instance_s* af, int cmd, any_t* arg)
{
  af_gate_t* s   = (af_gate_t*)af->setup; 

  switch(cmd){
  case AF_CONTROL_REINIT:
    // Sanity check
    if(!arg) return CONTROL_ERROR;

    af->data->rate   = ((mp_aframe_t*)arg)->rate;
    af->data->nch    = ((mp_aframe_t*)arg)->nch;
    af->data->format = MPAF_F|MPAF_NE|4;

    // Time constant set to 0.1s
    //    s->alpha = (1.0/0.2)/(2.0*M_PI*(float)((mp_aframe_t*)arg)->rate); 
    return af_test_output(af,(mp_aframe_t*)arg);
  case AF_CONTROL_COMMAND_LINE:{
/*     float v=-10.0; */
/*     float vol[AF_NCH]; */
/*     float s=0.0; */
/*     float clipp[AF_NCH]; */
/*     int i; */
/*     sscanf((char*)arg,"%f:%f", &v, &s); */
/*     for(i=0;i<AF_NCH;i++){ */
/*       vol[i]=v; */
/*       clipp[i]=s; */
/*     } */
/*     if(CONTROL_OK != control(af,AF_CONTROL_VOLUME_SOFTCLIP | AF_CONTROL_SET, clipp)) */
/*       return CONTROL_ERROR; */
/*     return control(af,AF_CONTROL_VOLUME_LEVEL | AF_CONTROL_SET, vol); */
  }

  case AF_CONTROL_GATE_ON_OFF | AF_CONTROL_SET:
    memcpy(s->enable,(int*)arg,AF_NCH*sizeof(int));
    return CONTROL_OK; 
  case AF_CONTROL_GATE_ON_OFF | AF_CONTROL_GET:
    memcpy((int*)arg,s->enable,AF_NCH*sizeof(int));
    return CONTROL_OK; 
  case AF_CONTROL_GATE_THRESH | AF_CONTROL_SET:
    return af_from_dB(AF_NCH,(float*)arg,s->tresh,20.0,-60.0,-1.0);
  case AF_CONTROL_GATE_THRESH | AF_CONTROL_GET:
    return af_to_dB(AF_NCH,s->tresh,(float*)arg,10.0);
  case AF_CONTROL_GATE_ATTACK | AF_CONTROL_SET:
    return af_from_ms(AF_NCH,(float*)arg,s->attack,af->data->rate,500.0,0.1);
  case AF_CONTROL_GATE_ATTACK | AF_CONTROL_GET:
    return af_to_ms(AF_NCH,s->attack,(float*)arg,af->data->rate);
  case AF_CONTROL_GATE_RELEASE | AF_CONTROL_SET:
    return af_from_ms(AF_NCH,(float*)arg,s->release,af->data->rate,3000.0,10.0);
  case AF_CONTROL_GATE_RELEASE | AF_CONTROL_GET:
    return af_to_ms(AF_NCH,s->release,(float*)arg,af->data->rate);
  case AF_CONTROL_GATE_RANGE | AF_CONTROL_SET:
    return af_from_dB(AF_NCH,(float*)arg,s->range,20.0,100.0,0.0);
  case AF_CONTROL_GATE_RANGE | AF_CONTROL_GET:
    return af_to_dB(AF_NCH,s->range,(float*)arg,10.0);
  default: break;
  }
  return CONTROL_UNKNOWN;
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
  mp_aframe_t*    c   = data;			// Current working data
  af_gate_t*  	s   = (af_gate_t*)af->setup; 	// Setup for this instance
  float*   	a   = (float*)c->audio;		// Audio data
  int       	len = c->len/4;			// Number of samples
  int           ch  = 0;			// Channel counter
  register int	nch = c->nch;			// Number of channels	
  register int  i   = 0;


  // Noise gate
  for(ch = 0; ch < nch ; ch++){
    if(s->enable[ch]){
      float	t   = 1.0 - s->time[ch];
      for(i=ch;i<len;i+=nch){
	register float x 	= a[i];
	register float _pow 	= x*x;
	s->pow[ch] = t*s->pow[ch] +
	  _pow*s->time[ch]; // LP filter
	if(_pow < s->pow[ch]){
	  ;
	}
	else{
	  ;
	}
	a[i] = x;
      }
    }
  }
  return c;
}

// Allocate memory and set function pointers
static ControlCodes __FASTCALL__ open(af_instance_t* af){
  af->control=control;
  af->uninit=uninit;
  af->play=play;
  af->mul.n=1;
  af->mul.d=1;
  af->data=mp_calloc(1,sizeof(mp_aframe_t));
  af->setup=mp_calloc(1,sizeof(af_gate_t));
  if(af->data == NULL || af->setup == NULL)
    return CONTROL_ERROR;
  return CONTROL_OK;
}

// Description of this filter
const af_info_t af_info_gate = {
    "Noise gate audio filter",
    "gate",
    "Anders",
    "",
    AF_FLAGS_NOT_REENTRANT,
    open
};
