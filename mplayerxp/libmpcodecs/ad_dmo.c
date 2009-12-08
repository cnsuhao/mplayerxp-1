#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>

#include "codecs_ld.h"

#include "mp_config.h"
#include "help_mp.h"
#include "ad_internal.h"
#include "ad_msg.h"

static const ad_info_t info = 
{
	"Win32/DMO decoders",
	"dmo",
	"A'rpi",
	"avifile.sf.net"
};

static const config_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

LIBAD_EXTERN(dmo)


#include "../../loader/dmo/DMO_AudioDecoder.h"

typedef struct dmo_priv_s
{
  float pts;
  DMO_AudioDecoder* ds_adec;
}dmo_priv_t;

static DMO_AudioDecoder* (*DMO_AudioDecoder_Open_ptr)(char* dllname, GUID* guid, WAVEFORMATEX* wf,int out_channels);
static void (*DMO_AudioDecoder_Destroy_ptr)(DMO_AudioDecoder *this);
static int (*DMO_AudioDecoder_Convert_ptr)(DMO_AudioDecoder *this, const void* in_data, unsigned int in_size,
			     void* out_data, unsigned int out_size,
			     unsigned int* size_read, unsigned int* size_written);
static int (*DMO_AudioDecoder_GetSrcSize_ptr)(DMO_AudioDecoder *this, int dest_size);

#define DMO_AudioDecoder_Open(a,b,c,d) (*DMO_AudioDecoder_Open_ptr)(a,b,c,d)
#define DMO_AudioDecoder_Destroy(a) (*DMO_AudioDecoder_Destroy_ptr)(a)
#define DMO_AudioDecoder_Convert(a,b,c,d,e,f,g) (*DMO_AudioDecoder_Convert_ptr)(a,b,c,d,e,f,g)
#define DMO_AudioDecoder_GetSrcSize(a,b) (*DMO_AudioDecoder_GetSrcSize_ptr)(a,b)

static void *dll_handle;
static int load_lib( const char *libname )
{
  if(!(dll_handle=ld_codec(libname,NULL))) return 0;
  DMO_AudioDecoder_Open_ptr = ld_sym(dll_handle,"DMO_AudioDecoder_Open");
  DMO_AudioDecoder_Destroy_ptr = ld_sym(dll_handle,"DMO_AudioDecoder_Destroy");
  DMO_AudioDecoder_Convert_ptr = ld_sym(dll_handle,"DMO_AudioDecoder_Convert");
  DMO_AudioDecoder_GetSrcSize_ptr = ld_sym(dll_handle,"DMO_AudioDecoder_GetSrcSize");
  return DMO_AudioDecoder_Open_ptr && DMO_AudioDecoder_Destroy_ptr && DMO_AudioDecoder_Convert_ptr &&
	 DMO_AudioDecoder_GetSrcSize_ptr;
}


static int init(sh_audio_t *sh)
{
  return 1;
}

extern int audio_output_channels;

static int preinit(sh_audio_t *sh_audio)
{
  dmo_priv_t*priv;
  int chans=(audio_output_channels==sh_audio->wf->nChannels) ?
      audio_output_channels : (sh_audio->wf->nChannels>=2 ? 2 : 1);
  if(!(sh_audio->context=malloc(sizeof(dmo_priv_t)))) return 0;
  priv=sh_audio->context;
  if(!load_lib(wineld_name("DMO_Filter"SLIBSUFFIX))) return 0;
  if(!(priv->ds_adec=DMO_AudioDecoder_Open(sh_audio->codec->dll_name,&sh_audio->codec->guid,sh_audio->wf,chans)))
  {
    MSG_ERR(MSGTR_MissingDLLcodec,sh_audio->codec->dll_name);
    free(sh_audio->context);
    return 0;
  }
    sh_audio->i_bps=sh_audio->wf->nAvgBytesPerSec;
    sh_audio->channels=chans;
    sh_audio->samplerate=sh_audio->wf->nSamplesPerSec;
    sh_audio->audio_in_minsize=4*sh_audio->wf->nBlockAlign;
    if(sh_audio->audio_in_minsize<8192) sh_audio->audio_in_minsize=8192;
    sh_audio->audio_out_minsize=4*16384;
  MSG_V("INFO: Win32/DMO audio codec init OK!\n");
  return 1;
}

static void uninit(sh_audio_t *sh)
{
    dmo_priv_t*priv = sh->context;
    DMO_AudioDecoder_Destroy(priv->ds_adec);
    free(priv);
    dlclose(dll_handle);
}

static int control(sh_audio_t *sh_audio,int cmd,void* arg, ...)
{
  int skip;
    switch(cmd)
    {
      case ADCTRL_SKIP_FRAME:
	{
	    float pts;
		    skip=sh_audio->wf->nBlockAlign;
		    if(skip<16){
		      skip=(sh_audio->wf->nAvgBytesPerSec/16)&(~7);
		      if(skip<16) skip=16;
		    }
		    demux_read_data_r(sh_audio->ds,NULL,skip,&pts);
	  return CONTROL_TRUE;
	}
    }
  return CONTROL_UNKNOWN;
}

static int decode_audio(sh_audio_t *sh_audio,unsigned char *buf,int minlen,int maxlen,float *pts)
{
	dmo_priv_t *priv=sh_audio->context;
//	int len=-1;
        int size_in=0;
        int size_out=0;
        int srcsize=DMO_AudioDecoder_GetSrcSize(priv->ds_adec, maxlen);
        MSG_DBG3("DMO says: srcsize=%d  (buffsize=%d)  out_size=%d\n",srcsize,sh_audio->a_in_buffer_size,maxlen);
        if(srcsize>sh_audio->a_in_buffer_size) srcsize=sh_audio->a_in_buffer_size; // !!!!!!
        if(sh_audio->a_in_buffer_len<srcsize){
          int l;
          l=demux_read_data_r(sh_audio->ds,&sh_audio->a_in_buffer[sh_audio->a_in_buffer_len],
            srcsize-sh_audio->a_in_buffer_len,pts);
          *pts=FIX_APTS(sh_audio,*pts,-sh_audio->a_in_buffer_len);
          sh_audio->a_in_buffer_len+=l;
	    priv->pts=*pts;
        }
	else *pts=priv->pts;
        DMO_AudioDecoder_Convert(priv->ds_adec, sh_audio->a_in_buffer,sh_audio->a_in_buffer_len,
            buf,maxlen, &size_in,&size_out);
        MSG_DBG2("DMO: audio %d -> %d converted  (in_buf_len=%d of %d)  %d\n",size_in,size_out,sh_audio->a_in_buffer_len,sh_audio->a_in_buffer_size,ds_tell_pts(sh_audio->ds));
        if(size_in>=sh_audio->a_in_buffer_len){
          sh_audio->a_in_buffer_len=0;
        } else {
          sh_audio->a_in_buffer_len-=size_in;
          memmove(sh_audio->a_in_buffer,&sh_audio->a_in_buffer[size_in],sh_audio->a_in_buffer_len);
	  priv->pts=FIX_APTS(sh_audio,priv->pts,size_in);
        }
//        len=size_out;
  return size_out;
}
