#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#define __USE_XOPEN 1
#include <unistd.h>
#include <assert.h>
#include <dlfcn.h> /* GLIBC specific. Exists under cygwin too! */
#include "liba52/a52.h"
#include "codecs_ld.h"

#include "mplayerxp.h"
#include "mpxp_help.h"
#include "osdep/cpudetect.h"
#include "osdep/bswap.h"

#include "libmpdemux/demuxer_r.h"
#include "ad.h"
#include "ad_msg.h"

#include "osdep/mm_accel.h"
#include "mplayerxp.h"
#include "libao3/afmt.h"
#include "libao3/audio_out.h"
#include "postproc/af.h"

namespace	usr {
    class a52_decoder : public Audio_Decoder {
	public:
	    a52_decoder(sh_audio_t&,audio_filter_info_t&,uint32_t wtag);
	    virtual ~a52_decoder();

	    virtual unsigned		run(unsigned char *buf,unsigned minlen,unsigned maxlen,float& pts);
	    virtual MPXP_Rc		ctrl(int cmd,any_t* arg);
	    virtual audio_probe_t	get_probe_information() const;
	private:
	    int				printinfo() const;
	    int				fillbuffer(float& pts);

	    a52_state_t*		mpxp_a52_state;
	    uint32_t			mpxp_a52_accel;
	    uint32_t			mpxp_a52_flags;
	    sh_audio_t&			sh;
	    float			last_pts;
	    const audio_probe_t*	probe;
	    static const int MAX_AC3_FRAME=3840;
    };

static const audio_probe_t probes[] = {
    { "liba52", "liba52", 0x2000, ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S16_LE} },
    { "liba52", "liba52", 0x20736D, ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S16_LE} },
    { "liba52", "liba52", FOURCC_TAG('A','C','_','3'), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S16_LE} },
    { "liba52", "liba52", FOURCC_TAG('D','N','E','T'), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S16_LE} },
    { "liba52", "liba52", FOURCC_TAG('S','A','C','3'), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S16_LE} },
    { NULL, NULL, 0x0, ACodecStatus_NotWorking, {AFMT_S8}}
};

int a52_decoder::fillbuffer(float& pts){
    int length=0;
    int flags=0;
    int sample_rate=0;
    int bit_rate=0;
    float apts=0.,null_pts;

    sh.a_in_buffer_len=0;
    /* sync frame:*/
    while(1){
	while(sh.a_in_buffer_len<8){
	    int c=demux_getc_r(*sh.ds,apts?null_pts:apts);
	    if(c<0) { last_pts=pts=apts; return -1; } /* EOF*/
	    sh.a_in_buffer[sh.a_in_buffer_len++]=c;
	}
	if(sh.wtag!=0x2000) swab(sh.a_in_buffer,sh.a_in_buffer,8);
	length = a52_syncinfo ((uint8_t*)sh.a_in_buffer, &flags, &sample_rate, &bit_rate);
	if(length>=7 && length<=MAX_AC3_FRAME) break; /* we're done.*/
	/* bad file => resync*/
	if(sh.wtag!=0x2000) swab(sh.a_in_buffer,sh.a_in_buffer,8);
	memmove(sh.a_in_buffer,sh.a_in_buffer+1,7);
	--sh.a_in_buffer_len;
	apts=0;
    }
    mpxp_dbg2<<"a52: len="<<length<<" flags=0x"<<std::hex<<flags<<"  "<<sample_rate<<" Hz "<<bit_rate<<" bit/s"<<std::endl;
    sh.rate=sample_rate;
    sh.i_bps=bit_rate/8;
    demux_read_data_r(*sh.ds,(uint8_t*)sh.a_in_buffer+8,length-8,apts?null_pts:apts);
    if(sh.wtag!=0x2000) swab(sh.a_in_buffer+8,sh.a_in_buffer+8,length-8);
    last_pts=pts=apts;
    if(crc16_block((uint8_t*)sh.a_in_buffer+2,length-2)!=0)
	mpxp_status<<"a52: CRC check failed!"<<std::endl;
    return length;
}

/* returns: number of available channels*/
int a52_decoder::printinfo() const {
    int flags, sample_rate, bit_rate;
    const char* mode="unknown";
    int channels=0;
    a52_syncinfo ((uint8_t*)sh.a_in_buffer, &flags, &sample_rate, &bit_rate);
    switch(flags&A52_CHANNEL_MASK){
	case A52_CHANNEL: mode="channel"; channels=2; break;
	case A52_MONO: mode="mono"; channels=1; break;
	case A52_STEREO: mode="stereo"; channels=2; break;
	case A52_3F: mode="3f";channels=3;break;
	case A52_2F1R: mode="2f+1r";channels=3;break;
	case A52_3F1R: mode="3f+1r";channels=4;break;
	case A52_2F2R: mode="2f+2r";channels=4;break;
	case A52_3F2R: mode="3f+2r";channels=5;break;
	case A52_CHANNEL1: mode="channel1"; channels=2; break;
	case A52_CHANNEL2: mode="channel2"; channels=2; break;
	case A52_DOLBY: mode="dolby"; channels=2; break;
    }
    mpxp_info<<"AC3: "<<channels<<"."<<((flags&A52_LFE)?1:0)<<" ("<<mode<<((flags&A52_LFE)?"+lfe":"")<<") "<<sample_rate<<" Hz  "<<(bit_rate*0.001f)<<"f kbit/s Out: "<<(afmt2bps(sh.afmt)*8)<<"-bit"<<std::endl;
    return (flags&A52_LFE) ? (channels+1) : channels;
}

a52_decoder::a52_decoder(sh_audio_t& _sh,audio_filter_info_t& afi,uint32_t wtag)
	    :Audio_Decoder(_sh,afi,wtag)
	    ,sh(_sh)
{
    unsigned i;
    for(i=0;probes[i].driver;i++)
	if(wtag==probes[i].wtag)
	    probe=&probes[i];
    if(!probe) throw bad_format_exception();

    /* Dolby AC3 audio: */
    /* however many channels, 2 bytes in a word, 256 samples in a block, 6 blocks in a frame */
#ifdef WORDS_BIGENDIAN
#define A52_FMT32 AFMT_S32_BE
#define A52_FMT24 AFMT_S24_BE
#else
#define A52_FMT32 AFMT_S32_LE
#define A52_FMT24 AFMT_S24_LE
#endif
    sh.afmt=bps2afmt(2);
    if(af_query_fmt(afi.afilter,mpaf_format_e(AFMT_FLOAT32)) == MPXP_Ok||
	af_query_fmt(afi.afilter,mpaf_format_e(A52_FMT32)) == MPXP_Ok ||
	af_query_fmt(afi.afilter,mpaf_format_e(A52_FMT24)) == MPXP_Ok) sh.afmt=AFMT_FLOAT32;
    sh.audio_out_minsize=mp_conf.ao_channels*afmt2bps(sh.afmt)*256*6;
    sh.audio_in_minsize=MAX_AC3_FRAME;

    sample_t level=1, bias=384;
    float pts;
    int flags=0;
    /* Dolby AC3 audio:*/
    mpxp_a52_accel = mpxp_context().mplayer_accel;
    mpxp_a52_state=a52_init (mpxp_a52_accel);
    if (mpxp_a52_state == NULL) throw bad_format_exception();
    if(fillbuffer(pts)<0) throw bad_format_exception();
    /* 'a52 cannot upmix' hotfix:*/
    printinfo();
    sh.nch=mp_conf.ao_channels;
    while(sh.nch>0){
	switch(sh.nch){
	    case 1: mpxp_a52_flags=A52_MONO; break;
/*	    case 2: mpxp_a52_flags=A52_STEREO; break; */
	    case 2: mpxp_a52_flags=A52_DOLBY; break;
/*	    case 3: mpxp_a52_flags=A52_3F; break;*/
	    case 3: mpxp_a52_flags=A52_2F1R; break;
	    case 4: mpxp_a52_flags=A52_2F2R; break; /* 2+2*/
	    case 5: mpxp_a52_flags=A52_3F2R; break;
	    case 6: mpxp_a52_flags=A52_3F2R|A52_LFE; break; /* 5.1*/
	}
	/* test:*/
	flags=mpxp_a52_flags|A52_ADJUST_LEVEL;
	mpxp_v<<"A52 flags before a52_frame: 0x"<<std::hex<<flags<<std::endl;
	if (a52_frame (mpxp_a52_state, (uint8_t*)sh.a_in_buffer, &flags, &level, bias)) throw bad_format_exception();
	mpxp_v<<"A52 flags after a52_frame: 0x"<<std::hex<<flags<<std::endl;
	/* frame decoded, let's init resampler:*/
	if(afmt2bps(sh.afmt)==4) {
	    if(a52_resample_init_float(mpxp_a52_state,mpxp_a52_accel,flags,sh.nch)) break;
	} else {
	    if(a52_resample_init(mpxp_a52_state,mpxp_a52_accel,flags,sh.nch)) break;
	}
	--sh.nch; /* try to decrease no. of channels*/
    }
    if(sh.nch<=0) throw bad_format_exception();
}

a52_decoder::~a52_decoder() {}

audio_probe_t a52_decoder::get_probe_information() const { return *probe; }

MPXP_Rc a52_decoder::ctrl(int cmd,any_t* arg)
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

unsigned a52_decoder::run(unsigned char *buf,unsigned minlen,unsigned maxlen,float& pts)
{
    sample_t level=1, bias=384;
    int flags=mpxp_a52_flags|A52_ADJUST_LEVEL;
    unsigned i;
    unsigned len=0;
    UNUSED(minlen);
    UNUSED(maxlen);
    if(!sh.a_in_buffer_len) {
	if(fillbuffer(pts)<0) return len; /* EOF */
    } else pts=last_pts;
    sh.a_in_buffer_len=0;
    if (a52_frame (mpxp_a52_state, (uint8_t*)sh.a_in_buffer, &flags, &level, bias)){
	mpxp_warn<<"a52: error decoding frame"<<std::endl;
	return len;
    }
//	a52_dynrng(&mpxp_a52_state, NULL, NULL);
    len=0;
    for (i = 0; i < 6; i++) {
	if (a52_block (mpxp_a52_state)){
	    mpxp_warn<<"a52: error at resampling"<<std::endl;
	    break;
	}
	if(afmt2bps(sh.afmt)==4)
	    len+=4*a52_resample32(a52_samples(mpxp_a52_state),(float *)&buf[len]);
	else
	    len+=2*a52_resample(a52_samples(mpxp_a52_state),(int16_t *)&buf[len]);
    }
    return len;
}

static const mpxp_option_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

static Audio_Decoder* query_interface(sh_audio_t& sh,audio_filter_info_t& afi,uint32_t wtag) { return new(zeromem) a52_decoder(sh,afi,wtag); }

extern const ad_info_t ad_a52_info = {
    "liba52 AC3 driver",
    "liba52",
    "Nickols_K",
    "build-in",
    query_interface,
    options
};

} // namespace	usr