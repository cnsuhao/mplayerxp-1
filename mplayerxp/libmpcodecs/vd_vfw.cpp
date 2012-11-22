#include "mp_config.h"
#include "mplayerxp.h"

#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h> /* GLIBC specific. Exists under cygwin too! */

#include "help_mp.h"

#include "vd_internal.h"
#include "codecs_ld.h"
#include "osdep/mplib.h"
#include "loader/wine/vfw.h"
#include "loader/wine/driver.h"
#include "libmpdemux/aviprint.h"

static const vd_info_t info_vfw = {
    "Win32/VfW video codecs",
    "vfw",
    "A'rpi",
    "build-in"
};

static const vd_info_t info_vfwex = {
    "Win32/VfWex video codecs",
    "vfwex",
    "A'rpi",
    "build-in"
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

static const video_probe_t* __FASTCALL__ probe(sh_video_t *sh,uint32_t fourcc) { return NULL; }

typedef struct priv_s {
    BITMAPINFOHEADER *o_bih; /* out format */
    HIC hic;
    int ex;
    unsigned char *palette;
}priv_t;

static void set_csp(BITMAPINFOHEADER *o_bih,unsigned int outfmt){
    int yuv = 0;

    switch (outfmt) {
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

#define IC_FCCTYPE	sh_video->codec->dll_name
static MPXP_Rc init_vfw_video_codec(sh_video_t *sh_video){
    HRESULT ret;
    int temp_len;
    int ex;
    priv_t *priv = reinterpret_cast<priv_t*>(sh_video->context);

    ex = priv->ex;
    MSG_V("======= Win32 (VFW) VIDEO Codec init =======\n");
    priv->hic = ICOpen((long)IC_FCCTYPE, sh_video->fourcc, ICMODE_DECOMPRESS);
    if(!priv->hic){
	MSG_ERR("ICOpen failed! unknown codec / wrong parameters?\n");
	return MPXP_False;
    }

//  sh_video->bih->biBitCount=32;

    temp_len = ICDecompressGetFormatSize(priv->hic, sh_video->bih);
    if(temp_len <= 0){
	MSG_ERR("ICDecompressGetFormatSize failed: Error %d\n", (int)temp_len);
	return MPXP_False;
    }

    priv->o_bih=(BITMAPINFOHEADER*)mp_mallocz(temp_len);
    priv->o_bih->biSize = temp_len;

    ret = ICDecompressGetFormat(priv->hic, sh_video->bih, priv->o_bih);
    if(ret < 0){
	MSG_ERR("ICDecompressGetFormat failed: Error %d\n", (int)ret);
	return MPXP_False;
    }

    // ok, let's set the choosen colorspace:
    set_csp(priv->o_bih,sh_video->codec->outfmt[sh_video->outfmtidx]);

    if(!(sh_video->codec->outflags[sh_video->outfmtidx]&CODECS_FLAG_FLIP)) {
	priv->o_bih->biHeight=-sh_video->bih->biHeight; // flip image!
    }

    if(sh_video->codec->outflags[sh_video->outfmtidx] & CODECS_FLAG_YUVHACK)
	priv->o_bih->biCompression = 0;

    if(mp_conf.verbose) {
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
    } else MSG_V("ICDecompressQuery OK\n");

    ret = ex ?
	ICDecompressBeginEx(priv->hic, sh_video->bih, priv->o_bih) :
	ICDecompressBegin(priv->hic, sh_video->bih, priv->o_bih);
    if(ret){
	MSG_ERR("ICDecompressBegin failed: Error %d\n", (int)ret);
//    return 0;
    }

//  avi_header.our_in_buffer=mp_malloc(avi_header.video.dwSuggestedBufferSize); // FIXME!!!!

    ICSendMessage(priv->hic, ICM_USER+80, (long)(&MPXPCtx->output_quality), 0);

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
    return MPXP_Ok;
}

static int vfw_set_postproc(sh_video_t* sh_video,int quality){
    // Works only with opendivx/divx4 based DLL
    priv_t *priv=reinterpret_cast<priv_t*>(sh_video->context);
    return ICSendMessage(priv->hic, ICM_USER+80, (long)(&quality), 0);
}

static MPXP_Rc vfw_close_video_codec(sh_video_t *sh_video)
{
    HRESULT ret;
    priv_t *priv=reinterpret_cast<priv_t*>(sh_video->context);

    ret = priv->ex ? ICDecompressEndEx(priv->hic):ICDecompressEnd(priv->hic);
    if (ret) {
	MSG_WARN( "ICDecompressEnd failed: %d\n", ret);
	return MPXP_False;
    }

    ret = ICClose(priv->hic);
    if (ret) {
	MSG_WARN( "ICClose failed: %d\n", ret);
	return MPXP_False;
    }
    return MPXP_Ok;
}

// to set/get/query special features/parameters
static MPXP_Rc control(sh_video_t *sh,int cmd,any_t* arg,...){
    priv_t *priv = reinterpret_cast<priv_t*>(sh->context);
    switch(cmd){
	case VDCTRL_QUERY_MAX_PP_LEVEL:
	    *((unsigned*)arg)=9;
	    return MPXP_Ok;
	case VDCTRL_SET_PP_LEVEL:
	    vfw_set_postproc(sh,10*(*((int*)arg)));
	    return MPXP_Ok;
	// FIXME: make this optional...
	case VDCTRL_QUERY_FORMAT: {
	    HRESULT ret;
//	if(!(sh->codec->outflags[sh->outfmtidx]&CODECS_FLAG_QUERY))
//	    return MPXP_Unknown;	// do not query!
	    set_csp(priv->o_bih,*((int*)arg));
	    if(priv->ex)
		ret = ICDecompressQueryEx(priv->hic, sh->bih, priv->o_bih);
	    else
		ret = ICDecompressQuery(priv->hic, sh->bih, priv->o_bih);
	    if (ret) {
		MSG_DBG2("ICDecompressQuery failed:: Error %d\n", (int)ret);
		return MPXP_False;
	    }
	    return MPXP_True;
	}
	default: break;
    }
    return MPXP_Unknown;
}

// init driver
static MPXP_Rc init(sh_video_t *sh,any_t* libinput){
    priv_t *priv;
    int vfw_ex;
    if(strcmp(sh->codec->driver_name,"vfwex") == 0) vfw_ex=1;
    else					    vfw_ex=0;
    if(!(priv = new(zeromem) priv_t)) {
	MSG_ERR(MSGTR_OutOfMemory);
	return MPXP_False;
    }
    sh->context = priv;
    priv->ex = vfw_ex;
    if(init_vfw_video_codec(sh)!=MPXP_Ok) return MPXP_False;
    MSG_V("INFO: Win32/VFW init OK!\n");
    return mpcodecs_config_vo(sh,sh->src_w,sh->src_h,libinput);
}

// uninit driver
static void uninit(sh_video_t *sh)
{
    priv_t *priv=reinterpret_cast<priv_t*>(sh->context);
    vfw_close_video_codec(sh);
    delete priv->o_bih;
    delete sh->context;
}

// decode a frame
static mp_image_t* decode(sh_video_t *sh,const enc_frame_t* frame){
    priv_t *priv = reinterpret_cast<priv_t*>(sh->context);
    mp_image_t* mpi;
    HRESULT ret;

    if(frame->len<=0) return NULL; // skipped frame

    mpi=mpcodecs_get_image(sh,
	MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_WIDTH,
	sh->src_w, sh->src_h);
    if(mpi->flags&MP_IMGFLAG_DIRECT) mpi->flags|=MP_IMGFLAG_RENDERED;

    // set stride:  (trick discovered by Andreas Ackermann - thanx!)
    sh->bih->biWidth=mpi->width; //mpi->stride[0]/(mpi->bpp/8);
    priv->o_bih->biWidth=mpi->width; //mpi->stride[0]/(mpi->bpp/8);

    sh->bih->biSizeImage = frame->len;

    if(priv->ex)
    ret = ICDecompressEx(priv->hic,
	  ( (sh->ds->flags&1) ? 0 : ICDECOMPRESS_NOTKEYFRAME ) |
	  ( ((frame->flags&3)==2 && !(sh->ds->flags&1))?(ICDECOMPRESS_HURRYUP|ICDECOMPRESS_PREROL):0 ),
	   sh->bih, frame->data, priv->o_bih, (frame->flags&3) ? 0 : mpi->planes[0]);
    else
    ret = ICDecompress(priv->hic,
	  ( (sh->ds->flags&1) ? 0 : ICDECOMPRESS_NOTKEYFRAME ) |
	  ( ((frame->flags&3)==2 && !(sh->ds->flags&1))?(ICDECOMPRESS_HURRYUP|ICDECOMPRESS_PREROL):0 ),
	   sh->bih, frame->data, priv->o_bih, (frame->flags&3) ? 0 : mpi->planes[0]);

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
