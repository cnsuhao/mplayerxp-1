#include "af.h"
#include "../libao2/afmt.h"

#define AFMT_AF_FLAGS             0x70000000


/* Decodes the format from mplayer format to libaf format */
int __FASTCALL__ af_format_decode(int ifmt,unsigned *bps)
{
  int ofmt = ~0;
  // Check input ifmt
  *bps=2;
  switch(ifmt){
  case(AFMT_U8):
    ofmt = AF_FORMAT_LE|AF_FORMAT_US;
    *bps=1;
    break;
  case(AFMT_S8):
    ofmt = AF_FORMAT_LE|AF_FORMAT_SI;
    *bps=1;
    break;
  case(AFMT_S16_LE):
    ofmt = AF_FORMAT_LE|AF_FORMAT_SI;
    *bps=2;
    break;
  case(AFMT_S16_BE):
    ofmt = AF_FORMAT_BE|AF_FORMAT_SI;
    *bps=2;
    break;
  case(AFMT_U16_LE):	
    ofmt = AF_FORMAT_LE|AF_FORMAT_US;
    *bps=2;
    break;
  case(AFMT_U16_BE):	
    ofmt = AF_FORMAT_BE|AF_FORMAT_US;
    *bps=2;
    break;
  case(AFMT_S24_LE):
    ofmt = AF_FORMAT_LE|AF_FORMAT_SI;
    *bps=3;
    break;
  case(AFMT_S24_BE):	
    ofmt = AF_FORMAT_BE|AF_FORMAT_SI;
    *bps=3;
    break;
  case(AFMT_U24_LE):
    ofmt = AF_FORMAT_LE|AF_FORMAT_US;
    *bps=3;
    break;
  case(AFMT_U24_BE):	
    ofmt = AF_FORMAT_BE|AF_FORMAT_US;
    *bps=3;
    break;
  case(AFMT_S32_LE):
    ofmt = AF_FORMAT_LE|AF_FORMAT_SI;
    *bps=4;
    break;
  case(AFMT_S32_BE):	
    ofmt = AF_FORMAT_BE|AF_FORMAT_SI;
    *bps=4;
    break;
  case(AFMT_U32_LE):
    ofmt = AF_FORMAT_LE|AF_FORMAT_US;
    *bps=4;
    break;
  case(AFMT_U32_BE):	
    ofmt = AF_FORMAT_BE|AF_FORMAT_US;
    *bps=4;
    break;
  case(AFMT_IMA_ADPCM):
    ofmt = AF_FORMAT_IMA_ADPCM;
    *bps=1;
    break;
  case(AFMT_MU_LAW):
    ofmt = AF_FORMAT_MU_LAW;
    *bps=1;
    break;
  case(AFMT_A_LAW):
    ofmt = AF_FORMAT_A_LAW;
    *bps=1;
    break;
  case(AFMT_MPEG):
    ofmt = AF_FORMAT_MPEG2;
    *bps=1;
    break;
  case(AFMT_AC3):
    ofmt = AF_FORMAT_AC3;
    *bps=1;
    break;
  case(AFMT_FLOAT32):
    ofmt = AF_FORMAT_F | AF_FORMAT_NE;
    *bps=4;
    break;
  default: 
    if ((ifmt & AFMT_AF_FLAGS) == AFMT_AF_FLAGS) {
      ofmt = ifmt & ~AFMT_AF_FLAGS;
      break;
    }
    //This can not happen .... 
    MSG_FATAL("[af_mp] Unrecognized input audio format %i\n",ifmt);
    break;
  }
  return ofmt;
}

/* Encodes the format from libaf format to mplayer (OSS) format */
int __FASTCALL__ af_format_encode(void* fmtp)
{
  af_data_t* fmt=(af_data_t*) fmtp;
  switch(fmt->format&AF_FORMAT_SPECIAL_MASK){
  case 0: // PCM:
    if((fmt->format&AF_FORMAT_POINT_MASK)==AF_FORMAT_I){
      if((fmt->format&AF_FORMAT_SIGN_MASK)==AF_FORMAT_SI){
        // signed int PCM:
        switch(fmt->bps){
          case 1: return AFMT_S8;
          case 2: return (fmt->format&AF_FORMAT_LE) ? AFMT_S16_LE : AFMT_S16_BE;
          case 3: return (fmt->format&AF_FORMAT_LE) ? AFMT_S24_LE : AFMT_S24_BE;
          case 4: return (fmt->format&AF_FORMAT_LE) ? AFMT_S32_LE : AFMT_S32_BE;
	}
      } else {
        // unsigned int PCM:
        switch(fmt->bps){
          case 1: return AFMT_U8;
          case 2: return (fmt->format&AF_FORMAT_LE) ? AFMT_U16_LE : AFMT_U16_BE;
          case 3: return (fmt->format&AF_FORMAT_LE) ? AFMT_U24_LE : AFMT_U24_BE;
          case 4: return (fmt->format&AF_FORMAT_LE) ? AFMT_U32_LE : AFMT_U32_BE;
	}
      }
    } else {
      // float PCM:
      return AFMT_FLOAT32; // FIXME?
    }
    break;
  case AF_FORMAT_MU_LAW: return AFMT_MU_LAW;
  case AF_FORMAT_A_LAW:  return AFMT_A_LAW;
  case AF_FORMAT_MPEG2:  return AFMT_MPEG;
  case AF_FORMAT_AC3:    return AFMT_AC3;
  case AF_FORMAT_IMA_ADPCM: return AFMT_IMA_ADPCM;
  }
  return (fmt->format | AFMT_AF_FLAGS);
}

