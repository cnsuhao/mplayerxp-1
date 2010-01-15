#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void *my_malloc(size_t __size)
{
  char *retval;
  long msize,mval;
  msize = __size;
  retval = malloc(msize+2*sizeof(long));
  if(retval) 
  {
    mval = (long)retval;
    memcpy(retval,&msize,sizeof(long));
    memcpy(retval+msize+sizeof(long),&mval,sizeof(long));
    retval += sizeof(long);
  }
//  printf("malloc returns: %08X for size: %08X\n",retval,__size);
  return retval;
}

void *my_realloc(void *__ptr, size_t __size)
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
  retval = realloc(myptr,__size+2*sizeof(long));
  {
    mval = (long)retval;
    memcpy(retval,&msize,sizeof(long));
    memcpy(retval+__size+sizeof(long),&mval,sizeof(long));
    retval += sizeof(long);
  }
  return retval;
}

void *my_calloc (size_t __nelem, size_t __size)
{
  char *retval;
  long my_size;
  long msize,mval;
  msize = __nelem*__size;
  retval = malloc(msize+2*sizeof(long));
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

void  my_free(void *__ptr)
{
  void *myptr;
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
    free((char *)__ptr-sizeof(long));
  }
}

char * my_strdup(const char *s)
{
  void *a;
  a = my_malloc(strlen(s)+1);
  strcpy(a,s); 
  return a;
}
