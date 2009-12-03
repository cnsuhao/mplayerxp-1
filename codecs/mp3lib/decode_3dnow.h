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
 *
 * 3DNow! and 3DNow!Ex optimizations by Nickols_K <nickols_k@mail.ru>
 */
#include <mm3dnow.h>
#include "../../mplayerxp/pvector/mmx2c.h"
#include "mpg123.h"

static __inline __m64 __attribute__((__gnu_inline__, __always_inline__))
_m_sign(__m64 src)
{
    register __m64 m0;
    asm volatile("pxor %0, %0":"=y"(m0));
    return _m_pfsub(m0,src);
}

int synth_1to1_3dnow32(real *bandPtr,int channel,unsigned char *out,int *pnt)
{
  __m64 mm[8];
  static real buffs[2][2][0x110];
  static const int step = 2;
  static int bo = 1;
  real *b0,(*buf)[0x110];
  int clip = 0;
  int bo1;
  float *samples = (float *) (out + *pnt);

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
    _m_femms();
    for (j=16;j;j--,b0+=0x10,window+=0x20,samples+=step)
    {
	mm[0]=_m_pfmul(_m_load(&window[0]),_m_load(&b0[0]));
	mm[1]=_m_pfmul(_m_load(&window[2]),_m_load(&b0[2]));
	mm[2]=_m_pfmul(_m_load(&window[4]),_m_load(&b0[4]));
	mm[3]=_m_pfmul(_m_load(&window[6]),_m_load(&b0[6]));
	mm[4]=_m_pfmul(_m_load(&window[8]), _m_load(&b0[8]));
	mm[5]=_m_pfmul(_m_load(&window[10]),_m_load(&b0[10]));
	mm[6]=_m_pfmul(_m_load(&window[12]),_m_load(&b0[12]));
	mm[7]=_m_pfmul(_m_load(&window[14]),_m_load(&b0[14]));
	mm[0]=_m_pfadd(mm[0],mm[1]);
	mm[2]=_m_pfadd(mm[2],mm[3]);
	mm[4]=_m_pfadd(mm[4],mm[5]);
	mm[6]=_m_pfadd(mm[6],mm[7]);
	mm[0]=_m_pfadd(mm[0],mm[2]);
	mm[4]=_m_pfadd(mm[4],mm[6]);
	mm[0]=_m_pfadd(mm[0],mm[4]);
	mm[0]=_m_pfpnacc(mm[0],mm[0]);
	_m_store_half(samples,mm[0]);
    }
    mm[0]=_m_load_half(&window[0]);
    mm[1]=_m_load_half(&b0[0]);
    mm[0]=_m_punpckldq(mm[0],_m_load(&window[2]));
    mm[1]=_m_punpckldq(mm[1],_m_load(&b0[2]));
    mm[0]=_m_pfmul(mm[0],mm[1]);
    mm[2]=_m_load_half(&window[4]);
    mm[3]=_m_load_half(&b0[4]);
    mm[2]=_m_punpckldq(mm[2],_m_load(&window[6]));
    mm[3]=_m_punpckldq(mm[3],_m_load(&b0[6]));
    mm[2]=_m_pfmul(mm[2],mm[3]);
    mm[4]=_m_load_half(&window[8]);
    mm[5]=_m_load_half(&b0[8]);
    mm[4]=_m_punpckldq(mm[4],_m_load(&window[10]));
    mm[5]=_m_punpckldq(mm[5],_m_load(&b0[10]));
    mm[4]=_m_pfmul(mm[4],mm[5]);
    mm[6]=_m_load_half(&window[12]);
    mm[7]=_m_load_half(&b0[12]);
    mm[6]=_m_punpckldq(mm[6],_m_load(&window[14]));
    mm[7]=_m_punpckldq(mm[7],_m_load(&b0[14]));
    mm[6]=_m_pfmul(mm[6],mm[7]);
    mm[0]=_m_pfadd(mm[0],mm[2]);
    mm[4]=_m_pfadd(mm[4],mm[6]);
    mm[0]=_m_pfadd(mm[0],mm[4]);
    mm[0]=_m_pfacc(mm[0],mm[0]);
    _m_store_half(samples,mm[0]);

    b0-=0x10,window-=0x20,samples+=step;
    window += bo1<<1;

    for (j=15;j;j--,b0-=0x10,window-=0x20,samples+=step)
    {
	mm[0]=_m_pswapd(_m_load(&window[-2]));
	mm[0]=_m_pfmul(mm[0],_m_load(&b0[0]));
	mm[1]=_m_pswapd(_m_load(&window[-4]));
	mm[1]=_m_pfmul(mm[1],_m_load(&b0[2]));
	mm[2]=_m_pswapd(_m_load(&window[-6]));
	mm[2]=_m_pfmul(mm[2],_m_load(&b0[4]));
	mm[3]=_m_pswapd(_m_load(&window[-8]));
	mm[3]=_m_pfmul(mm[3],_m_load(&b0[6]));
	mm[4]=_m_pswapd(_m_load(&window[-10]));
	mm[4]=_m_pfmul(mm[4],_m_load(&b0[8]));
	mm[5]=_m_pswapd(_m_load(&window[-12]));
	mm[5]=_m_pfmul(mm[5],_m_load(&b0[10]));
	mm[6]=_m_pswapd(_m_load(&window[-14]));
	mm[6]=_m_pfmul(mm[6],_m_load(&b0[12]));
	mm[7]=_m_load_half(&window[-15]);
	mm[7]=_m_punpckldq(mm[7],_m_load(&window[0]));
	mm[7]=_m_pfmul(mm[7],_m_load(&b0[14]));
	mm[0]=_m_pfadd(mm[0],mm[1]);
	mm[2]=_m_pfadd(mm[2],mm[3]);
	mm[0]=_m_pfadd(mm[0],mm[2]);
	mm[4]=_m_pfadd(mm[4],mm[5]);
	mm[6]=_m_pfadd(mm[6],mm[7]);
	mm[4]=_m_pfadd(mm[4],mm[6]);
	mm[0]=_m_pfadd(mm[0],mm[4]);
	mm[0]=_m_pfacc(mm[0],mm[0]);
	mm[4]=_m_sign(mm[0]);
	_m_store_half(samples,mm[4]);
    }
  }
  _m_femms();
  return clip;
}
