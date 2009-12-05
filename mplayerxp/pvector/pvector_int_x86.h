/*
  pvector_int_x86.h
*/
#undef _VEC
#if defined( OPTIMIZE_AVX )
#define _VEC(a) a ## _AVX
#include <avxintrin.h>
#elif defined(OPTIMIZE_AES)
#define _VEC(a) a ## _AES
#include <wmmintrin.h>
#elif defined (OPTIMIZE_SSE4)
#define _VEC(a) a ## _SSE4
#include <smmintrin.h>
#elif defined(OPTIMIZE_SSSE3)
#define _VEC(a) a ## _SSSE3
#include <tmmintrin.h>
#elif defined(OPTIMIZE_SSE3)
#define _VEC(a) a ## _SSE3
#include <pmmintrin.h>
#elif defined(OPTIMIZE_SSE2)
#define _VEC(a) a ## _SSE2
#include <emmintrin.h>
#elif defined(OPTIMIZE_MMX2)
#define _VEC(a) a ## _MMX2
#include <xmmintrin.h>
#else
#define _VEC(a) a ## _MMX
#include <mmintrin.h>
#endif

#undef __IVEC_SIZE
#undef __ivec
#ifdef OPTIMIZE_SSE2
#define __IVEC_SIZE	16
#define __ivec		__m128i
#else
#define __IVEC_SIZE	8
#define __ivec		__m64
#endif

extern __inline void __attribute__((__gnu_inline__, __always_inline__))
_VEC(empty)(void)
{
#ifdef OPTIMIZE_SSE2
#else
    _mm_empty();
#endif
}
#undef _ivec_empty
#define _ivec_empty _VEC(empty)

extern __inline void __attribute__((__gnu_inline__, __always_inline__))
_VEC(sfence)(void)
{
#ifdef OPTIMIZE_MMX2
    _mm_sfence();
#endif
}
#undef _ivec_sfence
#define _ivec_sfence _VEC(sfence)

extern __inline void __attribute__((__gnu_inline__, __always_inline__))
_VEC(prefetch)(void  const *__P)
{
#ifdef OPTIMIZE_MMX2
    _mm_prefetch(__P, _MM_HINT_T0);
#endif
}
#undef _ivec_prefetch
#define _ivec_prefetch _VEC(prefetch)

extern __inline void __attribute__((__gnu_inline__, __always_inline__))
_VEC(prefetchw)(void  const *__P)
{
#ifdef OPTIMIZE_MMX2
    _mm_prefetch(__P, _MM_HINT_NTA);
#endif
}
#undef _ivec_prefetchw
#define _ivec_prefetchw _VEC(prefetchw)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(loadu)(void const *__P)
{
#ifdef OPTIMIZE_SSE3
    return _mm_lddqu_si128(__P);
#elif defined OPTIMIZE_SSE2
    return (__ivec)_mm_loadu_si128(__P);
#else
    return *(__ivec const *)__P;
#endif
}
#undef _ivec_loadu
#define _ivec_loadu _VEC(loadu)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(loada)(void const *__P)
{
#ifdef OPTIMIZE_SSE2
    return (__ivec)_mm_load_si128(__P);
#else
    return *(__ivec const *)__P;
#endif
}
#undef _ivec_loada
#define _ivec_loada _VEC(loada)

extern __inline void __attribute__((__gnu_inline__, __always_inline__))
_VEC(storeu)(void *__P, __ivec src)
{
#ifdef OPTIMIZE_SSE2
    _mm_storeu_si128(__P,src);
#else
    *(__ivec *)__P = src;
#endif
}
#undef _ivec_storeu
#define _ivec_storeu _VEC(storeu)

extern __inline void __attribute__((__gnu_inline__, __always_inline__))
_VEC(storea)(void *__P, __ivec src)
{
#ifdef OPTIMIZE_SSE2
    _mm_store_si128(__P,src);
#else
    *(__ivec *)__P = src;
#endif
}
#undef _ivec_storea
#define _ivec_storea _VEC(storea)

extern __inline void __attribute__((__gnu_inline__, __always_inline__))
_VEC(stream)(void *__P, __ivec src)
{
#ifdef OPTIMIZE_SSE2
    _mm_stream_si128(__P,src);
#elif defined( OPTIMIZE_MMX2 )
    _mm_stream_pi(__P,src);
#else
    *(__ivec *)__P = src;
#endif
}
#undef _ivec_stream
#define _ivec_stream _VEC(stream)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(setzero)(void)
{
#ifdef OPTIMIZE_SSE2
    return (__ivec)_mm_setzero_si128();
#else
    return _mm_setzero_si64();
#endif
}
#undef _ivec_setzero
#define _ivec_setzero _VEC(setzero)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(setff)(void)
{
#ifdef OPTIMIZE_SSE2
    return _mm_set1_epi8(0xFF);
#else
    return _mm_set1_pi8(0xFF);
#endif
}
#undef _ivec_setff
#define _ivec_setff _VEC(setff)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(broadcast_u8)(unsigned char u8)
{
#ifdef OPTIMIZE_SSE2
    return _mm_set1_epi8(u8);
#else
    return _mm_set1_pi8(u8);
#endif
}
#undef _ivec_broadcast_u8
#define _ivec_broadcast_u8 _VEC(broadcast_u8)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(broadcast_u16)(unsigned short u16)
{
#ifdef OPTIMIZE_SSE2
    return _mm_set1_epi16(u16);
#else
    return _mm_set1_pi16(u16);
#endif
}
#undef _ivec_broadcast_u16
#define _ivec_broadcast_u16 _VEC(broadcast_u16)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(broadcast_u32)(unsigned int u32)
{
#ifdef OPTIMIZE_SSE2
    return _mm_set1_epi16(u32);
#else
    return _mm_set1_pi16(u32);
#endif
}
#undef _ivec_broadcast_u32
#define _ivec_broadcast_u32 _VEC(broadcast_u32)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(or)(__ivec __m1, __ivec __m2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_or_si128(__m1,__m2);
#else
    return _mm_or_si64(__m1,__m2);
#endif
}
#undef _ivec_or
#define _ivec_or _VEC(or)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(and)(__ivec __m1, __ivec __m2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_and_si128(__m1,__m2);
#else
    return _mm_and_si64(__m1,__m2);
#endif
}
#undef _ivec_and
#define _ivec_and _VEC(and)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(andnot)(__ivec __m1, __ivec __m2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_andnot_si128(__m1,__m2);
#else
    return _mm_andnot_si64(__m1,__m2);
#endif
}
#undef _ivec_andnot
#define _ivec_andnot _VEC(andnot)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(xor)(__ivec __m1, __ivec __m2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_xor_si128(__m1,__m2);
#else
    return _mm_xor_si64(__m1,__m2);
#endif
}
#undef _ivec_xor
#define _ivec_xor _VEC(xor)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(not)(__ivec __m)
{
    return _ivec_xor(__m,_ivec_setff());
}
#undef _ivec_not
#define _ivec_not _VEC(not)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(cmpgt_s8)(__ivec __m1, __ivec __m2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_cmpgt_epi8(__m1,__m2);
#else
    return _mm_cmpgt_pi8(__m1,__m2);
#endif
}
#undef _ivec_cmpgt_s8
#define _ivec_cmpgt_s8 _VEC(cmpgt_s8)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(cmpeq_s8)(__ivec __m1, __ivec __m2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_cmpeq_epi8(__m1,__m2);
#else
    return _mm_cmpeq_pi8(__m1,__m2);
#endif
}
#undef _ivec_cmpeq_s8
#define _ivec_cmpeq_s8 _VEC(cmpeq_s8)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(cmpgt_s16)(__ivec __m1, __ivec __m2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_cmpgt_epi16(__m1,__m2);
#else
    return _mm_cmpgt_pi16(__m1,__m2);
#endif
}
#undef _ivec_cmpgt_s16
#define _ivec_cmpgt_s16 _VEC(cmpgt_s16)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(cmpeq_s16)(__ivec __m1, __ivec __m2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_cmpeq_epi16(__m1,__m2);
#else
    return _mm_cmpeq_pi16(__m1,__m2);
#endif
}
#undef _ivec_cmpeq_s16
#define _ivec_cmpeq_s16 _VEC(cmpeq_s16)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(cmpgt_s32)(__ivec __m1, __ivec __m2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_cmpgt_epi32(__m1,__m2);
#else
    return _mm_cmpgt_pi32(__m1,__m2);
#endif
}
#undef _ivec_cmpgt_s32
#define _ivec_cmpgt_s32 _VEC(cmpgt_s32)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(cmpeq_s32)(__ivec __m1, __ivec __m2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_cmpeq_epi32(__m1,__m2);
#else
    return _mm_cmpeq_pi32(__m1,__m2);
#endif
}
#undef _ivec_cmpeq_s32
#define _ivec_cmpeq_s32 _VEC(cmpeq_s32)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(blend_u8)(__ivec src1,__ivec src2,__ivec mask)
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
#define _ivec_blend_u8 _VEC(blend_u8)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(interleave_lo_u8)(__ivec s1, __ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_unpacklo_epi8(s1,s2);
#else
    return _mm_unpacklo_pi8(s1,s2);
#endif
}
#undef _ivec_interleave_lo_u8
#define _ivec_interleave_lo_u8 _VEC(interleave_lo_u8)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(interleave_hi_u8)(__ivec s1, __ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_unpackhi_epi8(s1,s2);
#else
    return _mm_unpackhi_pi8(s1,s2);
#endif
}
#undef _ivec_interleave_hi_u8
#define _ivec_interleave_hi_u8 _VEC(interleave_hi_u8)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(interleave_lo_u16)(__ivec s1, __ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_unpacklo_epi16(s1,s2);
#else
    return _mm_unpacklo_pi16(s1,s2);
#endif
}
#undef _ivec_interleave_lo_u16
#define _ivec_interleave_lo_u16 _VEC(interleave_lo_u16)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(interleave_hi_u16)(__ivec s1, __ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_unpackhi_epi16(s1,s2);
#else
    return _mm_unpackhi_pi16(s1,s2);
#endif
}
#undef _ivec_interleave_hi_u16
#define _ivec_interleave_hi_u16 _VEC(interleave_hi_u16)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(interleave_lo_u32)(__ivec s1, __ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_unpacklo_epi32(s1,s2);
#else
    return _mm_unpacklo_pi32(s1,s2);
#endif
}
#undef _ivec_interleave_lo_u32
#define _ivec_interleave_lo_u32 _VEC(interleave_lo_u32)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(interleave_hi_u32)(__ivec s1, __ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_unpackhi_epi32(s1,s2);
#else
    return _mm_unpackhi_pi32(s1,s2);
#endif
}
#undef _ivec_interleave_hi_u32
#define _ivec_interleave_hi_u32 _VEC(interleave_hi_u32)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(u16_from_lou8)(__ivec s)
{
    return _ivec_interleave_lo_u8(s,_ivec_setzero());
}
#undef _ivec_u16_from_lou8
#define _ivec_u16_from_lou8 _VEC(u16_from_lou8)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(u16_from_hiu8)(__ivec s)
{
    return _ivec_interleave_hi_u8(s,_ivec_setzero());
}
#undef _ivec_u16_from_hiu8
#define _ivec_u16_from_hiu8 _VEC(u16_from_hiu8)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(u32_from_lou16)(__ivec s)
{
    return _ivec_interleave_lo_u16(s,_ivec_setzero());
}
#undef _ivec_u32_from_lou16
#define _ivec_u32_from_lou16 _VEC(u32_from_lou16)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(u32_from_hiu16)(__ivec s)
{
    return _ivec_interleave_hi_u16(s,_ivec_setzero());
}
#undef _ivec_u32_from_hiu16
#define _ivec_u32_from_hiu16 _VEC(u32_from_hiu16)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(s16_from_s32)(__ivec s1, __ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_packs_epi32(s1,s2);
#else
    return _mm_packs_pi32(s1,s2);
#endif
}
#undef _ivec_s16_from_s32
#define _ivec_s16_from_s32 _VEC(s16_from_s32)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(s8_from_s16)(__ivec s1, __ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_packs_epi16(s1,s2);
#else
    return _mm_packs_pi16(s1,s2);
#endif
}
#undef _ivec_s8_from_s16
#define _ivec_s8_from_s16 _VEC(s8_from_s16)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(u8_from_u16)(__ivec s1, __ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_packus_epi16(s1,s2);
#else
    return _mm_packs_pu16(s1,s2);
#endif
}
#undef _ivec_u8_from_u16
#define _ivec_u8_from_u16 _VEC(u8_from_u16)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(add_s8)(__ivec s1,__ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_add_epi8(s1,s2);
#else
    return _mm_add_pi8(s1,s2);
#endif
}
#undef _ivec_add_s8
#define _ivec_add_s8 _VEC(add_s8)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(add_s16)(__ivec s1,__ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_add_epi16(s1,s2);
#else
    return _mm_add_pi16(s1,s2);
#endif
}
#undef _ivec_add_s16
#define _ivec_add_s16 _VEC(add_s16)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(add_s32)(__ivec s1,__ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_add_epi32(s1,s2);
#else
    return _mm_add_pi32(s1,s2);
#endif
}
#undef _ivec_add_s32
#define _ivec_add_s32 _VEC(add_s32)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(sadd_s8)(__ivec s1,__ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_adds_epi8(s1,s2);
#else
    return _mm_adds_pi8(s1,s2);
#endif
}
#undef _ivec_sadd_s8
#define _ivec_sadd_s8 _VEC(sadd_s8)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(sadd_s16)(__ivec s1,__ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_adds_epi16(s1,s2);
#else
    return _mm_adds_pi16(s1,s2);
#endif
}
#undef _ivec_sadd_s16
#define _ivec_sadd_s16 _VEC(sadd_s16)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(sadd_u8)(__ivec s1,__ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_adds_epu8(s1,s2);
#else
    return _mm_adds_pu8(s1,s2);
#endif
}
#undef _ivec_sadd_u8
#define _ivec_sadd_u8 _VEC(sadd_u8)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(sadd_u16)(__ivec s1,__ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_adds_epu16(s1,s2);
#else
    return _mm_adds_pu16(s1,s2);
#endif
}
#undef _ivec_sadd_u16
#define _ivec_sadd_u16 _VEC(sadd_u16)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(sub_s8)(__ivec s1,__ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_sub_epi8(s1,s2);
#else
    return _mm_sub_pi8(s1,s2);
#endif
}
#undef _ivec_sub_s8
#define _ivec_sub_s8 _VEC(sub_s8)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(sub_s16)(__ivec s1,__ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_sub_epi16(s1,s2);
#else
    return _mm_sub_pi16(s1,s2);
#endif
}
#undef _ivec_sub_s16
#define _ivec_sub_s16 _VEC(sub_s16)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(sub_s32)(__ivec s1,__ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_sub_epi32(s1,s2);
#else
    return _mm_sub_pi32(s1,s2);
#endif
}
#undef _ivec_sub_s32
#define _ivec_sub_s32 _VEC(sub_s32)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(ssub_s8)(__ivec s1,__ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_subs_epi8(s1,s2);
#else
    return _mm_subs_pi8(s1,s2);
#endif
}
#undef _ivec_ssub_s8
#define _ivec_ssub_s8 _VEC(ssub_s8)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(ssub_s16)(__ivec s1,__ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_subs_epi16(s1,s2);
#else
    return _mm_subs_pi16(s1,s2);
#endif
}
#undef _ivec_ssub_s16
#define _ivec_ssub_s16 _VEC(ssub_s16)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(ssub_u8)(__ivec s1,__ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_subs_epu8(s1,s2);
#else
    return _mm_subs_pu8(s1,s2);
#endif
}
#undef _ivec_ssub_u8
#define _ivec_ssub_u8 _VEC(ssub_u8)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(ssub_u16)(__ivec s1,__ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_subs_epu16(s1,s2);
#else
    return _mm_subs_pu16(s1,s2);
#endif
}
#undef _ivec_ssub_u16
#define _ivec_ssub_u16 _VEC(ssub_u16)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(mullo_s16)(__ivec s1,__ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_mullo_epi16(s1,s2);
#else
    return _mm_mullo_pi16(s1,s2);
#endif
}
#undef _ivec_mullo_s16
#define _ivec_mullo_s16 _VEC(mullo_s16)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(mulhi_s16)(__ivec s1,__ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_mulhi_epi16(s1,s2);
#else
    return _mm_mulhi_pi16(s1,s2);
#endif
}
#undef _ivec_mulhi_s16
#define _ivec_mulhi_s16 _VEC(mulhi_s16)

extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(sll_s16)(__ivec s1,__ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_sll_epi16(s1,s2);
#else
    return _mm_sll_pi16(s1,s2);
#endif
}
#undef _ivec_sll_s16
#define _ivec_sll_s16 _VEC(sll_s16)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(sll_s16_imm)(__ivec s1,int c)
{
#ifdef OPTIMIZE_SSE2
    return _mm_slli_epi16(s1,c);
#else
    return _mm_slli_pi16(s1,c);
#endif
}
#undef _ivec_sll_s16_imm
#define _ivec_sll_s16_imm _VEC(sll_s16_imm)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(sll_s32)(__ivec s1,__ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_sll_epi32(s1,s2);
#else
    return _mm_sll_pi32(s1,s2);
#endif
}
#undef _ivec_sll_s32
#define _ivec_sll_s32 _VEC(sll_s32)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(sll_s32_imm)(__ivec s1,int c)
{
#ifdef OPTIMIZE_SSE2
    return _mm_slli_epi32(s1,c);
#else
    return _mm_slli_pi32(s1,c);
#endif
}
#undef _ivec_sll_s32_imm
#define _ivec_sll_s32_imm _VEC(sll_s32_imm)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(sll_s64)(__ivec s1,__ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_sll_epi64(s1,s2);
#else
    return _mm_sll_si64(s1,s2);
#endif
}
#undef _ivec_sll_s64
#define _ivec_sll_s64 _VEC(sll_s64)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(sll_s64_imm)(__ivec s1,int c)
{
#ifdef OPTIMIZE_SSE2
    return _mm_slli_epi64(s1,c);
#else
    return _mm_slli_si64(s1,c);
#endif
}
#undef _ivec_sll_s64_imm
#define _ivec_sll_s64_imm _VEC(sll_s64_imm)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(sra_s16)(__ivec s1,__ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_sra_epi16(s1,s2);
#else
    return _mm_sra_pi16(s1,s2);
#endif
}
#undef _ivec_sra_s16
#define _ivec_sra_s16 _VEC(sra_s16)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(sra_s16_imm)(__ivec s1,int c)
{
#ifdef OPTIMIZE_SSE2
    return _mm_srai_epi16(s1,c);
#else
    return _mm_srai_pi16(s1,c);
#endif
}
#undef _ivec_sra_s16_imm
#define _ivec_sra_s16_imm _VEC(sra_s16_imm)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(sra_s32)(__ivec s1,__ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_sra_epi32(s1,s2);
#else
    return _mm_sra_pi32(s1,s2);
#endif
}
#undef _ivec_sra_s32
#define _ivec_sra_s32 _VEC(sra_s32)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(sra_s32_imm)(__ivec s1,int c)
{
#ifdef OPTIMIZE_SSE2
    return _mm_srai_epi32(s1,c);
#else
    return _mm_srai_pi32(s1,c);
#endif
}
#undef _ivec_sra_s32_imm
#define _ivec_sra_s32_imm _VEC(sra_s32_imm)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(srl_s16)(__ivec s1,__ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_srl_epi16(s1,s2);
#else
    return _mm_srl_pi16(s1,s2);
#endif
}
#undef _ivec_srl_s16
#define _ivec_srl_s16 _VEC(srl_s16)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(srl_s16_imm)(__ivec s1,int c)
{
#ifdef OPTIMIZE_SSE2
    return _mm_srli_epi16(s1,c);
#else
    return _mm_srli_pi16(s1,c);
#endif
}
#undef _ivec_srl_s16_imm
#define _ivec_srl_s16_imm _VEC(srl_s16_imm)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(srl_s32)(__ivec s1,__ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_srl_epi32(s1,s2);
#else
    return _mm_srl_pi32(s1,s2);
#endif
}
#undef _ivec_srl_s32
#define _ivec_srl_s32 _VEC(srl_s32)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(srl_s32_imm)(__ivec s1,int c)
{
#ifdef OPTIMIZE_SSE2
    return _mm_srli_epi32(s1,c);
#else
    return _mm_srli_pi32(s1,c);
#endif
}
#undef _ivec_srl_s32_imm
#define _ivec_srl_s32_imm _VEC(srl_s32_imm)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(srl_s64)(__ivec s1,__ivec s2)
{
#ifdef OPTIMIZE_SSE2
    return _mm_srl_epi64(s1,s2);
#else
    return _mm_srl_si64(s1,s2);
#endif
}
#undef _ivec_srl_s64
#define _ivec_srl_s64 _VEC(srl_s64)
extern __inline __ivec __attribute__((__gnu_inline__, __always_inline__))
_VEC(srl_s64_imm)(__ivec s1,int c)
{
#ifdef OPTIMIZE_SSE2
    return _mm_srli_epi64(s1,c);
#else
    return _mm_srli_si64(s1,c);
#endif
}
#undef _ivec_srl_s64_imm
#define _ivec_srl_s64_imm _VEC(srl_s64_imm)
