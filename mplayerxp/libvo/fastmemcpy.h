#ifndef __MPLAYER_MEMCPY
#define __MPLAYER_MEMCPY 1

#include "../mp_config.h"

#ifdef USE_FASTMEMCPY
#include <stddef.h>
#include <string.h> /* memcpy prototypes */
extern void * (*fast_memcpy_ptr)(void * to, const void * from, size_t len);
extern void * (*fast_stream_copy_ptr)(void * to, const void * from, size_t len);
#define memcpy(a,b,c) (*fast_memcpy_ptr)(a,b,c)
#define stream_copy(a,b,c) (*fast_stream_copy_ptr)(a,b,c)
#else
#define stream_copy(a,b,c) memcpy(a,b,c)
#endif

static inline void * stream_copy_pic(void * dst, const void * src, int bytesPerLine, int height, int dstStride, int srcStride)
{
	int i;
	void *retval=dst;

	if(dstStride == srcStride) stream_copy(dst, src, srcStride*height);
	else
	{
		for(i=0; i<height; i++)
		{
			stream_copy(dst, src, bytesPerLine);
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
