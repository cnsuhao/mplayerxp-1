#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "mp_config.h"
#include <dlfcn.h> /* GLIBC specific. Exists under cygwin too! */

#include "help_mp.h"

#include "vd_internal.h"

#include "loader/dshow/DS_VideoDecoder.h"
#include "codecs_ld.h"

static const vd_info_t info = {
	"Win32/DirectShow video codecs",
	"dshow",
	"A'rpi",
	"build-in"
};

static const config_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

LIBVD_EXTERN(dshow)

// to set/get/query special features/parameters
static MPXP_Rc control(sh_video_t *sh,int cmd,any_t* arg,...){
    switch(cmd){
      case VDCTRL_QUERY_MAX_PP_LEVEL:
	return 4;
      case VDCTRL_SET_PP_LEVEL:
	if(!sh->context) return MPXP_Error;
	DS_VideoDecoder_SetValue(sh->context,"Quality",*((int*)arg));
	return MPXP_Ok;
      case VDCTRL_SET_EQUALIZER: {
	va_list ap;
	int value;
	va_start(ap, arg);
	value=va_arg(ap, int);
	va_end(ap);
	value=(value/2)+50;
	if(DS_VideoDecoder_SetValue(sh->context,arg,value)==0)
	    return MPXP_Ok;
	return MPXP_False;
      }
      case VDCTRL_QUERY_FORMAT:
	    if (*((int*)arg) == IMGFMT_YV12 ||
		*((int*)arg) == IMGFMT_I420 ||
		*((int*)arg) == IMGFMT_IYUV ||
		*((int*)arg) == IMGFMT_YVU9 ||
		*((int*)arg) == IMGFMT_YUY2 ||
		*((int*)arg) == IMGFMT_UYVY)
			return MPXP_True;
	    else 	return MPXP_False;
      default: break;
    }
    return MPXP_Unknown;
}

// init driver
static int init(sh_video_t *sh){
    unsigned int out_fmt;
    if(!(sh->context=DS_VideoDecoder_Open(sh->codec->dll_name,&sh->codec->guid, sh->bih, 0, 0))){
        MSG_ERR(MSGTR_MissingDLLcodec,sh->codec->dll_name);
        MSG_HINT("Maybe you forget to upgrade your win32 codecs?? It's time to download the new\n");
        MSG_HINT("package from:  ftp://mplayerhq.hu/MPlayer/releases/w32codec.zip  !\n");
	return 0;
    }
    if(!mpcodecs_config_vo(sh,sh->src_w,sh->src_h,NULL)) return 0;
    out_fmt=sh->codec->outfmt[sh->outfmtidx];
    switch(out_fmt){
    case IMGFMT_YUY2:
    case IMGFMT_UYVY:
	DS_VideoDecoder_SetDestFmt(sh->context,16,out_fmt);break; // packed YUV
    case IMGFMT_YV12:
    case IMGFMT_I420:
    case IMGFMT_IYUV:
	DS_VideoDecoder_SetDestFmt(sh->context,12,out_fmt);break; // planar YUV
    case IMGFMT_YVU9:
        DS_VideoDecoder_SetDestFmt(sh->context,9,out_fmt);break;
    default:
	DS_VideoDecoder_SetDestFmt(sh->context,out_fmt&255,0);    // RGB/BGR
    }
    DS_SetAttr_DivX("Quality",divx_quality);
    DS_VideoDecoder_StartInternal(sh->context);
    MSG_V("INFO: Win32/DShow init OK!\n");
    return 1;
}

// uninit driver
static void uninit(sh_video_t *sh){
    DS_VideoDecoder_Destroy(sh->context);
}

// decode a frame
static mp_image_t* decode(sh_video_t *sh,any_t* data,int len,int flags){
    mp_image_t* mpi;
    if(len<=0) return NULL; // skipped frame

    if(flags&3){
	// framedrop:
	DS_VideoDecoder_DecodeInternal(sh->context, data, len, sh->ds->flags&1, 0);
	return NULL;
    }

    mpi=mpcodecs_get_image(sh, MP_IMGTYPE_TEMP, 0 /*MP_IMGFLAG_ACCEPT_STRIDE*/, 
	sh->src_w, sh->src_h);
    if(mpi->flags&MP_IMGFLAG_DIRECT) mpi->flags|=MP_IMGFLAG_RENDERED;

    DS_VideoDecoder_DecodeInternal(sh->context, data, len, sh->ds->flags&1, mpi->planes[0]);

    return mpi;
}
