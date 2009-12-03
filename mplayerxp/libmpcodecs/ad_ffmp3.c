#include <stdio.h>
#include <stdlib.h>
#define __USE_GNU 1
#define __USE_XOPEN 1
#include <unistd.h>
#include <assert.h>
#include <dlfcn.h> /* GLIBC specific. Exists under cygwin too! */
#include "ad_internal.h"

#include "config.h"
#include "help_mp.h"
#include "bswap.h"

#include "interface/ffmpeg/avcodec.h"
#include "codecs_ld.h"

#ifndef FF_INPUT_BUFFER_PADDING_SIZE
#define FF_INPUT_BUFFER_PADDING_SIZE 8
#endif

static AVCodec *lavc_codec=NULL;
static AVCodecContext *lavc_context;
static int acodec_inited;

static const ad_info_t info =
{
	"FFmpeg layer-123 audio decoder - integer only",
	"ffmpeg",
	"Nickols_K",
	""
};

static const config_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

LIBAD_EXTERN(ffmp3)

unsigned (*avcodec_version_ptr)(void);
void (*avcodec_init_ptr)(void);
void (*avcodec_register_all_ptr)(void);
AVCodec * (*avcodec_find_decoder_by_name_ptr)(const char *name);
int (*avcodec_open_ptr)(AVCodecContext *avctx, AVCodec *codec);
int (*avcodec_close_ptr)(AVCodecContext *avctx);
int (*avcodec_decode_audio_ptr)(AVCodecContext *avctx, INT16 *samples, 
                         int *frame_size_ptr,
                         uint8_t *buf, int buf_size);
AVCodecContext* (*avcodec_alloc_context_ptr)(void);

static void *dll_handle;
static int load_dll(const char *libname)
{
  if(!(dll_handle=ld_codec(libname,"http://ffmpeg.sf.net"))) return 0;
  avcodec_init_ptr = ld_sym(dll_handle,"avcodec_init");
  avcodec_version_ptr = ld_sym(dll_handle,"avcodec_version");
  avcodec_register_all_ptr = ld_sym(dll_handle,"avcodec_register_all");
  avcodec_find_decoder_by_name_ptr = ld_sym(dll_handle,"avcodec_find_decoder_by_name");
  avcodec_alloc_context_ptr = ld_sym(dll_handle,"avcodec_alloc_context");
  avcodec_open_ptr = ld_sym(dll_handle,"avcodec_open");
  avcodec_close_ptr = ld_sym(dll_handle,"avcodec_close");
  avcodec_decode_audio_ptr = ld_sym(dll_handle,"avcodec_decode_audio2");
  return avcodec_init_ptr && avcodec_register_all_ptr && avcodec_find_decoder_by_name_ptr
	 && avcodec_open_ptr && avcodec_close_ptr && avcodec_decode_audio_ptr
	 && avcodec_version_ptr && avcodec_alloc_context_ptr;
}

#define MIN_LIBAVCODEC_VERSION_INT	((51<<16)+(0<<8)+0)

int preinit(sh_audio_t *sh)
{
  unsigned avc_version;
  sh->audio_out_minsize=AVCODEC_MAX_AUDIO_FRAME_SIZE;
  if(!load_dll(codec_name("libavcodec"SLIBSUFFIX))) /* try local copy first */
   if(!load_dll("libavcodec-0.4.9"SLIBSUFFIX))
    if(!load_dll("libavcodec"SLIBSUFFIX))
    {
	MSG_ERR("Detected error during loading libffmpeg.so! Try to upgrade this codec\n");
	return 0;
    }
    avc_version = (*avcodec_version_ptr)();
    if(avc_version < MIN_LIBAVCODEC_VERSION_INT)
    {
	MSG_ERR("You have wrong version of libavcodec %06X < %06X\n",
		avc_version,MIN_LIBAVCODEC_VERSION_INT);
	return 0;
    }
    sh->audio_out_minsize=AVCODEC_MAX_AUDIO_FRAME_SIZE;
    return 1;
}

int init(sh_audio_t *sh_audio)
{
   int x;
   float pts;
   MSG_V("FFmpeg's libavcodec audio codec\n");
    if(!acodec_inited){
      (*avcodec_init_ptr)();
      (*avcodec_register_all_ptr)();
      acodec_inited=1;
    }
    lavc_codec = (AVCodec *)(*avcodec_find_decoder_by_name_ptr)(sh_audio->codec->dll_name);
    if(!lavc_codec){
	MSG_ERR(MSGTR_MissingLAVCcodec,sh_audio->codec->dll_name);
	return 0;
    }
    lavc_context = (*avcodec_alloc_context_ptr)();
    sh_audio->context = lavc_context;
    if(sh_audio->wf)
    {
	lavc_context->channels = sh_audio->wf->nChannels;
	lavc_context->sample_rate = sh_audio->wf->nSamplesPerSec;
	lavc_context->bit_rate = sh_audio->wf->nAvgBytesPerSec * 8;
	lavc_context->block_align = sh_audio->wf->nBlockAlign;
	lavc_context->bits_per_coded_sample = sh_audio->wf->wBitsPerSample;
	/* alloc extra data */
	if (sh_audio->wf->cbSize > 0) {
	    lavc_context->extradata = malloc(sh_audio->wf->cbSize+FF_INPUT_BUFFER_PADDING_SIZE);
	    lavc_context->extradata_size = sh_audio->wf->cbSize;
	    memcpy(lavc_context->extradata, (char *)sh_audio->wf + sizeof(WAVEFORMATEX),
		    lavc_context->extradata_size);
	}
    }
    // for QDM2
    if (sh_audio->codecdata_len && sh_audio->codecdata && !lavc_context->extradata)
    {
        lavc_context->extradata = malloc(sh_audio->codecdata_len);
        lavc_context->extradata_size = sh_audio->codecdata_len;
        memcpy(lavc_context->extradata, (char *)sh_audio->codecdata, 
               lavc_context->extradata_size);	
    }
    lavc_context->codec_tag = sh_audio->format;
    lavc_context->codec_id = lavc_codec->id;
    /* open it */
    if ((*avcodec_open_ptr)(lavc_context, lavc_codec) < 0) {
        MSG_ERR( MSGTR_CantOpenCodec);
        return 0;
    }
   MSG_V("INFO: libavcodec init OK!\n");
   if(sh_audio->format==0x3343414D){
       // MACE 3:1
       sh_audio->ds->ss_div = 2*3; // 1 samples/packet
       sh_audio->ds->ss_mul = 2*sh_audio->wf->nChannels; // 1 byte*ch/packet
   } else
   if(sh_audio->format==0x3643414D){
       // MACE 6:1
       sh_audio->ds->ss_div = 2*6; // 1 samples/packet
       sh_audio->ds->ss_mul = 2*sh_audio->wf->nChannels; // 1 byte*ch/packet
   }

   // Decode at least 1 byte:  (to get header filled)
   x=decode_audio(sh_audio,sh_audio->a_buffer,1,sh_audio->a_buffer_size,&pts);
   if(x>0) sh_audio->a_buffer_len=x;

  sh_audio->channels=lavc_context->channels;
  sh_audio->samplerate=lavc_context->sample_rate;
  sh_audio->i_bps=lavc_context->bit_rate/8;
  return 1;
}

void uninit(sh_audio_t *sh)
{
  (*avcodec_close_ptr)(lavc_context);
  dlclose(dll_handle);
  if (lavc_context->extradata) free(lavc_context->extradata);
  free(lavc_context);
  acodec_inited=0;
}

int control(sh_audio_t *sh,int cmd,void* arg, ...)
{
  return CONTROL_UNKNOWN;
}

int decode_audio(sh_audio_t *sh_audio,unsigned char *buf,int minlen,int maxlen,float *pts)
{
    unsigned char *start=NULL;
    int y,len=-1;
    float apts=0.,null_pts;
    while(len<minlen){
	int len2=maxlen;
	int x=ds_get_packet_r(sh_audio->ds,&start,apts?&null_pts:&apts);
	if(x<=0) break; // error
	if(sh_audio->format==mmioFOURCC('d','n','e','t')) swab(start,start,x&(~1));
	y=(*avcodec_decode_audio_ptr)(sh_audio->context,(INT16*)buf,&len2,start,x);
	if(y<0){ MSG_V("lavc_audio: error\n");break; }
	if(y<x)
	{
	    sh_audio->ds->buffer_pos+=y-x;  // put back data (HACK!)
	    if(sh_audio->format==mmioFOURCC('d','n','e','t'))
		swab(start+y,start+y,(x-y)&~(1));
	}
	if(len2>0){
	  //len=len2;break;
	  if(len<0) len=len2; else len+=len2;
	  buf+=len2;
	}
        MSG_DBG2("Decoded %d -> %d  \n",y,len2);
    }
  *pts=apts;
  return len;
}
