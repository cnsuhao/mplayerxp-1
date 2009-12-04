#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h> /* GLIBC specific. Exists under cygwin too! */
#include "ad_internal.h"
#include "interface/mp3.h"
#include "config.h"
#include "../mplayer.h"
#include "../cpudetect.h"
#include "../mm_accel.h"
#include "codecs_ld.h"
#include "libao2/afmt.h"
#include "libao2/audio_out.h"
#include "postproc/af.h"

static const ad_info_t info =
{
	"MPEG layer-123",
	"mp3lib",
	"Nickols_K",
	"Optimized to MMX/SSE/3Dnow!"
};

static const config_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

LIBAD_EXTERN(mp3)

extern int mplayer_audio_read(char *buf,int size);
extern int fakemono;
static sh_audio_t* dec_audio_sh=NULL;

static void (*mpg123_init_ptr)(void);
#define mpg123_init() (*mpg123_init_ptr)()
static void (*mpg123_exit_ptr)(void);
#define mpg123_exit() (*mpg123_exit_ptr)()
static mpg123_handle * (*mpg123_new_ptr)(const char* decoder, int *error);
#define mpg123_new(a,b) (*mpg123_new_ptr)(a,b)
static void (*mpg123_delete_ptr)(mpg123_handle *mh);
#define mpg123_delete(a) (*mpg123_delete_ptr)(a)

static const char* (*mpg123_plain_strerror_ptr)(int errcode);
#define mpg123_plain_strerror(a) (*mpg123_plain_strerror_ptr)(a)
static int (*mpg123_open_feed_ptr)(mpg123_handle *mh);
#define mpg123_open_feed(a) (*mpg123_open_feed_ptr)(a)
static int (*mpg123_close_ptr)(mpg123_handle *mh);
#define mpg123_close(a) (*mpg123_close_ptr)(a)
static int (*mpg123_read_ptr)(mpg123_handle *mh, unsigned char *outmemory, size_t outmemsize, size_t *done);
#define mpg123_read(a,b,c,d) (*mpg123_read_ptr)(a,b,c,d)
static int (*mpg123_feed_ptr)(mpg123_handle *mh, const unsigned char *in, size_t size);
#define mpg123_feed(a,b,c) (*mpg123_feed_ptr)(a,b,c)
static int (*mpg123_decode_ptr)(mpg123_handle *mh, const unsigned char *inmemory, size_t inmemsize,
                         unsigned char *outmemory, size_t outmemsize, size_t *done);
#define mpg123_decode(a,b,c,d,e,f) (*mpg123_decode_ptr)(a,b,c,d,e,f)
static int (*mpg123_getformat_ptr)(mpg123_handle *mh, long *rate, int *channels, int *encoding);
#define mpg123_getformat(a,b,c,d) (*mpg123_getformat_ptr)(a,b,c,d)
static int (*mpg123_param_ptr)(mpg123_handle *mh, enum mpg123_parms type, long value, double fvalue);
#define mpg123_param(a,b,c,d) (*mpg123_param_ptr)(a,b,c,d)
static int (*mpg123_info_ptr)(mpg123_handle *mh, struct mpg123_frameinfo *mi);
#define mpg123_info(a,b) (*mpg123_info_ptr)(a,b)
static const char* (*mpg123_current_decoder_ptr)(mpg123_handle *mh);
#define mpg123_current_decoder(a) (*mpg123_current_decoder_ptr)(a)


static void *dll_handle;
static int load_dll(const char *libname)
{
  if(!(dll_handle=ld_codec(libname,NULL))) return 0;
  mpg123_init_ptr = ld_sym(dll_handle,"mpg123_init");
  mpg123_exit_ptr = ld_sym(dll_handle,"mpg123_exit");
  mpg123_new_ptr = ld_sym(dll_handle,"mpg123_new");
  mpg123_delete_ptr = ld_sym(dll_handle,"mpg123_delete");
  mpg123_plain_strerror_ptr = ld_sym(dll_handle,"mpg123_plain_strerror");
  mpg123_open_feed_ptr = ld_sym(dll_handle,"mpg123_open_feed");
  mpg123_close_ptr = ld_sym(dll_handle,"mpg123_close");
  mpg123_getformat_ptr = ld_sym(dll_handle,"mpg123_getformat");
  mpg123_param_ptr = ld_sym(dll_handle,"mpg123_param");
  mpg123_info_ptr = ld_sym(dll_handle,"mpg123_info");
  mpg123_current_decoder_ptr = ld_sym(dll_handle,"mpg123_current_decoder");
  mpg123_decode_ptr = ld_sym(dll_handle,"mpg123_decode");
  mpg123_read_ptr = ld_sym(dll_handle,"mpg123_read");
  mpg123_feed_ptr = ld_sym(dll_handle,"mpg123_feed");
  return mpg123_decode_ptr && mpg123_init_ptr && mpg123_exit_ptr &&
	 mpg123_new_ptr && mpg123_delete_ptr && mpg123_plain_strerror_ptr &&
	 mpg123_open_feed_ptr && mpg123_close_ptr && mpg123_getformat_ptr &&
	 mpg123_param_ptr && mpg123_info_ptr && mpg123_current_decoder_ptr &&
	 mpg123_read_ptr && mpg123_feed_ptr;
}


int preinit(sh_audio_t *sh)
{
  int rval;
  sh->audio_out_minsize=9216;
  rval = load_dll("libmpg123"SLIBSUFFIX); /* try standard libmpg123 first */
  if(!rval) rval = load_dll(codec_name("libMP3"SLIBSUFFIX)); /* if fail then fallback to internal codec */
  return rval;
}

extern char *audio_codec_param;
int init(sh_audio_t *sh)
{
  // MPEG Audio:
  float pts;
  long param,rate;
  size_t indata_size,done;
  int err=0,nch,enc;
  unsigned char *indata;
  struct mpg123_frameinfo fi;
  dec_audio_sh=sh; // save sh_audio for the callback:
  sh->samplesize=4;
  sh->sample_format=AFMT_FLOAT32;
  mpg123_init();
  sh->context = mpg123_new(NULL,&err);
  if(err) {
    err_exit:
    MSG_ERR("mpg123_init: %s\n",mpg123_plain_strerror(err));
    if(sh->context) mpg123_delete(sh->context);
    mpg123_exit();
    return 0;
  }
  if((err=mpg123_open_feed(sh->context))!=0) goto err_exit;
  param = MPG123_FORCE_STEREO|MPG123_FORCE_FLOAT;
  if(!verbose) param|=MPG123_QUIET;
  mpg123_param(sh->context,MPG123_FLAGS,param,0);
  // Decode first frame (to get header filled)
  err=MPG123_NEED_MORE;
  while(err==MPG123_NEED_MORE) {
    indata_size=ds_get_packet_r(sh->ds,&indata,&pts);
    mpg123_feed(sh->context,indata,indata_size);
    err=mpg123_read(sh->context,sh->a_buffer,sh->a_buffer_size,&done);
  }
  if(err!=MPG123_NEW_FORMAT) {
    MSG_ERR("mpg123_init: within [%d] can't retrieve stream property: %s\n",indata_size,mpg123_plain_strerror(err));
    mpg123_close(sh->context);
    mpg123_delete(sh->context);
    mpg123_exit();
    return 0;
  }
  mpg123_getformat(sh->context, &rate, &nch, &enc);
  sh->samplerate = rate;
  sh->channels = nch;
  mpg123_info(sh->context,&fi);
  sh->i_bps=fi.abr_rate?fi.abr_rate:fi.bitrate;
  // Prints first frame header in ascii.
  {
    static const char *modes[4] = { "Stereo", "Joint-Stereo", "Dual-Channel", "Mono" };
    static const char *layers[4] = { "???" , "I", "II", "III" };
    static const char *vers[4] = { "1.0", "2.0", "2.5", "???" };
    static const char *xbr[4]  = { "CBR", "VBR", "ABR", "???" };

    MSG_INFO("\rmpg123_init: MPEG-%s [Layer:%s (%s)], Hz=%d %d-kbit %s, BPF=%d Out=32-bit\n"
	,vers[fi.version&0x3]
	,layers[fi.layer&0x3]
	,xbr[fi.vbr&0x3]
	,fi.rate
	,sh->i_bps
	,modes[fi.mode&0x3]
	,fi.framesize);
    MSG_INFO("mpg123_init: Copyrght=%s Orig=%s CRC=%s Priv=%s Emphas=%d Optimiz=%s\n"
	,fi.flags&MPG123_COPYRIGHT?"Yes":"No"
	,fi.flags&MPG123_ORIGINAL?"Yes":"No"
	,fi.flags&MPG123_CRC?"Yes":"No"
	,fi.flags&MPG123_PRIVATE?"Yes":"No"
	,fi.emphasis,mpg123_current_decoder(sh->context));
  }
  return 1;
}

void uninit(sh_audio_t *sh)
{
  mpg123_close(sh->context);
  mpg123_delete(sh->context);
  mpg123_exit();
  dlclose(dll_handle);
}

int control(sh_audio_t *sh,int cmd,void* arg, ...)
{
    float pts;
    switch(cmd)
    {
      case ADCTRL_RESYNC_STREAM:
#if 0
          MP3_DecodeFrame(NULL,-2,&pts); // resync
          MP3_DecodeFrame(NULL,-2,&pts); // resync
          MP3_DecodeFrame(NULL,-2,&pts); // resync
#endif
	  return CONTROL_TRUE;
      case ADCTRL_SKIP_FRAME:
#if 0
	  MP3_DecodeFrame(NULL,-2,&pts); // skip MPEG frame
#endif
	  return CONTROL_TRUE;
      default:
	  return CONTROL_UNKNOWN;
    }
  return CONTROL_UNKNOWN;
}

int decode_audio(sh_audio_t *sh,unsigned char *buf,int minlen,int maxlen,float *pts)
{
    unsigned char *indata=NULL;
    int err,indata_size;
    size_t done;
    indata_size=ds_get_packet_r(sh->ds,&indata,&pts);
    mpg123_feed(sh->context,indata,indata_size);
    err=mpg123_read(sh->context,buf,maxlen,&done);
    if(!((err==MPG123_OK)||(err==MPG123_NEED_MORE)))
	MSG_ERR("mpg123_read = %s done = %u minlen = %u\n",mpg123_plain_strerror(err),done,minlen);
    return done;
}
