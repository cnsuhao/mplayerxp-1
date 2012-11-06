#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "mp_config.h"

#include "help_mp.h"

#include <dlfcn.h> /* GLIBC specific. Exists under cygwin too! */

#include "vd_internal.h"
#include "codecs_ld.h"

#include "loader/dmo/DMO_VideoDecoder.h"
#include "vd_msg.h"

static const vd_info_t info = {
	"Win32/DMO video codecs",
	"dmo",
	"A'rpi",
	"build-in"
};

static const config_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

LIBVD_EXTERN(dmo)

// to set/get/query special features/parameters
static ControlCodes control(sh_video_t *sh,int cmd,any_t* arg,...){
    switch(cmd){
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
    if(!(sh->context=DMO_VideoDecoder_Open(sh->codec->dll_name,&sh->codec->guid, sh->bih, 0, 0))){
        MSG_ERR(MSGTR_MissingDLLcodec,sh->codec->dll_name);
        MSG_HINT("Maybe you forget to upgrade your win32 codecs?? It's time to download the new\n"
                 "package from:  ftp://mplayerhq.hu/MPlayer/releases/w32codec.tar.bz2!\n");
	return 0;
    }
    if(!mpcodecs_config_vo(sh,sh->src_w,sh->src_h,NULL)) return 0;
    out_fmt=sh->codec->outfmt[sh->outfmtidx];
    switch(out_fmt){
    case IMGFMT_YUY2:
    case IMGFMT_UYVY:
	DMO_VideoDecoder_SetDestFmt(sh->context,16,out_fmt);break; // packed YUV
    case IMGFMT_YV12:
    case IMGFMT_I420:
    case IMGFMT_IYUV:
	DMO_VideoDecoder_SetDestFmt(sh->context,12,out_fmt);break; // planar YUV
    case IMGFMT_YVU9:
        DMO_VideoDecoder_SetDestFmt(sh->context,9,out_fmt);break;
    default:
	DMO_VideoDecoder_SetDestFmt(sh->context,out_fmt&255,0);    // RGB/BGR
    }
    DMO_VideoDecoder_StartInternal(sh->context);
    MSG_V("INFO: Win32/DMOhow video codec init OK!\n");
    return 1;
}

// uninit driver
static void uninit(sh_video_t *sh){
    DMO_VideoDecoder_Destroy(sh->context);
}

// decode a frame
static mp_image_t* decode(sh_video_t *sh,any_t* data,int len,int flags){
    mp_image_t* mpi;
    if(len<=0) return NULL; // skipped frame

    if(flags&3){
	// framedrop:
        DMO_VideoDecoder_DecodeInternal(sh->context, data, len, sh->ds->flags&1, 0);
	return NULL;
    }

    mpi=mpcodecs_get_image(sh, MP_IMGTYPE_TEMP, 0 /*MP_IMGFLAG_ACCEPT_STRIDE*/, 
	sh->src_w, sh->src_h);
    if(mpi->flags&MP_IMGFLAG_DIRECT) mpi->flags|=MP_IMGFLAG_RENDERED;

    if(!mpi){	// temporary!
	MSG_ERR("couldn't allocate image for cinepak codec\n");
	return NULL;
    }

    DMO_VideoDecoder_DecodeInternal(sh->context, data, len, sh->ds->flags&1, mpi->planes[0]);

    return mpi;
}
