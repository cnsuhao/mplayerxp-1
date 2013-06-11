#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <stddef.h>
#include <dlfcn.h>

#include "ad_internal.h"
#include "codecs_ld.h"
#include "ad_msg.h"
#include "osdep/bswap.h"
#include "libao3/afmt.h"

namespace	usr {
    struct ra_init_t {
	int samplerate;
	short bits;
	short channels;
	short quality;
	/* 2bytes padding here, by gcc */
	int bits_per_frame;
	int packetsize;
	int extradata_len;
	any_t* extradata;
    };

    class areal_decoder : public Audio_Decoder {
	public:
	    areal_decoder(sh_audio_t&,audio_filter_info_t&,uint32_t wtag);
	    virtual ~areal_decoder();

	    virtual unsigned		run(unsigned char *buf,unsigned minlen,unsigned maxlen,float& pts);
	    virtual MPXP_Rc		ctrl(int cmd,any_t* arg);
	    virtual audio_probe_t	get_probe_information() const;
	private:
	    MPXP_Rc			load_dll(const std::string& name);

	    const audio_probe_t*	probe;
	    any_t*			internal;
	    float			pts;
	    sh_audio_t&			sh;
	    any_t*			handle;
	    uint32_t	(*raCloseCodec)(uint32_t);
	    uint32_t	(*raDecode)(any_t*,any_t*,uint32_t,any_t*,any_t*,uint32_t);
	    uint32_t	(*raFreeDecoder)(uint32_t);
	    any_t*	(*raGetFlavorProperty)(any_t*,uint32_t,uint32_t,any_t*);
	    uint32_t	(*raInitDecoder)(any_t*,any_t*);
	    uint32_t	(*raOpenCodec2)(any_t*,any_t*);
	    uint32_t	(*raOpenCodec)(any_t*);
	    uint32_t	(*raSetFlavor)(any_t*,uint32_t);
	    void	(*raSetDLLAccessPath)(uint32_t);
	    void	(*raSetPwd)(char*,char*);
    };

static const audio_probe_t probes[] = {
    { "realaudio", "14_4.so.6.0", FOURCC_TAG('1','4','_','4'), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE, AFMT_S8} },
    { "realaudio", "28_8.so.6.0", FOURCC_TAG('2','8','_','8'), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE, AFMT_S8} },
    { "realaudio", "cook.so.6.0", FOURCC_TAG('C','O','O','K'), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE, AFMT_S8} },
    { "realaudio", "sipr.so.6.0", FOURCC_TAG('S','I','P','R'), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE, AFMT_S8} },
    { "realaudio", "atrc.so.6.0", FOURCC_TAG('A','T','R','C'), ACodecStatus_Working, {AFMT_FLOAT32, AFMT_S24_LE, AFMT_S16_LE, AFMT_S8} },
    { NULL, NULL, 0x0, ACodecStatus_NotWorking, {AFMT_S8}}
};

MPXP_Rc areal_decoder::load_dll(const std::string& name)
{
    if(!(handle = ::dlopen (name.c_str(), RTLD_LAZY))) return MPXP_False;

    raCloseCodec = (uint32_t (*)(uint32_t))ld_sym(handle, "RACloseCodec");
    raDecode = (uint32_t (*)(any_t*,any_t*,uint32_t,any_t*,any_t*,uint32_t))ld_sym(handle, "RADecode");
    raFreeDecoder = (uint32_t (*)(uint32_t))ld_sym(handle, "RAFreeDecoder");
    raGetFlavorProperty = (any_t* (*)(any_t*,uint32_t,uint32_t,any_t*))ld_sym(handle, "RAGetFlavorProperty");
    raOpenCodec = (uint32_t (*)(any_t*))ld_sym(handle, "RAOpenCodec");
    raOpenCodec2 = (uint32_t (*)(any_t*,any_t*))ld_sym(handle, "RAOpenCodec2");
    raInitDecoder = (uint32_t (*)(any_t*,any_t*))ld_sym(handle, "RAInitDecoder");
    raSetFlavor = (uint32_t (*)(any_t*,uint32_t))ld_sym(handle, "RASetFlavor");
    raSetDLLAccessPath = (void (*)(uint32_t))ld_sym(handle, "SetDLLAccessPath");
    raSetPwd = (void (*)(char*,char*))ld_sym(handle, "RASetPwd"); /* optional, used by SIPR */

    return (raCloseCodec && raDecode && raFreeDecoder &&
	raGetFlavorProperty && (raOpenCodec2||raOpenCodec) && raSetFlavor &&
	raInitDecoder)?MPXP_Ok:MPXP_False;
}

areal_decoder::areal_decoder(sh_audio_t& _sh,audio_filter_info_t& afi,uint32_t wtag)
	    :Audio_Decoder(_sh,afi,wtag)
	    ,sh(_sh)
{
    unsigned i;
    for(i=0;probes[i].driver;i++)
	if(wtag==probes[i].wtag)
	    probe=&probes[i];
    if(!probe) throw bad_format_exception();

    // let's check if the driver is available, return 0 if not.
    // (you should do that if you use external lib(s) which is optional)
    unsigned int result;
    int len=0;
    any_t* prop;
    char path[4096];
    char cpath[4096];

    if(load_dll(probe->codec_dll)!=MPXP_Ok) throw bad_format_exception();

    char *end;
    strcpy(cpath,sh.codec->dll_name);
    end = strrchr(cpath,'/');
    if(end) *end=0;
    if(!strlen(cpath)) strcpy(cpath,"/usr/lib");
    sprintf(path, "DT_Codecs=%s", cpath);
    if(path[strlen(path)-1]!='/'){
	path[strlen(path)+1]=0;
	path[strlen(path)]='/';
    }
    path[strlen(path)+1]=0;

    if(raSetDLLAccessPath)
	(*raSetDLLAccessPath)(long(path));

    if(raOpenCodec2) {
	strcat(cpath,"/");
	result=(*raOpenCodec2)(&internal,cpath);
    }
    else result=(*raOpenCodec)(&internal);
    if(result){
	mpxp_v<<"Decoder open failed, error code: 0x"<<std::hex<<result<<std::endl;
	throw bad_format_exception();
    }

    sh.rate=sh.wf->nSamplesPerSec;
    sh.afmt=bps2afmt(sh.wf->wBitsPerSample/8);
    sh.nch=sh.wf->nChannels;

    ra_init_t init_data={
	sh.wf->nSamplesPerSec,
	sh.wf->wBitsPerSample,
	sh.wf->nChannels,
	100, // quality
	((short*)(sh.wf+1))[0],  // subpacket size
	((short*)(sh.wf+1))[3],  // coded frame size
	((short*)(sh.wf+1))[4], // codec data length
	((char*)(sh.wf+1))+10 // extras
    };
    result=(*raInitDecoder)(internal,&init_data);
    if(result){
	mpxp_v<<"Decoder init failed, error code: 0x"<<std::hex<<result<<std::endl;
	throw bad_format_exception();
    }

    if(raSetPwd){
	// used by 'SIPR'
	(*raSetPwd)(reinterpret_cast<char*>(internal),const_cast<char*>("Ardubancel Quazanga")); // set password... lol.
    }

    result=(*raSetFlavor)(internal,((short*)(sh.wf+1))[2]);
    if(result){
	mpxp_warn<<"Decoder flavor setup failed, error code: 0x"<<std::hex<<result<<std::endl;
	throw bad_format_exception();
    }

    prop=(*raGetFlavorProperty)(internal,((short*)(sh.wf+1))[2],0,&len);
    mpxp_v<<"Audio codec: ["<<((short*)(sh.wf+1))[2]<<"] "<<prop<<std::endl;

    prop=(*raGetFlavorProperty)(internal,((short*)(sh.wf+1))[2],1,&len);
    if(prop){
	sh.i_bps=((*((int*)prop))+4)/8;
	mpxp_v<<"Audio bitrate: "<<((*((int*)prop))*0.001f)<<" kbit/s ("<<(sh.i_bps)<<" bps)"<<std::endl;
    } else sh.i_bps=sh.wf->nAvgBytesPerSec;

    sh.audio_out_minsize=128000; // no idea how to get... :(
    sh.audio_in_minsize=((short*)(sh.wf+1))[1]*sh.wf->nBlockAlign;
}

areal_decoder::~areal_decoder() {
    if (raFreeDecoder) raFreeDecoder(long(internal));
    if (raCloseCodec) raCloseCodec(long(internal));
    if(handle) ::dlclose(handle);
}

audio_probe_t areal_decoder::get_probe_information() const { return *probe; }

static const unsigned char sipr_swaps[38][2]={
    {0,63},{1,22},{2,44},{3,90},{5,81},{7,31},{8,86},{9,58},{10,36},{12,68},
    {13,39},{14,73},{15,53},{16,69},{17,57},{19,88},{20,34},{21,71},{24,46},
    {25,94},{26,54},{28,75},{29,50},{32,70},{33,92},{35,74},{38,85},{40,56},
    {42,87},{43,65},{45,59},{48,79},{49,93},{51,89},{55,95},{61,76},{67,83},
    {77,80} };

unsigned areal_decoder::run(unsigned char *buf,unsigned minlen,unsigned maxlen,float& _pts){
  float null__pts;
  int result;
  unsigned len=0;
  unsigned sps=((short*)(sh.wf+1))[0]; /* subpacket size */
  unsigned w=sh.wf->nBlockAlign; // 5
  unsigned h=((short*)(sh.wf+1))[1];
  unsigned cfs=((short*)(sh.wf+1))[3]; /* coded frame size */
  UNUSED(minlen);
  UNUSED(maxlen);
  if(sh.a_in_buffer_len<=0){
      // fill the buffer!
    if (sh.wtag == mmioFOURCC('1','4','_','4')) {
	demux_read_data_r(*sh.ds, reinterpret_cast<unsigned char*>(sh.a_in_buffer), sh.wf->nBlockAlign,_pts);
	sh.a_in_buffer_size=
	sh.a_in_buffer_len=sh.wf->nBlockAlign;
    } else
    if (sh.wtag == mmioFOURCC('2','8','_','8')) {
	unsigned i,j;
	unsigned char *p=reinterpret_cast<unsigned char*>(sh.a_in_buffer);
	for (j = 0; j < h; j++)
	    for (i = 0; i < h/2; i++)
		demux_read_data_r(*sh.ds, p+i*2*w+j*cfs, cfs,(i==0&&j==0)?_pts:null__pts);
	sh.a_in_buffer_size=
	sh.a_in_buffer_len=sh.wf->nBlockAlign*h;
    } else
    if(!sps){
      // 'sipr' way
      unsigned j,n;
      unsigned bs=h*w*2/96; // nibbles per subpacket
      unsigned char *p=reinterpret_cast<unsigned char*>(sh.a_in_buffer);
      demux_read_data_r(*sh.ds, p, h*w,_pts);
      for(n=0;n<38;n++){
	  int i=bs*sipr_swaps[n][0];
	  int o=bs*sipr_swaps[n][1];
	  // swap nibbles of block 'i' with 'o'      TODO: optimize
	  for(j=0;j<bs;j++){
	      int x=(i&1) ? (p[(i>>1)]>>4) : (p[(i>>1)]&15);
	      int y=(o&1) ? (p[(o>>1)]>>4) : (p[(o>>1)]&15);
	      if(o&1) p[(o>>1)]=(p[(o>>1)]&0x0F)|(x<<4);
		else  p[(o>>1)]=(p[(o>>1)]&0xF0)|x;
	      if(i&1) p[(i>>1)]=(p[(i>>1)]&0x0F)|(y<<4);
		else  p[(i>>1)]=(p[(i>>1)]&0xF0)|y;
	      ++i;++o;
	  }
      }
      sh.a_in_buffer_size=
      sh.a_in_buffer_len=w*h;
    } else {
      // 'cook' way
      unsigned char *p=reinterpret_cast<unsigned char*>(sh.a_in_buffer);
      unsigned x,y;
      w/=sps;
      for(y=0;y<h;y++)
	for(x=0;x<w;x++){
	    demux_read_data_r(*sh.ds, p+sps*(h*x+((h+1)/2)*(y&1)+(y>>1)), sps,(x==0&&y==0)?_pts:null__pts);
	}
      sh.a_in_buffer_size=
      sh.a_in_buffer_len=w*h*sps;
    }
    pts=_pts;
  }
  else _pts=pts;

  result=(*raDecode)(internal, sh.a_in_buffer+sh.a_in_buffer_size-sh.a_in_buffer_len, sh.wf->nBlockAlign, buf, &len, -1);
  if((int)len<0) len=0;
  sh.a_in_buffer_len-=sh.wf->nBlockAlign;
  pts=FIX_APTS(sh,pts,sh.wf->nBlockAlign);

  return len; // return value: number of _bytes_ written to output buffer,
	      // or -1 for EOF (or uncorrectable error)
}

MPXP_Rc areal_decoder::ctrl(int cmd,any_t* arg){
    UNUSED(arg);
    // various optional functions you MAY implement:
    switch(cmd){
	case ADCTRL_RESYNC_STREAM:
	    // it is called once after seeking, to resync.
	    // Note: sh.a_in_buffer_len=0; is done _before_ this call!
	    return MPXP_True;
	 case ADCTRL_SKIP_FRAME:
	    // it is called to skip (jump over) small amount (1/10 sec or 1 frame)
	    // of audio data - used to sync audio to video after seeking
	    // if you don't return MPXP_True, it will defaults to:
	    //      ds_fill_buffer(sh.ds);  // skip 1 demux packet
	    return MPXP_True;
    }
    return MPXP_Unknown;
}

static const mpxp_option_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

static Audio_Decoder* query_interface(sh_audio_t& sh,audio_filter_info_t& afi,uint32_t wtag) { return new(zeromem) areal_decoder(sh,afi,wtag); }

extern const ad_info_t ad_real_info = {
    "RealAudio decoder",
    "realaudio",
    "A'rpi",
    "build-in",
    query_interface,
    options
};
} // namespace	usr
