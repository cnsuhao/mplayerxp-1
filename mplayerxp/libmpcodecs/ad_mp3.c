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

static void (*MP3_Init_ptr)(int fakemono,unsigned accel,int (*audio_read)(char *buf,int size),const char *param);
#define MP3_Init(a,b,c,d) (*MP3_Init_ptr)(a,b,c,d)
static int (*MP3_DecodeFrame_ptr)(unsigned char *hova,short single,float *pts);
#define MP3_DecodeFrame(a,b,c) (*MP3_DecodeFrame_ptr)(a,b,c)
static void (*MP3_PrintHeader_ptr)(void);
#define MP3_PrintHeader() (*MP3_PrintHeader_ptr)()

static int (*MP3_channels_ptr);
static int (*MP3_ave_bitrate_ptr);
static int (*MP3_samplerate_ptr);

static void *dll_handle;
static int load_dll(const char *libname)
{
  if(!(dll_handle=ld_codec(libname,NULL))) return 0;
  MP3_DecodeFrame_ptr = ld_sym(dll_handle,"MP3_DecodeFrame");
  MP3_Init_ptr = ld_sym(dll_handle,"MP3_Init");
  MP3_PrintHeader_ptr = ld_sym(dll_handle,"MP3_PrintHeader");
  MP3_channels_ptr = ld_sym(dll_handle,"MP3_channels");
  MP3_ave_bitrate_ptr = ld_sym(dll_handle,"MP3_ave_bitrate");
  MP3_samplerate_ptr = ld_sym(dll_handle,"MP3_samplerate");
  return MP3_DecodeFrame_ptr && MP3_Init_ptr && MP3_PrintHeader_ptr &&
	 MP3_channels_ptr && MP3_ave_bitrate_ptr && MP3_samplerate_ptr;
}


int preinit(sh_audio_t *sh)
{
  sh->audio_out_minsize=9216;
  return load_dll(codec_name("libMP3"SLIBSUFFIX));
}

extern char *audio_codec_param;
int init(sh_audio_t *sh)
{
  // MPEG Audio:
  float pts;
  dec_audio_sh=sh; // save sh_audio for the callback:
  sh->samplesize=4;
  sh->sample_format=AFMT_FLOAT32;
  MP3_Init(fakemono,mplayer_accel,&mplayer_audio_read,audio_codec_param);
  *MP3_samplerate_ptr=*MP3_channels_ptr=0;
  sh->a_buffer_len=MP3_DecodeFrame(sh->a_buffer,-1,&pts);
  sh->channels=2; // hack
  sh->samplerate=*MP3_samplerate_ptr;
  sh->i_bps=*MP3_ave_bitrate_ptr/8;
  MP3_PrintHeader();
  return 1;
}

void uninit(sh_audio_t *sh)
{
  dlclose(dll_handle);
}

int control(sh_audio_t *sh,int cmd,void* arg, ...)
{
    float pts;
    switch(cmd)
    {
      case ADCTRL_RESYNC_STREAM:
          MP3_DecodeFrame(NULL,-2,&pts); // resync
          MP3_DecodeFrame(NULL,-2,&pts); // resync
          MP3_DecodeFrame(NULL,-2,&pts); // resync
	  return CONTROL_TRUE;
      case ADCTRL_SKIP_FRAME:
	  MP3_DecodeFrame(NULL,-2,&pts); // skip MPEG frame
	  return CONTROL_TRUE;
      default:
	  return CONTROL_UNKNOWN;
    }
  return CONTROL_UNKNOWN;
}

int decode_audio(sh_audio_t *sh_audio,unsigned char *buf,int minlen,int maxlen,float *pts)
{
   int retval=-1;
   while(retval<0)
   {
    retval = MP3_DecodeFrame(buf,-1,pts);
    if(retval<0) control(sh_audio,ADCTRL_RESYNC_STREAM,NULL);
   }
   return retval;
}
