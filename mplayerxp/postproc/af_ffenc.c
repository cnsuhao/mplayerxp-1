#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include "config.h"
#include "../libmpcodecs/interface/ffmpeg/avcodec.h"
#include "../libmpcodecs/codecs_ld.h"
#include "../libvo/fastmemcpy.h"
#include "af.h"

#define MIN_LIBAVCODEC_VERSION_INT	((51<<16)+(0<<8)+0)

static void (*avcodec_init_ptr)(void);
#define avcodec_init() (*avcodec_init_ptr)()

static unsigned (*avcodec_version_ptr)(void);
#define avcodec_version() (*avcodec_version_ptr)()

static void (*avcodec_register_all_ptr)(void);
#define avcodec_register_all() (*avcodec_register_all_ptr)()

static AVCodec* (*avcodec_find_encoder_by_name_ptr)(const char *name);
#define avcodec_find_encoder_by_name(a) (*avcodec_find_encoder_by_name_ptr)(a)

static AVCodecContext* (*avcodec_alloc_context_ptr)(void);
#define avcodec_alloc_context() (*avcodec_alloc_context_ptr)()

static int (*avcodec_open_ptr)(AVCodecContext *avctx, AVCodec *codec);
#define avcodec_open(a,b) (*avcodec_open_ptr)(a,b)

static int (*avcodec_encode_audio_ptr)(AVCodecContext *avctx, uint8_t *buf, int buf_size, const short *samples);
#define avcodec_encode_audio(a,b,c,d) (*avcodec_encode_audio_ptr)(a,b,c,d)

static int (*avcodec_close_ptr)(AVCodecContext *avctx);
#define avcodec_close(a) (*avcodec_close_ptr)(a)

static AVCodec* (*first_avcodec_ptr);
#define first_avcodec (*first_avcodec_ptr)

static void *dll_handle;
static int load_dll(const char *libname)
{
  if(!(dll_handle=ld_codec(libname,"http://ffmpeg.sf.net"))) return 0;
  avcodec_init_ptr = ld_sym(dll_handle,"avcodec_init");
  avcodec_version_ptr = ld_sym(dll_handle,"avcodec_version");
  avcodec_register_all_ptr = ld_sym(dll_handle,"avcodec_register_all");
  avcodec_find_encoder_by_name_ptr = ld_sym(dll_handle,"avcodec_find_encoder_by_name");
  avcodec_alloc_context_ptr = ld_sym(dll_handle,"avcodec_alloc_context");
  avcodec_open_ptr = ld_sym(dll_handle,"avcodec_open");
  avcodec_close_ptr = ld_sym(dll_handle,"avcodec_close");
  avcodec_encode_audio_ptr = ld_sym(dll_handle,"avcodec_encode_audio");
  first_avcodec_ptr = ld_sym(dll_handle,"first_avcodec");
  return avcodec_init_ptr && avcodec_register_all_ptr && avcodec_find_encoder_by_name_ptr
	 && avcodec_open_ptr && avcodec_close_ptr && avcodec_encode_audio_ptr
	 && avcodec_version_ptr && avcodec_alloc_context_ptr
	 && first_avcodec_ptr;
}

// Data for specific instances of this filter
typedef struct af_ffenc_s
{
    char cname[256];
    unsigned brate;
    AVCodec *lavc_codec;
    AVCodecContext *lavc_context;
    int acodec_inited;
    unsigned frame_size;
    uint8_t *tail;
    unsigned tail_size;
}af_ffenc_t;

static void print_encoders(void)
{
    AVCodec *p;
    p = first_avcodec;
    while (p) {
        if (p->encode != NULL && p->type == CODEC_TYPE_AUDIO)
            MSG_INFO("%s ",p->name);
        p = p->next;
    }
    MSG_INFO("\n");
}

static uint32_t find_atag(const char *codec)
{
	if(codec == NULL)
	        return 0;

	if(! strcasecmp(codec, "adpcm_ima_wav"))
		return 0x11;

	if(! strcasecmp(codec, "g726"))
		return 0x45;

	if(! strcasecmp(codec, "mp2"))
		return 0x50;

	if(! strcasecmp(codec, "mp3"))
		return 0x55;

	if(! strcasecmp(codec, "ac3"))
		return 0x2000;

	if(! strcasecmp(codec, "dts"))
		return 0x2001;

	if(! strcasecmp(codec, "sonic"))
		return 0x2048;

	if(! strncasecmp(codec, "bonk", 4))
		return 0x2048;

	if(! strcasecmp(codec, "aac"))
		return 0x706D;

	if(! strcasecmp(codec, "vorbis")) // FIXME ???
		return 0xFFFE;

	return 0;
}

// Initialization and runtime control
static int __FASTCALL__ control(struct af_instance_s* af, int cmd, void* arg)
{
  af_ffenc_t *s=af->setup;
  switch(cmd){
  case AF_CONTROL_REINIT:
    if(!s->acodec_inited){
      avcodec_init();
      avcodec_register_all();
      s->acodec_inited=1;
    }
    if(strcmp(s->cname,"help")==0)
    {
	print_encoders();
	return AF_ERROR;
    }
    if(!(s->lavc_codec=avcodec_find_encoder_by_name(s->cname))) {
	MSG_ERR("Can't find encoder %s in libavcodec\n",s->cname);
	return AF_ERROR;
    }
    s->lavc_context=avcodec_alloc_context();
    /* put sample parameters */
    s->lavc_context->bit_rate = s->brate;
    s->lavc_context->sample_rate = ((af_data_t*)arg)->rate;
    s->lavc_context->channels = ((af_data_t*)arg)->nch;
    s->lavc_context->sample_fmt = SAMPLE_FMT_S16;
    /* open it */
    if (avcodec_open(s->lavc_context, s->lavc_codec) < 0) {
        MSG_ERR("could not open codec %s with libavcodec\n",s->cname);
        return AF_ERROR;
    }
    s->frame_size = s->lavc_context->frame_size*((af_data_t*)arg)->nch*2/*bps*/;
    s->tail=malloc(s->frame_size);
    /* correct in format */
    af->data->rate   = ((af_data_t*)arg)->rate;
    af->data->nch    = ((af_data_t*)arg)->nch;
    af->data->format = find_atag(s->cname)<<16;
    af->data->bps    = 2;
    ((af_data_t*)arg)->format=AF_FORMAT_SI | AF_FORMAT_NE;
    MSG_V("[af_ffenc] Was reinitialized, rate=%iHz, nch = %i, format = 0x%08X and bps = %i\n",af->data->rate,af->data->nch,af->data->format,af->data->bps);
    return AF_OK;
  case AF_CONTROL_SHOWCONF:
    MSG_INFO("[af_ffenc] in use [%s %u]\n",s->cname,s->brate);
    return AF_OK;
  case AF_CONTROL_COMMAND_LINE:{
    char *comma;
    strcpy(s->cname,"mp3");
    s->brate=128000;
    if(arg)
    {
	sscanf((char*)arg,"%s", s->cname);
	comma=strchr(s->cname,':');
	if(comma) {
	    *comma='\0';
	    s->brate=atoi(++comma);
	}
    }
    return AF_OK;
  }
  }
  return AF_UNKNOWN;
}

// Deallocate memory 
static void __FASTCALL__ uninit(struct af_instance_s* af)
{
  af_ffenc_t *s=af->setup;
  avcodec_close(s->lavc_context);
  if(s->lavc_context)
    free(s->lavc_context);
  free(s->tail);
  if(af->data)
    free(af->data);
  if(af->setup)
    free(af->setup);
  dlclose(dll_handle);
}

// Filter data through filter
static af_data_t* __FASTCALL__ play(struct af_instance_s* af, af_data_t* data)
{
  unsigned tlen,ilen,olen,delta;
  af_ffenc_t *s=af->setup;
  af_data_t*   	 in = data;
  af_data_t*   	 out = af->data;
  uint8_t *inp,*outp;

  ilen=tlen=in->len;
  if(out->len<tlen) if(out->audio) free(out->audio);
  // Create new buffer and check that it is OK
  out->audio = malloc(tlen);
  MSG_DBG2("ff_encoding %u bytes frame_size=%u tail=%lu\n",tlen,s->frame_size,s->tail_size);
  if(out->audio) {
    out->len=0;
    inp=in->audio;
    outp=out->audio;
    if(s->tail_size && s->tail_size+ilen>=s->frame_size)
    {
	delta=s->frame_size-s->tail_size;
	memcpy(&s->tail[s->tail_size],inp,delta);
	ilen-=delta;
        olen = avcodec_encode_audio(s->lavc_context, outp, tlen, (const short *)s->tail);
	MSG_DBG2("encoding tail %u bytes + %u stream => %u compressed\n",s->tail_size,delta,olen);
	inp+=delta;
	out->len += olen;
	outp+=olen;
	tlen-=olen;
	s->tail_size=0;
    }
    while(ilen>=s->frame_size) {
      olen = avcodec_encode_audio(s->lavc_context, outp, tlen, (const short *)inp);
      MSG_DBG2("encoding [out %p %lu in %p %lu]=>%u compressed\n",outp,tlen,inp,ilen,olen);
      out->len += olen;
      inp+=s->frame_size;
      ilen-=s->frame_size;
      tlen-=olen;
      outp+=olen;
    }
    delta=ilen;
    if(delta)
    {
	MSG_DBG2("encoding append tail %lu bytes to %u existed\n",delta,s->tail_size);
	memcpy(&s->tail[s->tail_size],inp,delta);
	s->tail_size+=delta;
    }
  }
  return out;
}

// Allocate memory and set function pointers
static int __FASTCALL__ open(af_instance_t* af){
  unsigned avc_version;
  if(!load_dll(codec_name("libavcodec"SLIBSUFFIX))) /* try local copy first */
   if(!load_dll("libavcodec-0.4.8"SLIBSUFFIX))
    if(!load_dll("libavcodec"SLIBSUFFIX))
    {
	MSG_ERR("Detected error during loading libffmpeg.so! Try to upgrade this codec\n");
	return AF_ERROR;
    }
    avc_version = (*avcodec_version_ptr)();
    if(avc_version < MIN_LIBAVCODEC_VERSION_INT)
    {
	MSG_ERR("You have wrong version of libavcodec %06X < %06X\n",
		avc_version,MIN_LIBAVCODEC_VERSION_INT);
	return AF_ERROR;
    }
  af->control=control;
  af->uninit=uninit;
  af->play=play;
  af->mul.d=1;
  af->mul.n=1;
  af->data=malloc(sizeof(af_data_t));
  af->setup=calloc(1,sizeof(af_ffenc_t));
  if(af->data == NULL) return AF_ERROR;
  return AF_OK;
}

// Description of this filter
const af_info_t af_info_ffenc = {
    "Encode audio with using of libavcodec",
    "ffenc",
    "Nickols_K",
    "",
    AF_FLAGS_REENTRANT,
    open
};
