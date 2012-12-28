/****************************************************************************
 *
 *  AVIFMT - AVI file format definitions
 *
 ****************************************************************************/
#ifndef AVIFMT
#define AVIFMT

#ifndef NOAVIFMT

#ifndef  __WINE_WINDEF_H
#include "win32loader/wine/windef.h"
#endif

#ifndef __WINE_MMSYSTEM_H
#ifndef __WINE_MSACM_H
typedef DWORD FOURCC;
#endif
#endif


#ifdef _MSC_VER
#pragma warning(disable:4200)
#endif

/* The following is a short description of the AVI file format.  Please
 * see the accompanying documentation for a full explanation.
 *
 * An AVI file is the following RIFF form:
 *
 *	RIFF('AVI'
 *	      LIST('hdrl'
 *		    avih(<MainAVIHeader>)
 *                  LIST ('strl'
 *                      strh(<Stream header>)
 *                      strf(<Stream format>)
 *                      ... additional header data
 *            LIST('movi'
 *      	  { LIST('rec'
 *      		      SubChunk...
 *      		   )
 *      	      | SubChunk } ....
 *            )
 *            [ <AVIIndex> ]
 *      )
 *
 *	The main file header specifies how many streams are present.  For
 *	each one, there must be a stream header chunk and a stream format
 *	chunk, enlosed in a 'strl' LIST chunk.  The 'strf' chunk contains
 *	type-specific format information; for a video stream, this should
 *	be a BITMAPINFO structure, including palette.  For an audio stream,
 *	this should be a WAVEFORMAT (or PCMWAVEFORMAT) structure.
 *
 *	The actual data is contained in subchunks within the 'movi' LIST
 *	chunk.  The first two characters of each data chunk are the
 *	stream number with which that data is associated.
 *
 *	Some defined chunk types:
 *           Video Streams:
 *                  ##db:	RGB DIB bits
 *                  ##dc:	RLE8 compressed DIB bits
 *                  ##pc:	Palette Change
 *
 *           Audio Streams:
 *                  ##wb:	waveform audio bytes
 *
 * The grouping into LIST 'rec' chunks implies only that the contents of
 *   the chunk should be read into memory at the same time.  This
 *   grouping is used for files specifically intended to be played from
 *   CD-ROM.
 *
 * The index chunk at the end of the file should contain one entry for
 *   each data chunk in the file.
 *
 * Limitations for the current software:
 *	Only one video stream and one audio stream are allowed.
 *	The streams must start at the beginning of the file.
 *
 *
 * To register codec types please obtain a copy of the Multimedia
 * Developer Registration Kit from:
 *
 *  Microsoft Corporation
 *  Multimedia Systems Group
 *  Product Marketing
 *  One Microsoft Way
 *  Redmond, WA 98052-6399
 *
 */

#ifndef mmioFOURCC
#define mmioFOURCC( ch0, ch1, ch2, ch3 )				\
		( (DWORD)(BYTE)(ch0) | ( (DWORD)(BYTE)(ch1) << 8 ) |	\
		( (DWORD)(BYTE)(ch2) << 16 ) | ( (DWORD)(BYTE)(ch3) << 24 ) )
#endif

/* Macro to make a TWOCC out of two characters */
#ifndef aviTWOCC
#define aviTWOCC(ch0, ch1) ((WORD)(BYTE)(ch0) | ((WORD)(BYTE)(ch1) << 8))
#endif

typedef WORD TWOCC;

/* form types, list types, and chunk types */
#define formtypeAVI             mmioFOURCC('A', 'V', 'I', ' ')
#define listtypeAVIHEADER       mmioFOURCC('h', 'd', 'r', 'l')
#define ckidAVIMAINHDR          mmioFOURCC('a', 'v', 'i', 'h')
#define listtypeSTREAMHEADER    mmioFOURCC('s', 't', 'r', 'l')
#define ckidSTREAMHEADER        mmioFOURCC('s', 't', 'r', 'h')
#define ckidSTREAMFORMAT        mmioFOURCC('s', 't', 'r', 'f')
#define ckidSTREAMHANDLERDATA   mmioFOURCC('s', 't', 'r', 'd')
#define ckidSTREAMNAME		mmioFOURCC('s', 't', 'r', 'n')

#define listtypeAVIMOVIE        mmioFOURCC('m', 'o', 'v', 'i')
#define listtypeAVIRECORD       mmioFOURCC('r', 'e', 'c', ' ')

#define ckidAVINEWINDEX         mmioFOURCC('i', 'd', 'x', '1')

/*
** Stream types for the <fccType> field of the stream header.
*/
#define streamtypeVIDEO         mmioFOURCC('v', 'i', 'd', 's')
#define streamtypeAUDIO         mmioFOURCC('a', 'u', 'd', 's')
#define streamtypeMIDI		mmioFOURCC('m', 'i', 'd', 's')
#define streamtypeTEXT          mmioFOURCC('t', 'x', 't', 's')

/* Basic chunk types */
#define cktypeDIBbits           aviTWOCC('d', 'b')
#define cktypeDIBcompressed     aviTWOCC('d', 'c')
#define cktypePALchange         aviTWOCC('p', 'c')
#define cktypeWAVEbytes         aviTWOCC('w', 'b')

/* Chunk id to use for extra chunks for padding. */
#define ckidAVIPADDING          mmioFOURCC('J', 'U', 'N', 'K')

/*
** Useful macros
**
** Warning: These are nasty macro, and MS C 6.0 compiles some of them
** incorrectly if optimizations are on.  Ack.
*/

/* Macro to get stream number out of a FOURCC ckid */
#define FromHex(n)	(((n) >= 'A') ? ((n) + 10 - 'A') : ((n) - '0'))
#define StreamFromFOURCC(fcc) ((WORD) ((FromHex(LOBYTE(LOWORD(fcc))) << 4) + \
					     (FromHex(HIBYTE(LOWORD(fcc))))))

/* Macro to get TWOCC chunk type out of a FOURCC ckid */
#define TWOCCFromFOURCC(fcc)    HIWORD(fcc)

/* Macro to make a ckid for a chunk out of a TWOCC and a stream number
** from 0-255.
*/
#define ToHex(n)	((BYTE) (((n) > 9) ? ((n) - 10 + 'A') : ((n) + '0')))
#define MAKEAVICKID(tcc, stream) \
	MAKELONG((ToHex((stream) & 0x0f) << 8) | \
			    (ToHex(((stream) & 0xf0) >> 4)), tcc)

/*
** Main AVI File Header
*/

/* flags for use in <dwFlags> in AVIFileHdr */
#define AVIF_HASINDEX		0x00000010	// Index at end of file?
#define AVIF_MUSTUSEINDEX	0x00000020
#define AVIF_ISINTERLEAVED	0x00000100
#define AVIF_TRUSTCKTYPE	0x00000800	// Use CKType to find key frames?
#define AVIF_WASCAPTUREFILE	0x00010000
#define AVIF_COPYRIGHTED	0x00020000

/* The AVI File Header LIST chunk should be padded to this size */
#define AVI_HEADERSIZE  2048                    // size of AVI header list

typedef struct __attribute__((packed))
{
    DWORD		dwMicroSecPerFrame;	// frame display rate (or 0L)
    DWORD		dwMaxBytesPerSec;	// max. transfer rate
    DWORD		dwPaddingGranularity;	// pad to multiples of this
						// size; normally 2K.
    DWORD		dwFlags;		// the ever-present flags
    DWORD		dwTotalFrames;		// # frames in file
    DWORD		dwInitialFrames;
    DWORD		dwStreams;
    DWORD		dwSuggestedBufferSize;

    DWORD		dwWidth;
    DWORD		dwHeight;

    DWORD		dwReserved[4];
} MainAVIHeader;

/*
** Stream header
*/

#define AVISF_DISABLED			0x00000001

#define AVISF_VIDEO_PALCHANGES		0x00010000


typedef struct __attribute__((packed))
{
    FOURCC		fccType;
    FOURCC		fccHandler;
    DWORD		dwFlags;	/* Contains AVITF_* flags */
    WORD		wPriority;
    WORD		wLanguage;
    DWORD		dwInitialFrames;
    DWORD		dwScale;
    DWORD		dwRate;	/* dwRate / dwScale == samples/second */
    DWORD		dwStart;
    DWORD		dwLength; /* In units above... */
    DWORD		dwSuggestedBufferSize;
    DWORD		dwQuality;
    DWORD		dwSampleSize;
    RECT		rcFrame;
} AVIStreamHeader;

/* Flags for index */
#define AVIIF_LIST          0x00000001L // chunk is a 'LIST'
#define AVIIF_KEYFRAME      0x00000010L // this frame is a key frame.

#define AVIIF_NOTIME	    0x00000100L // this frame doesn't take any time
#define AVIIF_COMPUSE       0x0FFF0000L // these bits are for compressor use

#define FOURCC_RIFF     mmioFOURCC('R', 'I', 'F', 'F')
#define FOURCC_LIST     mmioFOURCC('L', 'I', 'S', 'T')

typedef struct __attribute__((packed))
{
    DWORD		ckid;
    DWORD		dwFlags;
    DWORD		dwChunkOffset;		// Position of chunk
    DWORD		dwChunkLength;		// Length of chunk
} AVIINDEXENTRY;

#define AVISTREAMREAD_CONVENIENT	(-1L)

/* Below are located AVIX extensions */

typedef struct __attribute__((packed)) _avisuperindex_entry
{
    QWORD	qwOffset;           // full file offset
    DWORD	dwSize;             // size of index chunk at this offset
    DWORD	dwDuration;         // time span in stream ticks
} avisuperindex_entry;

typedef struct __attribute__((packed)) _avistdindex_entry
{
    DWORD	dwOffset;           // qwBaseOffset + this is full file offset
    DWORD	dwSize;             // bit 31 is set if this is NOT a keyframe
} avistdindex_entry;

// Standard index
typedef struct __attribute__((packed)) _avistdindex_chunk {
    BYTE	fcc[4];       // ix##
    DWORD	dwSize;            // size of this chunk
    WORD	wLongsPerEntry;     // must be sizeof(aIndex[0])/sizeof(DWORD)
    BYTE	bIndexSubType;      // must be 0
    BYTE	bIndexType;         // must be AVI_INDEX_OF_CHUNKS
    DWORD	nEntriesInUse;     // first unused entry
    BYTE	dwChunkId[4]; // '##dc' or '##db' or '##wb' etc..
    QWORD	qwBaseOffset;       // all dwOffsets in aIndex array are relative to this
    DWORD	dwReserved3;       // must be 0
    avistdindex_entry *aIndex;   // the actual frames
} avistdindex_chunk;

// Base Index Form 'indx'
typedef struct __attribute__((packed)) _avisuperindex_chunk
{
    BYTE	fcc[4];
    DWORD	dwSize;                // size of this chunk
    WORD	wLongsPerEntry;         // size of each entry in aIndex array (must be 4*4 for us)
    BYTE	bIndexSubType;          // future use. must be 0
    BYTE	bIndexType;             // one of AVI_INDEX_* codes
    DWORD	nEntriesInUse;         // index of first unused member in aIndex array
    BYTE	dwChunkId[4];         // fcc of what is indexed
    DWORD	dwReserved[3];         // meaning differs for each index type/subtype.
				     // 0 if unused
    avisuperindex_entry *aIndex;     // position of ix## chunks
    avistdindex_chunk *stdidx;       // the actual std indices
} avisuperindex_chunk;

typedef struct __attribute__((packed))
{
	DWORD	CompressedBMHeight;
	DWORD	CompressedBMWidth;
	DWORD	ValidBMHeight;
	DWORD	ValidBMWidth;
	DWORD	ValidBMXOffset;
	DWORD	ValidBMYOffset;
	DWORD	VideoXOffsetInT;
	DWORD	VideoYValidStartLine;
} VIDEO_FIELD_DESC;

typedef struct __attribute__((packed))
{
	DWORD	VideoFormatToken;
	DWORD	VideoStandard;
	DWORD	dwVerticalRefreshRate;
	DWORD	dwHTotalInT;
	DWORD	dwVTotalInLines;
	DWORD	dwFrameAspectRatio;
	DWORD	dwFrameWidthInPixels;
	DWORD	dwFrameHeightInLines;
	DWORD	nbFieldPerFrame;
	VIDEO_FIELD_DESC FieldInfo[2];
} VideoPropHeader;

typedef enum {
	FORMAT_UNKNOWN,
	FORMAT_PAL_SQUARE,
	FORMAT_PAL_CCIR_601,
	FORMAT_NTSC_SQUARE,
	FORMAT_NTSC_CCIR_601,
} VIDEO_FORMAT;

typedef enum {
	STANDARD_UNKNOWN,
	STANDARD_PAL,
	STANDARD_NTSC,
	STANDARD_SECAM
} VIDEO_STANDARD;

#define MAKE_AVI_ASPECT(a, b) (((a)<<16)|(b))
#define GET_AVI_ASPECT(a) ((float)((a)>>16)/(float)((a)&0xffff))
/*
** Palette change chunk
**
** Used in video streams.
*/
#endif /* NOAVIFMT */
#endif
