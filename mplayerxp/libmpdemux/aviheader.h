#ifndef _aviheader_h
#define	_aviheader_h

//#include "mpxp_config.h"	/* get correct definition WORDS_BIGENDIAN */
#include "osdep/bswap.h"

/*
 * Some macros to swap little endian structures read from an AVI file
 * into machine endian format
 */
#ifdef WORDS_BIGENDIAN
static inline void le2me_MainAVIHeader(MainAVIHeader*h) {
    h->dwMicroSecPerFrame = le2me_32(h->dwMicroSecPerFrame);
    h->dwMaxBytesPerSec = le2me_32(h->dwMaxBytesPerSec);
    h->dwPaddingGranularity = le2me_32(h->dwPaddingGranularity);
    h->dwFlags = le2me_32(h->dwFlags);
    h->dwTotalFrames = le2me_32(h->dwTotalFrames);
    h->dwInitialFrames = le2me_32(h->dwInitialFrames);
    h->dwStreams = le2me_32(h->dwStreams);
    h->dwSuggestedBufferSize = le2me_32(h->dwSuggestedBufferSize);
    h->dwWidth = le2me_32(h->dwWidth);
    h->dwHeight = le2me_32(h->dwHeight);
}

static inline void le2me_RECT(RECT*h) {
    h->left = le2me_16(h->left);
    h->top = le2me_16(h->top);
    h->right = le2me_16(h->right);
    h->bottom = le2me_16(h->bottom);
}

static inline void le2me_AVIStreamHeader(AVIStreamHeader*h) {
    h->fccType = le2me_32(h->fccType);
    h->fccHandler = le2me_32(h->fccHandler);
    h->dwFlags = le2me_32(h->dwFlags);
    h->wPriority = le2me_16(h->wPriority);
    h->wLanguage = le2me_16(h->wLanguage);
    h->dwInitialFrames = le2me_32(h->dwInitialFrames);
    h->dwScale = le2me_32(h->dwScale);
    h->dwRate = le2me_32(h->dwRate);
    h->dwStart = le2me_32(h->dwStart);
    h->dwLength = le2me_32(h->dwLength);
    h->dwSuggestedBufferSize = le2me_32(h->dwSuggestedBufferSize);
    h->dwQuality = le2me_32(h->dwQuality);
    h->dwSampleSize = le2me_32(h->dwSampleSize);
    le2me_RECT(&h->rcFrame);
}
static inline void le2me_BITMAPINFOHEADER(BITMAPINFOHEADER*h) {
    h->biSize = le2me_32(h->biSize);
    h->biWidth = le2me_32(h->biWidth);
    h->biHeight = le2me_32(h->biHeight);
    h->biPlanes = le2me_16(h->biPlanes);
    h->biBitCount = le2me_16(h->biBitCount);
    h->biCompression = le2me_32(h->biCompression);
    h->biSizeImage = le2me_32(h->biSizeImage);
    h->biXPelsPerMeter = le2me_32(h->biXPelsPerMeter);
    h->biYPelsPerMeter = le2me_32(h->biYPelsPerMeter);
    h->biClrUsed = le2me_32(h->biClrUsed);
    h->biClrImportant = le2me_32(h->biClrImportant);
}
static inline void le2me_WAVEFORMATEX(WAVEFORMATEX*h) {
    h->wFormatTag = le2me_16(h->wFormatTag);
    h->nChannels = le2me_16(h->nChannels);
    h->nSamplesPerSec = le2me_32(h->nSamplesPerSec);
    h->nAvgBytesPerSec = le2me_32(h->nAvgBytesPerSec);
    h->nBlockAlign = le2me_16(h->nBlockAlign);
    h->wBitsPerSample = le2me_16(h->wBitsPerSample);
    h->cbSize = le2me_16(h->cbSize);
}
static inline void le2me_AVIINDEXENTRY(AVIINDEXENTRY*h) {
    h->ckid = le2me_32(h->ckid);
    h->dwFlags = le2me_32(h->dwFlags);
    h->dwChunkOffset = le2me_32(h->dwChunkOffset);
    h->dwChunkLength = le2me_32(h->dwChunkLength);
}
static inline void le2me_avisuperindex_chunk(avisuperindex_chunk*h) {
    char c;
    c = h->fcc[0]; h->fcc[0] = h->fcc[3]; h->fcc[3] = c;
    c = h->fcc[1]; h->fcc[1] = h->fcc[2]; h->fcc[2] = c;
    h->dwSize = le2me_32(h->dwSize);
    h->wLongsPerEntry = le2me_16(h->wLongsPerEntry);
    h->nEntriesInUse = le2me_32(h->nEntriesInUse);
    c = h->dwChunkId[0]; h->dwChunkId[0] = h->dwChunkId[3]; h->dwChunkId[3] = c;
    c = h->dwChunkId[1]; h->dwChunkId[1] = h->dwChunkId[2]; h->dwChunkId[2] = c;
    h->qwBaseOffset = le2me_64(h->qwBaseOffset);
    h->dwReserved3 = le2me_32(h->dwReserved3);
}
static inline void le2me_avistdindex_entry(avistdindex_entry*h)  {
    h->dwOffset = le2me_32(h->dwOffset);
    h->dwSize = le2me_32(h->dwSize);
}
static inline void le2me_avistdindex_chunk(avistdindex_chunk*h)  {
    h->fcc = le2me_32(h->fcc);
    h->dwSize = le2me_32(h->dwSize);
    h->wLongsPerEntry = le2me_16(h->LongsPerEntry);
    h->nEntriesInUse = le2me_32(nEntriesInUse);
    h->dwChunkId = le2me_32(h->dwChunkId);
    h->qwBaseOffset = le2me_64(h->qwBaseOffset);
}

static inline void le2me_VideoPropHeader(VideoPropHeader*h) {
    h->VideoFormatToken = le2me_32(h->VideoFormatToken);
    h->VideoStandard = le2me_32(h->VideoStandard);
    h->dwVerticalRefreshRate = le2me_32(h->dwVerticalRefreshRate);
    h->dwHTotalInT = le2me_32(h->dwHTotalInT);
    h->dwVTotalInLines = le2me_32(h->dwVTotalInLines);
    h->dwFrameAspectRatio = le2me_32(h->dwFrameAspectRatio);
    h->dwFrameWidthInPixels = le2me_32(h->dwFrameWidthInPixels);
    h->dwFrameHeightInLines = le2me_32(h->dwFrameHeightInLines);
    h->nbFieldPerFrame = le2me_32(h->nbFieldPerFrame);
}
static inline void le2me_VIDEO_FIELD_DESC(VIDEO_FIELD_DESC*h) {
    h->CompressedBMHeight = le2me_32(h->CompressedBMHeight);
    h->CompressedBMWidth = le2me_32(h->CompressedBMWidth);
    h->ValidBMHeight = le2me_32(h->ValidBMHeight);
    h->ValidBMWidth = le2me_32(h->ValidBMWidth);
    h->ValidBMXOffset = le2me_32(h->ValidBMXOffset);
    h->ValidBMYOffset = le2me_32(h->ValidBMYOffset);
    h->VideoXOffsetInT = le2me_32(h->VideoXOffsetInT);
    h->VideoYValidStartLine = le2me_32(h->VideoYValidStartLine);
}
#else
static inline void le2me_MainAVIHeader(MainAVIHeader*h) { UNUSED(h); }
static inline void le2me_AVIStreamHeader(AVIStreamHeader*h){ UNUSED(h); }
static inline void le2me_RECT(RECT*h) { UNUSED(h); }
static inline void le2me_BITMAPINFOHEADER(BITMAPINFOHEADER*h){ UNUSED(h); }
static inline void le2me_WAVEFORMATEX(WAVEFORMATEX*h){ UNUSED(h); }
static inline void le2me_AVIINDEXENTRY(AVIINDEXENTRY*h){ UNUSED(h); }
static inline void le2me_avisuperindex_chunk(avisuperindex_chunk*h){ UNUSED(h); }
static inline void le2me_avistdindex_entry(avistdindex_entry*h){ UNUSED(h); }
static inline void le2me_avistdindex_chunk(avistdindex_chunk*h){UNUSED(h); }
static inline void le2me_VideoPropHeader(VideoPropHeader*h){ UNUSED(h); }
static inline void le2me_VIDEO_FIELD_DESC(VIDEO_FIELD_DESC*h){ UNUSED(h); }
#endif

#endif
