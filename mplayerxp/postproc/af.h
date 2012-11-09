#ifndef __AF_H__
#define __AF_H__

#include <stdio.h>

#include "mp_config.h"
#include "af_control.h"
#include "xmpcore/xmp_enums.h"
#include "xmpcore/mp_aframe.h"

struct af_instance_s;

// Fraction, used to calculate buffer lengths
typedef struct frac_s
{
  int n; // Numerator
  int d; // Denominator
} frac_t;

// Flags used for defining the behavior of an audio filter
enum {
    AF_FLAGS_REENTRANT		=0x00000000,
    AF_FLAGS_NOT_REENTRANT	=0x00000001
};
/* Audio filter information not specific for current instance, but for
   a specific filter */
typedef struct af_info_s
{
  const char *info;
  const char *name;
  const char *author;
  const char *comment;
  const unsigned flags;
  MPXP_Rc (* __FASTCALL__ open)(struct af_instance_s* vf);
} af_info_t;

// Linked list of audio filters
typedef struct af_instance_s
{
  const af_info_t* info;
  MPXP_Rc (* __FASTCALL__ control)(struct af_instance_s* af, int cmd, any_t* arg);
  void (* __FASTCALL__ uninit)(struct af_instance_s* af);
  mp_aframe_t* (* __FASTCALL__ play)(struct af_instance_s* af, mp_aframe_t* data,int final);
  any_t* setup;	  // setup data for this specific instance and filter
  mp_aframe_t* data; // configuration for outgoing data stream
  struct af_instance_s* next;
  struct af_instance_s* prev;
  any_t*parent;
  double delay; // Delay caused by the filter [ms]
  frac_t mul; /* length multiplier: how much does this instance change
		 the length of the buffer. */
}af_instance_t;

// Initialization flags
extern int* af_cpu_speed;
enum {
    AF_INIT_AUTO	=0x00000000,
    AF_INIT_SLOW	=0x00000001,
    AF_INIT_FAST	=0x00000002,
    AF_INIT_FORCE	=0x00000003,
    AF_INIT_TYPE_MASK	=0x00000003,

    AF_INIT_INT		=0x00000000,
    AF_INIT_FLOAT	=0x00000004,
    AF_INIT_FORMAT_MASK	=0x00000004
};
// Default init type
#ifndef AF_INIT_TYPE
#if defined(HAVE_SSE) || defined(HAVE_3DNOW)
static inline int AF_INIT_TYPE(void) { return (af_cpu_speed?*af_cpu_speed:AF_INIT_FAST); }
#else
static inline int AF_INIT_TYPE(void) { return (af_cpu_speed?*af_cpu_speed:AF_INIT_SLOW); }
#endif
#endif

// Configuration switches
typedef struct af_cfg_s{
  int force;	// Initialization type
  char* list;	/* list of names of filters that are added to filter
		   list during first initialization of stream */
}af_cfg_t;

// Current audio stream
typedef struct af_stream_s
{
  // The first and last filter in the list
  af_instance_t* first;
  af_instance_t* last;
  // Storage for input and output data formats
  mp_aframe_t input;
  mp_aframe_t output;
  // Configuration for this stream
  af_cfg_t cfg;
  any_t*parent;
}af_stream_t;


/*********************************************
// Export functions
*/

af_stream_t *af_new(any_t*_parent);

/* Initialize the stream "s". This function creates a new filter list
   if necessary according to the values set in input and output. Input
   and output should contain the format of the current movie and the
   formate of the preferred output respectively. The function is
   reentrant i.e. if called with an already initialized stream the
   stream will be reinitialized. If the binary parameter
   "force_output" is set, the output format will be converted to the
   format given in "s", otherwise the output format in the last filter
   will be copied "s". The return value is 0 if success and -1 if
   failure */
int af_init(af_stream_t* s, int force_output);

// Uninit and remove all filters
void af_uninit(af_stream_t* s);

/* Reinitializes all filters downstream from the filter given in the
   argument the return value is MPXP_Ok if success and MPXP_Error if
   failure */
int af_reinit(af_stream_t* s, af_instance_t* af);

/* Add filter during execution. This function adds the filter "name"
   to the stream s. The filter will be inserted somewhere nice in the
   list of filters. The return value is a pointer to the new filter,
   If the filter couldn't be added the return value is NULL. */
af_instance_t* af_add(af_stream_t* s, char* name);

// Uninit and remove the filter "af"
void af_remove(af_stream_t* s, af_instance_t* af);

/* Find filter in the dynamic filter list using it's name This
   function is used for finding already initialized filters */
af_instance_t* __FASTCALL__ af_get(af_stream_t* s, char* name);

// Filter data chunk through the filters in the list
mp_aframe_t* __FASTCALL__ af_play(af_stream_t* s, mp_aframe_t* data);

// send control to all filters, starting with the last until
// one accepts the command with MPXP_Ok.
// Returns true if accepting filter was found.
MPXP_Rc __FASTCALL__ af_control_any_rev (af_stream_t* s, int cmd, any_t* arg);

/* Calculate how long the output from the filters will be given the
   input length "len". The calculated length is >= the actual
   length */
int __FASTCALL__ af_outputlen(af_stream_t* s, int len);

/* Calculate how long the input to the filters should be to produce a
   certain output length, i.e. the return value of this function is
   the input length required to produce the output length "len". The
   calculated length is <= the actual length */
int __FASTCALL__ af_inputlen(af_stream_t* s, int len);

/* Calculate the total delay caused by the filters */
double __FASTCALL__ af_calc_delay(af_stream_t* s);

// Helper functions and macros used inside the audio filters

/* Helper function called by the macro with the same name only to be
   called from inside filters */
int __FASTCALL__ af_resize_local_buffer(af_instance_t* af,unsigned len);

/* Helper function used to calculate the exact buffer length needed
   when buffers are resized. The returned length is >= than what is
   needed */
unsigned __FASTCALL__ af_lencalc(frac_t mul, mp_aframe_t* data);

/* Helper function used to convert to gain value from dB. Returns
   MPXP_Ok if of and MPXP_Error if fail */
int __FASTCALL__ af_from_dB(int n, float* in, float* out, float k, float mi, float ma);
/* Helper function used to convert from gain value to dB. Returns
   MPXP_Ok if of and MPXP_Error if fail */
int __FASTCALL__ af_to_dB(int n, float* in, float* out, float k);
/* Helper function used to convert from ms to sample time*/
int __FASTCALL__ af_from_ms(int n, float* in, int* out, int rate, float mi, float ma);
/* Helper function used to convert from sample time to ms */
int __FASTCALL__ af_to_ms(int n, int* in, float* out, int rate); 
/* Helper function for testing the output format */
int __FASTCALL__ af_test_output(struct af_instance_s* af, mp_aframe_t* out);

/** Print a list of all available audio filters */
void af_help(void);

/* returns 1 if first filter requires (or ao_driver supports) fmt */
MPXP_Rc __FASTCALL__ af_query_fmt (af_stream_t* s,mpaf_format_e fmt);
/* returns 1 if first filter requires (or ao_driver supports) rate */
MPXP_Rc __FASTCALL__ af_query_rate (af_stream_t* s,unsigned rate);
/* returns 1 if first filter requires (or ao_driver supports) nch */
MPXP_Rc __FASTCALL__ af_query_channels (af_stream_t* s,unsigned nch);

/* print out configuration of filter's chain */
extern void af_showconf(af_instance_t *first);

/* Memory reallocation macro: if a local buffer is used (i.e. if the
   filter doesn't operate on the incoming buffer this macro must be
   called to ensure the buffer is big enough. */
static inline int RESIZE_LOCAL_BUFFER(af_instance_t* a, mp_aframe_t* d) {
    unsigned len=af_lencalc(a->mul,d);
    return ((unsigned)a->data->len < len)?af_resize_local_buffer(a,len):MPXP_Ok;
}

/* Some other useful macro definitions*/
#ifndef min
#define min(a,b)(((a)>(b))?(b):(a))
#endif

#ifndef max
#define max(a,b)(((a)>(b))?(a):(b))
#endif

#ifndef clamp
#define clamp(a,min,max) (((a)>(max))?(max):(((a)<(min))?(min):(a)))
#endif

#ifndef sign
#define sign(a) (((a)>0)?(1):(-1))
#endif

#ifndef lrnd
#define lrnd(a,b) ((b)((a)>=0.0?(a)+0.5:(a)-0.5))
#endif

#endif /* __aop_h__ */


