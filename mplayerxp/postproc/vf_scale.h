#ifndef __VF_SCALE_H_INCLUDED
#define __VF_SCALE_H_INCLUDED

int get_sws_cpuflags();
struct SwsContext *sws_getContextFromCmdLine(int srcW, int srcH, int srcFormat, int dstW, int dstH, int dstFormat);
#endif