#include <math.h>
#include <string.h>
#include <af.h>
#include "osdep/mplib.h"

using namespace mpxp;

/* Convert to gain value from dB. Returns MPXP_Ok if of and MPXP_Error if
   fail */
MPXP_Rc __FASTCALL__ af_from_dB(int n, float* in, float* out, float k, float mi, float ma)
{
  int i = 0;
  // Sanity check
  if(!in || !out)
    return MPXP_Error;

  for(i=0;i<n;i++){
    if(in[i]<=-200)
      out[i]=0.0;
    else
      out[i]=pow(10.0,clamp(in[i],mi,ma)/k);
  }
  return MPXP_Ok;
}

/* Convert from gain value to dB. Returns MPXP_Ok if of and MPXP_Error if
   fail */
MPXP_Rc __FASTCALL__ af_to_dB(int n, float* in, float* out, float k)
{
  int i = 0;
  // Sanity check
  if(!in || !out)
    return MPXP_Error;

  for(i=0;i<n;i++){
    if(in[i] == 0.0)
      out[i]=-200.0;
    else
      out[i]=k*log10(in[i]);
  }
  return MPXP_Ok;
}

/* Convert from ms to sample time */
MPXP_Rc __FASTCALL__ af_from_ms(int n, float* in, int* out, int rate, float mi, float ma)
{
  int i = 0;
  // Sanity check
  if(!in || !out)
    return MPXP_Error;

  for(i=0;i<n;i++)
    out[i]=(int)((float)rate * clamp(in[i],mi,ma)/1000.0);

  return MPXP_Ok;
}

/* Convert from sample time to ms */
MPXP_Rc __FASTCALL__ af_to_ms(int n, int* in, float* out, int rate)
{
  int i = 0;
  // Sanity check
  if(!in || !out || !rate)
    return MPXP_Error;

  for(i=0;i<n;i++)
    out[i]=1000.0 * (float)in[i]/((float)rate);

  return MPXP_Ok;
}

/* Helper function for testing the output format */
MPXP_Rc __FASTCALL__ af_test_output(struct af_instance_s* af,const af_conf_t* out)
{
  if((af->conf.format != out->format) ||
     (af->conf.rate   != out->rate)   ||
     (af->conf.nch    != out->nch)){
#if 0
    MSG_DBG2("af_test_out %i!=%i || %i!=%i || %i!=%i\n",
    af->conf.format,out->format,
    af->conf.rate,out->rate,
    af->conf.nch,out->nch);
#endif
    return MPXP_False;
  }
  return MPXP_Ok;
}
