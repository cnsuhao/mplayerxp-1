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

static void *dll_handle=NULL;
static void (*sws_freeContext_ptr)(struct SwsContext *swsContext);
static struct SwsContext *(*sws_getContext_ptr)(int srcW, int srcH, int srcFormat, int dstW, int dstH, int dstFormat, int flags,
			 SwsFilter *srcFilter, SwsFilter *dstFilter, double *param);
static int (*sws_scale_ptr)(struct SwsContext *context, uint8_t* src[], int srcStride[], int srcSliceY,
                           int srcSliceH, uint8_t* dst[], int dstStride[]);
static int (*sws_scale_ordered_ptr)(struct SwsContext *context, uint8_t* src[], int srcStride[], int srcSliceY,
                           int srcSliceH, uint8_t* dst[], int dstStride[]);
static int (*sws_setColorspaceDetails_ptr)(struct SwsContext *c, const int inv_table[4], int srcRange, const int table[4], int dstRange, int brightness, int contrast, int saturation);
static int (*sws_getColorspaceDetails_ptr)(struct SwsContext *c, int **inv_table, int *srcRange, int **table, int *dstRange, int *brightness, int *contrast, int *saturation);
static SwsVector *(*sws_getGaussianVec_ptr)(double variance, double quality);
static SwsVector *(*sws_getConstVec_ptr)(double c, int length);
static SwsVector *(*sws_getIdentityVec_ptr)(void);
static void (*sws_scaleVec_ptr)(SwsVector *a, double scalar);
static void (*sws_normalizeVec_ptr)(SwsVector *a, double height);
static void (*sws_freeVec_ptr)(SwsVector *a);
static void (*sws_rgb2rgb_init_ptr)(int flags);

static SwsFilter *(*sws_getDefaultFilter_ptr)(float lumaGBlur, float chromaGBlur, 
				float lumaSarpen, float chromaSharpen,
				float chromaHShift, float chromaVShift,
				int verbose);
static void (*sws_freeFilter_ptr)(SwsFilter *filter);

static void (*palette8torgb16_ptr)(const uint8_t *src, uint8_t *dst, long num_pixels, const uint8_t *palette);
static void (*palette8tobgr16_ptr)(const uint8_t *src, uint8_t *dst, long num_pixels, const uint8_t *palette);
static void (*palette8torgb15_ptr)(const uint8_t *src, uint8_t *dst, long num_pixels, const uint8_t *palette);
static void (*palette8tobgr15_ptr)(const uint8_t *src, uint8_t *dst, long num_pixels, const uint8_t *palette);
static void (*palette8topacked32_ptr)(const uint8_t *src, uint8_t *dst, long num_pixels, const uint8_t *palette);
static void (*palette8topacked24_ptr)(const uint8_t *src, uint8_t *dst, long num_pixels, const uint8_t *palette);

static int load_dll(const char *libname)
{
  if(!(dll_handle=ld_codec(libname,"http://ffmpeg.sf.net"))) return 0;
  sws_getContext_ptr=ld_sym(dll_handle,"sws_getContext");
  sws_freeContext_ptr=ld_sym(dll_handle,"sws_freeContext");
  sws_scale_ordered_ptr=ld_sym(dll_handle,"sws_scale_ordered");
  sws_scale_ptr=ld_sym(dll_handle,"sws_scale");
  sws_getColorspaceDetails_ptr=ld_sym(dll_handle,"sws_getColorspaceDetails");
  sws_setColorspaceDetails_ptr=ld_sym(dll_handle,"sws_setColorspaceDetails");
  sws_getGaussianVec_ptr=ld_sym(dll_handle,"sws_getGaussianVec");
  sws_getConstVec_ptr=ld_sym(dll_handle,"sws_getConstVec");
  sws_getIdentityVec_ptr=ld_sym(dll_handle,"sws_getIdentityVec");
  sws_scaleVec_ptr=ld_sym(dll_handle,"sws_scaleVec");
  sws_normalizeVec_ptr=ld_sym(dll_handle,"sws_normalizeVec");
  sws_freeVec_ptr=ld_sym(dll_handle,"sws_freeVec");
  sws_getDefaultFilter_ptr=ld_sym(dll_handle,"sws_getDefaultFilter");
  sws_freeFilter_ptr=ld_sym(dll_handle,"sws_freeFilter");
  sws_rgb2rgb_init_ptr=ld_sym(dll_handle,"sws_rgb2rgb_init");
  palette8torgb16_ptr=ld_sym(dll_handle,"palette8torgb16");
  palette8tobgr16_ptr=ld_sym(dll_handle,"palette8tobgr16");
  palette8torgb15_ptr=ld_sym(dll_handle,"palette8torgb15");
  palette8tobgr15_ptr=ld_sym(dll_handle,"palette8tobgr15");
  palette8topacked24_ptr=ld_sym(dll_handle,"palette8topacked24");
  palette8topacked32_ptr=ld_sym(dll_handle,"palette8topacked32");
  return sws_getContext_ptr && sws_freeContext_ptr && sws_scale_ordered_ptr && sws_scale_ptr &&
	 sws_getColorspaceDetails_ptr && sws_setColorspaceDetails_ptr && sws_getGaussianVec_ptr &&
	 sws_getConstVec_ptr && sws_getIdentityVec_ptr && sws_scaleVec_ptr && sws_normalizeVec_ptr &&
	 sws_freeVec_ptr && sws_getDefaultFilter_ptr && sws_freeFilter_ptr && sws_rgb2rgb_init_ptr &&
	 palette8torgb16_ptr && palette8tobgr16_ptr && palette8torgb15_ptr && palette8tobgr15_ptr &&
	 palette8topacked24_ptr && palette8topacked32_ptr;
}

static int load_avcodec( void )
{
    if(!load_dll(codec_name("libswscale"SLIBSUFFIX))) /* try local copy first */
    {
	MSG_ERR("Detected error during loading libswscale"SLIBSUFFIX"!\n");
	return 0;
    }
    return 1;
}

void sws_freeContext(struct SwsContext *swsContext) {
    (*sws_freeContext_ptr)(swsContext);
}

struct SwsContext *sws_getContext(int srcW, int srcH, int srcFormat, int dstW, int dstH, int dstFormat, int flags,
			 SwsFilter *srcFilter, SwsFilter *dstFilter, double *param)
{
    return (*sws_getContext_ptr)(srcW,srcH,srcFormat,dstW,dstH,dstFormat,flags,srcFilter,dstFilter,param);
}

int sws_scale(struct SwsContext *context, uint8_t* src[], int srcStride[], int srcSliceY,
                           int srcSliceH, uint8_t* dst[], int dstStride[])
{
    return (*sws_scale_ptr)(context,src,srcStride,srcSliceY,srcSliceH,dst,dstStride);
}

int sws_scale_ordered(struct SwsContext *context, uint8_t* src[], int srcStride[], int srcSliceY,
                           int srcSliceH, uint8_t* dst[], int dstStride[])
{
    return (*sws_scale_ordered_ptr)(context,src,srcStride,srcSliceY,srcSliceH,dst,dstStride);
}

int sws_setColorspaceDetails(struct SwsContext *c, const int inv_table[4], int srcRange, const int table[4], int dstRange, int brightness, int contrast, int saturation)
{
    return (*sws_setColorspaceDetails_ptr)(c,inv_table,srcRange,table,dstRange,brightness,contrast,saturation);
}
int sws_getColorspaceDetails(struct SwsContext *c, int **inv_table, int *srcRange, int **table, int *dstRange, int *brightness, int *contrast, int *saturation)
{
    return (*sws_getColorspaceDetails_ptr)(c,inv_table,srcRange,table,dstRange,brightness,contrast,saturation);
}

SwsVector *sws_getGaussianVec(double variance, double quality)
{
    return (*sws_getGaussianVec_ptr)(variance,quality);
}

SwsVector *sws_getConstVec(double c, int length)
{
    return (*sws_getConstVec_ptr)(c,length);
}
SwsVector *sws_getIdentityVec(void)
{
    return (*sws_getIdentityVec_ptr)();
}

void sws_scaleVec(SwsVector *a, double scalar) {
    (*sws_scaleVec_ptr)(a,scalar);
}
void sws_normalizeVec(SwsVector *a, double height){
    (*sws_normalizeVec_ptr)(a,height);
}
void sws_freeVec(SwsVector *a) {
    (*sws_freeVec_ptr)(a);
}

SwsFilter *sws_getDefaultFilter(float lumaGBlur, float chromaGBlur, 
				float lumaSharpen, float chromaSharpen,
				float chromaHShift, float chromaVShift,
				int verbose)
{
    return (*sws_getDefaultFilter_ptr)(lumaGBlur,chromaGBlur,lumaSharpen,chromaSharpen,chromaHShift,chromaVShift,verbose);
}
void sws_freeFilter(SwsFilter *filter)
{
    (*sws_freeFilter_ptr)(filter);
}

void palette8torgb16(const uint8_t *src, uint8_t *dst, long num_pixels, const uint8_t *palette)
{
    (*palette8torgb16_ptr)(src,dst,num_pixels,palette);
}
void palette8tobgr16(const uint8_t *src, uint8_t *dst, long num_pixels, const uint8_t *palette)
{
    (*palette8tobgr16_ptr)(src,dst,num_pixels,palette);
}
void palette8torgb15(const uint8_t *src, uint8_t *dst, long num_pixels, const uint8_t *palette)
{
    (*palette8torgb15_ptr)(src,dst,num_pixels,palette);
}
void palette8tobgr15(const uint8_t *src, uint8_t *dst, long num_pixels, const uint8_t *palette)
{
    (*palette8tobgr15_ptr)(src,dst,num_pixels,palette);
}

void palette8tobgr32(const uint8_t *src, uint8_t *dst, long num_pixels, const uint8_t *palette)
{
    (*palette8topacked32_ptr)(src,dst,num_pixels,palette);
}
void palette8tobgr24(const uint8_t *src, uint8_t *dst, long num_pixels, const uint8_t *palette)
{
    (*palette8topacked24_ptr)(src,dst,num_pixels,palette);
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


void (*yv12toyuy2)(const uint8_t *ysrc, const uint8_t *usrc, const uint8_t *vsrc, uint8_t *dst,
                    long width, long height,
                    long lumStride, long chromStride, long dstStride);
void (*yuv422ptoyuy2)(const uint8_t *ysrc, const uint8_t *usrc, const uint8_t *vsrc, uint8_t *dst,
                    long width, long height,
                    long lumStride, long chromStride, long dstStride);
void (*rgb24tobgr24)(const uint8_t *src, uint8_t *dst, long src_size);
void (*rgb32tobgr32)(const uint8_t *src, uint8_t *dst, long src_size);
void (*vu9_to_vu12)(const uint8_t *src1, const uint8_t *src2,
                    uint8_t *dst1, uint8_t *dst2,
                    long width, long height,
                    long srcStride1, long srcStride2,
                    long dstStride1, long dstStride2);


void sws_rgb2rgb_init(int flags) {
    (*sws_rgb2rgb_init)(flags);
/* init additional stuff here */
    yv12toyuy2=ld_sym(dll_handle,"yv12toyuy2");
    yuv422ptoyuy2=ld_sym(dll_handle,"yuv422ptoyuy2");
    vu9_to_vu12=ld_sym(dll_handle,"vu9_to_vu12");
    rgb24tobgr24=ld_sym(dll_handle,"rgb24tobgr24");
    rgb32tobgr32=ld_sym(dll_handle,"rgb32tobgr32");
}

int sws_init(void)
{
    if(!dll_handle) return load_avcodec();
    return 1;
}

void sws_uninit(void)
{
    if(dll_handle) dlclose(dll_handle);
    dll_handle=NULL;
}
