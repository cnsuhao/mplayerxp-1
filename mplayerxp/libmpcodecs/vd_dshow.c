#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "mp_config.h"
#include <dlfcn.h> /* GLIBC specific. Exists under cygwin too! */

#include "help_mp.h"

#include "vd_internal.h"

#include "interface/dshow/DS_VideoDecoder.h"
#include "codecs_ld.h"

static const vd_info_t info = {
	"DirectShow video codecs",
	"dshow",
	"A'rpi",
	"based on http://avifile.sf.net",
	"win32 codecs"
};

static const config_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

LIBVD_EXTERN(dshow)

void* (*DS_VideoDecoder_Open_ptr)(char* dllname, GUID* guid, BITMAPINFOHEADER* format, int flip, int maxauto);

void (*DS_VideoDecoder_StartInternal_ptr)(void* _handle);

void (*DS_VideoDecoder_Destroy_ptr)(void* _handle);

int (*DS_VideoDecoder_DecodeInternal_ptr)(void* _handle, char* src, int size, int is_keyframe, char* dest);

int (*DS_VideoDecoder_SetDestFmt_ptr)(void* _handle, int bits, int csp);

int (*DS_VideoDecoder_SetValue_ptr)(void* _handle, char* name, int value);
int (*DS_SetAttr_DivX_ptr)(char* attribute, int value);

static void *dll_handle;

static int load_lib( const char *libname )
{
  if(!(dll_handle=ld_codec(libname,NULL))) return 0;
  DS_VideoDecoder_Open_ptr = ld_sym(dll_handle,"DS_VideoDecoder_Open");
  DS_VideoDecoder_StartInternal_ptr = ld_sym(dll_handle,"DS_VideoDecoder_StartInternal");
  DS_VideoDecoder_Destroy_ptr = ld_sym(dll_handle,"DS_VideoDecoder_Destroy");
  DS_VideoDecoder_DecodeInternal_ptr = ld_sym(dll_handle,"DS_VideoDecoder_DecodeInternal");
  DS_VideoDecoder_SetDestFmt_ptr = ld_sym(dll_handle,"DS_VideoDecoder_SetDestFmt");
  DS_VideoDecoder_SetValue_ptr = ld_sym(dll_handle,"DS_VideoDecoder_SetValue");
  DS_SetAttr_DivX_ptr = ld_sym(dll_handle,"DS_SetAttr_DivX");
  return DS_VideoDecoder_Open_ptr && DS_VideoDecoder_StartInternal_ptr &&
	 DS_VideoDecoder_Destroy_ptr && DS_VideoDecoder_DecodeInternal_ptr &&
	 DS_VideoDecoder_SetDestFmt_ptr && DS_VideoDecoder_SetValue_ptr &&
	 DS_SetAttr_DivX_ptr;
}

// to set/get/query special features/parameters
static int control(sh_video_t *sh,int cmd,void* arg,...){
    switch(cmd){
      case VDCTRL_QUERY_MAX_PP_LEVEL:
	return 4;
      case VDCTRL_SET_PP_LEVEL:
	if(!sh->context) return CONTROL_ERROR;
	(*DS_VideoDecoder_SetValue_ptr)(sh->context,"Quality",*((int*)arg));
	return CONTROL_OK;
      case VDCTRL_SET_EQUALIZER: {
	va_list ap;
	int value;
	va_start(ap, arg);
	value=va_arg(ap, int);
	va_end(ap);
	value=(value/2)+50;
	if((*DS_VideoDecoder_SetValue_ptr)(sh->context,arg,value)==0)
	    return CONTROL_OK;
	return CONTROL_FALSE;
      }
      case VDCTRL_QUERY_FORMAT:
	    if (*((int*)arg) == IMGFMT_YV12 ||
		*((int*)arg) == IMGFMT_I420 ||
		*((int*)arg) == IMGFMT_IYUV ||
		*((int*)arg) == IMGFMT_YVU9 ||
		*((int*)arg) == IMGFMT_YUY2 ||
		*((int*)arg) == IMGFMT_UYVY)
			return CONTROL_TRUE;
	    else 	return CONTROL_FALSE;
      default: break;
    }
    return CONTROL_UNKNOWN;
}

// init driver
static int init(sh_video_t *sh){
    unsigned int out_fmt;
    if(!load_lib(wineld_name("DS_Filter"SLIBSUFFIX))) return 0;
    if(!(sh->context=(*DS_VideoDecoder_Open_ptr)(sh->codec->dll_name,&sh->codec->guid, sh->bih, 0, 0))){
        MSG_ERR(MSGTR_MissingDLLcodec,sh->codec->dll_name);
        MSG_HINT("Maybe you forget to upgrade your win32 codecs?? It's time to download the new\n");
        MSG_HINT("package from:  ftp://mplayerhq.hu/MPlayer/releases/w32codec.zip  !\n");
	return 0;
    }
    if(!mpcodecs_config_vo(sh,sh->disp_w,sh->disp_h,NULL)) return 0;
    out_fmt=sh->codec->outfmt[sh->outfmtidx];
    switch(out_fmt){
    case IMGFMT_YUY2:
    case IMGFMT_UYVY:
	(*DS_VideoDecoder_SetDestFmt_ptr)(sh->context,16,out_fmt);break; // packed YUV
    case IMGFMT_YV12:
    case IMGFMT_I420:
    case IMGFMT_IYUV:
	(*DS_VideoDecoder_SetDestFmt_ptr)(sh->context,12,out_fmt);break; // planar YUV
    case IMGFMT_YVU9:
        (*DS_VideoDecoder_SetDestFmt_ptr)(sh->context,9,out_fmt);break;
    default:
	(*DS_VideoDecoder_SetDestFmt_ptr)(sh->context,out_fmt&255,0);    // RGB/BGR
    }
    (*DS_SetAttr_DivX_ptr)("Quality",divx_quality);
    (*DS_VideoDecoder_StartInternal_ptr)(sh->context);
    MSG_V("INFO: Win32/DShow (%s) video codec init OK!\n",CODECDIR"/wine/DS_Filter"SLIBSUFFIX);
    return 1;
}

// uninit driver
static void uninit(sh_video_t *sh){
    (*DS_VideoDecoder_Destroy_ptr)(sh->context);
    dlclose(dll_handle);
}

// decode a frame
static mp_image_t* decode(sh_video_t *sh,void* data,int len,int flags){
    mp_image_t* mpi;
    if(len<=0) return NULL; // skipped frame
    
    if(flags&3){
	// framedrop:
        (*DS_VideoDecoder_DecodeInternal_ptr)(sh->context, data, len, sh->ds->flags&1, 0);
	return NULL;
    }
    
    mpi=mpcodecs_get_image(sh, MP_IMGTYPE_TEMP, 0 /*MP_IMGFLAG_ACCEPT_STRIDE*/, 
	sh->disp_w, sh->disp_h);
    if(mpi->flags&MP_IMGFLAG_DIRECT) mpi->flags|=MP_IMGFLAG_RENDERED;

    (*DS_VideoDecoder_DecodeInternal_ptr)(sh->context, data, len, sh->ds->flags&1, mpi->planes[0]);

    return mpi;
}
