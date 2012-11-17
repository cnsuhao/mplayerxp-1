/* This audio output filter changes the format of a data block. Valid
   formats are: AFMT_U8, AFMT_S8, AFMT_S16_LE, AFMT_S16_BE
   AFMT_U16_LE, AFMT_U16_BE, AFMT_S32_LE and AFMT_S32_BE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <limits.h>

#include "af.h"
#include "osdep/bswap.h"
#include "osdep/mplib.h"
#include "aflib.h"
#include "loader/wine/mmreg.h"
#include "pp_msg.h"

// Integer to float conversion through lrintf()
#ifdef HAVE_LRINTF
#define __USE_ISOC99 1
#include <math.h>
#else
#define lrintf(x) ((int)(x))
#endif

/* Functions used by play to convert the input audio to the correct
   format */

typedef struct af_format_s
{
  mpaf_format_e fmt;
}af_format_t;

// Switch endianess
static void endian(const mp_aframe_t* in, mp_aframe_t* out);
// From singed to unsigned
static void si2us(const mp_aframe_t* in, mp_aframe_t* out);
// From unsinged to signed
static void us2si(const mp_aframe_t* in, mp_aframe_t* out);

// Helper functions to check sanity for input arguments

// Sanity check for unsupported formats
static MPXP_Rc __FASTCALL__ check_format(mpaf_format_e format)
{
    char buf[256];
    switch(format & MPAF_SPECIAL_MASK){
	case MPAF_MPEG2:
	case MPAF_AC3 :
	    MSG_ERR("[format] Sample format %s not yet supported \n",
	    mpaf_fmt2str(format,buf,255));
	    return MPXP_Error;
    }
    if((format&MPAF_BPS_MASK) < 1 || (format&MPAF_BPS_MASK) > 4) {
	MSG_ERR("[format] The number of bytes per sample"
		" must be 1, 2, 3 or 4. Current value is %i \n",format&MPAF_BPS_MASK);
	return MPXP_Error;
    }
    return MPXP_Ok;
}

static MPXP_Rc __FASTCALL__ config(struct af_instance_s* af,const af_conf_t* arg)
{
    af_format_t* s = af->setup;
    // Make sure this filter isn't redundant
    if(af->conf.format == arg->format) return MPXP_Detach;
    // Check for errors in configuraton
    if((MPXP_Ok != check_format(arg->format)) ||
	(MPXP_Ok != check_format(af->conf.format)))
	return MPXP_Error;
    s->fmt = arg->format;
    af->conf.rate  = arg->rate;
    af->conf.nch   = arg->nch;
    af->mul.n      = af->conf.format&MPAF_BPS_MASK;
    af->mul.d      = arg->format&MPAF_BPS_MASK;
    return MPXP_Ok;
}
// Initialization and runtime control
static MPXP_Rc __FASTCALL__ control(struct af_instance_s* af, int cmd, any_t* arg)
{
    af_format_t* s = af->setup;
    char buf1[256],buf2[256];
    switch(cmd){
	case AF_CONTROL_SHOWCONF:
	    MSG_INFO("[af_format] Changing sample format %s -> %s\n",
		mpaf_fmt2str(s->fmt,buf1,255),
		mpaf_fmt2str(af->conf.format,buf2,255));
	    return MPXP_Ok;
	case AF_CONTROL_COMMAND_LINE:{
	    int format = MPAF_NE;
	    // Convert string to format
	    format = mpaf_str2fmt((char *)arg);
	    if((MPXP_Ok != af->control(af,AF_CONTROL_FORMAT | AF_CONTROL_SET,&format)))
		return MPXP_Error;
	    return MPXP_Ok;
	}
	case AF_CONTROL_FORMAT | AF_CONTROL_SET:
	    // Reinit must be called after this function has been called
	    // Check for errors in configuraton
	    if(MPXP_Ok != check_format(*(int*)arg)) return MPXP_Error;
	    af->conf.format = *(int*)arg;
	    return MPXP_Ok;
	default: break;
    }
    return MPXP_Unknown;
}

// Deallocate memory
static void __FASTCALL__ uninit(struct af_instance_s* af)
{
    if(af->setup) mp_free(af->setup);
}

static mp_aframe_t* change_endian(const mp_aframe_t* in) {
    mp_aframe_t* out;
    out=new_mp_aframe_genome(in);
    mp_alloc_aframe(out);
    endian(in,out);
    return out;
}

static mp_aframe_t* convert_audio(const struct af_instance_s* af,const mp_aframe_t*in) {
    mp_aframe_t* out,*tmp;
    out=new_mp_aframe_genome(in);
    out->len=af_lencalc(af->mul,in);
    mp_alloc_aframe(out);

    if(in->format&MPAF_F) {
	float2int(in, out);
	if((out->format&MPAF_SIGN_MASK) == MPAF_US) si2us(out, out);
    } else {
	// Input is INT
	if((out->format&(MPAF_SPECIAL_MASK|MPAF_POINT_MASK))==MPAF_F){
	    if((in->format&(MPAF_SIGN_MASK))==MPAF_US) us2si(in, out);
	    int2float(out, out);
	}
	else {
	    if((in->format&MPAF_BPS_MASK) != (out->format&MPAF_BPS_MASK)) {
		// Change BPS
		tmp=new_mp_aframe_genome(in);
		mp_alloc_aframe(tmp);
		if((in->format&(MPAF_SIGN_MASK))==MPAF_SI) si2us(in, tmp);
		else tmp = (mp_aframe_t*)in;
		change_bps(tmp, out); // works with US only for now
		if(tmp!=in) free_mp_aframe(tmp);
		if((out->format&(MPAF_SIGN_MASK))==MPAF_SI) us2si(in, out);
	    } else if((in->format&MPAF_SIGN_MASK)!=(out->format&MPAF_SIGN_MASK)) {
		// Change signed/unsigned
		if((in->format&MPAF_SIGN_MASK) == MPAF_US)
		    us2si(in, out);
		else
		    si2us(in, out);
	    } else {
		// should never happens: bypass
		memcpy(out->audio,in->audio,in->len);
	    }
	}
    }
    return out;
}

// Filter data through filter
static mp_aframe_t* __FASTCALL__ play(struct af_instance_s* af, const mp_aframe_t* in)
{
    mp_aframe_t*out,*tmp;
    out=new_mp_aframe_genome(in);
    out->len=af_lencalc(af->mul,in);
    mp_alloc_aframe(out);
    tmp=NULL;
    // Change to cpu native endian format
    if((in->format&MPAF_END_MASK)!=MPAF_NE)
	tmp=change_endian(in);
    else
	tmp=(mp_aframe_t*)in;

    // Conversion table
    out=convert_audio(af,tmp);
    if(tmp!=in) free_mp_aframe(tmp);
    tmp=out;

    // Switch from cpu native endian to the correct endianess
    if((out->format&MPAF_END_MASK)!=MPAF_NE) {
	out=change_endian(tmp);
	free_mp_aframe(tmp);
    }

    return out;
}

// Allocate memory and set function pointers
static MPXP_Rc __FASTCALL__ af_open(af_instance_t* af){
    af->config=config;
    af->control=control;
    af->uninit=uninit;
    af->play=play;
    af->mul.n=1;
    af->mul.d=1;
    af->setup=mp_calloc(1,sizeof(af_format_t));
    if(af->setup == NULL) return MPXP_Error;
    check_pin("afilter",af->pin,AF_PIN);
    return MPXP_Ok;
}

// Description of this filter
af_info_t af_info_format = {
    "Sample format conversion",
    "format",
    "Anders",
    "",
    AF_FLAGS_REENTRANT,
    af_open
};

uint32_t load24bit(const any_t* data, int pos) {
#if WORDS_BIGENDIAN
    return (((uint32_t)((uint8_t*)data)[3*pos])<<24) |
	    (((uint32_t)((uint8_t*)data)[3*pos+1])<<16) |
	    (((uint32_t)((uint8_t*)data)[3*pos+2])<<8);
#else
    return (((uint32_t)((uint8_t*)data)[3*pos])<<8) |
	    (((uint32_t)((uint8_t*)data)[3*pos+1])<<16) |
	    (((uint32_t)((uint8_t*)data)[3*pos+2])<<24);
#endif
}

void store24bit(any_t* data, int pos, uint32_t expanded_value) {
#if WORDS_BIGENDIAN
      ((uint8_t*)data)[3*pos]=expanded_value>>24;
      ((uint8_t*)data)[3*pos+1]=expanded_value>>16;
      ((uint8_t*)data)[3*pos+2]=expanded_value>>8;
#else
      ((uint8_t*)data)[3*pos]=expanded_value>>8;
      ((uint8_t*)data)[3*pos+1]=expanded_value>>16;
      ((uint8_t*)data)[3*pos+2]=expanded_value>>24;
#endif
}

// Function implementations used by play
static void endian(const mp_aframe_t* in, mp_aframe_t* out)
{
    unsigned i;
    switch(in->format&MPAF_BPS_MASK){
	case 2:
	    for(i=0;i<in->len;i++) ((uint16_t*)out->audio)[i]=bswap_16(((uint16_t*)in->audio)[i]);
	    break;
	case 3:{
	    register uint8_t s;
	    for(i=0;i<in->len;i++){
		s=((uint8_t*)in->audio)[3*i];
		((uint8_t*)out->audio)[3*i]=((uint8_t*)in->audio)[3*i+2];
		if (in->audio != out->audio) ((uint8_t*)out->audio)[3*i+1]=((uint8_t*)in->audio)[3*i+1];
		((uint8_t*)out->audio)[3*i+2]=s;
	    }
	    break;
	}
	case 4:
	    for(i=0;i<in->len;i++) ((uint32_t*)out->audio)[i]=bswap_32(((uint32_t*)in->audio)[i]);
	    break;
    }
}

static void si2us(const mp_aframe_t* in, mp_aframe_t* out)
{
    unsigned i;
    switch(in->format&MPAF_BPS_MASK) {
	case 1:
	    for(i=0;i<in->len;i++) ((uint8_t*)out->audio)[i]=(uint8_t)(SCHAR_MAX+((int)((int8_t*)in->audio)[i]));
	    break;
	case 2:
	    for(i=0;i<in->len;i++) ((uint16_t*)out->audio)[i]=(uint16_t)(SHRT_MAX+((int)((int16_t*)in->audio)[i]));
	    break;
	case 3:
	    for(i=0;i<in->len;i++) store24bit(out->audio, i, (uint32_t)(INT_MAX+(int32_t)load24bit(in->audio, i)));
	    break;
	case 4:
	    for(i=0;i<in->len;i++) ((uint32_t*)out->audio)[i]=(uint32_t)(INT_MAX+((int32_t*)in->audio)[i]);
	    break;
    }
}

static void us2si(const mp_aframe_t* in, mp_aframe_t* out)
{
    unsigned i;
    switch(in->format&MPAF_BPS_MASK){
	case 1:
	    for(i=0;i<in->len;i++) ((int8_t*)out->audio)[i]=(int8_t)(SCHAR_MIN+((int)((uint8_t*)in->audio)[i]));
	    break;
	case 2:
	    for(i=0;i<in->len;i++) ((int16_t*)out->audio)[i]=(int16_t)(SHRT_MIN+((int)((uint16_t*)in->audio)[i]));
	    break;
	case 3:
	    for(i=0;i<in->len;i++) store24bit(out->audio, i, (int32_t)(INT_MIN+(uint32_t)load24bit(in->audio, i)));
	    break;
	case 4:
	    for(i=0;i<in->len;i++) ((int32_t*)out->audio)[i]=(int32_t)(INT_MIN+((uint32_t*)in->audio)[i]);
	    break;
    }
}
