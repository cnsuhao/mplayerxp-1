#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
/*
 * downmix.c
 * Copyright (C) 2000-2002 Michel Lespinasse <walken@zoy.org>
 * Copyright (C) 1999-2000 Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
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
#include <string.h>
#include <inttypes.h>

#include "a52.h"
#include "a52_internal.h"
#include "osdep/mm_accel.h"
#include "osdep/cpudetect.h"

#define CONVERT(acmod,output) (((output) << 3) + (acmod))

extern uint32_t a52_accel;

int a52_downmix_init (int input, int flags, sample_t * level,
		      sample_t clev, sample_t slev)
{
    static uint8_t table[11][8] = {
	{A52_CHANNEL,	A52_DOLBY,	A52_STEREO,	A52_STEREO,
	 A52_STEREO,	A52_STEREO,	A52_STEREO,	A52_STEREO},
	{A52_MONO,	A52_MONO,	A52_MONO,	A52_MONO,
	 A52_MONO,	A52_MONO,	A52_MONO,	A52_MONO},
	{A52_CHANNEL,	A52_DOLBY,	A52_STEREO,	A52_STEREO,
	 A52_STEREO,	A52_STEREO,	A52_STEREO,	A52_STEREO},
	{A52_CHANNEL,	A52_DOLBY,	A52_STEREO,	A52_3F,
	 A52_STEREO,	A52_3F,		A52_STEREO,	A52_3F},
	{A52_CHANNEL,	A52_DOLBY,	A52_STEREO,	A52_STEREO,
	 A52_2F1R,	A52_2F1R,	A52_2F1R,	A52_2F1R},
	{A52_CHANNEL,	A52_DOLBY,	A52_STEREO,	A52_STEREO,
	 A52_2F1R,	A52_3F1R,	A52_2F1R,	A52_3F1R},
	{A52_CHANNEL,	A52_DOLBY,	A52_STEREO,	A52_3F,
	 A52_2F2R,	A52_2F2R,	A52_2F2R,	A52_2F2R},
	{A52_CHANNEL,	A52_DOLBY,	A52_STEREO,	A52_3F,
	 A52_2F2R,	A52_3F2R,	A52_2F2R,	A52_3F2R},
	{A52_CHANNEL1,	A52_MONO,	A52_MONO,	A52_MONO,
	 A52_MONO,	A52_MONO,	A52_MONO,	A52_MONO},
	{A52_CHANNEL2,	A52_MONO,	A52_MONO,	A52_MONO,
	 A52_MONO,	A52_MONO,	A52_MONO,	A52_MONO},
	{A52_CHANNEL,	A52_DOLBY,	A52_STEREO,	A52_DOLBY,
	 A52_DOLBY,	A52_DOLBY,	A52_DOLBY,	A52_DOLBY}
    };
    int output;

    output = flags & A52_CHANNEL_MASK;
    if (output > A52_DOLBY)
	return -1;

    output = table[output][input & 7];

    if (output == A52_STEREO &&
	(input == A52_DOLBY || (input == A52_3F && clev == LEVEL_3DB)))
	output = A52_DOLBY;

    if (flags & A52_ADJUST_LEVEL)
	switch (CONVERT (input & 7, output)) {

	case CONVERT (A52_3F, A52_MONO):
	    *level *= LEVEL_3DB / (1 + clev);
	    break;

	case CONVERT (A52_STEREO, A52_MONO):
	case CONVERT (A52_2F2R, A52_2F1R):
	case CONVERT (A52_3F2R, A52_3F1R):
	level_3db:
	    *level *= LEVEL_3DB;
	    break;

	case CONVERT (A52_3F2R, A52_2F1R):
	    if (clev < LEVEL_PLUS3DB - 1)
		goto level_3db;
	    /* break thru */
	case CONVERT (A52_3F, A52_STEREO):
	case CONVERT (A52_3F1R, A52_2F1R):
	case CONVERT (A52_3F1R, A52_2F2R):
	case CONVERT (A52_3F2R, A52_2F2R):
	    *level /= 1 + clev;
	    break;

	case CONVERT (A52_2F1R, A52_MONO):
	    *level *= LEVEL_PLUS3DB / (2 + slev);
	    break;

	case CONVERT (A52_2F1R, A52_STEREO):
	case CONVERT (A52_3F1R, A52_3F):
	    *level /= 1 + slev * LEVEL_3DB;
	    break;

	case CONVERT (A52_3F1R, A52_MONO):
	    *level *= LEVEL_3DB / (1 + clev + 0.5 * slev);
	    break;

	case CONVERT (A52_3F1R, A52_STEREO):
	    *level /= 1 + clev + slev * LEVEL_3DB;
	    break;

	case CONVERT (A52_2F2R, A52_MONO):
	    *level *= LEVEL_3DB / (1 + slev);
	    break;

	case CONVERT (A52_2F2R, A52_STEREO):
	case CONVERT (A52_3F2R, A52_3F):
	    *level /= 1 + slev;
	    break;

	case CONVERT (A52_3F2R, A52_MONO):
	    *level *= LEVEL_3DB / (1 + clev + slev);
	    break;

	case CONVERT (A52_3F2R, A52_STEREO):
	    *level /= 1 + clev + slev;
	    break;

	case CONVERT (A52_MONO, A52_DOLBY):
	    *level *= LEVEL_PLUS3DB;
	    break;

	case CONVERT (A52_3F, A52_DOLBY):
	case CONVERT (A52_2F1R, A52_DOLBY):
	    *level *= 1 / (1 + LEVEL_3DB);
	    break;

	case CONVERT (A52_3F1R, A52_DOLBY):
	case CONVERT (A52_2F2R, A52_DOLBY):
	    *level *= 1 / (1 + 2 * LEVEL_3DB);
	    break;

	case CONVERT (A52_3F2R, A52_DOLBY):
	    *level *= 1 / (1 + 3 * LEVEL_3DB);
	    break;
	default: break;
	}

    return output;
}

int a52_downmix_coeff (sample_t * coeff, int acmod, int output, sample_t level,
		       sample_t clev, sample_t slev)
{
    switch (CONVERT (acmod, output & A52_CHANNEL_MASK)) {

    case CONVERT (A52_CHANNEL, A52_CHANNEL):
    case CONVERT (A52_MONO, A52_MONO):
    case CONVERT (A52_STEREO, A52_STEREO):
    case CONVERT (A52_3F, A52_3F):
    case CONVERT (A52_2F1R, A52_2F1R):
    case CONVERT (A52_3F1R, A52_3F1R):
    case CONVERT (A52_2F2R, A52_2F2R):
    case CONVERT (A52_3F2R, A52_3F2R):
    case CONVERT (A52_STEREO, A52_DOLBY):
	coeff[0] = coeff[1] = coeff[2] = coeff[3] = coeff[4] = level;
	return 0;

    case CONVERT (A52_CHANNEL, A52_MONO):
	coeff[0] = coeff[1] = level * LEVEL_6DB;
	return 3;

    case CONVERT (A52_STEREO, A52_MONO):
	coeff[0] = coeff[1] = level * LEVEL_3DB;
	return 3;

    case CONVERT (A52_3F, A52_MONO):
	coeff[0] = coeff[2] = level * LEVEL_3DB;
	coeff[1] = level * clev * LEVEL_PLUS3DB;
	return 7;

    case CONVERT (A52_2F1R, A52_MONO):
	coeff[0] = coeff[1] = level * LEVEL_3DB;
	coeff[2] = level * slev * LEVEL_3DB;
	return 7;

    case CONVERT (A52_2F2R, A52_MONO):
	coeff[0] = coeff[1] = level * LEVEL_3DB;
	coeff[2] = coeff[3] = level * slev * LEVEL_3DB;
	return 15;

    case CONVERT (A52_3F1R, A52_MONO):
	coeff[0] = coeff[2] = level * LEVEL_3DB;
	coeff[1] = level * clev * LEVEL_PLUS3DB;
	coeff[3] = level * slev * LEVEL_3DB;
	return 15;

    case CONVERT (A52_3F2R, A52_MONO):
	coeff[0] = coeff[2] = level * LEVEL_3DB;
	coeff[1] = level * clev * LEVEL_PLUS3DB;
	coeff[3] = coeff[4] = level * slev * LEVEL_3DB;
	return 31;

    case CONVERT (A52_MONO, A52_DOLBY):
	coeff[0] = level * LEVEL_3DB;
	return 0;

    case CONVERT (A52_3F, A52_DOLBY):
	clev = LEVEL_3DB;
    case CONVERT (A52_3F, A52_STEREO):
    case CONVERT (A52_3F1R, A52_2F1R):
    case CONVERT (A52_3F2R, A52_2F2R):
	coeff[0] = coeff[2] = coeff[3] = coeff[4] = level;
	coeff[1] = level * clev;
	return 7;

    case CONVERT (A52_2F1R, A52_DOLBY):
	slev = 1;
    case CONVERT (A52_2F1R, A52_STEREO):
	coeff[0] = coeff[1] = level;
	coeff[2] = level * slev * LEVEL_3DB;
	return 7;

    case CONVERT (A52_3F1R, A52_DOLBY):
	clev = LEVEL_3DB;
	slev = 1;
    case CONVERT (A52_3F1R, A52_STEREO):
	coeff[0] = coeff[2] = level;
	coeff[1] = level * clev;
	coeff[3] = level * slev * LEVEL_3DB;
	return 15;

    case CONVERT (A52_2F2R, A52_DOLBY):
	slev = LEVEL_3DB;
    case CONVERT (A52_2F2R, A52_STEREO):
	coeff[0] = coeff[1] = level;
	coeff[2] = coeff[3] = level * slev;
	return 15;

    case CONVERT (A52_3F2R, A52_DOLBY):
	clev = LEVEL_3DB;
    case CONVERT (A52_3F2R, A52_2F1R):
	slev = LEVEL_3DB;
    case CONVERT (A52_3F2R, A52_STEREO):
	coeff[0] = coeff[2] = level;
	coeff[1] = level * clev;
	coeff[3] = coeff[4] = level * slev;
	return 31;

    case CONVERT (A52_3F1R, A52_3F):
	coeff[0] = coeff[1] = coeff[2] = level;
	coeff[3] = level * slev * LEVEL_3DB;
	return 13;

    case CONVERT (A52_3F2R, A52_3F):
	coeff[0] = coeff[1] = coeff[2] = level;
	coeff[3] = coeff[4] = level * slev;
	return 29;

    case CONVERT (A52_2F2R, A52_2F1R):
	coeff[0] = coeff[1] = level;
	coeff[2] = coeff[3] = level * LEVEL_3DB;
	return 12;

    case CONVERT (A52_3F2R, A52_3F1R):
	coeff[0] = coeff[1] = coeff[2] = level;
	coeff[3] = coeff[4] = level * LEVEL_3DB;
	return 24;

    case CONVERT (A52_2F1R, A52_2F2R):
	coeff[0] = coeff[1] = level;
	coeff[2] = level * LEVEL_3DB;
	return 0;

    case CONVERT (A52_3F1R, A52_2F2R):
	coeff[0] = coeff[2] = level;
	coeff[1] = level * clev;
	coeff[3] = level * LEVEL_3DB;
	return 7;

    case CONVERT (A52_3F1R, A52_3F2R):
	coeff[0] = coeff[1] = coeff[2] = level;
	coeff[3] = level * LEVEL_3DB;
	return 0;

    case CONVERT (A52_CHANNEL, A52_CHANNEL1):
	coeff[0] = level;
	coeff[1] = 0;
	return 0;

    case CONVERT (A52_CHANNEL, A52_CHANNEL2):
	coeff[0] = 0;
	coeff[1] = level;
	return 0;
    }

    return -1;	/* NOTREACHED */
}

#undef HAVE_MMX
#undef HAVE_MMX2
#undef HAVE_3DNOW
#undef HAVE_3DNOW2
#undef HAVE_SSE
#define RENAME(a) a ## _c
#include "downmix_mmx.h"

#ifdef ARCH_X86
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
#include "downmix_mmx.h"
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
#include "downmix_mmx.h"
#endif

#endif // CAN_COMPILE_X86_ASM


static void mix2to1_init(sample_t * dest, sample_t * src, sample_t bias);
static void (*mix2to1)(sample_t * dest, sample_t * src, sample_t bias)=mix2to1_init;
static void mix2to1_init(sample_t * dest, sample_t * src, sample_t bias)
{
#ifdef CAN_COMPILE_X86_ASM
#ifdef CAN_COMPILE_SSE
	if(a52_accel & MM_ACCEL_X86_SSE) mix2to1 = mix2to1_SSE;
	else
#endif
#ifdef CAN_COMPILE_3DNOW
	if(a52_accel & MM_ACCEL_X86_3DNOW) mix2to1 = mix2to1_3DNow;
	else
#endif
#endif //CAN_COMPILE_X86_ASM
	mix2to1 = mix2to1_c;
	(*mix2to1)(dest,src,bias);
}

static void mix3to1_init (sample_t * samples, sample_t bias);
static void (*mix3to1)(sample_t * samples, sample_t bias)=mix3to1_init;
static void mix3to1_init (sample_t * samples, sample_t bias)
{
#ifdef CAN_COMPILE_X86_ASM
#ifdef CAN_COMPILE_SSE
	if(a52_accel & MM_ACCEL_X86_SSE) mix3to1 = mix3to1_SSE;
	else
#endif
#ifdef CAN_COMPILE_3DNOW
	if(a52_accel & MM_ACCEL_X86_3DNOW) mix3to1 = mix3to1_3DNow;
	else
#endif
#endif //CAN_COMPILE_X86_ASM
	mix3to1 = mix3to1_c;
	(*mix3to1)(samples,bias);
}

static void mix4to1_init (sample_t * samples, sample_t bias);
static void (*mix4to1)(sample_t * samples, sample_t bias)=mix4to1_init;
static void mix4to1_init (sample_t * samples, sample_t bias)
{
#ifdef CAN_COMPILE_X86_ASM
#ifdef CAN_COMPILE_SSE
	if(a52_accel & MM_ACCEL_X86_SSE) mix4to1 = mix4to1_SSE;
	else
#endif
#ifdef CAN_COMPILE_3DNOW
	if(a52_accel & MM_ACCEL_X86_3DNOW) mix4to1 = mix4to1_3DNow;
	else
#endif
#endif //CAN_COMPILE_X86_ASM
	mix4to1 = mix4to1_c;
	(*mix4to1)(samples,bias);
}

static void mix5to1_init (sample_t * samples, sample_t bias);
static void (*mix5to1)(sample_t * samples, sample_t bias)=mix5to1_init;
static void mix5to1_init (sample_t * samples, sample_t bias)
{
#ifdef CAN_COMPILE_X86_ASM
#ifdef CAN_COMPILE_SSE
	if(a52_accel & MM_ACCEL_X86_SSE) mix5to1 = mix5to1_SSE;
	else
#endif
#ifdef CAN_COMPILE_3DNOW
	if(a52_accel & MM_ACCEL_X86_3DNOW) mix5to1 = mix5to1_3DNow;
	else
#endif
#endif //CAN_COMPILE_X86_ASM
	mix5to1 = mix5to1_c;
	(*mix5to1)(samples,bias);
}

static void mix3to2_init (sample_t * samples, sample_t bias);
static void (*mix3to2)(sample_t * samples, sample_t bias)=mix3to2_init;
static void mix3to2_init (sample_t * samples, sample_t bias)
{
#ifdef CAN_COMPILE_X86_ASM
#ifdef CAN_COMPILE_SSE
	if(a52_accel & MM_ACCEL_X86_SSE) mix3to2 = mix3to2_SSE;
	else
#endif
#ifdef CAN_COMPILE_3DNOW
	if(a52_accel & MM_ACCEL_X86_3DNOW) mix3to2 = mix3to2_3DNow;
	else
#endif
#endif //CAN_COMPILE_X86_ASM
	mix3to2 = mix3to2_c;
	(*mix3to2)(samples,bias);
}

static void mix21to2_init (sample_t * left, sample_t * right, sample_t bias);
static void (*mix21to2)(sample_t * left, sample_t * right, sample_t bias)=mix21to2_init;
static void mix21to2_init (sample_t * left, sample_t * right, sample_t bias)
{
#ifdef CAN_COMPILE_X86_ASM
#ifdef CAN_COMPILE_SSE
	if(a52_accel & MM_ACCEL_X86_SSE) mix21to2 = mix21to2_SSE;
	else
#endif
#ifdef CAN_COMPILE_3DNOW
	if(a52_accel & MM_ACCEL_X86_3DNOW) mix21to2 = mix21to2_3DNow;
	else
#endif
#endif //CAN_COMPILE_X86_ASM
	mix21to2 = mix21to2_c;
	(*mix21to2)(left,right,bias);
}

static void mix21toS_init (sample_t * samples, sample_t bias);
static void (*mix21toS)(sample_t * samples, sample_t bias)=mix21toS_init;
static void mix21toS_init (sample_t * samples, sample_t bias)
{
#ifdef CAN_COMPILE_X86_ASM
#ifdef CAN_COMPILE_SSE
	if(a52_accel & MM_ACCEL_X86_SSE) mix21toS = mix21toS_SSE;
	else
#endif
#ifdef CAN_COMPILE_3DNOW
	if(a52_accel & MM_ACCEL_X86_3DNOW) mix21toS = mix21toS_3DNow;
	else
#endif
#endif //CAN_COMPILE_X86_ASM
	mix21toS = mix21toS_c;
	(*mix21toS)(samples,bias);
}

static void mix31to2_init (sample_t * samples, sample_t bias);
static void (*mix31to2)(sample_t * samples, sample_t bias)=mix31to2_init;
static void mix31to2_init (sample_t * samples, sample_t bias)
{
#ifdef CAN_COMPILE_X86_ASM
#ifdef CAN_COMPILE_SSE
	if(a52_accel & MM_ACCEL_X86_SSE) mix31to2 = mix31to2_SSE;
	else
#endif
#ifdef CAN_COMPILE_3DNOW
	if(a52_accel & MM_ACCEL_X86_3DNOW) mix31to2 = mix31to2_3DNow;
	else
#endif
#endif //CAN_COMPILE_X86_ASM
	mix31to2 = mix31to2_c;
	(*mix31to2)(samples,bias);
}

static void mix31toS_init (sample_t * samples, sample_t bias);
static void (*mix31toS)(sample_t * samples, sample_t bias)=mix31toS_init;
static void mix31toS_init (sample_t * samples, sample_t bias)
{
#ifdef CAN_COMPILE_X86_ASM
#ifdef CAN_COMPILE_SSE
	if(a52_accel & MM_ACCEL_X86_SSE) mix31toS = mix31toS_SSE;
	else
#endif
#ifdef CAN_COMPILE_3DNOW
	if(a52_accel & MM_ACCEL_X86_3DNOW) mix31toS = mix31toS_3DNow;
	else
#endif
#endif //CAN_COMPILE_X86_ASM
	mix31toS = mix31toS_c;
	(*mix31toS)(samples,bias);
}

static void mix22toS_init (sample_t * samples, sample_t bias);
static void (*mix22toS)(sample_t * samples, sample_t bias)=mix22toS_init;
static void mix22toS_init (sample_t * samples, sample_t bias)
{
#ifdef CAN_COMPILE_X86_ASM
#ifdef CAN_COMPILE_SSE
	if(a52_accel & MM_ACCEL_X86_SSE) mix22toS = mix22toS_SSE;
	else
#endif
#ifdef CAN_COMPILE_3DNOW
	if(a52_accel & MM_ACCEL_X86_3DNOW) mix22toS = mix22toS_3DNow;
	else
#endif
#endif //CAN_COMPILE_X86_ASM
	mix22toS = mix22toS_c;
	(*mix22toS)(samples,bias);
}

static void mix32to2_init (sample_t * samples, sample_t bias);
static void (*mix32to2)(sample_t * samples, sample_t bias)=mix32to2_init;
static void mix32to2_init (sample_t * samples, sample_t bias)
{
#ifdef CAN_COMPILE_X86_ASM
#ifdef CAN_COMPILE_SSE
	if(a52_accel & MM_ACCEL_X86_SSE) mix32to2 = mix32to2_SSE;
	else
#endif
#ifdef CAN_COMPILE_3DNOW
	if(a52_accel & MM_ACCEL_X86_3DNOW) mix32to2 = mix32to2_3DNow;
	else
#endif
#endif //CAN_COMPILE_X86_ASM
	mix32to2 = mix32to2_c;
	(*mix32to2)(samples,bias);
}

static void mix32toS_init (sample_t * samples, sample_t bias);
static void (*mix32toS)(sample_t * samples, sample_t bias)=mix32toS_init;
static void mix32toS_init (sample_t * samples, sample_t bias)
{
#ifdef CAN_COMPILE_X86_ASM
#ifdef CAN_COMPILE_SSE
	if(a52_accel & MM_ACCEL_X86_SSE) mix32toS = mix32toS_SSE;
	else
#endif
#ifdef CAN_COMPILE_3DNOW
	if(a52_accel & MM_ACCEL_X86_3DNOW) mix32toS = mix32toS_3DNow;
	else
#endif
#endif //CAN_COMPILE_X86_ASM
	mix32toS = mix32toS_c;
	(*mix32toS)(samples,bias);
}

static void move2to1_init (sample_t * src, sample_t * dest, sample_t bias);
static void (*move2to1)(sample_t * src, sample_t * dest, sample_t bias)=move2to1_init;
static void move2to1_init (sample_t * src, sample_t * dest, sample_t bias)
{
#ifdef CAN_COMPILE_X86_ASM
#ifdef CAN_COMPILE_SSE
	if(a52_accel & MM_ACCEL_X86_SSE) move2to1 = move2to1_SSE;
	else
#endif
#ifdef CAN_COMPILE_3DNOW
	if(a52_accel & MM_ACCEL_X86_3DNOW) move2to1 = move2to1_3DNow;
	else
#endif
#endif //CAN_COMPILE_X86_ASM
	move2to1 = move2to1_c;
	(*move2to1)(src,dest,bias);
}

static void zero (sample_t * samples)
{
    int i;

    for (i = 0; i < 256; i++)
	samples[i] = 0;
}

void a52_downmix (sample_t * samples, int acmod, int output, sample_t bias,
		  sample_t clev, sample_t slev)
{
    switch (CONVERT (acmod, output & A52_CHANNEL_MASK)) {

    case CONVERT (A52_CHANNEL, A52_CHANNEL2):
	memcpy (samples, samples + 256, 256 * sizeof (sample_t));
	break;

    case CONVERT (A52_CHANNEL, A52_MONO):
    case CONVERT (A52_STEREO, A52_MONO):
    mix_2to1:
	mix2to1 (samples, samples + 256, bias);
	break;

    case CONVERT (A52_2F1R, A52_MONO):
	if (slev == 0)
	    goto mix_2to1;
    case CONVERT (A52_3F, A52_MONO):
    mix_3to1:
	mix3to1 (samples, bias);
	break;

    case CONVERT (A52_3F1R, A52_MONO):
	if (slev == 0)
	    goto mix_3to1;
    case CONVERT (A52_2F2R, A52_MONO):
	if (slev == 0)
	    goto mix_2to1;
	mix4to1 (samples, bias);
	break;

    case CONVERT (A52_3F2R, A52_MONO):
	if (slev == 0)
	    goto mix_3to1;
	mix5to1 (samples, bias);
	break;

    case CONVERT (A52_MONO, A52_DOLBY):
	memcpy (samples + 256, samples, 256 * sizeof (sample_t));
	break;

    case CONVERT (A52_3F, A52_STEREO):
    case CONVERT (A52_3F, A52_DOLBY):
    mix_3to2:
	mix3to2 (samples, bias);
	break;

    case CONVERT (A52_2F1R, A52_STEREO):
	if (slev == 0)
	    break;
	mix21to2 (samples, samples + 256, bias);
	break;

    case CONVERT (A52_2F1R, A52_DOLBY):
	mix21toS (samples, bias);
	break;

    case CONVERT (A52_3F1R, A52_STEREO):
	if (slev == 0)
	    goto mix_3to2;
	mix31to2 (samples, bias);
	break;

    case CONVERT (A52_3F1R, A52_DOLBY):
	mix31toS (samples, bias);
	break;

    case CONVERT (A52_2F2R, A52_STEREO):
	if (slev == 0)
	    break;
	mix2to1 (samples, samples + 512, bias);
	mix2to1 (samples + 256, samples + 768, bias);
	break;

    case CONVERT (A52_2F2R, A52_DOLBY):
	mix22toS (samples, bias);
	break;

    case CONVERT (A52_3F2R, A52_STEREO):
	if (slev == 0)
	    goto mix_3to2;
	mix32to2 (samples, bias);
	break;

    case CONVERT (A52_3F2R, A52_DOLBY):
	mix32toS (samples, bias);
	break;

    case CONVERT (A52_3F1R, A52_3F):
	if (slev == 0)
	    break;
	mix21to2 (samples, samples + 512, bias);
	break;

    case CONVERT (A52_3F2R, A52_3F):
	if (slev == 0)
	    break;
	mix2to1 (samples, samples + 768, bias);
	mix2to1 (samples + 512, samples + 1024, bias);
	break;

    case CONVERT (A52_3F1R, A52_2F1R):
	mix3to2 (samples, bias);
	memcpy (samples + 512, samples + 768, 256 * sizeof (sample_t));
	break;

    case CONVERT (A52_2F2R, A52_2F1R):
	mix2to1 (samples + 512, samples + 768, bias);
	break;

    case CONVERT (A52_3F2R, A52_2F1R):
	mix3to2 (samples, bias);
	move2to1 (samples + 768, samples + 512, bias);
	break;

    case CONVERT (A52_3F2R, A52_3F1R):
	mix2to1 (samples + 768, samples + 1024, bias);
	break;

    case CONVERT (A52_2F1R, A52_2F2R):
	memcpy (samples + 768, samples + 512, 256 * sizeof (sample_t));
	break;

    case CONVERT (A52_3F1R, A52_2F2R):
	mix3to2 (samples, bias);
	memcpy (samples + 512, samples + 768, 256 * sizeof (sample_t));
	break;

    case CONVERT (A52_3F2R, A52_2F2R):
	mix3to2 (samples, bias);
	memcpy (samples + 512, samples + 768, 256 * sizeof (sample_t));
	memcpy (samples + 768, samples + 1024, 256 * sizeof (sample_t));
	break;

    case CONVERT (A52_3F1R, A52_3F2R):
	memcpy (samples + 1024, samples + 768, 256 * sizeof (sample_t));
	break;
    }
}

void a52_upmix (sample_t * samples, int acmod, int output)
{
    switch (CONVERT (acmod, output & A52_CHANNEL_MASK)) {

    case CONVERT (A52_CHANNEL, A52_CHANNEL2):
	memcpy (samples + 256, samples, 256 * sizeof (sample_t));
	break;

    case CONVERT (A52_3F2R, A52_MONO):
	zero (samples + 1024);
    case CONVERT (A52_3F1R, A52_MONO):
    case CONVERT (A52_2F2R, A52_MONO):
	zero (samples + 768);
    case CONVERT (A52_3F, A52_MONO):
    case CONVERT (A52_2F1R, A52_MONO):
	zero (samples + 512);
    case CONVERT (A52_CHANNEL, A52_MONO):
    case CONVERT (A52_STEREO, A52_MONO):
	zero (samples + 256);
	break;

    case CONVERT (A52_3F2R, A52_STEREO):
    case CONVERT (A52_3F2R, A52_DOLBY):
	zero (samples + 1024);
    case CONVERT (A52_3F1R, A52_STEREO):
    case CONVERT (A52_3F1R, A52_DOLBY):
	zero (samples + 768);
    case CONVERT (A52_3F, A52_STEREO):
    case CONVERT (A52_3F, A52_DOLBY):
    mix_3to2:
	memcpy (samples + 512, samples + 256, 256 * sizeof (sample_t));
	zero (samples + 256);
	break;

    case CONVERT (A52_2F2R, A52_STEREO):
    case CONVERT (A52_2F2R, A52_DOLBY):
	zero (samples + 768);
    case CONVERT (A52_2F1R, A52_STEREO):
    case CONVERT (A52_2F1R, A52_DOLBY):
	zero (samples + 512);
	break;

    case CONVERT (A52_3F2R, A52_3F):
	zero (samples + 1024);
    case CONVERT (A52_3F1R, A52_3F):
    case CONVERT (A52_2F2R, A52_2F1R):
	zero (samples + 768);
	break;

    case CONVERT (A52_3F2R, A52_3F1R):
	zero (samples + 1024);
	break;

    case CONVERT (A52_3F2R, A52_2F1R):
	zero (samples + 1024);
    case CONVERT (A52_3F1R, A52_2F1R):
    mix_31to21:
	memcpy (samples + 768, samples + 512, 256 * sizeof (sample_t));
	goto mix_3to2;

    case CONVERT (A52_3F2R, A52_2F2R):
	memcpy (samples + 1024, samples + 768, 256 * sizeof (sample_t));
	goto mix_31to21;
    }
}
