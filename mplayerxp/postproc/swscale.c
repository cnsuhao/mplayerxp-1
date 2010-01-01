/* SW scaler wrapper */
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <dlfcn.h> /* GLIBC specific. Exists under cygwin too! */

#include "../mp_config.h"
#include "swscale.h"
#include "../libmpcodecs/codecs_ld.h"
#include "../cpudetect.h"
#define MSGT_CLASS MSGT_PP
#include "../__mp_msg.h"

void palette8tobgr32(const uint8_t *src, uint8_t *dst, long num_pixels, const uint8_t *palette)
{
    palette8topacked32(src,dst,num_pixels,palette);
}
void palette8tobgr24(const uint8_t *src, uint8_t *dst, long num_pixels, const uint8_t *palette)
{
    palette8topacked24(src,dst,num_pixels,palette);
}

/* TODO !!! */
void palette8torgb32(const uint8_t *src, uint8_t *dst, long num_pixels, const uint8_t *palette)
{
    palette8tobgr32(src,dst,num_pixels,palette);
}
void palette8torgb24(const uint8_t *src, uint8_t *dst, long num_pixels, const uint8_t *palette)
{
    palette8tobgr24(src,dst,num_pixels,palette);
}

int sws_init(void)
{
    return 1;
}

void sws_uninit(void)
{
}
