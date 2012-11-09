/* Audio filter that adds and removes channels, according to the
   command line parameter channels. It is stupid and can only add
   silence or copy channels not mix or filter.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>

#include "af.h"
#include "osdep/mplib.h"
#include "pp_msg.h"

#define FR 0
#define TO 1

typedef struct af_channels_s{
  int route[AF_NCH][2];
  int nr;
  int router;
  int ich;
}af_channels_t;

// Local function for copying data
static void __FASTCALL__ copy(any_t* in, any_t* out, int ins, int inos,int outs, int outos, int len, int bps)
{
  switch(bps){
  case 1:{
    int8_t* tin  = (int8_t*)in;
    int8_t* tout = (int8_t*)out;
    tin  += inos;
    tout += outos;
    len = len/ins;
    while(len--){
      *tout=*tin;
      tin +=ins;
      tout+=outs;
    }
    break;
  }
  case 2:{
    int16_t* tin  = (int16_t*)in;
    int16_t* tout = (int16_t*)out;
    tin  += inos;
    tout += outos;
    len = len/(2*ins);
    while(len--){
      *tout=*tin;
      tin +=ins;
      tout+=outs;
    }
    break;
  }
  case 3:{
    int8_t* tin  = (int8_t*)in;
    int8_t* tout = (int8_t*)out;
    tin  += 3 * inos;
    tout += 3 * outos;
    len = len / ( 3 * ins);
    while (len--) {
      tout[0] = tin[0];
      tout[1] = tin[1];
      tout[2] = tin[2];
      tin += 3 * ins;
      tout += 3 * outs;
    }
    break;
  }
  case 4:{
    int32_t* tin  = (int32_t*)in;
    int32_t* tout = (int32_t*)out;
    tin  += inos;
    tout += outos;
    len = len/(4*ins);
    while(len--){
      *tout=*tin;
      tin +=ins;
      tout+=outs;
    }
    break;
  }
  case 8:{
    int64_t* tin  = (int64_t*)in;
    int64_t* tout = (int64_t*)out;
    tin  += inos;
    tout += outos;
    len = len/(8*ins);
    while(len--){
      *tout=*tin;
      tin +=ins;
      tout+=outs;
    }
    break;
  }
  default:
    MSG_ERR("[channels] Unsupported number of bytes/sample: %i" 
	   " please report this error on the MPlayer mailing list. \n",bps);
  }
}

// Make sure the routes are sane
static int __FASTCALL__ check_routes(af_channels_t* s, int nin, int nout)
{
  int i;
  if((s->nr < 1) || (s->nr > AF_NCH)){
    MSG_ERR("[channels] The number of routing pairs must be" 
	   " between 1 and %i. Current value is %i\n",AF_NCH,s->nr);
    return MPXP_Error;
  }
	
  for(i=0;i<s->nr;i++){
    if((s->route[i][FR] >= nin) || (s->route[i][TO] >= nout)){
      MSG_ERR("[channels] Invalid routing in pair nr. %i.\n", i);
      return MPXP_Error;
    }
  }
  return MPXP_Ok;
}

// Initialization and runtime control
static MPXP_Rc __FASTCALL__ control(struct af_instance_s* af, int cmd, any_t* arg)
{
  af_channels_t* s = af->setup;
  switch(cmd){
  case AF_CONTROL_REINIT:

    // Set default channel assignment
    if(!s->router){
      int i;
      // Make sure this filter isn't redundant 
      if(af->data->nch == ((mp_aframe_t*)arg)->nch)
	return MPXP_Detach;

      // If mono: fake stereo
      if(((mp_aframe_t*)arg)->nch == 1){
	s->nr = min(af->data->nch,2);
	for(i=0;i<s->nr;i++){
	  s->route[i][FR] = 0;
	  s->route[i][TO] = i;
	}
      }
      else{
	s->nr = min(af->data->nch, ((mp_aframe_t*)arg)->nch);
	for(i=0;i<s->nr;i++){
	  s->route[i][FR] = i;
	  s->route[i][TO] = i;
	}
      }
    }
    s->ich=((mp_aframe_t*)arg)->nch;
    af->data->rate	= ((mp_aframe_t*)arg)->rate;
    af->data->format	= ((mp_aframe_t*)arg)->format;
    af->mul.n		= af->data->nch;
    af->mul.d		= ((mp_aframe_t*)arg)->nch;
    return check_routes(s,((mp_aframe_t*)arg)->nch,af->data->nch);
  case AF_CONTROL_SHOWCONF:
    MSG_INFO("[af_channels] Changing channels %d -> %d\n",s->ich,af->data->nch);
    return MPXP_Ok;
  case AF_CONTROL_COMMAND_LINE:{
    int nch = 0;
    int n = 0;
    // Check number of channels and number of routing pairs
    sscanf(arg, "%i:%i%n", &nch, &s->nr, &n);

    // If router scan commandline for routing pairs
    if(s->nr){
      char* cp = &((char*)arg)[n];
      int ch = 0;
      // Sanity check
      if((s->nr < 1) || (s->nr > AF_NCH)){
	MSG_ERR("[channels] The number of routing pairs must be" 
	     " between 1 and %i. Current value is %i\n",AF_NCH,s->nr);
      }	
      s->router = 1;
      // Scan for pairs on commandline
      while((*cp == ':') && (ch < s->nr)){
	sscanf(cp, ":%i:%i%n" ,&s->route[ch][FR], &s->route[ch][TO], &n);
	MSG_V("[channels] Routing from channel %i to" 
	       " channel %i\n",s->route[ch][FR],s->route[ch][TO]);
	cp = &cp[n];
	ch++;
      }
    }

    if(MPXP_Ok != af->control(af,AF_CONTROL_CHANNELS | AF_CONTROL_SET ,&nch))
      return MPXP_Error;
    return MPXP_Ok;
  }    
  case AF_CONTROL_CHANNELS | AF_CONTROL_SET: 
    // Reinit must be called after this function has been called
    
    // Sanity check
    if(((int*)arg)[0] <= 0 || ((int*)arg)[0] > AF_NCH){
      MSG_ERR("[channels] The number of output channels must be" 
	     " between 1 and %i. Current value is %i\n",AF_NCH,((int*)arg)[0]);
      return MPXP_Error;
    }

    af->data->nch=((int*)arg)[0]; 
    if(!s->router)
      MSG_V("[channels] Changing number of channels" 
	     " to %i\n",af->data->nch);
    return MPXP_Ok;
  case AF_CONTROL_CHANNELS | AF_CONTROL_GET:
    *(int*)arg = af->data->nch;
    return MPXP_Ok;
  case AF_CONTROL_CHANNELS_ROUTING | AF_CONTROL_SET:{
    int ch = ((af_control_ext_t*)arg)->ch;
    int* route = ((af_control_ext_t*)arg)->arg;
    s->route[ch][FR] = route[FR];
    s->route[ch][TO] = route[TO];
    return MPXP_Ok;
  }
  case AF_CONTROL_CHANNELS_ROUTING | AF_CONTROL_GET:{
    int ch = ((af_control_ext_t*)arg)->ch;
    int* route = ((af_control_ext_t*)arg)->arg;
    route[FR] = s->route[ch][FR];
    route[TO] = s->route[ch][TO];
    return MPXP_Ok;
  }
  case AF_CONTROL_CHANNELS_NR | AF_CONTROL_SET:
    s->nr = *(int*)arg;
    return MPXP_Ok;
  case AF_CONTROL_CHANNELS_NR | AF_CONTROL_GET:
    *(int*)arg = s->nr;
    return MPXP_Ok;
  case AF_CONTROL_CHANNELS_ROUTER | AF_CONTROL_SET:
    s->router = *(int*)arg;
    return MPXP_Ok;
  case AF_CONTROL_CHANNELS_ROUTER | AF_CONTROL_GET:
    *(int*)arg = s->router;
    return MPXP_Ok;
  }
  return MPXP_Unknown;
}

// Deallocate memory 
static void __FASTCALL__ uninit(struct af_instance_s* af)
{
  if(af->setup)
    mp_free(af->setup);
  if(af->data)
    mp_free(af->data);
}

// Filter data through filter
static mp_aframe_t* __FASTCALL__ play(struct af_instance_s* af, mp_aframe_t* data,int final)
{
  mp_aframe_t*   	 c = data;			// Current working data
  af_channels_t* s = af->setup;
  int 		 i;

  if(MPXP_Ok != RESIZE_LOCAL_BUFFER(af,data))
    return NULL;
  mp_aframe_t*   	 l = af->data;	 		// Local data

  // Reset unused channels
  memset(l->audio,0,(c->len*af->mul.n)/af->mul.d);

  if(MPXP_Ok == check_routes(s,c->nch,l->nch))
    for(i=0;i<s->nr;i++)
      copy(c->audio,l->audio,c->nch,s->route[i][FR],
	   l->nch,s->route[i][TO],c->len,c->format&MPAF_BPS_MASK);

  // Set output data
  c->audio = l->audio;
  c->len   = (c->len*af->mul.n)/af->mul.d;
  c->nch   = l->nch;

  return c;
}

// Allocate memory and set function pointers
static MPXP_Rc __FASTCALL__ open(af_instance_t* af){
  af->control=control;
  af->uninit=uninit;
  af->play=play;
  af->mul.n=1;
  af->mul.d=1;
  af->data=mp_calloc(1,sizeof(mp_aframe_t));
  af->setup=mp_calloc(1,sizeof(af_channels_t));
  if((af->data == NULL) || (af->setup == NULL))
    return MPXP_Error;
  return MPXP_Ok;
}

// Description of this filter
const af_info_t af_info_channels = {
  "Insert or remove channels",
  "channels",
  "Anders",
  "",
  AF_FLAGS_REENTRANT,
  open
};
