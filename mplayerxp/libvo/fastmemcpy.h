#ifndef __MPLAYER_MEMCPY
#define __MPLAYER_MEMCPY 1

#include "../config.h"

#ifdef USE_FASTMEMCPY
#include <stddef.h>
#include <string.h> /* memcpy prototypes */
extern void * (*fast_memcpy_ptr)(void * to, const void * from, size_t len);
extern void * (*mem2agpcpy_ptr)(void * to, const void * from, size_t len);
#define memcpy(a,b,c) (*fast_memcpy_ptr)(a,b,c)
#define mem2agpcpy(a,b,c) (*mem2agpcpy_ptr)(a,b,c)
#else
#define mem2agpcpy(a,b,c) memcpy(a,b,c)
#endif

static inline void * mem2agpcpy_pic(void * dst, const void * src, int bytesPerLine, int height, int dstStride, int srcStride)
{
	int i;
	void *retval=dst;

	if(dstStride == srcStride) mem2agpcpy(dst, src, srcStride*height);
	else
	{
		for(i=0; i<height; i++)
		{
			mem2agpcpy(dst, src, bytesPerLine);
			src+= srcStride;
			dst+= dstStride;
		}
	}

	return retval;
}

static inline void * memcpy_pic(void * dst, const void * src, int bytesPerLine, int height, int dstStride, int srcStride)
{
	int i;
	void *retval=dst;

	if(dstStride == srcStride) memcpy(dst, src, srcStride*height);
	else
	{
		for(i=0; i<height; i++)
		{
			memcpy(dst, src, bytesPerLine);
			src+= srcStride;
			dst+= dstStride;
		}
	}

	return retval;
}

#endif
