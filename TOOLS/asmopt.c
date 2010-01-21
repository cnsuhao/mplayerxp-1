/*
   fastmemcpybench.c used to benchmark fastmemcpy.h code from libvo.
     
   Note: this code can not be used on PentMMX-PII because they contain
   a bug in rdtsc. For Intel processors since P6(PII) rdpmc should be used
   instead. For PIII it's disputable and seems bug was fixed but I don't
   tested it.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <inttypes.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/time.h>

#include "../mplayerxp/mp_config.h"
#include "../mplayerxp/cpudetect.h"

#define PVECTOR_TESTING
#define PVECTOR_ACCEL_H "asmopt_template.h"
#include "../mplayerxp/pvector/pvector_inc.h"

#define ARR_SIZE (1024*64*2)*10
unsigned verbose=1;
extern CpuCaps gCpuCaps;

/* Fixme: put here any complexness of source array filling */
#define INIT_ARRAYS(x) \
{\
	for(i=0; i<x; i++) srca[i] = i; \
	for(i=0; i<x; i++) src[i] = i+64; \
	for(i=0; i<x; i++) dsta[i] = i+128; \
}

// Returns current time in microseconds
unsigned int GetTimer(){
  struct timeval tv;
  struct timezone tz;
//  float s;
  gettimeofday(&tv,&tz);
//  s=tv.tv_usec;s*=0.000001;s+=tv.tv_sec;
  return (tv.tv_sec*1000000+tv.tv_usec);
}  

static inline unsigned long long int read_tsc( void )
{
  unsigned long long int retval;
  __asm __volatile ("rdtsc":"=A"(retval)::"memory");
  return retval;
}

unsigned char  __attribute__((aligned(4096)))src[ARR_SIZE],srca[ARR_SIZE],dsta[ARR_SIZE];

int cmp_result(const char *name)
{
    FILE *in1,*in2;
    unsigned char  __attribute__((aligned(4096)))src1[ARR_SIZE],src2[ARR_SIZE];
    if((in1=fopen("asmopt.gen","rb"))==NULL) return 0;
    if((in2=fopen(name,"rb"))==NULL) { fclose(in1); return 0; }
    fread(src1,ARR_SIZE,1,in1);
    fread(src2,ARR_SIZE,1,in2);
    fclose(in1);
    fclose(in2);
    return ((memcmp(src1,src2,ARR_SIZE)==0)?1:0);
}

typedef void (*mmx_test_f)(unsigned char *d,unsigned char *s1, unsigned char *s2,unsigned n);

void test_simd(const char *fname,const char *pfx,mmx_test_f func)
{
	unsigned long long int v1,v2;
	unsigned int t;
	unsigned i;
	FILE *out=fopen(fname,"wb");
	fprintf(stderr,"%s ",pfx);
	INIT_ARRAYS(ARR_SIZE);
	t=GetTimer();
	v1 = read_tsc();
	(*func)(dsta,src,srca,ARR_SIZE);
	v2 = read_tsc();
	t=GetTimer()-t;
	fprintf(stderr,"cpu clocks=%llu = %dus...",v2-v1,t);
	if(out) fwrite(dsta,ARR_SIZE,1,out);
	fclose(out);
	fprintf(stderr,"[%s]\n",cmp_result(fname)?"OK":"FAIL");
}

int main( void )
{
  GetCpuCaps(&gCpuCaps);
  fprintf(stderr,"CPUflags: MMX: %d MMX2: %d 3DNow: %d 3DNow2: %d SSE: %d SSE2: %d\n",
      gCpuCaps.hasMMX,gCpuCaps.hasMMX2,
      gCpuCaps.has3DNow, gCpuCaps.has3DNowExt,
      gCpuCaps.hasSSE, gCpuCaps.hasSSE2);

			 test_simd("asmopt.gen" ,"GENERIC:",convert_c);
// ordered per speed fasterst first
#ifdef __SSE3__
    if(gCpuCaps.hasSSE3) test_simd("asmopt.sse3","SSE3   :",convert_SSE3);
#endif
#ifdef __SSE2__
    if(gCpuCaps.hasSSE2) test_simd("asmopt.sse2","SSE2   :",convert_SSE2);
#endif
#ifdef __SSE__
    if(gCpuCaps.hasMMX2) test_simd("asmopt.mmx2","MMX2   :",convert_SSE);
#endif
#ifdef __MMX__
    if(gCpuCaps.hasMMX)  test_simd("asmopt.mmx", "MMX    :",convert_MMX);
#endif
  return 0;
}
