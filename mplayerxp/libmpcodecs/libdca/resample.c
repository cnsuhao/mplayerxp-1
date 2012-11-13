
// dca_resample_init should find the requested converter (from type flags ->
// given number of channels) and set up some function pointers...

// dca_resample() should do the conversion.

#include <inttypes.h>
#include <stdio.h>
#include "dca.h"
#include "dca_internal.h"
#include "mp_config.h"
#include "osdep/mm_accel.h"
#include "osdep/mangle.h"
#include "osdep/cpudetect.h"

int (* dca_resample) (float * _f, int16_t * s16)=NULL;
int (* dca_resample32) (float * _f, float * s16)=NULL;

#include "resample_c.c"

#ifdef CAN_COMPILE_MMX
#include "resample_mmx.c"
#endif

any_t* dca_resample_init(dca_state_t * state,uint32_t _mm_accel,int flags,int chans){
any_t* tmp;

#ifdef CAN_COMPILE_MMX
    if(_mm_accel&MM_ACCEL_X86_MMX){
	tmp=dca_resample_MMX(state,flags,chans);
	if(tmp){
	    if(dca_resample==NULL) fprintf(stderr, "Using MMX optimized resampler\n");
	    dca_resample=tmp;
	    return tmp;
	}
    }
#endif

    tmp=dca_resample_C(state,flags,chans);
    if(tmp){
	if(dca_resample==NULL) fprintf(stderr, "No accelerated resampler found\n");
	dca_resample=tmp;
	return tmp;
    }

    fprintf(stderr, "Unimplemented resampler for mode 0x%X -> %d channels conversion - Contact MPlayer developers!\n", flags, chans);
    return NULL;
}

any_t* dca_resample_init_float(dca_state_t * state,uint32_t _mm_accel,int flags,int chans){
any_t* tmp;

#if 0 //#if defined( ARCH_X86 ) || defined(ARCH_X86_64)
    if(_mm_accel&MM_ACCEL_X86_MMX){
	tmp=dca_resample_MMX(state,flags,chans);
	if(tmp){
	    if(dca_resample==NULL) fprintf(stderr, "Using MMX optimized resampler\n");
	    dca_resample=tmp;
	    return tmp;
	}
    }
#endif

    tmp=dca_resample_f32(state,flags,chans);
    if(tmp){
	if(dca_resample_f32==NULL) fprintf(stderr, "No accelerated resampler found\n");
	dca_resample32=tmp;
	return tmp;
    }

    fprintf(stderr, "Unimplemented resampler for mode 0x%X -> %d channels conversion - Contact MPlayer developers!\n", flags, chans);
    return NULL;
}
