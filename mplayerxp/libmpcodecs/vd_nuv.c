#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include "mp_config.h"

#include "vd_internal.h"
#include "codecs_ld.h"

static const vd_info_t info = {
	"NuppelVideo decoder",
	"nuv",
	"A'rpi",
	"Alex & Panagiotis Issaris <takis@lumumba.luc.ac.be>",
	"native codecs"
};

static const config_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

LIBVD_EXTERN(nuv)

static void (*decode_nuv_ptr)(
  unsigned char *encoded,
  int encoded_size,
  unsigned char *decoded,
  int width,
  int height);
#define decode_nuv(a,b,c,d,e) (*decode_nuv_ptr)(a,b,c,d,e)

static void *dll_handle;
static int load_lib( const char *libname )
{
  if(!(dll_handle=ld_codec(libname,NULL))) return 0;
  decode_nuv_ptr = ld_sym(dll_handle,"decode_nuv");
  return decode_nuv_ptr != NULL;
}

// to set/get/query special features/parameters
static int control(sh_video_t *sh,int cmd,void* arg,...){
    switch(cmd) {
      case VDCTRL_QUERY_FORMAT:
	    if (*((int*)arg) == IMGFMT_I420 ||
		*((int*)arg) == IMGFMT_IYUV) 
			return CONTROL_TRUE;
	    else 	return CONTROL_FALSE;
      default: break;
    }
    return CONTROL_UNKNOWN;
}

// init driver
static int init(sh_video_t *sh){
    if(!load_lib(codec_name("libnuppelvideo"SLIBSUFFIX))) return 0;
    return mpcodecs_config_vo(sh,sh->disp_w,sh->disp_h,NULL);
}

// uninit driver
static void uninit(sh_video_t *sh){
  dlclose(dll_handle);
}

// decode a frame
static mp_image_t* decode(sh_video_t *sh,void* data,int len,int flags){
    mp_image_t* mpi;
    if(len<=0) return NULL; // skipped frame
    
    mpi=mpcodecs_get_image(sh, MP_IMGTYPE_TEMP, 0, 
	sh->disp_w, sh->disp_h);
    if(mpi->flags&MP_IMGFLAG_DIRECT) mpi->flags|=MP_IMGFLAG_RENDERED;

    decode_nuv(data, len, mpi->planes[0], sh->disp_w, sh->disp_h);

    return mpi;
}
