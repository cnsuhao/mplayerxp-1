void RENAME(apply_scalefactors)(faacDecHandle hDecoder, ic_stream *ics,
                        real_t *x_invquant, uint16_t frame_len)
{
    uint8_t g, sfb;
#ifdef HAVE_MMX
    uint16_t top_mm;
#endif
    uint16_t top;
    int32_t exp, frac;
    uint8_t groups = 0;
    uint16_t nshort = frame_len/8;

    for (g = 0; g < ics->num_window_groups; g++)
    {
        uint16_t k;
	k = 0;

        /* using this nshort*groups doesn't hurt long blocks, because
           long blocks only have 1 group, so that means 'groups' is
           always 0 for long blocks
        */
        for (sfb = 0; sfb < ics->max_sfb; sfb++)
        {
            top = ics->sect_sfb_offset[g][sfb+1];

            /* this could be scalefactor for IS or PNS, those can be negative or bigger then 255 */
            /* just ignore them */
            if (ics->scale_factors[g][sfb] < 0 || ics->scale_factors[g][sfb] > 255)
            {
                exp = 0;
                frac = 0;
            } else {
                /* ics->scale_factors[g][sfb] must be between 0 and 255 */
                exp = (ics->scale_factors[g][sfb] /* - 100 */) >> 2;
                frac = (ics->scale_factors[g][sfb] /* - 100 */) & 3;
            }

#ifdef FIXED_POINT
            exp -= 25;
            /* IMDCT pre-scaling */
            if (hDecoder->object_type == LD)
            {
                exp -= 6 /*9*/;
            } else {
                if (ics->window_sequence == EIGHT_SHORT_SEQUENCE)
                    exp -= 4 /*7*/;
                else
                    exp -= 7 /*10*/;
            }
#endif

#ifdef HAVE_SSE
		__asm __volatile(
		    "movss	%0, %%xmm2\n\t"
		    "shufps	$0, %%xmm2, %%xmm2\n\t"
		    "movss	%1, %%xmm3\n\t"
		    "shufps	$0, %%xmm3, %%xmm3\n\t"
		    ::"m"(pow2sf_tab[exp]),"m"(pow2_table[frac])
		    :"memory"
#ifdef SSE_CLOBBERED
		    ,SSE_CLOBBERED
#endif
		    );
#elif defined(HAVE_3DNOW)
		__asm __volatile(
		    "movd	%0, %%mm4\n\t"
		    "movd	%1, %%mm5\n\t"
		    "punpckldq	%%mm4, %%mm4\n\t"
		    "punpckldq	%%mm5, %%mm5\n\t"
		    ::"m"(pow2sf_tab[exp]),"m"(pow2_table[frac])
		    :"memory"
#ifdef FPU_CLOBBERED
		    ,FPU_CLOBBERED
#endif
#ifdef MMX_CLOBBERED
		    ,MMX_CLOBBERED
#endif
		    );
#endif
#ifdef HAVE_SSE
	    top_mm = top & (~3);
            for ( ; k < top_mm; k += 4)
            {
		__asm __volatile(
		    "movups	(%0), %%xmm0\n\t"
		    "mulps	%%xmm2, %%xmm0\n\t"
		    "mulps	%%xmm3, %%xmm0\n\t"
		    "movups	%%xmm0, (%0)\n\t"
		    ::"g"(&x_invquant[k+(groups*nshort)])
		    :"memory"
#ifdef SSE_CLOBBERED
		    ,SSE_CLOBBERED
#endif
		    );
            }
#elif defined(HAVE_3DNOW)
	    top_mm = top & (~3);
            for ( ; k < top_mm; k += 4)
            {
		__asm __volatile(
		    "movq	(%0), %%mm0\n\t"
		    "movq	8(%0), %%mm1\n\t"
		    "pfmul	%%mm4, %%mm0\n\t"
		    "pfmul	%%mm4, %%mm1\n\t"
		    "pfmul	%%mm5, %%mm0\n\t"
		    "pfmul	%%mm5, %%mm1\n\t"
		    "movq	%%mm0, (%0)\n\t"
		    "movq	%%mm1, 8(%0)\n\t"
		    ::"g"(&x_invquant[k+(groups*nshort)])
		    :"memory"
#ifdef FPU_CLOBBERED
		    ,FPU_CLOBBERED
#endif
#ifdef MMX_CLOBBERED
		    ,MMX_CLOBBERED
#endif
		    );
            }
    __asm __volatile("femms":::"memory");
#endif
            /* minimum size of a sf band is 4 and always a multiple of 4 */
            for ( ; k < top; k += 4)
            {
#ifdef FIXED_POINT
                if (exp < 0)
                {
                    x_invquant[k+(groups*nshort)] >>= -exp;
                    x_invquant[k+(groups*nshort)+1] >>= -exp;
                    x_invquant[k+(groups*nshort)+2] >>= -exp;
                    x_invquant[k+(groups*nshort)+3] >>= -exp;
                } else {
                    x_invquant[k+(groups*nshort)] <<= exp;
                    x_invquant[k+(groups*nshort)+1] <<= exp;
                    x_invquant[k+(groups*nshort)+2] <<= exp;
                    x_invquant[k+(groups*nshort)+3] <<= exp;
                }
#else
                x_invquant[k+(groups*nshort)]   = x_invquant[k+(groups*nshort)]   * pow2sf_tab[exp/*+25*/];
                x_invquant[k+(groups*nshort)+1] = x_invquant[k+(groups*nshort)+1] * pow2sf_tab[exp/*+25*/];
                x_invquant[k+(groups*nshort)+2] = x_invquant[k+(groups*nshort)+2] * pow2sf_tab[exp/*+25*/];
                x_invquant[k+(groups*nshort)+3] = x_invquant[k+(groups*nshort)+3] * pow2sf_tab[exp/*+25*/];
#endif

                x_invquant[k+(groups*nshort)]   = MUL_C(x_invquant[k+(groups*nshort)],pow2_table[frac /* + 3*/]);
                x_invquant[k+(groups*nshort)+1] = MUL_C(x_invquant[k+(groups*nshort)+1],pow2_table[frac /* + 3*/]);
                x_invquant[k+(groups*nshort)+2] = MUL_C(x_invquant[k+(groups*nshort)+2],pow2_table[frac /* + 3*/]);
                x_invquant[k+(groups*nshort)+3] = MUL_C(x_invquant[k+(groups*nshort)+3],pow2_table[frac /* + 3*/]);
            }
        }
        groups += ics->window_group_length[g];
    }
}
