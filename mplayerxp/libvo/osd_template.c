// Generic alpha renderers for all YUV modes and RGB depths.
// Optimized by Nick and Michael
// Code from Michael Niedermayer (michaelni@gmx.at) is under GPL
#include "pvector/pvector.h"

#ifdef HAVE_INT_PVECTOR
static __inline __m64 __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(_m_load)(const any_t*__P)
{
    return *(const __m64 *)__P;
}
#undef _m_load
#define _m_load PVECTOR_RENAME(_m_load)

static __inline __m64 __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(_m_load_half)(const any_t*__P)
{
    return _mm_cvtsi32_si64 (*(const int *)__P);
}
#undef _m_load_half
#define _m_load_half PVECTOR_RENAME(_m_load_half)

static __inline void __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(_m_store)(any_t*__P, __m64 src)
{
    *(__m64 *)__P = src;
}
#undef _m_store
#define _m_store PVECTOR_RENAME(_m_store)

static __inline void __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(_m_store_half)(any_t*__P, __m64 src)
{
    *(int *)__P = _mm_cvtsi64_si32(src);
}
#undef _m_store_half
#define _m_store_half PVECTOR_RENAME(_m_store_half)

static __inline void __attribute__((__gnu_inline__, __always_inline__))
PVECTOR_RENAME(_m_movntq)(any_t*__P, __m64 src)
{
#ifdef HAVE_MMX2
    _mm_stream_pi(__P,src);
#else
    _m_store(__P,src);
#endif
}
#undef _m_movntq
#define _m_movntq PVECTOR_RENAME(_m_movntq)

#endif

static inline void PVECTOR_RENAME(vo_draw_alpha_yv12)(int w,int h,const unsigned char* src,const unsigned char *srca, int srcstride, unsigned char* dstbase,int dststride,int finalize){
    unsigned y;
#ifdef HAVE_INT_PVECTOR
    __ivec vzero = _ivec_setzero();
#endif
PROFILE_START();
    for(y=0;y<(unsigned)h;y++){
	int x;
	x=0;
#ifdef HAVE_INT_PVECTOR
	_ivec_prefetchw(&dstbase[x]);
	_ivec_prefetch(&src[x]);
	_ivec_prefetch(&srca[x]);
	/* MOVNTDQ: #GP(0) - If memory operand is not aligned on a 16-byte boundary */
	if(!IVEC_ALIGNED(dstbase))
	for(;x<w;x++){
	    unsigned char *dst=&dstbase[x];
	    if(srca[x]) *dst=((dstbase[x]*srca[x])>>8)+src[x];
	    if(IVEC_ALIGNED(dst)) break; /* align on sizeof(MMREG) boundary */
	}
	if((w-x)>=__IVEC_SIZE)
	for(;x<w;x+=__IVEC_SIZE){
	    __ivec vmsk,vdest,vsrc,vsrca,vt[4];
	    _ivec_prefetchw(&dstbase[x+__IVEC_SIZE*4]);
	    _ivec_prefetch(&src[x+__IVEC_SIZE*4]);
	    _ivec_prefetch(&srca[x+__IVEC_SIZE*4]);
	    vdest = _ivec_loada(&dstbase[x]);
	    if(IVEC_ALIGNED(&src[x]))
		vsrc  = _ivec_loada(&src[x]);
	    else
		vsrc  = _ivec_loadu(&src[x]);
	    if(IVEC_ALIGNED(&srca[x]))
		vsrca = _ivec_loada(&srca[x]);
	    else
		vsrca = _ivec_loadu(&srca[x]);
	    vmsk  = _ivec_not(_ivec_cmpeq_s8(vsrca,vzero));
	    vt[0] = _ivec_u16_from_u8(vdest,&vt[1]);
	    vt[2] = _ivec_u16_from_u8(vsrca,&vt[3]);
	    vt[0] = _ivec_srl_s16_imm(_ivec_mullo_s16(vt[0],vt[2]),8);
	    vt[1] = _ivec_srl_s16_imm(_ivec_mullo_s16(vt[1],vt[3]),8);
	    vt[0] = _ivec_add_s8(_ivec_u8_from_u16(vt[0],vt[1]),vsrc);
	    vt[1] = _ivec_blend_u8(vdest,vt[0],vmsk);
	    if(finalize)
		_ivec_stream(&dstbase[x],vt[1]);
	    else
		_ivec_storea(&dstbase[x],vt[1]);
	}
#endif
	for(;x<w;x++){
	    if(srca[x]) dstbase[x]=((dstbase[x]*srca[x])>>8)+src[x];
	}
	src+=srcstride;
	srca+=srcstride;
	dstbase+=dststride;
    }
#ifdef HAVE_INT_PVECTOR
    _ivec_empty();
    if(finalize) _ivec_sfence();
#endif
PROFILE_END("vo_draw_alpha_yv12");
    return;
}

static inline void PVECTOR_RENAME(vo_draw_alpha_yuy2)(int w,int h,const unsigned char* src,const unsigned char *srca, int srcstride, unsigned char* dstbase,int dststride,int finalize){
    int y;
#if defined(FAST_OSD) && !defined(HAVE_MMX)
    w=w>>1;
#endif
PROFILE_START();
    for(y=0;y<h;y++){
        register int x=0;
#ifdef HAVE_INT_PVECTOR
	/* TODO: port this stuff on PVector */
	__m64 mm[8];
	_ivec_prefetchw(&dstbase[x]);
	_ivec_prefetch(&src[x]);
	_ivec_prefetch(&srca[x]);
	mm[7]=_mm_setzero_si64();
	mm[5]=_mm_set1_pi8(0xFF);
	mm[4]=mm[5];
	mm[5]=_m_psllwi(mm[5],8);
	mm[4]=_m_psrlwi(mm[4],8);
    for(;x<w;x+=4){
	    _ivec_prefetchw(&dstbase[x+__IVEC_SIZE*4]);
	    _ivec_prefetch(&src[x+__IVEC_SIZE*4]);
	    _ivec_prefetch(&srca[x+__IVEC_SIZE*4]);
	    mm[0]=_m_load(&(((char *)dstbase)[x*2]));
	    mm[1]=mm[0];
	    mm[0]=_m_pand(mm[0],mm[4]);
	    mm[2]=_m_load_half(&(((char *)srca)[x]));
	    mm[2]=_m_paddb(mm[2],_m_load(&bFF));
	    mm[2]=_m_punpcklbw(mm[2],mm[7]);
	    mm[0]=_m_pmullw(mm[0],mm[2]);
	    mm[0]=_m_psrlwi(mm[0],8);
	    mm[1]=_m_pand(mm[1],mm[5]);
	    mm[2]=_m_load_half(&(((char *)src)[x]));
	    mm[2]=_m_punpcklbw(mm[2],mm[7]);
	    mm[0]=_m_por(mm[0],mm[1]);
	    mm[0]=_m_paddb(mm[0],mm[2]);
	    if(finalize)
		_m_movntq(&(((char *)dstbase)[x*2]),mm[0]);
	    else
		_m_store(&(((char *)dstbase)[x*2]),mm[0]);
	}
#endif
        for(;x<w;x++){
#ifdef FAST_OSD
            if(srca[2*x+0]) dstbase[4*x+0]=src[2*x+0];
            if(srca[2*x+1]) dstbase[4*x+2]=src[2*x+1];
#else
            if(srca[x]) {
               dstbase[2*x]=((dstbase[2*x]*srca[x])>>8)+src[x];
               dstbase[2*x+1]=((((signed)dstbase[2*x+1]-128)*srca[x])>>8)+128;
           }
#endif
        }
	src+=srcstride;
        srca+=srcstride;
        dstbase+=dststride;
    }
#ifdef HAVE_INT_PVECTOR
    _ivec_empty();
    if(finalize) _ivec_sfence();
#endif
PROFILE_END("vo_draw_alpha_yuy2");
    return;
}

static inline void PVECTOR_RENAME(vo_draw_alpha_rgb24)(int w,int h,const unsigned char* src,const unsigned char *srca, int srcstride, unsigned char* dstbase,int dststride,int finalize){
    int y;
    for(y=0;y<h;y++){
        register unsigned char *dst = dstbase;
        register int x=0;
#ifdef HAVE_INT_PVECTOR
	__m64 mm[8];
	_ivec_prefetchw(&dstbase[x]);
	_ivec_prefetch(&src[x]);
	_ivec_prefetch(&srca[x]);
	mm[7]=_mm_setzero_si64();
	mm[6]=_mm_set1_pi8(0xFF);
    for(;x<w;x+=2){
     if(srca[x] || srca[x+1]) {
	    _ivec_prefetchw(&dstbase[x+__IVEC_SIZE*4]);
	    _ivec_prefetch(&src[x+__IVEC_SIZE*4]);
	    _ivec_prefetch(&srca[x+__IVEC_SIZE*4]);
	    mm[0]=_m_load(&dstbase[0]);
	    mm[1]=mm[0];
	    mm[5]=mm[0];
	    mm[0]=_m_punpcklbw(mm[0],mm[7]);
	    mm[1]=_m_punpckhbw(mm[1],mm[7]);
	    mm[2]=_m_load_half(&srca[x]);
	    mm[2]=_m_paddb(mm[2],mm[6]);
	    mm[2]=_m_punpcklbw(mm[2],mm[2]);
	    mm[2]=_m_punpcklbw(mm[2],mm[2]);
	    mm[3]=mm[2];
	    mm[2]=_m_punpcklbw(mm[2],mm[7]);
	    mm[3]=_m_punpckhbw(mm[3],mm[7]);
	    mm[0]=_m_pmullw(mm[0],mm[2]);
	    mm[1]=_m_pmullw(mm[1],mm[3]);
	    mm[0]=_m_psrlwi(mm[0],8);
	    mm[1]=_m_psrlwi(mm[1],8);
	    mm[0]=_m_packuswb(mm[0],mm[1]);
	    mm[2]=_m_load_half(&src[x]);
	    mm[2]=_m_punpcklbw(mm[2],mm[2]);
	    mm[2]=_m_punpcklbw(mm[2],mm[2]);
	    mm[0]=_m_paddb(mm[0],mm[2]);
	    mm[5]=_m_pand(mm[5],_m_load(&mask24lh));
	    mm[0]=_m_pand(mm[0],_m_load(&mask24hl));
	    mm[5]=_m_por(mm[5],mm[0]);
	    if(finalize)
		_m_movntq(&dstbase[0],mm[5]);
	    else
		_m_store(&dstbase[0],mm[5]);
	}
	dst += 6;
    }
#endif /* arch_x86 */
        for(;x<w;x++){
            if(srca[x]){
#ifdef FAST_OSD
		dst[0]=dst[1]=dst[2]=src[x];
#else
		dst[0]=((dst[0]*srca[x])>>8)+src[x];
		dst[1]=((dst[1]*srca[x])>>8)+src[x];
		dst[2]=((dst[2]*srca[x])>>8)+src[x];
#endif
            }
            dst+=3; // 24bpp
        }
        src+=srcstride;
        srca+=srcstride;
        dstbase+=dststride;
    }
#ifdef HAVE_INT_PVECTOR
    _ivec_empty();
    if(finalize) _ivec_sfence();
#endif
    return;
}

static inline void PVECTOR_RENAME(vo_draw_alpha_rgb32)(int w,int h,const unsigned char* src,const unsigned char *srca, int srcstride, unsigned char* dstbase,int dststride,int finalize){
    int y;
PROFILE_START();
    for(y=0;y<h;y++){
        register int x=0;
#ifdef HAVE_INT_PVECTOR
	__m64 mm[8];
	_ivec_prefetchw(&dstbase[x]);
	_ivec_prefetch(&src[x]);
	_ivec_prefetch(&srca[x]);
	mm[7]=_mm_setzero_si64();
	mm[6]=_mm_set1_pi8(0xFF);
    for(;x<w;x+=2){
     if(srca[x] || srca[x+1]) {
	    _ivec_prefetchw(&dstbase[x+__IVEC_SIZE*4]);
	    _ivec_prefetch(&src[x+__IVEC_SIZE*4]);
	    _ivec_prefetch(&srca[x+__IVEC_SIZE*4]);
	    mm[0]=_m_load(&dstbase[4*x]);
	    mm[1]=mm[0];
	    mm[0]=_m_punpcklbw(mm[0],mm[7]);
	    mm[1]=_m_punpckhbw(mm[1],mm[7]);
	    mm[2]=_m_load_half(&srca[x]);
	    mm[2]=_m_paddb(mm[2],mm[6]);
	    mm[2]=_m_punpcklbw(mm[2],mm[2]);
	    mm[2]=_m_punpcklbw(mm[2],mm[2]);
	    mm[3]=mm[2];
	    mm[2]=_m_punpcklbw(mm[2],mm[7]);
	    mm[3]=_m_punpckhbw(mm[3],mm[7]);
	    mm[0]=_m_pmullw(mm[0],mm[2]);
	    mm[1]=_m_pmullw(mm[1],mm[3]);
	    mm[0]=_m_psrlwi(mm[0],8);
	    mm[1]=_m_psrlwi(mm[1],8);
	    mm[0]=_m_packuswb(mm[0],mm[1]);
	    mm[2]=_m_load_half(&src[x]);
	    mm[2]=_m_punpcklbw(mm[2],mm[2]);
	    mm[2]=_m_punpcklbw(mm[2],mm[2]);
	    mm[0]=_m_paddb(mm[0],mm[2]);
	    if(finalize)
		_m_movntq(&dstbase[4*x],mm[0]);
	    else
		_m_store(&dstbase[4*x],mm[0]);
	}
    }
#endif /* arch_x86 */
        for(;x<w;x++){
            if(srca[x]){
#ifdef FAST_OSD
		dstbase[4*x+0]=dstbase[4*x+1]=dstbase[4*x+2]=src[x];
#else
		dstbase[4*x+0]=((dstbase[4*x+0]*srca[x])>>8)+src[x];
		dstbase[4*x+1]=((dstbase[4*x+1]*srca[x])>>8)+src[x];
		dstbase[4*x+2]=((dstbase[4*x+2]*srca[x])>>8)+src[x];
#endif
            }
        }
        src+=srcstride;
        srca+=srcstride;
        dstbase+=dststride;
    }
#ifdef HAVE_INT_PVECTOR
    _ivec_empty();
    if(finalize) _ivec_sfence();
#endif
PROFILE_END("vo_draw_alpha_rgb32");
    return;
}
