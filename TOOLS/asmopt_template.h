/* Put your stuff_template.h here */
#include "../mplayerxp/pvector/pvector.h"

/*
Benchmarks for this test:
-------------------------
GENERIC: cpu clocks=27765822 = 12564us
SSE3   : cpu clocks=3991075 = 1808us...[OK]
SSE2   : cpu clocks=3942081 = 1785us...[OK]
MMX2   : cpu clocks=5874563 = 2659us...[OK]
MMX    : cpu clocks=6092012 = 2757us...[OK]
*/
void RENAME(convert)(unsigned char *dstbase,unsigned char *src,unsigned char *srca,unsigned int asize)
{
#ifdef HAVE_INT_PVECTOR
    __ivec vzero = _ivec_setzero();
#endif
    unsigned x,w;
    x = 0;
    w = asize;
#ifdef HAVE_INT_PVECTOR
	if(w>=__IVEC_SIZE)
	for(;x<w;x+=__IVEC_SIZE){
	    __ivec vmsk,vdest,vsrc,vsrca,vt[4];
	    vdest = _ivec_loadu(&dstbase[x]);
	    vsrc  = _ivec_loada(&src[x]);
	    vsrca = _ivec_loada(&srca[x]);
	    vmsk  = _ivec_not(_ivec_cmpeq_s8(vsrca,vzero));
	    vt[0] = _ivec_u16_from_lou8(vdest);
	    vt[1] = _ivec_u16_from_hiu8(vdest);
	    vt[2] = _ivec_u16_from_lou8(vsrca);
	    vt[3] = _ivec_u16_from_hiu8(vsrca);
	    vt[0] = _ivec_srl_s16_imm(_ivec_mullo_s16(vt[0],vt[2]),8);
	    vt[1] = _ivec_srl_s16_imm(_ivec_mullo_s16(vt[1],vt[3]),8);
	    vt[0] = _ivec_add_s8(_ivec_u8_from_u16(vt[0],vt[1]),vsrc);
	    _ivec_storeu(&dstbase[x],_ivec_blend_u8(vdest,vt[0],vmsk));
	}
#endif
	for(;x<(unsigned)w;x++){
	    if(srca[x]) dstbase[x]=((dstbase[x]*srca[x])>>8)+src[x];
	}
#ifdef HAVE_INT_PVECTOR
    _ivec_empty();
    _ivec_sfence();
#endif
}
