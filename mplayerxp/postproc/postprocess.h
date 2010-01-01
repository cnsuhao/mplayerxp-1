#ifndef HAVE_PP_H
#define HAVE_PP_H 1

#include "libpostproc/postprocess.h"
extern int pp2_init(void);
extern void pp2_uninit(void);
extern pp_context_t *pp2_get_context(int width, int height, int flags);

static inline void pp2_free_context(pp_context_t *ppContext) { pp_free_context(ppContext); }
static inline void pp2_free_mode(pp_mode_t *mode) { pp_free_mode(mode); }
static inline void  pp2_postprocess(const uint8_t * src[3], const int srcStride[3],
		 uint8_t * dst[3], int dstStride[3],
		 int horizontalSize, int verticalSize,
		 const QP_STORE_T *QP_store,  int QP_stride,
		 pp_mode_t *mode, pp_context_t *ppContext, int pict_type) {
    pp_postprocess(src,srcStride,dst,dstStride,
		   horizontalSize,verticalSize,QP_store,
		   QP_stride,mode,ppContext,pict_type);
}
static inline pp_mode_t *pp2_get_mode_by_name_and_quality(char *opts,int quality) { return pp_get_mode_by_name_and_quality(opts,quality); }
#endif
