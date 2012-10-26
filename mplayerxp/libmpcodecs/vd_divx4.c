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
	"http://labs.divx.com/DivXLinuxCodec"
};

static const config_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL, NULL}
};

LIBVD_EXTERN(divx4)

#define DIVX4LINUX_BETA 0
#define DIVX4LINUX	1
#define DIVX5LINUX	2

#define DEC_OPT_INIT 0 ///< Initialize the decoder.  See LibQDecoreFunction for example usage.
#define DEC_OPT_RELEASE 1 ///< Release the decoder.  See LibQDecoreFunction for example usage.
#define DEC_OPT_INFO 2 ///<  Obtain information about the video.  See LibQDecoreFunction for example usage.
#define DEC_OPT_FRAME 3 ///<  Decode a frame.  See LibQDecoreFunction for example usage.
#define DEC_OPT_SET 4 ///< Specify a parameter to adjust/set. 
#define DEC_OPT_FLUSH 5 ///< Flush the decoder status.

// Decoder parameter specifier

#define DEC_PAR_OUTPUT 0 ///< Specify a different output format. pParam2 will point to a DecInit structure
#define DEC_PAR_POSTPROCESSING 1 ///< pParam2 will specify a postprocessing level.
#define DEC_PAR_POSTPROCDEBLOC 2 ///< pParam2 will specify a deblocking level.
#define DEC_PAR_POSTPROCDERING 3 ///< pParam2 will specify a deringing level.
#define DEC_PAR_WARMTHLEVEL 4 ///< pParam2 will specify a level for the warmth filter (film effect).
#define DEC_PAR_CONTRAST 5 ///< pParam2 will specify the contrast of the output image.
#define DEC_PAR_BRIGHTNESS 6 ///< pParam2 will specify the brightness of the output image.
#define DEC_PAR_SATURATION 7 ///< pParam2 will specify the saturation of the output image.
#define DEC_PAR_LOGO 8 ///< Display the DivX logo on the bottom right of the picture when pParam is the to 1.
#define DEC_PAR_SMOOTH 9 ///< Use smooth playback when pParam is set to 1.
#define DEC_PAR_SHOWPP 10 ///< Show the postprocessing level in use on the top left corner of the output image.

// Decoder return values.

#define DEC_OK 0 ///< Decoder call succeded.
#define DEC_INVALID_SYNTAX -1 ///< A semantic error occourred while parsing the stream. 
#define DEC_FAIL 1 ///< General failure message. An unexpected problem occourred. 
#define DEC_INVALID_ARGUMENT 3 ///< One of the arguments passed to the decoder is invalid. 
#define DEC_NOT_IMPLEMENTED 4 ///< The stream requires tools that have not been implemented. 

typedef int (LibQDecoreFunction)(any_t* pHandle, int decOpt, any_t* pParam1, any_t* pParam2);

/// Four Character Code used to decribe the media type of both compressed
/// and uncompressed video.
typedef uint32_t FourCC;
/// Describes a compressed or uncompressed video format.
typedef struct FormatInfo
{
    FourCC fourCC; /// Four CC of the video format.
    int bpp; /// Bits per pixel for RGB (zero if not known).
    int width; /// Width of the image in pixels.
    int height; /// Height of the image in pixels.
    int inverted; /// Set non-zero if the bottom line of the image appears first in the buffer.
    int pixelAspectX; /// Pixel aspect ratio:  horizontal part.
    int pixelAspectY; /// Pixel aspect ratio:  vertical part.
    int sizeMax; /// Maximum size in bytes of a video frame of this format.
    int timescale; /// Number of units of time in a second.
    int framePeriod; /// Duration of each frame, in units of time defined by timescale.  In the case of variable framerate, this should be set to the maximum expected frame period.
    int framePeriodIsConstant; /// 1 if frame rate is constant; 0 otherwise.
}FormatInfo;

/// Structure containing compressed video bitstream.
typedef struct DecBitstream
{
    any_t* pBuff; ///< Bitstream buffer.  Allocated by caller.  May be modified by decoder.
    int iLength;  ///< Length of bitstream buffer in bytes.
}DecBitstream;

/// Structure used to obtain information about the decoded video.
typedef struct DecInfo
{
    DecBitstream bitstream; ///< Bitstream buffer.  Allocated by caller.  Bitstream will not be modified by DecInfo.
    FormatInfo formatOut; ///< Populated by decoder.
}DecInfo;

/// Structure containing input bitstream and decoder frame buffer.
/// Default settings are when the structure is memset() to 0.
typedef struct DecFrame
{
    DecBitstream bitstream; ///< Input bitstream to be decoded.
    any_t* pBmp; ///< Decoded bitmap buffer.  Allocated by caller. If the buffer pointer is 0 the bitmap will not be rendered (fast decode).
    int bmpStride; ///< Bitmap stride in pixels.  Set by caller.  Currently ignored by decoder.
    int bConstBitstream; ///< Set zero if it is OK for decoder to trash the input bitstream when it is decoded.  This gives a small performance boost.
    int bBitstreamUpdated;    ///< Notify API that the bitstream is updated [Used by the reference decoder to dump the bitstream to a disk file so that it can be read in].
    int bBitstreamIsEOS; ///< Set non-zero to tell the decoder that bitstream is the last part of the stream.
    int frameWasDecoded; ///< Non-zero value means that a frame was successfully decoded.  Set by decoder.
    int timestampDisplay; ///< Display timestamp of the decoded frame.  Set by decoder.
    int shallowDecode; ///< Set non-zero to allow the decoder not to decode any video data (just parse headers).
    int bSingleFrame; ///< Set non-zero to indicate that the decoder is receiving a single frame in this buffer (no packet B-frames)
}DecFrame;

typedef struct DecInit
{
    FormatInfo formatOut; ///< Desired output video format.
    FormatInfo formatIn; ///< Given input video format
    int isQ; ///< Reserved parameter, value ignored.
}DecInit;


typedef struct
{
    any_t*pHandle;
    LibQDecoreFunction* decoder;
    int resync;
}priv_t;

static LibQDecoreFunction* (*getDecore_ptr)(unsigned long format);
static any_t*dll_handle;

// to set/get/query special features/parameters
static int control(sh_video_t *sh,int cmd,any_t* arg,...){
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
  if(!(dll_handle=ld_codec(libname,mpcodecs_vd_divx4.info->url))) return 0;
  getDecore_ptr = ld_sym(dll_handle,"getDecore");
  return getDecore_ptr != NULL;
}

// init driver
static int init(sh_video_t *sh){
    DecInit dinit;
    priv_t*p;
    int bits=12;
    if(!load_lib("libdivx"SLIBSUFFIX)) return 0;
    if(!(mpcodecs_config_vo(sh,sh->src_w,sh->src_h,NULL))) return 0;
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
    if(!(p->decoder=getDecore_ptr(sh->fourcc))) {
	char *p=(char *)&(sh->fourcc);
	MSG_ERR("Can't find decoder for %c%c%c%c fourcc\n",p[0],p[1],p[2],p[3]);
	return 0;
    }
    dinit.formatOut.fourCC=sh->codec->outfmt[sh->outfmtidx];
    dinit.formatOut.bpp=bits;
    dinit.formatOut.width=sh->src_w;
    dinit.formatOut.height=sh->src_h;
    dinit.formatOut.pixelAspectX=1;
    dinit.formatOut.pixelAspectY=1;
    dinit.formatOut.sizeMax=sh->src_w*sh->src_h*bits;
    dinit.formatIn.fourCC=sh->fourcc;
    dinit.formatIn.framePeriod=sh->fps;
    if(p->decoder(NULL, DEC_OPT_INIT, (any_t*) &p->pHandle, &dinit)!=DEC_OK) {
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
static mp_image_t* decode(sh_video_t *sh,any_t* data,int len,int flags){
    priv_t*p=sh->context;
    mp_image_t* mpi;
    DecFrame decFrame;

    memset(&decFrame,0,sizeof(DecFrame));
    if(len<=0) return NULL; // skipped frame

    mpi=mpcodecs_get_image(sh, MP_IMGTYPE_TEMP, MP_IMGFLAG_PRESERVE | MP_IMGFLAG_ACCEPT_WIDTH,
	sh->src_w, sh->src_h);
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

