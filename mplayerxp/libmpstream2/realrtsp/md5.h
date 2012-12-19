/*
 * copyright (c) 2006 Michael Niedermayer <michaelni@gmx.at>
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is mp_free software; you can redistribute it and/or
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

#ifndef FFMPEG_MD5_H
#define FFMPEG_MD5_H

#include <stdint.h>

# ifdef WORDS_BIGENDIAN
#  define AV_RB32(x)    AV_RN32(x)
#  define AV_WB32(p, d) AV_WN32(p, d)

#  define AV_RL32(x)    bswap_32(AV_RN32(x))
#  define AV_WL32(p, d) AV_WN32(p, bswap_32(d))
# else /* WORDS_BIGENDIAN */
#  define AV_RB32(x)    bswap_32(AV_RN32(x))
#  define AV_WB32(p, d) AV_WN32(p, bswap_32(d))

#  define AV_RL32(x)    AV_RN32(x)
#  define AV_WL32(p, d) AV_WN32(p, d)
# endif

#define AV_RN16(a) (*((uint16_t*)(a)))
#define AV_RN32(a) (*((uint32_t*)(a)))
#define AV_RN64(a) (*((uint64_t*)(a)))

#define AV_WN16(a, b) *((uint16_t*)(a)) = (b)
#define AV_WN32(a, b) *((uint32_t*)(a)) = (b)
#define AV_WN64(a, b) *((uint64_t*)(a)) = (b)

# ifdef WORDS_BIGENDIAN
#  define AV_RB16(x)    AV_RN16(x)
#  define AV_WB16(p, d) AV_WN16(p, d)

#  define AV_RL16(x)    bswap_16(AV_RN16(x))
#  define AV_WL16(p, d) AV_WN16(p, bswap_16(d))
# else /* WORDS_BIGENDIAN */
#  define AV_RB16(x)    bswap_16(AV_RN16(x))
#  define AV_WB16(p, d) AV_WN16(p, bswap_16(d))

#  define AV_RL16(x)    AV_RN16(x)
#  define AV_WL16(p, d) AV_WN16(p, d)
# endif

#include "mpxp_conf_lavc.h"

#endif /* FFMPEG_MD5_H */

