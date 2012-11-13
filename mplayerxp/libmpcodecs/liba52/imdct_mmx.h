/*
 * imdct_mmx.h
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
#undef PSWAP_MM
#ifdef HAVE_3DNOW2
#define PSWAP_MM(mm_base,mm_hlp) "pswapd	"mm_base","mm_base"\n\t"
#else
#define PSWAP_MM(mm_base,mm_hlp)\
	"movq	"mm_base","mm_hlp"\n\t"\
	"psrlq $32, "mm_base"\n\t"\
	"punpckldq "mm_hlp","mm_base"\n\t"
#endif
#undef PFNACC_MM
#ifdef HAVE_3DNOWEX
#define PFNACC_MM(mm_base,mm_hlp)	"pfnacc	"mm_base","mm_base"\n\t"
#else
#define PFNACC_MM(mm_base,mm_hlp)\
	"movq	"mm_base","mm_hlp"\n\t"\
	"psrlq	$32,"mm_hlp"\n\t"\
	"punpckldq "mm_hlp","mm_hlp"\n\t"\
	"pfsub	"mm_hlp","mm_base"\n\t"
#endif
#undef PFPNACC_MM
#ifdef HAVE_3DNOW2
#define PFPNACC_MM(mm_src,mm_dest,mm_08) "pfpnacc	"mm_src","mm_dest"\n\t"
#else
#define PFPNACC_MM(mm_src,mm_dest,mm_08)\
		"pxor	"mm_08","mm_dest"\n\t"\
		"pfacc	"mm_src","mm_dest"\n\t"
#endif

static void RENAME(ifft2_2) (complex_t * buf)
{
#ifdef HAVE_SSE
    __asm __volatile(
	"movups	(%0), %%xmm0\n\t"
	"movups	16(%0), %%xmm1\n\t"
	"movaps %%xmm0, %%xmm2\n\t"
	"movaps %%xmm1, %%xmm3\n\t"
	"shufps $0x44, %%xmm1, %%xmm0\n\t"
	"shufps $0xEE, %%xmm2, %%xmm3\n\t"
	"addps	%%xmm0, %%xmm2\n\t"
	"subps	%%xmm0, %%xmm3\n\t"
	"movups %%xmm2, (%0)\n\t"
	"movups %%xmm3, 16(%0)\n\t"
	::"r"(buf):"memory");
#elif defined( HAVE_3DNOW )
    __asm __volatile(
	"movq	(%0), %%mm0\n\t"
	"movq	16(%0), %%mm4\n\t"
	"movq	%%mm0, %%mm2\n\t"
	"movq	%%mm4, %%mm6\n\t"
	"pfadd	8(%0),%%mm0\n\t"
	"pfadd	24(%0),%%mm4\n\t"
	"pfsub	8(%0),%%mm2\n\t"
	"pfsub	24(%0),%%mm6\n\t"
	"movq	%%mm0, (%0)\n\t"
	"movq	%%mm2, 8(%0)\n\t"
	"movq	%%mm4, 16(%0)\n\t"
	"movq	%%mm6, 24(%0)\n\t"
	"femms"
	::"r"(buf):"memory"
#ifdef FPU_CLOBBERED
	,FPU_CLOBBERED
#endif
#ifdef MMX_CLOBBERED
	,MMX_CLOBBERED
#endif
	);
#else
    double r, i;
    r = buf[0].real;
    i = buf[0].imag;
    buf[0].real += buf[1].real;
    buf[0].imag += buf[1].imag;
    buf[1].real = r - buf[1].real;
    buf[1].imag = i - buf[1].imag;

    r = buf[2].real;
    i = buf[2].imag;
    buf[2].real += buf[3].real;
    buf[2].imag += buf[3].imag;
    buf[3].real = r - buf[3].real;
    buf[3].imag = i - buf[3].imag;
#endif
}

static void RENAME(ifft4)(complex_t *buf)
{
#ifdef HAVE_SSE
#elif defined( HAVE_3DNOW )
    const unsigned  __attribute__((__aligned__(8))) plus_minus_3dnow[2] = { 0x00000000UL, 0x80000000UL };
    __asm __volatile(
	"movq	(%0), %%mm0\n\t"  //buf[0]
	"movq	16(%0), %%mm1\n\t"//buf[2]
	"movq	%%mm0, %%mm2\n\t" //buf[0]
	"movq	%%mm1, %%mm3\n\t" //buf[2]
	"movq	8(%0), %%mm4\n\t" //buf[1]
	"movq	24(%0), %%mm5\n\t"//buf[3]
	"movq	%%mm4, %%mm6\n\t" //buf[1]
	"movq	%%mm5, %%mm7\n\t" //buf[3]
	"pfadd	%%mm4, %%mm0\n\t" //tmp1=buf[0]+buf[1]
	"pfadd	%%mm5, %%mm1\n\t" //tmp2=buf[2]+buf[3]
	"pfsub	%%mm6, %%mm2\n\t" //tmp3=buf[0]-buf[1]
	"pxor	(%1), %%mm3\n\t"  //buf[2]=+-
	"pxor	(%1), %%mm7\n\t"  //buf[3]=+-
	"pfsub	%%mm3, %%mm7\n\t" //tmp4=buf[3]-buf[2]
	"movq	%%mm2, %%mm4\n\t" //tmp3
	"movq	%%mm0, %%mm6\n\t" //tmp1
	PSWAP_MM("%%mm7","%%mm5") //tmp4
	"pfadd	%%mm1, %%mm0\n\t" //buf[0]=tmp1+tmp2
	"pfadd	%%mm7, %%mm2\n\t" //buf[1]=tmp3+tmp4
	"pfsub	%%mm1, %%mm6\n\t" //buf[2]=tmp1-tmp2
	"pfsub  %%mm7, %%mm4\n\t" //buf[3]=tmp3-tmp4
	"movq	%%mm0, (%0)\n\t"
	"movq	%%mm2, 8(%0)\n\t"
	"movq   %%mm6, 16(%0)\n\t"
	"movq	%%mm4, 24(%0)\n\t"
	"femms"
	::"r"(buf),"r"(plus_minus_3dnow):"memory"
#ifdef FPU_CLOBBERED
	,FPU_CLOBBERED
#endif
#ifdef MMX_CLOBBERED
	,MMX_CLOBBERED
#endif
	);
#else
    complex_t tmp1, tmp2, tmp3, tmp4;

    tmp1.real = buf[0].real + buf[1].real;
    tmp1.imag = buf[0].imag + buf[1].imag;
    tmp2.real = buf[2].real + buf[3].real;
    tmp2.imag = buf[2].imag + buf[3].imag;

    tmp3.real = buf[0].real - buf[1].real;
    tmp3.imag = buf[0].imag - buf[1].imag;
    tmp4.imag = +buf[3].real - buf[2].real;
    tmp4.real = -buf[3].imag + buf[2].imag;

    buf[0].real = tmp1.real + tmp2.real;
    buf[0].imag = tmp1.imag + tmp2.imag;
    buf[1].real = tmp3.real + tmp4.real;
    buf[1].imag = tmp3.imag + tmp4.imag;

    buf[2].real = tmp1.real - tmp2.real;
    buf[2].imag = tmp1.imag - tmp2.imag;
    buf[3].real = tmp3.real - tmp4.real;
    buf[3].imag = tmp3.imag - tmp4.imag;
#endif
}

static void RENAME(imdct_512) (sample_t * data, sample_t * delay, sample_t bias)
{
    complex_t buf[128],t,a,b;
    sample_t w_1, w_2;
    const sample_t * window = a52_imdct_window;
    int i, k;
#ifdef HAVE_3DNOW
    const unsigned __attribute__((__aligned__(8))) minus_plus_3dnow[2] = { 0x00000000UL, 0x80000000UL };
#endif

#ifdef HAVE_SSE
#elif defined ( HAVE_3DNOW )
    for (i = 0; i < 128; i++) {
	k = fftorder[i];
	__asm __volatile(
	"movd	%1,%%mm0\n\t"
	"punpckldq %0, %%mm0\n\t" // data[k] | data[255-k]
	::"m"(data[k]),"m"(data[255-k]):"memory");
	__asm __volatile(
	"movq	%1, %%mm1\n\t" // t.real | t.imag
	"movq	%%mm1, %%mm2\n\t"// t.real | t.imag
	"pfmul	%%mm0, %%mm1\n\t"// t.real*data[k] | t.imag*data[255-k]
	PSWAP_MM("%%mm2","%%mm3")// t.imag | t.real
	"pfmul	%%mm0, %%mm2\n\t" // t.imag*data[k] | t.real*data[255-k]
	PFPNACC_MM("%%mm2","%%mm1","%2")
	PSWAP_MM("%%mm1","%%mm3")// t.real | t.imag
	"movq	%%mm1, %0"
	:"=m"(buf[i]):"m"(pre1[i]),"m"(minus_plus_3dnow[0]):"memory"
#ifdef FPU_CLOBBERED
	,FPU_CLOBBERED
#endif
#ifdef MMX_CLOBBERED
	,MMX_CLOBBERED
#endif
	);
    }
    __asm __volatile("femms":::"memory"
#ifdef FPU_CLOBBERED
			,FPU_CLOBBERED
#endif
#ifdef MMX_CLOBBERED
			,MMX_CLOBBERED
#endif
    );
#else
    for (i = 0; i < 128; i++) {
	k = fftorder[i];
	t.real = pre1[i].real;
	t.imag = pre1[i].imag;

	buf[i].real = t.imag * data[255-k] + t.real * data[k];
	buf[i].imag = t.real * data[255-k] - t.imag * data[k];
    }
#endif
    ifft128 (buf);
    /* Post IFFT complex multiply plus IFFT complex conjugate*/
    /* Window and convert to real valued signal */
    for (i = 0; i < 64; i++) {
	/* y[n] = z[n] * (xcos1[n] + j * xsin1[n]) ; */
	t.real = post1[i].real;
	t.imag = post1[i].imag;

	a.real = t.real * buf[i].real     + t.imag * buf[i].imag;
	a.imag = t.imag * buf[i].real     - t.real * buf[i].imag;
	b.real = t.imag * buf[127-i].real + t.real * buf[127-i].imag;
	b.imag = t.real * buf[127-i].real - t.imag * buf[127-i].imag;

	w_1 = window[2*i];
	w_2 = window[255-2*i];
	data[2*i]     = delay[2*i] * w_2 - a.real * w_1 + bias;
	data[255-2*i] = delay[2*i] * w_1 + a.real * w_2 + bias;
	delay[2*i] = a.imag;

	w_1 = window[2*i+1];
	w_2 = window[254-2*i];
	data[2*i+1]   = delay[2*i+1] * w_2 + b.real * w_1 + bias;
	data[254-2*i] = delay[2*i+1] * w_1 - b.real * w_2 + bias;
	delay[2*i+1] = b.imag;
    }
}

static void RENAME(imdct_256)(sample_t * data, sample_t * delay, sample_t bias)
{
    complex_t t,a,b,c,d;
    sample_t w_1, w_2;
    int i, k;
    const sample_t * window = a52_imdct_window;
    complex_t buf1[64], buf2[64];
#ifdef HAVE_3DNOW
    const unsigned __attribute__((__aligned__(8))) minus_plus_3dnow[2] = { 0x00000000UL, 0x80000000UL };
#endif

#ifdef HAVE_SSE
#elif defined ( HAVE_3DNOW )
    for (i = 0; i < 64; i++) {
	k = fftorder[i];
	__asm __volatile(
	"movd	%1,%%mm6\n\t"
	"punpckldq %0, %%mm6\n\t" // data[k] | data[254-k]
	::"m"(data[k]),"m"(data[254-k]):"memory");
	__asm __volatile(
	"movd	%1,%%mm7\n\t"
	"punpckldq %0, %%mm7\n\t" // data[k+1] | data[255-k]
	::"m"(data[k+1]),"m"(data[255-k]):"memory");
	__asm __volatile(
	"movq	%1, %%mm1\n\t" // t.real | t.imag
	"movq	%%mm1, %%mm2\n\t"// t.real | t.imag
	"movq	%%mm1, %%mm4\n\t"// t.real | t.imag
	"movq	%%mm1, %%mm5\n\t"// t.real | t.imag
	"pfmul	%%mm6, %%mm1\n\t"// t.real*data[k] | t.imag*data[254-k]
	PSWAP_MM("%%mm2","%%mm3")// t.imag | t.real
	"pfmul	%%mm6, %%mm2\n\t" // t.imag*data[k] | t.real*data[254-k]
	PFPNACC_MM("%%mm2","%%mm1","%2")
	PSWAP_MM("%%mm1","%%mm3")// t.real | t.imag
	"movq	%%mm1, %0"
	:"=m"(buf1[i]):"m"(pre2[i]),"m"(minus_plus_3dnow[0]):"memory");
	__asm __volatile(
	"pfmul	%%mm7, %%mm4\n\t"// t.real*data[k+1] | t.imag*data[255-k]
	PSWAP_MM("%%mm5","%%mm3")// t.imag | t.real
	"pfmul	%%mm7, %%mm5\n\t" // t.imag*data[k+1] | t.real*data[255-k]
	PFPNACC_MM("%%mm5","%%mm4","%1")
	PSWAP_MM("%%mm4","%%mm3")// t.real | t.imag
	"movq	%%mm4, %0"
	:"=m"(buf2[i]):"m"(minus_plus_3dnow[0]):"memory"
#ifdef FPU_CLOBBERED
	,FPU_CLOBBERED
#endif
#ifdef MMX_CLOBBERED
	,MMX_CLOBBERED
#endif
	);
    }
    __asm __volatile("femms":::"memory"
#ifdef FPU_CLOBBERED
			,FPU_CLOBBERED
#endif
#ifdef MMX_CLOBBERED
			,MMX_CLOBBERED
#endif
    );
#else
    /* Pre IFFT complex multiply plus IFFT cmplx conjugate */
    for (i = 0; i < 64; i++) {
	k = fftorder[i];
	t.real = pre2[i].real;
	t.imag = pre2[i].imag;

	buf1[i].real = t.imag * data[254-k] + t.real * data[k];
	buf1[i].imag = t.real * data[254-k] - t.imag * data[k];

	buf2[i].real = t.imag * data[255-k] + t.real * data[k+1];
	buf2[i].imag = t.real * data[255-k] - t.imag * data[k+1];
    }
#endif
    ifft64 (buf1);
    ifft64 (buf2);

    /* Post IFFT complex multiply */
    /* Window and convert to real valued signal */
    for (i = 0; i < 32; i++) {
	/* y1[n] = z1[n] * (xcos2[n] + j * xs in2[n]) ; */
	t.real = post2[i].real;
	t.imag = post2[i].imag;

	a.real = t.real * buf1[i].real    + t.imag * buf1[i].imag;
	a.imag = t.imag * buf1[i].real    - t.real * buf1[i].imag;
	b.real = t.imag * buf1[63-i].real + t.real * buf1[63-i].imag;
	b.imag = t.real * buf1[63-i].real - t.imag * buf1[63-i].imag;

	c.real = t.real * buf2[i].real    + t.imag * buf2[i].imag;
	c.imag = t.imag * buf2[i].real    - t.real * buf2[i].imag;
	d.real = t.imag * buf2[63-i].real + t.real * buf2[63-i].imag;
	d.imag = t.real * buf2[63-i].real - t.imag * buf2[63-i].imag;

	w_1 = window[2*i];
	w_2 = window[255-2*i];
	data[2*i]     = delay[2*i] * w_2 - a.real * w_1 + bias;
	data[255-2*i] = delay[2*i] * w_1 + a.real * w_2 + bias;
	delay[2*i] = c.imag;

	w_1 = window[128+2*i];
	w_2 = window[127-2*i];
	data[128+2*i] = delay[127-2*i] * w_2 + a.imag * w_1 + bias;
	data[127-2*i] = delay[127-2*i] * w_1 - a.imag * w_2 + bias;
	delay[127-2*i] = c.real;

	w_1 = window[2*i+1];
	w_2 = window[254-2*i];
	data[2*i+1]   = delay[2*i+1] * w_2 - b.imag * w_1 + bias;
	data[254-2*i] = delay[2*i+1] * w_1 + b.imag * w_2 + bias;
	delay[2*i+1] = d.real;

	w_1 = window[129+2*i];
	w_2 = window[126-2*i];
	data[129+2*i] = delay[126-2*i] * w_2 + b.real * w_1 + bias;
	data[126-2*i] = delay[126-2*i] * w_1 - b.real * w_2 + bias;
	delay[126-2*i] = d.imag;
    }
}
