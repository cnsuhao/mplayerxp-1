#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
#include <stdio.h>
#include <pthread.h>

#include "mplayerxp.h"
#define MSGT_CLASS MSGT_GLOBAL
#include "mp_msg.h"

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
namespace mpxp {
static any_t* init_fast_memcpy(any_t* to, const any_t* from, size_t len)
{
#ifdef __SSE2__
	if(gCpuCaps.hasSSE2)
	{
		MSG_V("Using SSE2 optimized memcpy\n");
		fast_memcpy_ptr = fast_memcpy_SSE2;
	}
	else
#endif
#ifndef __x86_64__
#ifdef __SSE__
	if(gCpuCaps.hasMMX2)
	{
		MSG_V("Using MMX2 optimized memcpy\n");
		fast_memcpy_ptr = fast_memcpy_SSE;
	}
	else
#endif
//#ifdef __MMX__
//	if(gCpuCaps.hasMMX)
//	{
//		MSG_V("Using MMX optimized memcpy\n");
//		fast_memcpy_ptr = fast_memcpy_MMX;
//	}
//	else
//#endif
#endif
	{
		MSG_V("Using generic memcpy\n");
		fast_memcpy_ptr = memcpy; /* prior to mmx we use the standart memcpy */
	}
	return (*fast_memcpy_ptr)(to,from,len);
}

static any_t* init_stream_copy(any_t* to, const any_t* from, size_t len)
{
#ifdef __SSE2__
	if(gCpuCaps.hasSSE2)
	{
		MSG_V("Using SSE2 optimized agpcpy\n");
		fast_stream_copy_ptr = fast_stream_copy_SSE2;
	}
#endif
#ifndef __x86_64__
#ifdef __SSE__
	if(gCpuCaps.hasMMX2)
	{
		MSG_V("Using MMX2 optimized agpcpy\n");
		fast_stream_copy_ptr = fast_stream_copy_SSE;
	}
	else
#endif
//#ifdef __MMX__
//	if(gCpuCaps.hasMMX)
//	{
//		MSG_V("Using MMX optimized agpcpy\n");
//		fast_stream_copy_ptr = fast_stream_copy_MMX;
//	}
//	else
//#endif
#endif
	{
		MSG_V("Using generic optimized agpcpy\n");
		fast_stream_copy_ptr = ::memcpy; /* prior to mmx we use the standart memcpy */
	}
	return (*fast_stream_copy_ptr)(to,from,len);
}

any_t*(*fast_memcpy_ptr)(any_t* to, const any_t* from, size_t len) = init_fast_memcpy;
any_t*(*fast_stream_copy_ptr)(any_t* to, const any_t* from, size_t len) = init_stream_copy;
} // namespace mpxp
#endif /* use fastmemcpy */

