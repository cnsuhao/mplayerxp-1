/**
@file
@verbatim

$Id: divx4linux.h,v 1.4 2007/11/24 12:52:44 nickols_k Exp $

Copyright (c) 2002-2004 DivX, Inc. All rights reserved.

This software is the confidential and proprietary information of DivX, Inc.,
Inc. and may be used only in accordance with the terms of your license from
DivX, Inc.

@endverbatim

Decoder API header file. Contains the entrance function of the decoder core. 
*/

#ifndef _LIBQDEC_H_
#define _LIBQDEC_H_

#ifdef __cplusplus
extern "C" {
#endif 

/// Four Character Code used to decribe the media type of both compressed
/// and uncompressed video.
typedef uint32_t FourCC;


/// Creates a FourCC from a four-character string like "divx". 
/// @param str Pointer to a string such as "divx" 
/// @return Created FourCC.
FourCC FourCC_create(const char* str);


/// A mechanism to create a FourCC from four characters.
#define FOURCC(A, B, C, D) ( \
    ((FourCC) A) | \
    (((FourCC) B)<<8) | \
    (((FourCC) C)<<16) | \
    (((FourCC) D)<<24) )

#define FOURCC_div3 FOURCC('d','i','v','3')
#define FOURCC_div4 FOURCC('d','i','v','4')
#define FOURCC_div5 FOURCC('d','i','v','5')
#define FOURCC_DIVX FOURCC('D','I','V','X')
#define FOURCC_divx FOURCC('d','i','v','x')
#define FOURCC_dx50 FOURCC('d','x','5','0')
#define FOURCC_DX50 FOURCC('D','X','5','0')
#define FOURCC_IYUV FOURCC('I','Y','U','V')
#define FOURCC_iyuv FOURCC('i','y','u','v')
#define FOURCC_YV12 FOURCC('Y','V','1','2')
#define FOURCC_yv12 FOURCC('y','v','1','2')

/// Renders a FourCC to an ASCII string. 
/// @param fourCC FourCC code to be rendered. 
/// @param str Pointer to buffer where string will be rendered. 
/// @return Returns str for convenience. 
char* FourCC_toStr(char* str, FourCC fourCC);


/// Converts all letters 'A' to 'Z' present in fourCC to their lower case equivalent.
/// @param fourCC FourCC code to be converted. 
/// @return Converted FourCC.
FourCC FourCC_lowerCase(FourCC fourCC);

/// Describes a compressed or uncompressed video format.
typedef struct FormatInfo
{
    /// Four CC of the video format.
    FourCC fourCC;

    /// Bits per pixel (zero if not known).  This field is used to distinguish the 
    /// various uncompressed RGB formats.
    int bpp;

    /// Width of the image in pixels.
    int width;
    
    /// Height of the image in pixels.
    int height; 

    /// Set non-zero if the bottom line of the image appears first in the buffer.
    int inverted;

    /// Pixel aspect ratio:  horizontal part.
    int pixelAspectX;

    /// Pixel aspect ratio:  vertical part.
    int pixelAspectY;

    /// Maximum size in bytes of a video frame of this format.
    int sizeMax;

    /// Number of units of time in a second.
    int timescale;

    /// Duration of each frame, in units of time defined by timescale.  In the case 
    /// of variable framerate, this should be set to the maximum expected frame period.
    int framePeriod;

    /// 1 if frame rate is constant; 0 otherwise.
    int framePeriodIsConstant;
} 
FormatInfo;

/// Convenience function.
/// @param pFormatInfo Pointer to a populated FormatInfo structure.  
/// @return The total number of luminance pictures in each video frame.
static int FormatInfo_getTotalPixels(const FormatInfo* pFormatInfo) 
{ 
    return pFormatInfo->height * pFormatInfo->width; 
}

/// Convenience function.  Only works when bpp is known and set correctly.
/// @param pFormatInfo Pointer to a populated FormatInfo structure.
/// @return The size in bytes of a single video frame.
static int FormatInfo_getFrameSize(const FormatInfo* pFormatInfo) 
{ 
    return FormatInfo_getTotalPixels(pFormatInfo) * pFormatInfo->bpp / 8;; 
}

/// Convenience function.
/// @param pFormatInfo Pointer to a populated FormatInfo structure.
/// @return The framerate of the format according to framePeriod and timescale members.
static double FormatInfo_getFramerate(const FormatInfo* pFormatInfo) 
{ 
    return (double)pFormatInfo->timescale / (double)pFormatInfo->framePeriod; 
}

static int FormatInfo_diff(const FormatInfo* pFormat1, const FormatInfo* pFormat2)
{
    return
        (pFormat1->fourCC != pFormat2->fourCC) ||
        (pFormat1->fourCC == 0 && pFormat1->bpp != pFormat2->bpp) ||
        (pFormat1->width != pFormat2->width) ||
        (pFormat1->height != pFormat2->height) ||
        (pFormat1->inverted != pFormat2->inverted) ||
        (pFormat1->pixelAspectX != pFormat2->pixelAspectX) ||
        (pFormat1->pixelAspectY != pFormat2->pixelAspectY) ||
        (pFormat1->timescale != pFormat2->timescale) ||
        (pFormat1->framePeriodIsConstant != pFormat2->framePeriodIsConstant) ||
        (pFormat1->framePeriodIsConstant && (pFormat1->framePeriod != pFormat2->framePeriod));
}

// Decoder method specifiers (parameter \ref DecOpt) 

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

/// Structure with parameters used to instantiate a decoder.
/// Set by caller.
typedef struct DecInit
{
    FormatInfo formatOut; ///< Desired output video format.
    FormatInfo formatIn; ///< Given input video format
    int isQ; ///< Reserved parameter, value ignored.
}
DecInit;

/// Structure containing compressed video bitstream.
typedef struct DecBitstream
{
    void* pBuff; ///< Bitstream buffer.  Allocated by caller.  May be modified by decoder.   
    int iLength;  ///< Length of bitstream buffer in bytes.  
}
DecBitstream;

/// Structure used to obtain information about the decoded video. 
typedef struct DecInfo
{
    DecBitstream bitstream; ///< Bitstream buffer.  Allocated by caller.  Bitstream will not be modified by DecInfo.
    FormatInfo formatOut; ///< Populated by decoder.
}
DecInfo;

/// Structure containing input bitstream and decoder frame buffer. 
/// Default settings are when the structure is memset() to 0.
typedef struct DecFrame
{
    DecBitstream bitstream; ///< Input bitstream to be decoded. 
    void* pBmp; ///< Decoded bitmap buffer.  Allocated by caller. If the buffer pointer is 0 the bitmap will not be rendered (fast decode).
    int bmpStride; ///< Bitmap stride in pixels.  Set by caller.  Currently ignored by decoder.
    int bConstBitstream; ///< Set zero if it is OK for decoder to trash the input bitstream when it is decoded.  This gives a small performance boost.
    int bBitstreamUpdated;    ///< Notify API that the bitstream is updated [Used by the reference decoder to dump the bitstream to a disk file so that it can be read in].
    int bBitstreamIsEOS; ///< Set non-zero to tell the decoder that bitstream is the last part of the stream.
    int frameWasDecoded; ///< Non-zero value means that a frame was successfully decoded.  Set by decoder. 
    int timestampDisplay; ///< Display timestamp of the decoded frame.  Set by decoder. 
    int shallowDecode; ///< Set non-zero to allow the decoder not to decode any video data (just parse headers). 
    int bSingleFrame; ///< Set non-zero to indicate that the decoder is receiving a single frame in this buffer (no packet B-frames)
} 
DecFrame;

/// Main decode engine function type.  Decoder operation comprises instantiation, 
/// initialization, decoding each frame and release.  All these operations may be achieved
/// by calls to this function - see the example below.  
/// 
/// Decoder instantiation is achieved like this:
///
/// \code
/// LibQDecoreFunction* pDecore = LibQDecoreMPEG4;
/// DecInit decInit;
/// // ...set up decInit members here...
/// 
/// void *pHandle;
/// int iRetVal=pDecore(NULL, DEC_OPT_INIT, (void*) &pHandle, &decInit);
/// 
/// assert(pHandle != 0);
/// assert(iRetVal == DEC_OK);
/// \endcode
/// 
/// Next the user may call the decoder with the first chunk of
/// bitstream to have the decoder detirmine width, height and other 
/// pertinent information.  This step is necessary only if the user needs 
/// to garner information from the bitstream such as video frame width 
/// and height. 
/// 
/// \code
/// DecInfo decInfo;
/// // ...set up decInfo.bitstream...
/// 
/// int rv = pDecore(pHandle, DEC_OPT_INFO, &decInfo, 0);
/// assert(rv == DEC_OK);
/// 
/// // Members of decInfo.formatInfo are now populated with
/// // information about the video:
/// // width, height, etc.
/// \endcode
/// 
/// Video frames are decoded by further calls to the decoder function.
/// The decoder outputs video frames in display order - it performs
/// any necessary reordering.
/// \code
/// 
/// while (!bEndOfStream)
/// {
///     DecFrame decFrame;
///     memset(&decFrame, 0, sizeof(DecFrame));
///
///     // ...load up decFrame.bitstream with the next bitstream chunk
///     // and set any necessary DecFrame parameters here...
///
///     // Depending on the bitstream, the decoder may produce
///     // more than one decoded frame per input bitstream chunk.
///     // Multiple calls to pDecore achieve this (decFrame.bitstream is
///     // updated as the decoder consumes the chunk).
///     while (1)
///     {
///         int rv = pDecore(pHandle, DEC_OPT_FRAME, &decFrame, 0);
///         assert(rv == DEC_OK);
/// 
///         if (!decFrame.frameWasDecoded)
///         {
///             break;
///         }
/// 
///         // ...read out decompressed frame from decFrame.pBmp...
///     }
/// }
/// \endcode
/// 
/// When decoding is finished the decoder should be released:
/// 
/// \code
/// 
/// int rv = pDecore(pHandle, DEC_OPT_RELEASE, 0, 0);
/// assert(rv == DEC_OK);
/// \endcode
///
/// Some decoder properties (contrast, saturation, brightness, warmth level, ...) 
/// can be changed at runtime. In order to do this the user may call the 
/// decoder specifying the parameter he wants to change and the new value:
///
/// \code
/// 
/// int iOperation = DEC_PAR_CONTRAST;
/// int iLevel = 0; // set the contrast level to zero
/// int rv = pDecore(pHandle, DEC_OPT_SET, &iLevel, &iOperation);
///
/// \endcode
///
/// \param pHandle - Handle of the Decore instance.
/// \param decOpt - Method specifier. Controls function performed by this call.
///     Use one of the following method specifiers:
///     - #DEC_OPT_INIT
///     - #DEC_OPT_RELEASE
///     - #DEC_OPT_FRAME
///     - #DEC_OPT_SET
/// \param pParam1 - First parameter (meaning depends on \p DecOpt).
///     When this parameter is set the DEC_OPT_SET, the following
///     operation specifiers are valid:
///     - #DEC_PAR_OUTPUT 
///     - #DEC_PAR_POSTPROCESSING 
///     - #DEC_PAR_POSTPROCDEBLOC 
///     - #DEC_PAR_POSTPROCDERING 
///     - #DEC_PAR_WARMTHLEVEL 
///     - #DEC_PAR_CONTRAST 
///     - #DEC_PAR_BRIGHTNESS 
///     - #DEC_PAR_SATURATION 
///     - #DEC_PAR_LOGO 
///     - #DEC_PAR_SMOOTH 
///     - #DEC_PAR_SHOWPP 
///     Note that these parameters might not have any effect on
///     some decoders or on some situation
/// \param pParam2 - Second parameter (meaning depends on \p DecOpt).
/// \return Returns one of the following:
///     - #DEC_OK
///     - #DEC_INVALID_SYNTAX
///     - #DEC_FAIL
///     - #DEC_NOT_IMPLEMENTED
///     - #DEC_INVALID_ARGUMENT
///
///
typedef int (LibQDecoreFunction)(void* pHandle, int decOpt, void* pParam1, void* pParam2);


/// Retrieves a pointer to the appropriate decore function for the specified video format.
/// @param fourCC FourCC of the video format that is to be decoded.
/// @return Decore function pointer on success or null on failure.
LibQDecoreFunction* getDecore(unsigned long fccFormat);


#ifdef __cplusplus
}
#endif

#endif // _LIBQDEC_H_
