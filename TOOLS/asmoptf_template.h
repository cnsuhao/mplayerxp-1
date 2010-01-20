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
void RENAME(convert)(uint8_t *dstbase,const uint8_t *src,const uint8_t *srca,unsigned int asize)
{
    unsigned i,len;
    const uint8_t *in;
    uint8_t *out;
#ifdef HAVE_F32_PVECTOR
    __f32vec rev_imax = _f32vec_broadcast(1.0/INT_MAX);
#endif
    in = src;
    out = dstbase;
    i=0;
    len=asize;
#ifdef HAVE_F32_PVECTOR
    for(;i<len;i++) {
      ((float*)out)[i]=(1.0/INT_MAX)*((float)((int32_t*)in)[i]);
      if((((long)out)&(__F32VEC_SIZE-1))==0) break;
    }
    _f32vec_empty();
    if((len-i)>=__F32VEC_SIZE)
    for(;i<len;i+=__F32VEC_SIZE/sizeof(float)) {
	__f32vec tmp;
	tmp = _f32vec_mul(rev_imax,_f32vec_from_s32u(&((int32_t*)in)[i]));
	_f32vec_storea(&((float*)out)[i],tmp);
    }
    _f32vec_sfence();
    _f32vec_empty();
#endif
    for(;i<len;i++) {
      ((float*)out)[i]=(1.0/INT_MAX)*((float)((int32_t*)in)[i]);
    }
}
