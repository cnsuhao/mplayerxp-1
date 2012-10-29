
// a52_resample_init should find the requested converter (from type flags ->
// given number of channels) and set up some function pointers...

// a52_resample() should do the conversion.

#include "mp_config.h"
#include <inttypes.h>
#include <stdio.h>
#include "a52.h"
#include "a52_internal.h"
#include "osdep/mm_accel.h"
#include "osdep/mangle.h"
#include "osdep/cpudetect.h"

int (* a52_resample) (float * _f, int16_t * s16)=NULL;
int (* a52_resample32) (float * _f, float * s16)=NULL;

#include "resample_c.c"

#ifdef CAN_COMPILE_MMX
#include "resample_mmx.c"
#endif

any_t* a52_resample_init(a52_state_t * state,uint32_t mm_accel,int flags,int chans){
any_t* tmp;

#ifdef CAN_COMPILE_MMX
    if(mm_accel&MM_ACCEL_X86_MMX){
	tmp=a52_resample_MMX(state,flags,chans);
	if(tmp){
	    if(a52_resample==NULL) fprintf(stderr, "Using MMX optimized resampler\n");
	    a52_resample=tmp;
	    return tmp;
	}
    }
#endif

    tmp=a52_resample_C(state,flags,chans);
    if(tmp){
	if(a52_resample==NULL) fprintf(stderr, "No accelerated resampler found\n");
	a52_resample=tmp;
	return tmp;
    }

    fprintf(stderr, "Unimplemented resampler for mode 0x%X -> %d channels conversion - Contact MPlayer developers!\n", flags, chans);
    return NULL;
}

any_t* a52_resample_init_float(a52_state_t * state,uint32_t mm_accel,int flags,int chans){
any_t* tmp;

#if 0 //#if defined( ARCH_X86 ) || defined(ARCH_X86_64)
    if(mm_accel&MM_ACCEL_X86_MMX){
	tmp=a52_resample_MMX(state,flags,chans);
	if(tmp){
	    if(a52_resample==NULL) fprintf(stderr, "Using MMX optimized resampler\n");
	    a52_resample=tmp;
	    return tmp;
	}
    }
#endif

    tmp=a52_resample_f32(state,flags,chans);
    if(tmp){
	if(a52_resample_f32==NULL) fprintf(stderr, "No accelerated resampler found\n");
	a52_resample32=tmp;
	return tmp;
    }

    fprintf(stderr, "Unimplemented resampler for mode 0x%X -> %d channels conversion - Contact MPlayer developers!\n", flags, chans);
    return NULL;
}
