/* screenshot prototypes */

#ifndef __VO_SCREENSHOT
#define __VO_SCREENSHOT 1

extern int gr_screenshot(const char *fname,uint8_t *planes[],int *strides,uint32_t fourcc,unsigned w,unsigned h);

#endif
