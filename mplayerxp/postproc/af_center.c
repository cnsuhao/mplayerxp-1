/*
    (C) Alex Beregszaszi
    License: GPL

    This filter adds a center channel to the audio stream by
    averaging the left and right channel.
    There are two runtime controls one for setting which channel to
    insert the center-audio into called AF_CONTROL_SUB_CH.

    FIXME: implement a high-pass filter for better results.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "af.h"
#include "osdep/mplib.h"
#include "pp_msg.h"

// Data for specific instances of this filter
typedef struct af_center_s
{
    unsigned ch;		// Channel number which to insert the filtered data
}af_center_t;

// Initialization and runtime control
static MPXP_Rc control(struct af_instance_s* af, int cmd, any_t* arg)
{
  af_center_t* s   = af->setup;

  switch(cmd){
  case AF_CONTROL_REINIT:{
    // Sanity check
    if(!arg) return MPXP_Error;

    af->data->rate   = ((mp_aframe_t*)arg)->rate;
    af->data->nch    = max(s->ch+1,((mp_aframe_t*)arg)->nch);
    af->data->format = MPAF_NE|MPAF_F|4;

    return af_test_output(af,(mp_aframe_t*)arg);
  }
  case AF_CONTROL_COMMAND_LINE:{
    int   ch=1;
    sscanf(arg,"%i", &ch);
    // Sanity check
    if((ch >= AF_NCH) || (ch < 0)){
      MSG_ERR("[sub] Center channel number must be between "
	     " 0 and %i current value is %i\n", AF_NCH-1, ch);
      return MPXP_Error;
    }
    s->ch = ch;
    return MPXP_Ok;
  }
  case AF_CONTROL_SHOWCONF:
  {
    MSG_INFO("[af_center] %i\n",s->ch);
    return MPXP_Ok;
  }
  }
  return MPXP_Unknown;
}

// Deallocate memory
static void uninit(struct af_instance_s* af)
{
  if(af->data)
    mp_free(af->data);
  if(af->setup)
    mp_free(af->setup);
}

// Filter data through filter
static mp_aframe_t* play(struct af_instance_s* af, mp_aframe_t* data,int final)
{
  mp_aframe_t*    c   = data;	 // Current working data
  af_center_t*  s   = af->setup; // Setup for this instance
  float*   	a   = c->audio;	 // Audio data
  int		len = c->len/4;	 // Number of samples in current audio block
  int		nch = c->nch;	 // Number of channels
  int		ch  = s->ch;	 // Channel in which to insert the center audio
  register int  i;

  // Run filter
  for(i=0;i<len;i+=nch){
    // Average left and right
    a[i+ch] = (a[i]/2) + (a[i+1]/2);
  }

  return c;
}

// Allocate memory and set function pointers
static MPXP_Rc open(af_instance_t* af){
  af_center_t* s;
  af->control=control;
  af->uninit=uninit;
  af->play=play;
  af->mul.n=1;
  af->mul.d=1;
  af->data=mp_calloc(1,sizeof(mp_aframe_t));
  af->setup=s=mp_calloc(1,sizeof(af_center_t));
  if(af->data == NULL || af->setup == NULL)
    return MPXP_Error;
  // Set default values
  s->ch = 1;  	 // Channel nr 2
    check_pin("afilter",af->pin,AF_PIN);
  return MPXP_Ok;
}

// Description of this filter
const af_info_t af_info_center = {
    "Audio filter for adding a center channel",
    "center",
    "Alex Beregszaszi",
    "",
    AF_FLAGS_REENTRANT,
    open
};
