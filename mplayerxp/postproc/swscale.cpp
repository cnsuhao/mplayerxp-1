#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
/* SW scaler wrapper */
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <dlfcn.h> /* GLIBC specific. Exists under cygwin too! */

#include "osdep/bswap.h"
#include "swscale.h"
#include "libmpcodecs/codecs_ld.h"
#include "osdep/cpudetect.h"
#include "pp_msg.h"

/**
 * Convert the palette to the same packet 32-bit format as the palette
 */
void palette8torgb32(const uint8_t *src, uint8_t *dst, long num_pixels, const uint8_t *palette)
{
    long i;

    for (i=0; i<num_pixels; i++)
	((uint32_t *) dst)[i] = ((const uint32_t *) palette)[src[i]];
}

void palette8tobgr32(const uint8_t *src, uint8_t *dst, long num_pixels, const uint8_t *palette)
{
    long i;

    for (i=0; i<num_pixels; i++)
	((uint32_t *) dst)[i] = bswap_32(((const uint32_t *) palette)[src[i]]);
}

/**
 * Palette format: ABCD -> dst format: ABC
 */
void palette8torgb24(const uint8_t *src, uint8_t *dst, long num_pixels, const uint8_t *palette)
{
    long i;

    for (i=0; i<num_pixels; i++) {
	//FIXME slow?
	dst[0]= palette[src[i]*4+0];
	dst[1]= palette[src[i]*4+1];
	dst[2]= palette[src[i]*4+2];
	dst+= 3;
    }
}

void palette8tobgr24(const uint8_t *src, uint8_t *dst, long num_pixels, const uint8_t *palette)
{
    long i;

    for (i=0; i<num_pixels; i++) {
	//FIXME slow?
	dst[2]= palette[src[i]*4+0];
	dst[1]= palette[src[i]*4+1];
	dst[0]= palette[src[i]*4+2];
	dst+= 3;
    }
}

/**
 * Palette is assumed to contain BGR16, see rgb32to16 to convert the palette.
 */
void palette8torgb16(const uint8_t *src, uint8_t *dst, long num_pixels, const uint8_t *palette)
{
    long i;
    for (i=0; i<num_pixels; i++)
	((uint16_t *)dst)[i] = ((const uint16_t *)palette)[src[i]];
}
void palette8tobgr16(const uint8_t *src, uint8_t *dst, long num_pixels, const uint8_t *palette)
{
    long i;
    for (i=0; i<num_pixels; i++)
	((uint16_t *)dst)[i] = bswap_16(((const uint16_t *)palette)[src[i]]);
}

/**
 * Palette is assumed to contain BGR15, see rgb32to15 to convert the palette.
 */
void palette8torgb15(const uint8_t *src, uint8_t *dst, long num_pixels, const uint8_t *palette)
{
    long i;
    for (i=0; i<num_pixels; i++)
	((uint16_t *)dst)[i] = ((const uint16_t *)palette)[src[i]];
}
void palette8tobgr15(const uint8_t *src, uint8_t *dst, long num_pixels, const uint8_t *palette)
{
    long i;
    for (i=0; i<num_pixels; i++)
	((uint16_t *)dst)[i] = bswap_16(((const uint16_t *)palette)[src[i]]);
}


int sws_init(void)
{
    return 1;
}

void sws_uninit(void)
{
}
