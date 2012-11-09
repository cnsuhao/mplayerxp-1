/* screenshot prototypes */

#ifndef __VO_SCREENSHOT
#define __VO_SCREENSHOT 1
#include "xmpcore/xmp_enums.h"

extern MPXP_Rc gr_screenshot(const char *fname,uint8_t *planes[],unsigned *strides,uint32_t fourcc,unsigned w,unsigned h);

#endif
