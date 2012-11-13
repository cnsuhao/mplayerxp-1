static void RENAME(mix2to1) (sample_t * dest, sample_t * src, sample_t bias)
{
#ifdef HAVE_SSE
	asm volatile(
	"movlps %2, %%xmm7		\n\t"
	"shufps $0x00, %%xmm7, %%xmm7	\n\t"
	"movl $-1024, %%"REG_S"		\n\t"
	".balign 16\n\t"
	"1:				\n\t"
	"movaps (%0, %%"REG_S"), %%xmm0	\n\t"
	"movaps 16(%0, %%"REG_S"), %%xmm1	\n\t"
	"addps (%1, %%"REG_S"), %%xmm0	\n\t"
	"addps 16(%1, %%"REG_S"), %%xmm1	\n\t"
	"addps %%xmm7, %%xmm0		\n\t"
	"addps %%xmm7, %%xmm1		\n\t"
	"movaps %%xmm0, (%1, %%"REG_S")	\n\t"
	"movaps %%xmm1, 16(%1, %%"REG_S")	\n\t"
	"addl $32, %%"REG_S"		\n\t"
	" jnz 1b			\n\t"
	:: "r" (src+256), "r" (dest+256), "m" (bias)
	: "%"REG_S""
	, "cc" /* conditional flags Jxx CMOVxx */
#ifdef SSE_CLOBBERED
	,SSE_CLOBBERED
#endif
	);
#elif defined( HAVE_3DNOW )
	asm volatile(
	"movd  %2, %%mm7	\n\t"
	"punpckldq %2, %%mm7	\n\t"
	"movl  $-1024, %%"REG_S"	\n\t"
	".balign 16\n\t"
	"1:			\n\t"
	"movq  (%0, %%"REG_S"), %%mm0	\n\t"
	"movq  8(%0, %%"REG_S"), %%mm1	\n\t"
	"movq  16(%0, %%"REG_S"), %%mm2	\n\t"
	"movq  24(%0, %%"REG_S"), %%mm3	\n\t"
	"pfadd (%1, %%"REG_S"), %%mm0	\n\t"
	"pfadd 8(%1, %%"REG_S"), %%mm1	\n\t"
	"pfadd 16(%1, %%"REG_S"), %%mm2	\n\t"
	"pfadd 24(%1, %%"REG_S"), %%mm3	\n\t"
	"pfadd %%mm7, %%mm0		\n\t"
	"pfadd %%mm7, %%mm1		\n\t"
	"pfadd %%mm7, %%mm2		\n\t"
	"pfadd %%mm7, %%mm3		\n\t"
	"movq  %%mm0, (%1, %%"REG_S")	\n\t"
	"movq  %%mm1, 8(%1, %%"REG_S")	\n\t"
	"movq  %%mm2, 16(%1, %%"REG_S")	\n\t"
	"movq  %%mm3, 24(%1, %%"REG_S")	\n\t"
	"addl $32, %%"REG_S"		\n\t"
	" jnz 1b			\n\t"
	"femms"
	:: "r" (src+256), "r" (dest+256), "m" (bias)
	: "%"REG_S""
	, "cc" /* conditional flags Jxx CMOVxx */
#ifdef FPU_CLOBBERED
	,FPU_CLOBBERED
#endif
#ifdef MMX_CLOBBERED
	,MMX_CLOBBERED
#endif
	);
#else
    int i;

    for (i = 0; i < 256; i++)
	dest[i] += BIAS (src[i]);
#endif
}

static void RENAME(mix3to1) (sample_t * samples, sample_t bias)
{
#ifdef HAVE_SSE
	asm volatile(
	"movlps %1, %%xmm7		\n\t"
	"shufps $0x00, %%xmm7, %%xmm7	\n\t"
	"movl $-1024, %%"REG_S"		\n\t"
	".balign 16\n\t"
	"1:				\n\t"
	"movaps (%0, %%"REG_S"), %%xmm0	\n\t"
	"movaps 1024(%0, %%"REG_S"), %%xmm1	\n\t"
	"addps 2048(%0, %%"REG_S"), %%xmm0	\n\t"
	"addps %%xmm7, %%xmm1		\n\t"
	"addps %%xmm1, %%xmm0		\n\t"
	"movaps %%xmm0, (%0, %%"REG_S")	\n\t"
	"addl $16, %%"REG_S"		\n\t"
	" jnz 1b			\n\t"
	:: "r" (samples+256), "m" (bias)
	: "%"REG_S""
	, "cc" /* conditional flags Jxx CMOVxx */
#ifdef SSE_CLOBBERED
	,SSE_CLOBBERED
#endif
	);
#elif defined( HAVE_3DNOW )
	asm volatile(
	"movd  %1, %%mm7	\n\t"
	"punpckldq %1, %%mm7	\n\t"
	"movl $-1024, %%"REG_S"	\n\t"
	".balign 16\n\t"
	"1:			\n\t"
	"movq  (%0, %%"REG_S"), %%mm0	\n\t"
	"movq  8(%0, %%"REG_S"), %%mm1	\n\t"
	"movq  1024(%0, %%"REG_S"), %%mm2	\n\t"
	"movq  1032(%0, %%"REG_S"), %%mm3	\n\t"
	"pfadd 2048(%0, %%"REG_S"), %%mm0	\n\t"
	"pfadd 2056(%0, %%"REG_S"), %%mm1	\n\t"
	"pfadd %%mm7, %%mm0		\n\t"
	"pfadd %%mm7, %%mm1		\n\t"
	"pfadd %%mm2, %%mm0		\n\t"
	"pfadd %%mm3, %%mm1		\n\t"
	"movq  %%mm0, (%0, %%"REG_S")	\n\t"
	"movq  %%mm1, 8(%0, %%"REG_S")	\n\t"
	"addl $16, %%"REG_S"		\n\t"
	" jnz 1b			\n\t"
	"femms"
	:: "r" (samples+256), "m" (bias)
	: "%"REG_S""
	, "cc" /* conditional flags Jxx CMOVxx */
#ifdef FPU_CLOBBERED
	,FPU_CLOBBERED
#endif
#ifdef MMX_CLOBBERED
	,MMX_CLOBBERED
#endif
	);
#else
    int i;

    for (i = 0; i < 256; i++)
	samples[i] += BIAS (samples[i + 256] + samples[i + 512]);
#endif
}

static void RENAME(mix4to1) (sample_t * samples, sample_t bias)
{
#ifdef HAVE_SSE
	asm volatile(
	"movlps %1, %%xmm7		\n\t"
	"shufps $0x00, %%xmm7, %%xmm7	\n\t"
	"movl $-1024, %%"REG_S"		\n\t"
	".balign 16\n\t"
	"1:				\n\t"
	"movaps (%0, %%"REG_S"), %%xmm0	\n\t"
	"movaps 1024(%0, %%"REG_S"), %%xmm1	\n\t"
	"addps 2048(%0, %%"REG_S"), %%xmm0	\n\t"
	"addps 3072(%0, %%"REG_S"), %%xmm1	\n\t"
	"addps %%xmm7, %%xmm0		\n\t"
	"addps %%xmm1, %%xmm0		\n\t"
	"movaps %%xmm0, (%0, %%"REG_S")	\n\t"
	"addl $16, %%"REG_S"		\n\t"
	" jnz 1b			\n\t"
	:: "r" (samples+256), "m" (bias)
	: "%"REG_S""
	, "cc" /* conditional flags Jxx CMOVxx */
#ifdef SSE_CLOBBERED
	,SSE_CLOBBERED
#endif
	);
#elif defined (HAVE_3DNOW )
	asm volatile(
	"movd  %1, %%mm7	\n\t"
	"punpckldq %1, %%mm7	\n\t"
	"movl $-1024, %%"REG_S"	\n\t"
	".balign 16\n\t"
	"1:			\n\t"
	"movq  (%0, %%"REG_S"), %%mm0	\n\t"
	"movq  8(%0, %%"REG_S"), %%mm1	\n\t"
	"movq  1024(%0, %%"REG_S"), %%mm2	\n\t"
	"movq  1032(%0, %%"REG_S"), %%mm3	\n\t"
	"pfadd 2048(%0, %%"REG_S"), %%mm0	\n\t"
	"pfadd 2056(%0, %%"REG_S"), %%mm1	\n\t"
	"pfadd 3072(%0, %%"REG_S"), %%mm2	\n\t"
	"pfadd 3080(%0, %%"REG_S"), %%mm3	\n\t"
	"pfadd %%mm7, %%mm0		\n\t"
	"pfadd %%mm7, %%mm1		\n\t"
	"pfadd %%mm2, %%mm0		\n\t"
	"pfadd %%mm3, %%mm1		\n\t"
	"movq  %%mm0, (%0, %%"REG_S")	\n\t"
	"movq  %%mm1, 8(%0, %%"REG_S")	\n\t"
	"addl $16, %%"REG_S"		\n\t"
	" jnz 1b			\n\t"
	"femms"
	:: "r" (samples+256), "m" (bias)
	: "%"REG_S""
	, "cc" /* conditional flags Jxx CMOVxx */
#ifdef FPU_CLOBBERED
	,FPU_CLOBBERED
#endif
#ifdef MMX_CLOBBERED
	,MMX_CLOBBERED
#endif
	);
#else
    int i;

    for (i = 0; i < 256; i++)
	samples[i] += BIAS (samples[i + 256] + samples[i + 512] +
			    samples[i + 768]);
#endif
}

static void RENAME(mix5to1) (sample_t * samples, sample_t bias)
{
#ifdef HAVE_SSE
	asm volatile(
	"movlps %1, %%xmm7		\n\t"
	"shufps $0x00, %%xmm7, %%xmm7	\n\t"
	"movl $-1024, %%"REG_S"		\n\t"
	".balign 16\n\t"
	"1:				\n\t"
	"movaps (%0, %%"REG_S"), %%xmm0	\n\t"
	"movaps 1024(%0, %%"REG_S"), %%xmm1	\n\t"
	"addps 2048(%0, %%"REG_S"), %%xmm0	\n\t"
	"addps 3072(%0, %%"REG_S"), %%xmm1	\n\t"
	"addps %%xmm7, %%xmm0		\n\t"
	"addps 4096(%0, %%"REG_S"), %%xmm1	\n\t"
	"addps %%xmm1, %%xmm0		\n\t"
	"movaps %%xmm0, (%0, %%"REG_S")	\n\t"
	"addl $16, %%"REG_S"		\n\t"
	" jnz 1b			\n\t"
	:: "r" (samples+256), "m" (bias)
	: "%"REG_S""
	, "cc" /* conditional flags Jxx CMOVxx */
#ifdef SSE_CLOBBERED
	,SSE_CLOBBERED
#endif
	);
#elif defined( HAVE_3DNOW )
	asm volatile(
	"movd  %1, %%mm7	\n\t"
	"punpckldq %1, %%mm7	\n\t"
	"movl $-1024, %%"REG_S"	\n\t"
	".balign 16\n\t"
	"1:			\n\t"
	"movq  (%0, %%"REG_S"), %%mm0	\n\t"
	"movq  8(%0, %%"REG_S"), %%mm1	\n\t"
	"movq  1024(%0, %%"REG_S"), %%mm2	\n\t"
	"movq  1032(%0, %%"REG_S"), %%mm3	\n\t"
	"pfadd 2048(%0, %%"REG_S"), %%mm0	\n\t"
	"pfadd 2056(%0, %%"REG_S"), %%mm1	\n\t"
	"pfadd 3072(%0, %%"REG_S"), %%mm2	\n\t"
	"pfadd 3080(%0, %%"REG_S"), %%mm3	\n\t"
	"pfadd %%mm7, %%mm0		\n\t"
	"pfadd %%mm7, %%mm1		\n\t"
	"pfadd 4096(%0, %%"REG_S"), %%mm2	\n\t"
	"pfadd 4104(%0, %%"REG_S"), %%mm3	\n\t"
	"pfadd %%mm2, %%mm0		\n\t"
	"pfadd %%mm3, %%mm1		\n\t"
	"movq  %%mm0, (%0, %%"REG_S")	\n\t"
	"movq  %%mm1, 8(%0, %%"REG_S")	\n\t"
	"addl $16, %%"REG_S"		\n\t"
	" jnz 1b			\n\t"
	"femms"
	:: "r" (samples+256), "m" (bias)
	: "%"REG_S""
	, "cc" /* conditional flags Jxx CMOVxx */
#ifdef FPU_CLOBBERED
	,FPU_CLOBBERED
#endif
#ifdef MMX_CLOBBERED
	,MMX_CLOBBERED
#endif
	);
#else
    int i;

    for (i = 0; i < 256; i++)
	samples[i] += BIAS (samples[i + 256] + samples[i + 512] +
			    samples[i + 768] + samples[i + 1024]);
#endif
}

static void RENAME(mix3to2) (sample_t * samples, sample_t bias)
{
#ifdef HAVE_SSE
	asm volatile(
	"movlps %1, %%xmm7		\n\t"
	"shufps $0x00, %%xmm7, %%xmm7	\n\t"
	"movl $-1024, %%"REG_S"		\n\t"
	".balign 16\n\t"
	"1:				\n\t"
	"movaps (%0, %%"REG_S"), %%xmm0	\n\t"
	"addps %%xmm7, %%xmm0		\n\t" //common
	"movaps 1024(%0, %%"REG_S"), %%xmm1	\n\t"
	"movaps 2048(%0, %%"REG_S"), %%xmm2	\n\t"
	"addps %%xmm0, %%xmm1		\n\t"
	"addps %%xmm0, %%xmm2		\n\t"
	"movaps %%xmm1, (%0, %%"REG_S")	\n\t"
	"movaps %%xmm2, 1024(%0, %%"REG_S")	\n\t"
	"addl $16, %%"REG_S"		\n\t"
	" jnz 1b			\n\t"
	:: "r" (samples+256), "m" (bias)
	: "%"REG_S""
	, "cc" /* conditional flags Jxx CMOVxx */
#ifdef SSE_CLOBBERED
	,SSE_CLOBBERED
#endif
	);
#elif defined( HAVE_3DNOW )
	asm volatile(
	"movd  %1, %%mm7	\n\t"
	"punpckldq %1, %%mm7	\n\t"
	"movl $-1024, %%"REG_S"	\n\t"
	".balign 16\n\t"
	"1:			\n\t"
	"movq   (%0, %%"REG_S"), %%mm0	\n\t"
	"movq   8(%0, %%"REG_S"), %%mm1	\n\t"
	"pfadd  %%mm7, %%mm0		\n\t" //common
	"pfadd  %%mm7, %%mm1		\n\t" //common
	"movq   1024(%0, %%"REG_S"), %%mm2	\n\t"
	"movq   1032(%0, %%"REG_S"), %%mm3	\n\t"
	"movq   2048(%0, %%"REG_S"), %%mm4	\n\t"
	"movq   2056(%0, %%"REG_S"), %%mm5	\n\t"
	"pfadd  %%mm0, %%mm2		\n\t"
	"pfadd  %%mm1, %%mm3		\n\t"
	"pfadd  %%mm0, %%mm4		\n\t"
	"pfadd  %%mm1, %%mm5		\n\t"
	"movq   %%mm2, (%0, %%"REG_S")	\n\t"
	"movq   %%mm3, 8(%0, %%"REG_S")	\n\t"
	"movq   %%mm4, 1024(%0, %%"REG_S")	\n\t"
	"movq   %%mm5, 1032(%0, %%"REG_S")	\n\t"
	"addl $16, %%"REG_S"		\n\t"
	" jnz 1b			\n\t"
	"femms"
	:: "r" (samples+256), "m" (bias)
	: "%"REG_S""
	, "cc" /* conditional flags Jxx CMOVxx */
#ifdef FPU_CLOBBERED
	,FPU_CLOBBERED
#endif
#ifdef MMX_CLOBBERED
	,MMX_CLOBBERED
#endif
	);
#else
    int i;
    sample_t common;

    for (i = 0; i < 256; i++) {
	common = BIAS (samples[i]);
	samples[i] = samples[i + 256] + common;
	samples[i + 256] = samples[i + 512] + common;
    }
#endif
}

static void RENAME(mix21to2) (sample_t * left, sample_t * right, sample_t bias)
{
#ifdef HAVE_SSE
	asm volatile(
		"movlps %2, %%xmm7		\n\t"
		"shufps $0x00, %%xmm7, %%xmm7	\n\t"
		"movl $-1024, %%"REG_S"		\n\t"
		".balign 16\n\t"
		"1:				\n\t"
		"movaps 1024(%1, %%"REG_S"), %%xmm0	\n\t"
		"addps %%xmm7, %%xmm0		\n\t" //common
		"movaps (%0, %%"REG_S"), %%xmm1	\n\t"
		"movaps (%1, %%"REG_S"), %%xmm2	\n\t"
		"addps %%xmm0, %%xmm1		\n\t"
		"addps %%xmm0, %%xmm2		\n\t"
		"movaps %%xmm1, (%0, %%"REG_S")	\n\t"
		"movaps %%xmm2, (%1, %%"REG_S")	\n\t"
		"addl $16, %%"REG_S"		\n\t"
		" jnz 1b			\n\t"
	:: "r" (left+256), "r" (right+256), "m" (bias)
	: "%"REG_S""
	, "cc" /* conditional flags Jxx CMOVxx */
#ifdef SSE_CLOBBERED
	,SSE_CLOBBERED
#endif
	);
#elif defined( HAVE_3DNOW )
	asm volatile(
		"movd  %2, %%mm7	\n\t"
		"punpckldq %2, %%mm7	\n\t"
		"movl $-1024, %%"REG_S"	\n\t"
		".balign 16\n\t"
		"1:			\n\t"
		"movq  1024(%1, %%"REG_S"), %%mm0	\n\t"
		"movq  1032(%1, %%"REG_S"), %%mm1	\n\t"
		"pfadd %%mm7, %%mm0		\n\t" //common
		"pfadd %%mm7, %%mm1		\n\t" //common
		"movq  (%0, %%"REG_S"), %%mm2	\n\t"
		"movq  8(%0, %%"REG_S"), %%mm3	\n\t"
		"movq  (%1, %%"REG_S"), %%mm4	\n\t"
		"movq  8(%1, %%"REG_S"), %%mm5	\n\t"
		"pfadd %%mm0, %%mm2		\n\t"
		"pfadd %%mm1, %%mm3		\n\t"
		"pfadd %%mm0, %%mm4		\n\t"
		"pfadd %%mm1, %%mm5		\n\t"
		"movq  %%mm2, (%0, %%"REG_S")	\n\t"
		"movq  %%mm3, 8(%0, %%"REG_S")	\n\t"
		"movq  %%mm4, (%1, %%"REG_S")	\n\t"
		"movq  %%mm5, 8(%1, %%"REG_S")	\n\t"
		"addl $16, %%"REG_S"		\n\t"
		" jnz 1b			\n\t"
		"femms"
	:: "r" (left+256), "r" (right+256), "m" (bias)
	: "%"REG_S""
	, "cc" /* conditional flags Jxx CMOVxx */
#ifdef FPU_CLOBBERED
	,FPU_CLOBBERED
#endif
#ifdef MMX_CLOBBERED
	,MMX_CLOBBERED
#endif
	);
#else
    int i;
    sample_t common;

    for (i = 0; i < 256; i++) {
	common = BIAS (right[i + 256]);
	left[i] += common;
	right[i] += common;
    }
#endif
}

static void RENAME(mix21toS) (sample_t * samples, sample_t bias)
{
#ifdef HAVE_SSE
	asm volatile(
		"movlps %1, %%xmm7		\n\t"
		"shufps $0x00, %%xmm7, %%xmm7	\n\t"
		"movl $-1024, %%"REG_S"		\n\t"
		".balign 16\n\t"
		"1:				\n\t"
		"movaps 2048(%0, %%"REG_S"), %%xmm0	\n\t"  // surround
		"movaps (%0, %%"REG_S"), %%xmm1	\n\t"
		"movaps 1024(%0, %%"REG_S"), %%xmm2	\n\t"
		"addps %%xmm7, %%xmm1		\n\t"
		"addps %%xmm7, %%xmm2		\n\t"
		"subps %%xmm0, %%xmm1		\n\t"
		"addps %%xmm0, %%xmm2		\n\t"
		"movaps %%xmm1, (%0, %%"REG_S")	\n\t"
		"movaps %%xmm2, 1024(%0, %%"REG_S")	\n\t"
		"addl $16, %%"REG_S"		\n\t"
		" jnz 1b			\n\t"
	:: "r" (samples+256), "m" (bias)
	: "%"REG_S""
	, "cc" /* conditional flags Jxx CMOVxx */
#ifdef SSE_CLOBBERED
	,SSE_CLOBBERED
#endif
	);
#elif defined( HAVE_3DNOW)
	asm volatile(
		"movd  %1, %%mm7	\n\t"
		"punpckldq %1, %%mm7	\n\t"
		"movl $-1024, %%"REG_S"	\n\t"
		".balign 16\n\t"
		"1:			\n\t"
		"movq  2048(%0, %%"REG_S"), %%mm0	\n\t"  // surround
		"movq  2056(%0, %%"REG_S"), %%mm1	\n\t"  // surround
		"movq  (%0, %%"REG_S"), %%mm2	\n\t"
		"movq  8(%0, %%"REG_S"), %%mm3	\n\t"
		"movq  1024(%0, %%"REG_S"), %%mm4	\n\t"
		"movq  1032(%0, %%"REG_S"), %%mm5	\n\t"
		"pfadd %%mm7, %%mm2		\n\t"
		"pfadd %%mm7, %%mm3		\n\t"
		"pfadd %%mm7, %%mm4		\n\t"
		"pfadd %%mm7, %%mm5		\n\t"
		"pfsub %%mm0, %%mm2		\n\t"
		"pfsub %%mm1, %%mm3		\n\t"
		"pfadd %%mm0, %%mm4		\n\t"
		"pfadd %%mm1, %%mm5		\n\t"
		"movq  %%mm2, (%0, %%"REG_S")	\n\t"
		"movq  %%mm3, 8(%0, %%"REG_S")	\n\t"
		"movq  %%mm4, 1024(%0, %%"REG_S")	\n\t"
		"movq  %%mm5, 1032(%0, %%"REG_S")	\n\t"
		"addl $16, %%"REG_S"		\n\t"
		" jnz 1b			\n\t"
		"femms"
	:: "r" (samples+256), "m" (bias)
	: "%"REG_S""
	, "cc" /* conditional flags Jxx CMOVxx */
#ifdef FPU_CLOBBERED
	,FPU_CLOBBERED
#endif
#ifdef MMX_CLOBBERED
	,MMX_CLOBBERED
#endif
	);
#else
    int i;
    sample_t surround;

    for (i = 0; i < 256; i++) {
	surround = samples[i + 512];
	samples[i] += BIAS (-surround);
	samples[i + 256] += BIAS (surround);
    }
#endif
}

static void RENAME(mix31to2) (sample_t * samples, sample_t bias)
{
#ifdef HAVE_SSE
	asm volatile(
		"movlps %1, %%xmm7		\n\t"
		"shufps $0x00, %%xmm7, %%xmm7	\n\t"
		"movl $-1024, %%"REG_S"		\n\t"
		".balign 16\n\t"
		"1:				\n\t"
		"movaps (%0, %%"REG_S"), %%xmm0	\n\t"
		"addps 3072(%0, %%"REG_S"), %%xmm0	\n\t"
		"addps %%xmm7, %%xmm0		\n\t" // common
		"movaps 1024(%0, %%"REG_S"), %%xmm1	\n\t"
		"movaps 2048(%0, %%"REG_S"), %%xmm2	\n\t"
		"addps %%xmm0, %%xmm1		\n\t"
		"addps %%xmm0, %%xmm2		\n\t"
		"movaps %%xmm1, (%0, %%"REG_S")	\n\t"
		"movaps %%xmm2, 1024(%0, %%"REG_S")	\n\t"
		"addl $16, %%"REG_S"		\n\t"
		" jnz 1b			\n\t"
	:: "r" (samples+256), "m" (bias)
	: "%"REG_S""
	, "cc" /* conditional flags Jxx CMOVxx */
#ifdef SSE_CLOBBERED
	,SSE_CLOBBERED
#endif
	);
#elif defined (HAVE_3DNOW)
	asm volatile(
		"movd  %1, %%mm7	\n\t"
		"punpckldq %1, %%mm7	\n\t"
		"movl $-1024, %%"REG_S"	\n\t"
		".balign 16\n\t"
		"1:			\n\t"
		"movq  (%0, %%"REG_S"), %%mm0	\n\t"
		"movq  8(%0, %%"REG_S"), %%mm1	\n\t"
		"pfadd 3072(%0, %%"REG_S"), %%mm0	\n\t"
		"pfadd 3080(%0, %%"REG_S"), %%mm1	\n\t"
		"pfadd %%mm7, %%mm0		\n\t" // common
		"pfadd %%mm7, %%mm1		\n\t" // common
		"movq  1024(%0, %%"REG_S"), %%mm2	\n\t"
		"movq  1032(%0, %%"REG_S"), %%mm3	\n\t"
		"movq  2048(%0, %%"REG_S"), %%mm4	\n\t"
		"movq  2056(%0, %%"REG_S"), %%mm5	\n\t"
		"pfadd %%mm0, %%mm2		\n\t"
		"pfadd %%mm1, %%mm3		\n\t"
		"pfadd %%mm0, %%mm4		\n\t"
		"pfadd %%mm1, %%mm5		\n\t"
		"movq  %%mm2, (%0, %%"REG_S")	\n\t"
		"movq  %%mm3, 8(%0, %%"REG_S")	\n\t"
		"movq  %%mm4, 1024(%0, %%"REG_S")	\n\t"
		"movq  %%mm5, 1032(%0, %%"REG_S")	\n\t"
		"addl $16, %%"REG_S"		\n\t"
		" jnz 1b			\n\t"
		"femms"
	:: "r" (samples+256), "m" (bias)
	: "%"REG_S""
	, "cc" /* conditional flags Jxx CMOVxx */
#ifdef FPU_CLOBBERED
	,FPU_CLOBBERED
#endif
#ifdef MMX_CLOBBERED
	,MMX_CLOBBERED
#endif
	);
#else
    int i;
    sample_t common;

    for (i = 0; i < 256; i++) {
	common = BIAS (samples[i] + samples[i + 768]);
	samples[i] = samples[i + 256] + common;
	samples[i + 256] = samples[i + 512] + common;
    }
#endif
}

static void RENAME(mix31toS) (sample_t * samples, sample_t bias)
{
#ifdef HAVE_SSE
	asm volatile(
		"movlps %1, %%xmm7		\n\t"
		"shufps $0x00, %%xmm7, %%xmm7	\n\t"
		"movl $-1024, %%"REG_S"		\n\t"
		".balign 16\n\t"
		"1:				\n\t"
		"movaps (%0, %%"REG_S"), %%xmm0	\n\t"
		"movaps 3072(%0, %%"REG_S"), %%xmm3	\n\t" // surround
		"addps %%xmm7, %%xmm0		\n\t" // common
		"movaps 1024(%0, %%"REG_S"), %%xmm1	\n\t"
		"movaps 2048(%0, %%"REG_S"), %%xmm2	\n\t"
		"addps %%xmm0, %%xmm1		\n\t"
		"addps %%xmm0, %%xmm2		\n\t"
		"subps %%xmm3, %%xmm1		\n\t"
		"addps %%xmm3, %%xmm2		\n\t"
		"movaps %%xmm1, (%0, %%"REG_S")	\n\t"
		"movaps %%xmm2, 1024(%0, %%"REG_S")	\n\t"
		"addl $16, %%"REG_S"		\n\t"
		" jnz 1b			\n\t"
	:: "r" (samples+256), "m" (bias)
	: "%"REG_S""
	, "cc" /* conditional flags Jxx CMOVxx */
#ifdef SSE_CLOBBERED
	,SSE_CLOBBERED
#endif
	);
#elif defined(HAVE_3DNOW)
	asm volatile(
		"movd  %1, %%mm7	\n\t"
		"punpckldq %1, %%mm7	\n\t"
		"movl $-1024, %%"REG_S"	\n\t"
		".balign 16\n\t"
		"1:			\n\t"
		"movq  (%0, %%"REG_S"), %%mm0	\n\t"
		"movq  8(%0, %%"REG_S"), %%mm1	\n\t"
		"pfadd 3072(%0, %%"REG_S"), %%mm0	\n\t"
		"pfadd 3080(%0, %%"REG_S"), %%mm1	\n\t"
		"pfadd %%mm7, %%mm0		\n\t" // common
		"pfadd %%mm7, %%mm1		\n\t" // common
		"movq  1024(%0, %%"REG_S"), %%mm2	\n\t"
		"movq  1032(%0, %%"REG_S"), %%mm3	\n\t"
		"movq  2048(%0, %%"REG_S"), %%mm4	\n\t"
		"movq  2056(%0, %%"REG_S"), %%mm5	\n\t"
		"pfadd %%mm0, %%mm2		\n\t"
		"pfadd %%mm1, %%mm3		\n\t"
		"pfadd %%mm0, %%mm4		\n\t"
		"pfadd %%mm1, %%mm5		\n\t"
		"movq  %%mm2, (%0, %%"REG_S")	\n\t"
		"movq  %%mm3, 8(%0, %%"REG_S")	\n\t"
		"movq  %%mm4, 1024(%0, %%"REG_S")	\n\t"
		"movq  %%mm5, 1032(%0, %%"REG_S")	\n\t"
		"addl $16, %%"REG_S"		\n\t"
		" jnz 1b			\n\t"
		"femms"
	:: "r" (samples+256), "m" (bias)
	: "%"REG_S""
	, "cc" /* conditional flags Jxx CMOVxx */
#ifdef FPU_CLOBBERED
	,FPU_CLOBBERED
#endif
#ifdef MMX_CLOBBERED
	,MMX_CLOBBERED
#endif
	);
#else
    int i;
    sample_t common, surround;

    for (i = 0; i < 256; i++) {
	common = BIAS (samples[i]);
	surround = samples[i + 768];
	samples[i] = samples[i + 256] + common - surround;
	samples[i + 256] = samples[i + 512] + common + surround;
    }
#endif
}

static void RENAME(mix22toS) (sample_t * samples, sample_t bias)
{
#ifdef HAVE_SSE
	asm volatile(
		"movlps %1, %%xmm7		\n\t"
		"shufps $0x00, %%xmm7, %%xmm7	\n\t"
		"movl $-1024, %%"REG_S"		\n\t"
		".balign 16\n\t"
		"1:				\n\t"
		"movaps 2048(%0, %%"REG_S"), %%xmm0	\n\t"
		"addps 3072(%0, %%"REG_S"), %%xmm0	\n\t" // surround
		"movaps (%0, %%"REG_S"), %%xmm1	\n\t"
		"movaps 1024(%0, %%"REG_S"), %%xmm2	\n\t"
		"addps %%xmm7, %%xmm1		\n\t"
		"addps %%xmm7, %%xmm2		\n\t"
		"subps %%xmm0, %%xmm1		\n\t"
		"addps %%xmm0, %%xmm2		\n\t"
		"movaps %%xmm1, (%0, %%"REG_S")	\n\t"
		"movaps %%xmm2, 1024(%0, %%"REG_S")	\n\t"
		"addl $16, %%"REG_S"		\n\t"
		" jnz 1b			\n\t"
	:: "r" (samples+256), "m" (bias)
	: "%"REG_S""
	, "cc" /* conditional flags Jxx CMOVxx */
#ifdef SSE_CLOBBERED
	,SSE_CLOBBERED
#endif
	);
#elif defined( HAVE_3DNOW )
	asm volatile(
		"movd  %1, %%mm7	\n\t"
		"punpckldq %1, %%mm7	\n\t"
		"movl $-1024, %%"REG_S"	\n\t"
		".balign 16\n\t"
		"1:			\n\t"
		"movq  2048(%0, %%"REG_S"), %%mm0	\n\t"
		"movq  2056(%0, %%"REG_S"), %%mm1	\n\t"
		"pfadd 3072(%0, %%"REG_S"), %%mm0	\n\t" // surround
		"pfadd 3080(%0, %%"REG_S"), %%mm1	\n\t" // surround
		"movq  (%0, %%"REG_S"), %%mm2	\n\t"
		"movq  8(%0, %%"REG_S"), %%mm3	\n\t"
		"movq  1024(%0, %%"REG_S"), %%mm4	\n\t"
		"movq  1032(%0, %%"REG_S"), %%mm5	\n\t"
		"pfadd %%mm7, %%mm2		\n\t"
		"pfadd %%mm7, %%mm3		\n\t"
		"pfadd %%mm7, %%mm4		\n\t"
		"pfadd %%mm7, %%mm5		\n\t"
		"pfsub %%mm0, %%mm2		\n\t"
		"pfsub %%mm1, %%mm3		\n\t"
		"pfadd %%mm0, %%mm4		\n\t"
		"pfadd %%mm1, %%mm5		\n\t"
		"movq  %%mm2, (%0, %%"REG_S")	\n\t"
		"movq  %%mm3, 8(%0, %%"REG_S")	\n\t"
		"movq  %%mm4, 1024(%0, %%"REG_S")	\n\t"
		"movq  %%mm5, 1032(%0, %%"REG_S")	\n\t"
		"addl $16, %%"REG_S"		\n\t"
		" jnz 1b			\n\t"
		"femms"
	:: "r" (samples+256), "m" (bias)
	: "%"REG_S""
	, "cc" /* conditional flags Jxx CMOVxx */
#ifdef FPU_CLOBBERED
	,FPU_CLOBBERED
#endif
#ifdef MMX_CLOBBERED
	,MMX_CLOBBERED
#endif
	);
#else
    int i;
    sample_t surround;

    for (i = 0; i < 256; i++) {
	surround = samples[i + 512] + samples[i + 768];
	samples[i] += BIAS (-surround);
	samples[i + 256] += BIAS (surround);
    }
#endif
}

static void RENAME(mix32to2) (sample_t * samples, sample_t bias)
{
#ifdef HAVE_SSE
	asm volatile(
	"movlps %1, %%xmm7		\n\t"
	"shufps $0x00, %%xmm7, %%xmm7	\n\t"
	"movl $-1024, %%"REG_S"		\n\t"
	".balign 16\n\t"
	"1:				\n\t"
	"movaps (%0, %%"REG_S"), %%xmm0	\n\t"
	"addps %%xmm7, %%xmm0		\n\t" // common
	"movaps %%xmm0, %%xmm1		\n\t" // common
	"addps 1024(%0, %%"REG_S"), %%xmm0	\n\t"
	"addps 2048(%0, %%"REG_S"), %%xmm1	\n\t"
	"addps 3072(%0, %%"REG_S"), %%xmm0	\n\t"
	"addps 4096(%0, %%"REG_S"), %%xmm1	\n\t"
	"movaps %%xmm0, (%0, %%"REG_S")	\n\t"
	"movaps %%xmm1, 1024(%0, %%"REG_S")	\n\t"
	"addl $16, %%"REG_S"		\n\t"
	" jnz 1b			\n\t"
	:: "r" (samples+256), "m" (bias)
	: "%"REG_S""
	, "cc" /* conditional flags Jxx CMOVxx */
#ifdef SSE_CLOBBERED
	,SSE_CLOBBERED
#endif
	);
#elif defined( HAVE_3DNOW )
	asm volatile(
	"movd  %1, %%mm7	\n\t"
	"punpckldq %1, %%mm7	\n\t"
	"movl $-1024, %%"REG_S"	\n\t"
	".balign 16\n\t"
	"1:			\n\t"
	"movq   (%0, %%"REG_S"), %%mm0	\n\t"
	"movq   8(%0, %%"REG_S"), %%mm1	\n\t"
	"pfadd  %%mm7, %%mm0		\n\t" // common
	"pfadd  %%mm7, %%mm1		\n\t" // common
	"movq   %%mm0, %%mm2		\n\t" // common
	"movq   %%mm1, %%mm3		\n\t" // common
	"pfadd  1024(%0, %%"REG_S"), %%mm0	\n\t"
	"pfadd  1032(%0, %%"REG_S"), %%mm1	\n\t"
	"pfadd  2048(%0, %%"REG_S"), %%mm2	\n\t"
	"pfadd  2056(%0, %%"REG_S"), %%mm3	\n\t"
	"pfadd  3072(%0, %%"REG_S"), %%mm0	\n\t"
	"pfadd  3080(%0, %%"REG_S"), %%mm1	\n\t"
	"pfadd  4096(%0, %%"REG_S"), %%mm2	\n\t"
	"pfadd  4104(%0, %%"REG_S"), %%mm3	\n\t"
	"movq   %%mm0, (%0, %%"REG_S")	\n\t"
	"movq   %%mm1, 8(%0, %%"REG_S")	\n\t"
	"movq   %%mm2, 1024(%0, %%"REG_S")	\n\t"
	"movq   %%mm3, 1032(%0, %%"REG_S")	\n\t"
	"addl $16, %%"REG_S"		\n\t"
	" jnz 1b			\n\t"
	"femms"
	:: "r" (samples+256), "m" (bias)
	: "%"REG_S""
	, "cc" /* conditional flags Jxx CMOVxx */
#ifdef FPU_CLOBBERED
	,FPU_CLOBBERED
#endif
#ifdef MMX_CLOBBERED
	,MMX_CLOBBERED
#endif
	);
#else
    int i;
    sample_t common;

    for (i = 0; i < 256; i++) {
	common = BIAS (samples[i]);
	samples[i] = common + samples[i + 256] + samples[i + 768];
	samples[i + 256] = common + samples[i + 512] + samples[i + 1024];
    }
#endif
}

static void RENAME(mix32toS) (sample_t * samples, sample_t bias)
{
#ifdef HAVE_SSE
	asm volatile(
	"movlps %1, %%xmm7		\n\t"
	"shufps $0x00, %%xmm7, %%xmm7	\n\t"
	"movl $-1024, %%"REG_S"		\n\t"
	".balign 16\n\t"
	"1:				\n\t"
	"movaps (%0, %%"REG_S"), %%xmm0	\n\t"
	"movaps 3072(%0, %%"REG_S"), %%xmm2	\n\t"
	"addps %%xmm7, %%xmm0		\n\t" // common
	"addps 4096(%0, %%"REG_S"), %%xmm2	\n\t" // surround
	"movaps 1024(%0, %%"REG_S"), %%xmm1	\n\t"
	"movaps 2048(%0, %%"REG_S"), %%xmm3	\n\t"
	"subps %%xmm2, %%xmm1		\n\t"
	"addps %%xmm2, %%xmm3		\n\t"
	"addps %%xmm0, %%xmm1		\n\t"
	"addps %%xmm0, %%xmm3		\n\t"
	"movaps %%xmm1, (%0, %%"REG_S")	\n\t"
	"movaps %%xmm3, 1024(%0, %%"REG_S")	\n\t"
	"addl $16, %%"REG_S"		\n\t"
	" jnz 1b			\n\t"
	:: "r" (samples+256), "m" (bias)
	: "%"REG_S""
	, "cc" /* conditional flags Jxx CMOVxx */
#ifdef SSE_CLOBBERED
	,SSE_CLOBBERED
#endif
	);
#elif defined(HAVE_3DNOW)
	asm volatile(
	"movl $-1024, %%"REG_S"	\n\t"
	".balign 16\n\t"
	"1:			\n\t"
	"movd  %1, %%mm7		\n\t"
	"punpckldq %1, %%mm7		\n\t"
	"movq  (%0, %%"REG_S"), %%mm0	\n\t"
	"movq  8(%0, %%"REG_S"), %%mm1	\n\t"
	"movq  3072(%0, %%"REG_S"), %%mm4	\n\t"
	"movq  3080(%0, %%"REG_S"), %%mm5	\n\t"
	"pfadd %%mm7, %%mm0		\n\t" // common
	"pfadd %%mm7, %%mm1		\n\t" // common
	"pfadd 4096(%0, %%"REG_S"), %%mm4	\n\t" // surround
	"pfadd 4104(%0, %%"REG_S"), %%mm5	\n\t" // surround
	"movq  1024(%0, %%"REG_S"), %%mm2	\n\t"
	"movq  1032(%0, %%"REG_S"), %%mm3	\n\t"
	"movq  2048(%0, %%"REG_S"), %%mm6	\n\t"
	"movq  2056(%0, %%"REG_S"), %%mm7	\n\t"
	"pfsub %%mm4, %%mm2		\n\t"
	"pfsub %%mm5, %%mm3		\n\t"
	"pfadd %%mm4, %%mm6		\n\t"
	"pfadd %%mm5, %%mm7		\n\t"
	"pfadd %%mm0, %%mm2		\n\t"
	"pfadd %%mm1, %%mm3		\n\t"
	"pfadd %%mm0, %%mm6		\n\t"
	"pfadd %%mm1, %%mm7		\n\t"
	"movq  %%mm2, (%0, %%"REG_S")	\n\t"
	"movq  %%mm3, 8(%0, %%"REG_S")	\n\t"
	"movq  %%mm6, 1024(%0, %%"REG_S")	\n\t"
	"movq  %%mm7, 1032(%0, %%"REG_S")	\n\t"
	"addl $16, %%"REG_S"		\n\t"
	" jnz 1b			\n\t"
	"femms"
	:: "r" (samples+256), "m" (bias)
	: "%"REG_S""
	, "cc" /* conditional flags Jxx CMOVxx */
#ifdef FPU_CLOBBERED
	,FPU_CLOBBERED
#endif
#ifdef MMX_CLOBBERED
	,MMX_CLOBBERED
#endif
	);
#else
    int i;
    sample_t common, surround;

    for (i = 0; i < 256; i++) {
	common = BIAS (samples[i]);
	surround = samples[i + 768] + samples[i + 1024];
	samples[i] = samples[i + 256] + common - surround;
	samples[i + 256] = samples[i + 512] + common + surround;
    }
#endif
}

static void RENAME(move2to1) (sample_t * src, sample_t * dest, sample_t bias)
{
#ifdef HAVE_SSE
	asm volatile(
		"movlps %2, %%xmm7		\n\t"
		"shufps $0x00, %%xmm7, %%xmm7	\n\t"
		"movl $-1024, %%"REG_S"		\n\t"
		".balign 16\n\t"
		"1:				\n\t"
		"movaps (%0, %%"REG_S"), %%xmm0	\n\t"
		"movaps 16(%0, %%"REG_S"), %%xmm1	\n\t"
		"addps 1024(%0, %%"REG_S"), %%xmm0	\n\t"
		"addps 1040(%0, %%"REG_S"), %%xmm1	\n\t"
		"addps %%xmm7, %%xmm0		\n\t"
		"addps %%xmm7, %%xmm1		\n\t"
		"movaps %%xmm0, (%1, %%"REG_S")	\n\t"
		"movaps %%xmm1, 16(%1, %%"REG_S")	\n\t"
		"addl $32, %%"REG_S"		\n\t"
		" jnz 1b			\n\t"
	:: "r" (src+256), "r" (dest+256), "m" (bias)
	: "%"REG_S""
	, "cc" /* conditional flags Jxx CMOVxx */
#ifdef SSE_CLOBBERED
	,SSE_CLOBBERED
#endif
	);
#elif defined(HAVE_3DNOW)
	asm volatile(
		"movd  %2, %%mm7	\n\t"
		"punpckldq %2, %%mm7	\n\t"
		"movl $-1024, %%"REG_S"	\n\t"
		".balign 16\n\t"
		"1:			\n\t"
		"movq  (%0, %%"REG_S"), %%mm0	\n\t"
		"movq  8(%0, %%"REG_S"), %%mm1	\n\t"
		"movq  16(%0, %%"REG_S"), %%mm2	\n\t"
		"movq  24(%0, %%"REG_S"), %%mm3	\n\t"
		"pfadd 1024(%0, %%"REG_S"), %%mm0	\n\t"
		"pfadd 1032(%0, %%"REG_S"), %%mm1	\n\t"
		"pfadd 1040(%0, %%"REG_S"), %%mm2	\n\t"
		"pfadd 1048(%0, %%"REG_S"), %%mm3	\n\t"
		"pfadd %%mm7, %%mm0		\n\t"
		"pfadd %%mm7, %%mm1		\n\t"
		"pfadd %%mm7, %%mm2		\n\t"
		"pfadd %%mm7, %%mm3		\n\t"
		"movq  %%mm0, (%1, %%"REG_S")	\n\t"
		"movq  %%mm1, 8(%1, %%"REG_S")	\n\t"
		"movq  %%mm2, 16(%1, %%"REG_S")	\n\t"
		"movq  %%mm3, 24(%1, %%"REG_S")	\n\t"
		"addl $32, %%"REG_S"		\n\t"
		" jnz 1b			\n\t"
		"femms"
	:: "r" (src+256), "r" (dest+256), "m" (bias)
	: "%"REG_S
	, "cc" /* conditional flags Jxx CMOVxx */
#ifdef FPU_CLOBBERED
	,FPU_CLOBBERED
#endif
#ifdef MMX_CLOBBERED
	,MMX_CLOBBERED
#endif
	);
#else
    int i;

    for (i = 0; i < 256; i++)
	dest[i] = BIAS (src[i] + src[i + 256]);
#endif
}
