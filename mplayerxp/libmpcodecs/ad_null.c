#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "ad_internal.h"

static const ad_info_t info =
{
	"Null audio decoder",
	"null",
	"Nickols_K",
	"build-in"
};

static const config_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL, NULL}
};

LIBAD_EXTERN(null)

int init(sh_audio_t *sh)
{
  return 1;
}

int preinit(sh_audio_t *sh)
{
  return 1;
}

void uninit(sh_audio_t *sh){}

int control(sh_audio_t *sh,int cmd,void* arg, ...)
{
  return CONTROL_UNKNOWN;
}

int decode_audio(sh_audio_t *sh_audio,unsigned char *buf,int minlen,int maxlen,float *pts)
{
  *pts=0;
  return -1;
}
