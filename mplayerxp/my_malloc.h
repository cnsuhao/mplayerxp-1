/*
    This file contains functions for debugging purposes only!
    They work with triplex - ptr,size,crc to control memory boundaries
    at run-time. They are useful only when you have builded mplayerxp
    with debug info (./configure --enable_debug[=3]).
    To use them please add '#include "my_malloc.h"' at end of mp_config.h
    manually.
*/
#ifndef __MY_MALLOC_H
#define __MY_MALLOC_H 1
#include "mp_config.h"

#include <stdlib.h>

//#define __ENABLE_MALLOC_DEBUG 1
#ifdef __ENABLE_MALLOC_DEBUG
extern any_t*my_malloc(size_t __size);
extern any_t*my_realloc(any_t*__ptr, size_t __size);
extern any_t*my_calloc (size_t __nelem, size_t __size);
extern void  my_free(any_t*__ptr);
extern char *my_strdup(const char *src);
#define strdup(a) my_strdup(a)
#define malloc(a) my_malloc(a)
#define memalign(a,b) my_malloc(b)
#define realloc(a,b) my_realloc(a,b)
#define calloc(a,b) my_calloc(a,b)
#define free(a) my_free(a)
#endif

/* Pseudo-randomizing memory objects makes memory exploits harder */
extern any_t*	random_malloc(size_t __size,unsigned upper_rnd_limit);

#endif
