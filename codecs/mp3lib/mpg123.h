/*
 * mpg123 defines
 * used source: musicout.h from mpegaudio package
 */
#ifndef MPG123_H_INCLUDED
#define MPG123_H_INCLUDED 1
#include <stdint.h>
#include "../config.h"

#if defined( ARCH_X86 ) || defined(ARCH_X86_64)
#define CAN_COMPILE_X86_ASM
#endif

#ifndef M_PI
#define M_PI		3.141592653589793238462
#endif
#ifndef M_SQRT2
#define M_SQRT2		1.414213562373095048802
#endif
#define REAL_IS_FLOAT
#define NEW_DCT9

# define REAL_MUL(x, y)		((x) * (y))

#undef MPG123_REMOTE           /* Get rid of this stuff for Win32 */

typedef float real;

#define         FALSE                   0
#define         TRUE                    1

#define         MAX_NAME_SIZE           81
#define         SBLIMIT                 32
#define         SCALE_BLOCK             12
#define         SSLIMIT                 18

#define         MPG_MD_STEREO           0
#define         MPG_MD_JOINT_STEREO     1
#define         MPG_MD_DUAL_CHANNEL     2
#define         MPG_MD_MONO             3

/* #define MAXOUTBURST 32768 */

/* Pre Shift fo 16 to 8 bit converter table */
#define AUSHIFT (3)

struct al_table
{
  short bits;
  short d;
};

struct xing_ext
{
  int	   is_xing;
  unsigned nframes;
  unsigned nbytes;
  int	   scale;
  unsigned char	toc[100]; /* Table Of Context */
};

typedef int  (*synth_func_t)(real *,int,unsigned char *,int *);
typedef void (*dct12_func_t)(real *,real *,real *,real *,real *);
typedef void (*dct36_func_t)(real *,real *,real *,real *,real *);
typedef void (*dct64_func_t)(real *,real *,real *);

struct frame {
    struct al_table *alloc;
    synth_func_t synth;
    int (*synth_mono)(real *,unsigned char *,int *);

    dct12_func_t dct12;
    dct36_func_t dct36;
    dct64_func_t dct64;

    int stereo;
    int jsbound;
    int single;
    int II_sblimit;
    int down_sample_sblimit;
         int lsf;
         int mpeg25;
    int down_sample;
         int header_change;
    int lay;
    int error_protection;
    int bitrate_index;
    uint32_t sampling_frequency;
    int padding;
    int extension;
    int mode;
         int mode_ext;
    int copyright;
         int original;
         int emphasis;
         uint32_t framesize; /* computed framesize */
    struct xing_ext xing;
};
extern struct frame fr;

struct gr_info_s {
      int scfsi;
      unsigned part2_3_length;
      unsigned big_values;
      unsigned scalefac_compress;
      unsigned block_type;
      unsigned mixed_block_flag;
      unsigned table_select[3];
      unsigned subblock_gain[3];
      unsigned maxband[3];
      unsigned maxbandl;
      unsigned maxb;
      unsigned region1start;
      unsigned region2start;
      unsigned preflag;
      unsigned scalefac_scale;
      unsigned count1table_select;
      real* full_gain[3];
      real* pow2gain;
};

struct III_sideinfo
{
  unsigned main_data_begin;
  unsigned private_bits;
  struct {
         struct gr_info_s gr[2];
  } ch[2];
};

extern uint32_t freqs[9];
extern real muls[27][64];
extern real decwin[(512+32)];
extern real *pnts[];

extern int bitindex;
extern unsigned char *wordpointer;
extern int bitsleft;
extern uint32_t getbits(short number_of_bits);
extern unsigned int get1bit(void);
extern uint32_t getbits_fast(short number_of_bits);
extern void set_pointer(int32_t backstep);

extern unsigned char *pcm_sample;	/* outbuffer address */
extern int pcm_point;			/* outbuffer offset */

extern void make_decode_tables(float scaleval);

extern real COS9[9];
extern real COS6_1,COS6_2;
extern real tfcos36[9];
extern real tfcos12[3];
extern real cos9[3],cos18[3];

extern void init_layer2(void);
extern void init_layer3(int);

extern int do_layer1(struct frame *fr,int outmode);
extern int do_layer2(struct frame *fr,int single);
extern int do_layer3(struct frame *fr,int single);

extern int synth_1to1_mono_32(real *bandPtr,unsigned char *samples,int *pnt);
extern int synth_1to1_mono2stereo_32(real *bandPtr,unsigned char *samples,int *pnt);
extern int synth_1to1_l_32(real *bandPtr,int channel,unsigned char *out,int *pnt);
extern int synth_1to1_r_32(real *bandPtr,int channel,unsigned char *out,int *pnt);
extern int synth_1to1_32(real *bandPtr,int channel,unsigned char *out,int *pnt);
extern int synth_1to1_3dnow32(real *bandPtr,int channel,unsigned char *out,int *pnt);

extern void dct12(real *in,real *rawout1,real *rawout2,register real *wi,register real *ts);

extern void dct36_3dnow(real *,real *,real *,real *,real *);
extern void dct36(real *,real *,real *,real *,real *);

extern void dct64(real *a,real *b,real *c);
extern void dct64_3dnow(real *a,real *b,real *c);

#endif
