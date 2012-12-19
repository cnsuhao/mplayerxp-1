#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
/* This audio filter delays the output signal for the different
   channels and can be used for simple position panning. Extension for
   this filter would be a reverb.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "af.h"
#include "af_internal.h"
#include "mpxp_help.h"
#include "pp_msg.h"

#define L 65536

#define UPDATEQI(qi) qi=(qi+1)&(L-1)

// Data for specific instances of this filter
struct af_delay_t
{
  any_t* q[AF_NCH];   	// Circular queues used for delaying audio signal
  int 	wi[AF_NCH];  	// Write index
  int 	ri;		// Read index
  float	d[AF_NCH];   	// Delay [ms]
};

// Initialization and runtime control_af
static MPXP_Rc __FASTCALL__ control_af(af_instance_t* af, int cmd, any_t* arg)
{
  af_delay_t* s = reinterpret_cast<af_delay_t*>(af->setup);

  switch(cmd){
  case AF_CONTROL_COMMAND_LINE:{
    int n = 1;
    int i = 0;
    char* cl = reinterpret_cast<char*>(arg);
    while(n && i < AF_NCH ){
      sscanf(cl,"%f:%n",&s->d[i],&n);
      if(n==0 || cl[n-1] == '\0')
	break;
      cl=&cl[n];
      i++;
    }
    return MPXP_Ok;
  }
  case AF_CONTROL_DELAY_LEN | AF_CONTROL_SET:{
    int i;
    if(MPXP_Ok != af_from_ms(AF_NCH, reinterpret_cast<float*>(arg), s->wi, af->conf.rate, 0.0, 1000.0))
      return MPXP_Error;
    s->ri = 0;
    for(i=0;i<AF_NCH;i++){
      MSG_DBG2("[delay] Channel %i delayed by %0.3fms\n",
	     i,clamp(s->d[i],0.0f,1000.0f));
      MSG_DBG2("[delay] Channel %i delayed by %i samples\n",
	     i,s->wi[i]);
    }
    return MPXP_Ok;
  }
  case AF_CONTROL_DELAY_LEN | AF_CONTROL_GET:{
    int i;
    for(i=0;i<AF_NCH;i++){
      if(s->ri > s->wi[i])
	s->wi[i] = L - (s->ri - s->wi[i]);
      else
	s->wi[i] = s->wi[i] - s->ri;
    }
    return af_to_ms(AF_NCH, s->wi, reinterpret_cast<float*>(arg), af->conf.rate);
  }
  default: break;
  }
  return MPXP_Unknown;
}

static MPXP_Rc __FASTCALL__ af_config(af_instance_t* af,const af_conf_t* arg)
{
    af_delay_t* s = reinterpret_cast<af_delay_t*>(af->setup);
    unsigned i;

    // Free prevous delay queues
    for(i=0;i<af->conf.nch;i++) if(s->q[i]) delete s->q[i];

    af->conf.rate   = arg->rate;
    af->conf.nch    = arg->nch;
    af->conf.format = arg->format;

    // Allocate new delay queues
    for(i=0;i<af->conf.nch;i++){
	s->q[i] = mp_calloc(L,af->conf.format&MPAF_BPS_MASK);
	if(NULL == s->q[i]) MSG_FATAL(MSGTR_OutOfMemory);
    }
    return control_af(af,AF_CONTROL_DELAY_LEN | AF_CONTROL_SET,s->d);
}

// Deallocate memory
static void __FASTCALL__ uninit(af_instance_t* af)
{
  int i;
  for(i=0;i<AF_NCH;i++)
    if(((af_delay_t*)(af->setup))->q[i])
      delete ((af_delay_t*)(af->setup))->q[i];
  if(af->setup)
    delete af->setup;
}

// Filter data through filter
static mp_aframe_t* __FASTCALL__ play(af_instance_t* af,const mp_aframe_t* in)
{
    mp_aframe_t* c=new_mp_aframe_genome(in);
    mp_alloc_aframe(c);
    memcpy(c->audio,in->audio,c->len);

    af_delay_t*	s   = reinterpret_cast<af_delay_t*>(af->setup); // Setup for this instance
    int 	nch = c->nch;	 // Number of channels
    int		len = c->len/(c->format&MPAF_BPS_MASK); // Number of sample in data chunk
    int		ri  = 0;
    int 	ch,i;

    for(ch=0;ch<nch;ch++){
	unsigned bps=c->format&MPAF_BPS_MASK;
	switch(bps){
	case 1:{
	    int8_t* a = reinterpret_cast<int8_t*>(c->audio);
	    int8_t* q = reinterpret_cast<int8_t*>(s->q[ch]);
	    int wi = s->wi[ch];
	    ri = s->ri;
	    for(i=ch;i<len;i+=nch){
		q[wi] = a[i];
		a[i]  = q[ri];
		UPDATEQI(wi);
		UPDATEQI(ri);
	    }
	    s->wi[ch] = wi;
	    break;
	}
	case 2:{
	    int16_t* a = reinterpret_cast<int16_t*>(c->audio);
	    int16_t* q = reinterpret_cast<int16_t*>(s->q[ch]);
	    int wi = s->wi[ch];
	    ri = s->ri;
	    for(i=ch;i<len;i+=nch){
		q[wi] = a[i];
		a[i]  = q[ri];
		UPDATEQI(wi);
		UPDATEQI(ri);
	    }
	    s->wi[ch] = wi;
	    break;
	}
	case 4:{
	    int32_t* a = reinterpret_cast<int32_t*>(c->audio);
	    int32_t* q = reinterpret_cast<int32_t*>(s->q[ch]);
	    int wi = s->wi[ch];
	    ri = s->ri;
	    for(i=ch;i<len;i+=nch){
		q[wi] = a[i];
		a[i]  = q[ri];
		UPDATEQI(wi);
		UPDATEQI(ri);
	    }
	    s->wi[ch] = wi;
	    break;
	}
    }
    }
    s->ri = ri;
    return c;
}

// Allocate memory and set function pointers
static MPXP_Rc __FASTCALL__ af_open(af_instance_t* af){
    af->config_af=af_config;
    af->control_af=control_af;
    af->uninit=uninit;
    af->play=play;
    af->mul.n=1;
    af->mul.d=1;
    af->setup=mp_calloc(1,sizeof(af_delay_t));
    if(af->setup == NULL) return MPXP_Error;
    check_pin("afilter",af->pin,AF_PIN);
    return MPXP_Ok;
}

// Description of this filter
extern const af_info_t af_info_delay = {
    "Delay audio filter",
    "delay",
    "Anders",
    "",
    AF_FLAGS_REENTRANT,
    af_open
};


