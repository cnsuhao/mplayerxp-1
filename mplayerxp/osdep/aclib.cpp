#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
#include <stdio.h>
#include <pthread.h>

#include "mplayerxp.h"
#include "osdep_msg.h"

#if defined(USE_FASTMEMCPY)
#include "fastmemcpy.h"
#include "osdep/cpudetect.h"

#define BLOCK_SIZE 4096
#define CONFUSION_FACTOR 0

#define PVECTOR_ACCEL_H "aclib_template.h"
#include "pvector/pvector_inc.h"

/*
  aclib - advanced C library ;)
  This file contains functions which improve and expand standard C-library
  see aclib_template.c ... this file only contains runtime cpu detection and config options stuff
  runtime cpu detection by michael niedermayer (michaelni@gmx.at) is under GPL
*/
namespace	usr {
static any_t* init_fast_memcpy(any_t* to, const any_t* from, size_t len)
{
#ifdef __SSE2__
    if(gCpuCaps.hasSSE2) {
	mpxp_v<<"Using SSE2 optimized memcpy"<<std::endl;
	fast_memcpy_ptr = fast_memcpy_SSE2;
    } else
#endif
#ifndef __x86_64__
#ifdef __SSE__
    if(gCpuCaps.hasMMX2) {
	mpxp_v<<"Using MMX2 optimized memcpy"<<std::endl;
	fast_memcpy_ptr = fast_memcpy_SSE;
    } else
#endif
//#ifdef __MMX__
//	if(gCpuCaps.hasMMX)
//	{
//		mpxp_v<<"Using MMX optimized memcpy"<<std::endl;
//		fast_memcpy_ptr = fast_memcpy_MMX;
//	}
//	else
//#endif
#endif
    {
	mpxp_v<<"Using generic memcpy"<<std::endl;
	fast_memcpy_ptr = memcpy; /* prior to mmx we use the standart memcpy */
    }
    return (*fast_memcpy_ptr)(to,from,len);
}

static any_t* init_stream_copy(any_t* to, const any_t* from, size_t len)
{
#ifdef __SSE2__
    if(gCpuCaps.hasSSE2) {
	mpxp_v<<"Using SSE2 optimized agpcpy"<<std::endl;
	fast_stream_copy_ptr = fast_stream_copy_SSE2;
    }
#endif
#ifndef __x86_64__
#ifdef __SSE__
    if(gCpuCaps.hasMMX2) {
	mpxp_v<<"Using MMX2 optimized agpcpy"<<std::endl;
	fast_stream_copy_ptr = fast_stream_copy_SSE;
    } else
#endif
//#ifdef __MMX__
//	if(gCpuCaps.hasMMX)
//	{
//		mpxp_v<<"Using MMX optimized agpcpy"<<std::endl;
//		fast_stream_copy_ptr = fast_stream_copy_MMX;
//	}
//	else
//#endif
#endif
    {
	mpxp_v<<"Using generic optimized agpcpy"<<std::endl;
	fast_stream_copy_ptr = ::memcpy; /* prior to mmx we use the standart memcpy */
    }
    return (*fast_stream_copy_ptr)(to,from,len);
}

any_t*(*fast_memcpy_ptr)(any_t* to, const any_t* from, size_t len) = init_fast_memcpy;
any_t*(*fast_stream_copy_ptr)(any_t* to, const any_t* from, size_t len) = init_stream_copy;
} // namespace	usr
#endif /* use fastmemcpy */

