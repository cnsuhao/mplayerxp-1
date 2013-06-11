#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
#include <stdio.h>
#include <stdlib.h>
#define __USE_GNU 1
#define __USE_XOPEN 1
#include <unistd.h>
#include <assert.h>
#include <dlfcn.h> /* GLIBC specific. Exists under cygwin too! */
#include "libao3/afmt.h"
#include "ad_internal.h"

#include "mpxp_help.h"
#include "osdep/bswap.h"
#include "libmpconf/codec-cfg.h"

#include "mpxp_conf_lavc.h"
#include "codecs_ld.h"

namespace	usr {
    class alavc_decoder: public Audio_Decoder {
	public:
	    alavc_decoder(sh_audio_t&,audio_filter_info_t&,uint32_t wtag);
	    virtual ~alavc_decoder();

	    virtual unsigned		run(unsigned char *buf,unsigned minlen,unsigned maxlen,float& pts);
	    virtual MPXP_Rc		ctrl(int cmd,any_t* arg);
	    virtual audio_probe_t	get_probe_information() const;
	private:
	    AVCodecContext*		lavc_ctx;
	    sh_audio_t&			sh;
	    audio_filter_info_t&	afi;
	    audio_probe_t*		probe;
    };

alavc_decoder::alavc_decoder(sh_audio_t& _sh,audio_filter_info_t& _afi,uint32_t wtag)
	    :Audio_Decoder(_sh,_afi,wtag)
	    ,sh(_sh)
	    ,afi(_afi)
{
    unsigned i;
    const char *what="AVCodecID";
    enum AVCodecID id = ff_codec_get_id(ff_codec_wav_tags,wtag);
    if (id <= 0) {
prn_err:
	mpxp_v<<"Cannot find "<<what<<" for '0x"<<std::hex<<wtag<<"' tag! Try force -ac option"<<std::endl;
	throw bad_format_exception();
    }
//  avcodec_init();
    avcodec_register_all();
    AVCodec *codec=avcodec_find_decoder(id);
    if(!codec) { what="AVCodec"; goto prn_err; }

    probe=new(zeromem) audio_probe_t;
    probe->codec_dll=mp_strdup(avcodec_get_name(id));
    probe->driver="lavc";
    probe->wtag=wtag;
    if(codec->sample_fmts)
    for(i=0;i<Audio_MaxOutSample;i++) {
	if(codec->sample_fmts[i]==-1) break;
	probe->sample_fmt[i]=ff_codec_get_tag(ff_codec_wav_tags,id);
    }

    sh.audio_out_minsize=AVCODEC_MAX_AUDIO_FRAME_SIZE;

    int x;
    float pts;
    AVCodec *lavc_codec=NULL;
    mpxp_v<<"LAVC audio codec"<<std::endl;
//  avcodec_init();
    avcodec_register_all();

    lavc_codec = (AVCodec *)avcodec_find_decoder_by_name(sh.codec->dll_name);
    if(!lavc_codec) throw bad_format_exception();

    lavc_ctx = avcodec_alloc_context3(lavc_codec);
    if(sh.wf) {
	lavc_ctx->channels = sh.wf->nChannels;
	lavc_ctx->sample_rate = sh.wf->nSamplesPerSec;
	lavc_ctx->bit_rate = sh.wf->nAvgBytesPerSec * 8;
	lavc_ctx->block_align = sh.wf->nBlockAlign;
	lavc_ctx->bits_per_coded_sample = sh.wf->wBitsPerSample;
	/* alloc extra data */
	if (sh.wf->cbSize > 0) {
	    lavc_ctx->extradata = new uint8_t[sh.wf->cbSize+FF_INPUT_BUFFER_PADDING_SIZE];
	    lavc_ctx->extradata_size = sh.wf->cbSize;
	    memcpy(lavc_ctx->extradata, (char *)sh.wf + sizeof(WAVEFORMATEX),
		    lavc_ctx->extradata_size);
	}
    }
    // for QDM2
    if (sh.codecdata_len && sh.codecdata && !lavc_ctx->extradata) {
	lavc_ctx->extradata = new uint8_t[sh.codecdata_len];
	lavc_ctx->extradata_size = sh.codecdata_len;
	memcpy(lavc_ctx->extradata, (char *)sh.codecdata,
		lavc_ctx->extradata_size);
    }
    lavc_ctx->codec_tag = sh.wtag;
    lavc_ctx->codec_type = lavc_codec->type;
    lavc_ctx->codec_id = lavc_codec->id;
    /* open it */
    if (avcodec_open2(lavc_ctx, lavc_codec, NULL) < 0) throw bad_format_exception();

    mpxp_v<<"INFO: libavcodec init OK!"<<std::endl;

    // Decode at least 1 byte:  (to get header filled)
    x=run(reinterpret_cast<unsigned char*>(sh.a_buffer),1,sh.a_buffer_size,pts);
    if(x>0) sh.a_buffer_len=x;

    sh.nch=lavc_ctx->channels;
    sh.rate=lavc_ctx->sample_rate;
    switch(lavc_ctx->sample_fmt) {
	case AV_SAMPLE_FMT_U8:  ///< unsigned 8 bits
	    sh.afmt=AFMT_U8;
	    break;
	default:
	case AV_SAMPLE_FMT_S16:             ///< signed 16 bits
	    sh.afmt=AFMT_S16_LE;
	    break;
	case AV_SAMPLE_FMT_S32:             ///< signed 32 bits
	    sh.afmt=AFMT_S32_LE;
	    break;
	case AV_SAMPLE_FMT_FLT:             ///< float
	    sh.afmt=AFMT_FLOAT32;
	    break;
    }
    sh.i_bps=lavc_ctx->bit_rate/8;
}

alavc_decoder::~alavc_decoder() {
    if(lavc_ctx) {
	avcodec_close(lavc_ctx);
	if (lavc_ctx->extradata) delete lavc_ctx->extradata;
	delete lavc_ctx;
    }
    delete probe;
}

audio_probe_t alavc_decoder::get_probe_information() const { return *probe; }

MPXP_Rc alavc_decoder::ctrl(int cmd,any_t* arg)
{
    UNUSED(arg);
    switch(cmd){
	case ADCTRL_RESYNC_STREAM:
	    avcodec_flush_buffers(lavc_ctx);
	    return MPXP_True;
	default: break;
    }
    return MPXP_Unknown;
}

unsigned alavc_decoder::run(unsigned char *buf,unsigned minlen,unsigned maxlen,float& pts)
{
    unsigned char *start=NULL;
    int y;
    unsigned len=0;
    float apts=0.,null_pts;
    while(len<minlen){
	AVPacket pkt;
	int len2=maxlen;
	int x=ds_get_packet_r(*sh.ds,&start,apts?null_pts:apts);
	if(x<=0) break; // error
	if(sh.wtag==mmioFOURCC('d','n','e','t')) swab(start,start,x&(~1));
	av_init_packet(&pkt);
	pkt.data = start;
	pkt.size = x;
	y=avcodec_decode_audio3(lavc_ctx,(int16_t*)buf,&len2,&pkt);
	if(y<0) { mpxp_v<<"lavc_audio: error"<<std::endl; break; }
	if(y<x)
	{
	    sh.ds->buffer_roll_back(x-y);  // put back data (HACK!)
	    if(sh.wtag==mmioFOURCC('d','n','e','t'))
		swab(start+y,start+y,(x-y)&~(1));
	}
	if(len2>0){
	  //len=len2;break;
	  if(len==0) len=len2; else len+=len2;
	  buf+=len2;
	}
	mpxp_dbg2<<"Decoded "<<y<<" -> "<<len2<<std::endl;
    }
    pts=apts;
    return len;
}

static const mpxp_option_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

static Audio_Decoder* query_interface(sh_audio_t& sh,audio_filter_info_t& afi,uint32_t wtag) { return new(zeromem) alavc_decoder(sh,afi,wtag); }

extern const ad_info_t ad_lavc_info = {
    "lavc audio decoders",
    "lavc",
    "Nickols_K",
    "build-in",
    query_interface,
    options
};
} // namespace	usr
