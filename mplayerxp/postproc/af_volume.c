/*=============================================================================
//
//  This software has been released under the terms of the GNU General Public
//  license. See http://www.gnu.org/copyleft/gpl.html for details.
//
//  Copyright 2002 Anders Johansson ajh@atri.curtin.edu.au
//
//=============================================================================
*/

/* This audio filter changes the volume of the sound, and can be used
   when the mixer doesn't support the PCM channel. It can handle
   between 1 and 6 channels. The volume can be adjusted between -60dB
   to +20dB and is set on a per channels basis. The is accessed through
   AF_CONTROL_VOLUME_LEVEL.

   The filter has support for soft-clipping, it is enabled by
   AF_CONTROL_VOLUME_SOFTCLIPP. It has also a probing feature which
   can be used to measure the power in the audio stream, both an
   instantaneous value and the maximum value can be probed. The
   probing is enable by AF_CONTROL_VOLUME_PROBE_ON_OFF and is done on a
   per channel basis. The result from the probing is obtained using
   AF_CONTROL_VOLUME_PROBE_GET and AF_CONTROL_VOLUME_PROBE_GET_MAX. The
   probed values are calculated in dB.
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

// Data for specific instances of this filter
typedef struct af_volume_s
{
  int   enable[AF_NCH];		// Enable/disable / channel
  float	pow[AF_NCH];		// Estimated power level [dB]
  float	max[AF_NCH];		// Max Power level [dB]
  float level[AF_NCH];		// Gain level for each channel
  float time;			// Forgetting factor for power estimate
  int soft;			// Enable/disable soft clipping
  int fast;			// Use fix-point volume control
}af_volume_t;

// Initialization and runtime control
static MPXP_Rc __FASTCALL__ control(struct af_instance_s* af, int cmd, any_t* arg)
{
  af_volume_t* s   = (af_volume_t*)af->setup;

  switch(cmd){
  case AF_CONTROL_REINIT:
    // Sanity check
    if(!arg) return MPXP_Error;

    af->data->rate   = ((mp_aframe_t*)arg)->rate;
    af->data->nch    = ((mp_aframe_t*)arg)->nch;

    if(s->fast && !mpaf_testa(((mp_aframe_t*)arg)->format,MPAF_F|MPAF_NE))
	af->data->format = MPAF_SI|MPAF_NE|2;
    else {
      // Cutoff set to 10Hz for forgetting factor
      float x = 2.0*M_PI*15.0/(float)af->data->rate;
      float t = 2.0-cos(x);
      s->time = 1.0 - (t - sqrt(t*t - 1));
      MSG_DBG2("[volume] Forgetting factor = %0.5f\n",s->time);
      af->data->format = MPAF_F|MPAF_NE|4;
    }
    return af_test_output(af,(mp_aframe_t*)arg);
  case AF_CONTROL_SHOWCONF:
    MSG_INFO("[af_volume] using soft %i\n",s->soft);
    return MPXP_Ok;
  case AF_CONTROL_COMMAND_LINE:{
    float v=-10.0;
    float vol[AF_NCH];
    int   i;
    sscanf((char*)arg,"%f:%i", &v, &s->soft);
    for(i=0;i<AF_NCH;i++) vol[i]=v;
    return control(af,AF_CONTROL_VOLUME_LEVEL | AF_CONTROL_SET, vol);
  }
  case AF_CONTROL_POST_CREATE:
    s->fast = ((((af_cfg_t*)arg)->force & AF_INIT_FORMAT_MASK) ==
      AF_INIT_FLOAT) ? 0 : 1;
    return MPXP_Ok;
  case AF_CONTROL_VOLUME_ON_OFF | AF_CONTROL_SET:
    memcpy(s->enable,(int*)arg,AF_NCH*sizeof(int));
    return MPXP_Ok;
  case AF_CONTROL_VOLUME_ON_OFF | AF_CONTROL_GET:
    memcpy((int*)arg,s->enable,AF_NCH*sizeof(int));
    return MPXP_Ok;
  case AF_CONTROL_VOLUME_SOFTCLIP | AF_CONTROL_SET:
    s->soft = *(int*)arg;
    return MPXP_Ok;
  case AF_CONTROL_VOLUME_SOFTCLIP | AF_CONTROL_GET:
    *(int*)arg = s->soft;
    return MPXP_Ok;
  case AF_CONTROL_VOLUME_LEVEL | AF_CONTROL_SET:
    return af_from_dB(AF_NCH,(float*)arg,s->level,20.0,-200.0,60.0);
  case AF_CONTROL_VOLUME_LEVEL | AF_CONTROL_GET:
    return af_to_dB(AF_NCH,s->level,(float*)arg,20.0);
  case AF_CONTROL_VOLUME_PROBE | AF_CONTROL_GET:
    return af_to_dB(AF_NCH,s->pow,(float*)arg,10.0);
  case AF_CONTROL_VOLUME_PROBE_MAX | AF_CONTROL_GET:
    return af_to_dB(AF_NCH,s->max,(float*)arg,10.0);
  case AF_CONTROL_PRE_DESTROY:{
    float m = 0.0;
    int i;
    if(!s->fast){
      for(i=0;i<AF_NCH;i++)
	m=max(m,s->max[i]);
	af_to_dB(1, &m, &m, 10.0);
	MSG_INFO("[volume] The maximum volume was %0.2fdB \n", m);
    }
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
  mp_aframe_t*    c   = data;			// Current working data
  af_volume_t*  s   = (af_volume_t*)af->setup; 	// Setup for this instance
  int           ch  = 0;			// Channel counter
  register int	nch = c->nch;			// Number of channels
  register int  i   = 0;

  // Basic operation volume control only (used on slow machines)
  if(af->data->format == (MPAF_SI | MPAF_NE)){
    int16_t*    a   = (int16_t*)c->audio;	// Audio data
    int         len = c->len/2;			// Number of samples
    for(ch = 0; ch < nch ; ch++){
      if(s->enable[ch]){
	register int vol = (int)(255.0 * s->level[ch]);
	for(i=ch;i<len;i+=nch){
	  register int x = (a[i] * vol) >> 8;
	  a[i]=clamp(x,SHRT_MIN,SHRT_MAX);
	}
      }
    }
  }
  // Machine is fast and data is floating point
  else if(af->data->format == (MPAF_F | MPAF_NE)){
    float*   	a   	= (float*)c->audio;	// Audio data
    int       	len 	= c->len/4;		// Number of samples
    for(ch = 0; ch < nch ; ch++){
      // Volume control (fader)
      if(s->enable[ch]){
	float	t   = 1.0 - s->time;
	for(i=ch;i<len;i+=nch){
	  register float x 	= a[i];
	  register float _pow 	= x*x;
	  // Check maximum power value
	  if(_pow > s->max[ch])
	    s->max[ch] = _pow;
	  // Set volume
	  x *= s->level[ch];
	  // Peak meter
	  _pow 	= x*x;
	  if(_pow > s->pow[ch])
	    s->pow[ch] = _pow;
	  else
	    s->pow[ch] = t*s->pow[ch] + _pow*s->time; // LP filter
	  /* Soft clipping, the sound of a dream, thanks to Jon Wattes
	     post to Musicdsp.org */
	  if(s->soft){
	    if (x >=  M_PI/2)
	      x = 1.0;
	    else if(x <= -M_PI/2)
	      x = -1.0;
	    else
	      x = sin(x);
	  }
	  // Hard clipping
	  else
	    x=clamp(x,-1.0,1.0);
	  a[i] = x;
	}
      }
    }
  }
  return c;
}

// Allocate memory and set function pointers
static MPXP_Rc __FASTCALL__ af_open(af_instance_t* af){
  int i = 0;
  af->control=control;
  af->uninit=uninit;
  af->play=play;
  af->mul.n=1;
  af->mul.d=1;
  af->data=mp_calloc(1,sizeof(mp_aframe_t));
  af->setup=mp_calloc(1,sizeof(af_volume_t));
  if(af->data == NULL || af->setup == NULL)
    return MPXP_Error;
  // Enable volume control and set initial volume to 0dB.
  for(i=0;i<AF_NCH;i++){
    ((af_volume_t*)af->setup)->enable[i] = 1;
    ((af_volume_t*)af->setup)->level[i]  = 1.0;
  }
    check_pin("afilter",af->pin,AF_PIN);
  return MPXP_Ok;
}

// Description of this filter
const af_info_t af_info_volume = {
    "Volume control audio filter",
    "volume",
    "Anders",
    "",
    AF_FLAGS_NOT_REENTRANT,
    af_open
};
