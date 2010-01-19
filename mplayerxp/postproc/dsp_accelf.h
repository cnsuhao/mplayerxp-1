/* DSP floating-point acceleration routines */

static void __FASTCALL__ RENAME(float2int)(void* in, void* out, int len, int bps)
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

static void __FASTCALL__ RENAME(int2float)(void* in, void* out, int len, int bps)
{
#ifdef HAVE_3DNOW
  unsigned len_mm;
  float tmp_f32[2];
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
#ifdef HAVE_3DNOW
    tmp_f32[0]=
    tmp_f32[1]=1.0/INT_MAX;
    len_mm=len&(~15);
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
		"pi2fd %%mm0, %%mm0\n\t"
		"pi2fd %%mm1, %%mm1\n\t"
		"pi2fd %%mm2, %%mm2\n\t"
		"pi2fd %%mm3, %%mm3\n\t"
		"pi2fd %%mm4, %%mm4\n\t"
		"pi2fd %%mm5, %%mm5\n\t"
		"pi2fd %%mm6, %%mm6\n\t"
		"pi2fd %%mm7, %%mm7\n\t"
		"pfmul %2, %%mm0\n\t"
		"pfmul %2, %%mm1\n\t"
		"pfmul %2, %%mm2\n\t"
		"pfmul %2, %%mm3\n\t"
		"pfmul %2, %%mm4\n\t"
		"pfmul %2, %%mm5\n\t"
		"pfmul %2, %%mm6\n\t"
		"pfmul %2, %%mm7\n\t"
		"movq  %%mm0,   (%0)\n\t"
		"movq  %%mm1,  8(%0)\n\t"
		"movq  %%mm2, 16(%0)\n\t"
		"movq  %%mm3, 24(%0)\n\t"
		"movq  %%mm4, 32(%0)\n\t"
		"movq  %%mm5, 40(%0)\n\t"
		"movq  %%mm6, 48(%0)\n\t"
		"movq  %%mm7, 56(%0)\n\t"
		"femms"
		::"r"(&(((float*)out)[i])),"r"(&(((int32_t*)in)[i])),"m"(tmp_f32[0])
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
      ((float*)out)[i]=(1.0/INT_MAX)*((float)((int32_t*)in)[i]);
    break;
  }
}

static float __FASTCALL__ RENAME(FIR_f32)(float *x,float *w)
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
