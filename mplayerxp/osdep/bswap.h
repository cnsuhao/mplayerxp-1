#ifndef __BSWAP_H__
#define __BSWAP_H__

#ifdef HAVE_CONFIG_H
#include "mp_config.h"
#endif

#ifdef HAVE_BYTESWAP_H
#include <byteswap.h>
#else

#include <inttypes.h> /* for __WORDSIZE */

#define MAKE_FOURCC( ch0, ch1, ch2, ch3 ) \
        (((long)(unsigned char)(ch3)       ) | \
        ( (long)(unsigned char)(ch2) << 8  ) | \
        ( (long)(unsigned char)(ch1) << 16 ) | \
        ( (long)(unsigned char)(ch0) << 24 ) )
#define MAKE_TWOCC(ch0, ch1) \
        ((short)(unsigned char)(ch0) |\
        ((short)(unsigned char)(ch1) << 8))


#if defined(__i386__)
inline static unsigned short ByteSwap16(unsigned short x)
{
  __asm("xchgb %b0,%h0"	:
        "=q" (x)	:
        "0" (x));
    return x;
}
#define bswap_16(x) ByteSwap16(x)

inline static unsigned int ByteSwap32(unsigned int x)
{
#if __CPU__ > 386
 __asm("bswap	%0":
      "=r" (x)     :
#else
 __asm("xchgb	%b0,%h0\n"
      "	rorl	$16,%0\n"
      "	xchgb	%b0,%h0":
      "=q" (x)		:
#endif
      "0" (x));
  return x;
}
#define bswap_32(x) ByteSwap32(x)

inline static unsigned long long int ByteSwap64(unsigned long long int x)
{
  register union { __extension__ unsigned long long int __ll;
          unsigned long int __l[2]; } __x;
  asm("xchgl	%0,%1":
      "=r"(__x.__l[0]),"=r"(__x.__l[1]):
      "0"(bswap_32((unsigned long)x)),"1"(bswap_32((unsigned long)(x>>32))));
  return __x.__ll;
}
#define bswap_64(x) ByteSwap64(x)

#elif defined(__x86_64__)

inline static unsigned short ByteSwap16(unsigned short x)
{
  __asm("rorw $8, %w0"	:
	"=r" (x)	:
	"0" (x)		:
	"cc");
    return x;
}
#define bswap_16(x) ByteSwap16(x)

inline static unsigned int ByteSwap32(unsigned int x)
{
 __asm("bswapl	%0":
      "=r" (x)     :
      "0" (x));
  return x;
}
#define bswap_32(x) ByteSwap32(x)

inline static unsigned long long int ByteSwap64(unsigned long long int x)
{
 __asm("bswapq	%0":
      "=r" (x)     :
      "0" (x));
  return x;
}
#define bswap_64(x) ByteSwap64(x)

#else

#define bswap_16(x) (((x) & 0x00ff) << 8 | ((x) & 0xff00) >> 8)
			

// code from bits/byteswap.h (C) 1997, 1998 Free Software Foundation, Inc.
#define bswap_32(x) \
     ((((x) & 0xff000000) >> 24) | (((x) & 0x00ff0000) >>  8) | \
      (((x) & 0x0000ff00) <<  8) | (((x) & 0x000000ff) << 24))

#if __WORDSIZE >= 64
# define bswap_64(x) \
     ((((x) & 0xff00000000000000ull) >> 56)				      \
      | (((x) & 0x00ff000000000000ull) >> 40)				      \
      | (((x) & 0x0000ff0000000000ull) >> 24)				      \
      | (((x) & 0x000000ff00000000ull) >> 8)				      \
      | (((x) & 0x00000000ff000000ull) << 8)				      \
      | (((x) & 0x0000000000ff0000ull) << 24)				      \
      | (((x) & 0x000000000000ff00ull) << 40)				      \
      | (((x) & 0x00000000000000ffull) << 56))
#else
#define bswap_64(x) \
     (__extension__						\
      ({ union { __extension__ unsigned long long int __ll;	\
                 unsigned long int __l[2]; } __w, __r;		\
         __w.__ll = (x);					\
         __r.__l[0] = bswap_32 (__w.__l[1]);			\
         __r.__l[1] = bswap_32 (__w.__l[0]);			\
         __r.__ll; }))
#endif
#endif	/* !ARCH_X86 */

#endif	/* !HAVE_BYTESWAP_H */

inline static float bswap_flt(float x) {
  union {uint32_t i; float f;} u;
  u.f = x;
  u.i = bswap_32(u.i);
  return u.f;
}

inline static double bswap_dbl(double x) {
  union {uint64_t i; double d;} u;
  u.d = x;
  u.i = bswap_64(u.i);
  return u.d;
}

inline static long double bswap_ldbl(long double x) {
  union {char d[10]; long double ld;} uin;
  union {char d[10]; long double ld;} uout;
  uin.ld = x;
  uout.d[0] = uin.d[9];
  uout.d[1] = uin.d[8];
  uout.d[2] = uin.d[7];
  uout.d[3] = uin.d[6];
  uout.d[4] = uin.d[5];
  uout.d[5] = uin.d[4];
  uout.d[6] = uin.d[3];
  uout.d[7] = uin.d[2];
  uout.d[8] = uin.d[1];
  uout.d[9] = uin.d[0];
  return uout.ld;
}

// be2me ... BigEndian to MachineEndian
// le2me ... LittleEndian to MachineEndian

#ifdef WORDS_BIGENDIAN
#define be2me_16(x) (x)
#define be2me_32(x) (x)
#define be2me_64(x) (x)
#define me2be_16(x) (x)
#define me2be_32(x) (x)
#define me2be_64(x) (x)
#define le2me_16(x) bswap_16(x)
#define le2me_32(x) bswap_32(x)
#define le2me_64(x) bswap_64(x)
#define me2le_16(x) bswap_16(x)
#define me2le_32(x) bswap_32(x)
#define me2le_64(x) bswap_64(x)
#define be2me_flt(x) (x)
#define be2me_dbl(x) (x)
#define be2me_ldbl(x) (x)
#define le2me_flt(x) bswap_flt(x)
#define le2me_dbl(x) bswap_dbl(x)
#define le2me_ldbl(x) bswap_ldbl(x)
#else
#define be2me_16(x) bswap_16(x)
#define be2me_32(x) bswap_32(x)
#define be2me_64(x) bswap_64(x)
#define me2be_16(x) bswap_16(x)
#define me2be_32(x) bswap_32(x)
#define me2be_64(x) bswap_64(x)
#define le2me_16(x) (x)
#define le2me_32(x) (x)
#define le2me_64(x) (x)
#define me2le_16(x) (x)
#define me2le_32(x) (x)
#define me2le_64(x) (x)
#define be2me_flt(x) bswap_flt(x)
#define be2me_dbl(x) bswap_dbl(x)
#define be2me_ldbl(x) bswap_ldbl(x)
#define le2me_flt(x) (x)
#define le2me_dbl(x) (x)
#define le2me_ldbl(x) (x)
#endif

#endif
