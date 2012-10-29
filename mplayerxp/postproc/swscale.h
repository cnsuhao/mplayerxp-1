/*
    Copyright (C) 2001-2003 Michael Niedermayer <michaelni@gmx.at>

    This program is mp_free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

    the C code (not assembly, mmx, ...) of the swscaler which has been written
    by Michael Niedermayer can be used under the LGPL license too
*/

#ifndef SWSCALE_H
#define SWSCALE_H

#include "libswscale/swscale.h"

extern int sws_init(void);
extern void sws_uninit(void);

#define MODE_RGB  0x1
#define MODE_BGR  0x2
extern void palette8torgb32(const uint8_t *src, uint8_t *dst, long num_pixels, const uint8_t *palette);
extern void palette8tobgr32(const uint8_t *src, uint8_t *dst, long num_pixels, const uint8_t *palette);
extern void palette8torgb24(const uint8_t *src, uint8_t *dst, long num_pixels, const uint8_t *palette);
extern void palette8tobgr24(const uint8_t *src, uint8_t *dst, long num_pixels, const uint8_t *palette);
extern void palette8torgb16(const uint8_t *src, uint8_t *dst, long num_pixels, const uint8_t *palette);
extern void palette8tobgr16(const uint8_t *src, uint8_t *dst, long num_pixels, const uint8_t *palette);
extern void palette8torgb15(const uint8_t *src, uint8_t *dst, long num_pixels, const uint8_t *palette);
extern void palette8tobgr15(const uint8_t *src, uint8_t *dst, long num_pixels, const uint8_t *palette);

#endif
