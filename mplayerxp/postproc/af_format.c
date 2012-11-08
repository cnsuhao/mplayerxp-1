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
#include "dsp.h"
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

/* The below includes retrives functions for converting to and from
   ulaw and alaw */
#include "af_format_ulaw.c"
#include "af_format_alaw.c"

typedef struct af_format_s
{
  mpaf_format_e fmt;
}af_format_t;

// Switch endianess
static void endian(any_t* in, any_t* out, int len, int bps,int final);
// From singed to unsigned
static void si2us(any_t* in, any_t* out, int len, int bps,int final);
// From unsinged to signed
static void us2si(any_t* in, any_t* out, int len, int bps,int final);

// Helper functions to check sanity for input arguments

// Sanity check for unsupported formats
static ControlCodes __FASTCALL__ check_format(mpaf_format_e format)
{
    char buf[256];
    switch(format & MPAF_SPECIAL_MASK){
	case MPAF_MPEG2:
	case MPAF_AC3 :
	    MSG_ERR("[format] Sample format %s not yet supported \n",
	    mpaf_fmt2str(format,buf,255));
	    return CONTROL_ERROR;
    }
    if((format&MPAF_BPS_MASK) < 1 || (format&MPAF_BPS_MASK) > 4) {
	MSG_ERR("[format] The number of bytes per sample"
		" must be 1, 2, 3 or 4. Current value is %i \n",format&MPAF_BPS_MASK);
	return CONTROL_ERROR;
    }
    return CONTROL_OK;
}

// Initialization and runtime control
static ControlCodes __FASTCALL__ control(struct af_instance_s* af, int cmd, any_t* arg)
{
    af_format_t* s = af->setup;
    char buf1[256],buf2[256];
    switch(cmd){
	case AF_CONTROL_REINIT:
	    // Make sure this filter isn't redundant
	    if(af->data->format == ((mp_aframe_t*)arg)->format)
		return CONTROL_DETACH;
	    // Check for errors in configuraton
	    if((CONTROL_OK != check_format(((mp_aframe_t*)arg)->format)) ||
		(CONTROL_OK != check_format(af->data->format)))
		return CONTROL_ERROR;
	    s->fmt = ((mp_aframe_t*)arg)->format;
	    af->data->rate = ((mp_aframe_t*)arg)->rate;
	    af->data->nch  = ((mp_aframe_t*)arg)->nch;
	    af->mul.n      = af->data->format&MPAF_BPS_MASK;
	    af->mul.d      = ((mp_aframe_t*)arg)->format&MPAF_BPS_MASK;
	    return CONTROL_OK;
	case AF_CONTROL_SHOWCONF:
	    MSG_INFO("[af_format] Changing sample format %s -> %s\n",
		mpaf_fmt2str(s->fmt,buf1,255),
		mpaf_fmt2str(af->data->format,buf2,255));
	    return CONTROL_OK;
	case AF_CONTROL_COMMAND_LINE:{
	    int format = MPAF_NE;
	    // Convert string to format
	    format = mpaf_str2fmt((char *)arg);
	    if((CONTROL_OK != af->control(af,AF_CONTROL_FORMAT | AF_CONTROL_SET,&format)))
		return CONTROL_ERROR;
	    return CONTROL_OK;
	}
	case AF_CONTROL_FORMAT | AF_CONTROL_SET:
	    // Reinit must be called after this function has been called
	    // Check for errors in configuraton
	    if(CONTROL_OK != check_format(*(int*)arg)) return CONTROL_ERROR;
	    af->data->format = *(int*)arg;
	    return CONTROL_OK;
	default: break;
    }
    return CONTROL_UNKNOWN;
}

// Deallocate memory
static void __FASTCALL__ uninit(struct af_instance_s* af)
{
    if(af->data) mp_free(af->data);
    if(af->setup) mp_free(af->setup);
}

// Filter data through filter
static mp_aframe_t* __FASTCALL__ play(struct af_instance_s* af, mp_aframe_t* data,int final)
{
    mp_aframe_t*c   = data;	// Current working data
    int		len = c->len/(c->format&MPAF_BPS_MASK); // Lenght in samples of current audio block

    if(CONTROL_OK != RESIZE_LOCAL_BUFFER(af,data)) return NULL;
    mp_aframe_t*   l   = af->data;	// Local data

    // Change to cpu native endian format
    if((c->format&MPAF_END_MASK)!=MPAF_NE)
	endian(c->audio,c->audio,len,c->format&MPAF_BPS_MASK,final);
    // Conversion table
    if((c->format&MPAF_SPECIAL_MASK)==(MPAF_MU_LAW|MPAF_A_LAW))
    switch(c->format&MPAF_SPECIAL_MASK){
	case MPAF_MU_LAW:
	    from_ulaw(c->audio, l->audio, len,l->format&MPAF_BPS_MASK, l->format&MPAF_POINT_MASK);
	    if(MPAF_A_LAW == (l->format&MPAF_SPECIAL_MASK))
		to_ulaw(l->audio, l->audio, len, 1, MPAF_SI);
	    else if((l->format&MPAF_SIGN_MASK) == MPAF_US) {
		si2us(l->audio,l->audio,len,l->format&MPAF_BPS_MASK,final);
	    }
	    break;
	case MPAF_A_LAW:
	    from_alaw(c->audio, l->audio, len,l->format&MPAF_BPS_MASK, l->format&MPAF_POINT_MASK);
	    if(MPAF_A_LAW == (l->format&MPAF_SPECIAL_MASK))
		to_alaw(l->audio, l->audio, len, 1, MPAF_SI);
	    else if((l->format&MPAF_SIGN_MASK) == MPAF_US) {
		si2us(l->audio,l->audio,len,l->format&MPAF_BPS_MASK,final);
	    }
	    break;
	default: break; // Bote: here not mp3/ac3 decoders
    }
    else if(c->format&MPAF_F) {
	switch(l->format&MPAF_SPECIAL_MASK){
	    case MPAF_MU_LAW:
		to_ulaw(c->audio, l->audio, len,c->format&MPAF_BPS_MASK, c->format&MPAF_POINT_MASK);
		break;
	    case MPAF_A_LAW:
		to_alaw(c->audio, l->audio, len,c->format&MPAF_BPS_MASK, c->format&MPAF_POINT_MASK);
		break;
	    default:
		if((l->format&MPAF_SIGN_MASK) == MPAF_US) {
		    float2int(c->audio, l->audio, len,l->format&MPAF_BPS_MASK,0);
		    si2us(l->audio,l->audio,len,l->format&MPAF_BPS_MASK,final);
		} else
		    float2int(c->audio, l->audio, len, l->format&MPAF_BPS_MASK,final);
		break;
	}
    }
    else {// Input must be int
	// Change signed/unsigned
	if((c->format&MPAF_SIGN_MASK) != (l->format&MPAF_SIGN_MASK)){
	    if((c->format&MPAF_SIGN_MASK) == MPAF_US)
	    us2si(c->audio,c->audio,len,c->format&MPAF_BPS_MASK,final);
	else
	    si2us(c->audio,c->audio,len,c->format&MPAF_BPS_MASK,final);
	}
	// Convert to special formats
	switch(l->format&(MPAF_SPECIAL_MASK|MPAF_POINT_MASK)){
	    case MPAF_MU_LAW:
		to_ulaw(c->audio, l->audio, len, c->format&MPAF_BPS_MASK, c->format&MPAF_POINT_MASK);
		break;
	    case MPAF_A_LAW:
		to_alaw(c->audio, l->audio, len, c->format&MPAF_BPS_MASK, c->format&MPAF_POINT_MASK);
		break;
	    case MPAF_F:
		int2float(c->audio, l->audio, len, c->format&MPAF_BPS_MASK,final);
		break;
	    default:
		// Change the number of bits
		if((c->format&MPAF_BPS_MASK) != (l->format&MPAF_BPS_MASK))
		    change_bps(c->audio,l->audio,len,c->format&MPAF_BPS_MASK,l->format&MPAF_BPS_MASK,final);
		else
		    l->audio=c->audio;
		break;
	}
    }

    // Switch from cpu native endian to the correct endianess 
    if((l->format&MPAF_END_MASK)!=MPAF_NE)
	endian(l->audio,l->audio,len,l->format&MPAF_BPS_MASK,final);
    // Set output data
    c->audio  = l->audio;
    c->len    = len*(l->format&MPAF_BPS_MASK);
    c->format = l->format;
    return c;
}

// Allocate memory and set function pointers
static ControlCodes __FASTCALL__ open(af_instance_t* af){
    af->control=control;
    af->uninit=uninit;
    af->play=play;
    af->mul.n=1;
    af->mul.d=1;
    af->data=mp_calloc(1,sizeof(mp_aframe_t));
    af->setup=mp_calloc(1,sizeof(af_format_t));
    if(af->data == NULL) return CONTROL_ERROR;
    return CONTROL_OK;
}

// Description of this filter
af_info_t af_info_format = {
    "Sample format conversion",
    "format",
    "Anders",
    "",
    AF_FLAGS_REENTRANT,
    open
};

uint32_t load24bit(any_t* data, int pos) {
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
static void endian(any_t* in, any_t* out, int len, int bps,int final)
{
    register int i;
    switch(bps){
	case 2:
	    for(i=0;i<len;i++) ((uint16_t*)out)[i]=bswap_16(((uint16_t*)in)[i]);
	    break;
	case 3:{
	    register uint8_t s;
	    for(i=0;i<len;i++){
		s=((uint8_t*)in)[3*i];
		((uint8_t*)out)[3*i]=((uint8_t*)in)[3*i+2];
		if (in != out) ((uint8_t*)out)[3*i+1]=((uint8_t*)in)[3*i+1];
		((uint8_t*)out)[3*i+2]=s;
	    }
	    break;
	}
	case 4:
	    for(i=0;i<len;i++) ((uint32_t*)out)[i]=bswap_32(((uint32_t*)in)[i]);
	    break;
    }
}

static void si2us(any_t* in, any_t* out, int len, int bps,int final)
{
    register int i;
    switch(bps) {
	case 1:
	    for(i=0;i<len;i++) ((uint8_t*)out)[i]=(uint8_t)(SCHAR_MAX+((int)((int8_t*)in)[i]));
	    break;
	case 2:
	    for(i=0;i<len;i++) ((uint16_t*)out)[i]=(uint16_t)(SHRT_MAX+((int)((int16_t*)in)[i]));
	    break;
	case 3:
	    for(i=0;i<len;i++) store24bit(out, i, (uint32_t)(INT_MAX+(int32_t)load24bit(in, i)));
	    break;
	case 4:
	    for(i=0;i<len;i++) ((uint32_t*)out)[i]=(uint32_t)(INT_MAX+((int32_t*)in)[i]);
	    break;
    }
}

static void us2si(any_t* in, any_t* out, int len, int bps,int final)
{
    register int i;
    switch(bps){
	case 1:
	    for(i=0;i<len;i++) ((int8_t*)out)[i]=(int8_t)(SCHAR_MIN+((int)((uint8_t*)in)[i]));
	    break;
	case 2:
	    for(i=0;i<len;i++) ((int16_t*)out)[i]=(int16_t)(SHRT_MIN+((int)((uint16_t*)in)[i]));
	    break;
	case 3:
	    for(i=0;i<len;i++) store24bit(out, i, (int32_t)(INT_MIN+(uint32_t)load24bit(in, i)));
	    break;
	case 4:
	    for(i=0;i<len;i++) ((int32_t*)out)[i]=(int32_t)(INT_MIN+((uint32_t*)in)[i]);
	    break;
    }
}
