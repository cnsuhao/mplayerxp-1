/* The name speaks for itself this filter is a dummy and will not blow
   up regardless of what you do with it. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "af.h"
#include "osdep/mplib.h"
#include "pp_msg.h"

typedef struct s_lp
{
    unsigned fin;
    unsigned fake_out;
}af_lp_t;

// Initialization and runtime control
static MPXP_Rc __FASTCALL__ control(struct af_instance_s* af, int cmd, any_t* arg)
{
  af_lp_t* s   = (af_lp_t*)af->setup; 
  switch(cmd){
  case AF_CONTROL_REINIT:
    memcpy(af->data,(mp_aframe_t*)arg,sizeof(mp_aframe_t));
    s->fin=af->data->rate;
    af->delay=(float)s->fake_out/af->data->rate;
    af->data->rate=s->fake_out;
    return MPXP_Ok;
  case AF_CONTROL_SHOWCONF:
    MSG_INFO("[af_lp] in %u faked out %u\n",((af_lp_t*)af->setup)->fin,((af_lp_t*)af->setup)->fake_out);
    return MPXP_Ok;
  case AF_CONTROL_COMMAND_LINE:{
    sscanf((char*)arg,"%i", &s->fake_out);
    // Sanity check
    if(s->fake_out < 8000 || s->fake_out > 192000){
      MSG_ERR("[af_lp] The output sample frequency " 
	     "must be between 8kHz and 192kHz. Current value is %i \n",
	     s->fake_out);
      return MPXP_Error;
    }
    s->fin=af->data->rate;
    af->delay=(float)s->fake_out/af->data->rate;
    af->data->rate=s->fake_out;
    return MPXP_Ok;
  }
  case AF_CONTROL_POST_CREATE:
    s->fin=s->fake_out=af->data->rate;
    return MPXP_Ok;
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
  // Do something necessary to get rid of annoying warning during compile
  if(!af)
    MSG_ERR("EEEK: Argument af == NULL in af_lp.c play().");
  return data;
}

// Allocate memory and set function pointers
static MPXP_Rc __FASTCALL__ open(af_instance_t* af){
  af->control=control;
  af->uninit=uninit;
  af->play=play;
  af->mul.d=1;
  af->mul.n=1;
  af->data=mp_malloc(sizeof(mp_aframe_t));
  af->setup=mp_malloc(sizeof(af_lp_t));
  if(af->data == NULL)
    return MPXP_Error;
  return MPXP_Ok;
}

// Description of this filter
const af_info_t af_info_lp = {
    "LongPlay audio filer",
    "lp",
    "Nickols_K",
    "",
    AF_FLAGS_REENTRANT,
    open
};
