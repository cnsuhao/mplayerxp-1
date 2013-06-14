#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
/* This audio output filter changes the format of a data block. Valid
   formats are: AFMT_U8, AFMT_S8, AFMT_S16_LE, AFMT_S16_BE
   AFMT_U16_LE, AFMT_U16_BE, AFMT_S32_LE and AFMT_S32_BE.
*/
#include <vector>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <limits.h>

#include "af.h"
#include "af_internal.h"
#include "osdep/bswap.h"
#include "aflib.h"
#include "win32sdk/mmreg.h"
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
typedef mp_aframe_t (*convert_audio_t)(const af_instance_t* af,const mp_aframe_t& in);
struct af_format_t {
    mpaf_format_e fmt;
    std::vector<convert_audio_t> cvt_chain;
};

// Switch endianess
static void endian(const mp_aframe_t& in, mp_aframe_t* out);
// From singed to unsigned
static void si2us(const mp_aframe_t& in, mp_aframe_t* out);
// From unsinged to signed
static void us2si(const mp_aframe_t& in, mp_aframe_t* out);

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

static void endian(const mp_aframe_t& in, mp_aframe_t* out)
{
    unsigned i,nsamples=in.len/in.format&MPAF_BPS_MASK;
    unsigned bps=in.format&MPAF_BPS_MASK;
    switch(bps){
	case 2:
	    for(i=0;i<nsamples;i++) ((uint16_t*)out->audio)[i]=bswap_16(((uint16_t*)in.audio)[i]);
	    break;
	case 3:{
	    uint8_t s;
	    for(i=0;i<nsamples;i++){
		s=((uint8_t*)in.audio)[3*i];
		((uint8_t*)out->audio)[3*i]=((uint8_t*)in.audio)[3*i+2];
		if (in.audio != out->audio) ((uint8_t*)out->audio)[3*i+1]=((uint8_t*)in.audio)[3*i+1];
		((uint8_t*)out->audio)[3*i+2]=s;
	    }
	    break;
	}
	case 4:
	    for(i=0;i<nsamples;i++) ((uint32_t*)out->audio)[i]=bswap_32(((uint32_t*)in.audio)[i]);
	    break;
    }
}

static void si2us(const mp_aframe_t& in, mp_aframe_t* out)
{
    unsigned i,nsamples=in.len/in.format&MPAF_BPS_MASK;
    unsigned bps=in.format&MPAF_BPS_MASK;
    switch(bps) {
	case 1:
	    for(i=0;i<nsamples;i++) ((uint8_t*)out->audio)[i]=(uint8_t)(SCHAR_MAX+((int)((int8_t*)in.audio)[i]));
	    break;
	case 2:
	    for(i=0;i<nsamples;i++) ((uint16_t*)out->audio)[i]=(uint16_t)(SHRT_MAX+((int)((int16_t*)in.audio)[i]));
	    break;
	case 3:
	    for(i=0;i<nsamples;i++) store24bit(out->audio, i, (uint32_t)(INT_MAX+(int32_t)load24bit(in.audio, i)));
	    break;
	case 4:
	    for(i=0;i<nsamples;i++) ((uint32_t*)out->audio)[i]=(uint32_t)(INT_MAX+((int32_t*)in.audio)[i]);
	    break;
    }
}

static void us2si(const mp_aframe_t& in, mp_aframe_t* out)
{
    unsigned i,nsamples=in.len/in.format&MPAF_BPS_MASK;
    unsigned bps=in.format&MPAF_BPS_MASK;
    switch(bps){
	case 1:
	    for(i=0;i<nsamples;i++) ((int8_t*)out->audio)[i]=(int8_t)(SCHAR_MIN+((int)((uint8_t*)in.audio)[i]));
	    break;
	case 2:
	    for(i=0;i<nsamples;i++) ((int16_t*)out->audio)[i]=(int16_t)(SHRT_MIN+((int)((uint16_t*)in.audio)[i]));
	    break;
	case 3:
	    for(i=0;i<nsamples;i++) store24bit(out->audio, i, (int32_t)(INT_MIN+(uint32_t)load24bit(in.audio, i)));
	    break;
	case 4:
	    for(i=0;i<nsamples;i++) ((int32_t*)out->audio)[i]=(int32_t)(INT_MIN+((uint32_t*)in.audio)[i]);
	    break;
    }
}

static void print_fmts(const char *pfx,const mp_aframe_t& in,const mp_aframe_t& out) {
    mpxp_v<<pfx<<" in_fmt="<<mpaf_fmt2str(in.format)<<"[len="<<in.len<<"] -> out_fmt="<<mpaf_fmt2str(out.format)<<"[len="<<out.len<<"]"<<std::endl;
}

static mp_aframe_t change_endian(const af_instance_t* af,const mp_aframe_t& in) {
    mp_aframe_t out=in.genome();
    out.alloc();
    endian(in,&out);
    out.format^=MPAF_LE;
    return out;
}

static mp_aframe_t convert_audio_f(const af_instance_t* af,const mp_aframe_t& in) {
    mp_aframe_t out=in.genome();
    out.len=af_lencalc(af->mul,in);
    out.alloc();

    if(in.format&MPAF_F) {
	out.format=af->conf.format;
	float2int(in,&out);
    }
    if((out.format&(MPAF_SPECIAL_MASK|MPAF_POINT_MASK))==MPAF_F) {
	int2float(in,&out);
	out.format=MPAF_PCM|MPAF_NE|MPAF_F|MPAF_BPS_4;
    }
    return out;
}

static mp_aframe_t convert_audio_i(const af_instance_t* af,const mp_aframe_t& in) {
    mp_aframe_t out=in.genome();
    out.len=af_lencalc(af->mul,in);
    out.alloc();
    out.format=af->conf.format;
    change_bps(in,&out); // works with US only for now
    return out;
}

static mp_aframe_t convert_si2us(const af_instance_t* af,const mp_aframe_t& in) {
    mp_aframe_t out=in.genome();
    out.alloc();
    out.format|=MPAF_US;
    si2us(in,&out);
    return out;
}

static mp_aframe_t convert_us2si(const af_instance_t* af,const mp_aframe_t& in) {
    mp_aframe_t out=in.genome();
    out.alloc();
    out.format&=~MPAF_US;
    us2si(in,&out);
    return out;
}

static MPXP_Rc build_cvt_chain(af_instance_t* af,const af_conf_t* in) {
    af_format_t* s = reinterpret_cast<af_format_t*>(af->setup);

    s->cvt_chain.clear();
    if((in->format&MPAF_END_MASK)!=MPAF_NE) s->cvt_chain.push_back(change_endian);

    if(in->format&MPAF_F||af->conf.format&MPAF_F) s->cvt_chain.push_back(convert_audio_f);
    else {
	if(!(in->format&MPAF_US)) s->cvt_chain.push_back(convert_si2us);
	s->cvt_chain.push_back(convert_audio_i);
	if(!(af->conf.format&MPAF_SI)) s->cvt_chain.push_back(convert_us2si);
    }
    // Switch from cpu native endian to the correct endianess
    if((af->conf.format&MPAF_END_MASK)!=MPAF_NE) s->cvt_chain.push_back(change_endian);
    return MPXP_Ok;
}

// Sanity check for unsupported formats
static MPXP_Rc __FASTCALL__ check_format(mpaf_format_e format)
{
    if((format&MPAF_SPECIAL_MASK)!=MPAF_PCM){
	    mpxp_err<<"[format] Sample format "<<mpaf_fmt2str(format)<<" not yet supported"<<std::endl;
	    return MPXP_Error;
    }
    if((format&MPAF_BPS_MASK) < 1 || (format&MPAF_BPS_MASK) > 4) {
	mpxp_err<<"[format] The number of bytes per sample must be 1, 2, 3 or 4. Current value is "<<(format&MPAF_BPS_MASK)<<std::endl;
	return MPXP_Error;
    }
    return MPXP_Ok;
}

static MPXP_Rc __FASTCALL__ af_config(af_instance_t* af,const af_conf_t* arg)
{
    af_format_t* s = reinterpret_cast<af_format_t*>(af->setup);
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
    return build_cvt_chain(af,arg);
}
// Initialization and runtime control_af
static MPXP_Rc __FASTCALL__ control_af(af_instance_t* af, int cmd, any_t* arg)
{
    af_format_t* s = reinterpret_cast<af_format_t*>(af->setup);
    switch(cmd){
	case AF_CONTROL_SHOWCONF:
	    mpxp_info<<"[af_format] Changing sample format "<<mpaf_fmt2str(s->fmt)<<" -> "<<mpaf_fmt2str(af->conf.format)<<std::endl;
	    return MPXP_Ok;
	case AF_CONTROL_COMMAND_LINE:{
	    int format = MPAF_NE;
	    // Convert string to format
	    format = mpaf_str2fmt((char *)arg);
	    if((MPXP_Ok != af->control_af(af,AF_CONTROL_FORMAT | AF_CONTROL_SET,&format)))
		return MPXP_Error;
	    return MPXP_Ok;
	}
	case AF_CONTROL_FORMAT | AF_CONTROL_SET:
	    // Reinit must be called after this function has been called
	    // Check for errors in configuraton
	    if(MPXP_Ok != check_format(*(mpaf_format_e*)arg)) return MPXP_Error;
	    af->conf.format = *(mpaf_format_e*)arg;
	    return MPXP_Ok;
	default: break;
    }
    return MPXP_Unknown;
}

// Deallocate memory
static void __FASTCALL__ uninit(af_instance_t* af)
{
    if(af->setup) {
	af_format_t* s = reinterpret_cast<af_format_t*>(af->setup);
	s->cvt_chain.clear();
	delete af->setup;
    }
}

// Filter data through filter
static mp_aframe_t __FASTCALL__ play(af_instance_t* af, const mp_aframe_t& data)
{
    af_format_t* s = reinterpret_cast<af_format_t*>(af->setup);
    mp_aframe_t* in=&const_cast<mp_aframe_t&>(data);
    mp_aframe_t out=data.genome();
    std::vector<convert_audio_t>::iterator it;
    for ( it=s->cvt_chain.begin(); it < s->cvt_chain.end(); it++ ) {
	convert_audio_t cvt;
	cvt=*it;
	out=(*cvt)(af,*in);
	if(in!=&data) delete in;
	in=&out;
    }
    out.format=af->conf.format;
    return out;
}

// Allocate memory and set function pointers
static MPXP_Rc __FASTCALL__ af_open(af_instance_t* af){
    af->config_af=af_config;
    af->control_af=control_af;
    af->uninit=uninit;
    af->play=play;
    af->mul.n=1;
    af->mul.d=1;
    af->setup=new(zeromem) af_format_t;

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

