
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"

//#include "stream.h"
//#include "demuxer.h"
//#include "stheader.h"

#include "wine/mmreg.h"
#include "wine/avifmt.h"
#include "wine/vfw.h"

void print_avih_flags(MainAVIHeader *h){
  printf("MainAVIHeader.dwFlags: (%d)%s%s%s%s%s%s\n",
    h->dwFlags,
    (h->dwFlags&AVIF_HASINDEX)?" HAS_INDEX":"",
    (h->dwFlags&AVIF_MUSTUSEINDEX)?" MUST_USE_INDEX":"",
    (h->dwFlags&AVIF_ISINTERLEAVED)?" IS_INTERLEAVED":"",
    (h->dwFlags&AVIF_TRUSTCKTYPE)?" TRUST_CKTYPE":"",
    (h->dwFlags&AVIF_WASCAPTUREFILE)?" WAS_CAPTUREFILE":"",
    (h->dwFlags&AVIF_COPYRIGHTED)?" COPYRIGHTED":""
  );
}

void print_avih(MainAVIHeader *h){
  printf("======= AVI Header =======\n"
	 "us/frame: %d  (fps=%5.3f)\n"
	 "max bytes/sec: %d\n"
	 "padding: %d\n"
	 ,h->dwMicroSecPerFrame,1000000.0f/(float)h->dwMicroSecPerFrame
	 ,h->dwMaxBytesPerSec
	 ,h->dwPaddingGranularity);
  print_avih_flags(h);
  printf("frames  total: %d   initial: %d\n"
	 "streams: %d\n"
	 "Suggested BufferSize: %d\n"
	 "Size:  %d x %d\n"
	 ,h->dwTotalFrames,h->dwInitialFrames
	 ,h->dwStreams
	 ,h->dwSuggestedBufferSize
	 ,h->dwWidth,h->dwHeight);
}

void print_strh(AVIStreamHeader *h){
  printf("======= STREAM Header =======\n"
	 "Type: %.4s   FCC: %.4s (%X)\n"
	 "Flags: %d\n"
	 "Priority: %d   Language: %d\n"
	 "InitialFrames: %d\n"
	 "Rate: %d/%d = %5.3f\n"
	 "Start: %d   Len: %d\n"
	 "Suggested BufferSize: %d\n"
	 "Quality %d\n"
	 "Sample size: %d\n"
	 ,(char *)&h->fccType,(char *)&h->fccHandler,(unsigned int)h->fccHandler
	 ,h->dwFlags
	 ,h->wPriority,h->wLanguage
	 ,h->dwInitialFrames
	 ,h->dwRate,h->dwScale,(float)h->dwRate/(float)h->dwScale
	 ,h->dwStart,h->dwLength
	 ,h->dwSuggestedBufferSize
	 ,h->dwQuality
	 ,h->dwSampleSize);
}

void print_wave_header(WAVEFORMATEX *h){

  printf("======= WAVE Format =======\n"
	 "Format Tag: %d (0x%X)\n"
	 "Channels: %d\n"
	 "Samplerate: %d\n"
	 "avg byte/sec: %d\n"
	 "Block align: %d\n"
	 "bits/sample: %d\n"
	 "cbSize: %d\n"
	 ,h->wFormatTag,h->wFormatTag
	 ,h->nChannels
	 ,h->nSamplesPerSec
	 ,h->nAvgBytesPerSec
	 ,h->nBlockAlign
	 ,h->wBitsPerSample
	 ,h->cbSize);
}


void print_video_header(BITMAPINFOHEADER *h){
  printf("======= VIDEO Format ======\n"
	 "  biSize %d\n"
	 "  biWidth %d\n"
	 "  biHeight %d\n"
	 "  biPlanes %d\n"
	 "  biBitCount %d\n"
	 "  biCompression %d='%.4s'\n"
	 "  biSizeImage %d\n"
	 "===========================\n"
	 , h->biSize
	 , h->biWidth
	 , h->biHeight
	 , h->biPlanes
	 , h->biBitCount
	 , h->biCompression, (char *)&h->biCompression
	 , h->biSizeImage);
}

#if 0
void print_index(AVIINDEXENTRY *idx,int idx_size){
  int i;
  unsigned int pos[256];
  unsigned int num[256];
  for(i=0;i<256;i++) num[i]=pos[i]=0;
  for(i=0;i<idx_size;i++){
    int id=avi_stream_id(idx[i].ckid);
    if(id<0 || id>255) id=255;
    printf("%5d:  %.4s  %4X  %08X  len:%6ld  pos:%7d->%7.3f %7d->%7.3f\n",i,
      (char *)&idx[i].ckid,
      (unsigned int)idx[i].dwFlags,
      (unsigned int)idx[i].dwChunkOffset,
//      idx[i].dwChunkOffset+demuxer->movi_start,
      idx[i].dwChunkLength,
      pos[id],(float)pos[id]/18747.0f,
      num[id],(float)num[id]/23.976f
    );
    pos[id]+=idx[i].dwChunkLength;
    ++num[id];
  }
}
#endif

