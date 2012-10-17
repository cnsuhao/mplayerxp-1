/*
  pvector_f32_x86.h
*/

#if defined( OPTIMIZE_AVX )
#include <immintrin.h>
#elif defined(OPTIMIZE_AES)
#include <wmmintrin.h>
#elif defined (OPTIMIZE_SSE4)
#include <smmintrin.h>
#elif defined(OPTIMIZE_SSSE3)
#include <tmmintrin.h>
#elif defined(OPTIMIZE_SSE3)
#include <pmmintrin.h>
#elif defined(OPTIMIZE_SSE2)
#include <emmintrin.h>
#elif defined(OPTIMIZE_SSE)
#include <xmmintrin.h>
#else
#include <xmmintrin.h> // includes mm_*
#include <mm3dnow.h>
#endif

#undef __F32VEC_SIZE
#undef __f32vec
#ifdef OPTIMIZE_AVX
#define __F32VEC_SIZE	32
#define __f32vec	__m256
#elif defined( OPTIMIZE_SSE )
#define __F32VEC_SIZE	16
#define __f32vec	__m128
#else
#define __F32VEC_SIZE	8
#define __f32vec	__m64
#endif

extern __inline __f32vec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(f32_loadu)(float const *__P)
{
#ifdef OPTIMIZE_AVX
    return _mm256_loadu_ps(__P);
#elif defined( OPTIMIZE_SSE )
    return _mm_loadu_ps(__P);
#else
    return *(__f32vec const *)__P;
#endif
}
#undef _f32vec_loadu
#define _f32vec_loadu PVECTOR_RENAME(f32_loadu)

extern __inline __f32vec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(f32_loada)(float const *__P)
{
#ifdef OPTIMIZE_AVX
    return _mm256_load_ps(__P);
#elif defined( OPTIMIZE_SSE )
    return _mm_load_ps(__P);
#else
    return *(__m64 const *)__P;
#endif
}
#undef _f32vec_loada
#define _f32vec_loada PVECTOR_RENAME(f32_loada)

extern __inline void __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(f32_storeu)(float *__P, __f32vec src)
{
#ifdef OPTIMIZE_AVX
    _mm256_storeu_ps(__P,src);
#elif defined( OPTIMIZE_SSE )
    _mm_storeu_ps(__P,src);
#else
    *(__m64 *)__P = src;
#endif
}
#undef _f32vec_storeu
#define _f32vec_storeu PVECTOR_RENAME(f32_storeu)

extern __inline void __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(f32_storea)(float *__P, __f32vec src)
{
#ifdef OPTIMIZE_AVX
    _mm256_store_ps(__P,src);
#elif defined( OPTIMIZE_SSE )
    _mm_store_ps(__P,src);
#else
    *(__m64 *)__P = src;
#endif
}
#undef _f32vec_storea
#define _f32vec_storea PVECTOR_RENAME(f32_storea)

extern __inline void __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(f32_stream)(float *__P, __f32vec src)
{
#ifdef OPTIMIZE_AVX
    _mm256_stream_ps(__P,src);
#elif defined( OPTIMIZE_SSE )
    _mm_stream_ps(__P,src);
#else
    _mm_stream_pi((__m64 *)__P,(__m64)src);
#endif
}
#undef _f32vec_stream
#define _f32vec_stream PVECTOR_RENAME(f32_stream)

extern __inline __f32vec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(f32_setzero)(void)
{
#ifdef OPTIMIZE_AVX
    return _mm256_setzero_ps();
#elif defined( OPTIMIZE_SSE )
    return _mm_setzero_ps();
#else
    return _mm_setzero_si64();
#endif
}
#undef _f32vec_setzero
#define _f32vec_setzero PVECTOR_RENAME(f32_setzero)

extern __inline __f32vec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(f32_broadcast)(float f32)
{
#ifdef OPTIMIZE_AVX
    return _mm256_set1_ps(f32);
#elif defined( OPTIMIZE_SSE )
    return _mm_set1_ps(f32);
#else
    return (__m64)(__v2sf){ f32, f32 };
#endif
}
#undef _f32vec_broadcast
#define _f32vec_broadcast PVECTOR_RENAME(f32_broadcast)

extern __inline __f32vec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(f32_from_s32u)(void const * src)
{
#ifdef OPTIMIZE_AVX
    return _mm256_cvtepi32_ps(_mm256_loadu_si256(src));
#elif defined( OPTIMIZE_SSE2 )
    return _mm_cvtepi32_ps(_mm_loadu_si128(src));
#elif defined( OPTIMIZE_SSE )
    __m128 tmp=_mm_setzero_ps();
    tmp = _mm_cvtpi32_ps(tmp,*(__m64 const *)((char const *)src+8));
    tmp = _mm_movelh_ps (tmp, tmp);
    return _mm_cvtpi32_ps(tmp,*(__m64 const *)src);
#else
    return _m_pi2fd(*(__m64 const *)src);
#endif
}
#undef _f32vec_from_s32u
#define _f32vec_from_s32u PVECTOR_RENAME(f32_from_s32u)

extern __inline __f32vec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(f32_from_s32a)(void const * src)
{
#ifdef OPTIMIZE_AVX
    return _mm256_cvtepi32_ps(_mm256_load_si256(src));
#elif defined( OPTIMIZE_SSE2 )
    return _mm_cvtepi32_ps(_mm_load_si128(src));
#elif defined( OPTIMIZE_SSE )
    __m128 tmp=_mm_setzero_ps();
    tmp = _mm_cvtpi32_ps(tmp,*(__m64 const *)((char const *)src+8));
    tmp = _mm_movelh_ps (tmp, tmp);
    return _mm_cvtpi32_ps(tmp,*(__m64 const *)src);
#else
    return _m_pi2fd(*(__m64 const *)src);
#endif
}
#undef _f32vec_from_s32a
#define _f32vec_from_s32a PVECTOR_RENAME(f32_from_s32a)

extern __inline void __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(f32_to_s32u)(any_t*dst,__f32vec src)
{
#ifdef OPTIMIZE_AVX
    _mm256_storeu_si256(dst,_mm256_cvtps_epi32(src));
#elif defined( OPTIMIZE_SSE2 )
    _mm_storeu_si128(dst,_mm_cvtps_epi32(src));
#elif defined( OPTIMIZE_SSE )
    __m128 tmp;
    *(__m64 *)dst = _mm_cvtps_pi32(src);
    tmp = _mm_movehl_ps (tmp, src);
    *(__m64 *)((char *)dst+8) = _mm_cvtps_pi32(tmp);
#else
    *(__m64 *)dst = _m_pf2id(src);
#endif
}
#undef _f32vec_to_s32u
#define _f32vec_to_s32u PVECTOR_RENAME(f32_to_s32u)


extern __inline void __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(f32_to_s32a)(any_t*dst,__f32vec src)
{
#ifdef OPTIMIZE_AVX
    _mm256_store_si256(dst,_mm256_cvtps_epi32(src));
#elif defined( OPTIMIZE_SSE2 )
    _mm_store_si128(dst,_mm_cvtps_epi32(src));
#elif defined( OPTIMIZE_SSE )
    __m128 tmp=_mm_setzero_ps();
    *(__m64 *)dst = _mm_cvtps_pi32(src);
    tmp = _mm_movehl_ps (tmp, src);
    *(__m64 *)((char *)dst+8) = _mm_cvtps_pi32(tmp);
#else
    *(__m64 *)dst = _m_pf2id(src);
#endif
}
#undef _f32vec_to_s32a
#define _f32vec_to_s32a PVECTOR_RENAME(f32_to_s32a)

extern __inline void __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(f32_to_s32_stream)(any_t*dst,__f32vec src)
{
#ifdef OPTIMIZE_AVX
    _mm256_stream_si256(dst,_mm256_cvtps_epi32(src));
#elif defined( OPTIMIZE_SSE2 )
    _mm_stream_si128(dst,_mm_cvtps_epi32(src));
#elif defined( OPTIMIZE_SSE )
    __m128 tmp=_mm_setzero_ps();
    _mm_stream_pi(dst,_mm_cvtps_pi32(src));
    tmp = _mm_movehl_ps (tmp, src);
    _mm_stream_pi(&((char *)dst)[8],_mm_cvtps_pi32(tmp));
#else
    _mm_stream_pi(dst,_m_pf2id(src));
#endif
}
#undef _f32vec_to_s32_stream
#define _f32vec_to_s32_stream PVECTOR_RENAME(f32_to_s32_stream)

/* ARITHMETICS */

extern __inline __f32vec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(f32_add)(__f32vec f1,__f32vec f2)
{
#ifdef OPTIMIZE_AVX
    return _mm256_add_ps(f1,f2);
#elif defined( OPTIMIZE_SSE )
    return _mm_add_ps(f1,f2);
#else
    return _m_pfadd(f1,f2);
#endif
}
#undef _f32vec_add
#define _f32vec_add PVECTOR_RENAME(f32_add)

extern __inline __f32vec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(f32_sub)(__f32vec f1,__f32vec f2)
{
#ifdef OPTIMIZE_AVX
    return _mm256_sub_ps(f1,f2);
#elif defined( OPTIMIZE_SSE )
    return _mm_sub_ps(f1,f2);
#else
    return _m_pfsub(f1,f2);
#endif
}
#undef _f32vec_sub
#define _f32vec_sub PVECTOR_RENAME(f32_sub)

extern __inline __f32vec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(f32_mul)(__f32vec f1,__f32vec f2)
{
#ifdef OPTIMIZE_AVX
    return _mm256_mul_ps(f1,f2);
#elif defined( OPTIMIZE_SSE )
    return _mm_mul_ps(f1,f2);
#else
    return _m_pfmul(f1,f2);
#endif
}
#undef _f32vec_mul
#define _f32vec_mul PVECTOR_RENAME(f32_mul)

extern __inline __f32vec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(f32_div)(__f32vec f1,__f32vec f2)
{
#ifdef OPTIMIZE_AVX
    return _mm256_div_ps(f1,f2);
#elif defined( OPTIMIZE_SSE )
    return _mm_div_ps(f1,f2);
#else
    __m64 tmp;
    tmp = _m_pfrcp(f2);
    tmp = _m_pfrcpit1(tmp,f2);
    tmp = _m_pfrcpit2(tmp,f2);
    return _m_pfmul(f1,tmp);
#endif
}
#undef _f32vec_div
#define _f32vec_div PVECTOR_RENAME(f32_div)

extern __inline __f32vec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(f32_max)(__f32vec f1,__f32vec f2)
{
#ifdef OPTIMIZE_AVX
    return _mm256_max_ps(f1,f2);
#elif defined( OPTIMIZE_SSE )
    return _mm_max_ps(f1,f2);
#else
    return _m_pfmax(f1,f2);
#endif
}
#undef _f32vec_max
#define _f32vec_max PVECTOR_RENAME(f32_max)

extern __inline __f32vec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(f32_min)(__f32vec f1,__f32vec f2)
{
#ifdef OPTIMIZE_AVX
    return _mm256_min_ps(f1,f2);
#elif defined( OPTIMIZE_SSE )
    return _mm_min_ps(f1,f2);
#else
    return _m_pfmin(f1,f2);
#endif
}
#undef _f32vec_min
#define _f32vec_min PVECTOR_RENAME(f32_min)

extern __inline __f32vec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(f32_clamp)(__f32vec f1,__f32vec minval,__f32vec maxval)
{
    return _f32vec_max(_f32vec_min(f1,maxval),minval);
}
#undef _f32vec_clamp
#define _f32vec_clamp PVECTOR_RENAME(f32_clamp)

