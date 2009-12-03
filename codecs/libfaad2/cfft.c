/*
** FAAD2 - Freeware Advanced Audio (AAC) Decoder including SBR decoding
** Copyright (C) 2003-2004 M. Bakker, Ahead Software AG, http://www.nero.com
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
**
** Any non-GPL usage of this software or parts of this software is strictly
** forbidden.
**
** Commercial non-GPL licensing of this software is possible.
** For more info contact Ahead Software through Mpeg4AAClicense@nero.com.
**
** Initially modified for use with MPlayer by Arpad Gereöffy on 2003/08/30
** $Id: cfft.c,v 1.5 2007/12/14 08:51:08 nickols_k Exp $
** detailed CVS changelog at http://www.mplayerhq.hu/cgi-bin/cvsweb.cgi/main/
**/

/*
 * Algorithmically based on Fortran-77 FFTPACK
 * by Paul N. Swarztrauber(Version 4, 1985).
 *
 * Does even sized fft only
 */

/* isign is +1 for backward and -1 for forward transforms */

#include "common.h"
#include "structs.h"

#include <stdlib.h>

#include "cfft.h"
#include "cfft_tab.h"
#include "../mm_accel.h"

extern unsigned faad_cpu_flags;
/* static function declarations */
#ifdef USE_SSE
static void passf2pos_sse(const uint16_t l1, const complex_t *cc,
                          complex_t *ch, const complex_t *wa);
static void passf2pos_sse_ido(const uint16_t ido, const uint16_t l1, const complex_t *cc,
                              complex_t *ch, const complex_t *wa);
static void passf4pos_sse_ido(const uint16_t ido, const uint16_t l1, const complex_t *cc, complex_t *ch,
                              const complex_t *wa1, const complex_t *wa2, const complex_t *wa3);
#endif
INLINE void cfftf1(uint16_t n, complex_t *c, complex_t *ch,
                   const uint16_t *ifac, const complex_t *wa, const int8_t isign);
static void cffti1(uint16_t n, complex_t *wa, uint16_t *ifac);

static void (*passf2pos)(const uint16_t ido, const uint16_t l1, const complex_t *cc,
                      complex_t *ch, const complex_t *wa);
static void (*passf2neg)(const uint16_t ido, const uint16_t l1, const complex_t *cc,
                      complex_t *ch, const complex_t *wa);
static void (*passf3)(const uint16_t ido, const uint16_t l1, const complex_t *cc,
                   complex_t *ch, const complex_t *wa1, const complex_t *wa2,
                   const int8_t isign);
static void (*passf4pos)(const uint16_t ido, const uint16_t l1, const complex_t *cc,
                      complex_t *ch, const complex_t *wa1, const complex_t *wa2,
                      const complex_t *wa3);
static void (*passf4neg)(const uint16_t ido, const uint16_t l1, const complex_t *cc, complex_t *ch,
                      const complex_t *wa1, const complex_t *wa2, const complex_t *wa3);
static void (*passf5)(const uint16_t ido, const uint16_t l1, const complex_t *cc, complex_t *ch,
                   const complex_t *wa1, const complex_t *wa2, const complex_t *wa3,
                   const complex_t *wa4, const int8_t isign);
static void (*cfftf1pos)(uint16_t n, complex_t *c, complex_t *ch,
                             const uint16_t *ifac, const complex_t *wa,
                             const int8_t isign);
static void (*cfftf1neg)(uint16_t n, complex_t *c, complex_t *ch,
                             const uint16_t *ifac, const complex_t *wa,
                             const int8_t isign);

#undef HAVE_MMX
#undef HAVE_MMX2
#undef HAVE_3DNOW
#undef HAVE_3DNOW2
#undef HAVE_SSE
#define RENAME(a) a ## _c
#include "i386/cfft.h"

#if defined( ARCH_X86 ) || defined(ARCH_X86_64)
#define CAN_COMPILE_X86_ASM
#endif

#ifdef CAN_COMPILE_X86_ASM

//3DNow! versions
#ifdef CAN_COMPILE_3DNOW
#undef RENAME
#define HAVE_MMX
#undef HAVE_MMX2
#define HAVE_3DNOW
#define RENAME(a) a ## _3DNow
#include "i386/cfft.h"
#endif

//MMX2 versions
#ifdef CAN_COMPILE_SSE
#undef RENAME
#define HAVE_MMX
#define HAVE_MMX2
#undef HAVE_3DNOW
#undef HAVE_3DNOW2
#define HAVE_SSE
#define RENAME(a) a ## _SSE
#include "i386/cfft.h"
#endif

#endif // CAN_COMPILE_X86_ASM

/*----------------------------------------------------------------------
   passf2, passf3, passf4, passf5. Complex FFT passes fwd and bwd.
  ----------------------------------------------------------------------*/

#if 0 //def USE_SSE
static void passf2pos_sse(const uint16_t l1, const complex_t *cc,
                          complex_t *ch, const complex_t *wa)
{
    uint16_t k, ah, ac;

    for (k = 0; k < l1; k++)
    {
        ah = 2*k;
        ac = 4*k;

        RE(ch[ah])    = RE(cc[ac]) + RE(cc[ac+1]);
        IM(ch[ah])    = IM(cc[ac]) + IM(cc[ac+1]);

        RE(ch[ah+l1]) = RE(cc[ac]) - RE(cc[ac+1]);
        IM(ch[ah+l1]) = IM(cc[ac]) - IM(cc[ac+1]);
    }
}

static void passf2pos_sse_ido(const uint16_t ido, const uint16_t l1, const complex_t *cc,
                              complex_t *ch, const complex_t *wa)
{
    uint16_t i, k, ah, ac;

    for (k = 0; k < l1; k++)
    {
        ah = k*ido;
        ac = 2*k*ido;

        for (i = 0; i < ido; i+=4)
        {
            __m128 m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14;
            __m128 m15, m16, m17, m18, m19, m20, m21, m22, m23, m24;
            __m128 w1, w2, w3, w4;

            m1 = _mm_load_ps(&RE(cc[ac+i]));
            m2 = _mm_load_ps(&RE(cc[ac+ido+i]));
            m5 = _mm_load_ps(&RE(cc[ac+i+2]));
            m6 = _mm_load_ps(&RE(cc[ac+ido+i+2]));
            w1 = _mm_load_ps(&RE(wa[i]));
            w3 = _mm_load_ps(&RE(wa[i+2]));

            m3 = _mm_add_ps(m1, m2);
            m15 = _mm_add_ps(m5, m6);

            m4 = _mm_sub_ps(m1, m2);
            m16 = _mm_sub_ps(m5, m6);

            _mm_store_ps(&RE(ch[ah+i]), m3);
            _mm_store_ps(&RE(ch[ah+i+2]), m15);


            w2 = _mm_shuffle_ps(w1, w1, _MM_SHUFFLE(2, 3, 0, 1));
            w4 = _mm_shuffle_ps(w3, w3, _MM_SHUFFLE(2, 3, 0, 1));

            m7 = _mm_mul_ps(m4, w1);
            m17 = _mm_mul_ps(m16, w3);
            m8 = _mm_mul_ps(m4, w2);
            m18 = _mm_mul_ps(m16, w4);

            m9  = _mm_shuffle_ps(m7, m8, _MM_SHUFFLE(2, 0, 2, 0));
            m19 = _mm_shuffle_ps(m17, m18, _MM_SHUFFLE(2, 0, 2, 0));
            m10 = _mm_shuffle_ps(m7, m8, _MM_SHUFFLE(3, 1, 3, 1));
            m20 = _mm_shuffle_ps(m17, m18, _MM_SHUFFLE(3, 1, 3, 1));

            m11 = _mm_add_ps(m9, m10);
            m21 = _mm_add_ps(m19, m20);
            m12 = _mm_sub_ps(m9, m10);
            m22 = _mm_sub_ps(m19, m20);

            m13 = _mm_shuffle_ps(m11, m11, _MM_SHUFFLE(0, 0, 3, 2));
            m23 = _mm_shuffle_ps(m21, m21, _MM_SHUFFLE(0, 0, 3, 2));

            m14 = _mm_unpacklo_ps(m12, m13);
            m24 = _mm_unpacklo_ps(m22, m23);

            _mm_store_ps(&RE(ch[ah+i+l1*ido]), m14);
            _mm_store_ps(&RE(ch[ah+i+2+l1*ido]), m24);
        }
    }
}
#endif

static void passf2pos_init(const uint16_t ido, const uint16_t l1, const complex_t *cc,
                      complex_t *ch, const complex_t *wa)
{
#ifdef CAN_COMPILE_X86_ASM
#ifdef CAN_COMPILE_SSE
	if(faad_cpu_flags & MM_ACCEL_X86_SSE) passf2pos = passf2pos_SSE;
	else
#endif
#ifdef CAN_COMPILE_3DNOW
	if(faad_cpu_flags & MM_ACCEL_X86_3DNOW) passf2pos = passf2pos_3DNow;
	else
#endif
#endif //CAN_COMPILE_X86_ASM
	passf2pos = passf2pos_c;
	(*passf2pos)(ido,l1,cc,ch,wa);
}
static void (*passf2pos)(const uint16_t ido, const uint16_t l1, const complex_t *cc,
                      complex_t *ch, const complex_t *wa)=passf2pos_init;

static void passf2neg_init(const uint16_t ido, const uint16_t l1, const complex_t *cc,
                      complex_t *ch, const complex_t *wa)
{
#ifdef CAN_COMPILE_X86_ASM
#ifdef CAN_COMPILE_SSE
	if(faad_cpu_flags & MM_ACCEL_X86_SSE) passf2neg = passf2neg_SSE;
	else
#endif
#ifdef CAN_COMPILE_3DNOW
	if(faad_cpu_flags & MM_ACCEL_X86_3DNOW) passf2neg = passf2neg_3DNow;
	else
#endif
#endif //CAN_COMPILE_X86_ASM
	passf2neg = passf2neg_c;
	(*passf2neg)(ido,l1,cc,ch,wa);
}
static void (*passf2neg)(const uint16_t ido, const uint16_t l1, const complex_t *cc,
                      complex_t *ch, const complex_t *wa)=passf2neg_init;

static void passf3_init(const uint16_t ido, const uint16_t l1, const complex_t *cc,
                   complex_t *ch, const complex_t *wa1, const complex_t *wa2,
                   const int8_t isign)
{
#ifdef CAN_COMPILE_X86_ASM
#ifdef CAN_COMPILE_SSE
	if(faad_cpu_flags & MM_ACCEL_X86_SSE) passf3 = passf3_SSE;
	else
#endif
#ifdef CAN_COMPILE_3DNOW
	if(faad_cpu_flags & MM_ACCEL_X86_3DNOW) passf3 = passf3_3DNow;
	else
#endif
#endif //CAN_COMPILE_X86_ASM
	passf3 = passf3_c;
	(*passf3)(ido,l1,cc,ch,wa1,wa2,isign);
}
static void (*passf3)(const uint16_t ido, const uint16_t l1, const complex_t *cc,
                   complex_t *ch, const complex_t *wa1, const complex_t *wa2,
                   const int8_t isign)=passf3_init;

#ifdef USE_SSE
ALIGN static const int32_t negate[4] = { 0x0, 0x0, 0x0, 0x80000000 };

__declspec(naked) static void passf4pos_sse(const uint16_t l1, const complex_t *cc,
                                     complex_t *ch, const complex_t *wa1, const complex_t *wa2,
                                     const complex_t *wa3)
{
    __asm {
        push      ebx
        mov       ebx, esp
        and       esp, -16
        push      edi
        push      esi
        sub       esp, 8
        movzx     edi, WORD PTR [ebx+8]

        movaps    xmm1, XMMWORD PTR negate

        test      edi, edi
        jle       l1_is_zero

        lea       esi, DWORD PTR [edi+edi]
        add       esi, esi
        sub       esi, edi
        add       esi, esi
        add       esi, esi
        add       esi, esi
        mov       eax, DWORD PTR [ebx+16]
        add       esi, eax
        lea       ecx, DWORD PTR [edi+edi]
        add       ecx, ecx
        add       ecx, ecx
        add       ecx, ecx
        add       ecx, eax
        lea       edx, DWORD PTR [edi+edi]
        add       edx, edx
        add       edx, edx
        add       edx, eax
        xor       eax, eax
        mov       DWORD PTR [esp], ebp
        mov       ebp, DWORD PTR [ebx+12]

fftloop:
        lea       edi, DWORD PTR [eax+eax]
        add       edi, edi
        movaps    xmm2, XMMWORD PTR [ebp+edi*8]
        movaps    xmm0, XMMWORD PTR [ebp+edi*8+16]
        movaps    xmm7, XMMWORD PTR [ebp+edi*8+32]
        movaps    xmm5, XMMWORD PTR [ebp+edi*8+48]
        movaps    xmm6, xmm2
        addps     xmm6, xmm0
        movaps    xmm4, xmm1
        xorps     xmm4, xmm7
        movaps    xmm3, xmm1
        xorps     xmm3, xmm5
        xorps     xmm2, xmm1
        xorps     xmm0, xmm1
        addps     xmm7, xmm5
        subps     xmm2, xmm0
        movaps    xmm0, xmm6
        shufps    xmm0, xmm7, 68
        subps     xmm4, xmm3
        shufps    xmm6, xmm7, 238
        movaps    xmm5, xmm2
        shufps    xmm5, xmm4, 68
        movaps    xmm3, xmm0
        addps     xmm3, xmm6
        shufps    xmm2, xmm4, 187
        subps     xmm0, xmm6
        movaps    xmm4, xmm5
        addps     xmm4, xmm2
        mov       edi, DWORD PTR [ebx+16]
        movaps    XMMWORD PTR [edi+eax*8], xmm3
        subps     xmm5, xmm2
        movaps    XMMWORD PTR [edx+eax*8], xmm4
        movaps    XMMWORD PTR [ecx+eax*8], xmm0
        movaps    XMMWORD PTR [esi+eax*8], xmm5
        add       eax, 2
        movzx     eax, ax
        movzx     edi, WORD PTR [ebx+8]
        cmp       eax, edi
        jl        fftloop

        mov       ebp, DWORD PTR [esp]

l1_is_zero:

        add       esp, 8
        pop       esi
        pop       edi
        mov       esp, ebx
        pop       ebx
        ret
    }
}
#endif

#if 0
static void passf4pos_sse_ido(const uint16_t ido, const uint16_t l1, const complex_t *cc,
                              complex_t *ch, const complex_t *wa1, const complex_t *wa2,
                              const complex_t *wa3)
{
    uint16_t i, k, ac, ah;

    for (k = 0; k < l1; k++)
    {
        ac = 4*k*ido;
        ah = k*ido;

        for (i = 0; i < ido; i+=2)
        {
            __m128 m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14, m15, m16;
            __m128 n1, n2, n3, n4, n5, n6, n7, n8, n9, m17, m18, m19, m20, m21, m22, m23;
            __m128 w1, w2, w3, w4, w5, w6, m24, m25, m26, m27, m28, m29, m30;
            __m128 neg1 = _mm_set_ps(-1.0, 1.0, -1.0, 1.0);

            m1 = _mm_load_ps(&RE(cc[ac+i]));
            m2 = _mm_load_ps(&RE(cc[ac+i+2*ido]));
            m3 = _mm_add_ps(m1, m2);
            m4 = _mm_sub_ps(m1, m2);

            n1 = _mm_load_ps(&RE(cc[ac+i+ido]));
            n2 = _mm_load_ps(&RE(cc[ac+i+3*ido]));
            n3 = _mm_add_ps(n1, n2);

            n4 = _mm_mul_ps(neg1, n1);
            n5 = _mm_mul_ps(neg1, n2);
            n6 = _mm_sub_ps(n4, n5);

            m5 = _mm_add_ps(m3, n3);

            n7 = _mm_shuffle_ps(n6, n6, _MM_SHUFFLE(2, 3, 0, 1));
            n8 = _mm_add_ps(m4, n7);

            m6 = _mm_sub_ps(m3, n3);
            n9 = _mm_sub_ps(m4, n7);

            _mm_store_ps(&RE(ch[ah+i]), m5);

#if 0
            static INLINE void ComplexMult(real_t *y1, real_t *y2,
                real_t x1, real_t x2, real_t c1, real_t c2)
            {
                *y1 = MUL_F(x1, c1) + MUL_F(x2, c2);
                *y2 = MUL_F(x2, c1) - MUL_F(x1, c2);
            }

            m7.0 = RE(c2)*RE(wa1[i])
            m7.1 = IM(c2)*IM(wa1[i])
            m7.2 = RE(c6)*RE(wa1[i+1])
            m7.3 = IM(c6)*IM(wa1[i+1])

            m8.0 = RE(c2)*IM(wa1[i])
            m8.1 = IM(c2)*RE(wa1[i])
            m8.2 = RE(c6)*IM(wa1[i+1])
            m8.3 = IM(c6)*RE(wa1[i+1])

            RE(0) = m7.0 - m7.1
            IM(0) = m8.0 + m8.1
            RE(1) = m7.2 - m7.3
            IM(1) = m8.2 + m8.3

            ////
            RE(0) = RE(c2)*RE(wa1[i])   - IM(c2)*IM(wa1[i])
            IM(0) = RE(c2)*IM(wa1[i])   + IM(c2)*RE(wa1[i])
            RE(1) = RE(c6)*RE(wa1[i+1]) - IM(c6)*IM(wa1[i+1])
            IM(1) = RE(c6)*IM(wa1[i+1]) + IM(c6)*RE(wa1[i+1])
#endif

            w1 = _mm_load_ps(&RE(wa1[i]));
            w3 = _mm_load_ps(&RE(wa2[i]));
            w5 = _mm_load_ps(&RE(wa3[i]));

            w2 = _mm_shuffle_ps(w1, w1, _MM_SHUFFLE(2, 3, 0, 1));
            w4 = _mm_shuffle_ps(w3, w3, _MM_SHUFFLE(2, 3, 0, 1));
            w6 = _mm_shuffle_ps(w5, w5, _MM_SHUFFLE(2, 3, 0, 1));

            m7 = _mm_mul_ps(n8, w1);
            m15 = _mm_mul_ps(m6, w3);
            m23 = _mm_mul_ps(n9, w5);
            m8 = _mm_mul_ps(n8, w2);
            m16 = _mm_mul_ps(m6, w4);
            m24 = _mm_mul_ps(n9, w6);

            m9  = _mm_shuffle_ps(m7, m8, _MM_SHUFFLE(2, 0, 2, 0));
            m17 = _mm_shuffle_ps(m15, m16, _MM_SHUFFLE(2, 0, 2, 0));
            m25 = _mm_shuffle_ps(m23, m24, _MM_SHUFFLE(2, 0, 2, 0));
            m10 = _mm_shuffle_ps(m7, m8, _MM_SHUFFLE(3, 1, 3, 1));
            m18 = _mm_shuffle_ps(m15, m16, _MM_SHUFFLE(3, 1, 3, 1));
            m26 = _mm_shuffle_ps(m23, m24, _MM_SHUFFLE(3, 1, 3, 1));

            m11 = _mm_add_ps(m9, m10);
            m19 = _mm_add_ps(m17, m18);
            m27 = _mm_add_ps(m25, m26);
            m12 = _mm_sub_ps(m9, m10);
            m20 = _mm_sub_ps(m17, m18);
            m28 = _mm_sub_ps(m25, m26);

            m13 = _mm_shuffle_ps(m11, m11, _MM_SHUFFLE(0, 0, 3, 2));
            m21 = _mm_shuffle_ps(m19, m19, _MM_SHUFFLE(0, 0, 3, 2));
            m29 = _mm_shuffle_ps(m27, m27, _MM_SHUFFLE(0, 0, 3, 2));
            m14 = _mm_unpacklo_ps(m12, m13);
            m22 = _mm_unpacklo_ps(m20, m21);
            m30 = _mm_unpacklo_ps(m28, m29);

            _mm_store_ps(&RE(ch[ah+i+l1*ido]), m14);
            _mm_store_ps(&RE(ch[ah+i+2*l1*ido]), m22);
            _mm_store_ps(&RE(ch[ah+i+3*l1*ido]), m30);
        }
    }
}
#endif

static void passf4pos_init(const uint16_t ido, const uint16_t l1, const complex_t *cc,
                      complex_t *ch, const complex_t *wa1, const complex_t *wa2,
                      const complex_t *wa3)
{
#ifdef CAN_COMPILE_X86_ASM
#ifdef CAN_COMPILE_SSE
	if(faad_cpu_flags & MM_ACCEL_X86_SSE) passf4pos = passf4pos_SSE;
	else
#endif
#ifdef CAN_COMPILE_3DNOW
	if(faad_cpu_flags & MM_ACCEL_X86_3DNOW) passf4pos = passf4pos_3DNow;
	else
#endif
#endif //CAN_COMPILE_X86_ASM
	passf4pos = passf4pos_c;
	(*passf4pos)(ido,l1,cc,ch,wa1,wa2,wa3);
}
static void (*passf4pos)(const uint16_t ido, const uint16_t l1, const complex_t *cc,
                      complex_t *ch, const complex_t *wa1, const complex_t *wa2,
                      const complex_t *wa3)=passf4pos_init;

static void passf4neg_init(const uint16_t ido, const uint16_t l1, const complex_t *cc,
                      complex_t *ch, const complex_t *wa1, const complex_t *wa2,
                      const complex_t *wa3)
{
#ifdef CAN_COMPILE_X86_ASM
#ifdef CAN_COMPILE_SSE
	if(faad_cpu_flags & MM_ACCEL_X86_SSE) passf4neg = passf4neg_SSE;
	else
#endif
#ifdef CAN_COMPILE_3DNOW
	if(faad_cpu_flags & MM_ACCEL_X86_3DNOW) passf4neg = passf4neg_3DNow;
	else
#endif
#endif //CAN_COMPILE_X86_ASM
	passf4neg = passf4neg_c;
	(*passf4neg)(ido,l1,cc,ch,wa1,wa2,wa3);
}
static void (*passf4neg)(const uint16_t ido, const uint16_t l1, const complex_t *cc,
                      complex_t *ch, const complex_t *wa1, const complex_t *wa2,
                      const complex_t *wa3)=passf4neg_init;


static void passf5_init(const uint16_t ido, const uint16_t l1, const complex_t *cc,
                   complex_t *ch, const complex_t *wa1, const complex_t *wa2, const complex_t *wa3,
                   const complex_t *wa4, const int8_t isign)
{
#ifdef CAN_COMPILE_X86_ASM
#ifdef CAN_COMPILE_SSE
	if(faad_cpu_flags & MM_ACCEL_X86_SSE) passf5 = passf5_SSE;
	else
#endif
#ifdef CAN_COMPILE_3DNOW
	if(faad_cpu_flags & MM_ACCEL_X86_3DNOW) passf5 = passf5_3DNow;
	else
#endif
#endif //CAN_COMPILE_X86_ASM
	passf5 = passf5_c;
	(*passf5)(ido,l1,cc,ch,wa1,wa2,wa3,wa4,isign);
}
static void (*passf5)(const uint16_t ido, const uint16_t l1, const complex_t *cc,
                   complex_t *ch, const complex_t *wa1, const complex_t *wa2, const complex_t *wa3,
                   const complex_t *wa4, const int8_t isign)=passf5_init;



/*----------------------------------------------------------------------
   cfftf1, cfftf, cfftb, cffti1, cffti. Complex FFTs.
  ----------------------------------------------------------------------*/

#ifdef USE_SSE

#define CONV(A,B,C) ( (A<<2) | ((B & 0x1)<<1) | ((C==1)&0x1) )

static INLINE void cfftf1pos_sse(uint16_t n, complex_t *c, complex_t *ch,
                                 const uint16_t *ifac, const complex_t *wa,
                                 const int8_t isign)
{
    uint16_t i;
    uint16_t k1, l1, l2;
    uint16_t na, nf, ip, iw, ix2, ix3, ix4, ido, idl1;

    nf = ifac[1];
    na = 0;
    l1 = 1;
    iw = 0;

    for (k1 = 2; k1 <= nf+1; k1++)
    {
        ip = ifac[k1];
        l2 = ip*l1;
        ido = n / l2;
        idl1 = ido*l1;

        ix2 = iw + ido;
        ix3 = ix2 + ido;
        ix4 = ix3 + ido;

        switch (CONV(ip,na,ido))
        {
        case CONV(4,0,0):
            //passf4pos_sse_ido((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)c, ch, &wa[iw], &wa[ix2], &wa[ix3]);
            passf4pos((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)c, ch, &wa[iw], &wa[ix2], &wa[ix3]);
            break;
        case CONV(4,0,1):
            passf4pos_sse((const uint16_t)l1, (const complex_t*)c, ch, &wa[iw], &wa[ix2], &wa[ix3]);
            break;
        case CONV(4,1,0):
            passf4pos((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)ch, c, &wa[iw], &wa[ix2], &wa[ix3]);
            //passf4pos_sse_ido((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)ch, c, &wa[iw], &wa[ix2], &wa[ix3]);
            break;
        case CONV(4,1,1):
            passf4pos_sse((const uint16_t)l1, (const complex_t*)ch, c, &wa[iw], &wa[ix2], &wa[ix3]);
            break;
        case CONV(2,0,0):
            passf2pos((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)c, ch, &wa[iw]);
            //passf2pos_sse_ido((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)c, ch, &wa[iw]);
            break;
        case CONV(2,0,1):
            passf2pos((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)c, ch, &wa[iw]);
            //passf2pos_sse((const uint16_t)l1, (const complex_t*)c, ch, &wa[iw]);
            break;
        case CONV(2,1,0):
            passf2pos((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)ch, c, &wa[iw]);
            //passf2pos_sse_ido((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)ch, c, &wa[iw]);
            break;
        case CONV(2,1,1):
            passf2pos((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)ch, c, &wa[iw]);
            //passf2pos_sse((const uint16_t)l1, (const complex_t*)ch, c, &wa[iw]);
            break;
        case CONV(3,0,0):
        case CONV(3,0,1):
            passf3((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)c, ch, &wa[iw], &wa[ix2], isign);
            break;
        case CONV(3,1,0):
        case CONV(3,1,1):
            passf3((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)ch, c, &wa[iw], &wa[ix2], isign);
            break;
        case CONV(5,0,0):
        case CONV(5,0,1):
            passf5((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)c, ch, &wa[iw], &wa[ix2], &wa[ix3], &wa[ix4], isign);
            break;
        case CONV(5,1,0):
        case CONV(5,1,1):
            passf5((const uint16_t)ido, (const uint16_t)l1, (const complex_t*)ch, c, &wa[iw], &wa[ix2], &wa[ix3], &wa[ix4], isign);
            break;
        }

        na = 1 - na;

        l1 = l2;
        iw += (ip-1) * ido;
    }

    if (na == 0)
        return;

    for (i = 0; i < n; i++)
    {
        RE(c[i]) = RE(ch[i]);
        IM(c[i]) = IM(ch[i]);
    }
}
#endif

static void cfftf1pos_init(uint16_t n, complex_t *c, complex_t *ch,
                             const uint16_t *ifac, const complex_t *wa,
                             const int8_t isign)
{
#ifdef CAN_COMPILE_X86_ASM
#ifdef CAN_COMPILE_SSE
	if(faad_cpu_flags & MM_ACCEL_X86_SSE) cfftf1pos = cfftf1pos_SSE;
	else
#endif
#ifdef CAN_COMPILE_3DNOW
	if(faad_cpu_flags & MM_ACCEL_X86_3DNOW) cfftf1pos = cfftf1pos_3DNow;
	else
#endif
#endif //CAN_COMPILE_X86_ASM
	cfftf1pos = cfftf1pos_c;
	(*cfftf1pos)(n,c,ch,ifac,wa,isign);
}
static void (*cfftf1pos)(uint16_t n, complex_t *c, complex_t *ch,
                             const uint16_t *ifac, const complex_t *wa,
                             const int8_t isign)=cfftf1pos_init;

static void cfftf1neg_init(uint16_t n, complex_t *c, complex_t *ch,
                             const uint16_t *ifac, const complex_t *wa,
                             const int8_t isign)
{
#ifdef CAN_COMPILE_X86_ASM
#ifdef CAN_COMPILE_SSE
	if(faad_cpu_flags & MM_ACCEL_X86_SSE) cfftf1neg = cfftf1neg_SSE;
	else
#endif
#ifdef CAN_COMPILE_3DNOW
	if(faad_cpu_flags & MM_ACCEL_X86_3DNOW) cfftf1neg = cfftf1neg_3DNow;
	else
#endif
#endif //CAN_COMPILE_X86_ASM
	cfftf1neg = cfftf1neg_c;
	(*cfftf1neg)(n,c,ch,ifac,wa,isign);
}
static void (*cfftf1neg)(uint16_t n, complex_t *c, complex_t *ch,
                             const uint16_t *ifac, const complex_t *wa,
                             const int8_t isign)=cfftf1neg_init;


void cfftf(cfft_info *cfft, complex_t *c)
{
    cfftf1neg(cfft->n, c, cfft->work, (const uint16_t*)cfft->ifac, (const complex_t*)cfft->tab, -1);
}

void cfftb(cfft_info *cfft, complex_t *c)
{
    cfftf1pos(cfft->n, c, cfft->work, (const uint16_t*)cfft->ifac, (const complex_t*)cfft->tab, +1);
}

static void cffti1(uint16_t n, complex_t *wa, uint16_t *ifac)
{
    static uint16_t ntryh[4] = {3, 4, 2, 5};
#ifndef FIXED_POINT
    real_t arg, argh, argld, fi;
    uint16_t ido, ipm;
    uint16_t i1, k1, l1, l2;
    uint16_t ld, ii, ip;
#endif
    uint16_t ntry = 0, i, j;
    uint16_t ib;
    uint16_t nf, nl, nq, nr;

    nl = n;
    nf = 0;
    j = 0;

startloop:
    j++;

    if (j <= 4)
        ntry = ntryh[j-1];
    else
        ntry += 2;

    do
    {
        nq = nl / ntry;
        nr = nl - ntry*nq;

        if (nr != 0)
            goto startloop;

        nf++;
        ifac[nf+1] = ntry;
        nl = nq;

        if (ntry == 2 && nf != 1)
        {
            for (i = 2; i <= nf; i++)
            {
                ib = nf - i + 2;
                ifac[ib+1] = ifac[ib];
            }
            ifac[2] = 2;
        }
    } while (nl != 1);

    ifac[0] = n;
    ifac[1] = nf;

#ifndef FIXED_POINT
    argh = (real_t)2.0*(real_t)M_PI / (real_t)n;
    i = 0;
    l1 = 1;

    for (k1 = 1; k1 <= nf; k1++)
    {
        ip = ifac[k1+1];
        ld = 0;
        l2 = l1*ip;
        ido = n / l2;
        ipm = ip - 1;

        for (j = 0; j < ipm; j++)
        {
            i1 = i;
            RE(wa[i]) = 1.0;
            IM(wa[i]) = 0.0;
            ld += l1;
            fi = 0;
            argld = ld*argh;

            for (ii = 0; ii < ido; ii++)
            {
                i++;
                fi++;
                arg = fi * argld;
                RE(wa[i]) = (real_t)cos(arg);
#if 1
                IM(wa[i]) = (real_t)sin(arg);
#else
                IM(wa[i]) = (real_t)-sin(arg);
#endif
            }

            if (ip > 5)
            {
                RE(wa[i1]) = RE(wa[i]);
                IM(wa[i1]) = IM(wa[i]);
            }
        }
        l1 = l2;
    }
#endif
}

cfft_info *cffti(uint16_t n)
{
    cfft_info *cfft = (cfft_info*)faad_malloc(sizeof(cfft_info));

    cfft->n = n;
    cfft->work = (complex_t*)faad_malloc(n*sizeof(complex_t));

#ifndef FIXED_POINT
    cfft->tab = (complex_t*)faad_malloc(n*sizeof(complex_t));

    cffti1(n, cfft->tab, cfft->ifac);
#else
    cffti1(n, NULL, cfft->ifac);

    switch (n)
    {
    case 64: cfft->tab = cfft_tab_64; break;
    case 512: cfft->tab = cfft_tab_512; break;
#ifdef LD_DEC
    case 256: cfft->tab = cfft_tab_256; break;
#endif

#ifdef ALLOW_SMALL_FRAMELENGTH
    case 60: cfft->tab = cfft_tab_60; break;
    case 480: cfft->tab = cfft_tab_480; break;
#ifdef LD_DEC
    case 240: cfft->tab = cfft_tab_240; break;
#endif
#endif
    }
#endif

    return cfft;
}

void cfftu(cfft_info *cfft)
{
    if (cfft->work) faad_free(cfft->work);
#ifndef FIXED_POINT
    if (cfft->tab) faad_free(cfft->tab);
#endif

    if (cfft) faad_free(cfft);
}

