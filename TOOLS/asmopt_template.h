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
void PVECTOR_RENAME(convert)(unsigned char *dstbase,unsigned char *src,unsigned char *srca,unsigned int asize)
{
#ifdef HAVE_INT_PVECTOR
    __ivec izero = _ivec_setzero();
#endif
    uint8_t *out_data = dstbase;
    uint8_t *in_data = src;

    unsigned i,len;
    i = 0;
    len = asize;
#ifdef HAVE_INT_PVECTOR
    for(;i<len;i++) {
	((uint16_t*)out_data)[i]=((uint16_t)((uint8_t*)in_data)[i])<<8;
	if((((long)out_data)&(__IVEC_SIZE-1))==0) break;
    }
    if((len-i)>=__IVEC_SIZE)
    for(;i<len;i+=__IVEC_SIZE){
	    __ivec ind,itmp[2];
	    ind   = _ivec_loadu(&((uint8_t *)in_data)[i]);
	    itmp[0]= _ivec_s16_from_s8(ind,&itmp[1]);
	    _ivec_storea(&((uint16_t*)out_data)[i],itmp[0]);
	    _ivec_storea(&((uint16_t*)out_data)[i+__IVEC_SIZE/2],itmp[1]);
    }
#endif
    for(;i<len;i++)
	((int16_t*)out_data)[i]=((int16_t)((int8_t*)in_data)[i]);
#ifdef HAVE_INT_PVECTOR
    _ivec_empty();
    _ivec_sfence();
#endif
}
