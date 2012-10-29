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
  int fmt;
  int bps;
}af_format_t;

// Switch endianess
static void endian(any_t* in, any_t* out, int len, int bps,int final);
// From singed to unsigned
static void si2us(any_t* in, any_t* out, int len, int bps,int final);
// From unsinged to signed
static void us2si(any_t* in, any_t* out, int len, int bps,int final);

static const struct fmt_alias_s
{
    const char *name;
    unsigned short wtag;
}fmt_aliases[]=
{
    { "adpcm", WAVE_FORMAT_ADPCM },
    { "vselp", WAVE_FORMAT_VSELP },
    { "cvsd",  WAVE_FORMAT_IBM_CVSD },
    { "alaw",  WAVE_FORMAT_ALAW },
    { "mulaw", WAVE_FORMAT_MULAW },
    { "dts",   WAVE_FORMAT_DTS },
    { "oki_adpcm", WAVE_FORMAT_OKI_ADPCM },
    { "dvi_adpcm", WAVE_FORMAT_DVI_ADPCM },
    { "ima_adpcm", WAVE_FORMAT_IMA_ADPCM },
    { "mediaspace_adpcm", WAVE_FORMAT_MEDIASPACE_ADPCM },
    { "sierra_adpcm", WAVE_FORMAT_SIERRA_ADPCM },
    { "g723_adpcm", WAVE_FORMAT_G723_ADPCM },
    { "digistd",   WAVE_FORMAT_DIGISTD },
    { "digifix",   WAVE_FORMAT_DIGIFIX },
    { "dialogic_oki_adpcm", WAVE_FORMAT_DIALOGIC_OKI_ADPCM },
    { "mediavision_adpcm", WAVE_FORMAT_MEDIAVISION_ADPCM },
    { "cu_codec", WAVE_FORMAT_CU_CODEC },
    { "yamaha_adpcm", WAVE_FORMAT_YAMAHA_ADPCM },
    { "sonarc", WAVE_FORMAT_SONARC },
    { "dspgroup_truespeech", WAVE_FORMAT_DSPGROUP_TRUESPEECH },
    { "echosc1", WAVE_FORMAT_ECHOSC1 },
    { "audiofile_af36", WAVE_FORMAT_AUDIOFILE_AF36 },
    { "aptx", WAVE_FORMAT_APTX },
    { "audiofile_af10", WAVE_FORMAT_AUDIOFILE_AF10 },
    { "prosody_1612", WAVE_FORMAT_PROSODY_1612 },
    { "lrc", WAVE_FORMAT_LRC },
    { "dolby_ac2", WAVE_FORMAT_DOLBY_AC2 },
    { "gsm610", WAVE_FORMAT_GSM610 },
    { "msnaudio", WAVE_FORMAT_MSNAUDIO },
    { "antex_adpcme", WAVE_FORMAT_ANTEX_ADPCME },
    { "control_res_vqlpc", WAVE_FORMAT_CONTROL_RES_VQLPC },
    { "digireal", WAVE_FORMAT_DIGIREAL },
    { "digiadpcm", WAVE_FORMAT_DIGIADPCM },
    { "control_res_cr10", WAVE_FORMAT_CONTROL_RES_CR10 },
    { "nms_vbxadpcm", WAVE_FORMAT_NMS_VBXADPCM },
    { "cs_imaadpcm", WAVE_FORMAT_CS_IMAADPCM },
    { "echosc3", WAVE_FORMAT_ECHOSC3 },
    { "rockwell_adpcm", WAVE_FORMAT_ROCKWELL_ADPCM },
    { "rockwell_digitalk", WAVE_FORMAT_ROCKWELL_DIGITALK },
    { "xebec", WAVE_FORMAT_XEBEC },
    { "g721_adpcm", WAVE_FORMAT_G721_ADPCM },
    { "g728_celp", WAVE_FORMAT_G728_CELP },
    { "msg723", WAVE_FORMAT_MSG723 },
    { "mp2", WAVE_FORMAT_MPEG },
    { "rt24", WAVE_FORMAT_RT24 },
    { "pac", WAVE_FORMAT_PAC },
    { "mp3", WAVE_FORMAT_MPEGLAYER3 },
    { "lucent_g723", WAVE_FORMAT_LUCENT_G723 },
    { "cirrus", WAVE_FORMAT_CIRRUS },
    { "espcm", WAVE_FORMAT_ESPCM },
    { "voxware", WAVE_FORMAT_VOXWARE },
    { "canopus_atrac", WAVE_FORMAT_CANOPUS_ATRAC },
    { "g726_adpcm", WAVE_FORMAT_G726_ADPCM },
    { "g722_adpcm", WAVE_FORMAT_G722_ADPCM },
    { "dsat_display", WAVE_FORMAT_DSAT_DISPLAY },
    { "voxware_byte_aligned", WAVE_FORMAT_VOXWARE_BYTE_ALIGNED },
    { "voxware_ac8", WAVE_FORMAT_VOXWARE_AC8 },
    { "voxware_ac10", WAVE_FORMAT_VOXWARE_AC10 },
    { "voxware_ac16", WAVE_FORMAT_VOXWARE_AC16 },
    { "voxware_ac20", WAVE_FORMAT_VOXWARE_AC20 },
    { "voxware_rt24", WAVE_FORMAT_VOXWARE_RT24 },
    { "voxware_rt29", WAVE_FORMAT_VOXWARE_RT29 },
    { "voxware_rt29hw", WAVE_FORMAT_VOXWARE_RT29HW },
    { "voxware_vr12", WAVE_FORMAT_VOXWARE_VR12 },
    { "voxware_vr18", WAVE_FORMAT_VOXWARE_VR18 },
    { "voxware_tq40", WAVE_FORMAT_VOXWARE_TQ40 },
    { "softsound", WAVE_FORMAT_SOFTSOUND },
    { "voxware_tq60", WAVE_FORMAT_VOXWARE_TQ60 },
    { "msrt24", WAVE_FORMAT_MSRT24 },
    { "g729a", WAVE_FORMAT_G729A },
    { "mvi2", WAVE_FORMAT_MVI_MVI2 },
    { "df_g726", WAVE_FORMAT_DF_G726 },
    { "df_gsm610", WAVE_FORMAT_DF_GSM610 },
    { "isiaudio", WAVE_FORMAT_ISIAUDIO },
    { "onlive", WAVE_FORMAT_ONLIVE },
    { "sbc24", WAVE_FORMAT_SBC24 },
    { "dolby_ac3_spdif", WAVE_FORMAT_DOLBY_AC3_SPDIF },
    { "mediasonic_g723", WAVE_FORMAT_MEDIASONIC_G723 },
    { "prosody_8k", WAVE_FORMAT_PROSODY_8KBPS },
    { "zyxel_adpcm", WAVE_FORMAT_ZYXEL_ADPCM },
    { "philips_lpcbb", WAVE_FORMAT_PHILIPS_LPCBB },
    { "packed", WAVE_FORMAT_PACKED },
    { "malden_phonytalk", WAVE_FORMAT_MALDEN_PHONYTALK },
    { "phetorex_adpcm", WAVE_FORMAT_RHETOREX_ADPCM },
    { "irat", WAVE_FORMAT_IRAT },
    { "vivo_g723", WAVE_FORMAT_VIVO_G723 },
    { "vivo_siren", WAVE_FORMAT_VIVO_SIREN },
    { "digital_g723", WAVE_FORMAT_DIGITAL_G723 },
    { "sanyo_ld_adpcm", WAVE_FORMAT_SANYO_LD_ADPCM },
    { "siprolab_acelpnet", WAVE_FORMAT_SIPROLAB_ACEPLNET },
    { "siprolab_acelp4800", WAVE_FORMAT_SIPROLAB_ACELP4800 },
    { "siprolab_acelp8v3", WAVE_FORMAT_SIPROLAB_ACELP8V3 },
    { "siprolab_g729", WAVE_FORMAT_SIPROLAB_G729 },
    { "siprolab_g729a", WAVE_FORMAT_SIPROLAB_G729A },
    { "siprolab_kelvin", WAVE_FORMAT_SIPROLAB_KELVIN },
    { "g726adpcm", WAVE_FORMAT_G726ADPCM },
    { "qualcomm_purevoice", WAVE_FORMAT_QUALCOMM_PUREVOICE },
    { "qualcomm_halfrate", WAVE_FORMAT_QUALCOMM_HALFRATE },
    { "tubgsm", WAVE_FORMAT_TUBGSM },
    { "msaudio1", WAVE_FORMAT_MSAUDIO1 },
    { "creative_adpcm", WAVE_FORMAT_CREATIVE_ADPCM },
    { "creative_fastspeech8", WAVE_FORMAT_CREATIVE_FASTSPEECH8 },
    { "creative_fastspeech10", WAVE_FORMAT_CREATIVE_FASTSPEECH10 },
    { "uher_adpcm", WAVE_FORMAT_UHER_ADPCM },
    { "quarterdeck", WAVE_FORMAT_QUARTERDECK },
    { "ilink_vc", WAVE_FORMAT_ILINK_VC },
    { "raw_sport", WAVE_FORMAT_RAW_SPORT },
    { "ipi_hsx", WAVE_FORMAT_IPI_HSX },
    { "ipi_rpelp", WAVE_FORMAT_IPI_RPELP },
    { "cs2", WAVE_FORMAT_CS2 },
    { "sony_scx", WAVE_FORMAT_SONY_SCX },
    { "fm_towns_snd", WAVE_FORMAT_FM_TOWNS_SND },
    { "btv_digital", WAVE_FORMAT_BTV_DIGITAL },
    { "qdesign_music", WAVE_FORMAT_QDESIGN_MUSIC },
    { "vme_vmpcm", WAVE_FORMAT_VME_VMPCM },
    { "tpc", WAVE_FORMAT_TPC },
    { "oligsm", WAVE_FORMAT_OLIGSM },
    { "oliadpcm", WAVE_FORMAT_OLIADPCM },
    { "olicelp", WAVE_FORMAT_OLICELP },
    { "olisbc", WAVE_FORMAT_OLISBC },
    { "oliopr", WAVE_FORMAT_OLIOPR },
    { "lh_codec", WAVE_FORMAT_LH_CODEC },
    { "norris", WAVE_FORMAT_NORRIS },
    { "soundspace_musicompress", WAVE_FORMAT_SOUNDSPACE_MUSICOMPRESS },
    { "ac3", WAVE_FORMAT_DVM }
};

// Convert from string to format
int str2fmt(const char *str,int *bps)
{
    int retval;
    char val[3];
    retval=0;
    *bps=1;
    if(strlen(str)<3) goto bad_fmt;
    /* check for special cases */
    *bps=2;
    if(strncmp(str,"float",5)==0 || strncmp(str,"FLOAT",5)==0) { retval |= AF_FORMAT_F; str = &str[4]; }
    else
    if(str[0]=='S' || str[0]=='s') retval|=AF_FORMAT_SI;
    else
    if(str[0]=='U' || str[0]=='u') retval|=AF_FORMAT_US;
    else goto try_special;
    val[0]=str[1];
    val[1]=str[2];
    val[2]='\0';
    if(strcmp(val,"08")==0) *bps=1;
    else
    if(strcmp(val,"16")==0) *bps=2;
    else
    if(strcmp(val,"24")==0) *bps=3;
    else
    if(strcmp(val,"32")==0) *bps=4;
    else
    if(strcmp(val,"64")==0) *bps=8;
    else goto try_special;
    if(str[3]=='\0')
    {
#ifdef WORDS_BIGENDIAN
	retval|=AF_FORMAT_BE;
#else
	retval|=AF_FORMAT_LE;
#endif
    }
    else
    if(strcmp(&str[3],"LE")==0 || strcmp(&str[3],"le")==0) retval |= AF_FORMAT_LE;
    else
    if(strcmp(&str[3],"BE")==0 || strcmp(&str[3],"be")==0) retval |= AF_FORMAT_BE;
    else goto try_special;
    return retval;
    try_special:
    *bps=1;
    {
	unsigned i;
	for(i=0;i<sizeof(fmt_aliases)/sizeof(struct fmt_alias_s);i++)
	{
	    if(strcasecmp(str,fmt_aliases[i].name)==0) return fmt_aliases[i].wtag<<16;
	}
    }
    bad_fmt:
    MSG_ERR("[af_format] Bad value %s. Examples: S08LE U24BE S32 MP3 AC3\n",str);
    return -1;
}

/* Convert format to str input str is a buffer for the 
   converted string, size is the size of the buffer */
char* fmt2str(int format, unsigned bps, char* str, size_t size)
{
  int i=0;
  // Print endinaness

  if(format & AF_FORMAT_SPECIAL_MASK){
    unsigned short wtag;
    unsigned j;
    wtag = format >> 16;
    for(j=0;j<sizeof(fmt_aliases)/sizeof(struct fmt_alias_s);j++)
    {
	if(fmt_aliases[j].wtag==wtag)
	{
	    i+=snprintf(&str[i],size-i,fmt_aliases[j].name);
	    break;
	}
    }
  }
  else{
      // Type
    if(AF_FORMAT_F == (format & AF_FORMAT_POINT_MASK))
      i+=snprintf(&str[i],size,"FLOAT");
    else{
      if(AF_FORMAT_US == (format & AF_FORMAT_SIGN_MASK))
	i+=snprintf(&str[i],size-i,"U");
      else
	i+=snprintf(&str[i],size-i,"S");
    }
    // size
    i+=snprintf(&str[i],size,"%d",bps*8);
    // endian
    if(AF_FORMAT_LE == (format & AF_FORMAT_END_MASK))
	i+=snprintf(&str[i],size,"LE");
    else
	i+=snprintf(&str[i],size,"BE");
  }
  return str;
}

// Helper functions to check sanity for input arguments

// Sanity check for bytes per sample
static int __FASTCALL__ check_bps(int bps)
{
  if(bps != 4 && bps != 3 && bps != 2 && bps != 1){
    MSG_ERR("[format] The number of bytes per sample" 
	   " must be 1, 2, 3 or 4. Current value is %i \n",bps);
    return AF_ERROR;
  }
  return AF_OK;
}

// Check for unsupported formats
static int __FASTCALL__ check_format(int format)
{
  char buf[256];
  switch(format & AF_FORMAT_SPECIAL_MASK){
  case(AF_FORMAT_MPEG2): 
  case(AF_FORMAT_AC3):
    MSG_ERR("[format] Sample format %s not yet supported \n",
	 fmt2str(format,2,buf,255)); 
    return AF_ERROR;
  }
  return AF_OK;
}

// Initialization and runtime control
static int __FASTCALL__ control(struct af_instance_s* af, int cmd, any_t* arg)
{
  af_format_t* s = af->setup;
  char buf1[256],buf2[256];
  switch(cmd){
  case AF_CONTROL_REINIT:{
    // Make sure this filter isn't redundant 
    if(af->data->format == ((af_data_t*)arg)->format && 
       af->data->bps == ((af_data_t*)arg)->bps)
      return AF_DETACH;

    // Check for errors in configuraton
    if((AF_OK != check_bps(((af_data_t*)arg)->bps)) ||
       (AF_OK != check_format(((af_data_t*)arg)->format)) ||
       (AF_OK != check_bps(af->data->bps)) ||
       (AF_OK != check_format(af->data->format)))
      return AF_ERROR;

    s->fmt = ((af_data_t*)arg)->format;
    s->bps = ((af_data_t*)arg)->bps;
    af->data->rate = ((af_data_t*)arg)->rate;
    af->data->nch  = ((af_data_t*)arg)->nch;
    af->mul.n      = af->data->bps;
    af->mul.d      = ((af_data_t*)arg)->bps;
    return AF_OK;
  }
  case AF_CONTROL_SHOWCONF:
    MSG_INFO("[af_format] Changing sample format %s -> %s\n",
	    fmt2str(s->fmt,s->bps,buf1,255),
	    fmt2str(af->data->format,af->data->bps,buf2,255));
    return AF_OK;
  case AF_CONTROL_COMMAND_LINE:{
    int bps = 2;
    int format = AF_FORMAT_NE;
    // Convert string to format
    format = str2fmt((char *)arg,&bps);
    
    if((AF_OK != af->control(af,AF_CONTROL_FORMAT_BPS | AF_CONTROL_SET,&bps)) ||
       (AF_OK != af->control(af,AF_CONTROL_FORMAT_FMT | AF_CONTROL_SET,&format)))
      return AF_ERROR;
    return AF_OK;
  }
  case AF_CONTROL_FORMAT_BPS | AF_CONTROL_SET:
    // Reinit must be called after this function has been called
    
    // Check for errors in configuraton
    if(AF_OK != check_bps(*(int*)arg))
      return AF_ERROR;

    af->data->bps = *(int*)arg;
    return AF_OK;
  case AF_CONTROL_FORMAT_FMT | AF_CONTROL_SET:
    // Reinit must be called after this function has been called

    // Check for errors in configuraton
    if(AF_OK != check_format(*(int*)arg))
      return AF_ERROR;

    af->data->format = *(int*)arg;
    return AF_OK;
  default: break;
  }
  return AF_UNKNOWN;
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
static af_data_t* __FASTCALL__ play(struct af_instance_s* af, af_data_t* data,int final)
{
  af_data_t*   l   = af->data;	// Local data
  af_data_t*   c   = data;	// Current working data
  int 	       len = c->len/c->bps; // Lenght in samples of current audio block

  if(AF_OK != RESIZE_LOCAL_BUFFER(af,data))
    return NULL;

  // Change to cpu native endian format
  if((c->format&AF_FORMAT_END_MASK)!=AF_FORMAT_NE)
    endian(c->audio,c->audio,len,c->bps,final);

  // Conversion table
  switch(c->format & ~AF_FORMAT_END_MASK){
  case(AF_FORMAT_MU_LAW):
    from_ulaw(c->audio, l->audio, len, l->bps, l->format&AF_FORMAT_POINT_MASK);
    if(AF_FORMAT_A_LAW == (l->format&AF_FORMAT_SPECIAL_MASK))
      to_ulaw(l->audio, l->audio, len, 1, AF_FORMAT_SI);
    if((l->format&AF_FORMAT_SIGN_MASK) == AF_FORMAT_US)
      si2us(l->audio,l->audio,len,l->bps,final);
    break;
  case(AF_FORMAT_A_LAW):
    from_alaw(c->audio, l->audio, len, l->bps, l->format&AF_FORMAT_POINT_MASK);
    if(AF_FORMAT_A_LAW == (l->format&AF_FORMAT_SPECIAL_MASK))
      to_alaw(l->audio, l->audio, len, 1, AF_FORMAT_SI);
    if((l->format&AF_FORMAT_SIGN_MASK) == AF_FORMAT_US)
      si2us(l->audio,l->audio,len,l->bps,final);
    break;
  case(AF_FORMAT_F):
    switch(l->format&AF_FORMAT_SPECIAL_MASK){
    case(AF_FORMAT_MU_LAW):
      to_ulaw(c->audio, l->audio, len, c->bps, c->format&AF_FORMAT_POINT_MASK);
      break;
    case(AF_FORMAT_A_LAW):
      to_alaw(c->audio, l->audio, len, c->bps, c->format&AF_FORMAT_POINT_MASK);
      break;
    default:
      if((l->format&AF_FORMAT_SIGN_MASK) == AF_FORMAT_US) {
	float2int(c->audio, l->audio, len, l->bps,0);
	si2us(l->audio,l->audio,len,l->bps,final);
      }
      else
	float2int(c->audio, l->audio, len, l->bps,final);
      break;
    }
    break;
  default:
    // Input must be int
    
    // Change signed/unsigned
    if((c->format&AF_FORMAT_SIGN_MASK) != (l->format&AF_FORMAT_SIGN_MASK)){
      if((c->format&AF_FORMAT_SIGN_MASK) == AF_FORMAT_US)
	us2si(c->audio,c->audio,len,c->bps,final);
      else
	si2us(c->audio,c->audio,len,c->bps,final); 
    }
    // Convert to special formats
    switch(l->format&(AF_FORMAT_SPECIAL_MASK|AF_FORMAT_POINT_MASK)){
    case(AF_FORMAT_MU_LAW):
      to_ulaw(c->audio, l->audio, len, c->bps, c->format&AF_FORMAT_POINT_MASK);
      break;
    case(AF_FORMAT_A_LAW):
      to_alaw(c->audio, l->audio, len, c->bps, c->format&AF_FORMAT_POINT_MASK);
      break;
    case(AF_FORMAT_F):
      int2float(c->audio, l->audio, len, c->bps,final);
      break;
    default:
      // Change the number of bits
      if(c->bps != l->bps)
	change_bps(c->audio,l->audio,len,c->bps,l->bps,final);
      else
	l->audio=c->audio;
      break;
    }
    break;
  }

  // Switch from cpu native endian to the correct endianess 
  if((l->format&AF_FORMAT_END_MASK)!=AF_FORMAT_NE)
    endian(l->audio,l->audio,len,l->bps,final);

  // Set output data
  c->audio  = l->audio;
  c->len    = len*l->bps;
  c->bps    = l->bps;
  c->format = l->format;
  return c;
}

// Allocate memory and set function pointers
static int __FASTCALL__ open(af_instance_t* af){
  af->control=control;
  af->uninit=uninit;
  af->play=play;
  af->mul.n=1;
  af->mul.d=1;
  af->data=mp_calloc(1,sizeof(af_data_t));
  af->setup=mp_calloc(1,sizeof(af_format_t));
  if(af->data == NULL)
    return AF_ERROR;
  return AF_OK;
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
    case(2):{
      for(i=0;i<len;i++){
	((uint16_t*)out)[i]=bswap_16(((uint16_t*)in)[i]);
      }
      break;
    }
    case(3):{
      register uint8_t s;
      for(i=0;i<len;i++){
	s=((uint8_t*)in)[3*i];
	((uint8_t*)out)[3*i]=((uint8_t*)in)[3*i+2];
	if (in != out)
	  ((uint8_t*)out)[3*i+1]=((uint8_t*)in)[3*i+1];
	((uint8_t*)out)[3*i+2]=s;
      }
      break;
    }
    case(4):{
      for(i=0;i<len;i++){
	((uint32_t*)out)[i]=bswap_32(((uint32_t*)in)[i]);
      }
      break;
    }
  }
}

static void si2us(any_t* in, any_t* out, int len, int bps,int final)
{
  register int i;
  switch(bps){
  case(1):
    for(i=0;i<len;i++)
      ((uint8_t*)out)[i]=(uint8_t)(SCHAR_MAX+((int)((int8_t*)in)[i]));
    break;
  case(2):
    for(i=0;i<len;i++)
      ((uint16_t*)out)[i]=(uint16_t)(SHRT_MAX+((int)((int16_t*)in)[i]));
    break;
  case(3):
    for(i=0;i<len;i++)
      store24bit(out, i, (uint32_t)(INT_MAX+(int32_t)load24bit(in, i)));
    break;
  case(4):
    for(i=0;i<len;i++)
      ((uint32_t*)out)[i]=(uint32_t)(INT_MAX+((int32_t*)in)[i]);
    break;
  }
}

static void us2si(any_t* in, any_t* out, int len, int bps,int final)
{
  register int i;
  switch(bps){
  case(1):
    for(i=0;i<len;i++)
      ((int8_t*)out)[i]=(int8_t)(SCHAR_MIN+((int)((uint8_t*)in)[i]));
    break;
  case(2):
    for(i=0;i<len;i++)
      ((int16_t*)out)[i]=(int16_t)(SHRT_MIN+((int)((uint16_t*)in)[i]));
    break;
  case(3):
    for(i=0;i<len;i++)
      store24bit(out, i, (int32_t)(INT_MIN+(uint32_t)load24bit(in, i)));
    break;
  case(4):
    for(i=0;i<len;i++)
      ((int32_t*)out)[i]=(int32_t)(INT_MIN+((uint32_t*)in)[i]);
    break;
  }	
}
