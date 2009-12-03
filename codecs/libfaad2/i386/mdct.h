void RENAME(faad_imdct)(mdct_info *mdct, real_t *X_in, real_t *X_out)
{
    uint16_t k;

    complex_t x;
    ALIGN complex_t Z1[512];
    complex_t *sincos = mdct->sincos;

    uint16_t N  = mdct->N;
    uint16_t N2 = N >> 1;
    uint16_t N4 = N >> 2;
    uint16_t N8 = N >> 3;

#ifdef PROFILE
    int64_t count1, count2 = faad_get_ts();
#endif

    /* pre-IFFT complex multiplication */
    for (k = 0; k < N4; k++)
    {
	IM(Z1[k])=X_in[2*k]*RE(sincos[k])+X_in[N2 - 1 - 2*k]*IM(sincos[k]);
	RE(Z1[k])=X_in[N2 - 1 - 2*k]*RE(sincos[k])-X_in[2*k]*IM(sincos[k]);
    }

#ifdef PROFILE
    count1 = faad_get_ts();
#endif

    /* complex IFFT, any non-scaling FFT can be used here */
    cfftb(mdct->cfft, Z1);

#ifdef PROFILE
    count1 = faad_get_ts() - count1;
#endif

    /* post-IFFT complex multiplication */
    for (k = 0; k < N4; k++)
    {
        RE(x) = RE(Z1[k]);
        IM(x) = IM(Z1[k]);
	IM(Z1[k])=IM(x)*RE(sincos[k])+RE(x)*IM(sincos[k]);
	RE(Z1[k])=RE(x)*RE(sincos[k])-IM(x)*IM(sincos[k]);
    }

    /* reordering */
    for (k = 0; k < N8; k+=2)
    {
        X_out[              2*k] =  IM(Z1[N8 +     k]);
        X_out[          2 + 2*k] =  IM(Z1[N8 + 1 + k]);

        X_out[          1 + 2*k] = -RE(Z1[N8 - 1 - k]);
        X_out[          3 + 2*k] = -RE(Z1[N8 - 2 - k]);

        X_out[N4 +          2*k] =  RE(Z1[         k]);
        X_out[N4 +    + 2 + 2*k] =  RE(Z1[     1 + k]);

        X_out[N4 +      1 + 2*k] = -IM(Z1[N4 - 1 - k]);
        X_out[N4 +      3 + 2*k] = -IM(Z1[N4 - 2 - k]);

        X_out[N2 +          2*k] =  RE(Z1[N8 +     k]);
        X_out[N2 +    + 2 + 2*k] =  RE(Z1[N8 + 1 + k]);

        X_out[N2 +      1 + 2*k] = -IM(Z1[N8 - 1 - k]);
        X_out[N2 +      3 + 2*k] = -IM(Z1[N8 - 2 - k]);

        X_out[N2 + N4 +     2*k] = -IM(Z1[         k]);
        X_out[N2 + N4 + 2 + 2*k] = -IM(Z1[     1 + k]);

        X_out[N2 + N4 + 1 + 2*k] =  RE(Z1[N4 - 1 - k]);
        X_out[N2 + N4 + 3 + 2*k] =  RE(Z1[N4 - 2 - k]);
    }

#ifdef PROFILE
    count2 = faad_get_ts() - count2;
    mdct->fft_cycles += count1;
    mdct->cycles += (count2 - count1);
#endif
#ifdef HAVE_3DNOW
    __asm __volatile("femms"::
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

#ifdef LTP_DEC
void RENAME(faad_mdct)(mdct_info *mdct, real_t *X_in, real_t *X_out)
{
    uint16_t k;

    complex_t x;
    ALIGN complex_t Z1[512];
    complex_t *sincos = mdct->sincos;

    uint16_t N  = mdct->N;
    uint16_t N2 = N >> 1;
    uint16_t N4 = N >> 2;
    uint16_t N8 = N >> 3;

#ifndef FIXED_POINT
	real_t scale = REAL_CONST(N);
#else
	real_t scale = REAL_CONST(4.0/N);
#endif

    /* pre-FFT complex multiplication */
    for (k = 0; k < N8; k++)
    {
        uint16_t n = k << 1;
        RE(x) = X_in[N - N4 - 1 - n] + X_in[N - N4 +     n];
        IM(x) = X_in[    N4 +     n] - X_in[    N4 - 1 - n];

        ComplexMult(&RE(Z1[k]), &IM(Z1[k]),
            RE(x), IM(x), RE(sincos[k]), IM(sincos[k]));

        RE(Z1[k]) = MUL_R(RE(Z1[k]), scale);
        IM(Z1[k]) = MUL_R(IM(Z1[k]), scale);

        RE(x) =  X_in[N2 - 1 - n] - X_in[        n];
        IM(x) =  X_in[N2 +     n] + X_in[N - 1 - n];

        ComplexMult(&RE(Z1[k + N8]), &IM(Z1[k + N8]),
            RE(x), IM(x), RE(sincos[k + N8]), IM(sincos[k + N8]));

        RE(Z1[k + N8]) = MUL_R(RE(Z1[k + N8]), scale);
        IM(Z1[k + N8]) = MUL_R(IM(Z1[k + N8]), scale);
    }

    /* complex FFT, any non-scaling FFT can be used here  */
    cfftf(mdct->cfft, Z1);

    /* post-FFT complex multiplication */
    for (k = 0; k < N4; k++)
    {
        uint16_t n = k << 1;
        ComplexMult(&RE(x), &IM(x),
            RE(Z1[k]), IM(Z1[k]), RE(sincos[k]), IM(sincos[k]));

        X_out[         n] = -RE(x);
        X_out[N2 - 1 - n] =  IM(x);
        X_out[N2 +     n] = -IM(x);
        X_out[N  - 1 - n] =  RE(x);
    }
}
#endif
