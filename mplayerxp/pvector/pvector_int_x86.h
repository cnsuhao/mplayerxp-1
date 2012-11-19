/*
  pvector_int_x86.h
*/

#if defined( OPTIMIZE_AVX )
#define _VEC(a) a ## _AVX
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
#elif defined(OPTIMIZE_MMX2)
#include <xmmintrin.h>
#ifdef OPTIMIZE_3DNOW
#include <mm3dnow.h>
#endif
#else
#include <mmintrin.h>
#endif

#undef __ivec
#ifdef OPTIMIZE_SSE2
#define __ivec		__m128i
#else
#define __ivec		__m64
#endif

static __inline unsigned __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(ivec_size)()
{
#ifdef OPTIMIZE_AVX
    return 32;
#elif defined(OPTIMIZE_SSE2)
    return 16;
#else
    return 8;
#endif
}
#undef _ivec_size
#define _ivec_size PVECTOR_RENAME(ivec_size)

static __inline int __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(ivec_aligned)(const any_t* p) { return (((long)p)&(_ivec_size()-1))==0; }
#undef _ivec_aligned
#define _ivec_aligned PVECTOR_RENAME(ivec_aligned)

extern __inline void __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(empty)(void)
{
#if defined( OPTIMIZE_SSE2 ) || defined( OPTIMIZE_SSE )
#elif defined( OPTIMIZE_3DNOW )
    _m_femms();
#else
    _mm_empty();
#endif
}
#undef _ivec_empty
#define _ivec_empty PVECTOR_RENAME(empty)

extern __inline void __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(sfence)(void)
{
#ifdef OPTIMIZE_MMX2
    _mm_sfence();
#endif
}
#undef _ivec_sfence
#define _ivec_sfence PVECTOR_RENAME(sfence)

extern __inline void __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(prefetch)(any_t const *__P)
{
#ifdef OPTIMIZE_MMX2
    _mm_prefetch(__P, _MM_HINT_T0);
#endif
}
#undef _ivec_prefetch
#define _ivec_prefetch PVECTOR_RENAME(prefetch)

extern __inline void __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(prefetchw)(any_t const *__P)
{
#ifdef OPTIMIZE_MMX2
    _mm_prefetch(__P, _MM_HINT_NTA);
#endif
}
#undef _ivec_prefetchw
#define _ivec_prefetchw PVECTOR_RENAME(prefetchw)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(loadu)(any_t const *__P)
{
#ifdef OPTIMIZE_SSE3
    return _mm_lddqu_si128((__m128i*)__P);
#elif defined OPTIMIZE_SSE2
    return (__ivec)_mm_loadu_si128((__m128i*)__P);
#else
    return *(__ivec const *)__P;
#endif
}
#undef _ivec_loadu
#define _ivec_loadu PVECTOR_RENAME(loadu)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(loada)(any_t const *__P)
{
#ifdef OPTIMIZE_SSE2
    return (__ivec)_mm_load_si128((__m128i*)__P);
#else
    return *(__ivec const *)__P;
#endif
}
#undef _ivec_loada
#define _ivec_loada PVECTOR_RENAME(loada)

extern __inline void __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(storeu)(any_t*__P, __ivec src)
{
#ifdef OPTIMIZE_SSE2
    _mm_storeu_si128((__m128i*)__P,src);
#else
    *(__ivec *)__P = src;
#endif
}
#undef _ivec_storeu
#define _ivec_storeu PVECTOR_RENAME(storeu)

extern __inline void __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(storea)(any_t*__P, __ivec src)
{
#ifdef OPTIMIZE_SSE2
    _mm_store_si128((__m128i*)__P,src);
#else
    *(__ivec *)__P = src;
#endif
}
#undef _ivec_storea
#define _ivec_storea PVECTOR_RENAME(storea)

extern __inline void __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(stream)(any_t*__P, __ivec src)
{
#ifdef OPTIMIZE_SSE2
    _mm_stream_si128((__m128i*)__P,src);
#elif defined( OPTIMIZE_MMX2 )
    _mm_stream_pi((__m64*)__P,src);
#else
    *(__ivec *)__P = src;
#endif
}
#undef _ivec_stream
#define _ivec_stream PVECTOR_RENAME(stream)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(setzero)(void)
{
#ifdef OPTIMIZE_SSE2
    return (__ivec)_mm_setzero_si128();
#else
    return _mm_setzero_si64();
#endif
}
#undef _ivec_setzero
#define _ivec_setzero PVECTOR_RENAME(setzero)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(setff)(void)
{
#ifdef OPTIMIZE_SSE2
    return _mm_set1_epi8(0xFF);
#else
    return _mm_set1_pi8(0xFF);
#endif
}
#undef _ivec_setff
#define _ivec_setff PVECTOR_RENAME(setff)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(broadcast_u8)(unsigned char u8)
{
#ifdef OPTIMIZE_SSE2
    return _mm_set1_epi8(u8);
#else
    return _mm_set1_pi8(u8);
#endif
}
#undef _ivec_broadcast_u8
#define _ivec_broadcast_u8 PVECTOR_RENAME(broadcast_u8)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(broadcast_u16)(unsigned short u16)
{
#ifdef OPTIMIZE_SSE2
    return _mm_set1_epi16(u16);
#else
    return _mm_set1_pi16(u16);
#endif
}
#undef _ivec_broadcast_u16
#define _ivec_broadcast_u16 PVECTOR_RENAME(broadcast_u16)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(broadcast_u32)(unsigned int u32)
{
#ifdef OPTIMIZE_SSE2
    return _mm_set1_epi32(u32);
#else
    return _mm_set1_pi32(u32);
#endif
}
#undef _ivec_broadcast_u32
#define _ivec_broadcast_u32 PVECTOR_RENAME(broadcast_u32)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(or)(__ivec __m1, __ivec __m2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_or_si128(__m1,__m2);
#else
    return _mm_or_si64(__m1,__m2);
#endif
}
#undef _ivec_or
#define _ivec_or PVECTOR_RENAME(or)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(and)(__ivec __m1, __ivec __m2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_and_si128(__m1,__m2);
#else
    return _mm_and_si64(__m1,__m2);
#endif
}
#undef _ivec_and
#define _ivec_and PVECTOR_RENAME(and)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(andnot)(__ivec __m1, __ivec __m2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_andnot_si128(__m1,__m2);
#else
    return _mm_andnot_si64(__m1,__m2);
#endif
}
#undef _ivec_andnot
#define _ivec_andnot PVECTOR_RENAME(andnot)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(xor)(__ivec __m1, __ivec __m2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_xor_si128(__m1,__m2);
#else
    return _mm_xor_si64(__m1,__m2);
#endif
}
#undef _ivec_xor
#define _ivec_xor PVECTOR_RENAME(xor)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(not)(__ivec __m)
{
    return _ivec_xor(__m,_ivec_setff());
}
#undef _ivec_not
#define _ivec_not PVECTOR_RENAME(not)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(cmpgt_s8)(__ivec __m1, __ivec __m2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_cmpgt_epi8(__m1,__m2);
#else
    return _mm_cmpgt_pi8(__m1,__m2);
#endif
}
#undef _ivec_cmpgt_s8
#define _ivec_cmpgt_s8 PVECTOR_RENAME(cmpgt_s8)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(cmpeq_s8)(__ivec __m1, __ivec __m2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_cmpeq_epi8(__m1,__m2);
#else
    return _mm_cmpeq_pi8(__m1,__m2);
#endif
}
#undef _ivec_cmpeq_s8
#define _ivec_cmpeq_s8 PVECTOR_RENAME(cmpeq_s8)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(cmpgt_s16)(__ivec __m1, __ivec __m2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_cmpgt_epi16(__m1,__m2);
#else
    return _mm_cmpgt_pi16(__m1,__m2);
#endif
}
#undef _ivec_cmpgt_s16
#define _ivec_cmpgt_s16 PVECTOR_RENAME(cmpgt_s16)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(cmpeq_s16)(__ivec __m1, __ivec __m2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_cmpeq_epi16(__m1,__m2);
#else
    return _mm_cmpeq_pi16(__m1,__m2);
#endif
}
#undef _ivec_cmpeq_s16
#define _ivec_cmpeq_s16 PVECTOR_RENAME(cmpeq_s16)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(cmpgt_s32)(__ivec __m1, __ivec __m2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_cmpgt_epi32(__m1,__m2);
#else
    return _mm_cmpgt_pi32(__m1,__m2);
#endif
}
#undef _ivec_cmpgt_s32
#define _ivec_cmpgt_s32 PVECTOR_RENAME(cmpgt_s32)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(cmpeq_s32)(__ivec __m1, __ivec __m2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_cmpeq_epi32(__m1,__m2);
#else
    return _mm_cmpeq_pi32(__m1,__m2);
#endif
}
#undef _ivec_cmpeq_s32
#define _ivec_cmpeq_s32 PVECTOR_RENAME(cmpeq_s32)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(blend_u8)(__ivec src1,__ivec src2,__ivec mask)
{
#ifdef OPTIMIZE_SSE4
    return _mm_blendv_epi8(src1,src2,mask);
#else
/*
   Note: maskmovq is slowest instruction on both: mmx and sse engines.
   The programs coded with emulation of this instruction runs faster in 13-15 times!
   (Tested on AMD K10 cpu) Nickols_K.
#ifdef OPTIMIZE_SSE2
    _mm_maskmoveu_si128(src,mask,dest);
#elif defined(OPTIMIZE_MMX2)
    _mm_maskmove_si64(src,mask,dest);
#else
*/
    return _ivec_or(_ivec_andnot(mask,src1),_ivec_and(mask,src2));
#endif
}
#undef _ivec_blend_u8
#define _ivec_blend_u8 PVECTOR_RENAME(blend_u8)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(interleave_lo_u8)(__ivec s1, __ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_unpacklo_epi8(s1,s2);
#else
    return _mm_unpacklo_pi8(s1,s2);
#endif
}
#undef _ivec_interleave_lo_u8
#define _ivec_interleave_lo_u8 PVECTOR_RENAME(interleave_lo_u8)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(interleave_hi_u8)(__ivec s1, __ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_unpackhi_epi8(s1,s2);
#else
    return _mm_unpackhi_pi8(s1,s2);
#endif
}
#undef _ivec_interleave_hi_u8
#define _ivec_interleave_hi_u8 PVECTOR_RENAME(interleave_hi_u8)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(interleave_lo_u16)(__ivec s1, __ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_unpacklo_epi16(s1,s2);
#else
    return _mm_unpacklo_pi16(s1,s2);
#endif
}
#undef _ivec_interleave_lo_u16
#define _ivec_interleave_lo_u16 PVECTOR_RENAME(interleave_lo_u16)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(interleave_hi_u16)(__ivec s1, __ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_unpackhi_epi16(s1,s2);
#else
    return _mm_unpackhi_pi16(s1,s2);
#endif
}
#undef _ivec_interleave_hi_u16
#define _ivec_interleave_hi_u16 PVECTOR_RENAME(interleave_hi_u16)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(interleave_lo_u32)(__ivec s1, __ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_unpacklo_epi32(s1,s2);
#else
    return _mm_unpacklo_pi32(s1,s2);
#endif
}
#undef _ivec_interleave_lo_u32
#define _ivec_interleave_lo_u32 PVECTOR_RENAME(interleave_lo_u32)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(interleave_hi_u32)(__ivec s1, __ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_unpackhi_epi32(s1,s2);
#else
    return _mm_unpackhi_pi32(s1,s2);
#endif
}
#undef _ivec_interleave_hi_u32
#define _ivec_interleave_hi_u32 PVECTOR_RENAME(interleave_hi_u32)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(u16_from_u8)(__ivec s,__ivec *hipart)
{
    __ivec filler = _ivec_setzero();
    *hipart = _ivec_interleave_hi_u8(s,filler);
    return    _ivec_interleave_lo_u8(s,filler);
}
#undef _ivec_u16_from_u8
#define _ivec_u16_from_u8 PVECTOR_RENAME(u16_from_u8)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(u32_from_u16)(__ivec s,__ivec *hipart)
{
    __ivec filler = _ivec_setzero();
    *hipart = _ivec_interleave_hi_u16(s,filler);
    return    _ivec_interleave_lo_u16(s,filler);
}
#undef _ivec_u32_from_u16
#define _ivec_u32_from_u16 PVECTOR_RENAME(u32_from_u16)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(s16_from_s8)(__ivec s,__ivec* hipart)
{
    const __ivec izero = _ivec_setzero();
    __ivec filler;
    filler = _ivec_cmpgt_s8(izero,s);
    *hipart = _ivec_interleave_hi_u8(s,filler);
    return _ivec_interleave_lo_u8(s,filler);
}
#undef _ivec_s16_from_s8
#define _ivec_s16_from_s8 PVECTOR_RENAME(s16_from_s8)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(s32_from_s16)(__ivec s,__ivec* hipart)
{
    const __ivec izero = _ivec_setzero();
    __ivec filler;
    filler = _ivec_cmpgt_s16(izero,s);
    *hipart = _ivec_interleave_hi_u16(s,filler);
    return _ivec_interleave_lo_u16(s,filler);
}
#undef _ivec_s32_from_s16
#define _ivec_s32_from_s16 PVECTOR_RENAME(s32_from_s16)


extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(s16_from_s32)(__ivec s1, __ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_packs_epi32(s1,s2);
#else
    return _mm_packs_pi32(s1,s2);
#endif
}
#undef _ivec_s16_from_s32
#define _ivec_s16_from_s32 PVECTOR_RENAME(s16_from_s32)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(s8_from_s16)(__ivec s1, __ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_packs_epi16(s1,s2);
#else
    return _mm_packs_pi16(s1,s2);
#endif
}
#undef _ivec_s8_from_s16
#define _ivec_s8_from_s16 PVECTOR_RENAME(s8_from_s16)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(u8_from_u16)(__ivec s1, __ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_packus_epi16(s1,s2);
#else
    return _mm_packs_pu16(s1,s2);
#endif
}
#undef _ivec_u8_from_u16
#define _ivec_u8_from_u16 PVECTOR_RENAME(u8_from_u16)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(add_s8)(__ivec s1,__ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_add_epi8(s1,s2);
#else
    return _mm_add_pi8(s1,s2);
#endif
}
#undef _ivec_add_s8
#define _ivec_add_s8 PVECTOR_RENAME(add_s8)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(add_s16)(__ivec s1,__ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_add_epi16(s1,s2);
#else
    return _mm_add_pi16(s1,s2);
#endif
}
#undef _ivec_add_s16
#define _ivec_add_s16 PVECTOR_RENAME(add_s16)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(add_s32)(__ivec s1,__ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_add_epi32(s1,s2);
#else
    return _mm_add_pi32(s1,s2);
#endif
}
#undef _ivec_add_s32
#define _ivec_add_s32 PVECTOR_RENAME(add_s32)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(sadd_s8)(__ivec s1,__ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_adds_epi8(s1,s2);
#else
    return _mm_adds_pi8(s1,s2);
#endif
}
#undef _ivec_sadd_s8
#define _ivec_sadd_s8 PVECTOR_RENAME(sadd_s8)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(sadd_s16)(__ivec s1,__ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_adds_epi16(s1,s2);
#else
    return _mm_adds_pi16(s1,s2);
#endif
}
#undef _ivec_sadd_s16
#define _ivec_sadd_s16 PVECTOR_RENAME(sadd_s16)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(sadd_u8)(__ivec s1,__ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_adds_epu8(s1,s2);
#else
    return _mm_adds_pu8(s1,s2);
#endif
}
#undef _ivec_sadd_u8
#define _ivec_sadd_u8 PVECTOR_RENAME(sadd_u8)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(sadd_u16)(__ivec s1,__ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_adds_epu16(s1,s2);
#else
    return _mm_adds_pu16(s1,s2);
#endif
}
#undef _ivec_sadd_u16
#define _ivec_sadd_u16 PVECTOR_RENAME(sadd_u16)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(sub_s8)(__ivec s1,__ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_sub_epi8(s1,s2);
#else
    return _mm_sub_pi8(s1,s2);
#endif
}
#undef _ivec_sub_s8
#define _ivec_sub_s8 PVECTOR_RENAME(sub_s8)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(sub_s16)(__ivec s1,__ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_sub_epi16(s1,s2);
#else
    return _mm_sub_pi16(s1,s2);
#endif
}
#undef _ivec_sub_s16
#define _ivec_sub_s16 PVECTOR_RENAME(sub_s16)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(sub_s32)(__ivec s1,__ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_sub_epi32(s1,s2);
#else
    return _mm_sub_pi32(s1,s2);
#endif
}
#undef _ivec_sub_s32
#define _ivec_sub_s32 PVECTOR_RENAME(sub_s32)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(ssub_s8)(__ivec s1,__ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_subs_epi8(s1,s2);
#else
    return _mm_subs_pi8(s1,s2);
#endif
}
#undef _ivec_ssub_s8
#define _ivec_ssub_s8 PVECTOR_RENAME(ssub_s8)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(ssub_s16)(__ivec s1,__ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_subs_epi16(s1,s2);
#else
    return _mm_subs_pi16(s1,s2);
#endif
}
#undef _ivec_ssub_s16
#define _ivec_ssub_s16 PVECTOR_RENAME(ssub_s16)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(ssub_u8)(__ivec s1,__ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_subs_epu8(s1,s2);
#else
    return _mm_subs_pu8(s1,s2);
#endif
}
#undef _ivec_ssub_u8
#define _ivec_ssub_u8 PVECTOR_RENAME(ssub_u8)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(ssub_u16)(__ivec s1,__ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_subs_epu16(s1,s2);
#else
    return _mm_subs_pu16(s1,s2);
#endif
}
#undef _ivec_ssub_u16
#define _ivec_ssub_u16 PVECTOR_RENAME(ssub_u16)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(mullo_s16)(__ivec s1,__ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_mullo_epi16(s1,s2);
#else
    return _mm_mullo_pi16(s1,s2);
#endif
}
#undef _ivec_mullo_s16
#define _ivec_mullo_s16 PVECTOR_RENAME(mullo_s16)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(mulhi_s16)(__ivec s1,__ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_mulhi_epi16(s1,s2);
#else
    return _mm_mulhi_pi16(s1,s2);
#endif
}
#undef _ivec_mulhi_s16
#define _ivec_mulhi_s16 PVECTOR_RENAME(mulhi_s16)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(sll_s16)(__ivec s1,__ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_sll_epi16(s1,s2);
#else
    return _mm_sll_pi16(s1,s2);
#endif
}
#undef _ivec_sll_s16
#define _ivec_sll_s16 PVECTOR_RENAME(sll_s16)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(sll_s16_imm)(__ivec s1,int c)
{
#ifdef OPTIMIZE_SSE2
    return _mm_slli_epi16(s1,c);
#else
    return _mm_slli_pi16(s1,c);
#endif
}
#undef _ivec_sll_s16_imm
#define _ivec_sll_s16_imm PVECTOR_RENAME(sll_s16_imm)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(sll_s32)(__ivec s1,__ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_sll_epi32(s1,s2);
#else
    return _mm_sll_pi32(s1,s2);
#endif
}
#undef _ivec_sll_s32
#define _ivec_sll_s32 PVECTOR_RENAME(sll_s32)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(sll_s32_imm)(__ivec s1,int c)
{
#ifdef OPTIMIZE_SSE2
    return _mm_slli_epi32(s1,c);
#else
    return _mm_slli_pi32(s1,c);
#endif
}
#undef _ivec_sll_s32_imm
#define _ivec_sll_s32_imm PVECTOR_RENAME(sll_s32_imm)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(sll_s64)(__ivec s1,__ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_sll_epi64(s1,s2);
#else
    return _mm_sll_si64(s1,s2);
#endif
}
#undef _ivec_sll_s64
#define _ivec_sll_s64 PVECTOR_RENAME(sll_s64)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(sll_s64_imm)(__ivec s1,int c)
{
#ifdef OPTIMIZE_SSE2
    return _mm_slli_epi64(s1,c);
#else
    return _mm_slli_si64(s1,c);
#endif
}
#undef _ivec_sll_s64_imm
#define _ivec_sll_s64_imm PVECTOR_RENAME(sll_s64_imm)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(sra_s16)(__ivec s1,__ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_sra_epi16(s1,s2);
#else
    return _mm_sra_pi16(s1,s2);
#endif
}
#undef _ivec_sra_s16
#define _ivec_sra_s16 PVECTOR_RENAME(sra_s16)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(sra_s16_imm)(__ivec s1,int c)
{
#ifdef OPTIMIZE_SSE2
    return _mm_srai_epi16(s1,c);
#else
    return _mm_srai_pi16(s1,c);
#endif
}
#undef _ivec_sra_s16_imm
#define _ivec_sra_s16_imm PVECTOR_RENAME(sra_s16_imm)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(sra_s32)(__ivec s1,__ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_sra_epi32(s1,s2);
#else
    return _mm_sra_pi32(s1,s2);
#endif
}
#undef _ivec_sra_s32
#define _ivec_sra_s32 PVECTOR_RENAME(sra_s32)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(sra_s32_imm)(__ivec s1,int c)
{
#ifdef OPTIMIZE_SSE2
    return _mm_srai_epi32(s1,c);
#else
    return _mm_srai_pi32(s1,c);
#endif
}
#undef _ivec_sra_s32_imm
#define _ivec_sra_s32_imm PVECTOR_RENAME(sra_s32_imm)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(srl_s16)(__ivec s1,__ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_srl_epi16(s1,s2);
#else
    return _mm_srl_pi16(s1,s2);
#endif
}
#undef _ivec_srl_s16
#define _ivec_srl_s16 PVECTOR_RENAME(srl_s16)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(srl_s16_imm)(__ivec s1,int c)
{
#ifdef OPTIMIZE_SSE2
    return _mm_srli_epi16(s1,c);
#else
    return _mm_srli_pi16(s1,c);
#endif
}
#undef _ivec_srl_s16_imm
#define _ivec_srl_s16_imm PVECTOR_RENAME(srl_s16_imm)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(srl_s32)(__ivec s1,__ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_srl_epi32(s1,s2);
#else
    return _mm_srl_pi32(s1,s2);
#endif
}
#undef _ivec_srl_s32
#define _ivec_srl_s32 PVECTOR_RENAME(srl_s32)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(srl_s32_imm)(__ivec s1,int c)
{
#ifdef OPTIMIZE_SSE2
    return _mm_srli_epi32(s1,c);
#else
    return _mm_srli_pi32(s1,c);
#endif
}
#undef _ivec_srl_s32_imm
#define _ivec_srl_s32_imm PVECTOR_RENAME(srl_s32_imm)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(srl_s64)(__ivec s1,__ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_srl_epi64(s1,s2);
#else
    return _mm_srl_si64(s1,s2);
#endif
}
#undef _ivec_srl_s64
#define _ivec_srl_s64 PVECTOR_RENAME(srl_s64)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(srl_s64_imm)(__ivec s1,int c)
{
#ifdef OPTIMIZE_SSE2
    return _mm_srli_epi64(s1,c);
#else
    return _mm_srli_si64(s1,c);
#endif
}
#undef _ivec_srl_s64_imm
#define _ivec_srl_s64_imm PVECTOR_RENAME(srl_s64_imm)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(scale_u16_from_u8)(__ivec s,__ivec *hipart)
{
#if 0 /* slower but portable on non-x86 CPUs version */
    __ivec tmp[2];
    tmp[0]  = _ivec_u16_from_u8(s,&tmp[1]);
    *hipart = _ivec_sll_s16_imm(tmp[1],8);
    return    _ivec_sll_s16_imm(tmp[0],8);
#else
    __ivec filler = _ivec_setzero();
    *hipart = _ivec_interleave_hi_u8(filler,s);
    return    _ivec_interleave_lo_u8(filler,s);
#endif
}
#undef _ivec_scale_u16_from_u8
#define _ivec_scale_u16_from_u8 PVECTOR_RENAME(scale_u16_from_u8)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(scale_u32_from_u16)(__ivec s,__ivec *hipart)
{
#if 0 /* slower but portable on non-x86 CPUs version */
    __ivec tmp[2];
    tmp[0]  = _ivec_u32_from_u16(s,&tmp[1]);
    *hipart = _ivec_sll_s32_imm(tmp[1],16);
    return    _ivec_sll_s32_imm(tmp[0],16);
#else
    __ivec filler = _ivec_setzero();
    *hipart = _ivec_interleave_hi_u16(filler,s);
    return    _ivec_interleave_lo_u16(filler,s);
#endif
}
#undef _ivec_scale_u32_from_u16
#define _ivec_scale_u32_from_u16 PVECTOR_RENAME(scale_u32_from_u16)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(scale_s16_from_s32)(__ivec s1, __ivec s2)
{
    return _ivec_s16_from_s32(_ivec_sra_s32_imm(s1,16),_ivec_sra_s32_imm(s2,16));
}
#undef _ivec_scale_s16_from_s32
#define _ivec_scale_s16_from_s32 PVECTOR_RENAME(scale_s16_from_s32)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(scale_s8_from_s16)(__ivec s1, __ivec s2)
{
    return _ivec_s8_from_s16(_ivec_sra_s16_imm(s1,8),_ivec_sra_s16_imm(s2,8));
}
#undef _ivec_scale_s8_from_s16
#define _ivec_scale_s8_from_s16 PVECTOR_RENAME(scale_s8_from_s16)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(scale_u8_from_u16)(__ivec s1, __ivec s2)
{
    return _ivec_u8_from_u16(_ivec_sra_s16_imm(s1,8),_ivec_sra_s16_imm(s2,8));
}
#undef _ivec_scale_u8_from_u16
#define _ivec_scale_u8_from_u16 PVECTOR_RENAME(scale_u8_from_u16)

