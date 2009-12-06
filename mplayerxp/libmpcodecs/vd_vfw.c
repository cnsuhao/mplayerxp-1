#include "mp_config.h"

#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_MALLOC
#include <malloc.h>
#endif
#include <dlfcn.h> /* GLIBC specific. Exists under cygwin too! */

#include "help_mp.h"

#include "vd_internal.h"
#include "codecs_ld.h"
#include "../libmpdemux/wine/vfw.h"
#include "../libmpdemux/wine/driver.h"
#include "../libmpdemux/aviprint.h"

static const vd_info_t info_vfw = {
	"Win32/VfW video codecs",
	"vfw",
	"A'rpi",
	"based on http://avifile.sf.net",
	"win32 codecs"
};

static const vd_info_t info_vfwex = {
	"Win32/VfWex video codecs",
	"vfwex",
	"A'rpi",
	"based on http://avifile.sf.net",
	"win32 codecs"
};

static const config_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

#define info info_vfw
LIBVD_EXTERN(vfw)
#undef info

#define info info_vfwex
LIBVD_EXTERN(vfwex)
#undef info

static HIC	VFWAPI	(*ICOpen_ptr)(long fccType, long fccHandler, UINT wMode);
#define ICOpen(a,b,c) (*ICOpen_ptr)(a,b,c)
static LRESULT	VFWAPI (*ICSendMessage_ptr)(HIC hic, unsigned int msg, long dw1, long dw2);
#define ICSendMessage(a,b,c,d) (*ICSendMessage_ptr)(a,b,c,d)
static long VFWAPIV (*ICDecompress_ptr)(HIC hic,long dwFlags,LPBITMAPINFOHEADER lpbiFormat,void* lpData,LPBITMAPINFOHEADER lpbi,void* lpBits);
#define ICDecompress(a,b,c,d,e,f) (*ICDecompress_ptr)(a,b,c,d,e,f)
static LRESULT VFWAPI (*ICClose_ptr)(HIC hic);
#define ICClose(a) (*ICClose_ptr)(a)
static long VFWAPIV (*ICUniversalEx_ptr)(HIC hic,int command,LPBITMAPINFOHEADER lpbiFormat,LPBITMAPINFOHEADER lpbi);
#define ICUniversalEx(a,b,c,d) (*ICUniversalEx_ptr)(a,b,c,d)
static long VFWAPIV (*ICDecompressEx_ptr)(HIC hic,long dwFlags,LPBITMAPINFOHEADER lpbiFormat,void* lpData,LPBITMAPINFOHEADER  lpbi,void* lpBits);
#define ICDecompressEx(a,b,c,d,e,f) (*ICDecompressEx_ptr)(a,b,c,d,e,f)

typedef struct vfw_priv_s
{
    BITMAPINFOHEADER *o_bih; /* out format */
    HIC hic;
    int ex;
    unsigned char *palette;
}vfw_priv_t;

static void *dll_handle;

static void set_csp(BITMAPINFOHEADER *o_bih,unsigned int outfmt){
    int yuv = 0;

	switch (outfmt)
	{
	/* planar format */
	case IMGFMT_YV12:
	case IMGFMT_I420:
	case IMGFMT_IYUV:
	    o_bih->biBitCount=12;
	    yuv=1;
	    break;
	case IMGFMT_YVU9:
	case IMGFMT_IF09:
	    o_bih->biBitCount=9;
	    yuv=1;
	    break;
	/* packed format */
	case IMGFMT_YUY2:
        case IMGFMT_UYVY:
        case IMGFMT_YVYU:
    	    o_bih->biBitCount=16;
	    yuv=1;
	    break;
	/* rgb/bgr format */
	case IMGFMT_RGB8:
	case IMGFMT_BGR8:
	    o_bih->biBitCount=8;
	    break;
	case IMGFMT_RGB15:
	case IMGFMT_RGB16:
	case IMGFMT_BGR15:
	case IMGFMT_BGR16:
	    o_bih->biBitCount=16;
	    break;
	case IMGFMT_RGB24:
	case IMGFMT_BGR24:
	    o_bih->biBitCount=24;
	    break;
	case IMGFMT_RGB32:
	case IMGFMT_BGR32:
	    o_bih->biBitCount=32;
	    break;
	default:
	    MSG_ERR("Unsupported image format: %s\n", vo_format_name(outfmt));
	    return;
	}

	o_bih->biSizeImage = abs(o_bih->biWidth * o_bih->biHeight * (o_bih->biBitCount/8));

// Note: we cannot rely on sh->outfmtidx here, it's undefined at this stage!!!
//	if (yuv && !(sh->codec->outflags[sh->outfmtidx] & CODECS_FLAG_YUVHACK))
	if (yuv)
	    o_bih->biCompression = outfmt;
	else
	    o_bih->biCompression = 0;
}

static int load_lib( const char *libname )
{
  if(!(dll_handle=ld_codec(libname,NULL))) return 0;
  ICOpen_ptr = ld_sym(dll_handle,"ICOpen");
  ICClose_ptr = ld_sym(dll_handle,"ICClose");
  ICSendMessage_ptr = ld_sym(dll_handle,"ICSendMessage");
  ICDecompress_ptr = ld_sym(dll_handle,"ICDecompress");
  ICUniversalEx_ptr = ld_sym(dll_handle,"ICUniversalEx");
  ICDecompressEx_ptr = ld_sym(dll_handle,"ICDecompressEx");
  return ICOpen_ptr && ICSendMessage_ptr && ICDecompressEx_ptr &&
	 ICDecompress_ptr && ICClose_ptr && ICUniversalEx_ptr;
}

#define IC_FCCTYPE	sh_video->codec->dll_name
static int init_vfw_video_codec(sh_video_t *sh_video){
  HRESULT ret;
  int temp_len;
  int ex;
  vfw_priv_t *priv = sh_video->context;

  ex = priv->ex;
  MSG_V("======= Win32 (VFW) VIDEO Codec init =======\n");
  priv->hic = ICOpen(IC_FCCTYPE, sh_video->format, ICMODE_DECOMPRESS);
  if(!priv->hic){
    MSG_ERR("ICOpen failed! unknown codec / wrong parameters?\n");
    return 0;
  }

//  sh_video->bih->biBitCount=32;

  temp_len = ICDecompressGetFormatSize(priv->hic, sh_video->bih);
  if(temp_len <= 0){
    MSG_ERR("ICDecompressGetFormatSize failed: Error %d\n", (int)temp_len);
    return 0;
  }

  priv->o_bih=malloc(temp_len);
  memset(priv->o_bih, 0, temp_len);
  priv->o_bih->biSize = temp_len;

  ret = ICDecompressGetFormat(priv->hic, sh_video->bih, priv->o_bih);
  if(ret < 0){
    MSG_ERR("ICDecompressGetFormat failed: Error %d\n", (int)ret);
    return 0;
  }

  // ok, let's set the choosen colorspace:
  set_csp(priv->o_bih,sh_video->codec->outfmt[sh_video->outfmtidx]);

  if(!(sh_video->codec->outflags[sh_video->outfmtidx]&CODECS_FLAG_FLIP)) {
	priv->o_bih->biHeight=-sh_video->bih->biHeight; // flip image!
  }

  if(sh_video->codec->outflags[sh_video->outfmtidx] & CODECS_FLAG_YUVHACK)
	priv->o_bih->biCompression = 0;

    if(verbose)
    {
	MSG_V("Starting decompression, format:\n");
	print_video_header(sh_video->bih,sizeof(BITMAPINFOHEADER));
	MSG_V("Dest fmt:\n");
	print_video_header(priv->o_bih,sizeof(BITMAPINFOHEADER));
    }
  ret = ex ?
      ICDecompressQueryEx(priv->hic, sh_video->bih, priv->o_bih) :
      ICDecompressQuery(priv->hic, sh_video->bih, priv->o_bih);
  if(ret){
    MSG_ERR("ICDecompressQuery failed: Error %d\n", (int)ret);
//    return 0;
  } else
  MSG_V("ICDecompressQuery OK\n");

  ret = ex ?
      ICDecompressBeginEx(priv->hic, sh_video->bih, priv->o_bih) :
      ICDecompressBegin(priv->hic, sh_video->bih, priv->o_bih);
  if(ret){
    MSG_ERR("ICDecompressBegin failed: Error %d\n", (int)ret);
//    return 0;
  }

//  avi_header.our_in_buffer=malloc(avi_header.video.dwSuggestedBufferSize); // FIXME!!!!

  ICSendMessage(priv->hic, ICM_USER+80, (long)(&divx_quality) ,NULL);

  // don't do this palette mess always, it makes div3 dll crashing...
  if(sh_video->codec->outfmt[sh_video->outfmtidx]==IMGFMT_BGR8){
	if(ICDecompressGetPalette(priv->hic, sh_video->bih, priv->o_bih)){
	    priv->palette = (unsigned char*)(priv->o_bih+1);
	    MSG_V("ICDecompressGetPalette OK\n");
	} else {
	    if(sh_video->bih->biSize>=40+4*4)
		priv->palette = (unsigned char*)(sh_video->bih+1);
	}
  }

  MSG_V("VIDEO CODEC Init OK!!! ;-)\n");
  return 1;
}


static int vfw_set_postproc(sh_video_t* sh_video,int quality){
  // Works only with opendivx/divx4 based DLL
  vfw_priv_t *priv=sh_video->context;
  return ICSendMessage(priv->hic, ICM_USER+80, (long)(&quality) ,NULL);
}

static int vfw_close_video_codec(sh_video_t *sh_video)
{
    HRESULT ret;
    vfw_priv_t *priv=sh_video->context;
    
    ret = priv->ex ? ICDecompressEndEx(priv->hic):ICDecompressEnd(priv->hic);
    if (ret)
    {
	MSG_WARN( "ICDecompressEnd failed: %d\n", ret);
	return 0;
    }

    ret = ICClose(priv->hic);
    if (ret)
    {
	MSG_WARN( "ICClose failed: %d\n", ret);
	return 0;
    }
    return 1;
}

// to set/get/query special features/parameters
static int control(sh_video_t *sh,int cmd,void* arg,...){
    vfw_priv_t *priv = sh->context;
    switch(cmd){
    case VDCTRL_QUERY_MAX_PP_LEVEL:
	return 9;
    case VDCTRL_SET_PP_LEVEL:
	vfw_set_postproc(sh,10*(*((int*)arg)));
	return CONTROL_OK;
    // FIXME: make this optional...
    case VDCTRL_QUERY_FORMAT:
      {
	HRESULT ret;
//	if(!(sh->codec->outflags[sh->outfmtidx]&CODECS_FLAG_QUERY))
//	    return CONTROL_UNKNOWN;	// do not query!
	set_csp(priv->o_bih,*((int*)arg));
	if(priv->ex)
	    ret = ICDecompressQueryEx(priv->hic, sh->bih, priv->o_bih);
	else
	    ret = ICDecompressQuery(priv->hic, sh->bih, priv->o_bih);
	if (ret)
	{
	    MSG_DBG2("ICDecompressQuery failed:: Error %d\n", (int)ret);
	    return CONTROL_FALSE;
	}
	return CONTROL_TRUE;
      }
    default: break;
    }
    return CONTROL_UNKNOWN;
}

// init driver
static int init(sh_video_t *sh){
    vfw_priv_t *priv;
    int vfw_ex;
    if(!load_lib(wineld_name("libloader"SLIBSUFFIX))) return 0;
    if(strcmp(sh->codec->driver_name,"vfwex") == 0) vfw_ex=1;
    else					    vfw_ex=0;
    if(!(priv = malloc(sizeof(vfw_priv_t)))) 
    { 
	MSG_ERR(MSGTR_OutOfMemory);
	dlclose(dll_handle);
	return 0; 
    }
    sh->context = priv;
    priv->ex = vfw_ex;
    if(!init_vfw_video_codec(sh)) return 0;
    MSG_V("INFO: Win32 video codec (%s) init OK!\n",CODECDIR"/wine/libloader"SLIBSUFFIX);
    return mpcodecs_config_vo(sh,sh->disp_w,sh->disp_h,NULL);
}

// uninit driver
static void uninit(sh_video_t *sh)
{
  vfw_priv_t *priv=sh->context;
  vfw_close_video_codec(sh);
  free(priv->o_bih);
  free(sh->context);
  dlclose(dll_handle);
}

// decode a frame
static mp_image_t* decode(sh_video_t *sh,void* data,int len,int flags){
    vfw_priv_t *priv = sh->context;
    mp_image_t* mpi;
    HRESULT ret;
    
    if(len<=0) return NULL; // skipped frame

    mpi=mpcodecs_get_image(sh, 
	MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_WIDTH, 
	sh->disp_w, sh->disp_h);
    if(mpi->flags&MP_IMGFLAG_DIRECT) mpi->flags|=MP_IMGFLAG_RENDERED;

    // set stride:  (trick discovered by Andreas Ackermann - thanx!)
    sh->bih->biWidth=mpi->width; //mpi->stride[0]/(mpi->bpp/8);
    priv->o_bih->biWidth=mpi->width; //mpi->stride[0]/(mpi->bpp/8);

    sh->bih->biSizeImage = len;

    if(priv->ex)
    ret = ICDecompressEx(priv->hic,
	  ( (sh->ds->flags&1) ? 0 : ICDECOMPRESS_NOTKEYFRAME ) |
	  ( ((flags&3)==2 && !(sh->ds->flags&1))?(ICDECOMPRESS_HURRYUP|ICDECOMPRESS_PREROL):0 ),
	   sh->bih, data, priv->o_bih, (flags&3) ? 0 : mpi->planes[0]);
    else
    ret = ICDecompress(priv->hic,
	  ( (sh->ds->flags&1) ? 0 : ICDECOMPRESS_NOTKEYFRAME ) |
	  ( ((flags&3)==2 && !(sh->ds->flags&1))?(ICDECOMPRESS_HURRYUP|ICDECOMPRESS_PREROL):0 ),
	   sh->bih, data, priv->o_bih, (flags&3) ? 0 : mpi->planes[0]);

    if ((int)ret){
      MSG_WARN("Error decompressing frame, err=%d\n",ret);
      return NULL;
    }

    // export palette:
    if(mpi->imgfmt==IMGFMT_RGB8 || mpi->imgfmt==IMGFMT_BGR8){
	if (priv->palette)
	{
	    mpi->planes[1] = priv->palette;
	    mpi->flags |= MP_IMGFLAG_RGB_PALETTE;
	    MSG_DBG2("Found and copied palette\n");
	}
	else
	    mpi->planes[1]=NULL;
    }
    return mpi;
}
