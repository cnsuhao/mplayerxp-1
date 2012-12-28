#ifndef MPXP_AVIPRINT_H
#define MPXP_AVIPRINT_H 1

#include "win32loader/wine/mmreg.h"
#include "win32loader/wine/avifmt.h"
#include "win32loader/wine/vfw.h"

extern void print_avih_flags(MainAVIHeader *h);
extern void print_avih(MainAVIHeader *h);
extern void print_strh(AVIStreamHeader *h);
extern void print_wave_header(WAVEFORMATEX *h,unsigned size);
extern void print_video_header(BITMAPINFOHEADER *h,unsigned size);
extern void print_index(AVIINDEXENTRY *idx,int idx_size);
extern void print_avistdindex_chunk(avistdindex_chunk *h);
extern void print_avisuperindex_chunk(avisuperindex_chunk *h);
extern void print_vprp(VideoPropHeader *vprp);
#endif
