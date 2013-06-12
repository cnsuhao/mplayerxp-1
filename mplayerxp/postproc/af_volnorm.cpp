#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
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
#include "af_internal.h"
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
#define MUL_MIN 0.1f
#define MUL_MAX 5.0f
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
struct af_volnorm_t
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
};

static MPXP_Rc __FASTCALL__ config_af(af_instance_t* af,const af_conf_t* arg)
{
    // Sanity check
    if(!arg) return MPXP_Error;

    af->conf.rate   = arg->rate;
    af->conf.nch    = arg->nch;

    if(!(mpaf_testa(arg->format,MPAF_F|MPAF_NE) ||
	mpaf_testa(arg->format,MPAF_SI|MPAF_NE)))
	return MPXP_Error;

    if(mpaf_testa(arg->format,MPAF_F|MPAF_NE))
	af->conf.format = MPAF_F|MPAF_NE|MPAF_BPS_4;
    else
	af->conf.format = MPAF_SI|MPAF_NE|MPAF_BPS_2;
    return af_test_output(af,arg);
}
// Initialization and runtime control_af
static MPXP_Rc __FASTCALL__ control_af(af_instance_t* af, int cmd, any_t* arg)
{
  af_volnorm_t* s   = (af_volnorm_t*)af->setup;

  switch(cmd){
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
static void __FASTCALL__ uninit(af_instance_t* af)
{
    if(af->setup) delete af->setup;
}

static mp_aframe_t __FASTCALL__ method1_int16(af_volnorm_t *s,const mp_aframe_t& c)
{
    unsigned i = 0;
    int16_t *data = (int16_t*)c.audio;	// Audio data
    unsigned len = c.len/2;		// Number of samples
    float curavg = 0.0, newavg, neededmul;
    int tmp;
    mp_aframe_t out = c.genome();
    out.alloc();

    for (i = 0; i < len; i++) {
	tmp = data[i];
	curavg += tmp * tmp;
    }
    curavg = sqrt(curavg / (float) len);

    // Evaluate an adequate 'mul' coefficient based on previous state, current
    // samples level, etc

    if (curavg > SIL_S16) {
	neededmul = MID_S16 / (curavg * s->mul);
	s->mul = (1.0 - SMOOTH_MUL) * s->mul + SMOOTH_MUL * neededmul;

	// clamp the mul coefficient
	s->mul = clamp(s->mul, MUL_MIN, MUL_MAX);
    }

    // Scale & clamp the samples
    for (i = 0; i < len; i++) {
	tmp = s->mul * data[i];
	tmp = clamp(tmp, SHRT_MIN, SHRT_MAX);
	((int16_t *)out.audio)[i] = tmp;
    }

    // Evaulation of newavg (not 100% accurate because of values clamping)
    newavg = s->mul * curavg;

    // Stores computed values for future smoothing
    s->lastavg = (1.0 - SMOOTH_LASTAVG) * s->lastavg + SMOOTH_LASTAVG * newavg;
    return out;
}

static mp_aframe_t __FASTCALL__ method1_float(af_volnorm_t *s,const mp_aframe_t& c)
{
    unsigned	i = 0;
    float*	data = (float*)c.audio;	// Audio data
    unsigned	len = c.len/4;		// Number of samples
    float	curavg = 0.0, newavg, neededmul, tmp;
    mp_aframe_t out = c.genome();
    out.alloc();

    for (i = 0; i < len; i++) {
	tmp = data[i];
	curavg += tmp * tmp;
    }
    curavg = sqrt(curavg / (float) len);

    // Evaluate an adequate 'mul' coefficient based on previous state, current
    // samples level, etc

    if (curavg > SIL_FLOAT) {// FIXME
	neededmul = MID_FLOAT / (curavg * s->mul);
	s->mul = (1.0 - SMOOTH_MUL) * s->mul + SMOOTH_MUL * neededmul;

	// clamp the mul coefficient
	s->mul = clamp(s->mul, MUL_MIN, MUL_MAX);
    }

    // Scale & clamp the samples
    for (i = 0; i < len; i++)
	((float*)out.audio)[i] = data[i] * s->mul;

    // Evaulation of newavg (not 100% accurate because of values clamping)
    newavg = s->mul * curavg;

    // Stores computed values for future smoothing
    s->lastavg = (1.0 - SMOOTH_LASTAVG) * s->lastavg + SMOOTH_LASTAVG * newavg;
    return out;
}

static mp_aframe_t __FASTCALL__ method2_int16(af_volnorm_t *s,const mp_aframe_t& c)
{
    unsigned	i = 0;
    int16_t*	data = (int16_t*)c.audio;	// Audio data
    unsigned	len = c.len/2;		// Number of samples
    float	curavg = 0.0, newavg, avg = 0.0;
    int		tmp, totallen = 0;
    mp_aframe_t out = c.genome();
    out.alloc();

    for (i = 0; i < len; i++) {
	tmp = data[i];
	curavg += tmp * tmp;
    }
    curavg = sqrt(curavg / (float) len);

    // Evaluate an adequate 'mul' coefficient based on previous state, current
    // samples level, etc
    for (i = 0; i < NSAMPLES; i++) {
	avg += s->mem[i].avg * (float)s->mem[i].len;
	totallen += s->mem[i].len;
    }

    if (totallen > MIN_SAMPLE_SIZE) {
	avg /= (float)totallen;
	if (avg >= SIL_S16) {
	    s->mul = MID_S16 / avg;
	    s->mul = clamp(s->mul, MUL_MIN, MUL_MAX);
	}
    }

    // Scale & clamp the samples
    for (i = 0; i < len; i++) {
	tmp = s->mul * data[i];
	tmp = clamp(tmp, SHRT_MIN, SHRT_MAX);
	((int16_t*)out.audio)[i] = tmp;
    }

    // Evaulation of newavg (not 100% accurate because of values clamping)
    newavg = s->mul * curavg;

    // Stores computed values for future smoothing
    s->mem[s->idx].len = len;
    s->mem[s->idx].avg = newavg;
    s->idx = (s->idx + 1) % NSAMPLES;
    return out;
}

static mp_aframe_t __FASTCALL__ method2_float(af_volnorm_t *s,const mp_aframe_t& c)
{
    unsigned	i = 0;
    float*	data = (float*)c.audio;	// Audio data
    unsigned	len = c.len/4;		// Number of samples
    float	curavg = 0.0, newavg, avg = 0.0, tmp;
    unsigned	totallen = 0;
    mp_aframe_t out = c.genome();
    out.alloc();

    for (i = 0; i < len; i++) {
	tmp = data[i];
	curavg += tmp * tmp;
    }
    curavg = sqrt(curavg / (float) len);

    // Evaluate an adequate 'mul' coefficient based on previous state, current
    // samples level, etc
    for (i = 0; i < NSAMPLES; i++) {
	avg += s->mem[i].avg * (float)s->mem[i].len;
	totallen += s->mem[i].len;
    }

    if (totallen > MIN_SAMPLE_SIZE) {
	avg /= (float)totallen;
	if (avg >= SIL_FLOAT) {
	    s->mul = MID_FLOAT / avg;
	    s->mul = clamp(s->mul, MUL_MIN, MUL_MAX);
	}
    }

    // Scale & clamp the samples
    for (i = 0; i < len; i++) ((float*)out.audio)[i] = data[i] * s->mul;

    // Evaulation of newavg (not 100% accurate because of values clamping)
     newavg = s->mul * curavg;

    // Stores computed values for future smoothing
    s->mem[s->idx].len = len;
    s->mem[s->idx].avg = newavg;
    s->idx = (s->idx + 1) % NSAMPLES;
    return out;
}

// Filter data through filter
static mp_aframe_t __FASTCALL__ play(af_instance_t* af,const mp_aframe_t& in)
{
    af_volnorm_t *s = reinterpret_cast<af_volnorm_t*>(af->setup);
    mp_aframe_t out=in.genome();

    if(af->conf.format == (MPAF_SI | MPAF_NE)) {
	if (s->method)	out=method2_int16(s, in);
	else		out=method1_int16(s, in);
    }
    else {
	if (s->method)	out=method2_float(s, in);
	else		out=method1_float(s, in);
    }
    return out;
}

// Allocate memory and set function pointers
static MPXP_Rc __FASTCALL__ af_open(af_instance_t* af){
  int i = 0;
  af->config_af=config_af;
  af->control_af=control_af;
  af->uninit=uninit;
  af->play=play;
  af->mul.n=1;
  af->mul.d=1;
  af->setup=mp_calloc(1,sizeof(af_volnorm_t));
  if(af->setup == NULL) return MPXP_Error;

  ((af_volnorm_t*)af->setup)->mul = MUL_INIT;
  ((af_volnorm_t*)af->setup)->lastavg = MID_S16;
  ((af_volnorm_t*)af->setup)->idx = 0;
  for (i = 0; i < NSAMPLES; i++) {
     ((af_volnorm_t*)af->setup)->mem[i].len = 0;
     ((af_volnorm_t*)af->setup)->mem[i].avg = 0;
  }
    check_pin("afilter",af->pin,AF_PIN);
  return MPXP_Ok;
}

// Description of this filter
extern const af_info_t af_info_volnorm = {
    "Volume normalizer filter",
    "volnorm",
    "Alex Beregszaszi & Pierre Lombard",
    "",
    AF_FLAGS_NOT_REENTRANT,
    af_open
};
