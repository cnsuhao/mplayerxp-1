
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "mp_config.h"

#include <stddef.h>
#include <dlfcn.h>

#include "ad_internal.h"
#include "codecs_ld.h"
#include "ad_msg.h"
#include "osdep/mplib.h"
static const ad_info_t info =  {
	"RealAudio decoder",
	"realaud",
	"A'rpi",
	"build-in"
};

static const config_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL, NULL}
};

LIBAD_EXTERN(real)

static any_t*handle=NULL;

any_t*__builtin_new(unsigned long size) {
	return mp_malloc(size);
}

/* required for cook's uninit: */
void __builtin_delete(any_t* ize) {
	mp_free(ize);
}

#if defined(__FreeBSD__) || defined(__NetBSD__)
any_t* __ctype_b=NULL;
#endif

static uint32_t (*raCloseCodec)(uint32_t);
static uint32_t (*raDecode)(any_t*,any_t*,uint32_t,any_t*,any_t*,uint32_t);
static uint32_t (*raFreeDecoder)(uint32_t);
static any_t* (*raGetFlavorProperty)(any_t*,uint32_t,uint32_t,any_t*);
//static uint32_t (*raGetNumberOfFlavors2)(void);
static uint32_t (*raInitDecoder)(any_t*,any_t*);
static uint32_t (*raOpenCodec2)(any_t*,any_t*);
static uint32_t (*raOpenCodec)(any_t*);
static uint32_t (*raSetFlavor)(any_t*,uint32_t);
static void  (*raSetDLLAccessPath)(uint32_t);
static void  (*raSetPwd)(char*,char*);

typedef struct {
    int samplerate;
    short bits;
    short channels;
    short quality;
    /* 2bytes padding here, by gcc */
    int bits_per_frame;
    int packetsize;
    int extradata_len;
    any_t* extradata;
} ra_init_t;

typedef struct {
    any_t*internal;
    float pts;
} real_priv_t;

static int preinit(sh_audio_t *sh){
  // let's check if the driver is available, return 0 if not.
  // (you should do that if you use external lib(s) which is optional)
  unsigned int result;
  int len=0;
  any_t* prop;
  char path[4096];
  char cpath[4096];
  real_priv_t *rpriv;
  rpriv=sh->context=mp_malloc(sizeof(real_priv_t));
  if(!(handle = dlopen (sh->codec->dll_name, RTLD_LAZY)))
  {
      mp_free(sh->context);
      return 0;
  }

    raCloseCodec = ld_sym(handle, "RACloseCodec");
    raDecode = ld_sym(handle, "RADecode");
    raFreeDecoder = ld_sym(handle, "RAFreeDecoder");
    raGetFlavorProperty = ld_sym(handle, "RAGetFlavorProperty");
    raOpenCodec = ld_sym(handle, "RAOpenCodec");
    raOpenCodec2 = ld_sym(handle, "RAOpenCodec2");
    raInitDecoder = ld_sym(handle, "RAInitDecoder");
    raSetFlavor = ld_sym(handle, "RASetFlavor");
    raSetDLLAccessPath = ld_sym(handle, "SetDLLAccessPath");
    raSetPwd = ld_sym(handle, "RASetPwd"); /* optional, used by SIPR */
    
  if(!raCloseCodec || !raDecode || !raFreeDecoder ||
     !raGetFlavorProperty || !(raOpenCodec2||raOpenCodec) || !raSetFlavor ||
     !raInitDecoder){
      mp_free(sh->context);
      return 0;
  }

  {
      char *end;
      strcpy(cpath,sh->codec->dll_name);
      end = strrchr(cpath,'/');
      if(end) *end=0;
      if(!strlen(cpath)) strcpy(cpath,"/usr/lib");
      sprintf(path, "DT_Codecs=%s", cpath);
      if(path[strlen(path)-1]!='/'){
        path[strlen(path)+1]=0;
        path[strlen(path)]='/';
      }
      path[strlen(path)+1]=0;

  }
  if(raSetDLLAccessPath)
      raSetDLLAccessPath(path);

    if(raOpenCodec2)
    {
	strcat(cpath,"/");
	result=raOpenCodec2(&rpriv->internal,cpath);
    }
    else
	result=raOpenCodec(&rpriv->internal);
    if(result){
      MSG_WARN("Decoder open failed, error code: 0x%X\n",result);
      mp_free(sh->context);
      return 0;
    }

  sh->samplerate=sh->wf->nSamplesPerSec;
  sh->samplesize=sh->wf->wBitsPerSample/8;
  sh->channels=sh->wf->nChannels;

  {
    ra_init_t init_data={
	sh->wf->nSamplesPerSec,
	sh->wf->wBitsPerSample,
	sh->wf->nChannels,
	100, // quality
	((short*)(sh->wf+1))[0],  // subpacket size
	((short*)(sh->wf+1))[3],  // coded frame size
	((short*)(sh->wf+1))[4], // codec data length
	((char*)(sh->wf+1))+10 // extras
    };
    result=raInitDecoder(rpriv->internal,&init_data);
    if(result){
      MSG_WARN("Decoder init failed, error code: 0x%X\n",result);
      mp_free(sh->context);
      return 0;
    }
  }
  
    if(raSetPwd){
	// used by 'SIPR'
	raSetPwd(rpriv->internal,"Ardubancel Quazanga"); // set password... lol.
    }

    result=raSetFlavor(rpriv->internal,((short*)(sh->wf+1))[2]);
    if(result){
      MSG_WARN("Decoder flavor setup failed, error code: 0x%X\n",result);
      mp_free(sh->context);
      return 0;
    }

    prop=raGetFlavorProperty(rpriv->internal,((short*)(sh->wf+1))[2],0,&len);
    MSG_INFO("Audio codec: [%d] %s\n",((short*)(sh->wf+1))[2],prop);

    prop=raGetFlavorProperty(rpriv->internal,((short*)(sh->wf+1))[2],1,&len);
    if(prop){
	sh->i_bps=((*((int*)prop))+4)/8;
	MSG_INFO("Audio bitrate: %5.3f kbit/s (%d bps)  \n",(*((int*)prop))*0.001f,sh->i_bps);
    } else
	sh->i_bps=sh->wf->nAvgBytesPerSec;

//    prop=raGetFlavorProperty(rpriv->internal,((short*)(sh->wf+1))[2],0x13,&len);
//    MSG_INFO("Samples/block?: %d  \n",(*((int*)prop)));

  sh->audio_out_minsize=128000; // no idea how to get... :(
  sh->audio_in_minsize=((short*)(sh->wf+1))[1]*sh->wf->nBlockAlign;
  
  return 1; // return values: 1=OK 0=ERROR
}

static int init(sh_audio_t *sh_audio){
  // initialize the decoder, set tables etc...
  // set sample format/rate parameters if you didn't do it in preinit() yet.
  UNUSED(sh_audio);
  return 1; // return values: 1=OK 0=ERROR
}

static void uninit(sh_audio_t *sh){
  // uninit the decoder etc...
  real_priv_t *rpriv = sh->context;
  if (raFreeDecoder) raFreeDecoder(rpriv->internal);
  if (raCloseCodec) raCloseCodec(rpriv->internal);
  mp_free(sh->context);
}

static const unsigned char sipr_swaps[38][2]={
    {0,63},{1,22},{2,44},{3,90},{5,81},{7,31},{8,86},{9,58},{10,36},{12,68},
    {13,39},{14,73},{15,53},{16,69},{17,57},{19,88},{20,34},{21,71},{24,46},
    {25,94},{26,54},{28,75},{29,50},{32,70},{33,92},{35,74},{38,85},{40,56},
    {42,87},{43,65},{45,59},{48,79},{49,93},{51,89},{55,95},{61,76},{67,83},
    {77,80} };

static unsigned decode(sh_audio_t *sh,unsigned char *buf,unsigned minlen,unsigned maxlen,float *pts){
  real_priv_t *rpriv = sh->context;
  float null_pts;
  int result;
  unsigned len=0;
  unsigned sps=((short*)(sh->wf+1))[0]; /* subpacket size */
  unsigned w=sh->wf->nBlockAlign; // 5
  unsigned h=((short*)(sh->wf+1))[1];
  unsigned cfs=((short*)(sh->wf+1))[3]; /* coded frame size */
  UNUSED(minlen);
  UNUSED(maxlen);
  if(sh->a_in_buffer_len<=0){
      // fill the buffer!
    if (sh->wtag == mmioFOURCC('1','4','_','4')) {
	demux_read_data_r(sh->ds, sh->a_in_buffer, sh->wf->nBlockAlign,pts);
	sh->a_in_buffer_size=
	sh->a_in_buffer_len=sh->wf->nBlockAlign;
    } else
    if (sh->wtag == mmioFOURCC('2','8','_','8')) {
	unsigned i,j;
	for (j = 0; j < h; j++)
	    for (i = 0; i < h/2; i++)
		demux_read_data_r(sh->ds, sh->a_in_buffer+i*2*w+j*cfs, cfs,(i==0&&j==0)?pts:&null_pts);
	sh->a_in_buffer_size=
	sh->a_in_buffer_len=sh->wf->nBlockAlign*h;
    } else
    if(!sps){
      // 'sipr' way
      unsigned j,n;
      unsigned bs=h*w*2/96; // nibbles per subpacket
      unsigned char *p=sh->a_in_buffer;
      demux_read_data_r(sh->ds, p, h*w,pts);
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
      sh->a_in_buffer_size=
      sh->a_in_buffer_len=w*h;
    } else {
      // 'cook' way
      unsigned x,y;
      w/=sps;
      for(y=0;y<h;y++)
	for(x=0;x<w;x++){
	    demux_read_data_r(sh->ds, sh->a_in_buffer+sps*(h*x+((h+1)/2)*(y&1)+(y>>1)), sps,(x==0&&y==0)?pts:&null_pts);
	}
      sh->a_in_buffer_size=
      sh->a_in_buffer_len=w*h*sps;
    }
    rpriv->pts=*pts;
  }
  else *pts=rpriv->pts;

  result=raDecode(rpriv->internal, sh->a_in_buffer+sh->a_in_buffer_size-sh->a_in_buffer_len, sh->wf->nBlockAlign,
       buf, &len, -1);
  if((int)len<0) len=0;
  sh->a_in_buffer_len-=sh->wf->nBlockAlign;
  rpriv->pts=FIX_APTS(sh,rpriv->pts,sh->wf->nBlockAlign);

  return len; // return value: number of _bytes_ written to output buffer,
              // or -1 for EOF (or uncorrectable error)
}

static int control(sh_audio_t *sh,int cmd,any_t* arg, ...){
    UNUSED(sh);
    UNUSED(arg);
    // various optional functions you MAY implement:
    switch(cmd){
      case ADCTRL_RESYNC_STREAM:
        // it is called once after seeking, to resync.
	// Note: sh_audio->a_in_buffer_len=0; is done _before_ this call!
	return CONTROL_TRUE;
      case ADCTRL_SKIP_FRAME:
        // it is called to skip (jump over) small amount (1/10 sec or 1 frame)
	// of audio data - used to sync audio to video after seeking
	// if you don't return CONTROL_TRUE, it will defaults to:
	//      ds_fill_buffer(sh_audio->ds);  // skip 1 demux packet
	return CONTROL_TRUE;
    }
  return CONTROL_UNKNOWN;
}
