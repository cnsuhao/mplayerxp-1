/*
 * imdct.c
 * Copyright (C) 2000-2002 Michel Lespinasse <walken@zoy.org>
 * Copyright (C) 1999-2000 Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *
 * The ifft algorithms in this file have been largely inspired by Dan
 * Bernstein's work, djbfft, available at http://cr.yp.to/djbfft.html
 *
 * This file is part of a52dec, a mp_free ATSC A-52 stream decoder.
 * See http://liba52.sourceforge.net/ for updates.
 *
 * a52dec is mp_free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * a52dec is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "mp_config.h"

#include <math.h>
#include <stdio.h>
#ifndef M_PI
#define M_PI 3.1415926535897932384626433832795029
#endif
#include <inttypes.h>

#include "a52.h"
#include "a52_internal.h"
#include "osdep/mm_accel.h"

typedef struct complex_s {
    sample_t real;
    sample_t imag;
} complex_t;

static uint8_t fftorder[] = {
      0,128, 64,192, 32,160,224, 96, 16,144, 80,208,240,112, 48,176,
      8,136, 72,200, 40,168,232,104,248,120, 56,184, 24,152,216, 88,
      4,132, 68,196, 36,164,228,100, 20,148, 84,212,244,116, 52,180,
    252,124, 60,188, 28,156,220, 92, 12,140, 76,204,236,108, 44,172,
      2,130, 66,194, 34,162,226, 98, 18,146, 82,210,242,114, 50,178,
     10,138, 74,202, 42,170,234,106,250,122, 58,186, 26,154,218, 90,
    254,126, 62,190, 30,158,222, 94, 14,142, 78,206,238,110, 46,174,
      6,134, 70,198, 38,166,230,102,246,118, 54,182, 22,150,214, 86
};

/* Root values for IFFT */
static sample_t roots16[3];
static sample_t roots32[7];
static sample_t roots64[15];
static sample_t roots128[31];

/* Twiddle factors for IMDCT */
static complex_t pre1[128];
static complex_t post1[64];
static complex_t pre2[64];
static complex_t post2[32];

static sample_t a52_imdct_window[256];

static void (* ifft128) (complex_t * buf);
static void (* ifft64) (complex_t * buf);

extern uint32_t a52_accel;

#undef HAVE_MMX
#undef HAVE_MMX2
#undef HAVE_3DNOW
#undef HAVE_3DNOW2
#undef HAVE_SSE
#define RENAME(a) a ## _c
#include "imdct_mmx.h"

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
#include "imdct_mmx.h"
#endif

//3DNowEx! versions
#ifdef CAN_COMPILE_3DNOW
#undef RENAME
#define HAVE_MMX
#undef HAVE_MMX2
#define HAVE_3DNOW
#define HAVE_3DNOW2
#define RENAME(a) a ## _3DNow2
#include "imdct_mmx.h"
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
#include "imdct_mmx.h"
#endif

#endif // CAN_COMPILE_X86_ASM

static void (*ifft2_2) (complex_t * buf);
static void ifft2_2_init (complex_t * buf)
{
#ifdef CAN_COMPILE_X86_ASM
//#ifdef CAN_COMPILE_SSE
//	if(a52_accel & MM_ACCEL_X86_SSE) ifft2_2 = ifft2_2_SSE;
//	else
//#endif
#ifdef CAN_COMPILE_3DNOW2
	if(a52_accel & MM_ACCEL_X86_3DNOWEXT) ifft2_2 = ifft2_2_3DNow2;
	else
#endif
#ifdef CAN_COMPILE_3DNOW
	if(a52_accel & MM_ACCEL_X86_3DNOW) ifft2_2 = ifft2_2_3DNow;
	else
#endif
#endif //CAN_COMPILE_X86_ASM
	ifft2_2 = ifft2_2_c;
	(*ifft2_2)(buf);
}
static void (*ifft2_2) (complex_t * buf)=ifft2_2_init;

static void (*ifft4) (complex_t * buf);
static void ifft4_init (complex_t * buf)
{
#ifdef CAN_COMPILE_X86_ASM
//#ifdef CAN_COMPILE_SSE
//	if(a52_accel & MM_ACCEL_X86_SSE) ifft4 = ifft4_SSE;
//	else
//#endif
#ifdef CAN_COMPILE_3DNOW2
	if(a52_accel & MM_ACCEL_X86_3DNOWEXT) ifft4 = ifft4_3DNow2;
	else
#endif
#ifdef CAN_COMPILE_3DNOW
	if(a52_accel & MM_ACCEL_X86_3DNOW) ifft4 = ifft4_3DNow;
	else
#endif
#endif //CAN_COMPILE_X86_ASM
	ifft4 = ifft4_c;
	(*ifft4)(buf);
}
static void (*ifft4) (complex_t * buf)=ifft4_init;

/* the basic split-radix ifft butterfly */

#define BUTTERFLY(a0,a1,a2,a3,wr,wi) do {	\
    tmp5 = a2.real * wr + a2.imag * wi;		\
    tmp6 = a2.imag * wr - a2.real * wi;		\
    tmp7 = a3.real * wr - a3.imag * wi;		\
    tmp8 = a3.imag * wr + a3.real * wi;		\
    tmp1 = tmp5 + tmp7;				\
    tmp2 = tmp6 + tmp8;				\
    tmp3 = tmp6 - tmp8;				\
    tmp4 = tmp7 - tmp5;				\
    a2.real = a0.real - tmp1;			\
    a2.imag = a0.imag - tmp2;			\
    a3.real = a1.real - tmp3;			\
    a3.imag = a1.imag - tmp4;			\
    a0.real += tmp1;				\
    a0.imag += tmp2;				\
    a1.real += tmp3;				\
    a1.imag += tmp4;				\
} while (0)

/* split-radix ifft butterfly, specialized for wr=1 wi=0 */

#define BUTTERFLY_ZERO(a0,a1,a2,a3) do {	\
    tmp1 = a2.real + a3.real;			\
    tmp2 = a2.imag + a3.imag;			\
    tmp3 = a2.imag - a3.imag;			\
    tmp4 = a3.real - a2.real;			\
    a2.real = a0.real - tmp1;			\
    a2.imag = a0.imag - tmp2;			\
    a3.real = a1.real - tmp3;			\
    a3.imag = a1.imag - tmp4;			\
    a0.real += tmp1;				\
    a0.imag += tmp2;				\
    a1.real += tmp3;				\
    a1.imag += tmp4;				\
} while (0)

/* split-radix ifft butterfly, specialized for wr=wi */

#define BUTTERFLY_HALF(a0,a1,a2,a3,w) do {	\
    tmp5 = (a2.real + a2.imag) * w;		\
    tmp6 = (a2.imag - a2.real) * w;		\
    tmp7 = (a3.real - a3.imag) * w;		\
    tmp8 = (a3.imag + a3.real) * w;		\
    tmp1 = tmp5 + tmp7;				\
    tmp2 = tmp6 + tmp8;				\
    tmp3 = tmp6 - tmp8;				\
    tmp4 = tmp7 - tmp5;				\
    a2.real = a0.real - tmp1;			\
    a2.imag = a0.imag - tmp2;			\
    a3.real = a1.real - tmp3;			\
    a3.imag = a1.imag - tmp4;			\
    a0.real += tmp1;				\
    a0.imag += tmp2;				\
    a1.real += tmp3;				\
    a1.imag += tmp4;				\
} while (0)

static inline void ifft8 (complex_t * buf)
{
    double tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7, tmp8;

    ifft4 (buf);
    ifft2_2 (buf+4);
//    ifft2 (buf + 6);
    BUTTERFLY_ZERO (buf[0], buf[2], buf[4], buf[6]);
    BUTTERFLY_HALF (buf[1], buf[3], buf[5], buf[7], roots16[1]);
}

static void ifft_pass (complex_t * buf, sample_t * weight, int n)
{
    complex_t * buf1;
    complex_t * buf2;
    complex_t * buf3;
    double tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7, tmp8;
    int i;

    buf++;
    buf1 = buf + n;
    buf2 = buf + 2 * n;
    buf3 = buf + 3 * n;

    BUTTERFLY_ZERO (buf[-1], buf1[-1], buf2[-1], buf3[-1]);

    i = n - 1;

    do {
	BUTTERFLY (buf[0], buf1[0], buf2[0], buf3[0], weight[n], weight[2*i]);
	buf++;
	buf1++;
	buf2++;
	buf3++;
	weight++;
    } while (--i);
}

static void ifft16 (complex_t * buf)
{
    ifft8 (buf);
    ifft4 (buf + 8);
    ifft4 (buf + 12);
    ifft_pass (buf, roots16 - 4, 4);
}

static void ifft32 (complex_t * buf)
{
    ifft16 (buf);
    ifft8 (buf + 16);
    ifft8 (buf + 24);
    ifft_pass (buf, roots32 - 8, 8);
}

static void ifft64_c (complex_t * buf)
{
    ifft32 (buf);
    ifft16 (buf + 32);
    ifft16 (buf + 48);
    ifft_pass (buf, roots64 - 16, 16);
}

static void ifft128_c (complex_t * buf)
{
    ifft32 (buf);
    ifft16 (buf + 32);
    ifft16 (buf + 48);
    ifft_pass (buf, roots64 - 16, 16);

    ifft32 (buf + 64);
    ifft32 (buf + 96);
    ifft_pass (buf, roots128 - 32, 32);
}

static void (*imdct_512) (sample_t * data, sample_t * delay, sample_t bias);
static void imdct_512_init (sample_t * data, sample_t * delay, sample_t bias)
{
#ifdef CAN_COMPILE_X86_ASM
//#ifdef CAN_COMPILE_SSE
//	if(a52_accel & MM_ACCEL_X86_SSE) imdct_512 = imdct_512_SSE;
//	else
//#endif
#ifdef CAN_COMPILE_3DNOW2
	if(a52_accel & MM_ACCEL_X86_3DNOWEXT) imdct_512 = imdct_512_3DNow2;
	else
#endif
#ifdef CAN_COMPILE_3DNOW
	if(a52_accel & MM_ACCEL_X86_3DNOW) imdct_512 = imdct_512_3DNow;
	else
#endif
#endif //CAN_COMPILE_X86_ASM
	imdct_512 = imdct_512_c;
	(*imdct_512)(data,delay,bias);
}
static void (*imdct_512) (sample_t * data, sample_t * delay, sample_t bias)=imdct_512_init;

void a52_imdct_512 (sample_t * data, sample_t * delay, sample_t bias)
{
    (*imdct_512)(data,delay,bias);
}

static void (*imdct_256) (sample_t * data, sample_t * delay, sample_t bias);
static void imdct_256_init (sample_t * data, sample_t * delay, sample_t bias)
{
#ifdef CAN_COMPILE_X86_ASM
//#ifdef CAN_COMPILE_SSE
//	if(a52_accel & MM_ACCEL_X86_SSE) imdct_256 = imdct_256_SSE;
//	else
//#endif
#ifdef CAN_COMPILE_3DNOW2
	if(a52_accel & MM_ACCEL_X86_3DNOWEXT) imdct_256 = imdct_256_3DNow2;
	else
#endif
#ifdef CAN_COMPILE_3DNOW
	if(a52_accel & MM_ACCEL_X86_3DNOW) imdct_256 = imdct_256_3DNow;
	else
#endif
#endif //CAN_COMPILE_X86_ASM
	imdct_256 = imdct_256_c;
	(*imdct_256)(data,delay,bias);
}
static void (*imdct_256) (sample_t * data, sample_t * delay, sample_t bias)=imdct_256_init;

void a52_imdct_256 (sample_t * data, sample_t * delay, sample_t bias)
{
    (*imdct_256)(data,delay,bias);
}

static double besselI0 (double x)
{
    double bessel = 1;
    int i = 100;

    do
	bessel = bessel * x / (i * i) + 1;
    while (--i);
    return bessel;
}

char * a52_imdct_init (void)
{
    int i, k;
    double sum;
    char *rval;

    /* compute imdct window - kaiser-bessel derived window, alpha = 5.0 */
    sum = 0;
    for (i = 0; i < 256; i++) {
	sum += besselI0 (i * (256 - i) * (5 * M_PI / 256) * (5 * M_PI / 256));
	a52_imdct_window[i] = sum;
    }
    sum++;
    for (i = 0; i < 256; i++)
	a52_imdct_window[i] = sqrt (a52_imdct_window[i] / sum);

    for (i = 0; i < 3; i++)
	roots16[i] = cos ((M_PI / 8) * (i + 1));

    for (i = 0; i < 7; i++)
	roots32[i] = cos ((M_PI / 16) * (i + 1));

    for (i = 0; i < 15; i++)
	roots64[i] = cos ((M_PI / 32) * (i + 1));

    for (i = 0; i < 31; i++)
	roots128[i] = cos ((M_PI / 64) * (i + 1));

    for (i = 0; i < 64; i++) {
	k = fftorder[i] / 2 + 64;
	pre1[i].real = cos ((M_PI / 256) * (k - 0.25));
	pre1[i].imag = sin ((M_PI / 256) * (k - 0.25));
    }

    for (i = 64; i < 128; i++) {
	k = fftorder[i] / 2 + 64;
	pre1[i].real = -cos ((M_PI / 256) * (k - 0.25));
	pre1[i].imag = -sin ((M_PI / 256) * (k - 0.25));
    }

    for (i = 0; i < 64; i++) {
	post1[i].real = cos ((M_PI / 256) * (i + 0.5));
	post1[i].imag = sin ((M_PI / 256) * (i + 0.5));
    }

    for (i = 0; i < 64; i++) {
	k = fftorder[i] / 4;
	pre2[i].real = cos ((M_PI / 128) * (k - 0.25));
	pre2[i].imag = sin ((M_PI / 128) * (k - 0.25));
    }

    for (i = 0; i < 32; i++) {
	post2[i].real = cos ((M_PI / 128) * (i + 0.5));
	post2[i].imag = sin ((M_PI / 128) * (i + 0.5));
    }
	rval="generic";
#ifdef CAN_COMPILE_X86_ASM
//	if(a52_accel & MM_ACCEL_X86_SSE) rval="SSE";
//	else
	if(a52_accel & MM_ACCEL_X86_3DNOWEXT) rval="3DNowEx!";
	else
	if(a52_accel & MM_ACCEL_X86_3DNOW) rval="3DNow!";
	else
	if(a52_accel & MM_ACCEL_X86_MMX) rval="MMX";
#endif
    ifft128 = ifft128_c;
    ifft64 = ifft64_c;
    return rval;
}
