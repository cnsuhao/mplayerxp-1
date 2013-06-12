/*
  aclib - advanced C library ;)
  This file contains functions which improve and expand standard C-library
*/
#include "pvector/pvector.h"

#ifdef HAVE_INT_PVECTOR
/* for small memory blocks (<256 bytes) this version is faster */
#undef small_memcpy
#ifdef __x86_64__
#define small_memcpy(to,from,n)\
{\
unsigned long int siz;\
unsigned long int dummy;\
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
#else
#define small_memcpy(to,from,n)\
{\
unsigned long int dummy;\
__asm__ __volatile__(\
	"rep; movsb"\
	:"=&D"(to), "=&S"(from), "=&c"(dummy)\
/* It's most portable way to notify compiler */\
/* that edi, esi and ecx are clobbered in asm block. */\
/* Thanks to A'rpi for hint!!! */\
	:"0" (to), "1" (from),"2" (n)\
	: "memory","cc");\
}
#endif

#undef MIN_LEN
#if defined( OPTIMIZE_MMX ) && !defined( OPTIMIZE_MMX2 )
#define MIN_LEN 0x800  /* 2K blocks. Was found experimentally */
#else
#define MIN_LEN _ivec_size()*8
#endif

#undef FAST_MEMORY_COPY
#define FAST_MEMORY_COPY(to,from, len)\
{\
    any_t*retval;\
    const unsigned char *cfrom=(const unsigned char *)from;\
    unsigned char *tto=(unsigned char *)to;\
    const unsigned block_size = _ivec_size()*8;\
    __ivec iarr[8];\
    size_t i;\
    retval = to;\
    if(!len) return retval;\
\
    _ivec_prefetch(&cfrom[0]);\
    _ivec_prefetch(&cfrom[32]);\
    _ivec_prefetch(&cfrom[64]);\
    _ivec_prefetch(&cfrom[96]);\
    _ivec_prefetch(&cfrom[128]);\
    _ivec_prefetch(&cfrom[160]);\
    _ivec_prefetch(&cfrom[192]);\
    _ivec_prefetch(&cfrom[224]);\
    _ivec_prefetch(&cfrom[256]);\
\
    if(len >= MIN_LEN)\
    {\
	unsigned long int delta;\
	/* Align destinition to cache-line size -boundary */\
	delta = ((unsigned long int)tto)&(gCpuCaps.cl_size-1);\
	if(delta) {\
	    delta=gCpuCaps.cl_size-delta;\
	    len -= delta;\
	    small_memcpy(tto, cfrom, delta);\
	}\
	i = len/block_size;\
	len&=(block_size-1);\
	/*\
	   This algorithm is top effective when the code consequently\
	   reads and writes blocks which have size of cache line.\
	   Size of cache line is processor-dependent.\
	   It will, however, be a minimum of 32 bytes on any processors.\
	   It would be better to have a number of instructions which\
	   perform reading and writing to be multiple to a number of\
	   processor's decoders, but it's not always possible.\
	*/\
	if(((unsigned long)cfrom) & 15)\
	/* if SRC is misaligned */\
	for(; i>0; i--)\
	{\
	    _ivec_prefetch(&cfrom[_ivec_size()*8]);\
	    _ivec_prefetch(&cfrom[_ivec_size()*8+32]);\
	    iarr[0] = _ivec_loadu(&cfrom[_ivec_size()*0]);\
	    iarr[1] = _ivec_loadu(&cfrom[_ivec_size()*1]);\
	    iarr[2] = _ivec_loadu(&cfrom[_ivec_size()*2]);\
	    iarr[3] = _ivec_loadu(&cfrom[_ivec_size()*3]);\
	    iarr[4] = _ivec_loadu(&cfrom[_ivec_size()*4]);\
	    iarr[5] = _ivec_loadu(&cfrom[_ivec_size()*5]);\
	    iarr[6] = _ivec_loadu(&cfrom[_ivec_size()*6]);\
	    iarr[7] = _ivec_loadu(&cfrom[_ivec_size()*7]);\
	    MEM_STORE(&tto[_ivec_size()*0],iarr[0]);\
	    MEM_STORE(&tto[_ivec_size()*1],iarr[1]);\
	    MEM_STORE(&tto[_ivec_size()*2],iarr[2]);\
	    MEM_STORE(&tto[_ivec_size()*3],iarr[3]);\
	    MEM_STORE(&tto[_ivec_size()*4],iarr[4]);\
	    MEM_STORE(&tto[_ivec_size()*5],iarr[5]);\
	    MEM_STORE(&tto[_ivec_size()*6],iarr[6]);\
	    MEM_STORE(&tto[_ivec_size()*7],iarr[7]);\
	    cfrom+=block_size;\
	    tto+=block_size;\
	}\
	else\
	/* if SRC is aligned */\
	for(; i>0; i--)\
	{\
	    _ivec_prefetch(&cfrom[_ivec_size()*8]);\
	    _ivec_prefetch(&cfrom[_ivec_size()*8+32]);\
	    iarr[0] = _ivec_loada(&cfrom[_ivec_size()*0]);\
	    iarr[1] = _ivec_loada(&cfrom[_ivec_size()*1]);\
	    iarr[2] = _ivec_loada(&cfrom[_ivec_size()*2]);\
	    iarr[3] = _ivec_loada(&cfrom[_ivec_size()*3]);\
	    iarr[4] = _ivec_loada(&cfrom[_ivec_size()*4]);\
	    iarr[5] = _ivec_loada(&cfrom[_ivec_size()*5]);\
	    iarr[6] = _ivec_loada(&cfrom[_ivec_size()*6]);\
	    iarr[7] = _ivec_loada(&cfrom[_ivec_size()*7]);\
	    MEM_STORE(&tto[_ivec_size()*0],iarr[0]);\
	    MEM_STORE(&tto[_ivec_size()*1],iarr[1]);\
	    MEM_STORE(&tto[_ivec_size()*2],iarr[2]);\
	    MEM_STORE(&tto[_ivec_size()*3],iarr[3]);\
	    MEM_STORE(&tto[_ivec_size()*4],iarr[4]);\
	    MEM_STORE(&tto[_ivec_size()*5],iarr[5]);\
	    MEM_STORE(&tto[_ivec_size()*6],iarr[6]);\
	    MEM_STORE(&tto[_ivec_size()*7],iarr[7]);\
	    cfrom+=block_size;\
	    tto+=block_size;\
	}\
	MEM_SFENCE\
	_ivec_empty();\
    }\
    /*\
     *	Now do the tail of the block\
     */\
    if(len) small_memcpy(tto, cfrom, len);\
    return retval;\
}

namespace	usr {
#undef MEM_STORE
#undef MEM_SFENCE
#define MEM_STORE  _ivec_stream
#define MEM_SFENCE _ivec_sfence();
static inline any_t* PVECTOR_RENAME(fast_stream_copy)(any_t* to, const any_t* from, size_t len)
{
    mpxp_dbg3<<"fast_stream_copy("<<to<<", "<<from<<", "<<len<<") [cl_size="<<gCpuCaps.cl_size<<"]"<<std::endl;
    FAST_MEMORY_COPY(to,from,len);
}

#undef MEM_STORE
#undef MEM_SFENCE
#define MEM_STORE _ivec_storea
#define MEM_SFENCE
static inline any_t* PVECTOR_RENAME(fast_memcpy)(any_t* to, const any_t* from, size_t len)
{
    mpxp_dbg3<<"fast_memcpy("<<to<<", "<<from<<", "<<len<<") [cl_size="<<gCpuCaps.cl_size<<"]"<<std::endl;
    FAST_MEMORY_COPY(to,from,len);
}
} // namesapce mpxp
#endif
