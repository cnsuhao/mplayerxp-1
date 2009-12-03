static void RENAME(to_PCM_16bit)(faacDecHandle hDecoder, real_t **input,
                         uint8_t channels, uint16_t frame_len,
                         int16_t **sample_buffer)
{
    uint8_t ch, ch1;
    uint16_t i;

    switch (CONV(channels,hDecoder->downMatrix))
    {
    case CONV(1,0):
    case CONV(1,1):
        for(i = 0; i < frame_len; i++)
        {
            real_t inp = input[hDecoder->internal_channel[0]][i];

            CLIP(inp, 32767.0f, -32768.0f);

            (*sample_buffer)[i] = (int16_t)lrintf(inp);
        }
        break;
    case CONV(2,0):
        ch  = hDecoder->internal_channel[0];
        ch1 = hDecoder->internal_channel[1];
        for(i = 0; i < frame_len; i++)
        {
            real_t inp0 = input[ch ][i];
            real_t inp1 = input[ch1][i];

            CLIP(inp0, 32767.0f, -32768.0f);
            CLIP(inp1, 32767.0f, -32768.0f);

            (*sample_buffer)[(i*2)+0] = (int16_t)lrintf(inp0);
            (*sample_buffer)[(i*2)+1] = (int16_t)lrintf(inp1);
        }
        break;
    default:
        for (ch = 0; ch < channels; ch++)
        {
            for(i = 0; i < frame_len; i++)
            {
                real_t inp = get_sample(input, ch, i, hDecoder->downMatrix, hDecoder->internal_channel);

                CLIP(inp, 32767.0f, -32768.0f);

                (*sample_buffer)[(i*channels)+ch] = (int16_t)lrintf(inp);
            }
        }
        break;
    }
}

static void RENAME(to_PCM_24bit)(faacDecHandle hDecoder, real_t **input,
                         uint8_t channels, uint16_t frame_len,
                         int32_t **sample_buffer)
{
    uint8_t ch, ch1;
    uint16_t i;

    switch (CONV(channels,hDecoder->downMatrix))
    {
    case CONV(1,0):
    case CONV(1,1):
        for(i = 0; i < frame_len; i++)
        {
            real_t inp = input[hDecoder->internal_channel[0]][i];

            inp *= 256.0f;
            CLIP(inp, 8388607.0f, -8388608.0f);

            (*sample_buffer)[i] = (int32_t)lrintf(inp);
        }
        break;
    case CONV(2,0):
        ch  = hDecoder->internal_channel[0];
        ch1 = hDecoder->internal_channel[1];
        for(i = 0; i < frame_len; i++)
        {
            real_t inp0 = input[ch ][i];
            real_t inp1 = input[ch1][i];

            inp0 *= 256.0f;
            inp1 *= 256.0f;
            CLIP(inp0, 8388607.0f, -8388608.0f);
            CLIP(inp1, 8388607.0f, -8388608.0f);

            (*sample_buffer)[(i*2)+0] = (int32_t)lrintf(inp0);
            (*sample_buffer)[(i*2)+1] = (int32_t)lrintf(inp1);
        }
        break;
    default:
        for (ch = 0; ch < channels; ch++)
        {
            for(i = 0; i < frame_len; i++)
            {
                real_t inp = get_sample(input, ch, i, hDecoder->downMatrix, hDecoder->internal_channel);

                inp *= 256.0f;
                CLIP(inp, 8388607.0f, -8388608.0f);

                (*sample_buffer)[(i*channels)+ch] = (int32_t)lrintf(inp);
            }
        }
        break;
    }
}

static void RENAME(to_PCM_32bit)(faacDecHandle hDecoder, real_t **input,
                         uint8_t channels, uint16_t frame_len,
                         int32_t **sample_buffer)
{
    uint8_t ch, ch1;
    uint16_t i;

    switch (CONV(channels,hDecoder->downMatrix))
    {
    case CONV(1,0):
    case CONV(1,1):
        for(i = 0; i < frame_len; i++)
        {
            real_t inp = input[hDecoder->internal_channel[0]][i];

            inp *= 65536.0f;
            CLIP(inp, 2147483647.0f, -2147483648.0f);

            (*sample_buffer)[i] = (int32_t)lrintf(inp);
        }
        break;
    case CONV(2,0):
        ch  = hDecoder->internal_channel[0];
        ch1 = hDecoder->internal_channel[1];
        for(i = 0; i < frame_len; i++)
        {
            real_t inp0 = input[ch ][i];
            real_t inp1 = input[ch1][i];

            inp0 *= 65536.0f;
            inp1 *= 65536.0f;
            CLIP(inp0, 2147483647.0f, -2147483648.0f);
            CLIP(inp1, 2147483647.0f, -2147483648.0f);

            (*sample_buffer)[(i*2)+0] = (int32_t)lrintf(inp0);
            (*sample_buffer)[(i*2)+1] = (int32_t)lrintf(inp1);
        }
        break;
    default:
        for (ch = 0; ch < channels; ch++)
        {
            for(i = 0; i < frame_len; i++)
            {
                real_t inp = get_sample(input, ch, i, hDecoder->downMatrix, hDecoder->internal_channel);

                inp *= 65536.0f;
                CLIP(inp, 2147483647.0f, -2147483648.0f);

                (*sample_buffer)[(i*channels)+ch] = (int32_t)lrintf(inp);
            }
        }
        break;
    }
}

static void RENAME(to_PCM_float)(faacDecHandle hDecoder, real_t **input,
                         uint8_t channels, uint16_t frame_len,
                         float32_t **sample_buffer)
{
    uint8_t ch, ch1;
    uint16_t i;
#ifdef HAVE_SSE
	unsigned flen;
	float fscale = FLOAT_SCALE;
#elif defined( HAVE_3DNOW )
	unsigned flen;
	float __attribute__((aligned(16))) fscale[2] = { FLOAT_SCALE, FLOAT_SCALE };
#endif

    switch (CONV(channels,hDecoder->downMatrix))
    {
    case CONV(1,0):
    case CONV(1,1):
        for(i = 0; i < frame_len; i++)
        {
            real_t inp = input[hDecoder->internal_channel[0]][i];
            (*sample_buffer)[i] = inp*FLOAT_SCALE;
        }
        break;
    case CONV(2,0):
        ch  = hDecoder->internal_channel[0];
        ch1 = hDecoder->internal_channel[1];
	i=0;
#ifdef HAVE_SSE
	flen=frame_len&(~7);
	__asm __volatile(
	"movss %0, %%xmm7\n\t"
	"shufps $0, %%xmm7, %%xmm7\n\t" /* spread */
	::"m"(fscale)
	:"memory"
#ifdef SSE_CLOBBERED
	,SSE_CLOBBERED
#endif
	);
        for(; i < flen; i+=8)
        {
	    __asm __volatile(
		"movups	(%1), %%xmm0\n\t"
		"movups	16(%1), %%xmm2\n\t"
		"movups	(%2), %%xmm4\n\t"
		"movups	16(%2), %%xmm5\n\t"
		"movaps	%%xmm0, %%xmm1\n\t"
		"movaps	%%xmm2, %%xmm3\n\t"
		"unpcklps %%xmm4, %%xmm0\n\t"
		"unpckhps %%xmm4, %%xmm1\n\t"
		"unpcklps %%xmm5, %%xmm2\n\t"
		"unpckhps %%xmm5, %%xmm3\n\t"
		"mulps	%%xmm7, %%xmm0\n\t"
		"mulps	%%xmm7, %%xmm1\n\t"
		"mulps	%%xmm7, %%xmm2\n\t"
		"mulps	%%xmm7, %%xmm3\n\t"
		"movups	%%xmm0, (%0)\n\t"
		"movups	%%xmm1, 16(%0)\n\t"
		"movups	%%xmm2, 32(%0)\n\t"
		"movups	%%xmm3, 48(%0)\n\t"
	::"r"(&(*sample_buffer)[(i*2)]),"r"(&input[ch][i]),"r"(&input[ch1][i])
	:"memory"
#ifdef SSE_CLOBBERED
	,SSE_CLOBBERED
#endif
	);
        }
#elif defined ( HAVE_3DNOW )
	flen=frame_len&(~7);
        for(; i < flen; i+=8)
        {
	    __asm __volatile(
		"movq	(%1), %%mm0\n\t"
		"movq	8(%1), %%mm2\n\t"
		"movq	16(%1), %%mm4\n\t"
		"movq	24(%1), %%mm6\n\t"
		"movq	%%mm0, %%mm1\n\t"
		"movq	%%mm2, %%mm3\n\t"
		"movq	%%mm4, %%mm5\n\t"
		"movq	%%mm6, %%mm7\n\t"
		"punpckldq (%2), %%mm0\n\t"
		"punpckhdq (%2), %%mm1\n\t"
		"punpckldq 8(%2), %%mm2\n\t"
		"punpckhdq 8(%2), %%mm3\n\t"
		"punpckldq 16(%2), %%mm4\n\t"
		"punpckhdq 16(%2), %%mm5\n\t"
		"punpckldq 24(%2), %%mm6\n\t"
		"punpckhdq 24(%2), %%mm7\n\t"
		"pfmul	%3, %%mm0\n\t"
		"pfmul	%3, %%mm1\n\t"
		"pfmul	%3, %%mm2\n\t"
		"pfmul	%3, %%mm3\n\t"
		"pfmul	%3, %%mm4\n\t"
		"pfmul	%3, %%mm5\n\t"
		"pfmul	%3, %%mm6\n\t"
		"pfmul	%3, %%mm7\n\t"
		"movq	%%mm0, (%0)\n\t"
		"movq	%%mm1, 8(%0)\n\t"
		"movq	%%mm2, 16(%0)\n\t"
		"movq	%%mm3, 24(%0)\n\t"
		"movq	%%mm4, 32(%0)\n\t"
		"movq	%%mm5, 40(%0)\n\t"
		"movq	%%mm6, 48(%0)\n\t"
		"movq	%%mm7, 56(%0)\n\t"
	::"r"(&(*sample_buffer)[(i*2)]),"r"(&input[ch][i]),"r"(&input[ch1][i]),"m"(fscale[0])
	:"memory"
#ifdef FPU_CLOBBERED
	,FPU_CLOBBERED
#endif
#ifdef MMX_CLOBBERED
	,MMX_CLOBBERED
#endif
	    );
        }
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
        for(; i < frame_len; i++)
        {
            real_t inp0 = input[ch ][i];
            real_t inp1 = input[ch1][i];
            (*sample_buffer)[(i*2)+0] = inp0*FLOAT_SCALE;
            (*sample_buffer)[(i*2)+1] = inp1*FLOAT_SCALE;
        }
        break;
    default:
        for (ch = 0; ch < channels; ch++)
        {
            for(i = 0; i < frame_len; i++)
            {
                real_t inp = get_sample(input, ch, i, hDecoder->downMatrix, hDecoder->internal_channel);
                (*sample_buffer)[(i*channels)+ch] = inp*FLOAT_SCALE;
            }
        }
        break;
    }
}
