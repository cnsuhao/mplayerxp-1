/*
    wrapper to call postprocess from libavcodec.so
*/
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <dlfcn.h> /* GLIBC specific. Exists under cygwin too! */

#include "../mp_config.h"
#include "postprocess.h"
#include "../libmpcodecs/codecs_ld.h"
#include "../cpudetect.h"
#define MSGT_CLASS MSGT_PP
#include "../__mp_msg.h"

char * npp_options=NULL;

extern void exit_player(char *);
pp_context_t *pp2_get_context(int width, int height, int flags)
{
  flags &= 0x00FFFFFFUL; /* kill cpu related flags */
#if defined( ARCH_X86 ) || defined(ARCH_X86_64)
  if(gCpuCaps.hasMMX)	flags |= PP_CPU_CAPS_MMX;
  if(gCpuCaps.hasMMX2)	flags |= PP_CPU_CAPS_MMX2;
  if(gCpuCaps.has3DNow)	flags |= PP_CPU_CAPS_3DNOW;
#endif
  return pp_get_context(width,height,flags);
}

int	pp2_init(void)
{
    if(strcmp(npp_options,"help")==0)
    {
	if(pp_help)	MSG_INFO(*pp_help);
	else		MSG_ERR("Can't access to PP's help\n");
	exit_player("");
    }
    return 1;
}

void	pp2_uninit(void)
{
}
