#ifndef _aviheader_h
#define	_aviheader_h

//#include "mp_config.h"	/* get correct definition WORDS_BIGENDIAN */
#include "bswap.h"

/*
 * Some macros to swap little endian structures read from an AVI file
 * into machine endian wtag
 */
#ifdef WORDS_BIGENDIAN
#define	le2me_MainAVIHeader(h) {					\
    (h)->dwMicroSecPerFrame = le2me_32((h)->dwMicroSecPerFrame);	\
    (h)->dwMaxBytesPerSec = le2me_32((h)->dwMaxBytesPerSec);		\
    (h)->dwPaddingGranularity = le2me_32((h)->dwPaddingGranularity);	\
    (h)->dwFlags = le2me_32((h)->dwFlags);				\
    (h)->dwTotalFrames = le2me_32((h)->dwTotalFrames);			\
    (h)->dwInitialFrames = le2me_32((h)->dwInitialFrames);		\
    (h)->dwStreams = le2me_32((h)->dwStreams);				\
    (h)->dwSuggestedBufferSize = le2me_32((h)->dwSuggestedBufferSize);	\
    (h)->dwWidth = le2me_32((h)->dwWidth);				\
    (h)->dwHeight = le2me_32((h)->dwHeight);				\
}

#define	le2me_AVIStreamHeader(h) {					\
    (h)->fccType = le2me_32((h)->fccType);				\
    (h)->fccHandler = le2me_32((h)->fccHandler);			\
    (h)->dwFlags = le2me_32((h)->dwFlags);				\
    (h)->wPriority = le2me_16((h)->wPriority);				\
    (h)->wLanguage = le2me_16((h)->wLanguage);				\
    (h)->dwInitialFrames = le2me_32((h)->dwInitialFrames);		\
    (h)->dwScale = le2me_32((h)->dwScale);				\
    (h)->dwRate = le2me_32((h)->dwRate);				\
    (h)->dwStart = le2me_32((h)->dwStart);				\
    (h)->dwLength = le2me_32((h)->dwLength);				\
    (h)->dwSuggestedBufferSize = le2me_32((h)->dwSuggestedBufferSize);	\
    (h)->dwQuality = le2me_32((h)->dwQuality);				\
    (h)->dwSampleSize = le2me_32((h)->dwSampleSize);			\
    le2me_RECT(&(h)->rcFrame);						\
}
#define	le2me_RECT(h) {							\
    (h)->left = le2me_16((h)->left);					\
    (h)->top = le2me_16((h)->top);					\
    (h)->right = le2me_16((h)->right);					\
    (h)->bottom = le2me_16((h)->bottom);				\
}
#define le2me_BITMAPINFOHEADER(h) {					\
    (h)->biSize = le2me_32((h)->biSize);				\
    (h)->biWidth = le2me_32((h)->biWidth);				\
    (h)->biHeight = le2me_32((h)->biHeight);				\
    (h)->biPlanes = le2me_16((h)->biPlanes);				\
    (h)->biBitCount = le2me_16((h)->biBitCount);			\
    (h)->biCompression = le2me_32((h)->biCompression);			\
    (h)->biSizeImage = le2me_32((h)->biSizeImage);			\
    (h)->biXPelsPerMeter = le2me_32((h)->biXPelsPerMeter);		\
    (h)->biYPelsPerMeter = le2me_32((h)->biYPelsPerMeter);		\
    (h)->biClrUsed = le2me_32((h)->biClrUsed);				\
    (h)->biClrImportant = le2me_32((h)->biClrImportant);		\
}
#define le2me_WAVEFORMATEX(h) {						\
    (h)->wFormatTag = le2me_16((h)->wFormatTag);			\
    (h)->nChannels = le2me_16((h)->nChannels);				\
    (h)->nSamplesPerSec = le2me_32((h)->nSamplesPerSec);		\
    (h)->nAvgBytesPerSec = le2me_32((h)->nAvgBytesPerSec);		\
    (h)->nBlockAlign = le2me_16((h)->nBlockAlign);			\
    (h)->wBitsPerSample = le2me_16((h)->wBitsPerSample);		\
    (h)->cbSize = le2me_16((h)->cbSize);				\
}
#define le2me_AVIINDEXENTRY(h) {					\
    (h)->ckid = le2me_32((h)->ckid);					\
    (h)->dwFlags = le2me_32((h)->dwFlags);				\
    (h)->dwChunkOffset = le2me_32((h)->dwChunkOffset);			\
    (h)->dwChunkLength = le2me_32((h)->dwChunkLength);			\
}
#define le2me_AVISTDIDXCHUNK(h) {\
    char c; \
    c = (h)->fcc[0]; (h)->fcc[0] = (h)->fcc[3]; (h)->fcc[3] = c;  \
    c = (h)->fcc[1]; (h)->fcc[1] = (h)->fcc[2]; (h)->fcc[2] = c;  \
    (h)->dwSize = le2me_32((h)->dwSize);  \
    (h)->wLongsPerEntry = le2me_16((h)->wLongsPerEntry);  \
    (h)->nEntriesInUse = le2me_32((h)->nEntriesInUse);  \
    c = (h)->dwChunkId[0]; (h)->dwChunkId[0] = (h)->dwChunkId[3]; (h)->dwChunkId[3] = c;  \
    c = (h)->dwChunkId[1]; (h)->dwChunkId[1] = (h)->dwChunkId[2]; (h)->dwChunkId[2] = c;  \
    (h)->qwBaseOffset = le2me_64((h)->qwBaseOffset);  \
    (h)->dwReserved3 = le2me_32((h)->dwReserved3);  \
}
#define le2me_AVISTDIDXENTRY(h)  {\
    (h)->dwOffset = le2me_32((h)->dwOffset);  \
    (h)->dwSize = le2me_32((h)->dwSize);  \
}
#define le2me_VideoPropHeader(h) {					\
    (h)->VideoFormatToken = le2me_32((h)->VideoFormatToken);		\
    (h)->VideoStandard = le2me_32((h)->VideoStandard);			\
    (h)->dwVerticalRefreshRate = le2me_32((h)->dwVerticalRefreshRate);	\
    (h)->dwHTotalInT = le2me_32((h)->dwHTotalInT);			\
    (h)->dwVTotalInLines = le2me_32((h)->dwVTotalInLines);		\
    (h)->dwFrameAspectRatio = le2me_32((h)->dwFrameAspectRatio);	\
    (h)->dwFrameWidthInPixels = le2me_32((h)->dwFrameWidthInPixels);	\
    (h)->dwFrameHeightInLines = le2me_32((h)->dwFrameHeightInLines);	\
    (h)->nbFieldPerFrame = le2me_32((h)->nbFieldPerFrame);		\
}
#define le2me_VIDEO_FIELD_DESC(h) {					\
    (h)->CompressedBMHeight = le2me_32((h)->CompressedBMHeight);	\
    (h)->CompressedBMWidth = le2me_32((h)->CompressedBMWidth);		\
    (h)->ValidBMHeight = le2me_32((h)->ValidBMHeight);			\
    (h)->ValidBMWidth = le2me_32((h)->ValidBMWidth);			\
    (h)->ValidBMXOffset = le2me_32((h)->ValidBMXOffset);		\
    (h)->ValidBMYOffset = le2me_32((h)->ValidBMYOffset);		\
    (h)->VideoXOffsetInT = le2me_32((h)->VideoXOffsetInT);		\
    (h)->VideoYValidStartLine = le2me_32((h)->VideoYValidStartLine);	\
}
#else
#define	le2me_MainAVIHeader(h)	    /**/
#define le2me_AVIStreamHeader(h)    /**/
#define le2me_RECT(h)		    /**/
#define le2me_BITMAPINFOHEADER(h)   /**/
#define le2me_WAVEFORMATEX(h)	    /**/
#define le2me_AVIINDEXENTRY(h)	    /**/
#define le2me_AVISTDIDXCHUNK(h)     /**/
#define le2me_AVISTDIDXENTRY(h)     /**/
#define le2me_VideoPropHeader(h)    /**/
#define le2me_VIDEO_FIELD_DESC(h)   /**/
#endif

#endif
