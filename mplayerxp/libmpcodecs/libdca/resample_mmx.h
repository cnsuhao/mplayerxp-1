
// MMX optimizations from Michael Niedermayer (michaelni@gmx.at) (under GPL)

/* optimization TODO / NOTES
    movntq is slightly faster (0.5% with the current test.c benchmark)
	(but thats just test.c so that needs to be testd in reallity)
	and it would mean (C / MMX2 / MMX / 3DNOW) versions
*/

static uint64_t __attribute__((used)) __attribute__((aligned(8))) magicF2W= 0x43c0000043c00000LL;
static uint64_t __attribute__((used)) __attribute__((aligned(8))) wm1010= 0xFFFF0000FFFF0000LL;
static uint64_t __attribute__((used)) __attribute__((aligned(8))) wm0101= 0x0000FFFF0000FFFFLL;
static uint64_t __attribute__((used)) __attribute__((aligned(8))) wm1100= 0xFFFFFFFF00000000LL;

static int dca_resample_MONO_to_5_MMX(float * _f, int16_t * s16){
    int i;
    int32_t * f = (int32_t *) _f;
	asm volatile(
		"movl $-512, %%"REG_S"		\n\t"
		"movq "MANGLE(magicF2W)", %%mm7	\n\t"
		"movq "MANGLE(wm1100)", %%mm3	\n\t"
		"movq "MANGLE(wm0101)", %%mm4	\n\t"
		"movq "MANGLE(wm1010)", %%mm5	\n\t"
		"pxor %%mm6, %%mm6		\n\t"
		"1:				\n\t"
		"movq (%1, %%"REG_S", 2), %%mm0	\n\t"
		"movq 8(%1, %%"REG_S", 2), %%mm1	\n\t"
		"leal (%%"REG_S", %%"REG_S", 4), %%"REG_D"	\n\t"
		"psubd %%mm7, %%mm0		\n\t"
		"psubd %%mm7, %%mm1		\n\t"
		"packssdw %%mm1, %%mm0		\n\t"
		"movq %%mm0, %%mm1		\n\t"
		"pand %%mm4, %%mm0		\n\t"
		"pand %%mm5, %%mm1		\n\t"
		"movq %%mm6, (%0, %%"REG_D")	\n\t" // 0 0 0 0
		"movd %%mm0, 8(%0, %%"REG_D")	\n\t" // A 0
		"pand %%mm3, %%mm0		\n\t"
		"movd %%mm6, 12(%0, %%"REG_D")	\n\t" // 0 0
		"movd %%mm1, 16(%0, %%"REG_D")	\n\t" // 0 B
		"pand %%mm3, %%mm1		\n\t"
		"movd %%mm6, 20(%0, %%"REG_D")	\n\t" // 0 0
		"movq %%mm0, 24(%0, %%"REG_D")	\n\t" // 0 0 C 0
		"movq %%mm1, 32(%0, %%"REG_D")	\n\t" // 0 0 0 B
		"addl $8, %%"REG_S"			\n\t"
		" jnz 1b			\n\t"
		"emms				\n\t"
		:: "r" (s16+1280), "r" (f+256)
		:"%"REG_S"", "%"REG_D"", "memory"
		, "cc" /* conditional flags Jxx CMOVxx */
#ifdef FPU_CLOBBERED
		,FPU_CLOBBERED
#endif
#ifdef MMX_CLOBBERED
		,MMX_CLOBBERED
#endif
	);
    return 5*256;
}

static int dca_resample_STEREO_to_2_MMX(float * _f, int16_t * s16){
    int i;
    int32_t * f = (int32_t *) _f;
/* benchmark scores are 0.3% better with SSE but we would need to set bias=0 and premultiply it
#ifdef HAVE_SSE
	asm volatile(
		"movl $-1024, %%"REG_S"		\n\t"
		"1:				\n\t"
		"cvtps2pi (%1, %%"REG_S"), %%mm0	\n\t"
		"cvtps2pi 1024(%1, %%"REG_S"), %%mm2\n\t"
		"movq %%mm0, %%mm1		\n\t"
		"punpcklwd %%mm2, %%mm0		\n\t"
		"punpckhwd %%mm2, %%mm1		\n\t"
		"movq %%mm0, (%0, %%"REG_S")	\n\t"
		"movq %%mm1, 8(%0, %%"REG_S")	\n\t"
		"addl $16, %%"REG_S"		\n\t"
		" jnz 1b			\n\t"
		"emms				\n\t"
		:: "r" (s16+512), "r" (f+256)
		:"%"REG_S"", "memory"
	);*/
	asm volatile(
		"movl $-1024, %%"REG_S"		\n\t"
		"movq "MANGLE(magicF2W)", %%mm7	\n\t"
		"1:				\n\t"
		"movq (%1, %%"REG_S"), %%mm0	\n\t"
		"movq 8(%1, %%"REG_S"), %%mm1	\n\t"
		"movq 1024(%1, %%"REG_S"), %%mm2	\n\t"
		"movq 1032(%1, %%"REG_S"), %%mm3	\n\t"
		"psubd %%mm7, %%mm0		\n\t"
		"psubd %%mm7, %%mm1		\n\t"
		"psubd %%mm7, %%mm2		\n\t"
		"psubd %%mm7, %%mm3		\n\t"
		"packssdw %%mm1, %%mm0		\n\t"
		"packssdw %%mm3, %%mm2		\n\t"
		"movq %%mm0, %%mm1		\n\t"
		"punpcklwd %%mm2, %%mm0		\n\t"
		"punpckhwd %%mm2, %%mm1		\n\t"
		"movq %%mm0, (%0, %%"REG_S")	\n\t"
		"movq %%mm1, 8(%0, %%"REG_S")	\n\t"
		"addl $16, %%"REG_S"		\n\t"
		" jnz 1b			\n\t"
		"emms				\n\t"
		:: "r" (s16+512), "r" (f+256)
		:"%"REG_S"", "memory"
		, "cc" /* conditional flags Jxx CMOVxx */
#ifdef FPU_CLOBBERED
		,FPU_CLOBBERED
#endif
#ifdef MMX_CLOBBERED
		,MMX_CLOBBERED
#endif
	);
    return 2*256;
}

static int dca_resample_3F_to_5_MMX(float * _f, int16_t * s16){
    int i;
    int32_t * f = (int32_t *) _f;
	asm volatile(
		"movl $-1024, %%"REG_S"		\n\t"
		"movq "MANGLE(magicF2W)", %%mm7	\n\t"
		"pxor %%mm6, %%mm6		\n\t"
		"movq %%mm7, %%mm5		\n\t"
		"punpckldq %%mm6, %%mm5		\n\t"
		"1:				\n\t"
		"movd (%1, %%"REG_S"), %%mm0	\n\t"
		"punpckldq 2048(%1, %%"REG_S"), %%mm0\n\t"
		"movd 1024(%1, %%"REG_S"), %%mm1	\n\t"
		"punpckldq 4(%1, %%"REG_S"), %%mm1	\n\t"
		"movd 2052(%1, %%"REG_S"), %%mm2	\n\t"
		"movq %%mm7, %%mm3		\n\t"
		"punpckldq 1028(%1, %%"REG_S"), %%mm3\n\t"
		"movd 8(%1, %%"REG_S"), %%mm4	\n\t"
		"punpckldq 2056(%1, %%"REG_S"), %%mm4\n\t"
		"leal (%%"REG_S", %%"REG_S", 4), %%"REG_D"	\n\t"
		"sarl $1, %%"REG_D"			\n\t"
		"psubd %%mm7, %%mm0		\n\t"
		"psubd %%mm7, %%mm1		\n\t"
		"psubd %%mm5, %%mm2		\n\t"
		"psubd %%mm7, %%mm3		\n\t"
		"psubd %%mm7, %%mm4		\n\t"
		"packssdw %%mm6, %%mm0		\n\t"
		"packssdw %%mm2, %%mm1		\n\t"
		"packssdw %%mm4, %%mm3		\n\t"
		"movq %%mm0, (%0, %%"REG_D")	\n\t"
		"movq %%mm1, 8(%0, %%"REG_D")	\n\t"
		"movq %%mm3, 16(%0, %%"REG_D")	\n\t"

		"movd 1032(%1, %%"REG_S"), %%mm1	\n\t"
		"punpckldq 12(%1, %%"REG_S"), %%mm1\n\t"
		"movd 2060(%1, %%"REG_S"), %%mm2	\n\t"
		"movq %%mm7, %%mm3		\n\t"
		"punpckldq 1036(%1, %%"REG_S"), %%mm3\n\t"
		"pxor %%mm0, %%mm0		\n\t"
		"psubd %%mm7, %%mm1		\n\t"
		"psubd %%mm5, %%mm2		\n\t"
		"psubd %%mm7, %%mm3		\n\t"
		"packssdw %%mm1, %%mm0		\n\t"
		"packssdw %%mm3, %%mm2		\n\t"
		"movq %%mm0, 24(%0, %%"REG_D")	\n\t"
		"movq %%mm2, 32(%0, %%"REG_D")	\n\t"

		"addl $16, %%"REG_S"		\n\t"
		" jnz 1b			\n\t"
		"emms				\n\t"
		:: "r" (s16+1280), "r" (f+256)
		:"%"REG_S"", "%"REG_D"", "memory"
		, "cc" /* conditional flags Jxx CMOVxx */
#ifdef FPU_CLOBBERED
		,FPU_CLOBBERED
#endif
#ifdef MMX_CLOBBERED
		,MMX_CLOBBERED
#endif
	);
    return 5*256;
}

static int dca_resample_2F_2R_to_4_MMX(float * _f, int16_t * s16){
    int i;
    int32_t * f = (int32_t *) _f;
	asm volatile(
		"movl $-1024, %%"REG_S"		\n\t"
		"movq "MANGLE(magicF2W)", %%mm7	\n\t"
		"1:				\n\t"
		"movq (%1, %%"REG_S"), %%mm0	\n\t"
		"movq 8(%1, %%"REG_S"), %%mm1	\n\t"
		"movq 1024(%1, %%"REG_S"), %%mm2	\n\t"
		"movq 1032(%1, %%"REG_S"), %%mm3	\n\t"
		"psubd %%mm7, %%mm0		\n\t"
		"psubd %%mm7, %%mm1		\n\t"
		"psubd %%mm7, %%mm2		\n\t"
		"psubd %%mm7, %%mm3		\n\t"
		"packssdw %%mm1, %%mm0		\n\t"
		"packssdw %%mm3, %%mm2		\n\t"
		"movq 2048(%1, %%"REG_S"), %%mm3	\n\t"
		"movq 2056(%1, %%"REG_S"), %%mm4	\n\t"
		"movq 3072(%1, %%"REG_S"), %%mm5	\n\t"
		"movq 3080(%1, %%"REG_S"), %%mm6	\n\t"
		"psubd %%mm7, %%mm3		\n\t"
		"psubd %%mm7, %%mm4		\n\t"
		"psubd %%mm7, %%mm5		\n\t"
		"psubd %%mm7, %%mm6		\n\t"
		"packssdw %%mm4, %%mm3		\n\t"
		"packssdw %%mm6, %%mm5		\n\t"
		"movq %%mm0, %%mm1		\n\t"
		"movq %%mm3, %%mm4		\n\t"
		"punpcklwd %%mm2, %%mm0		\n\t"
		"punpckhwd %%mm2, %%mm1		\n\t"
		"punpcklwd %%mm5, %%mm3		\n\t"
		"punpckhwd %%mm5, %%mm4		\n\t"
		"movq %%mm0, %%mm2		\n\t"
		"movq %%mm1, %%mm5		\n\t"
		"punpckldq %%mm3, %%mm0		\n\t"
		"punpckhdq %%mm3, %%mm2		\n\t"
		"punpckldq %%mm4, %%mm1		\n\t"
		"punpckhdq %%mm4, %%mm5		\n\t"
		"movq %%mm0, (%0, %%"REG_S",2)	\n\t"
		"movq %%mm2, 8(%0, %%"REG_S",2)	\n\t"
		"movq %%mm1, 16(%0, %%"REG_S",2)	\n\t"
		"movq %%mm5, 24(%0, %%"REG_S",2)	\n\t"
		"addl $16, %%"REG_S"		\n\t"
		" jnz 1b			\n\t"
		"emms				\n\t"
		:: "r" (s16+1024), "r" (f+256)
		:"%"REG_S"", "memory"
		, "cc" /* conditional flags Jxx CMOVxx */
#ifdef FPU_CLOBBERED
		,FPU_CLOBBERED
#endif
#ifdef MMX_CLOBBERED
		,MMX_CLOBBERED
#endif
	);
    return 4*256;
}

static int dca_resample_3F_2R_to_5_MMX(float * _f, int16_t * s16){
    int i;
    int32_t * f = (int32_t *) _f;
	asm volatile(
		"movl $-1024, %%"REG_S"		\n\t"
		"movq "MANGLE(magicF2W)", %%mm7	\n\t"
		"1:				\n\t"
		"movd (%1, %%"REG_S"), %%mm0	\n\t"
		"punpckldq 2048(%1, %%"REG_S"), %%mm0\n\t"
		"movd 3072(%1, %%"REG_S"), %%mm1	\n\t"
		"punpckldq 4096(%1, %%"REG_S"), %%mm1\n\t"
		"movd 1024(%1, %%"REG_S"), %%mm2	\n\t"
		"punpckldq 4(%1, %%"REG_S"), %%mm2	\n\t"
		"movd 2052(%1, %%"REG_S"), %%mm3	\n\t"
		"punpckldq 3076(%1, %%"REG_S"), %%mm3\n\t"
		"movd 4100(%1, %%"REG_S"), %%mm4	\n\t"
		"punpckldq 1028(%1, %%"REG_S"), %%mm4\n\t"
		"movd 8(%1, %%"REG_S"), %%mm5	\n\t"
		"punpckldq 2056(%1, %%"REG_S"), %%mm5\n\t"
		"leal (%%"REG_S", %%"REG_S", 4), %%"REG_D"	\n\t"
		"sarl $1, %%"REG_D"			\n\t"
		"psubd %%mm7, %%mm0		\n\t"
		"psubd %%mm7, %%mm1		\n\t"
		"psubd %%mm7, %%mm2		\n\t"
		"psubd %%mm7, %%mm3		\n\t"
		"psubd %%mm7, %%mm4		\n\t"
		"psubd %%mm7, %%mm5		\n\t"
		"packssdw %%mm1, %%mm0		\n\t"
		"packssdw %%mm3, %%mm2		\n\t"
		"packssdw %%mm5, %%mm4		\n\t"
		"movq %%mm0, (%0, %%"REG_D")	\n\t"
		"movq %%mm2, 8(%0, %%"REG_D")	\n\t"
		"movq %%mm4, 16(%0, %%"REG_D")	\n\t"

		"movd 3080(%1, %%"REG_S"), %%mm0	\n\t"
		"punpckldq 4104(%1, %%"REG_S"), %%mm0\n\t"
		"movd 1032(%1, %%"REG_S"), %%mm1	\n\t"
		"punpckldq 12(%1, %%"REG_S"), %%mm1\n\t"
		"movd 2060(%1, %%"REG_S"), %%mm2	\n\t"
		"punpckldq 3084(%1, %%"REG_S"), %%mm2\n\t"
		"movd 4108(%1, %%"REG_S"), %%mm3	\n\t"
		"punpckldq 1036(%1, %%"REG_S"), %%mm3\n\t"
		"psubd %%mm7, %%mm0		\n\t"
		"psubd %%mm7, %%mm1		\n\t"
		"psubd %%mm7, %%mm2		\n\t"
		"psubd %%mm7, %%mm3		\n\t"
		"packssdw %%mm1, %%mm0		\n\t"
		"packssdw %%mm3, %%mm2		\n\t"
		"movq %%mm0, 24(%0, %%"REG_D")	\n\t"
		"movq %%mm2, 32(%0, %%"REG_D")	\n\t"

		"addl $16, %%"REG_S"		\n\t"
		" jnz 1b			\n\t"
		"emms				\n\t"
		:: "r" (s16+1280), "r" (f+256)
		:"%"REG_S"", "%"REG_D"", "memory"
		, "cc" /* conditional flags Jxx CMOVxx */
#ifdef FPU_CLOBBERED
		,FPU_CLOBBERED
#endif
#ifdef MMX_CLOBBERED
		,MMX_CLOBBERED
#endif
	);
    return 5*256;
}

static int dca_resample_MONO_LFE_to_6_MMX(float * _f, int16_t * s16){
    int i;
    int32_t * f = (int32_t *) _f;
	asm volatile(
		"movl $-1024, %%"REG_S"		\n\t"
		"movq "MANGLE(magicF2W)", %%mm7	\n\t"
		"pxor %%mm6, %%mm6		\n\t"
		"1:				\n\t"
		"movq 1024(%1, %%"REG_S"), %%mm0	\n\t"
		"movq 1032(%1, %%"REG_S"), %%mm1	\n\t"
		"movq (%1, %%"REG_S"), %%mm2	\n\t"
		"movq 8(%1, %%"REG_S"), %%mm3	\n\t"
		"psubd %%mm7, %%mm0		\n\t"
		"psubd %%mm7, %%mm1		\n\t"
		"psubd %%mm7, %%mm2		\n\t"
		"psubd %%mm7, %%mm3		\n\t"
		"packssdw %%mm1, %%mm0		\n\t"
		"packssdw %%mm3, %%mm2		\n\t"
		"movq %%mm0, %%mm1		\n\t"
		"punpcklwd %%mm2, %%mm0		\n\t"
		"punpckhwd %%mm2, %%mm1		\n\t"
		"leal (%%"REG_S", %%"REG_S", 2), %%"REG_D"	\n\t"
		"movq %%mm6, (%0, %%"REG_D")	\n\t"
		"movd %%mm0, 8(%0, %%"REG_D")	\n\t"
		"punpckhdq %%mm0, %%mm0		\n\t"
		"movq %%mm6, 12(%0, %%"REG_D")	\n\t"
		"movd %%mm0, 20(%0, %%"REG_D")	\n\t"
		"movq %%mm6, 24(%0, %%"REG_D")	\n\t"
		"movd %%mm1, 32(%0, %%"REG_D")	\n\t"
		"punpckhdq %%mm1, %%mm1		\n\t"
		"movq %%mm6, 36(%0, %%"REG_D")	\n\t"
		"movd %%mm1, 44(%0, %%"REG_D")	\n\t"
		"addl $16, %%"REG_S"		\n\t"
		" jnz 1b			\n\t"
		"emms				\n\t"
		:: "r" (s16+1536), "r" (f+256)
		:"%"REG_S"", "%"REG_D"", "memory"
		, "cc" /* conditional flags Jxx CMOVxx */
#ifdef FPU_CLOBBERED
		,FPU_CLOBBERED
#endif
#ifdef MMX_CLOBBERED
		,MMX_CLOBBERED
#endif
	);
    return 6*256;
}

static int dca_resample_STEREO_LFE_to_6_MMX(float * _f, int16_t * s16){
    int i;
    int32_t * f = (int32_t *) _f;
	asm volatile(
		"movl $-1024, %%"REG_S"		\n\t"
		"movq "MANGLE(magicF2W)", %%mm7	\n\t"
		"pxor %%mm6, %%mm6		\n\t"
		"1:				\n\t"
		"movq 1024(%1, %%"REG_S"), %%mm0	\n\t"
		"movq 2048(%1, %%"REG_S"), %%mm1	\n\t"
		"movq (%1, %%"REG_S"), %%mm5	\n\t"
		"psubd %%mm7, %%mm0		\n\t"
		"psubd %%mm7, %%mm1		\n\t"
		"psubd %%mm7, %%mm5		\n\t"
		"leal (%%"REG_S", %%"REG_S", 2), %%"REG_D"	\n\t"

		"pxor %%mm4, %%mm4		\n\t"
		"packssdw %%mm5, %%mm0		\n\t" // FfAa
		"packssdw %%mm4, %%mm1		\n\t" // 00Bb
		"punpckhwd %%mm0, %%mm4		\n\t" // F0f0
		"punpcklwd %%mm1, %%mm0		\n\t" // BAba
		"movq %%mm0, %%mm1		\n\t" // BAba
		"punpckldq %%mm4, %%mm3		\n\t" // f0XX
		"punpckldq %%mm6, %%mm0		\n\t" // 00ba
		"punpckhdq %%mm1, %%mm3		\n\t" // BAf0

		"movq %%mm0, (%0, %%"REG_D")	\n\t" // 00ba
		"punpckhdq %%mm4, %%mm0		\n\t" // F000
		"movq %%mm3, 8(%0, %%"REG_D")	\n\t" // BAf0
		"movq %%mm0, 16(%0, %%"REG_D")	\n\t" // F000
		"addl $8, %%"REG_S"			\n\t"
		" jnz 1b			\n\t"
		"emms				\n\t"
		:: "r" (s16+1536), "r" (f+256)
		:"%"REG_S"", "%"REG_D"", "memory"
		, "cc" /* conditional flags Jxx CMOVxx */
#ifdef FPU_CLOBBERED
		,FPU_CLOBBERED
#endif
#ifdef MMX_CLOBBERED
		,MMX_CLOBBERED
#endif
	);
    return 6*256;
}

static int dca_resample_3F_LFE_to_6_MMX(float * _f, int16_t * s16){
    int i;
    int32_t * f = (int32_t *) _f;
	asm volatile(
		"movl $-1024, %%"REG_S"		\n\t"
		"movq "MANGLE(magicF2W)", %%mm7	\n\t"
		"pxor %%mm6, %%mm6		\n\t"
		"1:				\n\t"
		"movq 1024(%1, %%"REG_S"), %%mm0	\n\t"
		"movq 3072(%1, %%"REG_S"), %%mm1	\n\t"
		"movq 2048(%1, %%"REG_S"), %%mm4	\n\t"
		"movq (%1, %%"REG_S"), %%mm5	\n\t"
		"psubd %%mm7, %%mm0		\n\t"
		"psubd %%mm7, %%mm1		\n\t"
		"psubd %%mm7, %%mm4		\n\t"
		"psubd %%mm7, %%mm5		\n\t"
		"leal (%%"REG_S", %%"REG_S", 2), %%"REG_D"	\n\t"

		"packssdw %%mm4, %%mm0		\n\t" // EeAa
		"packssdw %%mm5, %%mm1		\n\t" // FfBb
		"movq %%mm0, %%mm2		\n\t" // EeAa
		"punpcklwd %%mm1, %%mm0		\n\t" // BAba
		"punpckhwd %%mm1, %%mm2		\n\t" // FEfe
		"movq %%mm0, %%mm1		\n\t" // BAba
		"punpckldq %%mm6, %%mm0		\n\t" // 00ba
		"punpckhdq %%mm1, %%mm1		\n\t" // BABA

		"movq %%mm0, (%0, %%"REG_D")	\n\t"
		"punpckhdq %%mm2, %%mm0		\n\t" // FE00
		"punpckldq %%mm1, %%mm2		\n\t" // BAfe
		"movq %%mm2, 8(%0, %%"REG_D")	\n\t"
		"movq %%mm0, 16(%0, %%"REG_D")	\n\t"
		"addl $8, %%"REG_S"			\n\t"
		" jnz 1b			\n\t"
		"emms				\n\t"
		:: "r" (s16+1536), "r" (f+256)
		:"%"REG_S"", "%"REG_D"", "memory"
		, "cc" /* conditional flags Jxx CMOVxx */
#ifdef FPU_CLOBBERED
		,FPU_CLOBBERED
#endif
#ifdef MMX_CLOBBERED
		,MMX_CLOBBERED
#endif
	);
    return 6*256;
}

static int dca_resample_2F_2R_LFE_to_6_MMX(float * _f, int16_t * s16){
    int i;
    int32_t * f = (int32_t *) _f;
	asm volatile(
		"movl $-1024, %%"REG_S"		\n\t"
		"movq "MANGLE(magicF2W)", %%mm7	\n\t"
//		"pxor %%mm6, %%mm6		\n\t"
		"1:				\n\t"
		"movq 1024(%1, %%"REG_S"), %%mm0	\n\t"
		"movq 2048(%1, %%"REG_S"), %%mm1	\n\t"
		"movq 3072(%1, %%"REG_S"), %%mm2	\n\t"
		"movq 4096(%1, %%"REG_S"), %%mm3	\n\t"
		"movq (%1, %%"REG_S"), %%mm5	\n\t"
		"psubd %%mm7, %%mm0		\n\t"
		"psubd %%mm7, %%mm1		\n\t"
		"psubd %%mm7, %%mm2		\n\t"
		"psubd %%mm7, %%mm3		\n\t"
		"psubd %%mm7, %%mm5		\n\t"
		"leal (%%"REG_S", %%"REG_S", 2), %%"REG_D"	\n\t"

		"packssdw %%mm2, %%mm0		\n\t" // CcAa
		"packssdw %%mm3, %%mm1		\n\t" // DdBb
		"packssdw %%mm5, %%mm5		\n\t" // FfFf
		"movq %%mm0, %%mm2		\n\t" // CcAa
		"punpcklwd %%mm1, %%mm0		\n\t" // BAba
		"punpckhwd %%mm1, %%mm2		\n\t" // DCdc
		"pxor %%mm4, %%mm4		\n\t" // 0000
		"punpcklwd %%mm5, %%mm4		\n\t" // F0f0
		"movq %%mm0, %%mm1		\n\t" // BAba
		"movq %%mm4, %%mm3		\n\t" // F0f0
		"punpckldq %%mm2, %%mm0		\n\t" // dcba
		"punpckhdq %%mm1, %%mm1		\n\t" // BABA
		"punpckldq %%mm1, %%mm4		\n\t" // BAf0
		"punpckhdq %%mm3, %%mm2		\n\t" // F0DC

		"movq %%mm0, (%0, %%"REG_D")	\n\t"
		"movq %%mm4, 8(%0, %%"REG_D")	\n\t"
		"movq %%mm2, 16(%0, %%"REG_D")	\n\t"
		"addl $8, %%"REG_S"			\n\t"
		" jnz 1b			\n\t"
		"emms				\n\t"
		:: "r" (s16+1536), "r" (f+256)
		:"%"REG_S"", "%"REG_D"", "memory"
		, "cc" /* conditional flags Jxx CMOVxx */
#ifdef FPU_CLOBBERED
		,FPU_CLOBBERED
#endif
#ifdef MMX_CLOBBERED
		,MMX_CLOBBERED
#endif
	);
    return 6*256;
}

static int dca_resample_3F_2R_LFE_to_6_MMX(float * _f, int16_t * s16){
    int i;
    int32_t * f = (int32_t *) _f;
	asm volatile(
		"movl $-1024, %%"REG_S"		\n\t"
		"movq "MANGLE(magicF2W)", %%mm7	\n\t"
//		"pxor %%mm6, %%mm6		\n\t"
		"1:				\n\t"
		"movq 1024(%1, %%"REG_S"), %%mm0	\n\t"
		"movq 3072(%1, %%"REG_S"), %%mm1	\n\t"
		"movq 4096(%1, %%"REG_S"), %%mm2	\n\t"
		"movq 5120(%1, %%"REG_S"), %%mm3	\n\t"
		"movq 2048(%1, %%"REG_S"), %%mm4	\n\t"
		"movq (%1, %%"REG_S"), %%mm5	\n\t"
		"psubd %%mm7, %%mm0		\n\t"
		"psubd %%mm7, %%mm1		\n\t"
		"psubd %%mm7, %%mm2		\n\t"
		"psubd %%mm7, %%mm3		\n\t"
		"psubd %%mm7, %%mm4		\n\t"
		"psubd %%mm7, %%mm5		\n\t"
		"leal (%%"REG_S", %%"REG_S", 2), %%"REG_D"	\n\t"

		"packssdw %%mm2, %%mm0		\n\t" // CcAa
		"packssdw %%mm3, %%mm1		\n\t" // DdBb
		"packssdw %%mm4, %%mm4		\n\t" // EeEe
		"packssdw %%mm5, %%mm5		\n\t" // FfFf
		"movq %%mm0, %%mm2		\n\t" // CcAa
		"punpcklwd %%mm1, %%mm0		\n\t" // BAba
		"punpckhwd %%mm1, %%mm2		\n\t" // DCdc
		"punpcklwd %%mm5, %%mm4		\n\t" // FEfe
		"movq %%mm0, %%mm1		\n\t" // BAba
		"movq %%mm4, %%mm3		\n\t" // FEfe
		"punpckldq %%mm2, %%mm0		\n\t" // dcba
		"punpckhdq %%mm1, %%mm1		\n\t" // BABA
		"punpckldq %%mm1, %%mm4		\n\t" // BAfe
		"punpckhdq %%mm3, %%mm2		\n\t" // FEDC

		"movq %%mm0, (%0, %%"REG_D")	\n\t"
		"movq %%mm4, 8(%0, %%"REG_D")	\n\t"
		"movq %%mm2, 16(%0, %%"REG_D")	\n\t"
		"addl $8, %%"REG_S"			\n\t"
		" jnz 1b			\n\t"
		"emms				\n\t"
		:: "r" (s16+1536), "r" (f+256)
		:"%"REG_S, "%"REG_D, "memory"
		, "cc" /* conditional flags Jxx CMOVxx */
#ifdef FPU_CLOBBERED
		,FPU_CLOBBERED
#endif
#ifdef MMX_CLOBBERED
		,MMX_CLOBBERED
#endif
	);
    return 6*256;
}


static any_t* dca_resample_MMX(dca_state_t * state,int flags, int ch){
    switch (flags) {
    case DCA_MONO:
	if(ch==5) return dca_resample_MONO_to_5_MMX;
	break;
    case DCA_CHANNEL:
    case DCA_STEREO:
    case DCA_DOLBY:
	if(ch==2) return dca_resample_STEREO_to_2_MMX;
	break;
    case DCA_3F:
	if(ch==5) return dca_resample_3F_to_5_MMX;
	break;
    case DCA_2F2R:
	if(ch==4) return dca_resample_2F_2R_to_4_MMX;
	break;
    case DCA_3F2R:
	if(ch==5) return dca_resample_3F_2R_to_5_MMX;
	break;
    case DCA_MONO | DCA_LFE:
	if(ch==6) return dca_resample_MONO_LFE_to_6_MMX;
	break;
    case DCA_CHANNEL | DCA_LFE:
    case DCA_STEREO | DCA_LFE:
    case DCA_DOLBY | DCA_LFE:
	if(ch==6) return dca_resample_STEREO_LFE_to_6_MMX;
	break;
    case DCA_3F | DCA_LFE:
	if(ch==6) return dca_resample_3F_LFE_to_6_MMX;
	break;
    case DCA_2F2R | DCA_LFE:
	if(ch==6) return dca_resample_2F_2R_LFE_to_6_MMX;
	break;
    case DCA_3F2R | DCA_LFE:
	if(ch==6) return dca_resample_3F_2R_LFE_to_6_MMX;
	break;
    }
    return NULL;
}


