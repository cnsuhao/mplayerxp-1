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
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

LIBAD_EXTERN(null)

int init(sh_audio_t *sh)
{
  UNUSED(sh);
  return 1;
}

int preinit(sh_audio_t *sh)
{
  UNUSED(sh);
  return 1;
}

void uninit(sh_audio_t *sh)
{
  UNUSED(sh);
}

int control(sh_audio_t *sh,int cmd,any_t* arg, ...)
{
  UNUSED(sh);
  UNUSED(cmd);
  UNUSED(arg);
  return CONTROL_UNKNOWN;
}

unsigned decode(sh_audio_t *sh_audio,unsigned char *buf,unsigned minlen,unsigned maxlen,float *pts)
{
  UNUSED(sh_audio);
  UNUSED(buf);
  UNUSED(minlen);
  UNUSED(maxlen);
  *pts=0;
  return 0;
}
