/*
** FAAD2 - Freeware Advanced Audio (AAC) Decoder including SBR decoding
** Copyright (C) 2003 M. Bakker, Ahead Software AG, http://www.nero.com
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
** $Id: output.c,v 1.5 2007/12/14 08:51:08 nickols_k Exp $
** detailed CVS changelog at http://www.mplayerhq.hu/cgi-bin/cvsweb.cgi/main/
**/

#include "common.h"
#include "structs.h"

#include "output.h"
#include "decoder.h"
#include "../mm_accel.h"

extern unsigned faad_cpu_flags;

#ifndef FIXED_POINT


#define FLOAT_SCALE (1.0f/(1<<15))

#define DM_MUL REAL_CONST(0.3203772410170407) // 1/(1+sqrt(2) + 1/sqrt(2))
#define RSQRT2 REAL_CONST(0.7071067811865475244) // 1/sqrt(2)


static INLINE real_t get_sample(real_t **input, uint8_t channel, uint16_t sample,
                                uint8_t down_matrix, uint8_t *internal_channel)
{
    if (!down_matrix)
        return input[internal_channel[channel]][sample];

    if (channel == 0)
    {
        return DM_MUL * (input[internal_channel[1]][sample] +
            input[internal_channel[0]][sample] * RSQRT2 +
            input[internal_channel[3]][sample] * RSQRT2);
    } else {
        return DM_MUL * (input[internal_channel[2]][sample] +
            input[internal_channel[0]][sample] * RSQRT2 +
            input[internal_channel[4]][sample] * RSQRT2);
    }
}

#ifndef HAS_LRINTF
#define CLIP(sample, max, min) \
if (sample >= 0.0f)            \
{                              \
    sample += 0.5f;            \
    if (sample >= max)         \
        sample = max;          \
} else {                       \
    sample += -0.5f;           \
    if (sample <= min)         \
        sample = min;          \
}
#else
#define CLIP(sample, max, min) \
if (sample >= 0.0f)            \
{                              \
    if (sample >= max)         \
        sample = max;          \
} else {                       \
    if (sample <= min)         \
        sample = min;          \
}
#endif

#define CONV(a,b) ((a<<1)|(b&0x1))

#undef HAVE_MMX
#undef HAVE_MMX2
#undef HAVE_3DNOW
#undef HAVE_3DNOW2
#undef HAVE_SSE
#define RENAME(a) a ## _c
#include "i386/output.h"

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
#include "i386/output.h"
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
#include "i386/output.h"
#endif

#endif // CAN_COMPILE_X86_ASM

static void (*to_PCM_16bit)(faacDecHandle hDecoder, real_t **input,
                         uint8_t channels, uint16_t frame_len,
                         int16_t **sample_buffer);
static void to_PCM_16bit_init(faacDecHandle hDecoder, real_t **input,
                         uint8_t channels, uint16_t frame_len,
                         int16_t **sample_buffer)
{
#ifdef CAN_COMPILE_X86_ASM
#ifdef CAN_COMPILE_SSE
	if(faad_cpu_flags & MM_ACCEL_X86_SSE) to_PCM_16bit = to_PCM_16bit_SSE;
	else
#endif
#ifdef CAN_COMPILE_3DNOW
	if(faad_cpu_flags & MM_ACCEL_X86_3DNOW) to_PCM_16bit = to_PCM_16bit_3DNow;
	else
#endif
#endif //CAN_COMPILE_X86_ASM
	to_PCM_16bit = to_PCM_16bit_c;
	(*to_PCM_16bit)(hDecoder,input,channels,frame_len,sample_buffer);
}
static void (*to_PCM_16bit)(faacDecHandle hDecoder, real_t **input,
                         uint8_t channels, uint16_t frame_len,
                         int16_t **sample_buffer)=to_PCM_16bit_init;

static void (*to_PCM_24bit)(faacDecHandle hDecoder, real_t **input,
                         uint8_t channels, uint16_t frame_len,
                         int32_t **sample_buffer);
static void to_PCM_24bit_init(faacDecHandle hDecoder, real_t **input,
                         uint8_t channels, uint16_t frame_len,
                         int32_t **sample_buffer)
{
#ifdef CAN_COMPILE_X86_ASM
#ifdef CAN_COMPILE_SSE
	if(faad_cpu_flags & MM_ACCEL_X86_SSE) to_PCM_24bit = to_PCM_24bit_SSE;
	else
#endif
#ifdef CAN_COMPILE_3DNOW
	if(faad_cpu_flags & MM_ACCEL_X86_3DNOW) to_PCM_24bit = to_PCM_24bit_3DNow;
	else
#endif
#endif //CAN_COMPILE_X86_ASM
	to_PCM_24bit = to_PCM_24bit_c;
	(*to_PCM_24bit)(hDecoder,input,channels,frame_len,sample_buffer);
}
static void (*to_PCM_24bit)(faacDecHandle hDecoder, real_t **input,
                         uint8_t channels, uint16_t frame_len,
                         int32_t **sample_buffer)=to_PCM_24bit_init;

static void (*to_PCM_32bit)(faacDecHandle hDecoder, real_t **input,
                         uint8_t channels, uint16_t frame_len,
                         int32_t **sample_buffer);
static void to_PCM_32bit_init(faacDecHandle hDecoder, real_t **input,
                         uint8_t channels, uint16_t frame_len,
                         int32_t **sample_buffer)
{
#ifdef CAN_COMPILE_X86_ASM
#ifdef CAN_COMPILE_SSE
	if(faad_cpu_flags & MM_ACCEL_X86_SSE) to_PCM_32bit = to_PCM_32bit_SSE;
	else
#endif
#ifdef CAN_COMPILE_3DNOW
	if(faad_cpu_flags & MM_ACCEL_X86_3DNOW) to_PCM_32bit = to_PCM_32bit_3DNow;
	else
#endif
#endif //CAN_COMPILE_X86_ASM
	to_PCM_32bit = to_PCM_32bit_c;
	(*to_PCM_32bit)(hDecoder,input,channels,frame_len,sample_buffer);
}
static void (*to_PCM_32bit)(faacDecHandle hDecoder, real_t **input,
                         uint8_t channels, uint16_t frame_len,
                         int32_t **sample_buffer)=to_PCM_32bit_init;

static void (*to_PCM_float)(faacDecHandle hDecoder, real_t **input,
                         uint8_t channels, uint16_t frame_len,
                         float32_t **sample_buffer);
static void to_PCM_float_init(faacDecHandle hDecoder, real_t **input,
                         uint8_t channels, uint16_t frame_len,
                         float32_t **sample_buffer)
{
#ifdef CAN_COMPILE_X86_ASM
#ifdef CAN_COMPILE_SSE
	if(faad_cpu_flags & MM_ACCEL_X86_SSE) to_PCM_float = to_PCM_float_SSE;
	else
#endif
#ifdef CAN_COMPILE_3DNOW
	if(faad_cpu_flags & MM_ACCEL_X86_3DNOW) to_PCM_float = to_PCM_float_3DNow;
	else
#endif
#endif //CAN_COMPILE_X86_ASM
	to_PCM_float = to_PCM_float_c;
	(*to_PCM_float)(hDecoder,input,channels,frame_len,sample_buffer);
}
static void (*to_PCM_float)(faacDecHandle hDecoder, real_t **input,
                         uint8_t channels, uint16_t frame_len,
                         float32_t **sample_buffer)=to_PCM_float_init;


static void to_PCM_double(faacDecHandle hDecoder, real_t **input,
                          uint8_t channels, uint16_t frame_len,
                          double **sample_buffer)
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
            (*sample_buffer)[i] = (double)inp*FLOAT_SCALE;
        }
        break;
    case CONV(2,0):
        ch  = hDecoder->internal_channel[0];
        ch1 = hDecoder->internal_channel[1];
        for(i = 0; i < frame_len; i++)
        {
            real_t inp0 = input[ch ][i];
            real_t inp1 = input[ch1][i];
            (*sample_buffer)[(i*2)+0] = (double)inp0*FLOAT_SCALE;
            (*sample_buffer)[(i*2)+1] = (double)inp1*FLOAT_SCALE;
        }
        break;
    default:
        for (ch = 0; ch < channels; ch++)
        {
            for(i = 0; i < frame_len; i++)
            {
                real_t inp = get_sample(input, ch, i, hDecoder->downMatrix, hDecoder->internal_channel);
                (*sample_buffer)[(i*channels)+ch] = (double)inp*FLOAT_SCALE;
            }
        }
        break;
    }
}

void *output_to_PCM(faacDecHandle hDecoder,
                    real_t **input, void *sample_buffer, uint8_t channels,
                    uint16_t frame_len, uint8_t format)
{
    int16_t   *short_sample_buffer = (int16_t*)sample_buffer;
    int32_t   *int_sample_buffer = (int32_t*)sample_buffer;
    float32_t *float_sample_buffer = (float32_t*)sample_buffer;
    double    *double_sample_buffer = (double*)sample_buffer;

#ifdef PROFILE
    int64_t count = faad_get_ts();
#endif

    /* Copy output to a standard PCM buffer */
    switch (format)
    {
    case FAAD_FMT_16BIT:
        to_PCM_16bit(hDecoder, input, channels, frame_len, &short_sample_buffer);
        break;
    case FAAD_FMT_24BIT:
        to_PCM_24bit(hDecoder, input, channels, frame_len, &int_sample_buffer);
        break;
    case FAAD_FMT_32BIT:
        to_PCM_32bit(hDecoder, input, channels, frame_len, &int_sample_buffer);
        break;
    case FAAD_FMT_FLOAT:
        to_PCM_float(hDecoder, input, channels, frame_len, &float_sample_buffer);
        break;
    case FAAD_FMT_DOUBLE:
        to_PCM_double(hDecoder, input, channels, frame_len, &double_sample_buffer);
        break;
    }

#ifdef PROFILE
    count = faad_get_ts() - count;
    hDecoder->output_cycles += count;
#endif

    return sample_buffer;
}

#else

#define DM_MUL FRAC_CONST(0.3203772410170407) // 1/(1+sqrt(2) + 1/sqrt(2))
#define RSQRT2 FRAC_CONST(0.7071067811865475244) // 1/sqrt(2)

static INLINE real_t get_sample(real_t **input, uint8_t channel, uint16_t sample,
                                uint8_t down_matrix, uint8_t *internal_channel)
{
    if (!down_matrix)
        return input[internal_channel[channel]][sample];

    if (channel == 0)
    {
        real_t C   = MUL_F(input[internal_channel[0]][sample], RSQRT2);
        real_t L_S = MUL_F(input[internal_channel[3]][sample], RSQRT2);
        real_t cum = input[internal_channel[1]][sample] + C + L_S;
        return MUL_F(cum, DM_MUL);
    } else {
        real_t C   = MUL_F(input[internal_channel[0]][sample], RSQRT2);
        real_t R_S = MUL_F(input[internal_channel[4]][sample], RSQRT2);
        real_t cum = input[internal_channel[2]][sample] + C + R_S;
        return MUL_F(cum, DM_MUL);
    }
}

void* output_to_PCM(faacDecHandle hDecoder,
                    real_t **input, void *sample_buffer, uint8_t channels,
                    uint16_t frame_len, uint8_t format)
{
    uint8_t ch;
    uint16_t i;
    int16_t *short_sample_buffer = (int16_t*)sample_buffer;
    int32_t *int_sample_buffer = (int32_t*)sample_buffer;

    /* Copy output to a standard PCM buffer */
    for (ch = 0; ch < channels; ch++)
    {
        switch (format)
        {
        case FAAD_FMT_16BIT:
            for(i = 0; i < frame_len; i++)
            {
                //int32_t tmp = input[ch][i];
                int32_t tmp = get_sample(input, ch, i, hDecoder->downMatrix, hDecoder->internal_channel);
                if (tmp >= 0)
                {
                    tmp += (1 << (REAL_BITS-1));
                    if (tmp >= REAL_CONST(32767))
                    {
                        tmp = REAL_CONST(32767);
                    }
                } else {
                    tmp += -(1 << (REAL_BITS-1));
                    if (tmp <= REAL_CONST(-32768))
                    {
                        tmp = REAL_CONST(-32768);
                    }
                }
                tmp >>= REAL_BITS;
                short_sample_buffer[(i*channels)+ch] = (int16_t)tmp;
            }
            break;
        case FAAD_FMT_24BIT:
            for(i = 0; i < frame_len; i++)
            {
                //int32_t tmp = input[ch][i];
                int32_t tmp = get_sample(input, ch, i, hDecoder->downMatrix, hDecoder->internal_channel);
                if (tmp >= 0)
                {
                    tmp += (1 << (REAL_BITS-9));
                    tmp >>= (REAL_BITS-8);
                    if (tmp >= 8388607)
                    {
                        tmp = 8388607;
                    }
                } else {
                    tmp += -(1 << (REAL_BITS-9));
                    tmp >>= (REAL_BITS-8);
                    if (tmp <= -8388608)
                    {
                        tmp = -8388608;
                    }
                }
                int_sample_buffer[(i*channels)+ch] = (int32_t)tmp;
            }
            break;
        case FAAD_FMT_32BIT:
            for(i = 0; i < frame_len; i++)
            {
                //int32_t tmp = input[ch][i];
                int32_t tmp = get_sample(input, ch, i, hDecoder->downMatrix, hDecoder->internal_channel);
                if (tmp >= 0)
                {
                    tmp += (1 << (16-REAL_BITS-1));
                    tmp <<= (16-REAL_BITS);
                } else {
                    tmp += -(1 << (16-REAL_BITS-1));
                    tmp <<= (16-REAL_BITS);
                }
                int_sample_buffer[(i*channels)+ch] = (int32_t)tmp;
            }
            break;
        }
    }

    return sample_buffer;
}

#endif
