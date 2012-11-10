/*=============================================================================
//
//  This software has been released under the terms of the GNU General Public
//  license. See http://www.gnu.org/copyleft/gpl.html for details.
//
//  Copyright 2004 Alex Beregszaszi & Pierre Lombard
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
#include "pp_msg.h"

// Methods:
// 1: uses a 1 value memory and coefficients new=a*old+b*cur (with a+b=1)
// 2: uses several samples to smooth the variations (standard weighted mean
//    on past samples)

// Size of the memory array
// FIXME: should depend on the frequency of the data (should be a few seconds)
#define NSAMPLES 128

// If summing all the mem[].len is lower than MIN_SAMPLE_SIZE bytes, then we
// choose to ignore the computed value as it's not significant enough
// FIXME: should depend on the frequency of the data (0.5s maybe)
#define MIN_SAMPLE_SIZE 32000

// mul is the value by which the samples are scaled
// and has to be in [MUL_MIN, MUL_MAX]
#define MUL_INIT 1.0
#define MUL_MIN 0.1
#define MUL_MAX 5.0
// "Ideal" level
#define MID_S16 (SHRT_MAX * 0.25)
#define MID_FLOAT (INT_MAX * 0.25)

// Silence level
// FIXME: should be relative to the level of the samples
#define SIL_S16 (SHRT_MAX * 0.01)
#define SIL_FLOAT (INT_MAX * 0.01) // FIXME

// smooth must be in ]0.0, 1.0[
#define SMOOTH_MUL 0.06
#define SMOOTH_LASTAVG 0.06

// Data for specific instances of this filter
typedef struct af_volume_s
{
    int method; // method used
    float mul;
    // method 1
    float lastavg; // history value of the filter
    // method 2
    int idx;
    struct {
	float avg; // average level of the sample
	int len; // sample size (weight)
    } mem[NSAMPLES];
}af_volnorm_t;

// Initialization and runtime control
static MPXP_Rc __FASTCALL__ control(struct af_instance_s* af, int cmd, any_t* arg)
{
  af_volnorm_t* s   = (af_volnorm_t*)af->setup;

  switch(cmd){
  case AF_CONTROL_REINIT:
    // Sanity check
    if(!arg) return MPXP_Error;

    af->data->rate   = ((mp_aframe_t*)arg)->rate;
    af->data->nch    = ((mp_aframe_t*)arg)->nch;

    if(!(mpaf_testa(((mp_aframe_t*)arg)->format,MPAF_F|MPAF_NE) ||
	mpaf_testa(((mp_aframe_t*)arg)->format,MPAF_SI|MPAF_NE)))
	return MPXP_Error;

    if(mpaf_testa(((mp_aframe_t*)arg)->format,MPAF_F|MPAF_NE))
	af->data->format = MPAF_F|MPAF_NE|4;
    else
	af->data->format = MPAF_SI|MPAF_NE|2;
    return af_test_output(af,(mp_aframe_t*)arg);
  case AF_CONTROL_SHOWCONF:
    MSG_INFO("[af_volnorm] using method %d\n",s->method);
    return MPXP_Ok;
  case AF_CONTROL_COMMAND_LINE:{
    int   i;
    sscanf((char*)arg,"%d", &i);
    if (i != 1 && i != 2)
	return MPXP_Error;
    s->method = i-1;
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

static void __FASTCALL__ method1_int16(af_volnorm_t *s, mp_aframe_t *c,int final)
{
  register int i = 0;
  int16_t *data = (int16_t*)c->audio;	// Audio data
  int len = c->len/2;		// Number of samples
  float curavg = 0.0, newavg, neededmul;
  int tmp;

  for (i = 0; i < len; i++)
  {
    tmp = data[i];
    curavg += tmp * tmp;
  }
  curavg = sqrt(curavg / (float) len);

  // Evaluate an adequate 'mul' coefficient based on previous state, current
  // samples level, etc

  if (curavg > SIL_S16)
  {
    neededmul = MID_S16 / (curavg * s->mul);
    s->mul = (1.0 - SMOOTH_MUL) * s->mul + SMOOTH_MUL * neededmul;

    // clamp the mul coefficient
    s->mul = clamp(s->mul, MUL_MIN, MUL_MAX);
  }

  // Scale & clamp the samples
  for (i = 0; i < len; i++)
  {
    tmp = s->mul * data[i];
    tmp = clamp(tmp, SHRT_MIN, SHRT_MAX);
    data[i] = tmp;
  }

  // Evaulation of newavg (not 100% accurate because of values clamping)
  newavg = s->mul * curavg;

  // Stores computed values for future smoothing
  s->lastavg = (1.0 - SMOOTH_LASTAVG) * s->lastavg + SMOOTH_LASTAVG * newavg;
}

static void __FASTCALL__ method1_float(af_volnorm_t *s, mp_aframe_t *c,int final)
{
  register int i = 0;
  float *data = (float*)c->audio;	// Audio data
  int len = c->len/4;		// Number of samples
  float curavg = 0.0, newavg, neededmul, tmp;

  for (i = 0; i < len; i++)
  {
    tmp = data[i];
    curavg += tmp * tmp;
  }
  curavg = sqrt(curavg / (float) len);

  // Evaluate an adequate 'mul' coefficient based on previous state, current
  // samples level, etc

  if (curavg > SIL_FLOAT) // FIXME
  {
    neededmul = MID_FLOAT / (curavg * s->mul);
    s->mul = (1.0 - SMOOTH_MUL) * s->mul + SMOOTH_MUL * neededmul;

    // clamp the mul coefficient
    s->mul = clamp(s->mul, MUL_MIN, MUL_MAX);
  }

  // Scale & clamp the samples
  for (i = 0; i < len; i++)
    data[i] *= s->mul;

  // Evaulation of newavg (not 100% accurate because of values clamping)
  newavg = s->mul * curavg;

  // Stores computed values for future smoothing
  s->lastavg = (1.0 - SMOOTH_LASTAVG) * s->lastavg + SMOOTH_LASTAVG * newavg;
}

static void __FASTCALL__ method2_int16(af_volnorm_t *s, mp_aframe_t *c,int final)
{
  register int i = 0;
  int16_t *data = (int16_t*)c->audio;	// Audio data
  int len = c->len/2;		// Number of samples
  float curavg = 0.0, newavg, avg = 0.0;
  int tmp, totallen = 0;

  for (i = 0; i < len; i++)
  {
    tmp = data[i];
    curavg += tmp * tmp;
  }
  curavg = sqrt(curavg / (float) len);

  // Evaluate an adequate 'mul' coefficient based on previous state, current
  // samples level, etc
  for (i = 0; i < NSAMPLES; i++)
  {
    avg += s->mem[i].avg * (float)s->mem[i].len;
    totallen += s->mem[i].len;
  }

  if (totallen > MIN_SAMPLE_SIZE)
  {
    avg /= (float)totallen;
    if (avg >= SIL_S16)
    {
	s->mul = MID_S16 / avg;
	s->mul = clamp(s->mul, MUL_MIN, MUL_MAX);
    }
  }

  // Scale & clamp the samples
  for (i = 0; i < len; i++)
  {
    tmp = s->mul * data[i];
    tmp = clamp(tmp, SHRT_MIN, SHRT_MAX);
    data[i] = tmp;
  }

  // Evaulation of newavg (not 100% accurate because of values clamping)
  newavg = s->mul * curavg;

  // Stores computed values for future smoothing
  s->mem[s->idx].len = len;
  s->mem[s->idx].avg = newavg;
  s->idx = (s->idx + 1) % NSAMPLES;
}

static void __FASTCALL__ method2_float(af_volnorm_t *s, mp_aframe_t *c,int final)
{
  register int i = 0;
  float *data = (float*)c->audio;	// Audio data
  int len = c->len/4;		// Number of samples
  float curavg = 0.0, newavg, avg = 0.0, tmp;
  int totallen = 0;

  for (i = 0; i < len; i++)
  {
    tmp = data[i];
    curavg += tmp * tmp;
  }
  curavg = sqrt(curavg / (float) len);

  // Evaluate an adequate 'mul' coefficient based on previous state, current
  // samples level, etc
  for (i = 0; i < NSAMPLES; i++)
  {
    avg += s->mem[i].avg * (float)s->mem[i].len;
    totallen += s->mem[i].len;
  }

  if (totallen > MIN_SAMPLE_SIZE)
  {
    avg /= (float)totallen;
    if (avg >= SIL_FLOAT)
    {
	s->mul = MID_FLOAT / avg;
	s->mul = clamp(s->mul, MUL_MIN, MUL_MAX);
    }
  }

  // Scale & clamp the samples
  for (i = 0; i < len; i++)
    data[i] *= s->mul;

  // Evaulation of newavg (not 100% accurate because of values clamping)
  newavg = s->mul * curavg;

  // Stores computed values for future smoothing
  s->mem[s->idx].len = len;
  s->mem[s->idx].avg = newavg;
  s->idx = (s->idx + 1) % NSAMPLES;
}

// Filter data through filter
static mp_aframe_t* __FASTCALL__ play(struct af_instance_s* af, mp_aframe_t* data,int final)
{
  af_volnorm_t *s = af->setup;

  if(af->data->format == (MPAF_SI | MPAF_NE))
  {
    if (s->method)
	method2_int16(s, data,final);
    else
	method1_int16(s, data,final);
  }
  else if(af->data->format == (MPAF_F | MPAF_NE))
  {
    if (s->method)
	method2_float(s, data,final);
    else
	method1_float(s, data,final);
  }
  return data;
}

// Allocate memory and set function pointers
static MPXP_Rc __FASTCALL__ open(af_instance_t* af){
  int i = 0;
  af->control=control;
  af->uninit=uninit;
  af->play=play;
  af->mul.n=1;
  af->mul.d=1;
  af->data=mp_calloc(1,sizeof(mp_aframe_t));
  af->setup=mp_calloc(1,sizeof(af_volnorm_t));
  if(af->data == NULL || af->setup == NULL)
    return MPXP_Error;

  ((af_volnorm_t*)af->setup)->mul = MUL_INIT;
  ((af_volnorm_t*)af->setup)->lastavg = MID_S16;
  ((af_volnorm_t*)af->setup)->idx = 0;
  for (i = 0; i < NSAMPLES; i++)
  {
     ((af_volnorm_t*)af->setup)->mem[i].len = 0;
     ((af_volnorm_t*)af->setup)->mem[i].avg = 0;
  }
  return MPXP_Ok;
}

// Description of this filter
const af_info_t af_info_volnorm = {
    "Volume normalizer filter",
    "volnorm",
    "Alex Beregszaszi & Pierre Lombard",
    "",
    AF_FLAGS_NOT_REENTRANT,
    open
};
