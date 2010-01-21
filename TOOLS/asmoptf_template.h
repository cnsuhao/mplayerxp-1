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
#ifndef SATURATE
#define SATURATE(x,_min,_max) {if((x)<(_min)) (x)=(_min); else if((x)>(_max)) (x)=(_max);}
#endif

static void __FASTCALL__ PVECTOR_RENAME(float2int32)(float* in, int32_t* out, unsigned len, int final)
{
    register unsigned i;
    float ftmp;
#ifdef HAVE_F32_PVECTOR
  __f32vec int_max;
#endif
    i=0;
#ifdef HAVE_F32_PVECTOR
    int_max = _f32vec_broadcast(INT_MAX-1);
    for(;i<len;i++) {
      ftmp=((float*)in)[i];
      SATURATE(ftmp,-1.0,+1.0);
      ((int32_t*)out)[i]=(int32_t)lrintf((INT_MAX-1)*ftmp);
      if((((long)out)&(__F32VEC_SIZE-1))==0) break;
    }
    _ivec_empty();
    if((len-i)>=__F32VEC_SIZE)
    for(;i<len;i+=__F32VEC_SIZE/sizeof(float)) {
	__f32vec tmp;
	tmp = _f32vec_mul(int_max,_f32vec_loadu(&((float*)in)[i]));
	if(final)
	    _f32vec_to_s32_stream(&((int32_t*)out)[i],tmp);
	else
	    _f32vec_to_s32a(&((int32_t*)out)[i],tmp);
    }
    _ivec_sfence();
    _ivec_empty();
#endif
    for(;i<len;i++) {
      ftmp=((float*)in)[i];
      SATURATE(ftmp,-1.0,+1.0);
      ((int32_t*)out)[i]=(int32_t)lrintf((INT_MAX-1)*ftmp);
    }
}

void PVECTOR_RENAME(convert)(uint8_t *dstbase,const uint8_t *src,const uint8_t *srca,unsigned int asize)
{
    unsigned i,len;
    const uint8_t *in;
    float ftmp;
    int32_t __attribute__((aligned(__IVEC_SIZE))) i32_tmp[asize];
#ifdef HAVE_F32_PVECTOR
    __f32vec int_max = _f32vec_broadcast(SHRT_MAX);
#endif
    uint8_t *out;
    in = srca;
    out = dstbase;
    i=0;
    len=asize;
#ifdef HAVE_F32_PVECTOR
    for(;i<len;i++) {
      ftmp=((float*)in)[i];
      SATURATE(ftmp,-1.0,+1.0);
      ((int16_t*)out)[i]=(int16_t)lrintf(SHRT_MAX*ftmp);
      if((((long)out)&(__F32VEC_SIZE-1))==0) break;
    }
    _ivec_empty();
    if((len-i)>=__F32VEC_SIZE) {
    PVECTOR_RENAME(float2int32)(in,i32_tmp,len-i,0);
    for(;i<len;i+=__IVEC_SIZE*2/sizeof(int32_t)) {
	__ivec tmp[2];
	tmp[0] = _ivec_loada(&((int32_t*)i32_tmp)[i]));
	tmp[1] = _ivec_loada(&((int32_t*)i32_tmp)[i+__F32VEC_SIZE]));
	_f32vec_to_s32a(itmp[0],ftmp[0]);
	_f32vec_to_s32a(itmp[1],ftmp[1]);
	tmp=_ivec_s16_from_s32(_ivec_loada(itmp[1]),_ivec_loada(itmp[0]));
	_ivec_storea(&((int16_t*)out)[i],tmp);
    }
    }
    _ivec_sfence();
    _ivec_empty();
#endif
    for(;i<len;i++) {
      ftmp=((float*)in)[i];
      SATURATE(ftmp,-1.0,+1.0);
      ((int16_t*)out)[i]=(int16_t)lrintf(SHRT_MAX*ftmp);
    }
}
