/*=============================================================================
//
//  This software has been released under the terms of the GNU General Public
//  license. See http://www.gnu.org/copyleft/gpl.html for details.
//
//  Copyright (C) 2001 Rafal Bosak <gyver@fanthom.irc.pl>
//  Copyright 2004 Nickols_K <nickols_k@mail.ru>
  TODO:
 - MaxBass
 - audiophile quality by adding of even haromonics
 - coeffs should depends on i_bps of mp3 stream (if mp3 stream)
 - add anything what's syntetically improves psychoacoustic of sound

   HINT (for MaxBass and MaxHighFreqs):
   If we have this Frequencies respond:
  |      --------------------      |
  |    /                      \    |
  |  /                          \  |
  |/                              \|
--+--------------------------------+---
  40Hz?                         15-16kHz
   Then we should do nothing!
   If we have this FR:
  |--------------------------------|
  |                                |
  |                                |
  |                                |
--+--------------------------------+---
  40Hz?                         15-16kHz
   Then it would be correctly to predict that mp3 has cutted off basses
and high-freqs and try to restore FR as:
     |--------------------------------|
    /|                                |\
  /  |                                |  \
/    |                                |    \
-----+--------------------------------+------
    40Hz?                         15-16kHz

//=============================================================================
*/

/*
   Crystality filter, was based on Crystality Plugin v0.92 for XMMS by
   Rafal Bosak <gyver@fanthom.irc.pl>.

   Note: You will need a reasonably good stereo and a good ear to notice
   quality improvement, otherwise this is not for you.

   This plugin tries to patch mp3 format flaws, not a poor audio hardware!
*/

#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <inttypes.h>
#ifndef __USE_GNU
#define __USE_GNU
#endif
#include <math.h>
#include <limits.h>

#include "mplayerxp.h"
#include "af.h"
#include "aflib.h"

#include "libmpdemux/stream.h"
#include "libmpdemux/demuxer.h"
#include "libmpdemux/stheader.h"
#include "osdep/mplib.h"

#define SAMPLE_MAX 1.

//#define SAMPLE_MAX 1.0 /* for float32 */

/*#define SAMPLE_MAX 32768      - for S16LE */
/*#define SAMPLE_MAX 2147483647 - for S32LE */

/* Data for specific instances of this filter */
typedef struct af_crystality_s
{
    int bext_level;
    int echo_level;
    int stereo_level; /* not used*/
    int filter_level;
    int feedback_level;
    int feedback_threshold;
    int harmonics_level;

    float bext_sfactor;
    float echo_sfactor;
    float stereo_sfactor;
    float feedback_sfactor;
    float feedback_divisor;
    float harmonics_sfactor;
    /* Real and Imag arrays for Discrete Fourier Transform */
    float lsine[65536];
    float rsine[65536];
    highp_t hp;
    lowp_t lp_reverb[2];
    lowp_t lp_bass[2];
    float hf_div;
} af_crystality_t;

static void __FASTCALL__ set_defaults(af_crystality_t *s)
{
    s->bext_level = 28;
    s->echo_level = 11;
    s->stereo_level = 11;
    s->filter_level = 3;
    s->feedback_level = 30;
    s->feedback_threshold = 20; /*dB*/
    s->harmonics_level = 43;
}

static void __FASTCALL__ bext_level_cb(af_crystality_t *s)
{
    s->bext_sfactor = (float)((SAMPLE_MAX*5) / (float)(s->bext_level + 1)) + (float)(102 - s->bext_level) * 128;
}

static void __FASTCALL__ echo_level_cb(af_crystality_t *s)
{
    s->echo_sfactor = s->echo_level;
}

static void __FASTCALL__ stereo_level_cb(af_crystality_t *s)
{
    s->stereo_sfactor = s->stereo_level;
}

static void __FASTCALL__ feedback_level_cb(af_crystality_t *s)
{
    s->feedback_sfactor = (s->feedback_level * 3) / 2;
}

static void __FASTCALL__ feedback_threshold_cb(af_crystality_t *s)
{
    s->feedback_divisor = pow(10,(float)s->feedback_threshold/20);
}

static void __FASTCALL__ harmonics_level_cb(af_crystality_t *s)
{
    s->harmonics_sfactor = s->harmonics_level;
}

static void __FASTCALL__ init_crystality(af_crystality_t *s,unsigned rate)
{
#if 0
    unsigned i;
    double lsum;
    double rsum;
#endif
    bext_level_cb(s);
    echo_level_cb(s);
    stereo_level_cb(s);
    feedback_level_cb(s);
    feedback_threshold_cb(s);
    harmonics_level_cb(s);
    highp_init(&s->hp,15000,rate);
    lowp_init(&s->lp_reverb[0],5000,rate);
    lowp_init(&s->lp_reverb[1],5000,rate);
    lowp_init(&s->lp_bass[0],40,rate);
    lowp_init(&s->lp_bass[1],40,rate);
#if 0
#define COND 0
    /* non linear coeefs for DFT */
    for (i = 0; i < 32768; ++i) {
	lsum = rsum = 0;
	if (COND || i < 32768 )lsum+=  ((cos((double)i * 3.141592535 / 32768/2	)) + 0) / 2;
	if (COND || i < 16384 )rsum-=  ((cos((double)i * 3.141592535 / 16384/2	)) + 0) / 4;
	rsum = lsum;

	if (COND || i < 8192 ) lsum += ((cos((double)i * 3.141592535 / 8192/2	 )) + 0) /  8;
	if (COND || i < 5641 ) rsum += ((cos((double)i * 3.141592535 / 5641.333333/2)) + 0) /  8;

	s->lsine[32768 + i] = (double)(lsum - 0.5) * SAMPLE_MAX * 1.45;
	s->lsine[32768 - i] = s->lsine[32768 + i];
	s->rsine[32768 + i] = (double)(rsum - 0.5) * SAMPLE_MAX * 1.45;
	s->rsine[32768 - i] = s->rsine[32768 + i];
    }
#endif
}


/*
 * quite nice echo
 */

#define DELAY2 21000
#define DELAY1 35000
#define DELAY3 14000

#define BUF_SIZE DELAY1+DELAY2+DELAY3

static float left0p = 0, right0p = 0;
static float buf[BUF_SIZE];
static unsigned _bufPos = BUF_SIZE - 1;
static unsigned bufPos[3];
static void __FASTCALL__ echo3d(af_crystality_t *setup,float *data, unsigned datasize)
{
  unsigned x,i;
  float _left, _right, dif, difh, leftc, rightc, left[4], right[4];
  float *dataptr;
  float lt, rt;


#if 0
  float lsine,rsine;
  static float lharmb = 0, rharmb = 0, lhfb = 0, rhfb = 0;
  float lharm0, rharm0;
  float lsf, rsf;
#endif
  bufPos[0] = 1 + BUF_SIZE - DELAY1;
  bufPos[1] = 1 + BUF_SIZE - DELAY1 - DELAY2;
  bufPos[2] = 1 + BUF_SIZE - DELAY1 - DELAY2 - DELAY3;
  dataptr = data;

  for (x = 0; x < datasize; x += 8) {

    // ************ load sample **********
    left[0] = dataptr[0];
    right[0] = dataptr[1];

    // ************ calc 4 echos **********
    for(i=0;i<3;i++)
    {
	dif = (left[i] - right[i]);
	// ************ slightly expand stereo for direct input **********
	if(!i)
	{
	    difh = highpass(&setup->hp,dif)*setup->stereo_sfactor / setup->hf_div;
	    dif *= setup->stereo_sfactor / 64;
	    dif += difh;
	}
	left[i] += dif;
	right[i] -= dif;
	left[i] *= 0.8;
	right[i] *= 0.8;

	// ************ compute echo  **********
	left[i+1] = buf[bufPos[i]++];
	if (bufPos[i] == BUF_SIZE) bufPos[i] = 0;
	right[i+1] = buf[bufPos[i]++];
	if (bufPos[i] == BUF_SIZE) bufPos[i] = 0;
    }
    // ************ a weighted sum taken from reverb buffer **********
    leftc = left[1] / 9 + right[2] /8  + left[3] / 8;
    rightc = right[1] / 11 + left[2] / 9 + right[3] / 10;

    // ************ mix reverb with (near to) direct input **********
    _left = left0p + leftc * setup->echo_sfactor / 16;
    _right = right0p + rightc * setup->echo_sfactor / 16;

    /* do not reverb high frequencies (filter) */
    lt=(lowpass(&setup->lp_reverb[0],leftc+left[0]/2)*setup->feedback_sfactor)/256;
    rt=(lowpass(&setup->lp_reverb[1],rightc+right[0]/2)*setup->feedback_sfactor)/256;;

    buf[_bufPos++] = lt;
    if (_bufPos == BUF_SIZE) _bufPos = 0;
    buf[_bufPos++] = rt;
    if (_bufPos == BUF_SIZE) _bufPos = 0;

#if 0
    // TODO: make nonolinearity with using of other algorithms.
    //		to reach audiophile quality
    // ************ add some extra even harmonics **********
    // ************ or rather specific nonlinearity

    lhfb += (lt - lhfb) / 32;
    rhfb += (rt - rhfb) / 32;

    lsf = ls - lhfb;
    rsf = rs - rhfb;
    lsine=setup->lsine[(unsigned)(((lsf/4) + 0.5)*32768) % 65536];
    rsine=setup->rsine[(unsigned)(((rsf/4) + 0.5)*32768) % 65536];
    lharm0 =
	+ ((lsf + SAMPLE_MAX/3) * (((((float)lsine*setup->harmonics_sfactor)) / 64))) / SAMPLE_MAX
	- ((float)rsine * setup->harmonics_sfactor) / 128;
    rharm0 =
	+ ((rsf + SAMPLE_MAX/3) * (((((float)lsine*setup->harmonics_sfactor)) / 64))) / SAMPLE_MAX
	- ((float)rsine * setup->harmonics_sfactor) / 128;
    lharmb += (lharm0 * 32768 - lharmb) / 16384;
    rharmb += (rharm0 * 32768 - rharmb) / 16384;

    _left  += lharm0 - lharmb / 32768;
    _right += rharm0 - rharmb / 32768;
#endif
    left0p = left[0];
    right0p = right[0];

    // ************ store sample **********
    dataptr[0] = _left; //clamp(_left,-1.0,1.0);
    dataptr[1] = _right;//clamp(_right,-1.0,1.0);
    dataptr += 2;
   }
}

/*
 * simple pith shifter, plays short fragments at 1.75 speed
 */
#define SH_BUF_SIZE 100 * 4
static float shBuf[SH_BUF_SIZE];
static unsigned shBufPos = SH_BUF_SIZE - 8;
static unsigned shBufPos1 = SH_BUF_SIZE - 8;
static int cond;
static void __FASTCALL__ pitchShifter(const float lin, const float rin, float *lout, float *rout){

    shBuf[shBufPos++] = lin;
    shBuf[shBufPos++] = rin;

    if (shBufPos == SH_BUF_SIZE) shBufPos = 0;

    switch (cond){
	case 1:
	    *lout = (shBuf[shBufPos1 + 0] * 2 + shBuf[shBufPos1 + 2])/4;
	    *rout = (shBuf[shBufPos1 + 1] * 2 + shBuf[shBufPos1 + 3])/4;
	    break;
	case 0:
	    *lout = (shBuf[shBufPos1 + 4] * 2 + shBuf[shBufPos1 + 6])/4;
	    *rout = (shBuf[shBufPos1 + 5] * 2 + shBuf[shBufPos1 + 7])/4;
	    cond = 2;
	    shBufPos1 += 8;
	    if (shBufPos1 == SH_BUF_SIZE) {
		shBufPos1 = 0;
	    }
	    break;
    }
    cond--;
}

struct Interpolation{
    int acount;		// counter
    float lval, rval;	// value
    float sal, sar;	// sum
    float al, ar;
    float a1l, a1r;
};

/*
 * interpolation routine for ampliude and "energy"
 */
static inline void __FASTCALL__ interpolate(struct Interpolation *s, float l, float r){
#define AMPL_COUNT 64
    float a0l, a0r, dal = 0, dar = 0;

    if (l < 0) l = -l;
    if (r < 0) r = -r;

    s->lval += l / 8;
    s->rval += r / 8;

    s->lval = (s->lval * 120) / 128;
    s->rval = (s->rval * 120) / 128;

    s->sal += s->lval;
    s->sar += s->rval;

    s->acount++;
    if (s->acount == AMPL_COUNT){
	s->acount = 0;
	a0l = s->a1l;
	a0r = s->a1r;
	s->a1l = s->sal / AMPL_COUNT;
	s->a1r = s->sar / AMPL_COUNT;
	s->sal = 0;
	s->sar = 0;
	dal = s->a1l - a0l;
	dar = s->a1r - a0r;
	s->al = a0l * AMPL_COUNT;
	s->ar = a0r * AMPL_COUNT;
    }

    s->al += dal;
    s->ar += dar;
}

/*
 * calculate scalefactor for mixer
 */
inline float __FASTCALL__ calc_scalefactor(float a, float e){
    float x;

    a=clamp(a,0,SAMPLE_MAX/4);
    e=clamp(e,0,SAMPLE_MAX/4);
//    return (e + a) /2;
    x = ((e+500/SAMPLE_MAX) * 4096 )/ (a + 300/SAMPLE_MAX) + e;
    return clamp(x,0,SAMPLE_MAX/2);
}

static struct Interpolation bandext_energy;
static struct Interpolation bandext_amplitude;

/*
 * exact bandwidth extender ("exciter") routine
 */
static void __FASTCALL__ bandext(af_crystality_t *setup,float *data, const unsigned datasize)
{

    unsigned x,i;
    float _left, _right;
    float *dataptr = data;
    static float lprev[4], rprev[4];
    float left[5], right[5];
    static float lamplUp, lamplDown;
    static float ramplUp, ramplDown;
    float lampl, rampl;
    float tmp;

    for (x = 0; x < datasize; x += 8) {

	// ************ load sample **********
	left[0] = dataptr[0];
	right[0] = dataptr[1];

#if 0
	_left=lowpass(&setup->lp_bass[0],left[0]);
	_right=lowpass(&setup->lp_bass[1],right[0]);
	left[0] =left[0]/2+_left;
	right[0]=right[0]/2+_right;
#endif
	// ************ highpass filter part 1 **********
	for(i=0;i<4;i++)
	{
	    left[i+1]  = (left[i]  - lprev[i]) * 56880 / 65536;
	    right[i+1] = (right[i] - rprev[i]) * 56880 / 65536;
	}
	pitchShifter(left[setup->filter_level],right[setup->filter_level],&_left,&_right);

	// ************ amplitude detector ************
	tmp = left[0] + lprev[0];
	if      (tmp * 16 > lamplUp  ) lamplUp   += (tmp - lamplUp  );
	else if (tmp * 16 < lamplDown) lamplDown += (tmp - lamplDown);
	lamplUp   = (lamplUp   * 1000) /1024;
	lamplDown = (lamplDown * 1000) /1024;
	lampl = lamplUp - lamplDown;

	tmp = right[0] + rprev[0];
	if      (tmp * 16 > ramplUp  ) ramplUp   += (tmp - ramplUp  );
	else if (tmp * 16 < ramplDown) ramplDown += (tmp - ramplDown);
	ramplUp   = (ramplUp   * 1000) /1024;
	ramplDown = (ramplDown * 1000) /1024;
	rampl = ramplUp - ramplDown;

	interpolate(&bandext_amplitude, lampl, rampl);

	// ************ "sound energy" detector (approx. spectrum complexity) ***********
	interpolate(&bandext_energy, left[0]  - lprev[0], right[0] - rprev[0]);

	// ************ mixer ***********
	_left   = left[0] + _left  * calc_scalefactor(bandext_amplitude.lval, bandext_energy.lval) / setup->bext_sfactor;
	_right  = right[0] + _right * calc_scalefactor(bandext_amplitude.rval, bandext_energy.rval) / setup->bext_sfactor; //16384

	// ************ highpass filter part 2 **********
	// ************ save previous values for filter
	for(i=0;i<3;i++)
	{
	    lprev[i]=left[i];
	    rprev[i]=right[i];
	}
	// ************ END highpass filter part 2 **********
	dataptr[0] = _left;//clamp(_left,-1.0,+1.0);
	dataptr[1] = _right;//clamp(_right,-1.0,+1.0);
	dataptr += 2;
    }
}

// Initialization and runtime control
static MPXP_Rc __FASTCALL__ control(struct af_instance_s* af, int cmd, any_t* arg)
{
  af_crystality_t* s   = (af_crystality_t*)af->setup;

  switch(cmd){
  case AF_CONTROL_REINIT:{
    unsigned i_bps,fmt;
    // Sanity check
    if(!arg) return MPXP_Error;
    if(((mp_aframe_t*)arg)->nch!=2) return MPXP_Error;

    af->data->rate   = ((mp_aframe_t*)arg)->rate;
    af->data->nch    = ((mp_aframe_t*)arg)->nch;
    af->data->format = MPAF_NE|MPAF_F|4;
    init_crystality(s,af->data->rate);
    i_bps=((sh_audio_t *)((af_stream_t *)af->parent)->parent)->i_bps*8;
    fmt=((sh_audio_t *)((af_stream_t *)af->parent)->parent)->wtag;
    if(fmt==0x55 || fmt==0x50) /* MP3 */
    {
	((af_crystality_t *)af->setup)->hf_div=i_bps/6000;
    }
    else ((af_crystality_t *)af->setup)->hf_div=32;

    return af_test_output(af,arg);
  }
  case AF_CONTROL_COMMAND_LINE:{
    sscanf((char*)arg,"%d:%d:%d:%d:%d:%d:%d",
	    &s->bext_level,
	    &s->echo_level,
	    &s->stereo_level,
	    &s->filter_level,
	    &s->feedback_level,
	    &s->feedback_threshold,
	    &s->harmonics_level);
    s->filter_level=clamp(s->filter_level,0,4);
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
    mp_aframe_t* c = data; /* Current working data */
    echo3d(af->setup,(float*)c->audio, c->len);
    bandext(af->setup,(float*)c->audio, c->len);
    return c;
}

// Allocate memory and set function pointers
static MPXP_Rc __FASTCALL__ af_open(af_instance_t* af){
  af->control=control;
  af->uninit=uninit;
  af->play=play;
  af->mul.n=1;
  af->mul.d=1;
  af->data=mp_calloc(1,sizeof(mp_aframe_t));
  af->setup=mp_calloc(1,sizeof(af_crystality_t));
  if(af->data == NULL || af->setup == NULL)
    return MPXP_Error;
  set_defaults(af->setup);
  init_crystality(af->setup,44100);
  left0p = right0p = 0;
  _bufPos = BUF_SIZE - 1;
  shBufPos = SH_BUF_SIZE - 8;
  shBufPos1 = SH_BUF_SIZE - 8;
    check_pin("afilter",af->pin,AF_PIN);
  return MPXP_Ok;
}

// Description of this filter
const af_info_t af_info_crystality = {
  "Crystality audio filter",
  "crystality",
  "Rafal Bosak <gyver@fanthom.irc.pl>",
  "Imported/improved by Nickols_K",
  AF_FLAGS_NOT_REENTRANT,
  af_open
};







