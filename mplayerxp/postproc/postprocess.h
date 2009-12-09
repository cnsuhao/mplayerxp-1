#ifndef HAVE_PP_H
#define HAVE_PP_H 1

#include "libpostproc/postprocess.h"
extern int pp2_init(void);
extern void pp2_uninit(void);

extern pp_context_t *pp2_get_context(int width, int height, int flags);
extern void pp2_free_context(pp_context_t *ppContext);
extern void pp2_free_mode(pp_mode_t *mode);
extern void  pp2_postprocess(uint8_t * src[3], int srcStride[3],
		 uint8_t * dst[3], int dstStride[3],
		 int horizontalSize, int verticalSize,
		 QP_STORE_T *QP_store,  int QP_stride,
		 pp_mode_t *mode, pp_context_t *ppContext, int pict_type);
extern pp_mode_t *pp2_get_mode_by_name_and_quality(char *opts,int quality);
#endif
