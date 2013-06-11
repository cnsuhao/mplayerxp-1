#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
#include <stdio.h>
#include <stdlib.h>
#define __USE_XOPEN 1
#include <unistd.h>
#include <assert.h>
#include <dlfcn.h> /* GLIBC specific. Exists under cygwin too! */
#include "codecs_ld.h"

#include "mplayerxp.h"
#include "mpxp_help.h"
#include "osdep/cpudetect.h"

#include "libdca/dca.h"
#include "osdep/mm_accel.h"
#include "mplayerxp.h"
#include "osdep/bswap.h"
#include "libao3/afmt.h"
#include "libao3/audio_out.h"
#include "postproc/af.h"

#include "libmpdemux/demuxer_r.h"
#include "ad.h"
#include "ad_msg.h"

namespace	usr {
    class dca_decoder : public Audio_Decoder {
	public:
	    dca_decoder(sh_audio_t&,audio_filter_info_t&,uint32_t wtag);
	    virtual ~dca_decoder();

	    virtual unsigned		run(unsigned char *buf,unsigned minlen,unsigned maxlen,float& pts);
	    virtual MPXP_Rc		ctrl(int cmd,any_t* arg);
	    virtual audio_probe_t	get_probe_information() const;
	private:
	    int				fillbuffer(float& pts);
	    int				printinfo() const;

	    float		last_pts;
	    sh_audio_t&		sh;
	    const audio_probe_t*probe;

	    dca_state_t*	mpxp_dca_state;
	    uint32_t		mpxp_dca_accel;
	    uint32_t		mpxp_dca_flags;
	    static const unsigned MAX_AC5_FRAME=4096;
    };

static const audio_probe_t probes[] = {
    { "libdca", "libdca", 0x86,   ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE} },
    { "libdca", "libdca", 0x2001, ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE} },
    { "libdca", "libdca", FOURCC_TAG('D','T','S',' '), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE} },
    { "libdca", "libdca", FOURCC_TAG('D','T','S','B'), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE} },
    { "libdca", "libdca", FOURCC_TAG('D','T','S','C'), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE} },
    { "libdca", "libdca", FOURCC_TAG('D','T','S','E'), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE} },
    { "libdca", "libdca", FOURCC_TAG('D','T','S','H'), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE} },
    { "libdca", "libdca", FOURCC_TAG('D','T','S','L'), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE} },
    { NULL, NULL, 0x0, ACodecStatus_NotWorking, {AFMT_S8}}
};

int dca_decoder::fillbuffer(float& pts){
    int length=0,flen=0;
    int flags=0;
    int sample_rate=0;
    int bit_rate=0;
    float apts=0.,null_pts;
    sh.a_in_buffer_len=0;
    /* sync frame:*/
    while(1){
	while(sh.a_in_buffer_len<16){
	    int c=demux_getc_r(*sh.ds,apts?null_pts:apts);
	    if(c<0) { last_pts=pts=apts; return -1; } /* EOF*/
	    sh.a_in_buffer[sh.a_in_buffer_len++]=c;
	}
	length = dca_syncinfo (mpxp_dca_state,reinterpret_cast<uint8_t*>(sh.a_in_buffer), reinterpret_cast<int *>(&flags), &sample_rate, &bit_rate, &flen);
	if(length>=16 && length<=int(MAX_AC5_FRAME)) break; /* we're done.*/
	/* bad file => resync*/
	memmove(sh.a_in_buffer,sh.a_in_buffer+1,15);
	--sh.a_in_buffer_len;
	apts=0;
    }
    mpxp_dbg2<<"dca["<<std::hex<<std::setfill('0')<<std::setw(8)<<(*((long *)sh.a_in_buffer))<<"]: len="<<length<<" flags=0x"<<std::hex<<flags<<"  "<<sample_rate<<" Hz "<<bit_rate<<" bit/s frame="<<flen<<std::endl;
    sh.rate=sample_rate;
    sh.i_bps=bit_rate/8;
    demux_read_data_r(*sh.ds,reinterpret_cast<unsigned char*>(sh.a_in_buffer+16),length-16,apts?null_pts:apts);
    last_pts=pts=apts;

    return length;
}

/* returns: number of available channels*/
int dca_decoder::printinfo() const {
    int flags, sample_rate, bit_rate,flen;
    const char* mode="unknown";
    int channels=0;
    dca_syncinfo (mpxp_dca_state,reinterpret_cast<uint8_t*>(sh.a_in_buffer), reinterpret_cast<int*>(&flags), &sample_rate, &bit_rate,&flen);
    switch(flags&DCA_CHANNEL_MASK){
	case DCA_CHANNEL: mode="channel"; channels=2; break;
	case DCA_MONO: mode="mono"; channels=1; break;
	case DCA_STEREO: mode="stereo"; channels=2; break;
	case DCA_3F: mode="3f"; channels=3;break;
	case DCA_2F1R: mode="2f+1r"; channels=3;break;
	case DCA_3F1R: mode="3f+1r"; channels=4;break;
	case DCA_2F2R: mode="2f+2r"; channels=4;break;
	case DCA_3F2R: mode="3f+2r"; channels=5;break;
	case DCA_4F2R: mode="4f+2r"; channels=6;break;
	case DCA_DOLBY: mode="dolby"; channels=2; break;
	default: channels=0; break;
    }
    mpxp_info<<"DCA: "<<channels<<"."<<((flags&DCA_LFE)?1:0)<<" ("<<mode<<((flags&DCA_LFE)?"+lfe":"")<<")  "<<sample_rate<<" Hz  "<<bit_rate*0.001f<<" kbit/s Out: "<<(afmt2bps(sh.afmt)*8)<<"-bit"<<std::endl;
    return (flags&DCA_LFE) ? (channels+1) : channels;
}

dca_decoder::dca_decoder(sh_audio_t& _sh,audio_filter_info_t& afi,uint32_t wtag)
	    :Audio_Decoder(_sh,afi,wtag)
	    ,sh(_sh)
{
    unsigned i;
    for(i=0;probes[i].driver;i++)
	if(wtag==probes[i].wtag)
	    probe=&probes[i];
    if(!probe) throw bad_format_exception();
    /*	DTS audio:
	however many channels, 2 bytes in a word, 256 samples in a block, 6 blocks in a frame */
#ifdef WORDS_BIGENDIAN
#define DCA_FMT32 AFMT_S32_BE
#define DCA_FMT24 AFMT_S24_BE
#else
#define DCA_FMT32 AFMT_S32_LE
#define DCA_FMT24 AFMT_S24_LE
#endif
    sh.afmt=bps2afmt(2);
    if(	af_query_fmt(afi.afilter,afmt2mpaf(AFMT_FLOAT32)) == MPXP_Ok||
	af_query_fmt(afi.afilter,afmt2mpaf(DCA_FMT32)) == MPXP_Ok ||
	af_query_fmt(afi.afilter,afmt2mpaf(DCA_FMT24)) == MPXP_Ok) {
	    sh.afmt=AFMT_FLOAT32;
    }
    sh.audio_out_minsize=mp_conf.ao_channels*afmt2bps(sh.afmt)*256*8;
    sh.audio_in_minsize=MAX_AC5_FRAME;

    sample_t level=1, bias=384;
    float pts;
    int flags=0;
    /* Dolby AC3 audio:*/
    mpxp_dca_accel = mpxp_context().mplayer_accel;
    mpxp_dca_state = dca_init(mpxp_dca_accel);
    if (mpxp_dca_state == NULL) throw bad_format_exception();
    if(fillbuffer(pts)<0) throw bad_format_exception();

    /* 'dca cannot upmix' hotfix:*/
    printinfo();
    sh.nch=mp_conf.ao_channels;
    while(sh.nch>0){
	switch(sh.nch){
	    case 1: mpxp_dca_flags=DCA_MONO; break;
	    case 2: mpxp_dca_flags=DCA_STEREO; break;
/*	    case 2: mpxp_dca_flags=DCA_DOLBY; break; */
/*	    case 3: mpxp_dca_flags=DCA_3F; break; */
	    case 3: mpxp_dca_flags=DCA_2F1R; break;
	    case 4: mpxp_dca_flags=DCA_2F2R; break; /* 2+2*/
	    case 5: mpxp_dca_flags=DCA_3F2R; break;
	    case 6: mpxp_dca_flags=DCA_3F2R|DCA_LFE; break; /* 5.1*/
	}
	/* test:*/
	flags=mpxp_dca_flags|DCA_ADJUST_LEVEL;
	mpxp_v<<"dca flags before dca_frame: 0x"<<std::hex<<flags<<std::endl;
	if (dca_frame (mpxp_dca_state, reinterpret_cast<uint8_t*>(sh.a_in_buffer), reinterpret_cast<int*>(&flags), &level, bias))
	    throw bad_format_exception();
	mpxp_v<<"dca flags after dca_frame: 0x"<<std::hex<<flags<<std::endl;
	if(afmt2bps(sh.afmt)==4) {
	    if(dca_resample_init_float(mpxp_dca_state,mpxp_dca_accel,flags,sh.nch)) break;
	} else {
	    if(dca_resample_init(mpxp_dca_state,mpxp_dca_accel,flags,sh.nch)) break;
	}
	--sh.nch; /* try to decrease no. of channels*/
    }
    if(sh.nch<=0) throw bad_format_exception();
}

dca_decoder::~dca_decoder() {}

audio_probe_t dca_decoder::get_probe_information() const { return *probe; }

MPXP_Rc dca_decoder::ctrl(int cmd,any_t* arg)
{
    UNUSED(arg);
    switch(cmd) {
	case ADCTRL_RESYNC_STREAM:
	    sh.a_in_buffer_len=0;   // reset ACM/DShow audio buffer
	    return MPXP_True;
	case ADCTRL_SKIP_FRAME: {
	    float pts;
	    fillbuffer(pts); // skip AC3 frame
	    return MPXP_True;
	}
	default:
	    return MPXP_Unknown;
    }
    return MPXP_Unknown;
}

unsigned dca_decoder::run(unsigned char *buf,unsigned minlen,unsigned maxlen,float& pts)
{
    sample_t level=1, bias=384;
    unsigned i,nblocks,flags=mpxp_dca_flags|DCA_ADJUST_LEVEL;
    unsigned len=0;
    UNUSED(minlen);
    UNUSED(maxlen);
	if(!sh.a_in_buffer_len) {
	    if(fillbuffer(pts)<0) return len; /* EOF */
	}
	else pts=last_pts;
	sh.a_in_buffer_len=0;
	if (dca_frame (mpxp_dca_state, reinterpret_cast<uint8_t *>(sh.a_in_buffer), reinterpret_cast<int *>(&flags), &level, bias)!=0){
	    mpxp_warn<<"dca: error decoding frame"<<std::endl;
	    return len;
	}
//	dca_dynrng(&mpxp_dca_state, NULL, NULL);
	len=0;
	nblocks=dca_blocks_num(mpxp_dca_state);
	for (i = 0; i < nblocks; i++) {
	    if (dca_block (mpxp_dca_state)){
		mpxp_warn<<"dca: error at deblock"<<std::endl;
		break;
	    }
	    if(afmt2bps(sh.afmt)==4)
		len+=4*dca_resample32(dca_samples(mpxp_dca_state),(float *)&buf[len]);
	    else
		len+=2*dca_resample(dca_samples(mpxp_dca_state),(int16_t *)&buf[len]);
	}
  return len;
}

static const mpxp_option_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

static Audio_Decoder* query_interface(sh_audio_t& sh,audio_filter_info_t& afi,uint32_t wtag) { return new(zeromem) dca_decoder(sh,afi,wtag); }

extern const ad_info_t ad_dca_info = {
    "DTS Coherent Acoustics",
    "libdca",
    "Nickols_K",
    "build-in",
    query_interface,
    options
};
} // namespace	usr