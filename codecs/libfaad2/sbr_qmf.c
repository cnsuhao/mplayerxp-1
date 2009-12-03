/*
** FAAD2 - Freeware Advanced Audio (AAC) Decoder including SBR decoding
** Copyright (C) 2003-2004 M. Bakker, Ahead Software AG, http://www.nero.com
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
** $Id: sbr_qmf.c,v 1.5 2007/12/14 08:51:08 nickols_k Exp $
** detailed CVS changelog at http://www.mplayerhq.hu/cgi-bin/cvsweb.cgi/main/
**/

#include "common.h"
#include "structs.h"

#ifdef SBR_DEC

#include <stdlib.h>
#include <string.h>
#include "sbr_dct.h"
#include "sbr_qmf.h"
#include "sbr_qmf_c.h"
#include "sbr_syntax.h"
#include "../mm_accel.h"

#undef HAVE_MMX
#undef HAVE_MMX2
#undef HAVE_3DNOW
#undef HAVE_3DNOW2
#undef HAVE_SSE
#define RENAME(a) a ## _c
#include "i386/sbr_qmf.h"

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
#include "i386/sbr_qmf.h"
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
#include "i386/sbr_qmf.h"
#endif

#endif // CAN_COMPILE_X86_ASM
extern unsigned faad_cpu_flags;


qmfa_info *qmfa_init(uint8_t channels)
{
    qmfa_info *qmfa = (qmfa_info*)faad_malloc(sizeof(qmfa_info));
    qmfa->x = (real_t*)faad_malloc(channels * 10 * sizeof(real_t));
    memset(qmfa->x, 0, channels * 10 * sizeof(real_t));

    qmfa->channels = channels;

    return qmfa;
}

void qmfa_end(qmfa_info *qmfa)
{
    if (qmfa)
    {
        if (qmfa->x) faad_free(qmfa->x);
        faad_free(qmfa);
    }
}

void sbr_qmf_analysis_32_init(sbr_info *sbr, qmfa_info *qmfa, const real_t *input,
                         qmf_t X[MAX_NTSRHFG][32], uint8_t offset, uint8_t kx)
{
#ifdef CAN_COMPILE_X86_ASM
#ifdef CAN_COMPILE_SSE
	if(faad_cpu_flags & MM_ACCEL_X86_SSE) sbr_qmf_analysis_32 = sbr_qmf_analysis_32_SSE;
	else
#endif
#ifdef CAN_COMPILE_3DNOW
	if(faad_cpu_flags & MM_ACCEL_X86_3DNOW) sbr_qmf_analysis_32 = sbr_qmf_analysis_32_3DNow;
	else
#endif
#endif //CAN_COMPILE_X86_ASM
	sbr_qmf_analysis_32 = sbr_qmf_analysis_32_c;
	(*sbr_qmf_analysis_32)(sbr,qmfa,input,X,offset,kx);
}
void (*sbr_qmf_analysis_32)(sbr_info *sbr, qmfa_info *qmfa, const real_t *input,
                         qmf_t X[MAX_NTSRHFG][32], uint8_t offset, uint8_t kx)=sbr_qmf_analysis_32_init;

qmfs_info *qmfs_init(uint8_t channels)
{
    qmfs_info *qmfs = (qmfs_info*)faad_malloc(sizeof(qmfs_info));

#ifndef SBR_LOW_POWER
    qmfs->v[0] = (real_t*)faad_malloc(channels * 10 * sizeof(real_t));
    memset(qmfs->v[0], 0, channels * 10 * sizeof(real_t));
    qmfs->v[1] = (real_t*)faad_malloc(channels * 10 * sizeof(real_t));
    memset(qmfs->v[1], 0, channels * 10 * sizeof(real_t));
#else
    qmfs->v[0] = (real_t*)faad_malloc(channels * 20 * sizeof(real_t));
    memset(qmfs->v[0], 0, channels * 20 * sizeof(real_t));
    qmfs->v[1] = NULL;
#endif

    qmfs->v_index = 0;

    qmfs->channels = channels;

#ifdef USE_SSE
    if (cpu_has_sse())
    {
        qmfs->qmf_func = sbr_qmf_synthesis_64_sse;
    } else {
        qmfs->qmf_func = sbr_qmf_synthesis_64;
    }
#endif

    return qmfs;
}

void qmfs_end(qmfs_info *qmfs)
{
    if (qmfs)
    {
        if (qmfs->v[0]) faad_free(qmfs->v[0]);
#ifndef SBR_LOW_POWER
        if (qmfs->v[1]) faad_free(qmfs->v[1]);
#endif
        faad_free(qmfs);
    }
}

void sbr_qmf_synthesis_64_init(sbr_info *sbr, qmfs_info *qmfs, qmf_t X[MAX_NTSRHFG][64],
                          real_t *output)
{
#ifdef CAN_COMPILE_X86_ASM
#ifdef CAN_COMPILE_SSE
	if(faad_cpu_flags & MM_ACCEL_X86_SSE) sbr_qmf_synthesis_64 = sbr_qmf_synthesis_64_SSE;
	else
#endif
#ifdef CAN_COMPILE_3DNOW
	if(faad_cpu_flags & MM_ACCEL_X86_3DNOW) sbr_qmf_synthesis_64 = sbr_qmf_synthesis_64_3DNow;
	else
#endif
#endif //CAN_COMPILE_X86_ASM
	sbr_qmf_synthesis_64 = sbr_qmf_synthesis_64_c;
	(*sbr_qmf_synthesis_64)(sbr,qmfs,X,output);
}
void (*sbr_qmf_synthesis_64)(sbr_info *sbr, qmfs_info *qmfs, qmf_t X[MAX_NTSRHFG][64],
                          real_t *output)=sbr_qmf_synthesis_64_init;
#endif
