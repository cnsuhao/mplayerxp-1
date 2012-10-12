#ifndef HAVE_PP_H
#define HAVE_PP_H 1

#include "libpostproc/postprocess.h"
extern int pp2_init(void);
extern void pp2_uninit(void);
extern pp_context *pp2_get_context(int width, int height, int flags);
#endif
