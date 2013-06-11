#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>

#include <math.h>
#include <vorbis/codec.h>

#include "osdep/bswap.h"
#include "codecs_ld.h"
#include "ad_internal.h"
#include "libao3/afmt.h"
#include "libao3/audio_out.h"
#include "postproc/af.h"

namespace	usr {
    // This struct is also defined in demux_ogg.c => common header ?
    class vorbis_decoder : public Audio_Decoder {
	public:
	    vorbis_decoder(sh_audio_t&,audio_filter_info_t&,uint32_t wtag);
	    virtual ~vorbis_decoder();

	    virtual unsigned		run(unsigned char *buf,unsigned minlen,unsigned maxlen,float& pts);
	    virtual MPXP_Rc		ctrl(int cmd,any_t* arg);
	    virtual audio_probe_t	get_probe_information() const;
	private:
	    vorbis_info		vi; /* struct that stores all the static vorbis bitstream
				    settings */
	    vorbis_comment	vc; /* struct that stores all the bitstream user comments */
	    vorbis_dsp_state	vd; /* central working state for the packet->PCM decoder */
	    vorbis_block	vb; /* local working space for packet->PCM decode */
	    sh_audio_t&		sh;
	    audio_filter_info_t& afi;
	    const audio_probe_t*	probe;
};

static const audio_probe_t probes[] = {
    { "vorbis", "vorbis", 0x566F, ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE} },
    { "vorbis", "vorbis", FOURCC_TAG('V','R','B','S'), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE} },
    { NULL, NULL, 0x0, ACodecStatus_NotWorking, {AFMT_S8}}
};

vorbis_decoder::vorbis_decoder(sh_audio_t& _sh,audio_filter_info_t& _afi,uint32_t wtag)
	    :Audio_Decoder(_sh,_afi,wtag)
	    ,sh(_sh)
	    ,afi(_afi)
{
    unsigned i;
    for(i=0;probes[i].driver;i++)
	if(wtag==probes[i].wtag)
	    probe=&probes[i];
    if(!probe) throw bad_format_exception();

    sh.audio_out_minsize=1024*4; // 1024 samples/frame

    ogg_packet op;
    float pts;

    /// Init the decoder with the 3 header packets
    vorbis_info_init(&vi);
    vorbis_comment_init(&vc);
    op.bytes = ds_get_packet_r(*sh.ds,&op.packet,pts);
    op.b_o_s  = 1;
    /// Header
    if(vorbis_synthesis_headerin(&vi,&vc,&op) <0) {
	mpxp_v<<"OggVorbis: initial (identification) header broken!"<<std::endl;
	throw bad_format_exception();
    }
    op.bytes = ds_get_packet_r(*sh.ds,&op.packet,pts);
    op.b_o_s  = 0;
    /// Comments
    if(vorbis_synthesis_headerin(&vi,&vc,&op) <0) {
	mpxp_v<<"OggVorbis: comment header broken!"<<std::endl;
	throw bad_format_exception();
    }
    op.bytes = ds_get_packet_r(*sh.ds,&op.packet,pts);
    //// Codebook
    if(vorbis_synthesis_headerin(&vi,&vc,&op)<0) {
	mpxp_v<<"OggVorbis: codebook header broken!"<<std::endl;
	throw bad_format_exception();
    } else { /// Print the infos
	char **ptr=vc.user_comments;
	while(*ptr) {
	    mpxp_v<<"OggVorbisComment: "<<*ptr<<std::endl;
	    ++ptr;
	}
	mpxp_v<<"OggVorbis: Bitstream is "<<(int)vi.channels<<" channel, "<<(int)vi.rate<<"Hz, "<<(int)vi.bitrate_nominal
	    <<"bit/s "<<((vi.bitrate_lower!=vi.bitrate_nominal)||(vi.bitrate_upper!=vi.bitrate_nominal)?'V':'C')<<"BR"<<std::endl;
	mpxp_v<<"OggVorbis: Encoded by: "<<vc.vendor<<std::endl;
    }
    // Setup the decoder
    sh.nch=vi.channels;
    sh.rate=vi.rate;
#ifdef WORDS_BIGENDIAN
#define OGG_FMT32 AFMT_S32_BE
#define OGG_FMT24 AFMT_S24_BE
#define OGG_FMT16 AFMT_S16_BE
#else
#define OGG_FMT32 AFMT_S32_LE
#define OGG_FMT24 AFMT_S24_LE
#define OGG_FMT16 AFMT_S16_LE
#endif
    sh.afmt=OGG_FMT16;
    if(af_query_fmt(afi.afilter,mpaf_format_e(AFMT_FLOAT32)) == MPXP_Ok||
	af_query_fmt(afi.afilter,mpaf_format_e(OGG_FMT32)) == MPXP_Ok ||
	af_query_fmt(afi.afilter,mpaf_format_e(OGG_FMT24)) == MPXP_Ok) {
	sh.afmt=OGG_FMT32;
    }
    // assume 128kbit if bitrate not specified in the header
    sh.i_bps=((vi.bitrate_nominal>0) ? vi.bitrate_nominal : 128000)/8;

    /// Finish the decoder init
    vorbis_synthesis_init(&vd,&vi);
    vorbis_block_init(&vd,&vb);
    mpxp_ok<<"OggVorbis: Init OK!"<<std::endl;
}

vorbis_decoder::~vorbis_decoder() { }

audio_probe_t vorbis_decoder::get_probe_information() const { return *probe; }

MPXP_Rc vorbis_decoder::ctrl(int cmd,any_t* arg)
{
    UNUSED(cmd);
    UNUSED(arg);
    switch(cmd) {
#if 0
	case ADCTRL_RESYNC_STREAM:  return MPXP_True;
	case ADCTRL_SKIP_FRAME:  return MPXP_True;
#endif
    }
    return MPXP_Unknown;
}

unsigned vorbis_decoder::run(unsigned char *buf,unsigned minlen,unsigned maxlen,float& pts)
{
	unsigned len = 0;
	int samples;
	float **pcm;
	ogg_packet op;
	op.b_o_s =  op.e_o_s = 0;
	while(len < minlen) {
	  /* if file contains audio only steam there is no pts */
	  op.bytes = ds_get_packet_r(*sh.ds,&op.packet,pts);
	  if(!op.packet)
	    break;
	  if(vorbis_synthesis(&vb,&op)==0) /* test for success! */
	    vorbis_synthesis_blockin(&vd,&vb);
	  while((samples=vorbis_synthesis_pcmout(&vd,&pcm))>0){
	    unsigned i,j;
	    int clipflag=0;
	    int convsize=(maxlen-len)/(2*vi.channels); // max size!
	    int bout=(samples<convsize?samples:convsize);

	    if(bout<=0) break;

	    if(afmt2bps(sh.afmt)==4) {
	    /* convert floats to 32 bit signed ints (host order) and
	       interleave */
	    for(i=0;i<(unsigned)vi.channels;i++){
	      ogg_int32_t *convbuffer=(ogg_int32_t *)(&buf[len]);
	      ogg_int32_t *ptr=convbuffer+i;
	      float  *mono=pcm[i];
	      for(j=0;j<(unsigned)bout;j++){
#if 1
		int val=mono[j]*2147483647.f;
#else /* optional dither */
		int val=mono[j]*2147483647.f+drand48()-0.5f;
#endif
		/* might as well guard against clipping */
		if(val>2147483647){
		  val=2147483647;
		  clipflag=1;
		}
		if(val<-2147483647){
		  val=-2147483647;
		  clipflag=1;
		}
		*ptr=val;
		ptr+=vi.channels;
	      }
	    }
	    }
	    else
	    {
	    /* convert floats to 16 bit signed ints (host order) and
	       interleave */
	    for(i=0;i<(unsigned)vi.channels;i++){
	      ogg_int16_t *convbuffer=(ogg_int16_t *)(&buf[len]);
	      ogg_int16_t *ptr=convbuffer+i;
	      float  *mono=pcm[i];
	      for(j=0;j<(unsigned)bout;j++){
#if 1
		int val=mono[j]*32767.f;
#else /* optional dither */
		int val=mono[j]*32767.f+drand48()-0.5f;
#endif
		/* might as well guard against clipping */
		if(val>32767){
		  val=32767;
		  clipflag=1;
		}
		if(val<-32768){
		  val=-32768;
		  clipflag=1;
		}
		*ptr=val;
		ptr+=vi.channels;
	      }
	    }
	    }

	    if(clipflag) { mpxp_dbg2<<"Clipping in frame "<<(long)(vd.sequence)<<std::endl; }
	    len+=afmt2bps(sh.afmt)*vi.channels*bout;
	    mpxp_dbg2<<"[decoded: "<<bout<<" / "<<samples<<" ]"<<std::endl;
	    vorbis_synthesis_read(&vd,bout); /* tell libvorbis how
						    many samples we
						    actually consumed */
	  }
	}

  return len;
}

static const mpxp_option_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

static Audio_Decoder* query_interface(sh_audio_t& sh,audio_filter_info_t& afi,uint32_t wtag) { return new(zeromem) vorbis_decoder(sh,afi,wtag); }

extern const ad_info_t ad_vorbis_info = {
    "Ogg/Vorbis audio decoder",
    "vorbis",
    "Felix Buenemann, A'rpi",
    "build-in",
    query_interface,
    options
};
} // namespace	usr
