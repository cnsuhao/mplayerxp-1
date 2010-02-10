/* Put your stuff_template.h here */
#include "../mplayerxp/pvector/pvector.h"

/*
Benchmarks for this test:
-------------------------
GENERIC: cpu clocks=27765822 = 12564us
SSE3   : cpu clocks=3991075 = 1808us...[OK]
SSE2   : cpu clocks=3942081 = 1785us...[OK]
MMX2   : cpu clocks=5874563 = 2659us...[OK]
MMX    : cpu clocks=6092012 = 2757us...[OK]
*/
#ifdef HAVE_INT_PVECTOR
static __inline __m64 __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(_m_load)(const void *__P)
{
    return *(const __m64 *)__P;
}
#undef _m_load
#define _m_load PVECTOR_RENAME(_m_load)

static __inline __m64 __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(_m_load_half)(const void *__P)
{
    return _mm_cvtsi32_si64 (*(const int *)__P);
}
#undef _m_load_half
#define _m_load_half PVECTOR_RENAME(_m_load_half)

static __inline void __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(_m_store)(void *__P, __m64 src)
{
    *(__m64 *)__P = src;
}
#undef _m_store
#define _m_store PVECTOR_RENAME(_m_store)

static __inline void __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(_m_store_half)(void *__P, __m64 src)
{
    *(int *)__P = _mm_cvtsi64_si32(src);
}
#undef _m_store_half
#define _m_store_half PVECTOR_RENAME(_m_store_half)

static __inline void __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(_m_movntq)(void *__P, __m64 src)
{
#ifdef HAVE_MMX2
    _mm_stream_pi(__P,src);
#else
    _m_store(__P,src);
#endif
}
#undef _m_movntq
#define _m_movntq PVECTOR_RENAME(_m_movntq)
#endif

int PVECTOR_RENAME(pack)(const int16_t *w,const int16_t *x,unsigned int len)
{
#ifdef OPTIMIZE_SSE2
    __m128i xmm[4];
    xmm[0] = _mm_loadu_si128(&w[0]);
    xmm[1] = _mm_loadu_si128(&w[8]);

    xmm[2] = _mm_madd_epi16(xmm[0],_mm_loadu_si128(&x[0]));
    xmm[3] = _mm_madd_epi16(xmm[1],_mm_loadu_si128(&x[8]));

    xmm[0] = _mm_add_epi32(xmm[2],xmm[3]);

#ifdef OPTIMIZE_SSSE3
    xmm[0] = _mm_hadd_epi32(xmm[0],xmm[0]);
    xmm[0] = _mm_hadd_epi32(xmm[0],xmm[0]);
#else
    xmm[1] = _mm_shuffle_epi32(xmm[0],_MM_SHUFFLE(0,1,2,3));
    xmm[0] = _mm_add_epi32(xmm[0],xmm[1]);
    xmm[1] = _mm_shuffle_epi32(xmm[0],_MM_SHUFFLE(0,0,0,1));
    xmm[0] = _mm_add_epi32(xmm[0],xmm[1]);
#endif
    xmm[0] = _mm_srli_si128(xmm[0],16);
    return _mm_cvtsi128_si32(xmm[0]);
#elif defined( OPTIMIZE_MMX )
    __m64 mm[8];
    mm[0] = _m_load(&w[0]);
    mm[1] = _m_load(&w[4]);
    mm[2] = _m_load(&w[8]);
    mm[3] = _m_load(&w[12]);

    mm[4] = _m_pmaddwd(mm[0],_m_load(&x[0]));
    mm[5] = _m_pmaddwd(mm[1],_m_load(&x[4]));
    mm[6] = _m_pmaddwd(mm[2],_m_load(&x[8]));
    mm[7] = _m_pmaddwd(mm[3],_m_load(&x[12]));

    mm[0] = _m_paddd(mm[4],mm[5]);
    mm[1] = _m_paddd(mm[6],mm[7]);
    mm[2] = _m_paddd(mm[0],mm[1]);
#ifdef OPTIMIZE_SSE
    mm[0] = _m_pshufw(mm[2],0xFE);
#else
    mm[0] = mm[2];
    mm[0] = _m_psrlqi(mm[0],32);
#endif
    mm[0] = _m_paddd(mm[0],mm[2]);
    mm[0] = _m_psrldi(mm[0],16);
    return _mm_cvtsi64_si32(mm[0]);
#else
  return ( w[0] *x[0] +w[1] *x[1] +w[2] *x[2] +w[3] *x[3]
         + w[4] *x[4] +w[5] *x[5] +w[6] *x[6] +w[7] *x[7]
         + w[8] *x[8] +w[9] *x[9] +w[10]*x[10]+w[11]*x[11]
         + w[12]*x[12]+w[13]*x[13]+w[14]*x[14]+w[15]*x[15] ) >> 16;
#endif
}


void PVECTOR_RENAME(convert)(unsigned char *dstbase,unsigned char *src,unsigned char *srca,unsigned int asize)
{
#ifdef HAVE_INT_PVECTOR
    __ivec izero = _ivec_setzero();
#endif
    uint8_t *out_data = dstbase;
    uint8_t *in_data = src;

    unsigned i,len;
    i = 0;
    len = asize;
#ifdef HAVE_INT_PVECTOR
    for(;i<len;i++) {
	((uint16_t*)out_data)[i]=((uint16_t)((uint8_t*)in_data)[i])<<8;
	if((((long)out_data)&(__IVEC_SIZE-1))==0) break;
    }
    if((len-i)>=__IVEC_SIZE)
    for(;i<len;i+=__IVEC_SIZE){
	    __ivec ind,itmp[2];
	    ind   = _ivec_loadu(&((uint8_t *)in_data)[i]);
	    itmp[0]= _ivec_s16_from_s8(ind,&itmp[1]);
	    _ivec_storea(&((uint16_t*)out_data)[i],itmp[0]);
	    _ivec_storea(&((uint16_t*)out_data)[i+__IVEC_SIZE/2],itmp[1]);
    }
#endif
    for(;i<len;i++)
	((int16_t*)out_data)[i]=((int16_t)((int8_t*)in_data)[i]);
#ifdef HAVE_INT_PVECTOR
    _ivec_empty();
    _ivec_sfence();
#endif
}
