/*
 * Mpeg Layer-1,2,3 audio decoder
 * ------------------------------
 * copyright (c) 1995,1996,1997 by Michael Hipp, All rights reserved.
 * See also 'README'
 *
 * slighlty optimized for machines without autoincrement/decrement.
 * The performance is highly compiler dependend. Maybe
 * the decode.c version for 'normal' processor may be faster
 * even for Intel processors.
 */

#include "../config.h"
#include "mpg123.h"

/* new WRITE_SAMPLE */

/*
 * should be the same as the "old WRITE_SAMPLE" macro above, but uses
 * some tricks to avoid double->int conversions and floating point compares.
 *
 * Here's how it works:
 * ((((65536.0 * 65536.0 * 16)+(65536.0 * 0.5))* 65536.0)) is
 * 0x0010000080000000LL in hex.  It computes 0x0010000080000000LL + sum
 * as a double IEEE fp value and extracts the low-order 32-bits from the
 * IEEE fp representation stored in memory.  The 2^56 bit in the constant
 * is intended to force the bits of "sum" into the least significant bits
 * of the double mantissa.  After an integer substraction of 0x80000000
 * we have the original double value "sum" converted to an 32-bit int value.
 *
 * (Is that really faster than the clean and simple old version of the macro?)
 */

/*
 * On a SPARC cpu, we fetch the low-order 32-bit from the second 32-bit
 * word of the double fp value stored in memory.  On an x86 cpu, we fetch it
 * from the first 32-bit word.
 * I'm not sure if the WORDS_BIGENDIAN feature test covers all possible memory
 * layouts of double floating point values an all cpu architectures.  If
 * it doesn't work for you, just enable the "old WRITE_SAMPLE" macro.
 */
#if WORDS_BIGENDIAN
#define	MANTISSA_OFFSET	1
#else
#define	MANTISSA_OFFSET	0
#endif

#define WRITE_SAMPLE_F(samples,sum) { *(samples) = sum; }

int synth_1to1_mono_32(real *bandPtr,unsigned char *samples,int *pnt)
{
  float samples_tmp[64];
  float *tmp1 = samples_tmp;
  int i,ret;
  int pnt1 = 0;

  ret = synth_1to1_32(bandPtr,0,(unsigned char *) samples_tmp,&pnt1);
  samples += *pnt;

  for(i=0;i<32;i++) {
    *( (float *) samples) = *tmp1;
    samples += 2;
    tmp1 += 2;
  }
  *pnt += 128;

  return ret;
}

int synth_1to1_mono2stereo_32(real *bandPtr,unsigned char *samples,int *pnt)
{
  int i,ret;

  ret = synth_1to1_32(bandPtr,0,samples,pnt);
  samples = samples + *pnt - 256;

  for(i=0;i<32;i++) {
    ((float *)samples)[1] = ((float *)samples)[0];
    samples+=8;
  }

  return ret;
}

int synth_1to1_l_32(real *bandPtr,int channel,unsigned char *out,int *pnt)
{
  int i,ret;

  ret = synth_1to1_32(bandPtr,channel,out,pnt);
  out = out + *pnt - 256;

  for(i=0;i<32;i++) {
    ((float *)out)[1] = ((float *)out)[0];
    out+=8;
  }

  return ret;
}

int synth_1to1_r_32(real *bandPtr,int channel,unsigned char *out,int *pnt)
{
  int i,ret;

  ret = synth_1to1_32(bandPtr,channel,out,pnt);
  out = out + *pnt - 256;

  for(i=0;i<32;i++) {
    ((float *)out)[0] = ((float *)out)[1];
    out+=8;
  }

  return ret;
}

int synth_1to1_32(real *bandPtr,int channel,unsigned char *out,int *pnt)
{
  static real buffs[2][2][0x110];
  static const int step = 2;
  static int bo = 1;
  float *samples = (float *) (out + *pnt);

  real *b0,(*buf)[0x110];
  int clip = 0;
  int bo1;
  if(!channel) {     /* channel=0 */
    bo--;
    bo &= 0xf;
    buf = buffs[0];
  }
  else {
    samples++;
    buf = buffs[1];
  }

  if(bo & 0x1) {
    b0 = buf[0];
    bo1 = bo;
    fr.dct64(buf[1]+((bo+1)&0xf),buf[0]+bo,bandPtr);
  }
  else {
    b0 = buf[1];
    bo1 = bo+1;
    fr.dct64(buf[0]+bo,buf[1]+bo+1,bandPtr);
  }

  {
    register int j;
    real *window = decwin + 16 - bo1;

    for (j=16;j;j--,b0+=0x10,window+=0x20,samples+=step)
    {
      real sum;
      sum  = window[0x0] * b0[0x0];
      sum -= window[0x1] * b0[0x1];
      sum += window[0x2] * b0[0x2];
      sum -= window[0x3] * b0[0x3];
      sum += window[0x4] * b0[0x4];
      sum -= window[0x5] * b0[0x5];
      sum += window[0x6] * b0[0x6];
      sum -= window[0x7] * b0[0x7];
      sum += window[0x8] * b0[0x8];
      sum -= window[0x9] * b0[0x9];
      sum += window[0xA] * b0[0xA];
      sum -= window[0xB] * b0[0xB];
      sum += window[0xC] * b0[0xC];
      sum -= window[0xD] * b0[0xD];
      sum += window[0xE] * b0[0xE];
      sum -= window[0xF] * b0[0xF];

      WRITE_SAMPLE_F(samples,sum);
    }

    {
      real sum;
      sum  = window[0x0] * b0[0x0];
      sum += window[0x2] * b0[0x2];
      sum += window[0x4] * b0[0x4];
      sum += window[0x6] * b0[0x6];
      sum += window[0x8] * b0[0x8];
      sum += window[0xA] * b0[0xA];
      sum += window[0xC] * b0[0xC];
      sum += window[0xE] * b0[0xE];
      WRITE_SAMPLE_F(samples,sum);
      b0-=0x10,window-=0x20,samples+=step;
    }
    window += bo1<<1;

    for (j=15;j;j--,b0-=0x10,window-=0x20,samples+=step)
    {
      real sum;
      sum = -window[-0x1] * b0[0x0];
      sum -= window[-0x2] * b0[0x1];
      sum -= window[-0x3] * b0[0x2];
      sum -= window[-0x4] * b0[0x3];
      sum -= window[-0x5] * b0[0x4];
      sum -= window[-0x6] * b0[0x5];
      sum -= window[-0x7] * b0[0x6];
      sum -= window[-0x8] * b0[0x7];
      sum -= window[-0x9] * b0[0x8];
      sum -= window[-0xA] * b0[0x9];
      sum -= window[-0xB] * b0[0xA];
      sum -= window[-0xC] * b0[0xB];
      sum -= window[-0xD] * b0[0xC];
      sum -= window[-0xE] * b0[0xD];
      sum -= window[-0xF] * b0[0xE];
      sum -= window[-0x0] * b0[0xF];

      WRITE_SAMPLE_F(samples,sum);
    }
  }
  *pnt += 256;

  return clip;

}
