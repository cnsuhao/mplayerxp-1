/* DSP acceleration routines */
#include "pvector/pvector.h"
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

static void __FASTCALL__ PVECTOR_RENAME(change_bps)(const void* in_data, void* out_data, unsigned len, unsigned inbps, unsigned outbps)
{
#ifdef HAVE_INT_PVECTOR
  __ivec izero = _ivec_setzero();
  unsigned len_mm,j;
#endif
  unsigned i;
  // Change the number of bits
    switch(inbps){
    case 1:
      switch(outbps){
      case 2:
	i=0;
	for(;i<len;i++)
	  ((uint16_t*)out_data)[i]=((uint16_t)((uint8_t*)in_data)[i])<<8;
	break;
      case 3:
	for(i=0;i<len;i++)
	  ((uint8_t*)out_data)[3*i]=
	  ((uint8_t*)out_data)[3*i+1]=0;
	  ((uint8_t*)out_data)[3*i+2]=(((uint8_t*)in_data)[i]);
	break;
      case 4:
	i=0;
	for(;i<len;i++)
	  ((uint32_t*)out_data)[i]=((uint32_t)((uint8_t*)in_data)[i])<<24;
	break;
      }
      break;
    case 2:
      switch(outbps){
      case 1:
	i=0;
	for(;i<len;i++)
	  ((uint8_t*)out_data)[i]=(uint8_t)((((uint16_t*)in_data)[i])>>8);
	break;
      case 3:
	for(i=0;i<len;i++)
	  ((uint8_t*)out_data)[3*i]=0;
	  ((uint8_t*)out_data)[3*i+1]=(((uint8_t*)in_data)[2*i]);
	  ((uint8_t*)out_data)[3*i+2]=(((uint8_t*)in_data)[2*i+1]);
	break;
      case 4:
        i=0;
#ifdef HAVE_INT_PVECTOR
	j=0;
	len_mm=len&(~(__IVEC_SIZE-1));
	for(;i<len;i++,j+=2){
	    ((uint32_t*)out_data)[i]=((uint32_t)((uint16_t*)in_data)[i])<<16;
	    if((((long)out_data)&(__IVEC_SIZE-1))==0) break;
	}
	if((len_mm-i)>=__IVEC_SIZE)
	for(;i<len_mm;i+=__IVEC_SIZE/2,j+=__IVEC_SIZE)
	{
	    __ivec ind,tmp[2];
	    ind   = _ivec_loadu(&((uint8_t *)in_data)[j]);
#if 0 /* slower but portable on non-x86 CPUs version */
	    tmp[0]= _ivec_sll_s32_imm(_ivec_u32_from_lou16(ind),16);
	    tmp[1]= _ivec_sll_s32_imm(_ivec_u32_from_hiu16(ind),16);
#else
	    tmp[0]= _ivec_interleave_lo_u16(izero,ind);
	    tmp[1]= _ivec_interleave_hi_u16(izero,ind);
#endif
	    _ivec_storea(&((uint8_t *)out_data)[j*2],tmp[0]);
	    _ivec_storea(&((uint8_t *)out_data)[j*2+__IVEC_SIZE],tmp[1]);
	}
#endif
	for(;i<len;i++)
	  ((uint32_t*)out_data)[i]=((uint32_t)((uint16_t*)in_data)[i])<<16;
	break;
      }
      break;
    case 3:
      switch(outbps){
      case 1:
	for(i=0;i<len;i++)
	  ((uint8_t*)out_data)[i]=(((uint8_t*)in_data)[3*i]);
	break;
      case 2:
	for(i=0;i<len;i++)
	{
	  ((uint8_t*)out_data)[2*i]=(uint8_t)(((uint8_t*)in_data)[3*i+1]);
	  ((uint8_t*)out_data)[2*i+1]=(uint8_t)(((uint8_t*)in_data)[3*i+2]);
	}
	break;
      case 4:
	for(i=0;i<len;i++)
	{
	  ((uint8_t*)out_data)[4*i+1]=((uint8_t*)in_data)[3*i];
	  ((uint8_t*)out_data)[4*i+2]=((uint8_t*)in_data)[3*i+1];
	  ((uint8_t*)out_data)[4*i+3]=((uint8_t*)in_data)[3*i+2];
	  ((uint8_t*)out_data)[4*i]=0;
	}
	break;
      }
      break;
    case 4:
      switch(outbps){
      case 1:
	i=0;
	for(;i<len;i++)
	  ((uint8_t*)out_data)[i]=(uint8_t)((((uint32_t*)in_data)[i])>>24);
	break;
      case 2:
        i=0;
#ifdef HAVE_INT_PVECTOR
	j=0;
	len_mm=len&(~(__IVEC_SIZE-1));
	for(;i<len;i++,j+=2){
	    ((uint16_t*)out_data)[i]=(uint16_t)((((uint32_t*)in_data)[i])>>16);
	    if((((long)out_data)&(__IVEC_SIZE-1))==0) break;
	}
	if((len-i)>=__IVEC_SIZE)
	for(;i<len_mm;i+=__IVEC_SIZE/2,j+=__IVEC_SIZE)
	{
	    __ivec ind[2],tmp;
	    ind[0]= _ivec_sra_s32_imm(_ivec_loadu(&((uint8_t *)in_data)[j*2]),16);
	    ind[1]= _ivec_sra_s32_imm(_ivec_loadu(&((uint8_t *)in_data)[j*2+__IVEC_SIZE]),16);
	    tmp   = _ivec_s16_from_s32(ind[0],ind[1]);
	    _ivec_storea(&((uint8_t *)out_data)[j],tmp);
	}
#endif
	for(;i<len;i++)
	  ((uint16_t*)out_data)[i]=(uint16_t)((((uint32_t*)in_data)[i])>>16);
	break;
      case 3:
	for(i=0;i<len;i++)
	  ((uint8_t*)out_data)[3*i]=(((uint8_t*)in_data)[4*i+1]);
	  ((uint8_t*)out_data)[3*i+1]=(((uint8_t*)in_data)[4*i+2]);
	  ((uint8_t*)out_data)[3*i+2]=(((uint8_t*)in_data)[4*i+3]);
	break;
      }
      break;
    }
#ifdef HAVE_INT_PVECTOR
    _ivec_sfence();
    _ivec_empty();
#endif
}

static int32_t __FASTCALL__ PVECTOR_RENAME(FIR_i16)(int16_t *x,int16_t *w)
{
#ifdef OPTIMIZE_MMX
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
#ifdef OPTIMIZE_MMX2
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

static void __FASTCALL__ PVECTOR_RENAME(float2int)(void* in, void* out, int len, int bps)
{
#ifdef HAVE_3DNOW
  unsigned len_mm;
  float tmp_f32[2];
#endif
  float ftmp;
  register int i;
  switch(bps){
  case(1):
    for(i=0;i<len;i++) {
      ftmp=((float*)in)[i];
      SATURATE(ftmp,-1.0,+1.0);
      ((int8_t*)out)[i]=(int8_t)lrintf(SCHAR_MAX*ftmp);
    }
    break;
  case(2):
    i=0;
#ifdef HAVE_3DNOW
    len_mm=len&(~15);
    tmp_f32[0]=
    tmp_f32[1]=SHRT_MAX;
    for(;i<len_mm;i+=16)
    {
	__asm __volatile(
		PREFETCH" 64(%1)\n\t"
		PREFETCHW" 32(%0)\n\t"
		"movq	  (%1), %%mm0\n\t"
		"movq	 8(%1), %%mm1\n\t"
		"movq	16(%1), %%mm2\n\t"
		"movq	24(%1), %%mm3\n\t"
		"movq	32(%1), %%mm4\n\t"
		"movq	40(%1), %%mm5\n\t"
		"movq	48(%1), %%mm6\n\t"
		"movq	56(%1), %%mm7\n\t"
		"pfmul %2, %%mm0\n\t"
		"pfmul %2, %%mm1\n\t"
		"pfmul %2, %%mm2\n\t"
		"pfmul %2, %%mm3\n\t"
		"pfmul %2, %%mm4\n\t"
		"pfmul %2, %%mm5\n\t"
		"pfmul %2, %%mm6\n\t"
		"pfmul %2, %%mm7\n\t"
		"pf2id %%mm0, %%mm0\n\t"
		"pf2id %%mm1, %%mm1\n\t"
		"pf2id %%mm2, %%mm2\n\t"
		"pf2id %%mm3, %%mm3\n\t"
		"pf2id %%mm4, %%mm4\n\t"
		"pf2id %%mm5, %%mm5\n\t"
		"pf2id %%mm6, %%mm6\n\t"
		"pf2id %%mm7, %%mm7\n\t"
		"packssdw %%mm1, %%mm0\n\t"
		"packssdw %%mm3, %%mm2\n\t"
		"packssdw %%mm5, %%mm4\n\t"
		"packssdw %%mm7, %%mm6\n\t"
		"movq  %%mm0,   (%0)\n\t"
		"movq  %%mm2,  8(%0)\n\t"
		"movq  %%mm4, 16(%0)\n\t"
		"movq  %%mm6, 24(%0)"
		::"r"(&(((uint16_t*)out)[i])),"r"(&(((float*)in)[i])),"m"(tmp_f32[0])
		:"memory"
#ifdef FPU_CLOBBERED
		,FPU_CLOBBERED
#endif
#ifdef MMX_CLOBBERED
		,MMX_CLOBBERED
#endif
		);
    }
#endif
    for(;i<len;i++) {
      ftmp=((float*)in)[i];
      SATURATE(ftmp,-1.0,+1.0);
      ((int16_t*)out)[i]=(int16_t)lrintf(SHRT_MAX*ftmp);
    }
    break;
  case(3):
    for(i=0;i<len;i++) {
      ftmp=((float*)in)[i];
      SATURATE(ftmp,-1.0,+1.0);
      store24bit(out, i, (int32_t)lrintf((INT_MAX-1)*ftmp));
    }
    break;
  case(4):
    i=0;
#ifdef HAVE_3DNOW
    len_mm=len&(~15);
    tmp_f32[0]=
    tmp_f32[1]=INT_MAX;
    for(;i<len_mm;i+=16)
    {
	__asm __volatile(
		PREFETCH" 64(%1)\n\t"
		PREFETCHW" 64(%0)\n\t"
		"movq	  (%1), %%mm0\n\t"
		"movq	 8(%1), %%mm1\n\t"
		"movq	16(%1), %%mm2\n\t"
		"movq	24(%1), %%mm3\n\t"
		"movq	32(%1), %%mm4\n\t"
		"movq	40(%1), %%mm5\n\t"
		"movq	48(%1), %%mm6\n\t"
		"movq	56(%1), %%mm7\n\t"
		"pfmul %2, %%mm0\n\t"
		"pfmul %2, %%mm1\n\t"
		"pfmul %2, %%mm2\n\t"
		"pfmul %2, %%mm3\n\t"
		"pfmul %2, %%mm4\n\t"
		"pfmul %2, %%mm5\n\t"
		"pfmul %2, %%mm6\n\t"
		"pfmul %2, %%mm7\n\t"
		"pf2id %%mm0, %%mm0\n\t"
		"pf2id %%mm1, %%mm1\n\t"
		"pf2id %%mm2, %%mm2\n\t"
		"pf2id %%mm3, %%mm3\n\t"
		"pf2id %%mm4, %%mm4\n\t"
		"pf2id %%mm5, %%mm5\n\t"
		"pf2id %%mm6, %%mm6\n\t"
		"pf2id %%mm7, %%mm7\n\t"
		"movq  %%mm0,   (%0)\n\t"
		"movq  %%mm1,  8(%0)\n\t"
		"movq  %%mm2, 16(%0)\n\t"
		"movq  %%mm3, 24(%0)\n\t"
		"movq  %%mm4, 32(%0)\n\t"
		"movq  %%mm5, 40(%0)\n\t"
		"movq  %%mm6, 48(%0)\n\t"
		"movq  %%mm7, 56(%0)"
		::"r"(&(((uint32_t*)out)[i])),"r"(&(((float*)in)[i])),"m"(tmp_f32[0])
		:"memory"
#ifdef FPU_CLOBBERED
		,FPU_CLOBBERED
#endif
#ifdef MMX_CLOBBERED
		,MMX_CLOBBERED
#endif
		);
    }
#endif
    for(;i<len;i++) {
      ftmp=((float*)in)[i];
      SATURATE(ftmp,-1.0,+1.0);
      ((int32_t*)out)[i]=(int32_t)lrintf((INT_MAX-1)*ftmp);
    }
    break;
  }
#ifdef HAVE_3DNOW
    asm volatile(EMMS::
	:"memory"
#ifdef FPU_CLOBBERED
	,FPU_CLOBBERED
#endif
#ifdef MMX_CLOBBERED
	,MMX_CLOBBERED
#endif
	);
#endif
}

static void __FASTCALL__ PVECTOR_RENAME(int2float)(void* in, void* out, int len, int bps)
{
#ifdef HAVE_F32_PVECTOR
    __f32vec rev_imax = _f32vec_broadcast(1.0/INT_MAX);
#endif
  register int i;
  switch(bps){
  case(1):
    for(i=0;i<len;i++)
      ((float*)out)[i]=(1.0/SCHAR_MAX)*((float)((int8_t*)in)[i]);
    break;
  case(2):
    i=0;
#ifdef HAVE_3DNOW
    tmp_f32[0]=
    tmp_f32[1]=1.0/INT_MAX;
    len_mm=len&(~15);
    for(;i<len_mm;i+=16)
    {
	__asm __volatile(
		PREFETCH" 32(%1)\n\t"
		PREFETCHW" 64(%0)\n\t"
		"movq	(%1), %%mm0\n\t"
		"movq	8(%1), %%mm1\n\t"
		"movq	16(%1), %%mm2\n\t"
		"movq	24(%1), %%mm3\n\t"
		"pxor	%%mm4, %%mm4\n\t"
		"pxor	%%mm5, %%mm5\n\t"
		"pxor	%%mm6, %%mm6\n\t"
		"pxor	%%mm7, %%mm7\n\t"
		"punpcklwd %%mm0, %%mm4\n\t"
		"punpckhwd %%mm0, %%mm5\n\t"
		"punpcklwd %%mm1, %%mm6\n\t"
		"punpckhwd %%mm1, %%mm7\n\t"
		"pi2fd	%%mm4, %%mm4\n\t"
		"pi2fd	%%mm5, %%mm5\n\t"
		"pi2fd	%%mm6, %%mm6\n\t"
		"pi2fd	%%mm7, %%mm7\n\t"
		"pfmul	%2, %%mm4\n\t"
		"pfmul	%2, %%mm5\n\t"
		"pfmul	%2, %%mm6\n\t"
		"pfmul	%2, %%mm7\n\t"
		"movq  %%mm4, (%0)\n\t"
		"movq  %%mm5, 8(%0)\n\t"
		"movq  %%mm6, 16(%0)\n\t"
		"movq  %%mm7, 24(%0)\n\t"
		"pxor	%%mm4, %%mm4\n\t"
		"pxor	%%mm5, %%mm5\n\t"
		"pxor	%%mm6, %%mm6\n\t"
		"pxor	%%mm7, %%mm7\n\t"
		"punpcklwd %%mm2, %%mm4\n\t"
		"punpckhwd %%mm2, %%mm5\n\t"
		"punpcklwd %%mm3, %%mm6\n\t"
		"punpckhwd %%mm3, %%mm7\n\t"
		"pi2fd	%%mm4, %%mm4\n\t"
		"pi2fd	%%mm5, %%mm5\n\t"
		"pi2fd	%%mm6, %%mm6\n\t"
		"pi2fd	%%mm7, %%mm7\n\t"
		"pfmul	%2, %%mm4\n\t"
		"pfmul	%2, %%mm5\n\t"
		"pfmul	%2, %%mm6\n\t"
		"pfmul	%2, %%mm7\n\t"
		"movq  %%mm4, 32(%0)\n\t"
		"movq  %%mm5, 40(%0)\n\t"
		"movq  %%mm6, 48(%0)\n\t"
		"movq  %%mm7, 56(%0)\n\t"
		"femms"
		::"r"(&(((float*)out)[i])),"r"(&(((int16_t*)in)[i])),"m"(tmp_f32[0])
		:"memory"
#ifdef FPU_CLOBBERED
		,FPU_CLOBBERED
#endif
#ifdef MMX_CLOBBERED
		,MMX_CLOBBERED
#endif
		);
    }
#endif
    for(;i<len;i++)
      ((float*)out)[i]=(1.0/SHRT_MAX)*((float)((int16_t*)in)[i]);
    break;
  case(3):
    for(i=0;i<len;i++)
      ((float*)out)[i]=(1.0/INT_MAX)*((float)((int32_t)load24bit(in, i)));
    break;
  case(4):
    i=0;
#ifdef HAVE_F32_PVECTOR
    for(;i<len;i++) {
      ((float*)out)[i]=(1.0/INT_MAX)*((float)((int32_t*)in)[i]);
      if((((long)out)&(__F32VEC_SIZE-1))==0) break;
    }
    _ivec_empty();
    if((len-i)>=__F32VEC_SIZE)
    for(;i<len;i+=__F32VEC_SIZE/sizeof(float)) {
	__f32vec tmp;
	tmp = _f32vec_mul(rev_imax,_f32vec_from_s32u(&((int32_t*)in)[i]));
	_f32vec_storea(&((float*)out)[i],tmp);
    }
    _ivec_sfence();
    _ivec_empty();
#endif
    for(;i<len;i++)
      ((float*)out)[i]=(1.0/INT_MAX)*((float)((int32_t*)in)[i]);
    break;
  }
}

static float __FASTCALL__ PVECTOR_RENAME(FIR_f32)(float *x,float *w)
{
#ifdef HAVE_3DNOW
    float rval;
    __asm __volatile(
	"movq		(%1), %%mm0\n\t"
	"movq		8(%1), %%mm1\n\t"
	"movq		16(%1), %%mm2\n\t"
	"movq		24(%1), %%mm3\n\t"
	"movq		32(%1), %%mm4\n\t"
	"movq		40(%1), %%mm5\n\t"
	"movq		48(%1), %%mm6\n\t"
	"movq		56(%1), %%mm7\n\t"
	"pfmul		(%2), %%mm0\n\t"
	"pfmul		8(%2), %%mm1\n\t"
	"pfmul		16(%2), %%mm2\n\t"
	"pfmul		24(%2), %%mm3\n\t"
	"pfmul		32(%2), %%mm4\n\t"
	"pfmul		40(%2), %%mm5\n\t"
	"pfmul		48(%2), %%mm6\n\t"
	"pfmul		56(%2), %%mm7\n\t"
	"pfadd		%%mm1, %%mm0\n\t"
	"pfadd		%%mm3, %%mm2\n\t"
	"pfadd		%%mm5, %%mm4\n\t"
	"pfadd		%%mm7, %%mm6\n\t"
	"pfadd		%%mm2, %%mm0\n\t"
	"pfadd		%%mm6, %%mm4\n\t"
	"pfadd		%%mm4, %%mm0\n\t"
	"pfacc		%%mm0, %%mm0\n\t"
	"movd		%%mm0, %0\n\t"
	"femms"
	:"=&r"(rval):"r"(w),"r"(x)
	:"memory"
#ifdef FPU_CLOBBERED
	,FPU_CLOBBERED
#endif
#ifdef MMX_CLOBBERED
	,MMX_CLOBBERED
#endif
	);
	return rval;
#else
  return ( w[0] *x[0] +w[1] *x[1] +w[2] *x[2] +w[3] *x[3]
         + w[4] *x[4] +w[5] *x[5] +w[6] *x[6] +w[7] *x[7]
         + w[8] *x[8] +w[9] *x[9] +w[10]*x[10]+w[11]*x[11]
         + w[12]*x[12]+w[13]*x[13]+w[14]*x[14]+w[15]*x[15] );
#endif
}
