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
#include "osdep/mplib.h"
#include "pp_msg.h"

// Data for specific instances of this filter
typedef struct af_pan_s
{
  float level[AF_NCH][AF_NCH];	// Gain level for each channel
}af_pan_t;

// Initialization and runtime control
static ControlCodes __FASTCALL__ control(struct af_instance_s* af, int cmd, any_t* arg)
{
  af_pan_t* s = af->setup; 

  switch(cmd){
  case AF_CONTROL_REINIT:
    // Sanity check
    if(!arg) return CONTROL_ERROR;

    af->data->rate   = ((mp_aframe_t*)arg)->rate;
    af->data->format = MPAF_F|MPAF_NE|4;
    af->mul.n        = af->data->nch;
    af->mul.d	     = ((mp_aframe_t*)arg)->nch;

    if((af->data->format != ((mp_aframe_t*)arg)->format)) {
	((mp_aframe_t*)arg)->format = af->data->format;
	return CONTROL_FALSE;
    }
    return control(af,AF_CONTROL_PAN_NOUT | AF_CONTROL_SET, &af->data->nch);
  case AF_CONTROL_COMMAND_LINE:{
    int   nch = 0;
    int   n = 0;
    char* cp = NULL;
    int   j,k;
    // Read number of outputs
    sscanf((char*)arg,"%i%n", &nch,&n);
    if(CONTROL_OK != control(af,AF_CONTROL_PAN_NOUT | AF_CONTROL_SET, &nch))
      return CONTROL_ERROR;

    // Read pan values
    cp = &((char*)arg)[n];
    j = 0; k = 0;
    while((*cp == ':') && (k < AF_NCH)){
      sscanf(cp, ":%f%n" , &s->level[k][j], &n);
      s->level[k][j] = clamp(s->level[k][j],0.0,1.0);
      MSG_V("[pan] Pan level from channel %i to" 
	     " channel %i = %f\n",j,k,s->level[k][j]);
      cp =&cp[n];
      j++;
      if(j>=nch){
	j = 0; 
	k++;
      }
    }
    return CONTROL_OK;
  }
  case AF_CONTROL_PAN_LEVEL | AF_CONTROL_SET:{
    int    i;
    int    ch = ((af_control_ext_t*)arg)->ch;
    float* level = ((af_control_ext_t*)arg)->arg;
    for(i=0;i<AF_NCH;i++)
      s->level[ch][i] = clamp(level[i],0.0,1.0);
    return CONTROL_OK;
  }
  case AF_CONTROL_PAN_LEVEL | AF_CONTROL_GET:{
    int    i;
    int ch = ((af_control_ext_t*)arg)->ch;
    float* level = ((af_control_ext_t*)arg)->arg;
    for(i=0;i<AF_NCH;i++)
      level[i] = s->level[ch][i];
    return CONTROL_OK;
  }
  case AF_CONTROL_PAN_NOUT | AF_CONTROL_SET:
    // Reinit must be called after this function has been called

    // Sanity check
    if(((int*)arg)[0] <= 0 || ((int*)arg)[0] > AF_NCH){
      MSG_ERR("[pan] The number of output channels must be" 
	     " between 1 and %i. Current value is %i\n",AF_NCH,((int*)arg)[0]);
      return CONTROL_ERROR;
    }
    af->data->nch=((int*)arg)[0];
    return CONTROL_OK;
  case AF_CONTROL_PAN_NOUT | AF_CONTROL_GET:
    *(int*)arg = af->data->nch;
    return CONTROL_OK;
  default: break;
  }
  return CONTROL_UNKNOWN;
}

// Deallocate memory 
static void __FASTCALL__ uninit(struct af_instance_s* af)
{
  if(af->data->audio)
    mp_free(af->data->audio);
  if(af->data)
    mp_free(af->data);
  if(af->setup)
    mp_free(af->setup);
}

// Filter data through filter
static mp_aframe_t* __FASTCALL__ play(struct af_instance_s* af, mp_aframe_t* data,int final)
{
  mp_aframe_t*    c    = data;		// Current working data
  mp_aframe_t*	l    = af->data;	// Local data
  af_pan_t*  	s    = af->setup; 	// Setup for this instance
  float*   	in   = c->audio;	// Input audio data
  float*   	out  = NULL;		// Output audio data
  float*	end  = in+c->len/4; 	// End of loop
  int		nchi = c->nch;		// Number of input channels
  int		ncho = l->nch;		// Number of output channels
  register int  j,k;

  if(CONTROL_OK != RESIZE_LOCAL_BUFFER(af,data))
    return NULL;

  out = l->audio;
  // Execute panning 
  // FIXME: Too slow
  while(in < end){
    for(j=0;j<ncho;j++){
      register float  x   = 0.0;
      register float* tin = in;
      for(k=0;k<nchi;k++)
	x += tin[k] * s->level[j][k];
      out[j] = x;
    }
    out+= ncho;
    in+= nchi;
  }

  // Set output data
  c->audio = l->audio;
  c->len   = (c->len*af->mul.n)/af->mul.d;
  c->nch   = l->nch;

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
  af->setup=mp_calloc(1,sizeof(af_pan_t));
  if(af->data == NULL || af->setup == NULL)
    return CONTROL_ERROR;
  // Set initial pan to pass-through.
  return CONTROL_OK;
}

// Description of this filter
const af_info_t af_info_pan = {
    "Panning audio filter",
    "pan",
    "Anders",
    "",
    AF_FLAGS_NOT_REENTRANT,
    open
};
