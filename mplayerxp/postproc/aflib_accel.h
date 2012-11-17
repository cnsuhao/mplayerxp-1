/* DSP acceleration routines */
#include "pvector/pvector.h"

static void __FASTCALL__ PVECTOR_RENAME(int8_to_int16)(const int8_t* in_data, int16_t* out_data, unsigned inlen, int final)
{
    unsigned i,insamples = inlen/sizeof(int8_t);
#ifdef HAVE_INT_PVECTOR
    unsigned len_mm,mm_insamples,vec_insamples;
    len_mm=inlen&(~(_ivec_size()-1));
    mm_insamples=len_mm/sizeof(int8_t);
    vec_insamples=_ivec_size()/sizeof(int8_t);
#else
    UNUSED(final);
#endif
    i = 0;
#ifdef HAVE_INT_PVECTOR
    if(!_ivec_aligned(out_data))
    for(;i<insamples;i++) {
	((uint16_t*)out_data)[i]=((uint16_t)((const uint8_t*)in_data)[i])<<8;
	if(_ivec_aligned(out_data)) break;
    }
    if((len_mm-i)>=_ivec_size())
    for(;i<mm_insamples;i+=vec_insamples){
	__ivec ind,itmp[2];
	if(_ivec_aligned(in_data))
	    ind = _ivec_loada(&((const uint8_t *)in_data)[i]);
	else
	    ind = _ivec_loadu(&((const uint8_t *)in_data)[i]);
	itmp[0] = _ivec_scale_u16_from_u8(ind,&itmp[1]);
	if(final) {
	    _ivec_stream(&((uint16_t*)out_data)[i],itmp[0]);
	    _ivec_stream(&((uint16_t*)out_data)[i+_ivec_size()/2],itmp[1]);
	} else {
	    _ivec_storea(&((uint16_t*)out_data)[i],itmp[0]);
	    _ivec_storea(&((uint16_t*)out_data)[i+_ivec_size()/2],itmp[1]);
	}
    }
    _ivec_empty();
    if(final) _ivec_sfence();
#endif
	for(;i<insamples;i++)
	  ((uint16_t*)out_data)[i]=((uint16_t)((const uint8_t*)in_data)[i])<<8;
}

static void __FASTCALL__ PVECTOR_RENAME(int16_to_int8)(const int16_t* in_data, int8_t* out_data, unsigned inlen, int final)
{
    unsigned i,insamples = inlen/sizeof(int16_t);
#ifdef HAVE_INT_PVECTOR
    unsigned len_mm,mm_insamples,vec_insamples;
    len_mm=inlen&(~(_ivec_size()-1));
    mm_insamples=len_mm/sizeof(int16_t);
    vec_insamples=_ivec_size()/sizeof(int16_t);
#else
    UNUSED(final);
#endif
    i = 0;
#ifdef HAVE_INT_PVECTOR
    if(!_ivec_aligned(out_data))
    for(;i<insamples;i++) {
	((uint8_t*)out_data)[i]=(uint8_t)((((const uint16_t*)in_data)[i])>>8);
	if(_ivec_aligned(out_data)) break;
    }
    if((len_mm-i)>=_ivec_size())
    for(;i<mm_insamples;i+=vec_insamples){
	__ivec outd,itmp[2];
	if(_ivec_aligned(in_data)) {
	    itmp[0] = _ivec_loada(&((const uint16_t*)in_data)[i]);
	    itmp[1] = _ivec_loada(&((const uint16_t*)in_data)[i+_ivec_size()/2]);
	}
	else {
	    itmp[0] = _ivec_loadu(&((const uint16_t*)in_data)[i]);
	    itmp[1] = _ivec_loadu(&((const uint16_t*)in_data)[i+_ivec_size()/2]);
	}
	outd     = _ivec_scale_s8_from_s16(itmp[0],itmp[1]);
	if(final)
	    _ivec_stream(&((uint8_t*)out_data)[i],outd);
	else
	    _ivec_storea(&((uint8_t*)out_data)[i],outd);
    }
    _ivec_empty();
    if(final) _ivec_sfence();
#endif
    for(;i<insamples;i++)
	((uint8_t*)out_data)[i]=(uint8_t)((((const uint16_t*)in_data)[i])>>8);
}

static void __FASTCALL__ PVECTOR_RENAME(int16_to_int32)(const int16_t* in_data, int32_t* out_data, unsigned inlen, int final)
{
    unsigned i,insamples = inlen/sizeof(int16_t);
#ifdef HAVE_INT_PVECTOR
    unsigned len_mm,mm_insamples,vec_insamples;
    len_mm=inlen&(~(_ivec_size()-1));
    mm_insamples=len_mm/sizeof(int16_t);
    vec_insamples=_ivec_size()/sizeof(int16_t);
#else
    UNUSED(final);
#endif
    i=0;
#ifdef HAVE_INT_PVECTOR
    if(!_ivec_aligned(out_data))
    for(;i<insamples;i++){
	((uint32_t*)out_data)[i]=((uint32_t)((const uint16_t*)in_data)[i])<<16;
	if(_ivec_aligned(out_data)) break;
    }
    if((len_mm-i)>=_ivec_size())
    for(;i<mm_insamples;i+=vec_insamples)
    {
	__ivec ind,tmp[2];
	if(_ivec_aligned(in_data))
	    ind = _ivec_loada(&((const uint8_t *)in_data)[i]);
	else
	    ind = _ivec_loadu(&((const uint8_t *)in_data)[i]);
	tmp[0]= _ivec_scale_u32_from_u16(ind,&tmp[1]);
	if(final) {
	    _ivec_stream(&((uint8_t *)out_data)[i*2],tmp[0]);
	    _ivec_stream(&((uint8_t *)out_data)[i*2+_ivec_size()],tmp[1]);
	} else {
	    _ivec_storea(&((uint8_t *)out_data)[i*2],tmp[0]);
	    _ivec_storea(&((uint8_t *)out_data)[i*2+_ivec_size()],tmp[1]);
	}
    }
    if(final) _ivec_sfence();
    _ivec_empty();
#endif
    for(;i<insamples;i++)
	((uint32_t*)out_data)[i]=((uint32_t)((const uint16_t*)in_data)[i])<<16;
}

static void __FASTCALL__ PVECTOR_RENAME(int32_to_int16)(const int32_t* in_data, int16_t* out_data, unsigned inlen, int final)
{
    unsigned i,insamples = inlen/sizeof(int32_t);
#ifdef HAVE_INT_PVECTOR
    unsigned len_mm,mm_insamples,vec_insamples;
    len_mm=inlen&(~(_ivec_size()-1));
    mm_insamples=len_mm/sizeof(int32_t);
    vec_insamples=_ivec_size()/sizeof(int32_t);
#else
    UNUSED(final);
#endif
    i=0;
#ifdef HAVE_INT_PVECTOR
    if(!_ivec_aligned(out_data))
    for(;i<insamples;i++){
	((uint16_t*)out_data)[i]=(uint16_t)((((const uint32_t*)in_data)[i])>>16);
	if(_ivec_aligned(out_data)) break;
    }
    if((len_mm-i)>=_ivec_size())
    for(;i<mm_insamples;i+=vec_insamples)
    {
	__ivec ind[2],tmp;
	if(_ivec_aligned(in_data)) {
	    ind[0]=_ivec_loada(&((const uint8_t *)in_data)[i*2]);
	    ind[1]=_ivec_loada(&((const uint8_t *)in_data)[i*2+_ivec_size()]);
	} else {
	    ind[0]=_ivec_loadu(&((const uint8_t *)in_data)[i*2]);
	    ind[1]=_ivec_loadu(&((const uint8_t *)in_data)[i*2+_ivec_size()]);
	}
	tmp   = _ivec_scale_s16_from_s32(ind[0],ind[1]);
	if(final)
	    _ivec_stream(&((uint8_t *)out_data)[i],tmp);
	else
	    _ivec_storea(&((uint8_t *)out_data)[i],tmp);
    }
    if(final) _ivec_sfence();
    _ivec_empty();
#endif
    for(;i<insamples;i++)
	((uint16_t*)out_data)[i]=(uint16_t)((((const uint32_t*)in_data)[i])>>16);
}

static void __FASTCALL__ PVECTOR_RENAME(change_bps)(const mp_aframe_t* in, mp_aframe_t* out)
{
    unsigned i,bps=in->format&MPAF_BPS_MASK,outbps=out->format&MPAF_BPS_MASK;
  // Change the number of bits
    switch(bps){
	case 1:
	switch(outbps){
	    case 2:
		PVECTOR_RENAME(int8_to_int16)(reinterpret_cast<int8_t*>(in->audio),reinterpret_cast<int16_t*>(out->audio),in->len,out->flags&MP_AFLG_FINALIZED);
		break;
	    case 3:
		for(i=0;i<in->len;i++) {
		    ((uint8_t*)out->audio)[3*i]=
		    ((uint8_t*)out->audio)[3*i+1]=0;
		    ((uint8_t*)out->audio)[3*i+2]=(((uint8_t*)in->audio)[i]);
		}
		break;
	    case 4:
		for(i=0;i<in->len;i++) ((uint32_t*)out->audio)[i]=((uint32_t)((uint8_t*)in->audio)[i])<<24;
		break;
	}
	break;
	case 2:
	switch(outbps){
	    case 1:
		PVECTOR_RENAME(int16_to_int8)(reinterpret_cast<int16_t*>(in->audio),reinterpret_cast<int8_t*>(out->audio),in->len,out->flags&MP_AFLG_FINALIZED);
		break;
	    case 3:
		for(i=0;i<in->len;i++) {
		    ((uint8_t*)out->audio)[3*i]=0;
		    ((uint8_t*)out->audio)[3*i+1]=(((uint8_t*)in->audio)[2*i]);
		    ((uint8_t*)out->audio)[3*i+2]=(((uint8_t*)in->audio)[2*i+1]);
		}
		break;
	    case 4:
		PVECTOR_RENAME(int16_to_int32)(reinterpret_cast<int16_t*>(in->audio),reinterpret_cast<int32_t*>(out->audio),in->len,out->flags&MP_AFLG_FINALIZED);
		break;
	}
	break;
	case 3:
	switch(outbps){
	    case 1:
		for(i=0;i<in->len;i++) ((uint8_t*)out->audio)[i]=(((uint8_t*)in->audio)[3*i]);
		break;
	    case 2:
		for(i=0;i<in->len;i++) {
		    ((uint8_t*)out->audio)[2*i]=(uint8_t)(((uint8_t*)in->audio)[3*i+1]);
		    ((uint8_t*)out->audio)[2*i+1]=(uint8_t)(((uint8_t*)in->audio)[3*i+2]);
		}
		break;
	    case 4:
		for(i=0;i<in->len;i++) {
		    ((uint8_t*)out->audio)[4*i+1]=((uint8_t*)in->audio)[3*i];
		    ((uint8_t*)out->audio)[4*i+2]=((uint8_t*)in->audio)[3*i+1];
		    ((uint8_t*)out->audio)[4*i+3]=((uint8_t*)in->audio)[3*i+2];
		    ((uint8_t*)out->audio)[4*i]=0;
		}
		break;
	}
	break;
	case 4:
	switch(outbps){
	    case 1:
		for(i=0;i<in->len;i++) ((uint8_t*)out->audio)[i]=(uint8_t)((((uint32_t*)in->audio)[i])>>24);
		break;
	    case 2:
		PVECTOR_RENAME(int32_to_int16)(reinterpret_cast<int32_t*>(in->audio),reinterpret_cast<int16_t*>(out->audio),in->len,out->flags&MP_AFLG_FINALIZED);
		break;
	    case 3:
		for(i=0;i<in->len;i++) {
		    ((uint8_t*)out->audio)[3*i]=(((uint8_t*)in->audio)[4*i+1]);
		    ((uint8_t*)out->audio)[3*i+1]=(((uint8_t*)in->audio)[4*i+2]);
		    ((uint8_t*)out->audio)[3*i+2]=(((uint8_t*)in->audio)[4*i+3]);
		}
	    break;
	}
	break;
    }
}

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
static int32_t __FASTCALL__ PVECTOR_RENAME(FIR_i16)(const int16_t *x,const int16_t *w)
{
#ifdef OPTIMIZE_SSE
    __m64 mm[8];
    mm[0] = _m_load(&w[0]);
    mm[1] = _m_load(&w[4]);
    mm[2] = _m_load(&w[8]);
    mm[3] = _m_load(&w[12]);

    mm[4] = _m_pmaddwd(mm[0],_m_load(&x[0]));
    mm[5] = _m_pmaddwd(mm[1],_m_load(&x[4]));
    mm[6] = _m_pmaddwd(mm[2],_m_load(&x[8]));
    mm[7] = _m_pmaddwd(mm[3],_m_load(&x[12]));

    mm[0] = _m_paddd(mm[4],mm[5]);
    mm[1] = _m_paddd(mm[6],mm[7]);
    mm[2] = _m_paddd(mm[0],mm[1]);
    mm[0] = _m_pshufw(mm[2],0xFE);

    mm[0] = _m_paddd(mm[0],mm[2]);
    mm[0] = _m_psrldi(mm[0],16);
    return _mm_cvtsi64_si32(mm[0]);
#else
  return ( w[0] *x[0] +w[1] *x[1] +w[2] *x[2] +w[3] *x[3]
	 + w[4] *x[4] +w[5] *x[5] +w[6] *x[6] +w[7] *x[7]
	 + w[8] *x[8] +w[9] *x[9] +w[10]*x[10]+w[11]*x[11]
	 + w[12]*x[12]+w[13]*x[13]+w[14]*x[14]+w[15]*x[15] ) >> 16;
#endif
}

static void __FASTCALL__ PVECTOR_RENAME(float_to_int32)(const float* in, int32_t* out, unsigned inlen, int final)
{
    float ftmp;
    unsigned i,insamples = inlen/sizeof(float);
#ifdef HAVE_F32_PVECTOR
    __f32vec int_max,plus1,minus1;
    unsigned len_mm,mm_insamples,vec_insamples;
    len_mm=inlen&(~(_f32vec_size()-1));
    mm_insamples=len_mm/sizeof(float);
    vec_insamples=_f32vec_size()/sizeof(float);
#else
    UNUSED(final);
#endif
    i=0;
#ifdef HAVE_F32_PVECTOR
    int_max = _f32vec_broadcast(std::numeric_limits<int32_t>::max()-1);
    /* SSE float2int engine doesn't have SATURATION functionality.
       So CLAMP volume on 0.0002% here. */
    plus1 = _f32vec_broadcast(+0.999998);
    minus1= _f32vec_broadcast(-0.999998);
    if(!_ivec_aligned(out))
    for(;i<insamples;i++) {
      ftmp=((const float*)in)[i];
      SATURATE(ftmp,-0.999998,+0.999998);
      ((int32_t*)out)[i]=(int32_t)lrintf((std::numeric_limits<int32_t>::max()-1)*ftmp);
      if(_ivec_aligned(out)) break;
    }
    _ivec_empty();
    len_mm=inlen&(~(_f32vec_size()-1));
    if((len_mm-i)>=_f32vec_size()/sizeof(float))
    for(;i<mm_insamples;i+=vec_insamples) {
	__f32vec tmp;
	if(_f32vec_aligned(in))
	    tmp = _f32vec_loada(&((const float*)in)[i]);
	else
	    tmp = _f32vec_loadu(&((const float*)in)[i]);
	tmp = _f32vec_clamp(tmp,minus1,plus1);
	tmp = _f32vec_mul(int_max,tmp);
	if(final)
	    _f32vec_to_s32_stream(&((int32_t*)out)[i],tmp);
	else
	    _f32vec_to_s32a(&((int32_t*)out)[i],tmp);
    }
    if(final) _ivec_sfence();
    _ivec_empty();
#endif
    for(;i<insamples;i++) {
      ftmp=((const float*)in)[i];
      SATURATE(ftmp,-0.999998,+0.999998);
      ((int32_t*)out)[i]=(int32_t)lrintf((std::numeric_limits<int32_t>::max()-1)*ftmp);
    }
}

static void __FASTCALL__ PVECTOR_RENAME(int32_to_float)(const int32_t* in, float* out, unsigned inlen, int final)
{
    unsigned i,insamples = inlen/sizeof(int32_t);
#ifdef HAVE_INT_PVECTOR
    unsigned len_mm,mm_insamples,vec_insamples;
    len_mm=inlen&(~(_ivec_size()-1));
    mm_insamples=len_mm/sizeof(int32_t);
    vec_insamples=_ivec_size()/sizeof(int32_t);
    __f32vec rev_imax = _f32vec_broadcast(1.0/std::numeric_limits<int32_t>::max());
#else
    UNUSED(final);
#endif
    i=0;
#ifdef HAVE_F32_PVECTOR
    if(!_f32vec_aligned(out))
    for(;i<insamples;i++) {
      ((float*)out)[i]=(1.0/std::numeric_limits<int32_t>::max())*((float)((const int32_t*)in)[i]);
      if(_f32vec_aligned(out)) break;
    }
    _ivec_empty();
    if((len_mm-i)>=_f32vec_size())
    for(;i<mm_insamples;i+=vec_insamples) {
	__f32vec tmp;
	if(_ivec_aligned(in))
	    tmp = _f32vec_from_s32a(&((const int32_t*)in)[i]);
	else
	    tmp = _f32vec_from_s32u(&((const int32_t*)in)[i]);
	tmp = _f32vec_mul(rev_imax,tmp);
	if(final)
	    _f32vec_stream(&((float*)out)[i],tmp);
	else
	    _f32vec_storea(&((float*)out)[i],tmp);
    }
    if(final) _ivec_sfence();
    _ivec_empty();
#endif
    for(;i<insamples;i++)
      ((float*)out)[i]=(1.0/std::numeric_limits<int32_t>::max())*((float)((const int32_t*)in)[i]);
}

static void __FASTCALL__ PVECTOR_RENAME(float2int)(const mp_aframe_t* in, mp_aframe_t* out)
{
    float ftmp;
    unsigned i,bps=out->format&MPAF_BPS_MASK;
    switch(bps){
	case 1:
	for(i=0;i<in->len;i++) {
	    ftmp=((float*)in->audio)[i];
	    SATURATE(ftmp,-1.0,+1.0);
	    ((int8_t*)out->audio)[i]=(int8_t)lrintf(std::numeric_limits<int16_t>::max()*ftmp);
	}
	break;
	case 2:
#ifdef HAVE_INT_PVECTOR
	{
	    int32_t __attribute__((aligned(64))) itmp[in->len];
	    /* unfortunatelly there is now universal way for convertion on perfectly different chips */
	    PVECTOR_RENAME(float_to_int32)(reinterpret_cast<float*>(in->audio), itmp, in->len, 0);
	    PVECTOR_RENAME(int32_to_int16)(itmp, reinterpret_cast<int16_t*>(out->audio), in->len, out->flags&MP_AFLG_FINALIZED);
	}
#else
	    for(i=0;i<in->len;i++) {
		ftmp=((float*)in->audio)[i];
		SATURATE(ftmp,-1.0,+1.0);
		((int16_t*)out->audio)[i]=(int16_t)lrintf(std::numeric_limits<int16_t>::max()*ftmp);
	    }
#endif
	    break;
	case 3:
	    for(i=0;i<in->len;i++) {
		ftmp=((float*)in->audio)[i];
		SATURATE(ftmp,-1.0,+1.0);
		store24bit(out->audio, i, (int32_t)lrintf((std::numeric_limits<int32_t>::max()-1)*ftmp));
	    }
	    break;
	case 4:
	    PVECTOR_RENAME(float_to_int32)(reinterpret_cast<float*>(in->audio), reinterpret_cast<int32_t*>(out->audio),in->len,out->flags&MP_AFLG_FINALIZED);
	    break;
    }
}

static void __FASTCALL__ PVECTOR_RENAME(int2float)(const mp_aframe_t* in, mp_aframe_t* out)
{
    unsigned i,bps=in->format&MPAF_BPS_MASK;
    switch(bps){
    case 1:
	for(i=0;i<in->len;i++)
	    ((float*)out->audio)[i]=(1.0/std::numeric_limits<int16_t>::max())*((float)((int8_t*)in->audio)[i]);
	break;
    case 2:
#ifdef HAVE_INT_PVECTOR
	{
	int32_t __attribute__((aligned(64))) itmp[in->len];
	PVECTOR_RENAME(int16_to_int32)(reinterpret_cast<int16_t*>(in->audio), itmp, in->len, 0);
	PVECTOR_RENAME(int32_to_float)(itmp, reinterpret_cast<float*>(out->audio),in->len,out->flags&MP_AFLG_FINALIZED);
	}
#else
	for(i=0;i<in->len;i++)
	    ((float*)out->audio)[i]=(1.0/std::numeric_limits<int16_t>::max())*((float)((int16_t*)in->audio)[i]);
#endif
	break;
    case 3:
	for(i=0;i<in->len;i++)
	    ((float*)out->audio)[i]=(1.0/std::numeric_limits<int32_t>::max())*((float)((int32_t)load24bit(in->audio, i)));
	break;
    case 4:
	PVECTOR_RENAME(int32_to_float)(reinterpret_cast<int32_t*>(in->audio),reinterpret_cast<float*>(out->audio),in->len,out->flags&MP_AFLG_FINALIZED);
	break;
    }
}

#if defined ( OPTIMIZE_SSE )
static inline __m128 PVECTOR_RENAME(_my_hadd_ps)( __m128 a, __m128 b) {
    __m128 tempA = _mm_shuffle_ps(a,b, _MM_SHUFFLE(2,0,2,0));
    __m128 tempB = _mm_shuffle_ps(a,b, _MM_SHUFFLE(3,1,3,1));
    return _mm_add_ps( tempB, tempA);
}
#endif
static float __FASTCALL__ PVECTOR_RENAME(FIR_f32)(const float *x,const float *w)
{
#if defined ( OPTIMIZE_SSE )
    __m128 xmm[4];
    float rval;
    xmm[0] = _mm_loadu_ps(&w[0]);
    xmm[1] = _mm_loadu_ps(&w[4]);
    xmm[2] = _mm_loadu_ps(&w[8]);
    xmm[3] = _mm_loadu_ps(&w[12]);
    xmm[0] = _mm_mul_ps(xmm[0],_mm_loadu_ps(&x[0]));
    xmm[1] = _mm_mul_ps(xmm[1],_mm_loadu_ps(&x[4]));
    xmm[2] = _mm_mul_ps(xmm[2],_mm_loadu_ps(&x[8]));
    xmm[3] = _mm_mul_ps(xmm[3],_mm_loadu_ps(&x[12]));

    xmm[0] = _mm_add_ps(xmm[0],xmm[1]);
    xmm[2] = _mm_add_ps(xmm[2],xmm[3]);
    xmm[0] = _mm_add_ps(xmm[0],xmm[2]);

#ifdef OPTIMIZE_SSE3
    xmm[0] = _mm_hadd_ps(xmm[0],xmm[0]);
    xmm[0] = _mm_hadd_ps(xmm[0],xmm[0]);
#else
    xmm[0] = PVECTOR_RENAME(_my_hadd_ps)(xmm[0],xmm[0]);
    xmm[0] = PVECTOR_RENAME(_my_hadd_ps)(xmm[0],xmm[0]);
#endif
    _mm_store_ss(&rval,xmm[0]);
    return rval;
#elif defined ( OPTIMIZE_3DNOW ) && !defined(OPTIMIZE_SSE)
    __m64 mm[8];
    union {
	unsigned ipart;
	float    fpart;
    }rval;
    mm[0] = _m_load(&w[0]);
    mm[1] = _m_load(&w[2]);
    mm[2] = _m_load(&w[4]);
    mm[3] = _m_load(&w[6]);
    mm[4] = _m_load(&w[8]);
    mm[5] = _m_load(&w[10]);
    mm[6] = _m_load(&w[12]);
    mm[7] = _m_load(&w[14]);
    mm[0] = _m_pfmul(mm[0],_m_load(&x[0]));
    mm[1] = _m_pfmul(mm[1],_m_load(&x[2]));
    mm[2] = _m_pfmul(mm[2],_m_load(&x[4]));
    mm[3] = _m_pfmul(mm[3],_m_load(&x[6]));
    mm[4] = _m_pfmul(mm[4],_m_load(&x[8]));
    mm[5] = _m_pfmul(mm[5],_m_load(&x[10]));
    mm[6] = _m_pfmul(mm[6],_m_load(&x[12]));
    mm[7] = _m_pfmul(mm[7],_m_load(&x[14]));
    mm[0] = _m_pfadd(mm[0],mm[1]);
    mm[2] = _m_pfadd(mm[2],mm[3]);
    mm[4] = _m_pfadd(mm[4],mm[5]);
    mm[6] = _m_pfadd(mm[6],mm[7]);
    mm[0] = _m_pfadd(mm[0],mm[2]);
    mm[4] = _m_pfadd(mm[4],mm[6]);
    mm[0] = _m_pfadd(mm[0],mm[4]);
    mm[0] = _m_pfacc(mm[0],mm[0]);
    _m_store_half(&rval.ipart,mm[0]);
    _ivec_empty();
    return rval.fpart;
#else
  return ( w[0] *x[0] +w[1] *x[1] +w[2] *x[2] +w[3] *x[3]
	 + w[4] *x[4] +w[5] *x[5] +w[6] *x[6] +w[7] *x[7]
	 + w[8] *x[8] +w[9] *x[9] +w[10]*x[10]+w[11]*x[11]
	 + w[12]*x[12]+w[13]*x[13]+w[14]*x[14]+w[15]*x[15] );
#endif
}
