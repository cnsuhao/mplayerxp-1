/*=============================================================================
//
//  This software has been released under the terms of the GNU General Public
//  license. See http://www.gnu.org/copyleft/gpl.html for details.
//
//  Copyright 2002 Anders Johansson ajh@atri.curtin.edu.au
//
//=============================================================================
*/

/* This file contains the resampling engine, the sample format is
   controlled by the FORMAT parameter, the filter length by the L
   parameter and the resampling type by UP and DN. This file should
   only be included by af_resample.c
*/

#undef L
#undef SHIFT
#undef FORMAT
#undef FIR
#undef ADDQUE

/* The lenght Lxx definition selects the length of each poly phase
   component. Valid definitions are L8 and L16 where the number
   defines the nuber of taps. This definition affects the
   computational complexity, the performance and the memory usage.
*/

/* The FORMAT_x parameter selects the sample format type currently
   float and int16 are supported. Thes two formats are selected by
   defining eiter FORMAT_F or FORMAT_I. The advantage of using float
   is that the amplitude and therefore the SNR isn't affected by the
   filtering, the disadvantage is that it is a lot slower.
*/

#if defined(FORMAT_I)
#define SHIFT >>16
#define FORMAT int16_t
#if defined( ARCH_X86 ) || defined(ARCH_X86_64)
#define FIR(x,w,y) {y[0]=FIR_i16(x,w);}
#endif
#else
#define SHIFT
#define FORMAT float
#if defined( ARCH_X86 ) || defined(ARCH_X86_64)
#define FIR(x,w,y) {y[0]=FIR_f32(x,w);}
#endif
#endif

// Short filter
#if defined(L8)

#define L	8	// Filter length
// Unrolled loop to speed up execution
#if !(defined( ARCH_X86 ) || defined(ARCH_X86_64))
#define FIR(x,w,y) \
  (y[0])  = ( w[0]*x[0]+w[1]*x[1]+w[2]*x[2]+w[3]*x[3] \
            + w[4]*x[4]+w[5]*x[5]+w[6]*x[6]+w[7]*x[7] ) SHIFT
#endif


#else  /* L8/L16 */

#define L	16
// Unrolled loop to speed up execution 
#if !(defined( ARCH_X86 ) || defined(ARCH_X86_64))
#define FIR(x,w,y) \
  y[0] = ( w[0] *x[0] +w[1] *x[1] +w[2] *x[2] +w[3] *x[3] \
         + w[4] *x[4] +w[5] *x[5] +w[6] *x[6] +w[7] *x[7] \
         + w[8] *x[8] +w[9] *x[9] +w[10]*x[10]+w[11]*x[11] \
         + w[12]*x[12]+w[13]*x[13]+w[14]*x[14]+w[15]*x[15] ) SHIFT
#endif
#endif /* L8/L16 */

// Macro to add data to circular que 
#define ADDQUE(xi,xq,in)\
  xq[xi]=xq[(xi)+L]=*(in);\
  xi=((xi)-1)&(L-1);

#if defined(UP)

  uint32_t		inc   = s->up/s->dn;
  uint32_t		level = s->up%s->dn;
  register FORMAT*	w     = s->w;

  // Index current channel
  while(ci--){
    // Temporary pointers
    register FORMAT*	x     = s->xq[ci];
    register FORMAT*	in    = ((FORMAT*)c->audio)+ci;
    register FORMAT*	out   = ((FORMAT*)l->audio)+ci;
    FORMAT* 		end   = in+ns; // Block loop end
    wi = s->wi; xi = s->xi;

    while(in < end){
      register uint32_t	i = inc;
      if(wi<level) i++;

      ADDQUE(xi,x,in);
      in+=nch;
      while(i--){
	// Run the FIR filter
	FIR((&x[xi]),(&w[wi*L]),out);
	len++; out+=nch;
	// Update wi to point at the correct polyphase component
	wi=(wi+dn)%up;
      }
    }

  }
#endif /* UP */

#if defined(DN) /* DN */
  uint32_t		inc   = s->dn/s->up;
  uint32_t		level = s->dn%s->up;
  FORMAT*		w     = s->w;

  // Index current channel
  while(ci--){
    // Temporary pointers
    register FORMAT*	x     = s->xq[ci];
    register FORMAT*	in    = ((FORMAT*)c->audio)+ci;
    register FORMAT*	out   = ((FORMAT*)l->audio)+ci;
    register FORMAT* 	end   = in+ns;    // Block loop end
    i = s->i; wi = s->wi; xi = s->xi;

    while(in < end){

      ADDQUE(xi,x,in);
      in+=nch;
      if((--i)<=0){
	// Run the FIR filter
	FIR((&x[xi]),(&w[wi*L]),out);
	len++;	out+=nch;
	// Update wi to point at the correct polyphase component
	wi=(wi+dn)%up;
	// Insert i number of new samples in queue
	i = inc;
	if(wi<level) i++;
      }
    }
  }
  s->i = i;
#endif /* DN */
