#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
/*=============================================================================
//
//  This software has been released under the terms of the GNU General Public
//  license. See http://www.gnu.org/copyleft/gpl.html for details.
//
//  Copyright 2002 Anders Johansson ajh@atri.curtin.edu.au
//
//=============================================================================
*/

/* */

#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <inttypes.h>
#include <math.h>
#include <limits.h>

#include "af.h"
#include "pp_msg.h"

// Data for specific instances of this filter
struct af_pan_t
{
    float level[AF_NCH][AF_NCH];	// Gain level for each channel
};

// Initialization and runtime control_af
static MPXP_Rc __FASTCALL__ control_af(af_instance_t* af, int cmd, any_t* arg)
{
  af_pan_t* s = reinterpret_cast<af_pan_t*>(af->setup);

  switch(cmd){
  case AF_CONTROL_COMMAND_LINE:{
    int   nch = 0;
    int   n = 0;
    char* cp = NULL;
    int   j,k;
    // Read number of outputs
    sscanf((char*)arg,"%i%n", &nch,&n);
    if(MPXP_Ok != control_af(af,AF_CONTROL_PAN_NOUT | AF_CONTROL_SET, &nch))
      return MPXP_Error;

    // Read pan values
    cp = &((char*)arg)[n];
    j = 0; k = 0;
    while((*cp == ':') && (k < AF_NCH)){
      sscanf(cp, ":%f%n" , &s->level[k][j], &n);
      s->level[k][j] = clamp(s->level[k][j],0.0f,1.0f);
      MSG_V("[pan] Pan level from channel %i to"
	     " channel %i = %f\n",j,k,s->level[k][j]);
      cp =&cp[n];
      j++;
      if(j>=nch){
	j = 0;
	k++;
      }
    }
    return MPXP_Ok;
  }
  case AF_CONTROL_PAN_LEVEL | AF_CONTROL_SET:{
    af_control_ext_t*ce=reinterpret_cast<af_control_ext_t*>(arg);
    unsigned i;
    int ch = ce->ch;
    float* level = reinterpret_cast<float*>(ce->arg);
    for(i=0;i<AF_NCH;i++)
      s->level[ch][i] = clamp(level[i],0.0f,1.0f);
    return MPXP_Ok;
  }
  case AF_CONTROL_PAN_LEVEL | AF_CONTROL_GET:{
    af_control_ext_t*ce=reinterpret_cast<af_control_ext_t*>(arg);
    unsigned i;
    int ch = ce->ch;
    float* level = reinterpret_cast<float*>(ce->arg);
    for(i=0;i<AF_NCH;i++)
      level[i] = s->level[ch][i];
    return MPXP_Ok;
  }
  case AF_CONTROL_PAN_NOUT | AF_CONTROL_SET:
    // Reinit must be called after this function has been called

    // Sanity check
    if(((int*)arg)[0] <= 0 || ((int*)arg)[0] > AF_NCH){
      MSG_ERR("[pan] The number of output channels must be"
	     " between 1 and %i. Current value is %i\n",AF_NCH,((int*)arg)[0]);
      return MPXP_Error;
    }
    af->conf.nch=((int*)arg)[0];
    return MPXP_Ok;
  case AF_CONTROL_PAN_NOUT | AF_CONTROL_GET:
    *(int*)arg = af->conf.nch;
    return MPXP_Ok;
  default: break;
  }
  return MPXP_Unknown;
}

static MPXP_Rc __FASTCALL__ af_config(af_instance_t* af,const af_conf_t* arg)
{
    // Sanity check
    if(!arg) return MPXP_Error;

    af->conf.rate   = arg->rate;
    af->conf.format = MPAF_F|MPAF_NE|MPAF_BPS_4;
    af->mul.n       = af->conf.nch;
    af->mul.d       = arg->nch;

    if((af->conf.format != arg->format)) return MPXP_False;
    return control_af(af,AF_CONTROL_PAN_NOUT | AF_CONTROL_SET, &af->conf.nch);
}

// Deallocate memory
static void __FASTCALL__ uninit(af_instance_t* af)
{
  if(af->setup) delete af->setup;
}

// Filter data through filter
static mp_aframe_t* __FASTCALL__ play(af_instance_t* af,const mp_aframe_t* ind)
{
    const mp_aframe_t*c= ind;		// Current working data
    af_pan_t*	s    = reinterpret_cast<af_pan_t*>(af->setup);	// Setup for this instance
    float*	in   = reinterpret_cast<float*>(c->audio);// Input audio data
    float*	out  = NULL;		// Output audio data
    float*	end  = in+c->len/4; 	// End of loop
    unsigned	nchi = c->nch;		// Number of input channels
    unsigned	ncho = af->conf.nch;	// Number of output channels
    unsigned	j,k;

    mp_aframe_t* outd = new_mp_aframe_genome(ind);
    outd->len = af_lencalc(af->mul,ind);
    mp_alloc_aframe(outd);

    out = reinterpret_cast<float*>(outd->audio);
    // Execute panning
    // FIXME: Too slow
    while(in < end){
	for(j=0;j<ncho;j++){
	    float  x   = 0.0;
	    float* tin = in;
	    for(k=0;k<nchi;k++) x += tin[k] * s->level[j][k];
	    out[j] = x;
	}
	out+= ncho;
	in+= nchi;
    }

    return outd;
}

// Allocate memory and set function pointers
static MPXP_Rc __FASTCALL__ af_open(af_instance_t* af){
  af->config_af=af_config;
  af->control_af=control_af;
  af->uninit=uninit;
  af->play=play;
  af->mul.n=1;
  af->mul.d=1;
  af->setup=mp_calloc(1,sizeof(af_pan_t));
  if(af->setup == NULL) return MPXP_Error;
  // Set initial pan to pass-through.
    check_pin("afilter",af->pin,AF_PIN);
  return MPXP_Ok;
}

// Description of this filter
extern const af_info_t af_info_pan = {
    "Panning audio filter",
    "pan",
    "Anders",
    "",
    AF_FLAGS_NOT_REENTRANT,
    af_open
};
