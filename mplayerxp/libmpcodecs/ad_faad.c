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
#include "config.h"
#include "ad_internal.h"
#include "../mplayer.h"
#include "../../codecs/libfaad2/faad.h"
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
	"faad2"
};

static const config_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

LIBAD_EXTERN(faad)

typedef struct faad_priv_s
{
  float pts;
}faad_priv_t;

/* configure maximum supported channels, *
 * this is theoretically max. 64 chans   */
#define FAAD_MAX_CHANNELS 6
#define FAAD_BUFFLEN (FAAD_MIN_STREAMSIZE*FAAD_MAX_CHANNELS)		       

static faacDecHandle (*FAADAPI faacDecOpen_ptr)(unsigned);
#define faacDecOpen(a) (*faacDecOpen_ptr)(a)
static faacDecConfigurationPtr (*FAADAPI faacDecGetCurrentConfiguration_ptr)(faacDecHandle hDecoder);
#define faacDecGetCurrentConfiguration(a) (*faacDecGetCurrentConfiguration_ptr)(a)
static unsigned char FAADAPI (*faacDecSetConfiguration_ptr)(faacDecHandle hDecoder,
                                    faacDecConfigurationPtr config);
#define faacDecSetConfiguration(a,b) (*faacDecSetConfiguration_ptr)(a,b)
static long (*FAADAPI faacDecInit_ptr)(faacDecHandle hDecoder,
                        unsigned char *buffer,
                        unsigned long buffer_size,
                        unsigned long *samplerate,
                        unsigned char *channels);
#define faacDecInit(a,b,c,d,e) (*faacDecInit_ptr)(a,b,c,d,e)
static char (*FAADAPI faacDecInit2_ptr)(faacDecHandle hDecoder, unsigned char *pBuffer,
                         unsigned long SizeOfDecoderSpecificInfo,
                         unsigned long *samplerate, unsigned char *channels);
#define faacDecInit2(a,b,c,d,e) (*faacDecInit2_ptr)(a,b,c,d,e)

static void (*FAADAPI faacDecClose_ptr)(faacDecHandle hDecoder);
#define faacDecClose(a) (*faacDecClose_ptr)(a)
static void* (*FAADAPI faacDecDecode_ptr)(faacDecHandle hDecoder,
                            faacDecFrameInfo *hInfo,
                            unsigned char *buffer,
                            unsigned long buffer_size);
#define faacDecDecode(a,b,c,d) (*faacDecDecode_ptr)(a,b,c,d)
static char* (*FAADAPI faacDecGetErrorMessage_ptr)(unsigned char errcode);
#define faacDecGetErrorMessage(a) (*faacDecGetErrorMessage_ptr)(a)

//#define AAC_DUMP_COMPRESSED  

static faacDecHandle faac_hdec;
static faacDecFrameInfo faac_finfo;

static void *dll_handle;
static int load_dll(const char *libname)
{
  if(!(dll_handle=ld_codec(libname,NULL))) return 0;
  faacDecOpen_ptr = ld_sym(dll_handle,"faacDecOpen");
  faacDecGetCurrentConfiguration_ptr = ld_sym(dll_handle,"faacDecGetCurrentConfiguration");
  faacDecSetConfiguration_ptr = ld_sym(dll_handle,"faacDecSetConfiguration");
  faacDecInit_ptr = ld_sym(dll_handle,"faacDecInit");
  faacDecInit2_ptr = ld_sym(dll_handle,"faacDecInit2");
  faacDecClose_ptr = ld_sym(dll_handle,"faacDecClose");
  faacDecDecode_ptr = ld_sym(dll_handle,"faacDecDecode");
  faacDecGetErrorMessage_ptr = ld_sym(dll_handle,"faacDecGetErrorMessage");
  return faacDecOpen_ptr && faacDecGetCurrentConfiguration_ptr &&
	faacDecInit_ptr && faacDecInit2_ptr && faacDecGetCurrentConfiguration_ptr &&
	faacDecClose_ptr && faacDecDecode_ptr && faacDecGetErrorMessage_ptr;

}


static int preinit(sh_audio_t *sh)
{
  sh->audio_out_minsize=8192*FAAD_MAX_CHANNELS;
  sh->audio_in_minsize=FAAD_BUFFLEN;
  if(!(sh->context=malloc(sizeof(faad_priv_t)))) return 0;
  return load_dll(codec_name("libfaad2"SLIBSUFFIX));
}

static int init(sh_audio_t *sh)
{
  unsigned long faac_samplerate;
  unsigned char faac_channels;
  float pts;
  int faac_init;
  faacDecConfigurationPtr faac_conf;
  if(!(faac_hdec = faacDecOpen(mplayer_accel)))
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
  faac_conf = faacDecGetCurrentConfiguration(faac_hdec);
  if(sh->samplerate) faac_conf->defSampleRate = sh->samplerate;
#ifdef WORDS_BIGENDIAN
#define FAAC_FMT32 AFMT_S32_BE
#define FAAC_FMT24 AFMT_S24_BE
#define FAAC_FMT16 AFMT_S16_BE
#else
#define FAAC_FMT32 AFMT_S32_LE
#define FAAC_FMT24 AFMT_S24_LE
#define FAAC_FMT16 AFMT_S16_LE
#endif
  /* Set the maximal quality */
  /* This is useful for expesive audio cards */
  if(af_query_fmt(sh->afilter,AFMT_FLOAT32) == CONTROL_OK ||
     af_query_fmt(sh->afilter,FAAC_FMT32) == CONTROL_OK ||
     af_query_fmt(sh->afilter,FAAC_FMT24) == CONTROL_OK)
  {
    sh->samplesize=4;
    sh->sample_format=AFMT_FLOAT32;
    faac_conf->outputFormat=FAAD_FMT_FLOAT;
  }
  else
  {
    sh->samplesize=2;
    sh->sample_format=FAAC_FMT16;
    faac_conf->outputFormat=FAAD_FMT_16BIT;
  }
  /* Set the default object type and samplerate */
  faacDecSetConfiguration(faac_hdec, faac_conf);
  if(!sh->codecdata_len) {

    sh->a_in_buffer_len = demux_read_data_r(sh->ds, sh->a_in_buffer, sh->a_in_buffer_size,&pts);

    /* init the codec */
    faac_init = faacDecInit(faac_hdec, sh->a_in_buffer,
       sh->a_in_buffer_len, &faac_samplerate, &faac_channels);

    sh->a_in_buffer_len -= (faac_init > 0)?faac_init:0; // how many bytes init consumed
    // XXX FIXME: shouldn't we memcpy() here in a_in_buffer ?? --A'rpi

  } else { // We have ES DS in codecdata
#if 0
    int i;
    for(i = 0; i < sh->codecdata_len; i++)
      printf("codecdata_dump[%d]=0x%02X\n", i, sh->codecdata[i]);
#endif

    faac_init = faacDecInit2(faac_hdec, sh->codecdata,
       sh->codecdata_len,	&faac_samplerate, &faac_channels);
  }
  if(faac_init < 0) {
    MSG_WARN("FAAD: Failed to initialize the decoder!\n"); // XXX: deal with cleanup!
    faacDecClose(faac_hdec);
    // XXX: free a_in_buffer here or in uninit?
    return 0;
  } else {
    faac_conf = faacDecGetCurrentConfiguration(faac_hdec);
    sh->channels = faac_channels;
    sh->samplerate = faac_samplerate;
    switch(faac_conf->outputFormat){
	default:
	case FAAD_FMT_16BIT_DITHER:
	case FAAD_FMT_16BIT_L_SHAPE:
	case FAAD_FMT_16BIT_M_SHAPE:
	case FAAD_FMT_16BIT_H_SHAPE:
	case FAAD_FMT_16BIT: sh->samplesize=2; break;
	case FAAD_FMT_24BIT: sh->samplesize=3; break;
	case FAAD_FMT_FLOAT:
	case FAAD_FMT_32BIT: sh->samplesize=4; break;
	case FAAD_FMT_DOUBLE: sh->samplesize=8; break;
    }
    MSG_V("FAAD: Decoder init done (%dBytes)!\n", sh->a_in_buffer_len); // XXX: remove or move to debug!
    MSG_V("FAAD: Negotiated samplerate: %dHz  channels: %d bps: %d\n", faac_samplerate, faac_channels,sh->samplesize);
    //sh->o_bps = sh->samplesize*faac_channels*faac_samplerate;
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
  faacDecClose(faac_hdec);
  free(sh->context);
}

static int control(sh_audio_t *sh,int cmd,void* arg, ...)
{
  return CONTROL_UNKNOWN;
}

static int decode_audio(sh_audio_t *sh,unsigned char *buf,int minlen,int maxlen,float *pts)
{
  faad_priv_t *priv=sh->context;
  int j = 0, len = 0;	      
  void *faac_sample_buffer;

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
    faac_sample_buffer = faacDecDecode(faac_hdec, &faac_finfo, sh->a_in_buffer+j, sh->a_in_buffer_len);
	
    /* update buffer index after faacDecDecode */
    if(faac_finfo.bytesconsumed >= sh->a_in_buffer_len) {
      sh->a_in_buffer_len=0;
    } else {
      sh->a_in_buffer_len-=faac_finfo.bytesconsumed;
      memcpy(sh->a_in_buffer,&sh->a_in_buffer[faac_finfo.bytesconsumed],sh->a_in_buffer_len);
      priv->pts=FIX_APTS(sh,priv->pts,faac_finfo.bytesconsumed);
    }

    if(faac_finfo.error > 0) {
      MSG_WARN("FAAD: error: %s, trying to resync!\n",
              faacDecGetErrorMessage(faac_finfo.error));
      j++;
    } else
      break;
   } while(j < FAAD_BUFFLEN);	  
  } else {
   // packetized (.mp4) aac stream:
    unsigned char* bufptr=NULL;
    int buflen=ds_get_packet_r(sh->ds, &bufptr,pts);
    if(buflen<=0) break;
    faac_sample_buffer = faacDecDecode(faac_hdec, &faac_finfo, bufptr, buflen);
//    printf("FAAC decoded %d of %d  (err: %d)  \n",faac_finfo.bytesconsumed,buflen,faac_finfo.error);
  }
  
    if(faac_finfo.error > 0) {
      MSG_WARN("FAAD: Failed to decode frame: %s \n",
      faacDecGetErrorMessage(faac_finfo.error));
    } else if (faac_finfo.samples == 0) {
      MSG_DBG2("FAAD: Decoded zero samples!\n");
    } else {
      /* XXX: samples already multiplied by channels! */
      MSG_DBG2("FAAD: Successfully decoded frame (%d Bytes)!\n",
      sh->samplesize*faac_finfo.samples);
      memcpy(buf+len,faac_sample_buffer, sh->samplesize*faac_finfo.samples);
      len += sh->samplesize*faac_finfo.samples;
    //printf("FAAD: buffer: %d bytes  consumed: %d \n", k, faac_finfo.bytesconsumed);
    }
  }
  return len;
}

