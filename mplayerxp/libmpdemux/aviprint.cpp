#define __STDC_FORMAT_MACROS
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../mp_config.h"

#include "libmpstream/stream.h"
#include "demuxer.h"
#include "aviprint.h"
#include "demux_msg.h"

extern int avi_stream_id(unsigned int id);

void print_avih_flags(MainAVIHeader *h){
  MSG_V("MainAVIHeader.dwFlags: (%d)%s%s%s%s%s%s\n",
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
  MSG_V("======= AVI Header =======\n"
	 "us/frame: %d  (fps=%5.3f)\n"
	 "max bytes/sec: %d\n"
	 "padding: %d\n"
	 ,h->dwMicroSecPerFrame,1000000.0f/(float)h->dwMicroSecPerFrame
	 ,h->dwMaxBytesPerSec
	 ,h->dwPaddingGranularity);
  print_avih_flags(h);
  MSG_V("frames  total: %d   initial: %d\n"
	 "streams: %d\n"
	 "Suggested BufferSize: %d\n"
	 "Size:  %d x %d\n"
	 ,h->dwTotalFrames,h->dwInitialFrames
	 ,h->dwStreams
	 ,h->dwSuggestedBufferSize
	 ,h->dwWidth,h->dwHeight);
}

void print_strh(AVIStreamHeader *h){
  MSG_V("======= STREAM Header =======\n"
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

void print_wave_header(WAVEFORMATEX *h,unsigned size)
{
  MSG_V("======= WAVE Format (chunk size: %u )=======\n"
	 "Format Tag: %d (0x%X)\n"
	 "Channels: %d\n"
	 "Samplerate: %d\n"
	 "avg byte/sec: %d\n"
	 "Block align: %d\n"
	 "bits/sample: %d\n"
	 "cbSize: %d\n"
	 ,size
	 ,h->wFormatTag,h->wFormatTag
	 ,h->nChannels
	 ,h->nSamplesPerSec
	 ,h->nAvgBytesPerSec
	 ,h->nBlockAlign
	 ,h->wBitsPerSample
	 ,h->cbSize);
  if(h->wFormatTag==0x55 && h->cbSize>=12){
    MPEGLAYER3WAVEFORMAT* h2=(MPEGLAYER3WAVEFORMAT*)h;
     MSG_V("mp3.wID=%d\n"
	    "mp3.fdwFlags=0x%X\n"
	    "mp3.nBlockSize=%d\n"
	    "mp3.nFramesPerBlock=%d\n"
	    "mp3.nCodecDelay=%d\n"
	    ,h2->wID
	    ,h2->fdwFlags
	    ,h2->nBlockSize
	    ,h2->nFramesPerBlock
	    ,h2->nCodecDelay);
  }
  else if (h->cbSize > 0)
  {
    unsigned i;
    uint8_t* p = ((uint8_t*)h) + sizeof(WAVEFORMATEX);
    MSG_V("Unknown extra header dump: ");
    for (i = 0; i < h->cbSize; i++)
	MSG_V("[%02X] ",(unsigned)p[i]);
    MSG_V("\n");
  }
  MSG_V("======= End of WAVE Format =======\n");
}

static char * aspect_ratios[]=
{
"forbidden",
"1.0000 (VGA)",
"0.6735",
"0.7031 (16:9 - 625 lines)",
"0.7615",
"0.8055",
"0.8437 (15:9 - 525 lines)",
"0.8935",
"0.9375 (CCIR 601-625 lines)",
"0.9815",
"1.0255",
"1.0695",
"1.1250 (CCIR 601-525 lines)",
"1.1575",
"1.2015",
"reserved"
};

static char *decode_aspect_ratio(unsigned char id)
{
    if(id>15) id=0;
    return aspect_ratios[id];
}

void print_video_header(BITMAPINFOHEADER *h,unsigned size){
  MSG_V("======= VIDEO Format (chunk size: %u )======\n"
	 "  biSize %d\n"
	 "  biWidth %d\n"
	 "  biHeight %d\n"
	 "  biPlanes %d\n"
	 "  biBitCount %d\n"
	 "  biCompression %08X='%.4s'\n"
	 "  biSizeImage %d\n"
	 "  biXPelPerMeter %d\n"
	 "  biYPelPerMeter %d\n"
	 "  biClrUsed %d\n"
	 "  biClrIpmortant %d\n"
	 , size
	 , h->biSize
	 , h->biWidth
	 , h->biHeight
	 , h->biPlanes
	 , h->biBitCount
	 , h->biCompression, (char *)&h->biCompression
	 , h->biSizeImage
	 , h->biXPelsPerMeter
	 , h->biYPelsPerMeter
	 , h->biClrUsed
	 , h->biClrImportant
	 );
    if(h->biSize>sizeof(BITMAPINFOHEADER))
    {
      if(strncmp((char *)&h->biCompression,"MPGI",4) == 0)
	MSG_V("==== MPEG DIB Extension ===\n"
	      "bPixAspectRatio %s\n"
	      "===========================\n"
	      ,decode_aspect_ratio(*(((char *)h)+0x28)));
      else
      {
	unsigned i;
	uint8_t* p = ((uint8_t*)h) + sizeof(BITMAPINFOHEADER);
	MSG_V("Unknown extra header dump: ");
	    for (i = 0; i < (unsigned)h->biSize-sizeof(BITMAPINFOHEADER); i++) MSG_V("[%02X] ",(unsigned)p[i]);
	MSG_V("\n");
      }
    }
    MSG_V("======= End of Video Format =======\n");
}

void print_index(AVIINDEXENTRY *idx,int idx_size){
  int i;
  unsigned int pos[256];
  unsigned int num[256];
  for(i=0;i<256;i++) num[i]=pos[i]=0;
  for(i=0;i<idx_size;i++){
    int id=avi_stream_id(idx[i].ckid);
    if(id<0 || id>255) id=255;
    MSG_V("%5d:  %.4s  %4X  %08X  len:%6d  pos:%7d->%7.3f %7d->%7.3f\n",i,
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

void print_avistdindex_chunk(avistdindex_chunk *h){
    MSG_V("====== AVI Standard Index Header ========\n"
	  "  FCC (%.4s) dwSize (%d) wLongsPerEntry(%d)\n"
	  "  bIndexSubType (%d) bIndexType (%d)\n"
	  "  nEntriesInUse (%d) dwChunkId (%.4s)\n"
	  "  qwBaseOffset (0x%"PRIX64") dwReserved3 (%d)\n"
	  "===========================\n",
	  h->fcc, h->dwSize, h->wLongsPerEntry,
	  h->bIndexSubType, h->bIndexType,
	  h->nEntriesInUse, h->dwChunkId,
	  h->qwBaseOffset, h->dwReserved3);
}
void print_avisuperindex_chunk(avisuperindex_chunk *h){
    MSG_V("====== AVI Super Index Header ========\n"
	  "  FCC (%.4s) dwSize (%d) wLongsPerEntry(%d)\n"
	  "  bIndexSubType (%d) bIndexType (%d)\n"
	  "  nEntriesInUse (%d) dwChunkId (%.4s)\n"
	  "  dwReserved[0] (%d) dwReserved[1] (%d) dwReserved[2] (%d)\n"
	  "===========================\n",
	  h->fcc, h->dwSize, h->wLongsPerEntry,
	  h->bIndexSubType, h->bIndexType,
	  h->nEntriesInUse, h->dwChunkId,
	  h->dwReserved[0], h->dwReserved[1], h->dwReserved[2]);
}

void print_vprp(VideoPropHeader *vprp){
  int i;
  MSG_V("======= Video Properties Header =======\n"
	"Format: %d  VideoStandard: %d\n"
	"VRefresh: %d  HTotal: %d  VTotal: %d\n"
	"FrameAspect: %d:%d  Framewidth: %d  Frameheight: %d\n"
	"Fields: %d\n",
	vprp->VideoFormatToken,vprp->VideoStandard,
	vprp->dwVerticalRefreshRate, vprp->dwHTotalInT, vprp->dwVTotalInLines,
	vprp->dwFrameAspectRatio >> 16, vprp->dwFrameAspectRatio & 0xffff,
	vprp->dwFrameWidthInPixels, vprp->dwFrameHeightInLines,
	vprp->nbFieldPerFrame);
  for (i=0; i<vprp->nbFieldPerFrame; i++) {
    VIDEO_FIELD_DESC *vfd = &vprp->FieldInfo[i];
    MSG_V("  == Field %d description ==\n"
	  "  CompressedBMHeight: %d  CompressedBMWidth: %d\n"
	  "  ValidBMHeight: %d  ValidBMWidth: %d\n"
	  "  ValidBMXOffset: %d  ValidBMYOffset: %d\n"
	  "  VideoXOffsetInT: %d  VideoYValidStartLine: %d\n",
	i,
	vfd->CompressedBMHeight, vfd->CompressedBMWidth,
	vfd->ValidBMHeight, vfd->ValidBMWidth,
	vfd->ValidBMXOffset, vfd->ValidBMYOffset,
	vfd->VideoXOffsetInT, vfd->VideoYValidStartLine);
  }
  MSG_V("=======================================\n");
}

