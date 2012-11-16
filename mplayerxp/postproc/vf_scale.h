#ifndef __VF_SCALE_H_INCLUDED
#define __VF_SCALE_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

int get_sws_cpuflags();
struct SwsContext *sws_getContextFromCmdLine(int srcW, int srcH, int srcFormat, int dstW, int dstH, int dstFormat);
#ifdef __cplusplus
}
#endif

#endif