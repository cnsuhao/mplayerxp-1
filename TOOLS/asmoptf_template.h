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
#ifndef SATURATE
#define SATURATE(x,_min,_max) {if((x)<(_min)) (x)=(_min); else if((x)>(_max)) (x)=(_max);}
#endif

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



static float __FASTCALL__ PVECTOR_RENAME(pack)(const float* w, const float* x, unsigned len)
{
#if defined ( OPTIMIZE_SSE )
/* GCC supports nested functions */
inline __m128 _my_hadd_ps( __m128 a, __m128 b) {
    __m128 tempA = _mm_shuffle_ps(a,b, _MM_SHUFFLE(2,0,2,0));
    __m128 tempB = _mm_shuffle_ps(a,b, _MM_SHUFFLE(3,1,3,1));
    return _mm_add_ps( tempB, tempA);
}
    __m128 xmm[4];
    float rval;
    xmm[0] = _mm_loadu_ps(&w[0]);
    xmm[1] = _mm_loadu_ps(&w[4]);
    xmm[2] = _mm_loadu_ps(&w[8]);
    xmm[3] = _mm_loadu_ps(&w[12]);
    xmm[0] = _mm_mul_ps(xmm[0],_mm_loadu_ps(&x[0]));
    xmm[1] = _mm_mul_ps(xmm[1],_mm_loadu_ps(&x[4]));
    xmm[2] = _mm_mul_ps(xmm[2],_mm_loadu_ps(&x[8]));
    xmm[3] = _mm_mul_ps(xmm[3],_mm_loadu_ps(&x[12]));

    xmm[0] = _mm_add_ps(xmm[0],xmm[1]);
    xmm[2] = _mm_add_ps(xmm[2],xmm[3]);
    xmm[0] = _mm_add_ps(xmm[0],xmm[2]);

#ifdef OPTIMIZE_SSE3
    xmm[0] = _mm_hadd_ps(xmm[0],xmm[0]);
    xmm[0] = _mm_hadd_ps(xmm[0],xmm[0]);
#else
    xmm[0] = _my_hadd_ps(xmm[0],xmm[0]);
    xmm[0] = _my_hadd_ps(xmm[0],xmm[0]);
#endif
    _mm_store_ss(&rval,xmm[0]);
    return rval;
#elif defined ( OPTIMIZE_3DNOW ) && !defined(OPTIMIZE_SSE)
    __m64 mm[8];
    union {
	unsigned ipart;
	float    fpart;
    }rval;
    mm[0] = _m_load(&w[0]);
    mm[1] = _m_load(&w[2]);
    mm[2] = _m_load(&w[4]);
    mm[3] = _m_load(&w[6]);
    mm[4] = _m_load(&w[8]);
    mm[5] = _m_load(&w[10]);
    mm[6] = _m_load(&w[12]);
    mm[7] = _m_load(&w[14]);
    mm[0] = _m_pfmul(mm[0],_m_load(&x[0]));
    mm[1] = _m_pfmul(mm[1],_m_load(&x[2]));
    mm[2] = _m_pfmul(mm[2],_m_load(&x[4]));
    mm[3] = _m_pfmul(mm[3],_m_load(&x[6]));
    mm[4] = _m_pfmul(mm[4],_m_load(&x[8]));
    mm[5] = _m_pfmul(mm[5],_m_load(&x[10]));
    mm[6] = _m_pfmul(mm[6],_m_load(&x[12]));
    mm[7] = _m_pfmul(mm[7],_m_load(&x[14]));
    mm[0] = _m_pfadd(mm[0],mm[1]);
    mm[2] = _m_pfadd(mm[2],mm[3]);
    mm[4] = _m_pfadd(mm[4],mm[5]);
    mm[6] = _m_pfadd(mm[6],mm[7]);
    mm[0] = _m_pfadd(mm[0],mm[2]);
    mm[4] = _m_pfadd(mm[4],mm[6]);
    mm[0] = _m_pfadd(mm[0],mm[4]);
    mm[0] = _m_pfacc(mm[0],mm[0]);
    _m_store_half(&rval.ipart,mm[0]);
    _ivec_empty();
    return rval.fpart;
#else
  return ( w[0] *x[0] +w[1] *x[1] +w[2] *x[2] +w[3] *x[3]
         + w[4] *x[4] +w[5] *x[5] +w[6] *x[6] +w[7] *x[7]
         + w[8] *x[8] +w[9] *x[9] +w[10]*x[10]+w[11]*x[11]
         + w[12]*x[12]+w[13]*x[13]+w[14]*x[14]+w[15]*x[15] );
#endif
}

void PVECTOR_RENAME(convert)(uint8_t *dstbase,const uint8_t *src,const uint8_t *srca,unsigned int len)
{
    register unsigned i;
    float ftmp;
    const float* in = src;
    const uint32_t* out = dstbase;
    int final=1;
#ifdef HAVE_F32_PVECTOR
    unsigned len_mm;
  __f32vec int_max,plus1,minus1;
#endif
    i=0;
#ifdef HAVE_F32_PVECTOR
    int_max = _f32vec_broadcast(INT32_MAX-1);
    /* SSE float2int engine doesn't have SATURATION functionality.
       So CLAMP volume on 0.0002% here. */
    plus1 = _f32vec_broadcast(+0.999998);
    minus1= _f32vec_broadcast(-0.999998);
    if(!F32VEC_ALIGNED(out))
    for(;i<len;i++) {
      ftmp=((const float*)in)[i];
      SATURATE(ftmp,-1.0,+1.0);
      ((int32_t*)out)[i]=(int32_t)lrintf((INT_MAX-1)*ftmp);
      if(F32VEC_ALIGNED(out)) break;
    }
    _ivec_empty();
    len_mm=len&(~(__F32VEC_SIZE-1));
    if((len_mm-i)>=__F32VEC_SIZE/sizeof(float))
    for(;i<len_mm;i+=__F32VEC_SIZE/sizeof(float)) {
	__f32vec tmp;
	if(F32VEC_ALIGNED(in))
	    tmp = _f32vec_loada(&((const float*)in)[i]);
	else
	    tmp = _f32vec_loadu(&((const float*)in)[i]);
	tmp = _f32vec_clamp(tmp,minus1,plus1);
	tmp = _f32vec_mul(int_max,tmp);
	if(final)
	    _f32vec_to_s32_stream(&((int32_t*)out)[i],tmp);
	else
	    _f32vec_to_s32a(&((int32_t*)out)[i],tmp);
    }
    if(final) _ivec_sfence();
    _ivec_empty();
#endif
    for(;i<len;i++) {
      ftmp=((const float*)in)[i];
      SATURATE(ftmp,-0.99,+0.99);
      ((int32_t*)out)[i]=(int32_t)lrintf((INT32_MAX-1)*ftmp);
    }
}
