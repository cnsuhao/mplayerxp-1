/*
  aclib - advanced C library ;)
  This file contains functions which improve and expand standard C-library
*/

/* for small memory blocks (<256 bytes) this version is faster */
#define small_memcpy(to,from,n)\
{\
register unsigned long int siz;\
register unsigned long int dummy;\
    siz=n&0x7;  n>>=3;\
    if(siz)\
__asm__ __volatile__(\
	"rep; movsb"\
	:"=&D"(to), "=&S"(from), "=&c"(dummy)\
/* It's most portable way to notify compiler */\
/* that edi, esi and ecx are clobbered in asm block. */\
/* Thanks to A'rpi for hint!!! */\
        :"0" (to), "1" (from),"2" (siz)\
	: "memory","cc");\
    if(n)\
__asm__ __volatile__(\
	"rep; movsq"\
	:"=&D"(to), "=&S"(from), "=&c"(dummy)\
/* It's most portable way to notify compiler */\
/* that edi, esi and ecx are clobbered in asm block. */\
/* Thanks to A'rpi for hint!!! */\
        :"0" (to), "1" (from),"2" (n)\
	: "memory","cc");\
}


#define MMREG_SIZE 16ULL
#define MIN_LEN 257ULL
#define CL_SIZE 256ULL /*always align on 256 byte boundary */

static inline void * RENAME(fast_memcpy)(void * to, const void * from, size_t len)
{
	void *retval;
	const unsigned char *cfrom=from;
	unsigned char *tto=to;
	size_t i=0;
	retval = to;
	if(!len) return retval;
        /* PREFETCH has effect even for MOVSB instruction ;) */
	__asm__ __volatile__ (
		"prefetcht0 (%0)\n"
		"prefetcht0 64(%0)\n"
		"prefetcht0 128(%0)\n"
		"prefetcht0 192(%0)\n"
		:: "r" (cfrom));
        if(len >= MIN_LEN)
	{
	  register unsigned long int delta;
          /* Align destinition to cache-line size -boundary */
          delta = ((unsigned long int)tto)&(CL_SIZE-1ULL);
          if(delta)
	  {
	    delta=CL_SIZE-delta;
	    len -=delta;
	    small_memcpy(tto, cfrom, delta);
	  }
	  i = len>>8; /* len/256 */
	  len=len-(i<<8);
	}
	if(i) {
        /*
           This algorithm is top effective when the code consequently
           reads and writes blocks which have size of cache line.
           Size of cache line is processor-dependent.
           It will, however, be a minimum of 32 bytes on any processors.
           It would be better to have a number of instructions which
           perform reading and writing to be multiple to a number of
           processor's decoders, but it's not always possible.
        */
	if(((unsigned long)cfrom) & 15)
	/* if SRC is misaligned */
	for(; i>0; i--)
	{
		__asm__ __volatile__ (
		"prefetcht0 256(%0)\n"
		"prefetcht0 320(%0)\n"
		"movdqu (%0), %%xmm0\n"
		"movdqu 16(%0), %%xmm1\n"
		"movdqu 32(%0), %%xmm2\n"
		"movdqu 48(%0), %%xmm3\n"
		"movdqu 64(%0), %%xmm4\n"
		"movdqu 80(%0), %%xmm5\n"
		"movdqu 96(%0), %%xmm6\n"
		"movdqu 112(%0), %%xmm7\n"
		"prefetcht0 384(%0)\n"
		"prefetcht0 448(%0)\n"
		"movdqu 128(%0), %%xmm8\n"
		"movdqu 144(%0), %%xmm9\n"
		"movdqu 160(%0), %%xmm10\n"
		"movdqu 176(%0), %%xmm11\n"
		"movdqu 192(%0), %%xmm12\n"
		"movdqu 208(%0), %%xmm13\n"
		"movdqu 224(%0), %%xmm14\n"
		"movdqu 240(%0), %%xmm15\n"
		"movntdq %%xmm0, (%1)\n"
		"movntdq %%xmm1, 16(%1)\n"
		"movntdq %%xmm2, 32(%1)\n"
		"movntdq %%xmm3, 48(%1)\n"
		"movntdq %%xmm4, 64(%1)\n"
		"movntdq %%xmm5, 80(%1)\n"
		"movntdq %%xmm6, 96(%1)\n"
		"movntdq %%xmm7, 112(%1)\n"
		"movntdq %%xmm8, 128(%1)\n"
		"movntdq %%xmm9, 144(%1)\n"
		"movntdq %%xmm10, 160(%1)\n"
		"movntdq %%xmm11, 176(%1)\n"
		"movntdq %%xmm12, 192(%1)\n"
		"movntdq %%xmm13, 208(%1)\n"
		"movntdq %%xmm14, 224(%1)\n"
		"movntdq %%xmm15, 240(%1)\n"
		:: "r" (cfrom), "r" (tto):
		"memory"
		,"xmm0","xmm1","xmm2","xmm3","xmm4","xmm5","xmm6","xmm7","xmm8","xmm9","xmm10","xmm11","xmm12","xmm13","xmm14","xmm15"
		);
		cfrom+=256ULL;
		tto+=256ULL;
	}
	else
	/*
	   Only if SRC is aligned on 16-byte boundary.
	   It allows to use movdqa instead of movdqu, which required data
	   to be aligned or a general-protection exception (#GP) is generated.
	*/
	for(; i>0; i--)
	{
		__asm__ __volatile__ (
		"prefetcht0 256(%0)\n"
		"prefetcht0 320(%0)\n"
		"movdqa (%0), %%xmm0\n"
		"movdqa 16(%0), %%xmm1\n"
		"movdqa 32(%0), %%xmm2\n"
		"movdqa 48(%0), %%xmm3\n"
		"movdqa 64(%0), %%xmm4\n"
		"movdqa 80(%0), %%xmm5\n"
		"movdqa 96(%0), %%xmm6\n"
		"movdqa 112(%0), %%xmm7\n"
		"prefetcht0 384(%0)\n"
		"prefetcht0 448(%0)\n"
		"movdqa 128(%0), %%xmm8\n"
		"movdqa 144(%0), %%xmm9\n"
		"movdqa 160(%0), %%xmm10\n"
		"movdqa 176(%0), %%xmm11\n"
		"movdqa 192(%0), %%xmm12\n"
		"movdqa 208(%0), %%xmm13\n"
		"movdqa 224(%0), %%xmm14\n"
		"movdqa 240(%0), %%xmm15\n"
		"movntdq %%xmm0, (%1)\n"
		"movntdq %%xmm1, 16(%1)\n"
		"movntdq %%xmm2, 32(%1)\n"
		"movntdq %%xmm3, 48(%1)\n"
		"movntdq %%xmm4, 64(%1)\n"
		"movntdq %%xmm5, 80(%1)\n"
		"movntdq %%xmm6, 96(%1)\n"
		"movntdq %%xmm7, 112(%1)\n"
		"movntdq %%xmm8, 128(%1)\n"
		"movntdq %%xmm9, 144(%1)\n"
		"movntdq %%xmm10, 160(%1)\n"
		"movntdq %%xmm11, 176(%1)\n"
		"movntdq %%xmm12, 192(%1)\n"
		"movntdq %%xmm13, 208(%1)\n"
		"movntdq %%xmm14, 224(%1)\n"
		"movntdq %%xmm15, 240(%1)\n"
		:: "r" (cfrom), "r" (tto):
		"memory"
		,"xmm0","xmm1","xmm2","xmm3","xmm4","xmm5","xmm6","xmm7","xmm8","xmm9","xmm10","xmm11","xmm12","xmm13","xmm14","xmm15"
		);
		cfrom+=256ULL;
		tto+=256ULL;
	  }
	__asm__ __volatile__ ("sfence":::"memory");
	}
	/*
	 *	Now do the tail of the block
	 */
	if(len) small_memcpy(tto, cfrom, len);
	return retval;
}

/**
 * special copy routine for mem -> agp/pci copy (based upon fast_memcpy)
 */
static inline void * RENAME(mem2agpcpy)(void * to, const void * from, size_t len)
{
    return memcpy(to,from,len);
}
