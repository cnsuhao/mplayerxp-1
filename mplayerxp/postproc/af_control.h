#ifndef __af_control_h
#define __af_control_h

/*********************************************
// Control info struct.
//
// This struct is the argument in a info call to a filter.
*/

enum {
// Argument types
    AF_CONTROL_TYPE_BOOL	=(0x0<<0),
    AF_CONTROL_TYPE_CHAR	=(0x1<<0),
    AF_CONTROL_TYPE_INT		=(0x2<<0),
    AF_CONTROL_TYPE_FLOAT	=(0x3<<0),
    AF_CONTROL_TYPE_STRUCT	=(0x4<<0),
    AF_CONTROL_TYPE_SPECIAL	=(0x5<<0), // a pointer to a function for example
    AF_CONTROL_TYPE_MASK	=(0x7<<0)
};
enum {
// Argument geometry
    AF_CONTROL_GEOM_SCALAR	=(0x0<<3),
    AF_CONTROL_GEOM_ARRAY	=(0x1<<3),
    AF_CONTROL_GEOM_MATRIX	=(0x2<<3),
    AF_CONTROL_GEOM_MASK	=(0x3<<3),
// Argument properties
    AF_CONTROL_PROP_READ	=(0x0<<5), // The argument can be read
    AF_CONTROL_PROP_WRITE	=(0x1<<5), // The argument can be written
    AF_CONTROL_PROP_SAVE	=(0x2<<5), // Can be saved
    AF_CONTROL_PROP_RUNTIME	=(0x4<<5), // Acessable during execution
    AF_CONTROL_PROP_CHANNEL	=(0x8<<5), // Argument is set per channel
    AF_CONTROL_PROP_MASK	=(0xF<<5)
};

typedef struct af_control_info_s{
  int	 def;	// Control enumrification
  char*	 name; 	// Name of argument
  char*	 info;	// Description of what it does
  int 	 flags;	// Flags as defined above	
  float	 max;	// Max and min value 
  float	 min;	// (only aplicable on float and int) 
  int	 xdim;	// 1st dimension
  int	 ydim;	// 2nd dimension (=0 for everything except matrix)
  size_t sz;	// Size of argument in bytes
  int	 ch;	// Channel number (for future use)
  any_t*  arg;	// Data (for future use)
}af_control_info_t;


/*********************************************
// Extended control used with arguments that operates on only one
// channel at the time
*/
typedef struct af_control_ext_s{
  any_t* arg;	// Argument
  int	ch;	// Chanel number
}af_control_ext_t;

/*********************************************
// Control parameters 
*/

/* The control system is divided into 3 levels 
   mandatory calls 	 - all filters must answer to all of these
   optional calls  	 - are optional
   filter specific calls - applies only to some filters
*/
enum {
    AF_CONTROL_MANDATORY	=0x10000000,
    AF_CONTROL_OPTIONAL		=0x20000000,
    AF_CONTROL_FILTER_SPECIFIC	=0x40000000
};
// MANDATORY CALLS
enum {
/* Reinitialize filter. The optional argument contains the new
   configuration in form of a af_data_t struct. If the filter does not
   support the new format the struct should be changed and CONTROL_FALSE
   should be returned. If the incoming and outgoing data streams are
   identical the filter can return CONTROL_DETACH. This will remove the
   filter. */
    AF_CONTROL_REINIT		=0x00000100|AF_CONTROL_MANDATORY,
    AF_CONTROL_SHOWCONF		=0x00000200|AF_CONTROL_MANDATORY, /* should printout how filter was configured */
// OPTIONAL CALLS

/* Called just after creation with the af_cfg for the stream in which
   the filter resides as input parameter this call can be used by the
   filter to initialize itself */
    AF_CONTROL_POST_CREATE	=0x00000100|AF_CONTROL_OPTIONAL,
    AF_CONTROL_PRE_DESTROY	=0x00000200|AF_CONTROL_OPTIONAL, // Called just before destruction of a filter
/* Commandline parameters. If there were any commandline parameters
   for this specific filter, they will be given as a char* in the
   argument */
    AF_CONTROL_COMMAND_LINE	=0x00000300|AF_CONTROL_OPTIONAL,
};
// FILTER SPECIFIC CALLS
// Basic operations: These can be ored with any of the below calls

enum {
    AF_CONTROL_SET		=0x00000000, // Set argument
    AF_CONTROL_GET		=0x00000001, // Get argument
    AF_CONTROL_INFO		=0x00000002, // Get info about the control, i.e fill in everything except argument
// Resample
    AF_CONTROL_RESAMPLE_RATE	=0x00000100|AF_CONTROL_FILTER_SPECIFIC, // Set output rate in resample
    AF_CONTROL_RESAMPLE_SLOPPY	=0x00000200|AF_CONTROL_FILTER_SPECIFIC, // Enable sloppy resampling
    AF_CONTROL_RESAMPLE_ACCURACY=0x00000300|AF_CONTROL_FILTER_SPECIFIC, // Set resampling accuracy
// Format
    AF_CONTROL_FORMAT_BPS	=0x00000400|AF_CONTROL_FILTER_SPECIFIC, // Set output format bits per sample
    AF_CONTROL_FORMAT_FMT	=0x00000500|AF_CONTROL_FILTER_SPECIFIC, // Set output format sample format
// Channels
    AF_CONTROL_CHANNELS		=0x00000600|AF_CONTROL_FILTER_SPECIFIC, // Set number of output channels in channels
    AF_CONTROL_CHANNELS_ROUTES	=0x00000700|AF_CONTROL_FILTER_SPECIFIC, // Set number of channel routes
    AF_CONTROL_CHANNELS_ROUTING	=0x00000800|AF_CONTROL_FILTER_SPECIFIC, // Set channel routing pair, arg is int[2] and ch is used
    AF_CONTROL_CHANNELS_NR	=0x00000900|AF_CONTROL_FILTER_SPECIFIC, // Set nuber of channel routing pairs, arg is int*
    AF_CONTROL_CHANNELS_ROUTER	=0x00000A00|AF_CONTROL_FILTER_SPECIFIC, // Set make af_channels into a router
// Volume
    AF_CONTROL_VOLUME_ON_OFF	=0x00000B00|AF_CONTROL_FILTER_SPECIFIC, // Turn volume control on and off, arg is int*
    AF_CONTROL_VOLUME_SOFTCLIP	=0x00000C00|AF_CONTROL_FILTER_SPECIFIC, // Turn soft clipping of the volume on and off, arg is binary
    AF_CONTROL_VOLUME_LEVEL	=0x00000D00|AF_CONTROL_FILTER_SPECIFIC, // Set volume level, arg is a float* with the volume for all the channels
    AF_CONTROL_VOLUME_PROBE	=0x00000E00|AF_CONTROL_FILTER_SPECIFIC, // Probed power level for all channels, arg is a float* 
    AF_CONTROL_VOLUME_PROBE_MAX	=0x00000F00|AF_CONTROL_FILTER_SPECIFIC, // Maximum probed power level for all channels, arg is a float* 
// Compressor/expander
    AF_CONTROL_COMP_ON_OFF	=0x00001000|AF_CONTROL_FILTER_SPECIFIC, // Turn compressor/expander on and off
    AF_CONTROL_COMP_THRESH	=0x00001100|AF_CONTROL_FILTER_SPECIFIC, // Compression/expansion threshold [dB]
    AF_CONTROL_COMP_ATTACK	=0x00001200|AF_CONTROL_FILTER_SPECIFIC, // Compression/expansion attack time [ms]
    AF_CONTROL_COMP_RELEASE	=0x00001300|AF_CONTROL_FILTER_SPECIFIC, // Compression/expansion release time [ms]
    AF_CONTROL_COMP_RATIO	=0x00001400|AF_CONTROL_FILTER_SPECIFIC, // Compression/expansion gain level [dB]
// Noise gate
    AF_CONTROL_GATE_ON_OFF	=0x00001500|AF_CONTROL_FILTER_SPECIFIC, // Turn noise gate on an off
    AF_CONTROL_GATE_THRESH	=0x00001600|AF_CONTROL_FILTER_SPECIFIC, // Noise gate threshold [dB]
    AF_CONTROL_GATE_ATTACK	=0x00001700|AF_CONTROL_FILTER_SPECIFIC, // Noise gate attack time [ms]
    AF_CONTROL_GATE_RELEASE	=0x00001800|AF_CONTROL_FILTER_SPECIFIC, // Noise gate release time [ms]
    AF_CONTROL_GATE_RANGE	=0x00001900|AF_CONTROL_FILTER_SPECIFIC, // Noise gate release range level [dB]
// Pan
    AF_CONTROL_PAN_LEVEL	=0x00001A00|AF_CONTROL_FILTER_SPECIFIC, // Pan levels, arg is a control_ext with a float* 
    AF_CONTROL_PAN_NOUT		=0x00001B00|AF_CONTROL_FILTER_SPECIFIC, // Number of outputs from pan, arg is int*
    AF_CONTROL_EQUALIZER_GAIN	=0x00001C00|AF_CONTROL_FILTER_SPECIFIC, // Set equalizer gain, arg is a control_ext with a float*
    AF_CONTROL_DELAY_LEN	=0x00001D00|AF_CONTROL_FILTER_SPECIFIC, // Delay length in ms, arg is a control_ext with a float*
// Subwoofer
    AF_CONTROL_SUB_CH		=0x00001E00|AF_CONTROL_FILTER_SPECIFIC, // Channel number which to insert the filtered data, arg in int*
    AF_CONTROL_SUB_FC		=0x00001F00|AF_CONTROL_FILTER_SPECIFIC, // Cutoff frequency [Hz] for lowpass filter, arg is float*
//
    AF_CONTROL_EXPORT_SZ	=0x00002000|AF_CONTROL_FILTER_SPECIFIC, // Export
    AF_CONTROL_ES_MUL		=0x00002100|AF_CONTROL_FILTER_SPECIFIC, // ExtraStereo Multiplier
    AF_CONTROL_PLAYBACK_SPEED	=0x00002500|AF_CONTROL_FILTER_SPECIFIC,
    AF_CONTROL_SCALETEMPO_AMOUNT=0x00002600|AF_CONTROL_FILTER_SPECIFIC
};

enum { AF_NCH=8 };

/* Equalizer plugin header file defines struct used for setting or
   getting the gain of a specific channel and frequency */

typedef struct equalizer_s
{
  float gain;   	// Gain in dB  -15 - 15 
  int	channel; 	// Channel number 0 - 5 
  int 	band;		// Frequency band 0 - 9
}equalizer_t;

/* The different frequency bands are:
nr.    	center frequency
0  	31.25 Hz
1 	62.50 Hz
2	125.0 Hz
3	250.0 Hz
4	500.0 Hz
5	1.000 kHz
6	2.000 kHz
7	4.000 kHz
8	8.000 kHz
9       16.00 kHz
*/

#endif /*__af_control_h */
