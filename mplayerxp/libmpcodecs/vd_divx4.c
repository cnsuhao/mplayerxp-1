/*
   HACKING notes:
   first time it was OpenDivx project by Mayo (unsupported by mplayerxp)
   second it was divx4linux-beta by divx networks
   third it was divx4linux by divx network
   last it became divx5 by divx networks
   All these libraries have the same name libdivxdecore.so :(
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>

#include "mp_config.h"
#include "help_mp.h"

#include <dlfcn.h> /* GLIBC specific. Exists under cygwin too! */

#include "vd_internal.h"
#include "codecs_ld.h"
#include "libvo/video_out.h"

static const vd_info_t info = {
	"DivX4Linux lib (divx4/5 mode)",
	"divx4",
	"Nickols_K",
	"http://www.divx.com",
	"native codecs"
};

static const config_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

LIBVD_EXTERN(divx4)

#include "interface/divx4linux.h"

#define DIVX4LINUX_BETA 0
#define DIVX4LINUX	1
#define DIVX5LINUX	2

typedef struct
{
    void *pHandle;
    LibQDecoreFunction* decoder;
    int resync;
}priv_t;

static LibQDecoreFunction* (*getDecore_ptr)(unsigned long format);
static void *dll_handle;

// to set/get/query special features/parameters
static int control(sh_video_t *sh,int cmd,void* arg,...){
    priv_t*p=sh->context;
    switch(cmd){
	case VDCTRL_QUERY_MAX_PP_LEVEL: return 100; // for divx4linux
	case VDCTRL_SET_PP_LEVEL: {
	    int iOperation = DEC_PAR_POSTPROCESSING;
	    int iLevel = *((int*)arg);
	    if(iLevel<0 || iLevel>100) iLevel=100;
	    return p->decoder(p->pHandle,DEC_OPT_SET,&iOperation,&iLevel)==DEC_OK?CONTROL_OK:CONTROL_FALSE;
	}
	case VDCTRL_SET_EQUALIZER: {
	    int value;
	    int option;
	    va_list ap;
	    va_start(ap, arg);
	    value=va_arg(ap, int);
	    va_end(ap);

	    if(!strcmp(arg,VO_EC_BRIGHTNESS)) option=DEC_PAR_BRIGHTNESS;
	    else if(!strcmp(arg, VO_EC_CONTRAST)) option=DEC_PAR_CONTRAST;
	    else if(!strcmp(arg,VO_EC_SATURATION)) option=DEC_PAR_SATURATION;
	    else return CONTROL_FALSE;

	    value = (value * 256) / 100;
	    return p->decoder(p->pHandle,DEC_OPT_SET,&option,&value)==DEC_OK?CONTROL_OK:CONTROL_FALSE;
	}
	case VDCTRL_QUERY_FORMAT:
	    if (*((int*)arg) == IMGFMT_YV12 || 
		*((int*)arg) == IMGFMT_I420 || 
		*((int*)arg) == IMGFMT_IYUV)
			return CONTROL_TRUE;
	    else 	return CONTROL_FALSE;
	case VDCTRL_RESYNC_STREAM:
	    p->resync=1;
	    return CONTROL_TRUE;
    }
    return CONTROL_UNKNOWN;
}


static int load_lib( const char *libname )
{
  if(!(dll_handle=ld_codec(libname,"http://labs.divx.com/DivXLinuxCodec"))) return 0;
  getDecore_ptr = ld_sym(dll_handle,"getDecore");
  return getDecore_ptr != NULL;
}

// init driver
static int init(sh_video_t *sh){
    DecInit dinit;
    priv_t*p;
    int bits=12;
    if(!load_lib("libdivx"SLIBSUFFIX)) return 0;
    if(!(mpcodecs_config_vo(sh,sh->disp_w,sh->disp_h,NULL))) return 0;
    switch(sh->codec->outfmt[sh->outfmtidx]){
	case IMGFMT_YV12:
	case IMGFMT_I420:
	case IMGFMT_IYUV: break;
	default:
	  MSG_ERR("Unsupported out_fmt: 0x%X\n",sh->codec->outfmt[sh->outfmtidx]);
	  return 0;
    }
    if(!(p=malloc(sizeof(priv_t)))) { MSG_ERR("Out of memory\n"); return 0; }
    sh->context=p;
    memset(p,0,sizeof(priv_t));
    if(!(p->decoder=getDecore_ptr(sh->format))) {
	char *p=(char *)&(sh->format);
	MSG_ERR("Can't find decoder for %c%c%c%c fourcc\n",p[0],p[1],p[2],p[3]);
	return 0;
    }
    dinit.formatOut.fourCC=sh->codec->outfmt[sh->outfmtidx];
    dinit.formatOut.bpp=bits;
    dinit.formatOut.width=sh->disp_w;
    dinit.formatOut.height=sh->disp_h;
    dinit.formatOut.pixelAspectX=1;
    dinit.formatOut.pixelAspectY=1;
    dinit.formatOut.sizeMax=sh->disp_w*sh->disp_h*bits;
    dinit.formatIn.fourCC=sh->format;
    dinit.formatIn.framePeriod=sh->fps;
    if(p->decoder(NULL, DEC_OPT_INIT, (void*) &p->pHandle, &dinit)!=DEC_OK) {
	char *p=(char *)&(dinit.formatOut);
	MSG_ERR("Can't find decoder for %c%c%c%c fourcc\n",p[0],p[1],p[2],p[3]);
    }
    MSG_V("INFO: DivX4Linux (libdivx.so) video codec init OK!\n");
    fflush(stdout);
    return 1;
}

// uninit driver
static void uninit(sh_video_t *sh){
    priv_t*p=sh->context;
    p->decoder(p->pHandle, DEC_OPT_RELEASE, 0, 0);
    dlclose(dll_handle);
    free(p);
}

// decode a frame
static mp_image_t* decode(sh_video_t *sh,void* data,int len,int flags){
    priv_t*p=sh->context;
    mp_image_t* mpi;
    DecFrame decFrame;

    memset(&decFrame,0,sizeof(DecFrame));
    if(len<=0) return NULL; // skipped frame

    mpi=mpcodecs_get_image(sh, MP_IMGTYPE_TEMP, MP_IMGFLAG_PRESERVE | MP_IMGFLAG_ACCEPT_WIDTH,
	sh->disp_w, sh->disp_h);
    if(mpi->flags&MP_IMGFLAG_DIRECT) mpi->flags|=MP_IMGFLAG_RENDERED;

    decFrame.bitstream.pBuff = data;
    decFrame.bitstream.iLength = len;
    decFrame.shallowDecode = (flags&3)?1:0;
    decFrame.pBmp=mpi->planes[0];
    decFrame.bmpStride=(mpi->flags&(MP_IMGFLAG_YUV|MP_IMGFLAG_DIRECT))==(MP_IMGFLAG_YUV|MP_IMGFLAG_DIRECT)?
		     mpi->flags&MP_IMGFLAG_PLANAR?mpi->stride[0]:mpi->stride[0]/2:
		     mpi->width;
    if(p->resync) { decFrame.bBitstreamUpdated=1; p->resync=0; }
    
    if(p->decoder(p->pHandle, DEC_OPT_FRAME, &decFrame, 0)!=DEC_OK) MSG_WARN("divx: Error happened during decoding\n");

    return mpi;
}

