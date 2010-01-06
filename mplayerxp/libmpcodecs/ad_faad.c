/* ad_faad.c - MPlayer AAC decoder using libfaad2
 * This file is part of MPlayer, see http://mplayerhq.hu/ for info.  
 * (c)2002 by Felix Buenemann <atmosfear at users.sourceforge.net>
 * File licensed under the GPL, see http://www.fsf.org/ for more info.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <dlfcn.h> /* GLIBC specific. Exists under cygwin too! */
#include "help_mp.h"
#include "bswap.h"
#include "codecs_ld.h"
#include "mp_config.h"
#include "ad_internal.h"
#include "../mplayer.h"
#include "../cpudetect.h"
#include "../mm_accel.h"
#include "libao2/afmt.h"
#include "libao2/audio_out.h"
#include "../postproc/af.h"

static const ad_info_t info =
{
	"AAC (MPEG2/4 Advanced Audio Coding)",
	"faad",
	"Felix Buenemann",
	"http://www.audiocoding.com/faad2.html"
};

static const config_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

LIBAD_EXTERN(faad)

typedef struct faad_priv_s
{
  float pts;
}faad_priv_t;

typedef void *NeAACDecHandle;
typedef struct NeAACDecConfiguration
{
    unsigned char defObjectType;
    unsigned long defSampleRate;
    unsigned char outputFormat;
    unsigned char downMatrix;
    unsigned char useOldADTSFormat;
    unsigned char dontUpSampleImplicitSBR;
} NeAACDecConfiguration, *NeAACDecConfigurationPtr;
struct NeAACDecFrameInfo;
typedef struct NeAACDecFrameInfo
{
    unsigned long bytesconsumed;
    unsigned long samples;
    unsigned char channels;
    unsigned char error;
    unsigned long samplerate;

    /* SBR: 0: off, 1: on; upsample, 2: on; downsampled, 3: off; upsampled */
    unsigned char sbr;

    /* MPEG-4 ObjectType */
    unsigned char object_type;

    /* AAC header type; MP4 will be signalled as RAW also */
    unsigned char header_type;

    /* multichannel configuration */
    unsigned char num_front_channels;
    unsigned char num_side_channels;
    unsigned char num_back_channels;
    unsigned char num_lfe_channels;
    unsigned char channel_position[64];

    /* PS: 0: off, 1: on */
    unsigned char ps;
} NeAACDecFrameInfo;
#define FAAD_MIN_STREAMSIZE 768 /* 6144 bits/channel */
/* configure maximum supported channels, *
 * this is theoretically max. 64 chans   */
#define FAAD_MAX_CHANNELS 6
#define FAAD_BUFFLEN (FAAD_MIN_STREAMSIZE*FAAD_MAX_CHANNELS)
#ifdef _WIN32
  #pragma pack(push, 8)
  #ifndef NEAACDECAPI
    #define NEAACDECAPI __cdecl
  #endif
#else
  #ifndef NEAACDECAPI
    #define NEAACDECAPI
  #endif
#endif
/* library output formats */
#define FAAD_FMT_16BIT  1
#define FAAD_FMT_24BIT  2
#define FAAD_FMT_32BIT  3
#define FAAD_FMT_FLOAT  4
#define FAAD_FMT_FIXED  FAAD_FMT_FLOAT
#define FAAD_FMT_DOUBLE 5


static NeAACDecHandle (*NEAACDECAPI NeAACDecOpen_ptr)(unsigned);
#define NeAACDecOpen(a) (*NeAACDecOpen_ptr)(a)
static NeAACDecConfigurationPtr (*NEAACDECAPI NeAACDecGetCurrentConfiguration_ptr)(NeAACDecHandle hDecoder);
#define NeAACDecGetCurrentConfiguration(a) (*NeAACDecGetCurrentConfiguration_ptr)(a)
static unsigned char NEAACDECAPI (*NeAACDecSetConfiguration_ptr)(NeAACDecHandle hDecoder,
                                    NeAACDecConfigurationPtr config);
#define NeAACDecSetConfiguration(a,b) (*NeAACDecSetConfiguration_ptr)(a,b)
static long (*NEAACDECAPI NeAACDecInit_ptr)(NeAACDecHandle hDecoder,
                        unsigned char *buffer,
                        unsigned long buffer_size,
                        unsigned long *samplerate,
                        unsigned char *channels);
#define NeAACDecInit(a,b,c,d,e) (*NeAACDecInit_ptr)(a,b,c,d,e)
static char (*NEAACDECAPI NeAACDecInit2_ptr)(NeAACDecHandle hDecoder, unsigned char *pBuffer,
                         unsigned long SizeOfDecoderSpecificInfo,
                         unsigned long *samplerate, unsigned char *channels);
#define NeAACDecInit2(a,b,c,d,e) (*NeAACDecInit2_ptr)(a,b,c,d,e)

static void (*NEAACDECAPI NeAACDecClose_ptr)(NeAACDecHandle hDecoder);
#define NeAACDecClose(a) (*NeAACDecClose_ptr)(a)
static void* (*NEAACDECAPI NeAACDecDecode_ptr)(NeAACDecHandle hDecoder,
                            NeAACDecFrameInfo *hInfo,
                            unsigned char *buffer,
                            unsigned long buffer_size);
#define NeAACDecDecode(a,b,c,d) (*NeAACDecDecode_ptr)(a,b,c,d)
static char* (*NEAACDECAPI NeAACDecGetErrorMessage_ptr)(unsigned char errcode);
#define NeAACDecGetErrorMessage(a) (*NeAACDecGetErrorMessage_ptr)(a)

//#define AAC_DUMP_COMPRESSED

static NeAACDecHandle NeAAC_hdec;
static NeAACDecFrameInfo NeAAC_finfo;

static void *dll_handle;
static int load_dll(const char *libname)
{
  if(!(dll_handle=ld_codec(libname,mpcodecs_ad_faad.info->url))) return 0;
  NeAACDecOpen_ptr = ld_sym(dll_handle,"NeAACDecOpen");
  NeAACDecGetCurrentConfiguration_ptr = ld_sym(dll_handle,"NeAACDecGetCurrentConfiguration");
  NeAACDecSetConfiguration_ptr = ld_sym(dll_handle,"NeAACDecSetConfiguration");
  NeAACDecInit_ptr = ld_sym(dll_handle,"NeAACDecInit");
  NeAACDecInit2_ptr = ld_sym(dll_handle,"NeAACDecInit2");
  NeAACDecClose_ptr = ld_sym(dll_handle,"NeAACDecClose");
  NeAACDecDecode_ptr = ld_sym(dll_handle,"NeAACDecDecode");
  NeAACDecGetErrorMessage_ptr = ld_sym(dll_handle,"NeAACDecGetErrorMessage");
  return NeAACDecOpen_ptr && NeAACDecGetCurrentConfiguration_ptr &&
	NeAACDecInit_ptr && NeAACDecInit2_ptr && NeAACDecGetCurrentConfiguration_ptr &&
	NeAACDecClose_ptr && NeAACDecDecode_ptr && NeAACDecGetErrorMessage_ptr;

}


static int preinit(sh_audio_t *sh)
{
  sh->audio_out_minsize=8192*FAAD_MAX_CHANNELS;
  sh->audio_in_minsize=FAAD_BUFFLEN;
  if(!(sh->context=malloc(sizeof(faad_priv_t)))) return 0;
  return load_dll("libfaad"SLIBSUFFIX);
}

static int init(sh_audio_t *sh)
{
  unsigned long NeAAC_samplerate;
  unsigned char NeAAC_channels;
  float pts;
  int NeAAC_init;
  NeAACDecConfigurationPtr NeAAC_conf;
  if(!(NeAAC_hdec = NeAACDecOpen(mplayer_accel)))
  {
    MSG_WARN("FAAD: Failed to open the decoder!\n"); // XXX: deal with cleanup!
    return 0;
  }
  // If we don't get the ES descriptor, try manual config
  if(!sh->codecdata_len && sh->wf) {
    sh->codecdata_len = sh->wf->cbSize;
    sh->codecdata = (char*)(sh->wf+1);
    MSG_DBG2("FAAD: codecdata extracted from WAVEFORMATEX\n");
  }
  NeAAC_conf = NeAACDecGetCurrentConfiguration(NeAAC_hdec);
  if(sh->samplerate) NeAAC_conf->defSampleRate = sh->samplerate;
#ifdef WORDS_BIGENDIAN
#define NeAAC_FMT32 AFMT_S32_BE
#define NeAAC_FMT24 AFMT_S24_BE
#define NeAAC_FMT16 AFMT_S16_BE
#else
#define NeAAC_FMT32 AFMT_S32_LE
#define NeAAC_FMT24 AFMT_S24_LE
#define NeAAC_FMT16 AFMT_S16_LE
#endif
  /* Set the maximal quality */
  /* This is useful for expesive audio cards */
  if(af_query_fmt(sh->afilter,AFMT_FLOAT32) == CONTROL_OK ||
     af_query_fmt(sh->afilter,NeAAC_FMT32) == CONTROL_OK ||
     af_query_fmt(sh->afilter,NeAAC_FMT24) == CONTROL_OK)
  {
    sh->samplesize=4;
    sh->sample_format=AFMT_FLOAT32;
    NeAAC_conf->outputFormat=FAAD_FMT_FLOAT;
  }
  else
  {
    sh->samplesize=2;
    sh->sample_format=NeAAC_FMT16;
    NeAAC_conf->outputFormat=FAAD_FMT_16BIT;
  }
  /* Set the default object type and samplerate */
  NeAACDecSetConfiguration(NeAAC_hdec, NeAAC_conf);
  if(!sh->codecdata_len) {

    sh->a_in_buffer_len = demux_read_data_r(sh->ds, sh->a_in_buffer, sh->a_in_buffer_size,&pts);

    /* init the codec */
    NeAAC_init = NeAACDecInit(NeAAC_hdec, sh->a_in_buffer,
       sh->a_in_buffer_len, &NeAAC_samplerate, &NeAAC_channels);

    sh->a_in_buffer_len -= (NeAAC_init > 0)?NeAAC_init:0; // how many bytes init consumed
    // XXX FIXME: shouldn't we memcpy() here in a_in_buffer ?? --A'rpi

  } else { // We have ES DS in codecdata
#if 0
    int i;
    for(i = 0; i < sh->codecdata_len; i++)
      printf("codecdata_dump[%d]=0x%02X\n", i, sh->codecdata[i]);
#endif

    NeAAC_init = NeAACDecInit2(NeAAC_hdec, sh->codecdata,
       sh->codecdata_len,	&NeAAC_samplerate, &NeAAC_channels);
  }
  if(NeAAC_init < 0) {
    MSG_WARN("FAAD: Failed to initialize the decoder!\n"); // XXX: deal with cleanup!
    NeAACDecClose(NeAAC_hdec);
    // XXX: free a_in_buffer here or in uninit?
    return 0;
  } else {
    NeAAC_conf = NeAACDecGetCurrentConfiguration(NeAAC_hdec);
    sh->channels = NeAAC_channels;
    sh->samplerate = NeAAC_samplerate;
    switch(NeAAC_conf->outputFormat){
	default:
	case FAAD_FMT_16BIT: sh->samplesize=2; break;
	case FAAD_FMT_24BIT: sh->samplesize=3; break;
	case FAAD_FMT_FLOAT:
	case FAAD_FMT_32BIT: sh->samplesize=4; break;
	case FAAD_FMT_DOUBLE: sh->samplesize=8; break;
    }
    MSG_V("FAAD: Decoder init done (%dBytes)!\n", sh->a_in_buffer_len); // XXX: remove or move to debug!
    MSG_V("FAAD: Negotiated samplerate: %dHz  channels: %d bps: %d\n", NeAAC_samplerate, NeAAC_channels,sh->samplesize);
    //sh->o_bps = sh->samplesize*NeAAC_channels*NeAAC_samplerate;
    if(!sh->i_bps) {
      MSG_WARN("FAAD: compressed input bitrate missing, assuming 128kbit/s!\n");
      sh->i_bps = 128*1000/8; // XXX: HACK!!! ::atmos
    } else 
      MSG_V("FAAD: got %dkbit/s bitrate from MP4 header!\n",sh->i_bps*8/1000);
  }  
  return 1;
}

static void uninit(sh_audio_t *sh)
{
  MSG_V("FAAD: Closing decoder!\n");
  NeAACDecClose(NeAAC_hdec);
  free(sh->context);
}

static int control(sh_audio_t *sh,int cmd,void* arg, ...)
{
  UNUSED(sh);
  UNUSED(cmd);
  UNUSED(arg);
  return CONTROL_UNKNOWN;
}

static int decode_audio(sh_audio_t *sh,unsigned char *buf,int minlen,int maxlen,float *pts)
{
  faad_priv_t *priv=sh->context;
  int j = 0, len = 0;
  void *NeAAC_sample_buffer;
  UNUSED(maxlen);
  while(len < minlen) {

    /* update buffer for raw aac streams: */
  if(!sh->codecdata_len)
  {
    if(sh->a_in_buffer_len < sh->a_in_buffer_size){
      sh->a_in_buffer_len +=
	demux_read_data_r(sh->ds,&sh->a_in_buffer[sh->a_in_buffer_len],
	sh->a_in_buffer_size - sh->a_in_buffer_len,pts);
	*pts=FIX_APTS(sh,*pts,-sh->a_in_buffer_len);
	priv->pts=*pts;
    }
    else *pts=priv->pts;
#ifdef DUMP_AAC_COMPRESSED
    {int i;
    for (i = 0; i < 16; i++)
      printf ("%02X ", sh->a_in_buffer[i]);
    printf ("\n");}
#endif
  }
  if(!sh->codecdata_len){
   // raw aac stream:
   do {
    NeAAC_sample_buffer = NeAACDecDecode(NeAAC_hdec, &NeAAC_finfo, sh->a_in_buffer+j, sh->a_in_buffer_len);
	
    /* update buffer index after NeAACDecDecode */
    if(NeAAC_finfo.bytesconsumed >= sh->a_in_buffer_len) {
      sh->a_in_buffer_len=0;
    } else {
      sh->a_in_buffer_len-=NeAAC_finfo.bytesconsumed;
      memcpy(sh->a_in_buffer,&sh->a_in_buffer[NeAAC_finfo.bytesconsumed],sh->a_in_buffer_len);
      priv->pts=FIX_APTS(sh,priv->pts,NeAAC_finfo.bytesconsumed);
    }

    if(NeAAC_finfo.error > 0) {
      MSG_WARN("FAAD: error: %s, trying to resync!\n",
              NeAACDecGetErrorMessage(NeAAC_finfo.error));
      j++;
    } else
      break;
   } while(j < FAAD_BUFFLEN);	  
  } else {
   // packetized (.mp4) aac stream:
    unsigned char* bufptr=NULL;
    int buflen=ds_get_packet_r(sh->ds, &bufptr,pts);
    if(buflen<=0) break;
    NeAAC_sample_buffer = NeAACDecDecode(NeAAC_hdec, &NeAAC_finfo, bufptr, buflen);
//    printf("NeAAC decoded %d of %d  (err: %d)  \n",NeAAC_finfo.bytesconsumed,buflen,NeAAC_finfo.error);
  }
  
    if(NeAAC_finfo.error > 0) {
      MSG_WARN("FAAD: Failed to decode frame: %s \n",
      NeAACDecGetErrorMessage(NeAAC_finfo.error));
    } else if (NeAAC_finfo.samples == 0) {
      MSG_DBG2("FAAD: Decoded zero samples!\n");
    } else {
      /* XXX: samples already multiplied by channels! */
      MSG_DBG2("FAAD: Successfully decoded frame (%d Bytes)!\n",
      sh->samplesize*NeAAC_finfo.samples);
      memcpy(buf+len,NeAAC_sample_buffer, sh->samplesize*NeAAC_finfo.samples);
      len += sh->samplesize*NeAAC_finfo.samples;
    //printf("FAAD: buffer: %d bytes  consumed: %d \n", k, NeAAC_finfo.bytesconsumed);
    }
  }
  return len;
}

