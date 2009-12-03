/*
    This file contains functions for debugging purposes only!
    They work with triplex - ptr,size,crc to control memory boundaries
    at run-time. They are useful only when you have builded mplayerxp
    with debug info (./configure --enable_debug[=3]).
    To use them please add '#include "my_malloc.h"' at end of config.h
    manually.
*/
#ifndef __MY_MALLOC_H
#define __MY_MALLOC_H 1

#include <stdlib.h>

//#define __ENABLE_MALLOC_DEBUG 1
#ifdef __ENABLE_MALLOC_DEBUG
extern void *my_malloc(size_t __size);
extern void *my_realloc(void *__ptr, size_t __size);
extern void *my_calloc (size_t __nelem, size_t __size);
extern void  my_free(void *__ptr);
extern char *my_strdup(const char *src);
#define strdup(a) my_strdup(a)
#define malloc(a) my_malloc(a)
#define memalign(a,b) my_malloc(b)
#define realloc(a,b) my_realloc(a,b)
#define calloc(a,b) my_calloc(a,b)
#define free(a) my_free(a)
#endif

#endif
