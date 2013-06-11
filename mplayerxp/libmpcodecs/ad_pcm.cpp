#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "libao3/afmt.h"
#include "osdep/bswap.h"

#include "libmpdemux/demuxer_r.h"
#include "ad.h"
#include "ad_msg.h"

namespace	usr {
    class pcm_decoder : public Audio_Decoder {
	public:
	    pcm_decoder(sh_audio_t&,audio_filter_info_t&,uint32_t wtag);
	    virtual ~pcm_decoder();

	    virtual unsigned		run(unsigned char *buf,unsigned minlen,unsigned maxlen,float& pts);
	    virtual MPXP_Rc		ctrl(int cmd,any_t* arg);
	    virtual audio_probe_t	get_probe_information() const;
	private:
	    sh_audio_t&			sh;
	    audio_filter_info_t&	afi;
	    const audio_probe_t*	probe;
    };

static const audio_probe_t probes[] = {
    { "pcm", "pcm", 0x0,   ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE, AFMT_S8} },
    { "pcm", "pcm", 0x1,   ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE, AFMT_S8} },
    { "pcm", "pcm", 0x3,   ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE, AFMT_S8} },
    { "pcm", "pcm", 0xFFFE,ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE, AFMT_S8} },
    { "pcm", "pcm", FOURCC_TAG('F','L','3','2'), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE, AFMT_S8} },
    { "pcm", "pcm", FOURCC_TAG('2','3','L','F'), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE, AFMT_S8} },
    { "pcm", "pcm", FOURCC_TAG('N','O','N','E'), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE, AFMT_S8} },
    { "pcm", "pcm", FOURCC_TAG('I','N','2','4'), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE, AFMT_S8} },
    { "pcm", "pcm", FOURCC_TAG('4','2','N','I'), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE, AFMT_S8} },
    { "pcm", "pcm", FOURCC_TAG('I','N','3','2'), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE, AFMT_S8} },
    { "pcm", "pcm", FOURCC_TAG('2','3','N','I'), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE, AFMT_S8} },
    { "pcm", "pcm", FOURCC_TAG('L','P','C','M'), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE, AFMT_S8} },
    { "pcm", "pcm", FOURCC_TAG('R','A','W',' '), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE, AFMT_S8} },
    { "pcm", "pcm", FOURCC_TAG('T','W','O','S'), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE, AFMT_S8} },
    { "pcm", "pcm", FOURCC_TAG('S','O','W','T'), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE, AFMT_S8} },
    { NULL, NULL, 0x0, ACodecStatus_NotWorking, {AFMT_S8}}
};

pcm_decoder::pcm_decoder(sh_audio_t& _sh,audio_filter_info_t& _afi,uint32_t wtag)
	    :Audio_Decoder(_sh,_afi,wtag)
	    ,sh(_sh)
	    ,afi(_afi)
{
    unsigned i;
    for(i=0;probes[i].driver;i++)
	if(wtag==probes[i].wtag)
	    probe=&probes[i];
    if(!probe) throw bad_format_exception();

    sh.audio_out_minsize=16384;

    WAVEFORMATEX *h=sh.wf;
    sh.i_bps=h->nAvgBytesPerSec;
    sh.nch=h->nChannels;
    sh.rate=h->nSamplesPerSec;
    sh.afmt=bps2afmt((h->wBitsPerSample+7)/8);
    switch(sh.wtag){ /* hardware formats: */
	case 0x3:  sh.afmt=AFMT_FLOAT32; break;
	case 0x6:  sh.afmt=AFMT_A_LAW;break;
	case 0x7:  sh.afmt=AFMT_MU_LAW;break;
	case 0x11: sh.afmt=AFMT_IMA_ADPCM;break;
	case 0x50: sh.afmt=AFMT_MPEG;break;
/*	case 0x2000: sh.sample_format=AFMT_AC3; */
	case mmioFOURCC('r','a','w',' '): /* 'raw '*/
	    break;
	case mmioFOURCC('t','w','o','s'): /* 'twos'*/
	    if(afmt2bps(sh.afmt)!=1) sh.afmt=AFMT_S16_BE;
	    break;
	case mmioFOURCC('s','o','w','t'): /* 'swot'*/
	    if(afmt2bps(sh.afmt)!=1) sh.afmt=AFMT_S16_LE;
	    break;
	default:
	    if(afmt2bps(sh.afmt)==1);
	    else if(afmt2bps(sh.afmt)==2) sh.afmt=AFMT_S16_LE;
	    else if(afmt2bps(sh.afmt)==3) sh.afmt=AFMT_S24_LE;
	    else sh.afmt=AFMT_S32_LE;
	    break;
    }
}

pcm_decoder::~pcm_decoder() {}

audio_probe_t pcm_decoder::get_probe_information() const { return *probe; }

MPXP_Rc pcm_decoder::ctrl(int cmd,any_t* arg)
{
    int skip;
    UNUSED(arg);
    switch(cmd) {
	case ADCTRL_SKIP_FRAME: {
	    float pts;
	    skip=sh.i_bps/16;
	    skip=skip&(~3);
	    demux_read_data_r(*sh.ds,NULL,skip,pts);
	    return MPXP_True;
	}
	default:
	    return MPXP_Unknown;
    }
    return MPXP_Unknown;
}

unsigned pcm_decoder::run(unsigned char *buf,unsigned minlen,unsigned maxlen,float& pts)
{
    unsigned len = sh.nch*afmt2bps(sh.afmt);
    len = (minlen + len - 1) / len * len;
    if (len > maxlen)
	/* if someone needs hundreds of channels adjust audio_out_minsize based on channels in preinit() */
	return -1;
    len=demux_read_data_r(*sh.ds,buf,len,pts);
    return len;
}

static const mpxp_option_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

static Audio_Decoder* query_interface(sh_audio_t& sh,audio_filter_info_t& afi,uint32_t wtag) { return new(zeromem) pcm_decoder(sh,afi,wtag); }

extern const ad_info_t ad_pcm_info = {
    "Uncompressed PCM audio decoder",
    "pcm",
    "Nickols_K",
    "build-in",
    query_interface,
    options
};
} // namespace	usr