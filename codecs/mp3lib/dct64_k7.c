/*
 Replacement of dct64() with AMD's 3DNowEx(DSP)! SIMD operations support

 This code based 'dct64_3dnow.s' by Syuuhei Kashiyama
 <squash@mb.kcom.ne.jp>,only some types of changes have been made:

  - added new opcodes PSWAPD, PFPNACC
  - decreased number of opcodes (as it was suggested by k7 manual)
    (using memory reference as operand of instructions)
  - Phase 6 is rewritten with mixing of cpu and mmx opcodes
  - change function name for support 3DNowEx! automatic detect
  - negation of 3dnow reg was replaced with PXOR 0x800000000, MMi instead
    of PFMUL as it was suggested by athlon manual. (Two not separated PFMUL
    can not be paired, but PXOR can be).

 note: because K7 processors are an aggresive out-of-order three-way
       superscalar ones instruction order is not significand for them.

 Modified by Nickols_K <nickols_k@mail.ru>

 The author of this program disclaim whole expressed or implied
 warranties with regard to this program, and in no event shall the
 author of this program liable to whatever resulted from the use of
 this program. Use it at your own risk.

*/
#include <mm3dnow.h>
#include "../../mplayerxp/pvector/mmx2c.h"
#include "../../mplayerxp/mangle.h"
#include "../../mplayerxp/cpudetect.h"

typedef float real;
extern real * pnts[];
static unsigned  __attribute__((used)) __attribute__((__aligned__(8))) plus_minus_3dnow[2] = { 0x00000000UL, 0x80000000UL };
static const real f1 = 1.0;

/* Discrete Cosine Tansform (DCT) for subband synthesis */
void dct64_3dnow(real *a,real *b,real *c)
{
    char tmp[256],*tmp1,*tmp2;
    real* ppnts;
    __m64 mm[8];
	// 1
    ppnts = pnts[0];
    tmp1 = &tmp[0];
    tmp2 = &tmp[128];
    mm[0]=_m_load(&(((char *)c)[0]));
    mm[1]=mm[0];
    mm[2]=_m_load_half(&(((char *)c)[124]));
    mm[2]=_m_punpckldq(mm[2],_m_load(&(((char *)c)[120])));
    mm[0]=_m_pfadd(mm[0],mm[2]);
    _m_store(&(((char *)tmp1)[0]),mm[0]);
    mm[1]=_m_pfsub(mm[1],mm[2]);
    mm[1]=_m_pfmul(mm[1],_m_load(&(((char *)ppnts)[0])));
    mm[1]=_m_pswapd(mm[1]);
    _m_store(&(((char *)tmp1)[120]),mm[1]);
    mm[4]=_m_load(&(((char *)c)[8]));
    mm[5]=mm[4];
    mm[6]=_m_load_half(&(((char *)c)[116]));
    mm[6]=_m_punpckldq(mm[6],_m_load(&(((char *)c)[112])));
    mm[4]=_m_pfadd(mm[4],mm[6]);
    _m_store(&(((char *)tmp1)[8]),mm[4]);
    mm[5]=_m_pfsub(mm[5],mm[6]);
    mm[5]=_m_pfmul(mm[5],_m_load(&(((char *)ppnts)[8])));
    mm[5]=_m_pswapd(mm[5]);
    _m_store(&(((char *)tmp1)[112]),mm[5]);
    mm[0]=_m_load(&(((char *)c)[16]));
    mm[1]=mm[0];
    mm[2]=_m_load_half(&(((char *)c)[108]));
    mm[2]=_m_punpckldq(mm[2],_m_load(&(((char *)c)[104])));
    mm[0]=_m_pfadd(mm[0],mm[2]);
    _m_store(&(((char *)tmp1)[16]),mm[0]);
    mm[1]=_m_pfsub(mm[1],mm[2]);
    mm[1]=_m_pfmul(mm[1],_m_load(&(((char *)ppnts)[16])));
    mm[1]=_m_pswapd(mm[1]);
    _m_store(&(((char *)tmp1)[104]),mm[1]);
    mm[4]=_m_load(&(((char *)c)[24]));
    mm[5]=mm[4];
    mm[6]=_m_load_half(&(((char *)c)[100]));
    mm[6]=_m_punpckldq(mm[6],_m_load(&(((char *)c)[96])));
    mm[4]=_m_pfadd(mm[4],mm[6]);
    _m_store(&(((char *)tmp1)[24]),mm[4]);
    mm[5]=_m_pfsub(mm[5],mm[6]);
    mm[5]=_m_pfmul(mm[5],_m_load(&(((char *)ppnts)[24])));
    mm[5]=_m_pswapd(mm[5]);
    _m_store(&(((char *)tmp1)[96]),mm[5]);
    mm[0]=_m_load(&(((char *)c)[32]));
    mm[1]=mm[0];
    mm[2]=_m_load_half(&(((char *)c)[92]));
    mm[2]=_m_punpckldq(mm[2],_m_load(&(((char *)c)[88])));
    mm[0]=_m_pfadd(mm[0],mm[2]);
    _m_store(&(((char *)tmp1)[32]),mm[0]);
    mm[1]=_m_pfsub(mm[1],mm[2]);
    mm[1]=_m_pfmul(mm[1],_m_load(&(((char *)ppnts)[32])));
    mm[1]=_m_pswapd(mm[1]);
    _m_store(&(((char *)tmp1)[88]),mm[1]);
    mm[4]=_m_load(&(((char *)c)[40]));
    mm[5]=mm[4];
    mm[6]=_m_load_half(&(((char *)c)[84]));
    mm[6]=_m_punpckldq(mm[6],_m_load(&(((char *)c)[80])));
    mm[4]=_m_pfadd(mm[4],mm[6]);
    _m_store(&(((char *)tmp1)[40]),mm[4]);
    mm[5]=_m_pfsub(mm[5],mm[6]);
    mm[5]=_m_pfmul(mm[5],_m_load(&(((char *)ppnts)[40])));
    mm[5]=_m_pswapd(mm[5]);
    _m_store(&(((char *)tmp1)[80]),mm[5]);
    mm[0]=_m_load(&(((char *)c)[48]));
    mm[1]=mm[0];
    mm[2]=_m_load_half(&(((char *)c)[76]));
    mm[2]=_m_punpckldq(mm[2],_m_load(&(((char *)c)[72])));
    mm[0]=_m_pfadd(mm[0],mm[2]);
    _m_store(&(((char *)tmp1)[48]),mm[0]);
    mm[1]=_m_pfsub(mm[1],mm[2]);
    mm[1]=_m_pfmul(mm[1],_m_load(&(((char *)ppnts)[48])));
    mm[1]=_m_pswapd(mm[1]);
    _m_store(&(((char *)tmp1)[72]),mm[1]);
    mm[4]=_m_load(&(((char *)c)[56]));
    mm[5]=mm[4];
    mm[6]=_m_load_half(&(((char *)c)[68]));
    mm[6]=_m_punpckldq(mm[6],_m_load(&(((char *)c)[64])));
    mm[4]=_m_pfadd(mm[4],mm[6]);
    _m_store(&(((char *)tmp1)[56]),mm[4]);
    mm[5]=_m_pfsub(mm[5],mm[6]);
    mm[5]=_m_pfmul(mm[5],_m_load(&(((char *)ppnts)[56])));
    mm[5]=_m_pswapd(mm[5]);
    _m_store(&(((char *)tmp1)[64]),mm[5]);

	// 2
	// 0, 14
    ppnts = pnts[1];
    mm[0]=_m_load(&(((char *)tmp1)[0]));
    mm[1]=mm[0];
    mm[2]=_m_load_half(&(((char *)tmp1)[60]));
    mm[2]=_m_punpckldq(mm[2],_m_load(&(((char *)tmp1)[56])));
    mm[3]=_m_load(&(((char *)ppnts)[0]));
    mm[0]=_m_pfadd(mm[0],mm[2]);
    _m_store(&(((char *)tmp2)[0]),mm[0]);
    mm[1]=_m_pfsub(mm[1],mm[2]);
    mm[1]=_m_pfmul(mm[1],mm[3]);
    mm[1]=_m_pswapd(mm[1]);
    _m_store(&(((char *)tmp2)[56]),mm[1]);
	// 16, 30
    mm[0]=_m_load(&(((char *)tmp1)[64]));
    mm[1]=mm[0];
    mm[2]=_m_load_half(&(((char *)tmp1)[124]));
    mm[2]=_m_punpckldq(mm[2],_m_load(&(((char *)tmp1)[120])));
    mm[0]=_m_pfadd(mm[0],mm[2]);
    _m_store(&(((char *)tmp2)[64]),mm[0]);
    mm[1]=_m_pfsubr(mm[1],mm[2]);
    mm[1]=_m_pfmul(mm[1],mm[3]);
    mm[1]=_m_pswapd(mm[1]);
    _m_store(&(((char *)tmp2)[120]),mm[1]);
    mm[4]=_m_load(&(((char *)tmp1)[8]));
	// 2, 12
    mm[5]=mm[4];
    mm[6]=_m_load_half(&(((char *)tmp1)[52]));
    mm[6]=_m_punpckldq(mm[6],_m_load(&(((char *)tmp1)[48])));
    mm[7]=_m_load(&(((char *)ppnts)[8]));
    mm[4]=_m_pfadd(mm[4],mm[6]);
    _m_store(&(((char *)tmp2)[8]),mm[4]);
    mm[5]=_m_pfsub(mm[5],mm[6]);
    mm[5]=_m_pfmul(mm[5],mm[7]);
    mm[5]=_m_pswapd(mm[5]);
    _m_store(&(((char *)tmp2)[48]),mm[5]);
    mm[4]=_m_load(&(((char *)tmp1)[72]));
	// 18, 28
    mm[5]=mm[4];
    mm[6]=_m_load_half(&(((char *)tmp1)[116]));
    mm[6]=_m_punpckldq(mm[6],_m_load(&(((char *)tmp1)[112])));
    mm[4]=_m_pfadd(mm[4],mm[6]);
    _m_store(&(((char *)tmp2)[72]),mm[4]);
    mm[5]=_m_pfsubr(mm[5],mm[6]);
    mm[5]=_m_pfmul(mm[5],mm[7]);
    mm[5]=_m_pswapd(mm[5]);
    _m_store(&(((char *)tmp2)[112]),mm[5]);
    mm[0]=_m_load(&(((char *)tmp1)[16]));
	// 4, 10
    mm[1]=mm[0];
    mm[2]=_m_load_half(&(((char *)tmp1)[44]));
    mm[2]=_m_punpckldq(mm[2],_m_load(&(((char *)tmp1)[40])));
    mm[3]=_m_load(&(((char *)ppnts)[16]));
    mm[0]=_m_pfadd(mm[0],mm[2]);
    _m_store(&(((char *)tmp2)[16]),mm[0]);
    mm[1]=_m_pfsub(mm[1],mm[2]);
    mm[1]=_m_pfmul(mm[1],mm[3]);
    mm[1]=_m_pswapd(mm[1]);
    _m_store(&(((char *)tmp2)[40]),mm[1]);
    mm[0]=_m_load(&(((char *)tmp1)[80]));
	// 20, 26
    mm[1]=mm[0];
    mm[2]=_m_load_half(&(((char *)tmp1)[108]));
    mm[2]=_m_punpckldq(mm[2],_m_load(&(((char *)tmp1)[104])));
    mm[0]=_m_pfadd(mm[0],mm[2]);
    _m_store(&(((char *)tmp2)[80]),mm[0]);
    mm[1]=_m_pfsubr(mm[1],mm[2]);
    mm[1]=_m_pfmul(mm[1],mm[3]);
    mm[1]=_m_pswapd(mm[1]);
    _m_store(&(((char *)tmp2)[104]),mm[1]);
    mm[4]=_m_load(&(((char *)tmp1)[24]));
	// 6, 8
    mm[5]=mm[4];
    mm[6]=_m_load_half(&(((char *)tmp1)[36]));
    mm[6]=_m_punpckldq(mm[6],_m_load(&(((char *)tmp1)[32])));
    mm[7]=_m_load(&(((char *)ppnts)[24]));
    mm[4]=_m_pfadd(mm[4],mm[6]);
    _m_store(&(((char *)tmp2)[24]),mm[4]);
    mm[5]=_m_pfsub(mm[5],mm[6]);
    mm[5]=_m_pfmul(mm[5],mm[7]);
    mm[5]=_m_pswapd(mm[5]);
    _m_store(&(((char *)tmp2)[32]),mm[5]);
    mm[4]=_m_load(&(((char *)tmp1)[88]));
	// 22, 24
    mm[5]=mm[4];
    mm[6]=_m_load_half(&(((char *)tmp1)[100]));
    mm[6]=_m_punpckldq(mm[6],_m_load(&(((char *)tmp1)[96])));
    mm[4]=_m_pfadd(mm[4],mm[6]);
    _m_store(&(((char *)tmp2)[88]),mm[4]);
    mm[5]=_m_pfsubr(mm[5],mm[6]);
    mm[5]=_m_pfmul(mm[5],mm[7]);
    mm[5]=_m_pswapd(mm[5]);
    _m_store(&(((char *)tmp2)[96]),mm[5]);

	// 3
    ppnts=pnts[2];
    mm[0]=_m_load(&(((char *)ppnts)[0]));
    mm[1]=_m_load(&(((char *)ppnts)[8]));
    mm[2]=_m_load(&(((char *)tmp2)[0]));
	// 0, 6
    mm[3]=mm[2];
    mm[4]=_m_load_half(&(((char *)tmp2)[28]));
    mm[4]=_m_punpckldq(mm[4],_m_load(&(((char *)tmp2)[24])));
    mm[2]=_m_pfadd(mm[2],mm[4]);
    mm[3]=_m_pfsub(mm[3],mm[4]);
    mm[3]=_m_pfmul(mm[3],mm[0]);
    _m_store(&(((char *)tmp1)[0]),mm[2]);
    mm[3]=_m_pswapd(mm[3]);
    _m_store(&(((char *)tmp1)[24]),mm[3]);
    mm[5]=_m_load(&(((char *)tmp2)[8]));
	// 2, 4
    mm[6]=mm[5];
    mm[7]=_m_load_half(&(((char *)tmp2)[20]));
    mm[7]=_m_punpckldq(mm[7],_m_load(&(((char *)tmp2)[16])));
    mm[5]=_m_pfadd(mm[5],mm[7]);
    mm[6]=_m_pfsub(mm[6],mm[7]);
    mm[6]=_m_pfmul(mm[6],mm[1]);
    _m_store(&(((char *)tmp1)[8]),mm[5]);
    mm[6]=_m_pswapd(mm[6]);
    _m_store(&(((char *)tmp1)[16]),mm[6]);
    mm[2]=_m_load(&(((char *)tmp2)[32]));
	// 8, 14
    mm[3]=mm[2];
    mm[4]=_m_load_half(&(((char *)tmp2)[60]));
    mm[4]=_m_punpckldq(mm[4],_m_load(&(((char *)tmp2)[56])));
    mm[2]=_m_pfadd(mm[2],mm[4]);
    mm[3]=_m_pfsubr(mm[3],mm[4]);
    mm[3]=_m_pfmul(mm[3],mm[0]);
    _m_store(&(((char *)tmp1)[32]),mm[2]);
    mm[3]=_m_pswapd(mm[3]);
    _m_store(&(((char *)tmp1)[56]),mm[3]);
    mm[5]=_m_load(&(((char *)tmp2)[40]));
	// 10, 12
    mm[6]=mm[5];
    mm[7]=_m_load_half(&(((char *)tmp2)[52]));
    mm[7]=_m_punpckldq(mm[7],_m_load(&(((char *)tmp2)[48])));
    mm[5]=_m_pfadd(mm[5],mm[7]);
    mm[6]=_m_pfsubr(mm[6],mm[7]);
    mm[6]=_m_pfmul(mm[6],mm[1]);
    _m_store(&(((char *)tmp1)[40]),mm[5]);
    mm[6]=_m_pswapd(mm[6]);
    _m_store(&(((char *)tmp1)[48]),mm[6]);
    mm[2]=_m_load(&(((char *)tmp2)[64]));
	// 16, 22
    mm[3]=mm[2];
    mm[4]=_m_load_half(&(((char *)tmp2)[92]));
    mm[4]=_m_punpckldq(mm[4],_m_load(&(((char *)tmp2)[88])));
    mm[2]=_m_pfadd(mm[2],mm[4]);
    mm[3]=_m_pfsub(mm[3],mm[4]);
    mm[3]=_m_pfmul(mm[3],mm[0]);
    _m_store(&(((char *)tmp1)[64]),mm[2]);
    mm[3]=_m_pswapd(mm[3]);
    _m_store(&(((char *)tmp1)[88]),mm[3]);
    mm[5]=_m_load(&(((char *)tmp2)[72]));
	// 18, 20
    mm[6]=mm[5];
    mm[7]=_m_load_half(&(((char *)tmp2)[84]));
    mm[7]=_m_punpckldq(mm[7],_m_load(&(((char *)tmp2)[80])));
    mm[5]=_m_pfadd(mm[5],mm[7]);
    mm[6]=_m_pfsub(mm[6],mm[7]);
    mm[6]=_m_pfmul(mm[6],mm[1]);
    _m_store(&(((char *)tmp1)[72]),mm[5]);
    mm[6]=_m_pswapd(mm[6]);
    _m_store(&(((char *)tmp1)[80]),mm[6]);
    mm[2]=_m_load(&(((char *)tmp2)[96]));
	// 24, 30
    mm[3]=mm[2];
    mm[4]=_m_load_half(&(((char *)tmp2)[124]));
    mm[4]=_m_punpckldq(mm[4],_m_load(&(((char *)tmp2)[120])));
    mm[2]=_m_pfadd(mm[2],mm[4]);
    mm[3]=_m_pfsubr(mm[3],mm[4]);
    mm[3]=_m_pfmul(mm[3],mm[0]);
    _m_store(&(((char *)tmp1)[96]),mm[2]);
    mm[3]=_m_pswapd(mm[3]);
    _m_store(&(((char *)tmp1)[120]),mm[3]);
    mm[5]=_m_load(&(((char *)tmp2)[104]));
	// 26, 28
    mm[6]=mm[5];
    mm[7]=_m_load_half(&(((char *)tmp2)[116]));
    mm[7]=_m_punpckldq(mm[7],_m_load(&(((char *)tmp2)[112])));
    mm[5]=_m_pfadd(mm[5],mm[7]);
    mm[6]=_m_pfsubr(mm[6],mm[7]);
    mm[6]=_m_pfmul(mm[6],mm[1]);
    _m_store(&(((char *)tmp1)[104]),mm[5]);
    mm[6]=_m_pswapd(mm[6]);
    _m_store(&(((char *)tmp1)[112]),mm[6]);

	// 4
    ppnts=pnts[3];
    mm[0]=_m_load(&(((char *)ppnts)[0]));
    mm[1]=_m_load(&(((char *)tmp1)[0]));
	// 0
    mm[2]=mm[1];
    mm[3]=_m_load_half(&(((char *)tmp1)[12]));
    mm[3]=_m_punpckldq(mm[3],_m_load(&(((char *)tmp1)[8])));
    mm[1]=_m_pfadd(mm[1],mm[3]);
    mm[2]=_m_pfsub(mm[2],mm[3]);
    mm[2]=_m_pfmul(mm[2],mm[0]);
    _m_store(&(((char *)tmp2)[0]),mm[1]);
    mm[2]=_m_pswapd(mm[2]);
    _m_store(&(((char *)tmp2)[8]),mm[2]);
    mm[4]=_m_load(&(((char *)tmp1)[16]));
	// 4
    mm[5]=mm[4];
    mm[6]=_m_load_half(&(((char *)tmp1)[28]));
    mm[6]=_m_punpckldq(mm[6],_m_load(&(((char *)tmp1)[24])));
    mm[4]=_m_pfadd(mm[4],mm[6]);
    mm[5]=_m_pfsubr(mm[5],mm[6]);
    mm[5]=_m_pfmul(mm[5],mm[0]);
    _m_store(&(((char *)tmp2)[16]),mm[4]);
    mm[5]=_m_pswapd(mm[5]);
    _m_store(&(((char *)tmp2)[24]),mm[5]);
    mm[1]=_m_load(&(((char *)tmp1)[32]));
	// 8
    mm[2]=mm[1];
    mm[3]=_m_load_half(&(((char *)tmp1)[44]));
    mm[3]=_m_punpckldq(mm[3],_m_load(&(((char *)tmp1)[40])));
    mm[1]=_m_pfadd(mm[1],mm[3]);
    mm[2]=_m_pfsub(mm[2],mm[3]);
    mm[2]=_m_pfmul(mm[2],mm[0]);
    _m_store(&(((char *)tmp2)[32]),mm[1]);
    mm[2]=_m_pswapd(mm[2]);
    _m_store(&(((char *)tmp2)[40]),mm[2]);
    mm[4]=_m_load(&(((char *)tmp1)[48]));
	// 12
    mm[5]=mm[4];
    mm[6]=_m_load_half(&(((char *)tmp1)[60]));
    mm[6]=_m_punpckldq(mm[6],_m_load(&(((char *)tmp1)[56])));
    mm[4]=_m_pfadd(mm[4],mm[6]);
    mm[5]=_m_pfsubr(mm[5],mm[6]);
    mm[5]=_m_pfmul(mm[5],mm[0]);
    _m_store(&(((char *)tmp2)[48]),mm[4]);
    mm[5]=_m_pswapd(mm[5]);
    _m_store(&(((char *)tmp2)[56]),mm[5]);
    mm[1]=_m_load(&(((char *)tmp1)[64]));
	// 16
    mm[2]=mm[1];
    mm[3]=_m_load_half(&(((char *)tmp1)[76]));
    mm[3]=_m_punpckldq(mm[3],_m_load(&(((char *)tmp1)[72])));
    mm[1]=_m_pfadd(mm[1],mm[3]);
    mm[2]=_m_pfsub(mm[2],mm[3]);
    mm[2]=_m_pfmul(mm[2],mm[0]);
    _m_store(&(((char *)tmp2)[64]),mm[1]);
    mm[2]=_m_pswapd(mm[2]);
    _m_store(&(((char *)tmp2)[72]),mm[2]);
    mm[4]=_m_load(&(((char *)tmp1)[80]));
	// 20
    mm[5]=mm[4];
    mm[6]=_m_load_half(&(((char *)tmp1)[92]));
    mm[6]=_m_punpckldq(mm[6],_m_load(&(((char *)tmp1)[88])));
    mm[4]=_m_pfadd(mm[4],mm[6]);
    mm[5]=_m_pfsubr(mm[5],mm[6]);
    mm[5]=_m_pfmul(mm[5],mm[0]);
    _m_store(&(((char *)tmp2)[80]),mm[4]);
    mm[5]=_m_pswapd(mm[5]);
    _m_store(&(((char *)tmp2)[88]),mm[5]);
    mm[1]=_m_load(&(((char *)tmp1)[96]));
	// 24
    mm[2]=mm[1];
    mm[3]=_m_load_half(&(((char *)tmp1)[108]));
    mm[3]=_m_punpckldq(mm[3],_m_load(&(((char *)tmp1)[104])));
    mm[1]=_m_pfadd(mm[1],mm[3]);
    mm[2]=_m_pfsub(mm[2],mm[3]);
    mm[2]=_m_pfmul(mm[2],mm[0]);
    _m_store(&(((char *)tmp2)[96]),mm[1]);
    mm[2]=_m_pswapd(mm[2]);
    _m_store(&(((char *)tmp2)[104]),mm[2]);
    mm[4]=_m_load(&(((char *)tmp1)[112]));
	// 28
    mm[5]=mm[4];
    mm[6]=_m_load_half(&(((char *)tmp1)[124]));
    mm[6]=_m_punpckldq(mm[6],_m_load(&(((char *)tmp1)[120])));
    mm[4]=_m_pfadd(mm[4],mm[6]);
    mm[5]=_m_pfsubr(mm[5],mm[6]);
    mm[5]=_m_pfmul(mm[5],mm[0]);
    _m_store(&(((char *)tmp2)[112]),mm[4]);
    mm[5]=_m_pswapd(mm[5]);
    _m_store(&(((char *)tmp2)[120]),mm[5]);

	// 5
    ppnts=pnts[4];
    mm[0]=_m_load(&plus_minus_3dnow);
    mm[1]=_m_load_half(&f1);

    mm[2]=_m_load_half(&(((char *)ppnts)[0]));
    mm[1]=_m_punpckldq(mm[1],mm[2]);
    mm[2]=_m_load(&(((char *)tmp2)[0]));
	// 0
    mm[2]=_m_pfpnacc(mm[2],mm[2]);
    mm[2]=_m_pswapd(mm[2]);
    mm[2]=_m_pfmul(mm[2],mm[1]);
    _m_store(&(((char *)tmp1)[0]),mm[2]);
    mm[4]=_m_load(&(((char *)tmp2)[8]));
    mm[4]=_m_pfpnacc(mm[4],mm[4]);
    mm[4]=_m_pswapd(mm[4]);
    mm[4]=_m_pxor(mm[4],mm[0]);
    mm[4]=_m_pfmul(mm[4],mm[1]);
    mm[5]=mm[4];
    mm[5]=_m_psrlqi(mm[5],32);
    mm[4]=_m_pfacc(mm[4],mm[5]);
    _m_store(&(((char *)tmp1)[8]),mm[4]);
    mm[2]=_m_load(&(((char *)tmp2)[16]));
	// 4
    mm[2]=_m_pfpnacc(mm[2],mm[2]);
    mm[2]=_m_pswapd(mm[2]);
    mm[2]=_m_pfmul(mm[2],mm[1]);
    mm[4]=_m_load(&(((char *)tmp2)[24]));
    mm[4]=_m_pfpnacc(mm[4],mm[4]);
    mm[4]=_m_pswapd(mm[4]);
    mm[4]=_m_pxor(mm[4],mm[0]);
    mm[4]=_m_pfmul(mm[4],mm[1]);
    mm[5]=mm[4];
    mm[5]=_m_psrlqi(mm[5],32);
    mm[4]=_m_pfacc(mm[4],mm[5]);
    mm[3]=mm[2];
    mm[3]=_m_psrlqi(mm[3],32);
    mm[2]=_m_pfadd(mm[2],mm[4]);
    mm[4]=_m_pfadd(mm[4],mm[3]);
    _m_store(&(((char *)tmp1)[16]),mm[2]);
    _m_store(&(((char *)tmp1)[24]),mm[4]);
    mm[2]=_m_load(&(((char *)tmp2)[32]));
	// 8
    mm[2]=_m_pfpnacc(mm[2],mm[2]);
    mm[2]=_m_pswapd(mm[2]);
    mm[2]=_m_pfmul(mm[2],mm[1]);
    _m_store(&(((char *)tmp1)[32]),mm[2]);
    mm[4]=_m_load(&(((char *)tmp2)[40]));
    mm[4]=_m_pfpnacc(mm[4],mm[4]);
    mm[4]=_m_pswapd(mm[4]);
    mm[4]=_m_pxor(mm[4],mm[0]);
    mm[4]=_m_pfmul(mm[4],mm[1]);
    mm[5]=mm[4];
    mm[5]=_m_psrlqi(mm[5],32);
    mm[4]=_m_pfacc(mm[4],mm[5]);
    _m_store(&(((char *)tmp1)[40]),mm[4]);
    mm[2]=_m_load(&(((char *)tmp2)[48]));
	// 12
    mm[2]=_m_pfpnacc(mm[2],mm[2]);
    mm[2]=_m_pswapd(mm[2]);
    mm[2]=_m_pfmul(mm[2],mm[1]);
    mm[4]=_m_load(&(((char *)tmp2)[56]));
    mm[4]=_m_pfpnacc(mm[4],mm[4]);
    mm[4]=_m_pswapd(mm[4]);
    mm[4]=_m_pxor(mm[4],mm[0]);
    mm[4]=_m_pfmul(mm[4],mm[1]);
    mm[5]=mm[4];
    mm[5]=_m_psrlqi(mm[5],32);
    mm[4]=_m_pfacc(mm[4],mm[5]);
    mm[3]=mm[2];
    mm[3]=_m_psrlqi(mm[3],32);
    mm[2]=_m_pfadd(mm[2],mm[4]);
    mm[4]=_m_pfadd(mm[4],mm[3]);
    _m_store(&(((char *)tmp1)[48]),mm[2]);
    _m_store(&(((char *)tmp1)[56]),mm[4]);
    mm[2]=_m_load(&(((char *)tmp2)[64]));
	// 16
    mm[2]=_m_pfpnacc(mm[2],mm[2]);
    mm[2]=_m_pswapd(mm[2]);
    mm[2]=_m_pfmul(mm[2],mm[1]);
    _m_store(&(((char *)tmp1)[64]),mm[2]);
    mm[4]=_m_load(&(((char *)tmp2)[72]));
    mm[4]=_m_pfpnacc(mm[4],mm[4]);
    mm[4]=_m_pswapd(mm[4]);
    mm[4]=_m_pxor(mm[4],mm[0]);
    mm[4]=_m_pfmul(mm[4],mm[1]);
    mm[5]=mm[4];
    mm[5]=_m_psrlqi(mm[5],32);
    mm[4]=_m_pfacc(mm[4],mm[5]);
    _m_store(&(((char *)tmp1)[72]),mm[4]);
    mm[2]=_m_load(&(((char *)tmp2)[80]));
	// 20
    mm[2]=_m_pfpnacc(mm[2],mm[2]);
    mm[2]=_m_pswapd(mm[2]);
    mm[2]=_m_pfmul(mm[2],mm[1]);
    mm[4]=_m_load(&(((char *)tmp2)[88]));
    mm[4]=_m_pfpnacc(mm[4],mm[4]);
    mm[4]=_m_pswapd(mm[4]);
    mm[4]=_m_pxor(mm[4],mm[0]);
    mm[4]=_m_pfmul(mm[4],mm[1]);
    mm[5]=mm[4];
    mm[5]=_m_psrlqi(mm[5],32);
    mm[4]=_m_pfacc(mm[4],mm[5]);
    mm[3]=mm[2];
    mm[3]=_m_psrlqi(mm[3],32);
    mm[2]=_m_pfadd(mm[2],mm[4]);
    mm[4]=_m_pfadd(mm[4],mm[3]);
    _m_store(&(((char *)tmp1)[80]),mm[2]);
    _m_store(&(((char *)tmp1)[88]),mm[4]);
    mm[2]=_m_load(&(((char *)tmp2)[96]));
	// 24
    mm[2]=_m_pfpnacc(mm[2],mm[2]);
    mm[2]=_m_pswapd(mm[2]);
    mm[2]=_m_pfmul(mm[2],mm[1]);
    _m_store(&(((char *)tmp1)[96]),mm[2]);
    mm[4]=_m_load(&(((char *)tmp2)[104]));
    mm[4]=_m_pfpnacc(mm[4],mm[4]);
    mm[4]=_m_pswapd(mm[4]);
    mm[4]=_m_pxor(mm[4],mm[0]);
    mm[4]=_m_pfmul(mm[4],mm[1]);
    mm[5]=mm[4];
    mm[5]=_m_psrlqi(mm[5],32);
    mm[4]=_m_pfacc(mm[4],mm[5]);
    _m_store(&(((char *)tmp1)[104]),mm[4]);
    mm[2]=_m_load(&(((char *)tmp2)[112]));
	// 28
    mm[2]=_m_pfpnacc(mm[2],mm[2]);
    mm[2]=_m_pswapd(mm[2]);
    mm[2]=_m_pfmul(mm[2],mm[1]);
    mm[4]=_m_load(&(((char *)tmp2)[120]));
    mm[4]=_m_pfpnacc(mm[4],mm[4]);
    mm[4]=_m_pswapd(mm[4]);
    mm[4]=_m_pxor(mm[4],mm[0]);
    mm[4]=_m_pfmul(mm[4],mm[1]);
    mm[5]=mm[4];
    mm[5]=_m_psrlqi(mm[5],32);
    mm[4]=_m_pfacc(mm[4],mm[5]);
    mm[3]=mm[2];
    mm[3]=_m_psrlqi(mm[3],32);
    mm[2]=_m_pfadd(mm[2],mm[4]);
    mm[4]=_m_pfadd(mm[4],mm[3]);
    _m_store(&(((char *)tmp1)[112]),mm[2]);
    _m_store(&(((char *)tmp1)[120]),mm[4]);

	// Phase6
    mm[0]=_m_load_half(&(((char *)tmp1)[0]));
    _m_store_half(&(((char *)a)[1024]),mm[0]);
    mm[2]=_m_load_half(&(((char *)tmp1)[4]));
    _m_store_half(&(((char *)a)[0]),mm[2]);
    _m_store_half(&(((char *)b)[0]),mm[2]);
    mm[2]=_m_load_half(&(((char *)tmp1)[8]));
    _m_store_half(&(((char *)a)[512]),mm[2]);
    mm[3]=_m_load_half(&(((char *)tmp1)[12]));
    _m_store_half(&(((char *)b)[512]),mm[3]);
    mm[5]=_m_load_half(&(((char *)tmp1)[16]));
    _m_store_half(&(((char *)a)[768]),mm[5]);
    mm[5]=_m_load_half(&(((char *)tmp1)[20]));
    _m_store_half(&(((char *)b)[256]),mm[5]);
    mm[6]=_m_load_half(&(((char *)tmp1)[24]));
    _m_store_half(&(((char *)a)[256]),mm[6]);
    mm[7]=_m_load_half(&(((char *)tmp1)[28]));
    _m_store_half(&(((char *)b)[768]),mm[7]);
    mm[0]=_m_load(&(((char *)tmp1)[32]));
    mm[1]=_m_load(&(((char *)tmp1)[48]));
    mm[0]=_m_pfadd(mm[0],mm[1]);
    _m_store_half(&(((char *)a)[896]),mm[0]);
    mm[0]=_m_psrlqi(mm[0],32);
    _m_store_half(&(((char *)b)[128]),mm[0]);
    mm[2]=_m_load(&(((char *)tmp1)[40]));
    mm[1]=_m_pfadd(mm[1],mm[2]);
    _m_store_half(&(((char *)a)[640]),mm[1]);
    mm[1]=_m_psrlqi(mm[1],32);
    _m_store_half(&(((char *)b)[384]),mm[1]);
    mm[3]=_m_load(&(((char *)tmp1)[56]));
    mm[2]=_m_pfadd(mm[2],mm[3]);
    _m_store_half(&(((char *)a)[384]),mm[2]);
    mm[2]=_m_psrlqi(mm[2],32);
    _m_store_half(&(((char *)b)[640]),mm[2]);
    mm[4]=_m_load_half(&(((char *)tmp1)[36]));
    mm[3]=_m_pfadd(mm[3],mm[4]);
    _m_store_half(&(((char *)a)[128]),mm[3]);
    mm[3]=_m_psrlqi(mm[3],32);
    _m_store_half(&(((char *)b)[896]),mm[3]);
    mm[0]=_m_load(&(((char *)tmp1)[96]));
    mm[1]=_m_load(&(((char *)tmp1)[64]));
    mm[2]=_m_load(&(((char *)tmp1)[112]));
    mm[0]=_m_pfadd(mm[0],mm[2]);
    mm[3]=mm[0];
    mm[3]=_m_pfadd(mm[3],mm[1]);
    _m_store_half(&(((char *)a)[960]),mm[3]);
    mm[3]=_m_psrlqi(mm[3],32);
    _m_store_half(&(((char *)b)[64]),mm[3]);
    mm[1]=_m_load(&(((char *)tmp1)[80]));
    mm[0]=_m_pfadd(mm[0],mm[1]);
    _m_store_half(&(((char *)a)[832]),mm[0]);
    mm[0]=_m_psrlqi(mm[0],32);
    _m_store_half(&(((char *)b)[192]),mm[0]);
    mm[3]=_m_load(&(((char *)tmp1)[104]));
    mm[2]=_m_pfadd(mm[2],mm[3]);
    mm[4]=mm[2];
    mm[4]=_m_pfadd(mm[4],mm[1]);
    _m_store_half(&(((char *)a)[704]),mm[4]);
    mm[4]=_m_psrlqi(mm[4],32);
    _m_store_half(&(((char *)b)[320]),mm[4]);
    mm[1]=_m_load(&(((char *)tmp1)[72]));
    mm[2]=_m_pfadd(mm[2],mm[1]);
    _m_store_half(&(((char *)a)[576]),mm[2]);
    mm[2]=_m_psrlqi(mm[2],32);
    _m_store_half(&(((char *)b)[448]),mm[2]);
    mm[4]=_m_load(&(((char *)tmp1)[120]));
    mm[3]=_m_pfadd(mm[3],mm[4]);
    mm[5]=mm[3];
    mm[5]=_m_pfadd(mm[5],mm[1]);
    _m_store_half(&(((char *)a)[448]),mm[5]);
    mm[5]=_m_psrlqi(mm[5],32);
    _m_store_half(&(((char *)b)[576]),mm[5]);
    mm[1]=_m_load(&(((char *)tmp1)[88]));
    mm[3]=_m_pfadd(mm[3],mm[1]);
    _m_store_half(&(((char *)a)[320]),mm[3]);
    mm[3]=_m_psrlqi(mm[3],32);
    _m_store_half(&(((char *)b)[704]),mm[3]);
    mm[5]=_m_load_half(&(((char *)tmp1)[100]));
    mm[4]=_m_pfadd(mm[4],mm[5]);
    mm[6]=mm[4];
    mm[6]=_m_pfadd(mm[6],mm[1]);
    _m_store_half(&(((char *)a)[192]),mm[6]);
    mm[6]=_m_psrlqi(mm[6],32);
    _m_store_half(&(((char *)b)[832]),mm[6]);
    mm[1]=_m_load_half(&(((char *)tmp1)[68]));
    mm[4]=_m_pfadd(mm[4],mm[1]);
    _m_store_half(&(((char *)a)[64]),mm[4]);
    mm[4]=_m_psrlqi(mm[4],32);
    _m_store_half(&(((char *)b)[960]),mm[4]);

    _m_femms();
    return;
}
