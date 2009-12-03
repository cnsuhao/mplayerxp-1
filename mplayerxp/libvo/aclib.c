#include <stdio.h>
#include <pthread.h>
#include "../config.h"
#include "../mplayer.h"
#define MSGT_CLASS MSGT_GLOBAL
#include "../__mp_msg.h"

#if defined(USE_FASTMEMCPY)
#include "fastmemcpy.h"
#include "../cpudetect.h"
/*
  aclib - advanced C library ;)
  This file contains functions which improve and expand standard C-library
  see aclib_template.c ... this file only contains runtime cpu detection and config options stuff
  runtime cpu detection by michael niedermayer (michaelni@gmx.at) is under GPL
*/
#if defined( CAN_COMPILE_MMX ) && defined (ARCH_X86)

#define BLOCK_SIZE 4096
#define CONFUSION_FACTOR 0
//Feel free to fine-tune the above 2, it might be possible to get some speedup with them :)

//#define STATISTICS

#if defined( ARCH_X86 )
#define CAN_COMPILE_X86_ASM
#endif

//Note: we have MMX, MMX2, 3DNOW version there is no 3DNOW+MMX2 one
//Plain C versions
//#if !defined (HAVE_MMX) || defined (RUNTIME_CPUDETECT)
//#define COMPILE_C
//#endif

#ifdef CAN_COMPILE_X86_ASM

#undef HAVE_MMX
#undef HAVE_MMX2
#undef HAVE_3DNOW
#undef HAVE_SSE

//MMX versions
#ifdef CAN_COMPILE_MMX
#undef RENAME
#undef CL_SIZE
#define CL_SIZE 32
#define HAVE_MMX
#undef HAVE_MMX2
#undef HAVE_3DNOW
#define RENAME(a) a ## _MMX
#include "aclib_template.c"
#endif

//MMX2 versions 32-byte cache-line size
#ifdef CAN_COMPILE_MMX2
#undef RENAME
#undef CL_SIZE
#define CL_SIZE 32
#define HAVE_MMX
#define HAVE_MMX2
#undef HAVE_3DNOW
#define RENAME(a) a ## _MMX2_CL32
#include "aclib_template.c"
#endif

//MMX2 versions 64-byte cache-line size
#ifdef CAN_COMPILE_MMX2
#undef RENAME
#undef CL_SIZE
#define CL_SIZE 64
#define HAVE_MMX
#define HAVE_MMX2
#undef HAVE_3DNOW
#define RENAME(a) a ## _MMX2_CL64
#include "aclib_template.c"
#endif

//MMX2 versions 128-byte cache-line size
#ifdef CAN_COMPILE_MMX2
#undef RENAME
#undef CL_SIZE
#define CL_SIZE 128
#define HAVE_MMX
#define HAVE_MMX2
#undef HAVE_3DNOW
#define RENAME(a) a ## _MMX2_CL128
#include "aclib_template.c"
#endif

//3DNOW versions all K6 have 32-bit cache-line size
#ifdef CAN_COMPILE_3DNOW
#undef RENAME
#undef CL_SIZE
#define CL_SIZE 32
#define HAVE_MMX
#undef HAVE_MMX2
#define HAVE_3DNOW
#define RENAME(a) a ## _3DNow
#include "aclib_template.c"
#endif
#endif // CAN_COMPILE_X86_ASM

#elif defined( ARCH_X86_64 )
#define RENAME(a) a ## _x86_64
#include "aclib_x86_64.h"
#endif

static void * init_fast_memcpy(void * to, const void * from, size_t len)
{
#if defined( ARCH_X86_64 ) && defined( USE_FASTMEMCPY )
	fast_memcpy_ptr = fast_memcpy_x86_64;
#elif defined( CAN_COMPILE_X86_ASM )
	// ordered per speed fasterst first
#ifdef CAN_COMPILE_MMX2
	if(gCpuCaps.hasMMX2)
	{
		MSG_V("Using MMX2 optimized memcpy\n");
		if(gCpuCaps.cl_size >= 128) fast_memcpy_ptr = fast_memcpy_MMX2_CL128;
		else
		if(gCpuCaps.cl_size == 64) fast_memcpy_ptr = fast_memcpy_MMX2_CL64;
		else
		fast_memcpy_ptr = fast_memcpy_MMX2_CL32;
	}
	else
#endif
#ifdef CAN_COMPILE_3DNOW
	if(gCpuCaps.has3DNow)
	{
		MSG_V("Using 3DNow optimized memcpy\n");
		fast_memcpy_ptr = fast_memcpy_3DNow;
	}
	else
#endif
#ifdef CAN_COMPILE_MMX
	if(gCpuCaps.hasMMX)
	{
		MSG_V("Using MMX optimized memcpy\n");
		fast_memcpy_ptr = fast_memcpy_MMX;
	}
	else
#endif
#else
	{
		MSG_V("Using generic memcpy\n");
		fast_memcpy_ptr = memcpy; /* prior to mmx we use the standart memcpy */
	}
#endif
	return (*fast_memcpy_ptr)(to,from,len);
}

static void * init_mem2agpcpy(void * to, const void * from, size_t len)
{
#if defined( ARCH_X86_64 ) && defined( USE_FASTMEMCPY )
	mem2agpcpy_ptr = mem2agpcpy_x86_64;
#elif defined ( CAN_COMPILE_X86_ASM )
	// ordered per speed fasterst first
#ifdef CAN_COMPILE_MMX2
	if(gCpuCaps.hasMMX2)
	{
		MSG_V("Using MMX2 optimized agpcpy\n");
		if(gCpuCaps.cl_size >= 128) mem2agpcpy_ptr = mem2agpcpy_MMX2_CL128;
		else
		if(gCpuCaps.cl_size == 64) mem2agpcpy_ptr = mem2agpcpy_MMX2_CL64;
		else
		mem2agpcpy_ptr = mem2agpcpy_MMX2_CL32;
	}
	else
#endif
#ifdef CAN_COMPILE_3DNOW
	if(gCpuCaps.has3DNow)
	{
		MSG_V("Using 3DNow optimized agpcpy\n");
		mem2agpcpy_ptr = mem2agpcpy_3DNow;
	}
	else
#endif
#ifdef CAN_COMPILE_MMX
	if(gCpuCaps.hasMMX)
	{
		MSG_V("Using MMX optimized agpcpy\n");
		mem2agpcpy_ptr = mem2agpcpy_MMX;
	}
	else
#endif
#else
	{
		MSG_V("Using generic optimized agpcpy\n");
		mem2agpcpy_ptr = memcpy; /* prior to mmx we use the standart memcpy */
	}
#endif
	return (*mem2agpcpy_ptr)(to,from,len);
}

void *(*fast_memcpy_ptr)(void * to, const void * from, size_t len) = init_fast_memcpy;
void *(*mem2agpcpy_ptr)(void * to, const void * from, size_t len) = init_mem2agpcpy;

#endif /* use fastmemcpy */

