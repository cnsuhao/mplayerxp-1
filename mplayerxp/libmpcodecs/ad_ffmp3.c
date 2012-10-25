#include <stdio.h>
#include <stdlib.h>
#define __USE_GNU 1
#define __USE_XOPEN 1
#include <unistd.h>
#include <assert.h>
#include <dlfcn.h> /* GLIBC specific. Exists under cygwin too! */
#include "libao2/afmt.h"
#include "ad_internal.h"

#include "mp_config.h"
#include "help_mp.h"
#include "bswap.h"

#define FF_API_OLD_DECODE_AUDIO 1
#include "libavcodec/avcodec.h"
#include "codecs_ld.h"

#ifndef FF_INPUT_BUFFER_PADDING_SIZE
#define FF_INPUT_BUFFER_PADDING_SIZE 8
#endif

static int acodec_inited;

static const ad_info_t info =
{
	"FFmpeg/libavcodec audio decoders",
	"ffmpeg",
	"Nickols_K",
	"build-in"
};

static const config_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL, NULL}
};

LIBAD_EXTERN(ffmp3)

int preinit(sh_audio_t *sh)
{
  sh->audio_out_minsize=AVCODEC_MAX_AUDIO_FRAME_SIZE;
  return 1;
}

int init(sh_audio_t *sh_audio)
{
   int x;
   float pts;
   AVCodec *lavc_codec=NULL;
   AVCodecContext *lavc_context;
   MSG_V("FFmpeg's libavcodec audio codec\n");
    if(!acodec_inited){
//	avcodec_init();
	avcodec_register_all();
	acodec_inited=1;
    }
    lavc_codec = (AVCodec *)avcodec_find_decoder_by_name(sh_audio->codec->dll_name);
    if(!lavc_codec){
	MSG_ERR(MSGTR_MissingLAVCcodec,sh_audio->codec->dll_name);
	return 0;
    }
    lavc_context = avcodec_alloc_context3(lavc_codec);
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
    lavc_context->codec_type = lavc_codec->type;
    lavc_context->codec_id = lavc_codec->id;
    /* open it */
    if (avcodec_open2(lavc_context, lavc_codec, NULL) < 0) {
        MSG_ERR( MSGTR_CantOpenCodec);
        return 0;
    }
   MSG_V("INFO: libavcodec init OK!\n");

   // Decode at least 1 byte:  (to get header filled)
   x=decode(sh_audio,sh_audio->a_buffer,1,sh_audio->a_buffer_size,&pts);
   if(x>0) sh_audio->a_buffer_len=x;

  sh_audio->channels=lavc_context->channels;
  sh_audio->samplerate=lavc_context->sample_rate;
  switch(lavc_context->sample_fmt) {
    case AV_SAMPLE_FMT_U8:  ///< unsigned 8 bits
	sh_audio->samplesize=1;
	sh_audio->sample_format=AFMT_U8;
	break;
    default:
    case AV_SAMPLE_FMT_S16:             ///< signed 16 bits
	sh_audio->samplesize=2;
	sh_audio->sample_format=AFMT_S16_LE;
	break;
    case AV_SAMPLE_FMT_S32:             ///< signed 32 bits
	sh_audio->samplesize=4;
	sh_audio->sample_format=AFMT_S32_LE;
	break;
    case AV_SAMPLE_FMT_FLT:             ///< float
	sh_audio->samplesize=4;
	sh_audio->sample_format=AFMT_FLOAT32;
	break;
    case AV_SAMPLE_FMT_DBL:             ///< double
	sh_audio->samplesize=8;
	sh_audio->sample_format=AFMT_FLOAT32;
	break;
  }
  sh_audio->i_bps=lavc_context->bit_rate/8;
  return 1;
}

void uninit(sh_audio_t *sh)
{
  AVCodecContext *lavc_context=sh->context;
  avcodec_close(sh->context);
  if (lavc_context->extradata) free(lavc_context->extradata);
  free(lavc_context);
  acodec_inited=0;
}

int control(sh_audio_t *sh,int cmd,any_t* arg, ...)
{
    UNUSED(arg);
    AVCodecContext *lavc_context = sh->context;
    switch(cmd){
	case ADCTRL_RESYNC_STREAM:
	    avcodec_flush_buffers(lavc_context);
	    return CONTROL_TRUE;
	default: break;
    }
    return CONTROL_UNKNOWN;
}

unsigned decode(sh_audio_t *sh_audio,unsigned char *buf,unsigned minlen,unsigned maxlen,float *pts)
{
    unsigned char *start=NULL;
    int y;
    unsigned len=0;
    float apts=0.,null_pts;
    while(len<minlen){
	AVPacket pkt;
	int len2=maxlen;
	int x=ds_get_packet_r(sh_audio->ds,&start,apts?&null_pts:&apts);
	if(x<=0) break; // error
	if(sh_audio->format==mmioFOURCC('d','n','e','t')) swab(start,start,x&(~1));
	av_init_packet(&pkt);
	pkt.data = start;
	pkt.size = x;
	y=avcodec_decode_audio3(sh_audio->context,(int16_t*)buf,&len2,&pkt);
	if(y<0){ MSG_V("lavc_audio: error\n");break; }
	if(y<x)
	{
	    sh_audio->ds->buffer_pos+=y-x;  // put back data (HACK!)
	    if(sh_audio->format==mmioFOURCC('d','n','e','t'))
		swab(start+y,start+y,(x-y)&~(1));
	}
	if(len2>0){
	  //len=len2;break;
	  if(len==0) len=len2; else len+=len2;
	  buf+=len2;
	}
        MSG_DBG2("Decoded %d -> %d  \n",y,len2);
    }
  *pts=apts;
  return len;
}
