/* The name speaks for itself this filter is a dummy and will not blow
   up regardless of what you do with it. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "af.h"
#include "mp_config.h"
#include "osdep/mplib.h"
#include "pp_msg.h"

static MPXP_Rc __FASTCALL__ config(struct af_instance_s* af,const af_conf_t* arg)
{
    memcpy(&af->conf,arg,sizeof(af_conf_t));
    MSG_V("[dummy] Was reinitialized, rate=%iHz, nch = %i, format = 0x%08X\n"
	,af->conf.rate,af->conf.nch,af->conf.format);
    return MPXP_Ok;
}
// Initialization and runtime control
static MPXP_Rc __FASTCALL__ control(struct af_instance_s* af, int cmd, any_t* arg)
{
    return MPXP_Unknown;
}

// Deallocate memory
static void __FASTCALL__ uninit(struct af_instance_s* af)
{
}

// Filter data through filter
static mp_aframe_t* __FASTCALL__ play(struct af_instance_s* af,const mp_aframe_t* data)
{
    // Do something necessary to get rid of annoying warning during compile
    if(!af)
	MSG_ERR("EEEK: Argument af == NULL in af_dummy.c play().");
    return data;
}

// Allocate memory and set function pointers
static MPXP_Rc __FASTCALL__ af_open(af_instance_t* af){
  af->config = config;
  af->control=control;
  af->uninit=uninit;
  af->play=play;
  af->mul.d=1;
  af->mul.n=1;
  check_pin("afilter",af->pin,AF_PIN);
  return MPXP_Ok;
}

// Description of this filter
const af_info_t af_info_dummy = {
    "dummy",
    "dummy",
    "Anders",
    "",
    AF_FLAGS_REENTRANT,
    af_open
};
