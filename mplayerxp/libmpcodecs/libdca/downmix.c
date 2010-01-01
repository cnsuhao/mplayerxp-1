/*
 * downmix.c
 * Copyright (C) 2004 Gildas Bazin <gbazin@videolan.org>
 * Copyright (C) 2000-2003 Michel Lespinasse <walken@zoy.org>
 * Copyright (C) 1999-2000 Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *
 * This file is part of dtsdec, a free DTS Coherent Acoustics stream decoder.
 * See http://www.videolan.org/dtsdec.html for updates.
 *
 * dtsdec is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * dtsdec is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "../../mp_config.h"

#include <string.h>
#include <inttypes.h>

#include "../../cpudetect.h"
#include "dca.h"
#include "dca_internal.h"

#define CONVERT(acmod,output) (((output) << DCA_CHANNEL_BITS) + (acmod))

int dca_downmix_init (int input, int flags, level_t * level,
		      level_t clev, level_t slev)
{
    static uint8_t table[11][10] = {
        /* DCA_MONO */
        {DCA_MONO,      DCA_MONO,       DCA_MONO,       DCA_MONO,
         DCA_MONO,      DCA_MONO,       DCA_MONO,       DCA_MONO,
         DCA_MONO,      DCA_MONO},
        /* DCA_CHANNEL */
        {DCA_MONO,      DCA_CHANNEL,    DCA_STEREO,     DCA_STEREO,
         DCA_STEREO,    DCA_STEREO,     DCA_STEREO,     DCA_STEREO,
         DCA_STEREO,    DCA_STEREO},
        /* DCA_STEREO */
        {DCA_MONO,      DCA_CHANNEL,    DCA_STEREO,     DCA_STEREO,
         DCA_STEREO,    DCA_STEREO,     DCA_STEREO,     DCA_STEREO,
         DCA_STEREO,    DCA_STEREO},
        /* DCA_STEREO_SUMDIFF */
        {DCA_MONO,      DCA_CHANNEL,    DCA_STEREO,     DCA_STEREO,
         DCA_STEREO,    DCA_STEREO,     DCA_STEREO,     DCA_STEREO,
         DCA_STEREO,    DCA_STEREO},
        /* DCA_STEREO_TOTAL */
        {DCA_MONO,      DCA_CHANNEL,    DCA_STEREO,     DCA_STEREO,
         DCA_STEREO,    DCA_STEREO,     DCA_STEREO,     DCA_STEREO,
         DCA_STEREO,    DCA_STEREO},
        /* DCA_3F */
        {DCA_MONO,      DCA_CHANNEL,    DCA_STEREO,     DCA_STEREO,
         DCA_STEREO,    DCA_3F,         DCA_3F,         DCA_3F,
         DCA_3F,        DCA_3F},
        /* DCA_2F1R */
        {DCA_MONO,      DCA_CHANNEL,    DCA_STEREO,     DCA_STEREO,
         DCA_STEREO,    DCA_2F1R,       DCA_2F1R,       DCA_2F1R,
         DCA_2F1R,      DCA_2F1R},
        /* DCA_3F1R */
        {DCA_MONO,      DCA_CHANNEL,    DCA_STEREO,     DCA_STEREO,
         DCA_STEREO,    DCA_3F,         DCA_3F1R,       DCA_3F1R,
         DCA_3F1R,      DCA_3F1R},
        /* DCA_2F2R */
        {DCA_MONO,      DCA_CHANNEL,    DCA_STEREO,     DCA_STEREO,
         DCA_STEREO,    DCA_STEREO,     DCA_2F2R,       DCA_2F2R,
         DCA_2F2R,      DCA_2F2R},
        /* DCA_3F2R */
        {DCA_MONO,      DCA_CHANNEL,    DCA_STEREO,     DCA_STEREO,
         DCA_STEREO,    DCA_3F,         DCA_3F2R,       DCA_3F2R,
         DCA_3F2R,      DCA_3F2R},
        /* DCA_4F2R */
        {DCA_MONO,      DCA_CHANNEL,    DCA_STEREO,     DCA_STEREO,
         DCA_STEREO,    DCA_4F2R,       DCA_4F2R,       DCA_4F2R,
         DCA_4F2R,      DCA_4F2R},
    };
    int output;

    output = flags & DCA_CHANNEL_MASK;

    if (output > DCA_CHANNEL_MAX)
	return -1;

    output = table[output][input];

    if (output == DCA_STEREO &&
	(input == DCA_DOLBY || (input == DCA_3F && clev == LEVEL (LEVEL_3DB))))
	output = DCA_DOLBY;

    if (flags & DCA_ADJUST_LEVEL) {
	level_t adjust;

	switch (CONVERT (input & 7, output)) {

	case CONVERT (DCA_3F, DCA_MONO):
	    adjust = DIV (LEVEL_3DB, LEVEL (1) + clev);
	    break;

	case CONVERT (DCA_STEREO, DCA_MONO):
	case CONVERT (DCA_2F2R, DCA_2F1R):
	case CONVERT (DCA_3F2R, DCA_3F1R):
	level_3db:
	    adjust = LEVEL (LEVEL_3DB);
	    break;

	case CONVERT (DCA_3F2R, DCA_2F1R):
	    if (clev < LEVEL (LEVEL_PLUS3DB - 1))
		goto level_3db;
	    /* break thru */
	case CONVERT (DCA_3F, DCA_STEREO):
	case CONVERT (DCA_3F1R, DCA_2F1R):
	case CONVERT (DCA_3F1R, DCA_2F2R):
	case CONVERT (DCA_3F2R, DCA_2F2R):
	    adjust = DIV (1, LEVEL (1) + clev);
	    break;

	case CONVERT (DCA_2F1R, DCA_MONO):
	    adjust = DIV (LEVEL_PLUS3DB, LEVEL (2) + slev);
	    break;

	case CONVERT (DCA_2F1R, DCA_STEREO):
	case CONVERT (DCA_3F1R, DCA_3F):
	    adjust = DIV (1, LEVEL (1) + MUL_C (slev, LEVEL_3DB));
	    break;

	case CONVERT (DCA_3F1R, DCA_MONO):
	    adjust = DIV (LEVEL_3DB, LEVEL (1) + clev + MUL_C (slev, 0.5));
	    break;

	case CONVERT (DCA_3F1R, DCA_STEREO):
	    adjust = DIV (1, LEVEL (1) + clev + MUL_C (slev, LEVEL_3DB));
	    break;

	case CONVERT (DCA_2F2R, DCA_MONO):
	    adjust = DIV (LEVEL_3DB, LEVEL (1) + slev);
	    break;

	case CONVERT (DCA_2F2R, DCA_STEREO):
	case CONVERT (DCA_3F2R, DCA_3F):
	    adjust = DIV (1, LEVEL (1) + slev);
	    break;

	case CONVERT (DCA_3F2R, DCA_MONO):
	    adjust = DIV (LEVEL_3DB, LEVEL (1) + clev + slev);
	    break;

	case CONVERT (DCA_3F2R, DCA_STEREO):
	    adjust = DIV (1, LEVEL (1) + clev + slev);
	    break;

	case CONVERT (DCA_MONO, DCA_DOLBY):
	    adjust = LEVEL (LEVEL_PLUS3DB);
	    break;

	case CONVERT (DCA_3F, DCA_DOLBY):
	case CONVERT (DCA_2F1R, DCA_DOLBY):
	    adjust = LEVEL (1 / (1 + LEVEL_3DB));
	    break;

	case CONVERT (DCA_3F1R, DCA_DOLBY):
	case CONVERT (DCA_2F2R, DCA_DOLBY):
	    adjust = LEVEL (1 / (1 + 2 * LEVEL_3DB));
	    break;

	case CONVERT (DCA_3F2R, DCA_DOLBY):
	    adjust = LEVEL (1 / (1 + 3 * LEVEL_3DB));
	    break;

	default:
	    return output;
	}

	*level = MUL_L (*level, adjust);
    }

    return output;
}

int dca_downmix_coeff (level_t * coeff, int acmod, int output, level_t level,
		       level_t clev, level_t slev)
{
    level_t level_3db;

    level_3db = MUL_C (level, LEVEL_3DB);

    switch (CONVERT (acmod, output & DCA_CHANNEL_MASK)) {

    case CONVERT (DCA_CHANNEL, DCA_CHANNEL):
    case CONVERT (DCA_MONO, DCA_MONO):
    case CONVERT (DCA_STEREO, DCA_STEREO):
    case CONVERT (DCA_3F, DCA_3F):
    case CONVERT (DCA_2F1R, DCA_2F1R):
    case CONVERT (DCA_3F1R, DCA_3F1R):
    case CONVERT (DCA_2F2R, DCA_2F2R):
    case CONVERT (DCA_3F2R, DCA_3F2R):
    case CONVERT (DCA_STEREO, DCA_DOLBY):
	coeff[0] = coeff[1] = coeff[2] = coeff[3] = coeff[4] = level;
	return 0;

    case CONVERT (DCA_CHANNEL, DCA_MONO):
	coeff[0] = coeff[1] = MUL_C (level, LEVEL_6DB);
	return 3;

    case CONVERT (DCA_STEREO, DCA_MONO):
	coeff[0] = coeff[1] = level_3db;
	return 3;

    case CONVERT (DCA_3F, DCA_MONO):
	coeff[0] = coeff[2] = level_3db;
	coeff[1] = MUL_C (MUL_L (level_3db, clev), LEVEL_PLUS6DB);
	return 7;

    case CONVERT (DCA_2F1R, DCA_MONO):
	coeff[0] = coeff[1] = level_3db;
	coeff[2] = MUL_L (level_3db, slev);
	return 7;

    case CONVERT (DCA_2F2R, DCA_MONO):
	coeff[0] = coeff[1] = level_3db;
	coeff[2] = coeff[3] = MUL_L (level_3db, slev);
	return 15;

    case CONVERT (DCA_3F1R, DCA_MONO):
	coeff[0] = coeff[2] = level_3db;
	coeff[1] = MUL_C (MUL_L (level_3db, clev), LEVEL_PLUS6DB);
	coeff[3] = MUL_L (level_3db, slev);
	return 15;

    case CONVERT (DCA_3F2R, DCA_MONO):
	coeff[0] = coeff[2] = level_3db;
	coeff[1] = MUL_C (MUL_L (level_3db, clev), LEVEL_PLUS6DB);
	coeff[3] = coeff[4] = MUL_L (level_3db, slev);
	return 31;

    case CONVERT (DCA_MONO, DCA_DOLBY):
	coeff[0] = level_3db;
	return 0;

    case CONVERT (DCA_3F, DCA_DOLBY):
	coeff[0] = coeff[2] = coeff[3] = coeff[4] = level;
	coeff[1] = level_3db;
	return 7;

    case CONVERT (DCA_3F, DCA_STEREO):
    case CONVERT (DCA_3F1R, DCA_2F1R):
    case CONVERT (DCA_3F2R, DCA_2F2R):
	coeff[0] = coeff[2] = coeff[3] = coeff[4] = level;
	coeff[1] = MUL_L (level, clev);
	return 7;

    case CONVERT (DCA_2F1R, DCA_DOLBY):
	coeff[0] = coeff[1] = level;
	coeff[2] = level_3db;
	return 7;

    case CONVERT (DCA_2F1R, DCA_STEREO):
	coeff[0] = coeff[1] = level;
	coeff[2] = MUL_L (level_3db, slev);
	return 7;

    case CONVERT (DCA_3F1R, DCA_DOLBY):
	coeff[0] = coeff[2] = level;
	coeff[1] = coeff[3] = level_3db;
	return 15;

    case CONVERT (DCA_3F1R, DCA_STEREO):
	coeff[0] = coeff[2] = level;
	coeff[1] = MUL_L (level, clev);
	coeff[3] = MUL_L (level_3db, slev);
	return 15;

    case CONVERT (DCA_2F2R, DCA_DOLBY):
	coeff[0] = coeff[1] = level;
	coeff[2] = coeff[3] = level_3db;
	return 15;

    case CONVERT (DCA_2F2R, DCA_STEREO):
	coeff[0] = coeff[1] = level;
	coeff[2] = coeff[3] = MUL_L (level, slev);
	return 15;

    case CONVERT (DCA_3F2R, DCA_DOLBY):
	coeff[0] = coeff[2] = level;
	coeff[1] = coeff[3] = coeff[4] = level_3db;
	return 31;

    case CONVERT (DCA_3F2R, DCA_2F1R):
	coeff[0] = coeff[2] = level;
	coeff[1] = MUL_L (level, clev);
	coeff[3] = coeff[4] = level_3db;
	return 31;

    case CONVERT (DCA_3F2R, DCA_STEREO):
	coeff[0] = coeff[2] = level;
	coeff[1] = MUL_L (level, clev);
	coeff[3] = coeff[4] = MUL_L (level, slev);
	return 31;

    case CONVERT (DCA_3F1R, DCA_3F):
	coeff[0] = coeff[1] = coeff[2] = level;
	coeff[3] = MUL_L (level_3db, slev);
	return 13;

    case CONVERT (DCA_3F2R, DCA_3F):
	coeff[0] = coeff[1] = coeff[2] = level;
	coeff[3] = coeff[4] = MUL_L (level, slev);
	return 29;

    case CONVERT (DCA_2F2R, DCA_2F1R):
	coeff[0] = coeff[1] = level;
	coeff[2] = coeff[3] = level_3db;
	return 12;

    case CONVERT (DCA_3F2R, DCA_3F1R):
	coeff[0] = coeff[1] = coeff[2] = level;
	coeff[3] = coeff[4] = level_3db;
	return 24;

    case CONVERT (DCA_2F1R, DCA_2F2R):
	coeff[0] = coeff[1] = level;
	coeff[2] = level_3db;
	return 0;

    case CONVERT (DCA_3F1R, DCA_2F2R):
	coeff[0] = coeff[2] = level;
	coeff[1] = MUL_L (level, clev);
	coeff[3] = level_3db;
	return 7;

    case CONVERT (DCA_3F1R, DCA_3F2R):
	coeff[0] = coeff[1] = coeff[2] = level;
	coeff[3] = level_3db;
	return 0;
    }

    return -1;	/* NOTREACHED */
}

#include "../../mm_accel.h"

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

static void (*mix2to1)(sample_t * dest, sample_t * src, sample_t bias);
static void mix2to1_init(sample_t * dest, sample_t * src, sample_t bias)
{
#ifdef CAN_COMPILE_X86_ASM
#ifdef CAN_COMPILE_SSE
	if(dca_accel & MM_ACCEL_X86_SSE) mix2to1 = mix2to1_SSE;
	else
#endif
#ifdef CAN_COMPILE_3DNOW
	if(dca_accel & MM_ACCEL_X86_3DNOW) mix2to1 = mix2to1_3DNow;
	else
#endif
#endif //CAN_COMPILE_X86_ASM
	mix2to1 = mix2to1_c;
	(*mix2to1)(dest,src,bias);
}
static void (*mix2to1)(sample_t * dest, sample_t * src, sample_t bias)=mix2to1_init;

static void (*mix3to1)(sample_t * samples, sample_t bias);
static void mix3to1_init (sample_t * samples, sample_t bias)
{
#ifdef CAN_COMPILE_X86_ASM
#ifdef CAN_COMPILE_SSE
	if(dca_accel & MM_ACCEL_X86_SSE) mix3to1 = mix3to1_SSE;
	else
#endif
#ifdef CAN_COMPILE_3DNOW
	if(dca_accel & MM_ACCEL_X86_3DNOW) mix3to1 = mix3to1_3DNow;
	else
#endif
#endif //CAN_COMPILE_X86_ASM
	mix3to1 = mix3to1_c;
	(*mix3to1)(samples,bias);
}
static void (*mix3to1)(sample_t * samples, sample_t bias)=mix3to1_init;

static void (*mix4to1)(sample_t * samples, sample_t bias);
static void mix4to1_init (sample_t * samples, sample_t bias)
{
#ifdef CAN_COMPILE_X86_ASM
#ifdef CAN_COMPILE_SSE
	if(dca_accel & MM_ACCEL_X86_SSE) mix4to1 = mix4to1_SSE;
	else
#endif
#ifdef CAN_COMPILE_3DNOW
	if(dca_accel & MM_ACCEL_X86_3DNOW) mix4to1 = mix4to1_3DNow;
	else
#endif
#endif //CAN_COMPILE_X86_ASM
	mix4to1 = mix4to1_c;
	(*mix4to1)(samples,bias);
}
static void (*mix4to1)(sample_t * samples, sample_t bias)=mix4to1_init;

static void (*mix5to1)(sample_t * samples, sample_t bias);
static void mix5to1_init (sample_t * samples, sample_t bias)
{
#ifdef CAN_COMPILE_X86_ASM
#ifdef CAN_COMPILE_SSE
	if(dca_accel & MM_ACCEL_X86_SSE) mix5to1 = mix5to1_SSE;
	else
#endif
#ifdef CAN_COMPILE_3DNOW
	if(dca_accel & MM_ACCEL_X86_3DNOW) mix5to1 = mix5to1_3DNow;
	else
#endif
#endif //CAN_COMPILE_X86_ASM
	mix5to1 = mix5to1_c;
	(*mix5to1)(samples,bias);
}
static void (*mix5to1)(sample_t * samples, sample_t bias)=mix5to1_init;

static void (*mix3to2)(sample_t * samples, sample_t bias);
static void mix3to2_init (sample_t * samples, sample_t bias)
{
#ifdef CAN_COMPILE_X86_ASM
#ifdef CAN_COMPILE_SSE
	if(dca_accel & MM_ACCEL_X86_SSE) mix3to2 = mix3to2_SSE;
	else
#endif
#ifdef CAN_COMPILE_3DNOW
	if(dca_accel & MM_ACCEL_X86_3DNOW) mix3to2 = mix3to2_3DNow;
	else
#endif
#endif //CAN_COMPILE_X86_ASM
	mix3to2 = mix3to2_c;
	(*mix3to2)(samples,bias);
}
static void (*mix3to2)(sample_t * samples, sample_t bias)=mix3to2_init;

static void (*mix21to2)(sample_t * left, sample_t * right, sample_t bias);
static void mix21to2_init (sample_t * left, sample_t * right, sample_t bias)
{
#ifdef CAN_COMPILE_X86_ASM
#ifdef CAN_COMPILE_SSE
	if(dca_accel & MM_ACCEL_X86_SSE) mix21to2 = mix21to2_SSE;
	else
#endif
#ifdef CAN_COMPILE_3DNOW
	if(dca_accel & MM_ACCEL_X86_3DNOW) mix21to2 = mix21to2_3DNow;
	else
#endif
#endif //CAN_COMPILE_X86_ASM
	mix21to2 = mix21to2_c;
	(*mix21to2)(left,right,bias);
}
static void (*mix21to2)(sample_t * left, sample_t * right, sample_t bias)=mix21to2_init;

static void (*mix21toS)(sample_t * samples, sample_t bias);
static void mix21toS_init (sample_t * samples, sample_t bias)
{
#ifdef CAN_COMPILE_X86_ASM
#ifdef CAN_COMPILE_SSE
	if(dca_accel & MM_ACCEL_X86_SSE) mix21toS = mix21toS_SSE;
	else
#endif
#ifdef CAN_COMPILE_3DNOW
	if(dca_accel & MM_ACCEL_X86_3DNOW) mix21toS = mix21toS_3DNow;
	else
#endif
#endif //CAN_COMPILE_X86_ASM
	mix21toS = mix21toS_c;
	(*mix21toS)(samples,bias);
}
static void (*mix21toS)(sample_t * samples, sample_t bias)=mix21toS_init;

static void (*mix31to2)(sample_t * samples, sample_t bias);
static void mix31to2_init (sample_t * samples, sample_t bias)
{
#ifdef CAN_COMPILE_X86_ASM
#ifdef CAN_COMPILE_SSE
	if(dca_accel & MM_ACCEL_X86_SSE) mix31to2 = mix31to2_SSE;
	else
#endif
#ifdef CAN_COMPILE_3DNOW
	if(dca_accel & MM_ACCEL_X86_3DNOW) mix31to2 = mix31to2_3DNow;
	else
#endif
#endif //CAN_COMPILE_X86_ASM
	mix31to2 = mix31to2_c;
	(*mix31to2)(samples,bias);
}
static void (*mix31to2)(sample_t * samples, sample_t bias)=mix31to2_init;

static void (*mix31toS)(sample_t * samples, sample_t bias);
static void mix31toS_init (sample_t * samples, sample_t bias)
{
#ifdef CAN_COMPILE_X86_ASM
#ifdef CAN_COMPILE_SSE
	if(dca_accel & MM_ACCEL_X86_SSE) mix31toS = mix31toS_SSE;
	else
#endif
#ifdef CAN_COMPILE_3DNOW
	if(dca_accel & MM_ACCEL_X86_3DNOW) mix31toS = mix31toS_3DNow;
	else
#endif
#endif //CAN_COMPILE_X86_ASM
	mix31toS = mix31toS_c;
	(*mix31toS)(samples,bias);
}
static void (*mix31toS)(sample_t * samples, sample_t bias)=mix31toS_init;


static void (*mix22toS)(sample_t * samples, sample_t bias);
static void mix22toS_init (sample_t * samples, sample_t bias)
{
#ifdef CAN_COMPILE_X86_ASM
#ifdef CAN_COMPILE_SSE
	if(dca_accel & MM_ACCEL_X86_SSE) mix22toS = mix22toS_SSE;
	else
#endif
#ifdef CAN_COMPILE_3DNOW
	if(dca_accel & MM_ACCEL_X86_3DNOW) mix22toS = mix22toS_3DNow;
	else
#endif
#endif //CAN_COMPILE_X86_ASM
	mix22toS = mix22toS_c;
	(*mix22toS)(samples,bias);
}
static void (*mix22toS)(sample_t * samples, sample_t bias)=mix22toS_init;

static void (*mix32to2)(sample_t * samples, sample_t bias);
static void mix32to2_init (sample_t * samples, sample_t bias)
{
#ifdef CAN_COMPILE_X86_ASM
#ifdef CAN_COMPILE_SSE
	if(dca_accel & MM_ACCEL_X86_SSE) mix32to2 = mix32to2_SSE;
	else
#endif
#ifdef CAN_COMPILE_3DNOW
	if(dca_accel & MM_ACCEL_X86_3DNOW) mix32to2 = mix32to2_3DNow;
	else
#endif
#endif //CAN_COMPILE_X86_ASM
	mix32to2 = mix32to2_c;
	(*mix32to2)(samples,bias);
}
static void (*mix32to2)(sample_t * samples, sample_t bias)=mix32to2_init;

static void (*mix32toS)(sample_t * samples, sample_t bias);
static void mix32toS_init (sample_t * samples, sample_t bias)
{
#ifdef CAN_COMPILE_X86_ASM
#ifdef CAN_COMPILE_SSE
	if(dca_accel & MM_ACCEL_X86_SSE) mix32toS = mix32toS_SSE;
	else
#endif
#ifdef CAN_COMPILE_3DNOW
	if(dca_accel & MM_ACCEL_X86_3DNOW) mix32toS = mix32toS_3DNow;
	else
#endif
#endif //CAN_COMPILE_X86_ASM
	mix32toS = mix32toS_c;
	(*mix32toS)(samples,bias);
}
static void (*mix32toS)(sample_t * samples, sample_t bias)=mix32toS_init;

static void (*move2to1)(sample_t * src, sample_t * dest, sample_t bias);
static void move2to1_init (sample_t * src, sample_t * dest, sample_t bias)
{
#ifdef CAN_COMPILE_X86_ASM
#ifdef CAN_COMPILE_SSE
	if(dca_accel & MM_ACCEL_X86_SSE) move2to1 = move2to1_SSE;
	else
#endif
#ifdef CAN_COMPILE_3DNOW
	if(dca_accel & MM_ACCEL_X86_3DNOW) move2to1 = move2to1_3DNow;
	else
#endif
#endif //CAN_COMPILE_X86_ASM
	move2to1 = move2to1_c;
	(*move2to1)(src,dest,bias);
}
static void (*move2to1)(sample_t * src, sample_t * dest, sample_t bias)=move2to1_init;

static void zero (sample_t * samples)
{
    int i;

    for (i = 0; i < 256; i++)
	samples[i] = 0;
}

void dca_downmix (sample_t * samples, int acmod, int output, sample_t bias,
		  level_t clev, level_t slev)
{
    switch (CONVERT (acmod, output & DCA_CHANNEL_MASK)) {

    case CONVERT (DCA_CHANNEL, DCA_MONO):
    case CONVERT (DCA_STEREO, DCA_MONO):
    mix_2to1:
	mix2to1 (samples, samples + 256, bias);
	break;

    case CONVERT (DCA_2F1R, DCA_MONO):
	if (slev == 0)
	    goto mix_2to1;
    case CONVERT (DCA_3F, DCA_MONO):
    mix_3to1:
	mix3to1 (samples, bias);
	break;

    case CONVERT (DCA_3F1R, DCA_MONO):
	if (slev == 0)
	    goto mix_3to1;
    case CONVERT (DCA_2F2R, DCA_MONO):
	if (slev == 0)
	    goto mix_2to1;
	mix4to1 (samples, bias);
	break;

    case CONVERT (DCA_3F2R, DCA_MONO):
	if (slev == 0)
	    goto mix_3to1;
	mix5to1 (samples, bias);
	break;

    case CONVERT (DCA_MONO, DCA_DOLBY):
	memcpy (samples + 256, samples, 256 * sizeof (sample_t));
	break;

    case CONVERT (DCA_3F, DCA_STEREO):
    case CONVERT (DCA_3F, DCA_DOLBY):
    mix_3to2:
	mix3to2 (samples, bias);
	break;

    case CONVERT (DCA_2F1R, DCA_STEREO):
	if (slev == 0)
	    break;
	mix21to2 (samples, samples + 256, bias);
	break;

    case CONVERT (DCA_2F1R, DCA_DOLBY):
	mix21toS (samples, bias);
	break;

    case CONVERT (DCA_3F1R, DCA_STEREO):
	if (slev == 0)
	    goto mix_3to2;
	mix31to2 (samples, bias);
	break;

    case CONVERT (DCA_3F1R, DCA_DOLBY):
	mix31toS (samples, bias);
	break;

    case CONVERT (DCA_2F2R, DCA_STEREO):
	if (slev == 0)
	    break;
	mix2to1 (samples, samples + 512, bias);
	mix2to1 (samples + 256, samples + 768, bias);
	break;

    case CONVERT (DCA_2F2R, DCA_DOLBY):
	mix22toS (samples, bias);
	break;

    case CONVERT (DCA_3F2R, DCA_STEREO):
	if (slev == 0)
	    goto mix_3to2;
	mix32to2 (samples, bias);
	break;

    case CONVERT (DCA_3F2R, DCA_DOLBY):
	mix32toS (samples, bias);
	break;

    case CONVERT (DCA_3F1R, DCA_3F):
	if (slev == 0)
	    break;
	mix21to2 (samples, samples + 512, bias);
	break;

    case CONVERT (DCA_3F2R, DCA_3F):
	if (slev == 0)
	    break;
	mix2to1 (samples, samples + 768, bias);
	mix2to1 (samples + 512, samples + 1024, bias);
	break;

    case CONVERT (DCA_3F1R, DCA_2F1R):
	mix3to2 (samples, bias);
	memcpy (samples + 512, samples + 768, 256 * sizeof (sample_t));
	break;

    case CONVERT (DCA_2F2R, DCA_2F1R):
	mix2to1 (samples + 512, samples + 768, bias);
	break;

    case CONVERT (DCA_3F2R, DCA_2F1R):
	mix3to2 (samples, bias);
	move2to1 (samples + 768, samples + 512, bias);
	break;

    case CONVERT (DCA_3F2R, DCA_3F1R):
	mix2to1 (samples + 768, samples + 1024, bias);
	break;

    case CONVERT (DCA_2F1R, DCA_2F2R):
	memcpy (samples + 768, samples + 512, 256 * sizeof (sample_t));
	break;

    case CONVERT (DCA_3F1R, DCA_2F2R):
	mix3to2 (samples, bias);
	memcpy (samples + 512, samples + 768, 256 * sizeof (sample_t));
	break;

    case CONVERT (DCA_3F2R, DCA_2F2R):
	mix3to2 (samples, bias);
	memcpy (samples + 512, samples + 768, 256 * sizeof (sample_t));
	memcpy (samples + 768, samples + 1024, 256 * sizeof (sample_t));
	break;

    case CONVERT (DCA_3F1R, DCA_3F2R):
	memcpy (samples + 1024, samples + 768, 256 * sizeof (sample_t));
	break;
    }
}

void dca_upmix (sample_t * samples, int acmod, int output)
{
    switch (CONVERT (acmod, output & DCA_CHANNEL_MASK)) {

    case CONVERT (DCA_3F2R, DCA_MONO):
	zero (samples + 1024);
    case CONVERT (DCA_3F1R, DCA_MONO):
    case CONVERT (DCA_2F2R, DCA_MONO):
	zero (samples + 768);
    case CONVERT (DCA_3F, DCA_MONO):
    case CONVERT (DCA_2F1R, DCA_MONO):
	zero (samples + 512);
    case CONVERT (DCA_CHANNEL, DCA_MONO):
    case CONVERT (DCA_STEREO, DCA_MONO):
	zero (samples + 256);
	break;

    case CONVERT (DCA_3F2R, DCA_STEREO):
    case CONVERT (DCA_3F2R, DCA_DOLBY):
	zero (samples + 1024);
    case CONVERT (DCA_3F1R, DCA_STEREO):
    case CONVERT (DCA_3F1R, DCA_DOLBY):
	zero (samples + 768);
    case CONVERT (DCA_3F, DCA_STEREO):
    case CONVERT (DCA_3F, DCA_DOLBY):
    mix_3to2:
	memcpy (samples + 512, samples + 256, 256 * sizeof (sample_t));
	zero (samples + 256);
	break;

    case CONVERT (DCA_2F2R, DCA_STEREO):
    case CONVERT (DCA_2F2R, DCA_DOLBY):
	zero (samples + 768);
    case CONVERT (DCA_2F1R, DCA_STEREO):
    case CONVERT (DCA_2F1R, DCA_DOLBY):
	zero (samples + 512);
	break;

    case CONVERT (DCA_3F2R, DCA_3F):
	zero (samples + 1024);
    case CONVERT (DCA_3F1R, DCA_3F):
    case CONVERT (DCA_2F2R, DCA_2F1R):
	zero (samples + 768);
	break;

    case CONVERT (DCA_3F2R, DCA_3F1R):
	zero (samples + 1024);
	break;

    case CONVERT (DCA_3F2R, DCA_2F1R):
	zero (samples + 1024);
    case CONVERT (DCA_3F1R, DCA_2F1R):
    mix_31to21:
	memcpy (samples + 768, samples + 512, 256 * sizeof (sample_t));
	goto mix_3to2;

    case CONVERT (DCA_3F2R, DCA_2F2R):
	memcpy (samples + 1024, samples + 768, 256 * sizeof (sample_t));
	goto mix_31to21;
    }
}
