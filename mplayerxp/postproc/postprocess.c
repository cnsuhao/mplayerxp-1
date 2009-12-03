/*
    wrapper to call postprocess from libavcodec.so
*/
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <dlfcn.h> /* GLIBC specific. Exists under cygwin too! */

#include "config.h"
#include "postprocess.h"
#include "../libmpcodecs/codecs_ld.h"
#include "../cpudetect.h"
#define MSGT_CLASS MSGT_PP
#include "../__mp_msg.h"

char * npp_options=NULL;
static void *dll_handle=NULL;

static void (*pp_postprocess_ptr)(uint8_t * src[3], int srcStride[3],
                 uint8_t * dst[3], int dstStride[3],
                 int horizontalSize, int verticalSize,
                 QP_STORE_T *QP_store,  int QP_stride,
		 pp_mode_t *mode, pp_context_t *ppContext, int pict_type);
static pp_context_t *(*pp_get_context_ptr)(int width, int height, int flags);
static void (*pp_free_context_ptr)(pp_context_t *ppContext);
static pp_mode_t *(*pp_get_mode_by_name_and_quality_ptr)(char *name, int quality);
static void (*pp_free_mode_ptr)(pp_mode_t *mode);
static char **pp_help_ptr=NULL;

static int load_dll(const char *libname)
{
  if(!(dll_handle=ld_codec(libname,"http://ffmpeg.sf.net"))) return 0;
  pp_postprocess_ptr=dlsym(dll_handle,"pp_postprocess");
  pp_get_context_ptr=dlsym(dll_handle,"pp_get_context");
  pp_free_context_ptr=dlsym(dll_handle,"pp_free_context");
  pp_get_mode_by_name_and_quality_ptr=dlsym(dll_handle,"pp_get_mode_by_name_and_quality");
  pp_free_mode_ptr=dlsym(dll_handle,"pp_free_mode");
  pp_help_ptr=dlsym(dll_handle,"pp_help");
  return pp_postprocess_ptr && pp_get_context_ptr && pp_free_context_ptr &&
	 pp_get_mode_by_name_and_quality_ptr && pp_free_mode_ptr;
}

static int load_avcodec( void )
{
     if(!load_dll(codec_name("libpostproc"SLIBSUFFIX))) /* try local copy first */
      if(!load_dll("libpostproc-0.4.9"SLIBSUFFIX))
	if(!load_dll("libpostproc"SLIBSUFFIX))
	{
	    MSG_ERR("Detected error during loading libpostproc"SLIBSUFFIX"! Try to upgrade this codec\n");
	    return 0;
	} 
	return 1;   
}

extern void exit_player(char *);
pp_context_t *pp2_get_context(int width, int height, int flags)
{
  if(!dll_handle) load_avcodec();
  if(dll_handle)
  {
    flags &= 0x00FFFFFFUL; /* kill cpu related flags */
#if defined( ARCH_X86 ) || defined(ARCH_X86_64)
  if(gCpuCaps.hasMMX)	flags |= PP_CPU_CAPS_MMX;
  if(gCpuCaps.hasMMX2)	flags |= PP_CPU_CAPS_MMX2;
  if(gCpuCaps.has3DNow)	flags |= PP_CPU_CAPS_3DNOW;
#endif
    return (*pp_get_context_ptr)(width,height,flags);
  }
  return NULL;
}

void pp2_free_context(pp_context_t *ppContext)
{
  if(ppContext) {
    if(!dll_handle) load_avcodec();
    if(dll_handle) (*pp_free_context_ptr)(ppContext);
  }
}

void  pp2_postprocess(uint8_t * src[3], int srcStride[3],
                 uint8_t * dst[3], int dstStride[3],
                 int horizontalSize, int verticalSize,
                 QP_STORE_T *QP_store,  int QP_stride,
		 pp_mode_t *mode, pp_context_t *ppContext, int pict_type)
{
  if(!dll_handle) load_avcodec();
  if(dll_handle) (*pp_postprocess_ptr)(src,srcStride,dst,dstStride,
				       horizontalSize,verticalSize,QP_store,
				       QP_stride,mode,ppContext,pict_type);
}

pp_mode_t *pp2_get_mode_by_quality(int quality)
{
  if(!dll_handle) load_avcodec();
  if(dll_handle) return (*pp_get_mode_by_name_and_quality_ptr)(npp_options,quality);
  return NULL;
}

pp_mode_t *pp2_get_mode_by_name_and_quality(char *opts,int quality)
{
  if(!dll_handle) load_avcodec();
  if(dll_handle) return (*pp_get_mode_by_name_and_quality_ptr)(opts,quality);
  return NULL;
}

void pp2_free_mode(pp_mode_t *mode)
{
  if(!dll_handle) load_avcodec();
  if(dll_handle) (*pp_free_mode_ptr)(mode);
}

int	pp2_init(void)
{
  if(!dll_handle) load_avcodec();
  if(dll_handle)
  {
    if(strcmp(npp_options,"help")==0)
    {
	if(pp_help_ptr) MSG_INFO(*pp_help_ptr);
	else		MSG_ERR("Can't access to PP's help\n");
	dlclose(dll_handle);
	dll_handle=NULL;
	exit_player("");
    }
    return 1;
  }
  return 0;
}

void	pp2_uninit(void)
{
    if(dll_handle) dlclose(dll_handle);
    dll_handle=NULL;
}
