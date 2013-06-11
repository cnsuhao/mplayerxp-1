#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
/* ad_faad.c - MPlayer AAC decoder using libfaad2
 * This file is part of MPlayer, see http://mplayerhq.hu/ for info.
 * (c)2002 by Felix Buenemann <atmosfear at users.sourceforge.net>
 * File licensed under the GPL, see http://www.fsf.org/ for more info.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <dlfcn.h> /* GLIBC specific. Exists under cygwin too! */
#include "mpxp_help.h"
#include "osdep/bswap.h"
#include "codecs_ld.h"
#include "mplayerxp.h"
#include "osdep/cpudetect.h"
#include "osdep/mm_accel.h"
#include "libao3/afmt.h"
#include "libao3/audio_out.h"
#include "postproc/af.h"

#include "libmpdemux/demuxer_r.h"
#include "ad.h"
#include "ad_msg.h"

namespace	usr {
    typedef any_t* NeAACDecHandle;
    typedef struct NeAACDecConfiguration {
	unsigned char defObjectType;
	unsigned long defSampleRate;
	unsigned char outputFormat;
	unsigned char downMatrix;
	unsigned char useOldADTSFormat;
	unsigned char dontUpSampleImplicitSBR;
    } NeAACDecConfiguration, *NeAACDecConfigurationPtr;

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
    enum {
	FAAD_FMT_16BIT=1,
	FAAD_FMT_24BIT=2,
	FAAD_FMT_32BIT=3,
	FAAD_FMT_FLOAT=4,
	FAAD_FMT_FIXED=FAAD_FMT_FLOAT,
	FAAD_FMT_DOUBLE=5
    };

    class faad_decoder : public Audio_Decoder {
	public:
	    faad_decoder(sh_audio_t&,audio_filter_info_t&,uint32_t wtag);
	    virtual ~faad_decoder();

	    virtual unsigned		run(unsigned char *buf,unsigned minlen,unsigned maxlen,float& pts);
	    virtual MPXP_Rc		ctrl(int cmd,any_t* arg);
	    virtual audio_probe_t	get_probe_information() const;
	private:
	    MPXP_Rc			load_dll(const std::string& libname);

	    const audio_probe_t*	probe;
	    float			pts;
	    sh_audio_t&			sh;
	    audio_filter_info_t&	afi;
	    any_t*			dll_handle;

	    NeAACDecHandle (*NEAACDECAPI NeAACDecOpen_ptr)(unsigned);
	    NeAACDecConfigurationPtr (*NEAACDECAPI NeAACDecGetCurrentConfiguration_ptr)(NeAACDecHandle hDecoder);
	    unsigned char NEAACDECAPI (*NeAACDecSetConfiguration_ptr)(NeAACDecHandle hDecoder,
				    NeAACDecConfigurationPtr config);
	    long (*NEAACDECAPI NeAACDecInit_ptr)(NeAACDecHandle hDecoder,
			unsigned char *buffer,
			unsigned long buffer_size,
			unsigned long *samplerate,
			unsigned char *channels);
	    char (*NEAACDECAPI NeAACDecInit2_ptr)(NeAACDecHandle hDecoder, unsigned char *pBuffer,
			unsigned long SizeOfDecoderSpecificInfo,
			unsigned long *samplerate, unsigned char *channels);

	    void (*NEAACDECAPI NeAACDecClose_ptr)(NeAACDecHandle hDecoder);
	    any_t* (*NEAACDECAPI NeAACDecDecode_ptr)(NeAACDecHandle hDecoder,
			    NeAACDecFrameInfo *hInfo,
			    unsigned char *buffer,
			    unsigned long buffer_size);
	    char* (*NEAACDECAPI NeAACDecGetErrorMessage_ptr)(unsigned char errcode);
	    NeAACDecHandle	NeAAC_hdec;
	    NeAACDecFrameInfo	NeAAC_finfo;

	    static const int FAAD_MIN_STREAMSIZE=768; /* 6144 bits/channel */
	    /* configure maximum supported channels, *
	     * this is theoretically max. 64 chans   */
	    static const int FAAD_MAX_CHANNELS=6;
	    static const int FAAD_BUFFLEN=(FAAD_MIN_STREAMSIZE*FAAD_MAX_CHANNELS);
    };

static const audio_probe_t probes[] = {
    { "faad", "libfaad"SLIBSUFFIX,  0xFF,   ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE} },
    { "faad", "libfaad"SLIBSUFFIX,  0x4143, ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE} },
    { "faad", "libfaad"SLIBSUFFIX,  0x706D, ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE} },
    { "faad", "libfaad"SLIBSUFFIX,  0xA106, ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE} },
    { "faad", "libfaad"SLIBSUFFIX,  0xAAC0, ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE} },
    { "faad", "libfaad"SLIBSUFFIX,  FOURCC_TAG('A','A','C',' '), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE} },
    { "faad", "libfaad"SLIBSUFFIX,  FOURCC_TAG('A','A','C','P'), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE} },
    { "faad", "libfaad"SLIBSUFFIX,  FOURCC_TAG('M','P','4','A'), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE} },
    { "faad", "libfaad"SLIBSUFFIX,  FOURCC_TAG('M','P','4','L'), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE} },
    { "faad", "libfaad"SLIBSUFFIX,  FOURCC_TAG('M','P','4','A'), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE} },
    { "faad", "libfaad"SLIBSUFFIX,  FOURCC_TAG('R','A','A','C'), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE} },
    { "faad", "libfaad"SLIBSUFFIX,  FOURCC_TAG('R','A','A','P'), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE} },
    { "faad", "libfaad"SLIBSUFFIX,  FOURCC_TAG('V','L','B',' '), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE} },
    { NULL, NULL, 0x0, ACodecStatus_NotWorking, {AFMT_S8}}
};

extern const ad_info_t ad_faad_info;

MPXP_Rc faad_decoder::load_dll(const std::string& libname)
{
    if(!(dll_handle=ld_codec(libname,ad_faad_info.url))) return MPXP_False;
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

faad_decoder::faad_decoder(sh_audio_t& _sh,audio_filter_info_t& _afi,uint32_t wtag)
	    :Audio_Decoder(_sh,_afi,wtag)
	    ,sh(_sh)
	    ,afi(_afi)
{
    unsigned i;
    for(i=0;probes[i].driver;i++)
	if(wtag==probes[i].wtag)
	    probe=&probes[i];
    if(!probe) throw bad_format_exception();

    sh.audio_out_minsize=8192*FAAD_MAX_CHANNELS;
    sh.audio_in_minsize=FAAD_BUFFLEN;
    if(load_dll(probe->codec_dll)!=MPXP_Ok) throw bad_format_exception();

    unsigned long NeAAC_samplerate;
    unsigned char NeAAC_channels;
    int NeAAC_init;
    NeAACDecConfigurationPtr NeAAC_conf;
    if(!(NeAAC_hdec = (*NeAACDecOpen_ptr)(mpxp_context().mplayer_accel))) throw bad_format_exception();

    // If we don't get the ES descriptor, try manual config
    if(!sh.codecdata_len && sh.wf) {
	sh.codecdata_len = sh.wf->cbSize;
	sh.codecdata = (unsigned char*)(sh.wf+1);
	mpxp_dbg2<<"FAAD: codecdata extracted from WAVEFORMATEX"<<std::endl;
    }
    NeAAC_conf = (*NeAACDecGetCurrentConfiguration_ptr)(NeAAC_hdec);
    if(sh.rate) NeAAC_conf->defSampleRate = sh.rate;
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
    if(af_query_fmt(afi.afilter,afmt2mpaf(AFMT_FLOAT32)) == MPXP_Ok ||
	af_query_fmt(afi.afilter,afmt2mpaf(NeAAC_FMT32)) == MPXP_Ok ||
	af_query_fmt(afi.afilter,afmt2mpaf(NeAAC_FMT24)) == MPXP_Ok) {
	    sh.afmt=AFMT_FLOAT32;
	    NeAAC_conf->outputFormat=FAAD_FMT_FLOAT;
    } else {
	sh.afmt=NeAAC_FMT16;
	NeAAC_conf->outputFormat=FAAD_FMT_16BIT;
    }
    /* Set the default object type and samplerate */
    (*NeAACDecSetConfiguration_ptr)(NeAAC_hdec, NeAAC_conf);
    if(!sh.codecdata_len) {
	sh.a_in_buffer_len = demux_read_data_r(*sh.ds, reinterpret_cast<unsigned char*>(sh.a_in_buffer), sh.a_in_buffer_size,pts);
	/* init the codec */
	NeAAC_init = (*NeAACDecInit_ptr)(NeAAC_hdec, reinterpret_cast<unsigned char*>(sh.a_in_buffer),
				sh.a_in_buffer_len, &NeAAC_samplerate,
				&NeAAC_channels);
	sh.a_in_buffer_len -= (NeAAC_init > 0)?NeAAC_init:0; // how many bytes init consumed
	// XXX FIXME: shouldn't we memcpy() here in a_in_buffer ?? --A'rpi
    } else { // We have ES DS in codecdata
	NeAAC_init = (*NeAACDecInit2_ptr)(NeAAC_hdec, sh.codecdata,
				sh.codecdata_len,
				&NeAAC_samplerate,
				&NeAAC_channels);
    }
    if(NeAAC_init < 0) {
	(*NeAACDecClose_ptr)(NeAAC_hdec);
	// XXX: mp_free a_in_buffer here or in uninit?
	throw bad_format_exception();
    } else {
	NeAAC_conf = (*NeAACDecGetCurrentConfiguration_ptr)(NeAAC_hdec);
	sh.nch = NeAAC_channels;
	sh.rate = NeAAC_samplerate;
	switch(NeAAC_conf->outputFormat){
	    default:
	    case FAAD_FMT_16BIT: sh.afmt=bps2afmt(2); break;
	    case FAAD_FMT_24BIT: sh.afmt=bps2afmt(3); break;
	    case FAAD_FMT_FLOAT:
	    case FAAD_FMT_32BIT: sh.afmt=bps2afmt(4); break;
	}
	mpxp_v<<"FAAD: Decoder init done ("<<sh.a_in_buffer_len<<"Bytes)!"<<std::endl; // XXX: remove or move to debug!
	mpxp_v<<"FAAD: Negotiated samplerate: "<<NeAAC_samplerate<<"Hz  channels: "<<NeAAC_channels<<" bps: "<<afmt2bps(sh.afmt)<<std::endl;
	//sh.o_bps = sh.samplesize*NeAAC_channels*NeAAC_samplerate;
	if(!sh.i_bps) {
	    mpxp_warn<<"FAAD: compressed input bitrate missing, assuming 128kbit/s!"<<std::endl;
	    sh.i_bps = 128*1000/8; // XXX: HACK!!! ::atmos
	} else mpxp_v<<"FAAD: got "<<(sh.i_bps*8/1000)<<"kbit/s bitrate from MP4 header!"<<std::endl;
    }
}

faad_decoder::~faad_decoder() {  (*NeAACDecClose_ptr)(NeAAC_hdec); }

audio_probe_t faad_decoder::get_probe_information() const { return *probe; }

MPXP_Rc faad_decoder::ctrl(int cmd,any_t* arg)
{
    UNUSED(cmd);
    UNUSED(arg);
    return MPXP_Unknown;
}

unsigned faad_decoder::run(unsigned char *buf,unsigned minlen,unsigned maxlen,float& _pts)
{
  int j = 0;
  unsigned len = 0;
  any_t*NeAAC_sample_buffer;
  UNUSED(maxlen);
  while(len < minlen) {

    /* update buffer for raw aac streams: */
  if(!sh.codecdata_len)
  {
    if(sh.a_in_buffer_len < sh.a_in_buffer_size){
      sh.a_in_buffer_len +=
	demux_read_data_r(*sh.ds,reinterpret_cast<unsigned char*>(&sh.a_in_buffer[sh.a_in_buffer_len]),
	sh.a_in_buffer_size - sh.a_in_buffer_len,_pts);
	_pts=FIX_APTS(sh,_pts,-sh.a_in_buffer_len);
	_pts=_pts;
    }
    else _pts=_pts;
#ifdef DUMP_AAC_COMPRESSED
    {unsigned i;
    for (i = 0; i < 16; i++)
      printf ("%02X ", sh.a_in_buffer[i]);
    printf ("\n");}
#endif
  }
  if(!sh.codecdata_len){
   // raw aac stream:
   do {
    NeAAC_sample_buffer = (*NeAACDecDecode_ptr)(NeAAC_hdec, &NeAAC_finfo, reinterpret_cast<unsigned char *>(sh.a_in_buffer+j), sh.a_in_buffer_len);

    /* update buffer index after NeAACDecDecode */
    if(NeAAC_finfo.bytesconsumed >= (unsigned)sh.a_in_buffer_len) {
      sh.a_in_buffer_len=0;
    } else {
      sh.a_in_buffer_len-=NeAAC_finfo.bytesconsumed;
      memcpy(sh.a_in_buffer,&sh.a_in_buffer[NeAAC_finfo.bytesconsumed],sh.a_in_buffer_len);
      _pts=FIX_APTS(sh,_pts,NeAAC_finfo.bytesconsumed);
    }

    if(NeAAC_finfo.error > 0) {
      mpxp_err<<"FAAD: error: "<<((*NeAACDecGetErrorMessage_ptr)(NeAAC_finfo.error))<<", trying to resync!"<<std::endl;
      j++;
    } else
      break;
   } while(j < FAAD_BUFFLEN);
  } else {
   // packetized (.mp4) aac stream:
    unsigned char* bufptr=NULL;
    int buflen=ds_get_packet_r(*sh.ds, &bufptr,_pts);
    if(buflen<=0) break;
    NeAAC_sample_buffer = (*NeAACDecDecode_ptr)(NeAAC_hdec, &NeAAC_finfo, bufptr, buflen);
//    printf("NeAAC decoded %d of %d  (err: %d)  \n",NeAAC_finfo.bytesconsumed,buflen,NeAAC_finfo.error);
  }

    if(NeAAC_finfo.error > 0) {
      mpxp_warn<<"FAAD: Failed to decode frame: "<<(*NeAACDecGetErrorMessage_ptr)(NeAAC_finfo.error)<<std::endl;
    } else if (NeAAC_finfo.samples == 0) {
      mpxp_dbg2<<"FAAD: Decoded zero samples!"<<std::endl;
    } else {
      /* XXX: samples already multiplied by channels! */
      mpxp_dbg2<<"FAAD: Successfully decoded frame ("<<(afmt2bps(sh.afmt)*NeAAC_finfo.samples)<<" Bytes)!"<<std::endl;
      memcpy(buf+len,NeAAC_sample_buffer, afmt2bps(sh.afmt)*NeAAC_finfo.samples);
      len += afmt2bps(sh.afmt)*NeAAC_finfo.samples;
    //printf("FAAD: buffer: %d bytes  consumed: %d \n", k, NeAAC_finfo.bytesconsumed);
    }
  }
  return len;
}

static const mpxp_option_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

static Audio_Decoder* query_interface(sh_audio_t& sh,audio_filter_info_t& afi,uint32_t wtag) { return new(zeromem) faad_decoder(sh,afi,wtag); }

extern const ad_info_t ad_faad_info = {
    "AAC (MPEG2/4 Advanced Audio Coding)",
    "faad",
    "Felix Buenemann",
    "http://www.audiocoding.com/faad2.html",
    query_interface,
    options
};
} // namespace	usr
