#ifndef __MPLAYER_MEMCPY
#define __MPLAYER_MEMCPY 1
#include "mpxp_config.h"
using namespace mpxp;

#ifdef USE_FASTMEMCPY
#include <stddef.h>
#include <string.h> /* memcpy prototypes */
namespace mpxp {
    extern any_t* (*fast_memcpy_ptr)(any_t* to, const any_t* from, size_t len);
    extern any_t* (*fast_stream_copy_ptr)(any_t* to, const any_t* from, size_t len);
#define memcpy(a,b,c) (*fast_memcpy_ptr)(a,b,c)
#define stream_copy(a,b,c) (*fast_stream_copy_ptr)(a,b,c)
#else
#define stream_copy(a,b,c) ::memcpy(a,b,c)
#endif

    inline any_t* stream_copy_pic(any_t* dst, const any_t* src, int bytesPerLine, int height, int dstStride, int srcStride)
    {
	int i;
	any_t*retval=dst;

	if(dstStride == srcStride) stream_copy(dst, src, srcStride*height);
	else {
	    for(i=0; i<height; i++) {
		stream_copy(dst, src, bytesPerLine);
		src=(char *)src+ srcStride;
		dst=(char *)dst+ dstStride;
	    }
	}

	return retval;
    }

    inline any_t* memcpy_pic(any_t* dst, const any_t* src, int bytesPerLine, int height, int dstStride, int srcStride)
    {
	int i;
	any_t*retval=dst;

	if(dstStride == srcStride) memcpy(dst, src, srcStride*height);
	else {
	    for(i=0; i<height; i++) {
		memcpy(dst, src, bytesPerLine);
		src=(char *)src+ srcStride;
		dst=(char *)dst+ dstStride;
	    }
	}
	return retval;
    }
} // namespace mpxp
#endif
