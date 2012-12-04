#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
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
#include "osdep/bswap.h"
#include "codecs_ld.h"
#include "ad_internal.h"
#include "mplayerxp.h"
#include "osdep/cpudetect.h"
#include "osdep/mm_accel.h"
#include "libao2/afmt.h"
#include "libao2/audio_out.h"
#include "postproc/af.h"

static const ad_info_t info = {
    "AAC (MPEG2/4 Advanced Audio Coding)",
    "faad",
    "Felix Buenemann",
    "http://www.audiocoding.com/faad2.html"
};

static const config_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

LIBAD_EXTERN(faad)

struct ad_private_t {
    float pts;
    sh_audio_t* sh;
    audio_filter_info_t* afi;
};

static const audio_probe_t probes[] = {
    { "faad", "libfaad",  0xFF,   ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE} },
    { "faad", "libfaad",  0x4143, ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE} },
    { "faad", "libfaad",  0x706D, ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE} },
    { "faad", "libfaad",  0xA106, ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE} },
    { "faad", "libfaad",  0xAAC0, ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE} },
    { "faad", "libfaad",  FOURCC_TAG('A','A','C',' '), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE} },
    { "faad", "libfaad",  FOURCC_TAG('A','A','C','P'), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE} },
    { "faad", "libfaad",  FOURCC_TAG('M','P','4','A'), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE} },
    { "faad", "libfaad",  FOURCC_TAG('M','P','4','L'), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE} },
    { "faad", "libfaad",  FOURCC_TAG('M','P','4','A'), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE} },
    { "faad", "libfaad",  FOURCC_TAG('R','A','A','C'), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE} },
    { "faad", "libfaad",  FOURCC_TAG('R','A','A','P'), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE} },
    { "faad", "libfaad",  FOURCC_TAG('V','L','B',' '), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE} },
    { NULL, NULL, 0x0, ACodecStatus_NotWorking, {AFMT_S8}}
};

static const audio_probe_t* __FASTCALL__ probe(ad_private_t* ctx,uint32_t wtag) {
    UNUSED(ctx);
    unsigned i;
    for(i=0;probes[i].driver;i++)
	if(wtag==probes[i].wtag)
	    return &probes[i];
    return NULL;
}

typedef any_t*NeAACDecHandle;
typedef struct NeAACDecConfiguration {
    unsigned char defObjectType;
    unsigned long defSampleRate;
    unsigned char outputFormat;
    unsigned char downMatrix;
    unsigned char useOldADTSFormat;
    unsigned char dontUpSampleImplicitSBR;
} NeAACDecConfiguration, *NeAACDecConfigurationPtr;
struct NeAACDecFrameInfo;

typedef struct NeAACDecFrameInfo {
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
static any_t* (*NEAACDECAPI NeAACDecDecode_ptr)(NeAACDecHandle hDecoder,
			    NeAACDecFrameInfo *hInfo,
			    unsigned char *buffer,
			    unsigned long buffer_size);
#define NeAACDecDecode(a,b,c,d) (*NeAACDecDecode_ptr)(a,b,c,d)
static char* (*NEAACDECAPI NeAACDecGetErrorMessage_ptr)(unsigned char errcode);
#define NeAACDecGetErrorMessage(a) (*NeAACDecGetErrorMessage_ptr)(a)

//#define AAC_DUMP_COMPRESSED

static NeAACDecHandle NeAAC_hdec;
static NeAACDecFrameInfo NeAAC_finfo;

static any_t*dll_handle;
static MPXP_Rc load_dll(const char *libname)
{
    if(!(dll_handle=ld_codec(libname,mpcodecs_ad_faad.info->url))) return MPXP_False;
    NeAACDecOpen_ptr = (any_t* (*)(unsigned))ld_sym(dll_handle,"NeAACDecOpen");
    NeAACDecGetCurrentConfiguration_ptr = (NeAACDecConfiguration* (*)(any_t*))ld_sym(dll_handle,"NeAACDecGetCurrentConfiguration");
    NeAACDecSetConfiguration_ptr = (unsigned char (*)(any_t*,NeAACDecConfiguration*))ld_sym(dll_handle,"NeAACDecSetConfiguration");
    NeAACDecInit_ptr = (long (*)(any_t*,unsigned char*,unsigned long,unsigned long*,unsigned char*))ld_sym(dll_handle,"NeAACDecInit");
    NeAACDecInit2_ptr = (char (*)(any_t*,unsigned char *,unsigned long,unsigned long*,unsigned char*))ld_sym(dll_handle,"NeAACDecInit2");
    NeAACDecClose_ptr = (void (*)(any_t*))ld_sym(dll_handle,"NeAACDecClose");
    NeAACDecDecode_ptr = (any_t* (*)(any_t*,NeAACDecFrameInfo*,unsigned char *,unsigned long))ld_sym(dll_handle,"NeAACDecDecode");
    NeAACDecGetErrorMessage_ptr = (char* (*)(unsigned char))ld_sym(dll_handle,"NeAACDecGetErrorMessage");
    return (NeAACDecOpen_ptr && NeAACDecGetCurrentConfiguration_ptr &&
	NeAACDecInit_ptr && NeAACDecInit2_ptr && NeAACDecGetCurrentConfiguration_ptr &&
	NeAACDecClose_ptr && NeAACDecDecode_ptr && NeAACDecGetErrorMessage_ptr)?
	MPXP_Ok:MPXP_False;

}

static ad_private_t* preinit(sh_audio_t *sh,audio_filter_info_t* afi)
{
    sh->audio_out_minsize=8192*FAAD_MAX_CHANNELS;
    sh->audio_in_minsize=FAAD_BUFFLEN;
    ad_private_t* priv = new(zeromem) ad_private_t;
    priv->sh = sh;
    priv->afi = afi;
    if(load_dll("libfaad"SLIBSUFFIX)!=MPXP_Ok) {
	delete priv;
	return NULL;
    }
    return priv;
}

static MPXP_Rc init(ad_private_t *priv)
{
    sh_audio_t* sh = priv->sh;
    unsigned long NeAAC_samplerate;
    unsigned char NeAAC_channels;
    float pts;
    int NeAAC_init;
    NeAACDecConfigurationPtr NeAAC_conf;
    if(!(NeAAC_hdec = NeAACDecOpen(mpxp_context().mplayer_accel))) {
	MSG_WARN("FAAD: Failed to open the decoder!\n"); // XXX: deal with cleanup!
	return MPXP_False;
    }
    // If we don't get the ES descriptor, try manual config
    if(!sh->codecdata_len && sh->wf) {
	sh->codecdata_len = sh->wf->cbSize;
	sh->codecdata = (unsigned char*)(sh->wf+1);
	MSG_DBG2("FAAD: codecdata extracted from WAVEFORMATEX\n");
    }
    NeAAC_conf = NeAACDecGetCurrentConfiguration(NeAAC_hdec);
    if(sh->rate) NeAAC_conf->defSampleRate = sh->rate;
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
    if(af_query_fmt(priv->afi->afilter,afmt2mpaf(AFMT_FLOAT32)) == MPXP_Ok ||
	af_query_fmt(priv->afi->afilter,afmt2mpaf(NeAAC_FMT32)) == MPXP_Ok ||
	af_query_fmt(priv->afi->afilter,afmt2mpaf(NeAAC_FMT24)) == MPXP_Ok) {
	    sh->afmt=AFMT_FLOAT32;
	    NeAAC_conf->outputFormat=FAAD_FMT_FLOAT;
    } else {
	sh->afmt=NeAAC_FMT16;
	NeAAC_conf->outputFormat=FAAD_FMT_16BIT;
    }
    /* Set the default object type and samplerate */
    NeAACDecSetConfiguration(NeAAC_hdec, NeAAC_conf);
    if(!sh->codecdata_len) {
	sh->a_in_buffer_len = demux_read_data_r(sh->ds, reinterpret_cast<unsigned char*>(sh->a_in_buffer), sh->a_in_buffer_size,&pts);
	/* init the codec */
	NeAAC_init = NeAACDecInit(NeAAC_hdec, reinterpret_cast<unsigned char*>(sh->a_in_buffer),
				sh->a_in_buffer_len, &NeAAC_samplerate,
				&NeAAC_channels);
	sh->a_in_buffer_len -= (NeAAC_init > 0)?NeAAC_init:0; // how many bytes init consumed
	// XXX FIXME: shouldn't we memcpy() here in a_in_buffer ?? --A'rpi
    } else { // We have ES DS in codecdata
	NeAAC_init = NeAACDecInit2(NeAAC_hdec, sh->codecdata,
				sh->codecdata_len,
				&NeAAC_samplerate,
				&NeAAC_channels);
    }
    if(NeAAC_init < 0) {
	MSG_WARN("FAAD: Failed to initialize the decoder!\n"); // XXX: deal with cleanup!
	NeAACDecClose(NeAAC_hdec);
	// XXX: mp_free a_in_buffer here or in uninit?
	return MPXP_False;
    } else {
	NeAAC_conf = NeAACDecGetCurrentConfiguration(NeAAC_hdec);
	sh->nch = NeAAC_channels;
	sh->rate = NeAAC_samplerate;
	switch(NeAAC_conf->outputFormat){
	    default:
	    case FAAD_FMT_16BIT: sh->afmt=bps2afmt(2); break;
	    case FAAD_FMT_24BIT: sh->afmt=bps2afmt(3); break;
	    case FAAD_FMT_FLOAT:
	    case FAAD_FMT_32BIT: sh->afmt=bps2afmt(4); break;
	}
	MSG_V("FAAD: Decoder init done (%dBytes)!\n", sh->a_in_buffer_len); // XXX: remove or move to debug!
	MSG_V("FAAD: Negotiated samplerate: %dHz  channels: %d bps: %d\n", NeAAC_samplerate, NeAAC_channels,afmt2bps(sh->afmt));
	//sh->o_bps = sh->samplesize*NeAAC_channels*NeAAC_samplerate;
	if(!sh->i_bps) {
	    MSG_WARN("FAAD: compressed input bitrate missing, assuming 128kbit/s!\n");
	    sh->i_bps = 128*1000/8; // XXX: HACK!!! ::atmos
	} else MSG_V("FAAD: got %dkbit/s bitrate from MP4 header!\n",sh->i_bps*8/1000);
    }
    return MPXP_Ok;
}

static void uninit(ad_private_t *priv)
{
    MSG_V("FAAD: Closing decoder!\n");
    NeAACDecClose(NeAAC_hdec);
    delete priv;
}

static MPXP_Rc control_ad(ad_private_t *priv,int cmd,any_t* arg, ...)
{
    UNUSED(priv);
    UNUSED(cmd);
    UNUSED(arg);
    return MPXP_Unknown;
}

static unsigned decode(ad_private_t *priv,unsigned char *buf,unsigned minlen,unsigned maxlen,float *pts)
{
    sh_audio_t* sh = priv->sh;
  int j = 0;
  unsigned len = 0;
  any_t*NeAAC_sample_buffer;
  UNUSED(maxlen);
  while(len < minlen) {

    /* update buffer for raw aac streams: */
  if(!sh->codecdata_len)
  {
    if(sh->a_in_buffer_len < sh->a_in_buffer_size){
      sh->a_in_buffer_len +=
	demux_read_data_r(sh->ds,reinterpret_cast<unsigned char*>(&sh->a_in_buffer[sh->a_in_buffer_len]),
	sh->a_in_buffer_size - sh->a_in_buffer_len,pts);
	*pts=FIX_APTS(sh,*pts,-sh->a_in_buffer_len);
	priv->pts=*pts;
    }
    else *pts=priv->pts;
#ifdef DUMP_AAC_COMPRESSED
    {unsigned i;
    for (i = 0; i < 16; i++)
      printf ("%02X ", sh->a_in_buffer[i]);
    printf ("\n");}
#endif
  }
  if(!sh->codecdata_len){
   // raw aac stream:
   do {
    NeAAC_sample_buffer = NeAACDecDecode(NeAAC_hdec, &NeAAC_finfo, reinterpret_cast<unsigned char *>(sh->a_in_buffer+j), sh->a_in_buffer_len);

    /* update buffer index after NeAACDecDecode */
    if(NeAAC_finfo.bytesconsumed >= (unsigned)sh->a_in_buffer_len) {
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
	afmt2bps(sh->afmt)*NeAAC_finfo.samples);
      memcpy(buf+len,NeAAC_sample_buffer, afmt2bps(sh->afmt)*NeAAC_finfo.samples);
      len += afmt2bps(sh->afmt)*NeAAC_finfo.samples;
    //printf("FAAD: buffer: %d bytes  consumed: %d \n", k, NeAAC_finfo.bytesconsumed);
    }
  }
  return len;
}

