#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
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

#include "mpxp_help.h"

#include <dlfcn.h> /* GLIBC specific. Exists under cygwin too! */

#include "vd.h"
#include "codecs_ld.h"
#include "libvo2/video_out.h"
#include "osdep/bswap.h"

#include "libmpconf/codec-cfg.h"
#include "libvo2/img_format.h"
#include "vd.h"
#include "vd_msg.h"

namespace	usr {
    enum {
	DIVX4LINUX_BETA=0,
	DIVX4LINUX=1,
	DIVX5LINUX=2
    };

    enum {
	DEC_OPT_INIT=0, ///< Initialize the decoder.  See LibQDecoreFunction for example usage.
	DEC_OPT_RELEASE=1, ///< Release the decoder.  See LibQDecoreFunction for example usage.
	DEC_OPT_INFO=2, ///<  Obtain information about the video.  See LibQDecoreFunction for example usage.
	DEC_OPT_FRAME=3, ///<  Decode a frame.  See LibQDecoreFunction for example usage.
	DEC_OPT_SET=4, ///< Specify a parameter to adjust/set.
	DEC_OPT_FLUSH=5 ///< Flush the decoder status.
    };

    // Decoder parameter specifier
    enum {
	DEC_PAR_OUTPUT=0, ///< Specify a different output format. pParam2 will point to a DecInit structure
	DEC_PAR_POSTPROCESSING=1, ///< pParam2 will specify a postprocessing level.
	DEC_PAR_POSTPROCDEBLOC=2, ///< pParam2 will specify a deblocking level.
	DEC_PAR_POSTPROCDERING=3, ///< pParam2 will specify a deringing level.
	DEC_PAR_WARMTHLEVEL=4, ///< pParam2 will specify a level for the warmth filter (film effect).
	DEC_PAR_CONTRAST=5, ///< pParam2 will specify the contrast of the output image.
	DEC_PAR_BRIGHTNESS=6, ///< pParam2 will specify the brightness of the output image.
	DEC_PAR_SATURATION=7, ///< pParam2 will specify the saturation of the output image.
	DEC_PAR_LOGO=8, ///< Display the DivX logo on the bottom right of the picture when pParam is the to 1.
	DEC_PAR_SMOOTH=9, ///< Use smooth playback when pParam is set to 1.
	DEC_PAR_SHOWPP=10 ///< Show the postprocessing level in use on the top left corner of the output image.
    };

    // Decoder return values.
    enum {
	DEC_OK=0, ///< Decoder call succeded.
	DEC_INVALID_SYNTAX=-1, ///< A semantic error occourred while parsing the stream.
	DEC_FAIL=1, ///< General failure message. An unexpected problem occourred.
	DEC_INVALID_ARGUMENT=3, ///< One of the arguments passed to the decoder is invalid.
	DEC_NOT_IMPLEMENTED=4, ///< The stream requires tools that have not been implemented.
    };

typedef int (LibQDecoreFunction)(any_t* pHandle, int decOpt, any_t* pParam1, any_t* pParam2);

/// Four Character Code used to decribe the media type of both compressed
/// and uncompressed video.
typedef uint32_t FourCC;
/// Describes a compressed or uncompressed video format.
    struct FormatInfo {
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
    };

    /// Structure containing compressed video bitstream.
    struct DecBitstream {
	any_t* pBuff; ///< Bitstream buffer.  Allocated by caller.  May be modified by decoder.
	int iLength;  ///< Length of bitstream buffer in bytes.
    };

    /// Structure used to obtain information about the decoded video.
    struct DecInfo {
	DecBitstream bitstream; ///< Bitstream buffer.  Allocated by caller.  Bitstream will not be modified by DecInfo.
	FormatInfo formatOut; ///< Populated by decoder.
    };

    /// Structure containing input bitstream and decoder frame buffer.
    /// Default settings are when the structure is memset() to 0.
    struct DecFrame {
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
    };

    struct DecInit {
	FormatInfo formatOut; ///< Desired output video format.
	FormatInfo formatIn; ///< Given input video format
	int isQ; ///< Reserved parameter, value ignored.
    };

    class divx4_decoder : public Video_Decoder {
	public:
	    divx4_decoder(VD_Interface&,sh_video_t&,put_slice_info_t&,uint32_t fourcc);
	    virtual ~divx4_decoder();

	    virtual MPXP_Rc		ctrl(int cmd,any_t* arg,long arg2=0);
	    virtual mp_image_t*		run(const enc_frame_t& frame);
	    virtual video_probe_t	get_probe_information() const;
	private:
	    int				load_lib(const std::string& libname);

	    VD_Interface&		parent;
	    sh_video_t&			sh;
	    const video_probe_t*	probe;

	    any_t*			pHandle;
	    LibQDecoreFunction*		decoder;
	    int				resync;
	    any_t*			dll_handle;
	    LibQDecoreFunction*		(*getDecore_ptr)(unsigned long format);
    };

video_probe_t divx4_decoder::get_probe_information() const { return *probe; }

static const video_probe_t probes[] = {
    { "divx", "libdivx"SLIBSUFFIX,FOURCC_TAG('D','Y','U','V'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420}, {VideoFlag_None, VideoFlag_None } },
    { "divx", "libdivx"SLIBSUFFIX,FOURCC_TAG('D','I','V','3'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420}, {VideoFlag_None, VideoFlag_None } },
    { "divx", "libdivx"SLIBSUFFIX,FOURCC_TAG('D','I','V','4'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420}, {VideoFlag_None, VideoFlag_None } },
    { "divx", "libdivx"SLIBSUFFIX,FOURCC_TAG('D','I','V','5'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420}, {VideoFlag_None, VideoFlag_None } },
    { "divx", "libdivx"SLIBSUFFIX,FOURCC_TAG('D','I','V','6'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420}, {VideoFlag_None, VideoFlag_None } },
    { "divx", "libdivx"SLIBSUFFIX,FOURCC_TAG('D','I','V','X'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420}, {VideoFlag_None, VideoFlag_None } },
    { "divx", "libdivx"SLIBSUFFIX,FOURCC_TAG('D','X','5','0'), VCodecStatus_Working, {IMGFMT_YV12, IMGFMT_I420}, {VideoFlag_None, VideoFlag_None } },
    { NULL, NULL, 0x0, VCodecStatus_NotWorking, {0x0}, { VideoFlag_None }}
};

extern const vd_info_t vd_divx4_info;
int divx4_decoder::load_lib(const std::string& libname)
{
    if(!(dll_handle=ld_codec(libname,vd_divx4_info.url))) return 0;
    getDecore_ptr = (LibQDecoreFunction* (*)(unsigned long))ld_sym(dll_handle,"getDecore");
    return getDecore_ptr != NULL;
}

divx4_decoder::divx4_decoder(VD_Interface& p,sh_video_t& _sh,put_slice_info_t& psi,uint32_t _fourcc)
	    :Video_Decoder(p,_sh,psi,_fourcc)
	    ,parent(p)
	    ,sh(_sh)
{
    unsigned i;
    for(i=0;probes[i].driver;i++)
	if(_fourcc==probes[i].fourcc)
	    probe=&probes[i];
    if(!probe) throw bad_format_exception();

    if(!load_lib(probe->codec_dll)) throw bad_format_exception();

    DecInit dinit;
    int bits=12;
    if(!(parent.config_vf(sh.src_w,sh.src_h))) throw bad_format_exception();
    switch(sh.codec->outfmt[sh.outfmtidx]){
	case IMGFMT_YV12:
	case IMGFMT_I420:
	case IMGFMT_IYUV: break;
	default:
	    mpxp_err<<"Unsupported out_fmt: "; fourcc(mpxp_err,sh.codec->outfmt[sh.outfmtidx]);
	    mpxp_err<<std::endl;
	    throw bad_format_exception();
    }
    if(!(decoder=getDecore_ptr(sh.fourcc))) {
	mpxp_err<<"Can't find decoder for "; fourcc(mpxp_err,sh.fourcc);
	mpxp_err<<" fourcc"<<std::endl;
	throw bad_format_exception();
    }
    dinit.formatOut.fourCC=sh.codec->outfmt[sh.outfmtidx];
    dinit.formatOut.bpp=bits;
    dinit.formatOut.width=sh.src_w;
    dinit.formatOut.height=sh.src_h;
    dinit.formatOut.pixelAspectX=1;
    dinit.formatOut.pixelAspectY=1;
    dinit.formatOut.sizeMax=sh.src_w*sh.src_h*bits;
    dinit.formatIn.fourCC=sh.fourcc;
    dinit.formatIn.framePeriod=sh.fps;
    if(decoder(NULL, DEC_OPT_INIT, (any_t*) &pHandle, &dinit)!=DEC_OK) {
	mpxp_err<<"Can't find decoder for "; fourcc(mpxp_err,dinit.formatOut.fourCC);
	mpxp_err<<" fourcc"<<std::endl;
    }
    mpxp_v<<"INFO: DivX4Linux (libdivx.so) video codec init OK!"<<std::endl;
}

// uninit driver
divx4_decoder::~divx4_decoder() {
    if(pHandle) decoder(pHandle, DEC_OPT_RELEASE, 0, 0);
    if(dll_handle) ::dlclose(dll_handle);
}

// to set/get/query special features/parameters
MPXP_Rc divx4_decoder::ctrl(int cmd,any_t* arg,long arg2){
    switch(cmd){
	case VDCTRL_QUERY_MAX_PP_LEVEL:
	    *((unsigned*)arg)=100;
	    return MPXP_Ok;
	case VDCTRL_SET_PP_LEVEL: {
	    int iOperation = DEC_PAR_POSTPROCESSING;
	    int iLevel = *((int*)arg);
	    if(iLevel<0 || iLevel>100) iLevel=100;
	    return decoder(pHandle,DEC_OPT_SET,&iOperation,&iLevel)==DEC_OK?MPXP_Ok:MPXP_False;
	}
	case VDCTRL_SET_EQUALIZER: {
	    int value;
	    int option;
	    value=arg2;

	    if(!strcmp(reinterpret_cast<char*>(arg),VO_EC_BRIGHTNESS)) option=DEC_PAR_BRIGHTNESS;
	    else if(!strcmp(reinterpret_cast<char*>(arg),VO_EC_CONTRAST)) option=DEC_PAR_CONTRAST;
	    else if(!strcmp(reinterpret_cast<char*>(arg),VO_EC_SATURATION)) option=DEC_PAR_SATURATION;
	    else return MPXP_False;

	    value = (value * 256) / 100;
	    return decoder(pHandle,DEC_OPT_SET,&option,&value)==DEC_OK?MPXP_Ok:MPXP_False;
	}
	case VDCTRL_QUERY_FORMAT:
	    if (*((int*)arg) == IMGFMT_YV12 ||
		*((int*)arg) == IMGFMT_I420 ||
		*((int*)arg) == IMGFMT_IYUV)
			return MPXP_True;
	    else 	return MPXP_False;
	case VDCTRL_RESYNC_STREAM:
	    resync=1;
	    return MPXP_True;
    }
    return MPXP_Unknown;
}

// decode a frame
mp_image_t* divx4_decoder::run(const enc_frame_t& frame) {
    mp_image_t* mpi;
    DecFrame decFrame;

    memset(&decFrame,0,sizeof(DecFrame));
    if(frame.len<=0) return NULL; // skipped frame

    mpi=parent.get_image(MP_IMGTYPE_TEMP, MP_IMGFLAG_PRESERVE | MP_IMGFLAG_ACCEPT_WIDTH,sh.src_w, sh.src_h);
    if(mpi->flags&MP_IMGFLAG_DIRECT) mpi->flags|=MP_IMGFLAG_RENDERED;

    decFrame.bitstream.pBuff = frame.data;
    decFrame.bitstream.iLength = frame.len;
    decFrame.shallowDecode = (frame.flags&3)?1:0;
    decFrame.pBmp=mpi->planes[0];
    decFrame.bmpStride=(mpi->flags&(MP_IMGFLAG_YUV|MP_IMGFLAG_DIRECT))==(MP_IMGFLAG_YUV|MP_IMGFLAG_DIRECT)?
		     mpi->flags&MP_IMGFLAG_PLANAR?mpi->stride[0]:mpi->stride[0]/2:
		     mpi->width;
    if(resync) { decFrame.bBitstreamUpdated=1; resync=0; }

    if(decoder(pHandle, DEC_OPT_FRAME, &decFrame, 0)!=DEC_OK) mpxp_warn<<"divx: Error happened during decoding"<<std::endl;

    return mpi;
}

static const mpxp_option_t options[] = {
  { NULL, NULL, 0, 0, 0, 0, NULL}
};

static Video_Decoder* query_interface(VD_Interface& p,sh_video_t& sh,put_slice_info_t& psi,uint32_t fourcc) { return new(zeromem) divx4_decoder(p,sh,psi,fourcc); }

extern const vd_info_t vd_divx4_info = {
    "DivX4Linux lib (divx4/5 mode)",
    "divx",
    "Nickols_K",
    "http://labs.divx.com/DivXLinuxCodec",
    query_interface,
    options
};
} // namespace	usr