/*
   PVector - Portable vectoring implementation of parallel computing
*/
#define HAVE_INT_PVECTOR 1
#define HAVE_F32_PVECTOR 1

#ifndef PVECTOR_RENAME
#error "Never include this file directly! Include pvector_inc.h instead"
#endif

#ifdef OPTIMIZE_AVX
#define OPTIMIZE_AES
#endif
#ifdef OPTIMIZE_AES
#define OPTIMIZE_SSE4
#endif
#ifdef OPTIMIZE_SSE4
#define OPTIMIZE_SSSE3
#endif
#ifdef OPTIMIZE_SSSE3
#define OPTIMIZE_SSE3
#endif
#ifdef OPTIMIZE_SSE3
#define OPTIMIZE_SSE2
#endif
#ifdef OPTIMIZE_SSE2
#define OPTIMIZE_SSE
#endif
#ifdef OPTIMIZE_SSE
#define OPTIMIZE_3DNOW
#define OPTIMIZE_MMX2
#endif
#if defined( OPTIMIZE_MMX2 )
#define OPTIMIZE_MMX
#endif

#if defined( OPTIMIZE_MMX )
#include "pvector_int_x86.h"
#else
//#warning "pvector's generic version isn't yet ready"
#undef HAVE_INT_PVECTOR
#endif
#if defined( OPTIMIZE_3DNOW )
#include "pvector_f32_x86.h"
#else
//#warning "pvector's generic version isn't yet ready"
#undef HAVE_F32_PVECTOR
#endif

#undef IVEC_ALIGNED
#define IVEC_ALIGNED(p) ((((long)((any_t*)(p)))&(__IVEC_SIZE-1))==0)
#undef F32VEC_ALIGNED
#define F32VEC_ALIGNED(p) ((((long)((any_t*)(p)))&(__F32VEC_SIZE-1))==0)

/*
  ABBREVIATION:
  s8   - signed   int8_t
  u8   - unsigned uint8_t
  s16  - signed   int16_t
  u16  - unsigned uint16_t
  s32  - signed   int32_t
  u32  - unsigned uint32_t

  This interface defines:
  __IVEC_SIZE       size of vector in bytes
  __ivec            type of vector

  functions:
  ==========
  LOAD/STORE engine:
  ------------------
  void   _ivec_empty(void);                   // Empty MMX state
  void   _ivec_sfence(void);                  // Store fence
  void   _ivec_prefetch(void const *__P);     // Prefetch memory for reading
  void   _ivec_prefetchw(void const *__P);    // Prefetch memory for writing

  __ivec _ivec_loadu(void const *__P);        // load unaligned data into vector
  __ivec _ivec_loada(void const *__P);        // load aligned data into vector
  __ivec _ivec_setzero(void);                 // load ZERO into vector
  __ivec _ivec_setff(void);                   // load FF...FF into vector
  __ivec _ivec_broadcast_u8(unsigned char u8);   // fill vector with u8...u8 values
  __ivec _ivec_broadcast_u16(unsigned short u16);// fill vector with u16...u16 values
  __ivec _ivec_broadcast_u32(unsigned int u32);// fill vector with u16...u16 values
  void   _ivec_storeu(any_t*__P, __ivec src); // store vector into unaligned memory
  void   _ivec_storea(any_t*__P, __ivec src); // store vector into aligned memory
  void   _ivec_stream(any_t*__P, __ivec src); // store vector into memory across CPU's cache

  LOGICAL engine:
  ---------------
  __ivec _ivec_or(__ivec s1, __ivec s2);       // Logical OR
  __ivec _ivec_and(__ivec s1, __ivec s2);      // Logical AND
  __ivec _ivec_andnot(__ivec s1, __ivec s2);   // Logical NOT S1 AND S2
  __ivec _ivec_xor(__ivec s1, __ivec s2);      // Logical XOR
  __ivec _ivec_not(__ivec s);                  // Logical NOT

  SHIFT engine:
  -------------
  __ivec _ivec_sll_s16(__ivec s1,__ivec s2);    // Shift Logical Left S16
  __ivec _ivec_sll_s16_imm(__ivec s1,int count);// Shift Logical Left S16
  __ivec _ivec_sll_s32(__ivec s1,__ivec s2);    // Shift Logical Left S32
  __ivec _ivec_sll_s32_imm(__ivec s1,int count);// Shift Logical Left S32
  __ivec _ivec_sll_s64(__ivec s1,__ivec s2);    // Shift Logical Left S64
  __ivec _ivec_sll_s64_imm(__ivec s1,int count);// Shift Logical Left S64
  __ivec _ivec_sra_s16(__ivec s1,__ivec s2);    // Shift Arithmetical Right S16
  __ivec _ivec_sra_s16_imm(__ivec s1,int count);// Shift Arithmetical Right S16
  __ivec _ivec_sra_s32(__ivec s1,__ivec s2);    // Shift Arithmetical Right S32
  __ivec _ivec_sra_s32_imm(__ivec s1,int count);// Shift Arithmetical Right S32
  __ivec _ivec_srl_s16(__ivec s1,__ivec s2);    // Shift Logical Right S16
  __ivec _ivec_srl_s16_imm(__ivec s1,int count);// Shift Logical Right S16
  __ivec _ivec_srl_s32(__ivec s1,__ivec s2);    // Shift Logical Right S32
  __ivec _ivec_srl_s32_imm(__ivec s1,int count);// Shift Logical Right S32
  __ivec _ivec_srl_s64(__ivec s1,__ivec s2);    // Shift Logical Right S64
  __ivec _ivec_srl_s64_imm(__ivec s1,int count);// Shift Logical Right S64

  COMPARE engine:
  ---------------
  __ivec _ivec_cmpgt_s8(__ivec s1, __ivec s2); // Compare S1>S2
  __ivec _ivec_cmpeq_s8(__ivec s1, __ivec s2); // Compare S1==S2
  __ivec _ivec_cmpgt_s16(__ivec s1, __ivec s2);// Compare S1>S2
  __ivec _ivec_cmpeq_s16(__ivec s1, __ivec s2);// Compare S1==S2
  __ivec _ivec_cmpgt_s32(__ivec s1, __ivec s2);// Compare S1>S2
  __ivec _ivec_cmpeq_s32(__ivec s1, __ivec s2);// Compare S1==S2
  // Blend_s8 logic:
  //    if(Mask[7] == 1) dest[0..7] = src2[0..7]; else dest[0..7] = src[0..7]...
  //    if(Mask[63] == 1) Dest[56..63] = Source[56..63]...;
  __ivec _ivec_blend_u8(__ivec src1,__ivec src2,__ivec mask); 

  CONVERTION engine:
  ------------------
  __ivec _ivec_interleave_lo_u8(__ivec s1, _ivec_ s2);
  __ivec _ivec_interleave_hi_u8(__ivec s1, _ivec_ s2);
  __ivec _ivec_interleave_lo_u16(__ivec s1, _ivec_ s2);
  __ivec _ivec_interleave_hi_u16(__ivec s1, _ivec_ s2);
  __ivec _ivec_interleave_lo_u32(__ivec s1, _ivec_ s2);
  __ivec _ivec_interleave_hi_u32(__ivec s1, _ivec_ s2);

  __ivec _ivec_u16_from_u8(__ivec s,__ivec *hipart); // Convert ivec from U8 to U16
  __ivec _ivec_u32_from_u16(__ivec s,__ivec *hipart);// Convert ivec from U16 to U32
  __ivec _ivec_s16_from_s8(__ivec s,__ivec *hipart); // Convert ivec from S8 to S16
  __ivec _ivec_s32_from_s16(__ivec s,__ivec *hipart);// Convert ivec from S16 to S32

  __ivec _ivec_scale_u16_from_u8(__ivec s,__ivec *hipart); // Convert ivec from U8 to U16 and shift left on 8-bit
  __ivec _ivec_scale_u32_from_u16(__ivec s,__ivec *hipart);// Convert ivec from U16 to U32 and shift left on 16-bit

  __ivec _ivec_s16_from_s32(__ivec s1,__ivec s2);   // Convert from S32 to S16
  __ivec _ivec_s8_from_s16(__ivec s1,__ivec s2);    // Convert from S16 to S8
  __ivec _ivec_u8_from_u16(__ivec s1,__ivec s2);    // Convert from U16 to U8

  __ivec _ivec_scale_s16_from_s32(__ivec s1,__ivec s2);   // Convert from S32 to S16 and shift right on 16-bit
  __ivec _ivec_scale_s8_from_s16(__ivec s1,__ivec s2);    // Convert from S16 to S8 and shift right on 8-bit
  __ivec _ivec_scale_u8_from_u16(__ivec s1,__ivec s2);    // Convert from U16 to U8 and shift right on 8-bit

  ARITHMETIC engine:
  ------------------
  __ivec _ivec_add_s8(__ivec s1,__ivec s2);        // Add S8
  __ivec _ivec_add_s16(__ivec s1,__ivec s2);       // Add S16
  __ivec _ivec_add_s32(__ivec s1,__ivec s2);       // Add S32
  __ivec _ivec_sadd_s8(__ivec s1,__ivec s2);       // Add S8 with saturation
  __ivec _ivec_sadd_s16(__ivec s1,__ivec s2);      // Add S16 with saturation
  __ivec _ivec_sadd_u8(__ivec s1,__ivec s2);       // Add U8 with saturation
  __ivec _ivec_sadd_u16(__ivec s1,__ivec s2);      // Add U16 with saturation

  __ivec _ivec_sub_s8(__ivec s1,__ivec s2);        // Substract S8
  __ivec _ivec_sub_s16(__ivec s1,__ivec s2);       // Substract S16
  __ivec _ivec_sub_s32(__ivec s1,__ivec s2);       // Substract S32
  __ivec _ivec_ssub_s8(__ivec s1,__ivec s2);       // Substract S8 with saturation
  __ivec _ivec_ssub_s16(__ivec s1,__ivec s2);      // Substract S16 with saturation
  __ivec _ivec_ssub_u8(__ivec s1,__ivec s2);       // Substract U8 with saturation
  __ivec _ivec_ssub_u16(__ivec s1,__ivec s2);      // Substract U16 with saturation

  __ivec _ivec_mullo_s16(__ivec s1,__ivec s2);     // Multiply low part S16
  __ivec _ivec_mulhi_s16(__ivec s1,__ivec s2);     // Multiply high part S16
*/

/* ========================= FLOAT32 Support ================================ */

/*
  ABBREVIATION:
  f32   - float32_t

  This interface defines:
  __F32VEC_SIZE       size of vector in bytes
  __f32vec            type of vector

  functions:
  ==========
  LOAD/STORE engine:
  ------------------
  __f32vec _f32vec_loadu(float const *__P);          // load unaligned data into vector
  __f32vec _f32vec_loada(float const *__P);          // load aligned data into vector
  __f32vec _f32vec_setzero(void);                    // load ZERO into vector
  __f32vec _f32vec_broadcast(float f32);             // fill vector with f32...f32 values
  void   _f32vec_storeu(float *__P, __f32vec src); // store vector into unaligned memory
  void   _f32vec_storea(float *__P, __f32vec src); // store vector into aligned memory
  void   _f32vec_stream(float *__P, __f32vec src); // store vector into memory across CPU's cache

  CONVERTION engine:
  ------------------
  __f32vec _f32vec_from_s32u(void const * src);		// convert s32 to f32 unaligned vector 
  void     _f32vec_to_s32u(any_t*dst,__f32vec src);	// convert f32 to s32 unaligned vector
  __f32vec _f32vec_from_s32a(void const * src);		// convert s32 to f32 aligned vector
  void     _f32vec_to_s32a(any_t*dst,__f32vec src);	// convert f32 to s32 aligned vector
  void     _f32vec_to_s32_stream(any_t*dst,__f32vec src);// convert f32 to s32 aligned vector

  ARITHMETIC engine:
  ------------------
  __f32vec _f32vec_add(__f32vec f1, __f32vec f2);	// add: f1+f2
  __f32vec _f32vec_sub(__f32vec f1, __f32vec f2);	// sub: f1-f2
  __f32vec _f32vec_mul(__f32vec f1, __f32vec f2);	// mul: f1*f2
  __f32vec _f32vec_div(__f32vec f1, __f32vec f2);	// div: f1/f2

  COMPARE engine:
  ---------------
  __f32vec _f32vec_min(__f32vec f1, __f32vec f2);	// MIN(f1,f2)
  __f32vec _f32vec_max(__f32vec f1, __f32vec f2);	// MAX(f1,f2)
  __f32vec _f32vec_clamp(__f32vec f1, __f32vec minval,__f32vec maxval); // CLAMP(f1,minval,maxval);
*/
