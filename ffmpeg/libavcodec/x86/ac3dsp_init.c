/*
 * x86-optimized AC-3 DSP utils
 * Copyright (c) 2011 Justin Ruggles
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "libavutil/mem.h"
#include "libavutil/x86/asm.h"
#include "libavutil/x86/cpu.h"
#include "dsputil_mmx.h"
#include "libavcodec/ac3.h"
#include "libavcodec/ac3dsp.h"

extern void ff_ac3_exponent_min_mmx   (uint8_t *exp, int num_reuse_blocks, int nb_coefs);
extern void ff_ac3_exponent_min_mmxext(uint8_t *exp, int num_reuse_blocks, int nb_coefs);
extern void ff_ac3_exponent_min_sse2  (uint8_t *exp, int num_reuse_blocks, int nb_coefs);

extern int ff_ac3_max_msb_abs_int16_mmx  (const int16_t *src, int len);
extern int ff_ac3_max_msb_abs_int16_mmx2 (const int16_t *src, int len);
extern int ff_ac3_max_msb_abs_int16_sse2 (const int16_t *src, int len);
extern int ff_ac3_max_msb_abs_int16_ssse3(const int16_t *src, int len);

extern void ff_ac3_lshift_int16_mmx (int16_t *src, unsigned int len, unsigned int shift);
extern void ff_ac3_lshift_int16_sse2(int16_t *src, unsigned int len, unsigned int shift);

extern void ff_ac3_rshift_int32_mmx (int32_t *src, unsigned int len, unsigned int shift);
extern void ff_ac3_rshift_int32_sse2(int32_t *src, unsigned int len, unsigned int shift);

extern void ff_float_to_fixed24_3dnow(int32_t *dst, const float *src, unsigned int len);
extern void ff_float_to_fixed24_sse  (int32_t *dst, const float *src, unsigned int len);
extern void ff_float_to_fixed24_sse2 (int32_t *dst, const float *src, unsigned int len);

extern int ff_ac3_compute_mantissa_size_sse2(uint16_t mant_cnt[6][16]);

extern void ff_ac3_extract_exponents_3dnow(uint8_t *exp, int32_t *coef, int nb_coefs);
extern void ff_ac3_extract_exponents_sse2 (uint8_t *exp, int32_t *coef, int nb_coefs);
extern void ff_ac3_extract_exponents_ssse3(uint8_t *exp, int32_t *coef, int nb_coefs);

#if HAVE_SSE_INLINE

#define IF1(x) x
#define IF0(x)

#define MIX5(mono, stereo)                                      \
    __asm__ volatile (                                          \
        "movss           0(%2), %%xmm5          \n"             \
        "movss           8(%2), %%xmm6          \n"             \
        "movss          24(%2), %%xmm7          \n"             \
        "shufps     $0, %%xmm5, %%xmm5          \n"             \
        "shufps     $0, %%xmm6, %%xmm6          \n"             \
        "shufps     $0, %%xmm7, %%xmm7          \n"             \
        "1:                                     \n"             \
        "movaps       (%0, %1), %%xmm0          \n"             \
        "movaps  0x400(%0, %1), %%xmm1          \n"             \
        "movaps  0x800(%0, %1), %%xmm2          \n"             \
        "movaps  0xc00(%0, %1), %%xmm3          \n"             \
        "movaps 0x1000(%0, %1), %%xmm4          \n"             \
        "mulps          %%xmm5, %%xmm0          \n"             \
        "mulps          %%xmm6, %%xmm1          \n"             \
        "mulps          %%xmm5, %%xmm2          \n"             \
        "mulps          %%xmm7, %%xmm3          \n"             \
        "mulps          %%xmm7, %%xmm4          \n"             \
 stereo("addps          %%xmm1, %%xmm0          \n")            \
        "addps          %%xmm1, %%xmm2          \n"             \
        "addps          %%xmm3, %%xmm0          \n"             \
        "addps          %%xmm4, %%xmm2          \n"             \
   mono("addps          %%xmm2, %%xmm0          \n")            \
        "movaps         %%xmm0, (%0, %1)        \n"             \
 stereo("movaps         %%xmm2, 0x400(%0, %1)   \n")            \
        "add               $16, %0              \n"             \
        "jl                 1b                  \n"             \
        : "+&r"(i)                                              \
        : "r"(samples[0] + len), "r"(matrix)                    \
        : XMM_CLOBBERS("%xmm0", "%xmm1", "%xmm2", "%xmm3",      \
                      "%xmm4", "%xmm5", "%xmm6", "%xmm7",)      \
         "memory"                                               \
    );

#define MIX_MISC(stereo)                                        \
    __asm__ volatile (                                          \
        "1:                                 \n"                 \
        "movaps     (%3, %0), %%xmm0        \n"                 \
 stereo("movaps       %%xmm0, %%xmm1        \n")                \
        "mulps        %%xmm4, %%xmm0        \n"                 \
 stereo("mulps        %%xmm5, %%xmm1        \n")                \
        "lea    1024(%3, %0), %1            \n"                 \
        "mov              %5, %2            \n"                 \
        "2:                                 \n"                 \
        "movaps         (%1), %%xmm2        \n"                 \
 stereo("movaps       %%xmm2, %%xmm3        \n")                \
        "mulps      (%4, %2), %%xmm2        \n"                 \
 stereo("mulps    16(%4, %2), %%xmm3        \n")                \
        "addps        %%xmm2, %%xmm0        \n"                 \
 stereo("addps        %%xmm3, %%xmm1        \n")                \
        "add           $1024, %1            \n"                 \
        "add             $32, %2            \n"                 \
        "jl               2b                \n"                 \
        "movaps       %%xmm0,     (%3, %0)  \n"                 \
 stereo("movaps       %%xmm1, 1024(%3, %0)  \n")                \
        "add             $16, %0            \n"                 \
        "jl               1b                \n"                 \
        : "+&r"(i), "=&r"(j), "=&r"(k)                          \
        : "r"(samples[0] + len), "r"(matrix_simd + in_ch),      \
          "g"((intptr_t) - 32 * (in_ch - 1))                    \
        : "memory"                                              \
    );

static void ac3_downmix_sse(float (*samples)[256], float (*matrix)[2],
                            int out_ch, int in_ch, int len)
{
    int (*matrix_cmp)[2] = (int(*)[2])matrix;
    intptr_t i, j, k;

    i = -len * sizeof(float);
    if (in_ch == 5 && out_ch == 2 &&
        !(matrix_cmp[0][1] | matrix_cmp[2][0]   |
          matrix_cmp[3][1] | matrix_cmp[4][0]   |
          (matrix_cmp[1][0] ^ matrix_cmp[1][1]) |
          (matrix_cmp[0][0] ^ matrix_cmp[2][1]))) {
        MIX5(IF0, IF1);
    } else if (in_ch == 5 && out_ch == 1 &&
               matrix_cmp[0][0] == matrix_cmp[2][0] &&
               matrix_cmp[3][0] == matrix_cmp[4][0]) {
        MIX5(IF1, IF0);
    } else {
        DECLARE_ALIGNED(16, float, matrix_simd)[AC3_MAX_CHANNELS][2][4];
        j = 2 * in_ch * sizeof(float);
        __asm__ volatile (
            "1:                                 \n"
            "sub             $8, %0             \n"
            "movss     (%2, %0), %%xmm4         \n"
            "movss    4(%2, %0), %%xmm5         \n"
            "shufps          $0, %%xmm4, %%xmm4 \n"
            "shufps          $0, %%xmm5, %%xmm5 \n"
            "movaps      %%xmm4,   (%1, %0, 4)  \n"
            "movaps      %%xmm5, 16(%1, %0, 4)  \n"
            "jg              1b                 \n"
            : "+&r"(j)
            : "r"(matrix_simd), "r"(matrix)
            : "memory"
        );
        if (out_ch == 2) {
            MIX_MISC(IF1);
        } else {
            MIX_MISC(IF0);
        }
    }
}

#endif /* HAVE_SSE_INLINE */

av_cold void ff_ac3dsp_init_x86(AC3DSPContext *c, int bit_exact)
{
    int mm_flags = av_get_cpu_flags();

    if (EXTERNAL_MMX(mm_flags)) {
        c->ac3_exponent_min = ff_ac3_exponent_min_mmx;
        c->ac3_max_msb_abs_int16 = ff_ac3_max_msb_abs_int16_mmx;
        c->ac3_lshift_int16 = ff_ac3_lshift_int16_mmx;
        c->ac3_rshift_int32 = ff_ac3_rshift_int32_mmx;
    }
    if (EXTERNAL_AMD3DNOW(mm_flags)) {
        c->extract_exponents = ff_ac3_extract_exponents_3dnow;
        if (!bit_exact) {
            c->float_to_fixed24 = ff_float_to_fixed24_3dnow;
        }
    }
    if (EXTERNAL_MMXEXT(mm_flags)) {
        c->ac3_exponent_min = ff_ac3_exponent_min_mmxext;
        c->ac3_max_msb_abs_int16 = ff_ac3_max_msb_abs_int16_mmx2;
    }
    if (EXTERNAL_SSE(mm_flags)) {
        c->float_to_fixed24 = ff_float_to_fixed24_sse;
    }
    if (EXTERNAL_SSE2(mm_flags)) {
        c->ac3_exponent_min = ff_ac3_exponent_min_sse2;
        c->ac3_max_msb_abs_int16 = ff_ac3_max_msb_abs_int16_sse2;
        c->float_to_fixed24 = ff_float_to_fixed24_sse2;
        c->compute_mantissa_size = ff_ac3_compute_mantissa_size_sse2;
        c->extract_exponents = ff_ac3_extract_exponents_sse2;
        if (!(mm_flags & AV_CPU_FLAG_SSE2SLOW)) {
            c->ac3_lshift_int16 = ff_ac3_lshift_int16_sse2;
            c->ac3_rshift_int32 = ff_ac3_rshift_int32_sse2;
        }
    }
    if (EXTERNAL_SSSE3(mm_flags)) {
        c->ac3_max_msb_abs_int16 = ff_ac3_max_msb_abs_int16_ssse3;
        if (!(mm_flags & AV_CPU_FLAG_ATOM)) {
            c->extract_exponents = ff_ac3_extract_exponents_ssse3;
        }
    }

#if HAVE_SSE_INLINE
    if (INLINE_SSE(mm_flags)) {
        c->downmix = ac3_downmix_sse;
    }
#endif
}
