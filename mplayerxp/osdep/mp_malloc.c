#include "mp_config.h"
#include "mplib.h"
#define MSGT_CLASS MSGT_OSDEP
#include "__mp_msg.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <time.h>

#ifdef ENABLE_DEBUG_MALLOC
any_t*my_malloc(size_t __size)
{
  char *retval;
  long msize,mval;
  msize = __size;
  retval = mp_malloc(msize+2*sizeof(long));
  if(retval) 
  {
    mval = (long)retval;
    memcpy(retval,&msize,sizeof(long));
    memcpy(retval+msize+sizeof(long),&mval,sizeof(long));
    retval += sizeof(long);
  }
//  printf("mp_malloc returns: %08X for size: %08X\n",retval,__size);
  return retval;
}

any_t*my_realloc(any_t*__ptr, size_t __size)
{
  char *retval,*myptr;
  long crc,osize;
  long msize,mval;
  msize = __size;
  myptr = __ptr;
  if(__ptr)
  {
    memcpy(&osize,(char *)__ptr-sizeof(long),sizeof(long));
    memcpy(&crc,(char *)__ptr+osize,sizeof(long));
    if(crc != (long)((char *)__ptr-sizeof(long)))
    {
      printf("Internal error: my_realloc found out memory corruption!\n");
      printf("INFO: ptr=%p ptr[0]=%lX crc=%lX\n",
		__ptr,
		osize,
		crc);
#ifdef __i386__
	__asm __volatile(".short 0xffff":::"memory");
#endif
    }
    myptr -= sizeof(long);
  }
  retval = mp_realloc(myptr,__size+2*sizeof(long));
  {
    mval = (long)retval;
    memcpy(retval,&msize,sizeof(long));
    memcpy(retval+__size+sizeof(long),&mval,sizeof(long));
    retval += sizeof(long);
  }
  return retval;
}

any_t*my_calloc (size_t __nelem, size_t __size)
{
  char *retval;
  long my_size;
  long msize,mval;
  msize = __nelem*__size;
  retval = mp_malloc(msize+2*sizeof(long));
  if(retval) 
  {
    mval = (long)retval;
    memset(retval+sizeof(long),0,msize);
    memcpy(retval,&msize,sizeof(long));
    memcpy(retval+msize+sizeof(long),&mval,sizeof(long));
    retval += sizeof(long);
  }
  return retval;
}

void  my_free(any_t*__ptr)
{
  any_t*myptr;
  long crc,osize;
  if(__ptr)
  {
    myptr = (char *)__ptr-sizeof(long);
    memcpy(&osize,(char *)myptr,sizeof(long));
    memcpy(&crc,(char *)myptr+osize+sizeof(long),sizeof(long));
    if(crc != (long)myptr)
    {
	printf("Internal error: my_free found out memory corruption!\n");
	printf("INFO: ptr=%p ptr[0]=%lX crc=%lX\n",
		__ptr,
		osize,
		crc);
#ifdef __i386__
	__asm __volatile(".short 0xffff":::"memory");
#endif
    }
    mp_free((char *)__ptr-sizeof(long));
  }
}

char * my_strdup(const char *s)
{
  any_t*a;
  a = my_malloc(strlen(s)+1);
  strcpy(a,s);
  return a;
}
#endif

typedef struct priv_s {
    unsigned			rnd_limit;
    unsigned			every_nth_call;
    // statistics
    unsigned long long int	total_calls;
    unsigned long long int	num_allocs;
    // local statistics
    int				enable_stat;
    unsigned long long int	stat_total_calls;
    unsigned long long int	stat_num_allocs;
}priv_t;
static priv_t* priv;

void	mp_init_malloc(unsigned rnd_limit,unsigned every_nth_call)
{
    if(!priv) priv=malloc(sizeof(priv_t));
    memset(priv,0,sizeof(priv_t));
    priv->rnd_limit=rnd_limit;
    priv->every_nth_call=every_nth_call;
}

void	mp_uninit_malloc(int verbose)
{
    if(priv->num_allocs && verbose)
	MSG_WARN("Warning! From %lli total calls of alloc() were not freed %lli buffers\n",priv->total_calls,priv->num_allocs);
    free(priv);
    priv=NULL;
}

any_t* mp_malloc(size_t __size)
{
    any_t* rb,*rnd_buff=NULL;
    if(!priv) mp_init_malloc(1000,10);
    if(priv->every_nth_call && priv->rnd_limit) {
	if(priv->total_calls%priv->every_nth_call==0) {
	    rnd_buff=malloc(rand()%priv->rnd_limit);
	}
    }
    rb = malloc(__size);
    if(rnd_buff) free(rnd_buff);
    priv->total_calls++;
    priv->num_allocs++;
    if(priv->enable_stat) {
	priv->stat_total_calls++;
	priv->stat_num_allocs++;
    }
    return rb;
}

any_t*	mp_realloc(any_t*__ptr, size_t __size) { return realloc(__ptr,__size); }
any_t*	mp_calloc (size_t __nelem, size_t __size) { return mp_mallocz(__nelem*__size); }

any_t*	mp_mallocz (size_t __size) {
    any_t* rp;
    rp=mp_malloc(__size);
    if(rp) memset(rp,0,__size);
    return rp;
}
/* randomizing of memalign is useless feature */
any_t*	mp_memalign (size_t boundary, size_t __size)
{
    if(!priv) mp_init_malloc(1000,10);
    priv->num_allocs++;
    if(priv->enable_stat) priv->stat_num_allocs++;
    return memalign(boundary,__size);
}

void	mp_free(any_t*__ptr)
{
    if(!priv) mp_init_malloc(1000,10);
    free(__ptr);
    priv->num_allocs--;
    if(priv->enable_stat) priv->stat_num_allocs--;
}

char *	mp_strdup(const char *src) {
    char *rs=NULL;
    if(src) {
	unsigned len=strlen(src);
	rs=mp_malloc(len+1);
	strcpy(rs,src);
    }
    return rs;
}

void	__FASTCALL__ mp_open_malloc_stat(void) {
    if(!priv) mp_init_malloc(1000,10);
    priv->enable_stat=1;
    priv->stat_total_calls=priv->stat_num_allocs=0ULL;
}

unsigned long long __FASTCALL__ mp_close_malloc_stat(int verbose) {
    if(!priv) mp_init_malloc(1000,10);
    priv->enable_stat=0;
    if(verbose)
	MSG_INFO("mp_malloc stat: from %lli total calls of alloc() were not freed %lli buffers\n"
	,priv->stat_total_calls
	,priv->stat_num_allocs);
    return priv->stat_num_allocs;
}
