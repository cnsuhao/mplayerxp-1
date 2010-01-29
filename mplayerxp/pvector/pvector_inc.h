#ifndef __PVECTOR_INC_H
#define __PVECTOR_INC_H

#ifndef PVECTOR_ACCEL_H
#error "You should define PVECTOR_ACCEL_H before including of this file"
#endif

#undef OPTIMIZE_AVX
#undef OPTIMIZE_SSE4
#undef OPTIMIZE_SSSE3
#undef OPTIMIZE_SSE3
#undef OPTIMIZE_SSE2
#undef OPTIMIZE_SSE
#undef OPTIMIZE_MMX2
#undef OPTIMIZE_MMX
#undef OPTIMIZE_3DNOW
#define PVECTOR_RENAME(a) a ## _c
#include PVECTOR_ACCEL_H

#if !defined( __x86_64__ ) || defined(PVECTOR_TESTING)
#if defined(COMPILE_FOR_OLD_PC) || defined(PVECTOR_TESTING)
#ifdef __MMX__
#define OPTIMIZE_MMX
#undef PVECTOR_RENAME
#define PVECTOR_RENAME(a) a ## _MMX
#include PVECTOR_ACCEL_H
#endif
#endif // PVECTOR_TESTING

#ifdef __3dNOW__
#define OPTIMIZE_3DNOW
#define OPTIMIZE_MMX2
#undef PVECTOR_RENAME
#define PVECTOR_RENAME(a) a ## _3DNOW
#include PVECTOR_ACCEL_H
#endif

#ifdef __SSE__
#define OPTIMIZE_SSE
#define OPTIMIZE_MMX2
#undef PVECTOR_RENAME
#define PVECTOR_RENAME(a) a ## _SSE
#include PVECTOR_ACCEL_H
#endif
#endif //__x86_64__

#ifdef __SSE2__
#define OPTIMIZE_SSE2
#undef PVECTOR_RENAME
#define PVECTOR_RENAME(a) a ## _SSE2
#include PVECTOR_ACCEL_H
#endif

#ifdef __SSE3__
#define OPTIMIZE_SSE3
#undef PVECTOR_RENAME
#define PVECTOR_RENAME(a) a ## _SSE3
#include PVECTOR_ACCEL_H
#endif

#ifdef __SSSE3__
#define OPTIMIZE_SSSE3
#undef PVECTOR_RENAME
#define PVECTOR_RENAME(a) a ## _SSSE3
#include PVECTOR_ACCEL_H
#endif

#ifdef __SSE4_1__
#define OPTIMIZE_SSE4
#undef PVECTOR_RENAME
#define PVECTOR_RENAME(a) a ## _SSE4
#include PVECTOR_ACCEL_H
#endif

#ifdef __AVX__
#define OPTIMIZE_AVX
#undef PVECTOR_RENAME
#define PVECTOR_RENAME(a) a ## _AVX
#include PVECTOR_ACCEL_H
#endif

#endif /* PVECTOR_INC_H */
