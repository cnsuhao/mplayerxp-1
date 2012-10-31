#ifndef __AUDIO_OUT_H
#define __AUDIO_OUT_H 1
#include "mp_config.h"
#include "xmp_enums.h"
/** Text description of AO-driver */
typedef struct ao_info_s
{
        const char *name;	/**< driver name ("alsa driver") */
        const char *short_name; /**< short name (for config strings) ("alsa") */
        const char *author;	/**< author ("Aaron Holtzman <aholtzma@ess.engr.uvic.ca>") */
        const char *comment;	/**< any additional comments */
} ao_info_t;

/** Global data used by mplayerxp and plugins */
typedef struct ao_data_s
{
  char*		subdevice;
  unsigned	samplerate;	/**< rate of samples in Hz */
  unsigned	channels;	/**< number of audio channels */
  unsigned	format;		/**< format of audio samples */
  unsigned	bps;		/**< bytes per second */
  unsigned	outburst;	/**< outburst */
  unsigned	buffersize;	/**< size of audio buffer */
  float		pts;		/**< PTS of audio buffer  */
  any_t*	priv;
} ao_data_t;

/** AO-driver interface */
typedef struct ao_functions_s
{
	const ao_info_t *info;	/**< text-info about this driver */

	/** Control interface
	 * @param cmd	command. See AOCONTROL_** for detail
	 * @param arg	argument associated with command
	 * @return	CONTROL_OK if success CONTROL_FALSE CONTROL_ERROR CONTROL_NA otherwise
	 **/
	ControlCodes (* __FASTCALL__ control)(ao_data_t*,int cmd,long arg);

	/** Preinitializes driver
	 * @param flag	currently unused
	 * @return	1 on successful initialization, 0 on error.
	**/
	int (* __FASTCALL__ init)(ao_data_t*,unsigned flags);

        /** Configures the audio driver.
	 * @param rate		specifies samplerate in Hz
	 * @param channels	specifies number of channels (1 - mono, 2 - stereo, 6 - surround)
	 * @param format	specifies format of audio samples (see AFMT_* for detail)
	 * @return		1 on successful configuration, 0 on error.
	 **/
	int (* __FASTCALL__ configure)(ao_data_t*,unsigned rate,unsigned channels,unsigned format);

	/** Closes driver. Should restore the original state of the system.
	 **/
	void (*uninit)(ao_data_t*);

	/** Stops playing and empties buffers (for seeking/pause) **/
	void (*reset)(ao_data_t*);

	/** Returns how many bytes can be played without blocking **/
	unsigned (*get_space)(ao_data_t*);

	/** Plays decoded (PCM) audio buffer
	  * @param data		buffer with PCM data
	  * @param len		length of byffer in bytes
	  * @param flags	currently unused
	  * return		number of bytes which were copied into audio card
	**/
	unsigned (* __FASTCALL__ play)(ao_data_t*,any_t* data,unsigned len,unsigned flags);

	/** Returns delay in seconds between first and last sample in buffer **/
	float (*get_delay)(ao_data_t*);

	/** Stops playing, keep buffers (for pause) */
	void (*pause)(ao_data_t*);

	/** Resumes playing, after audio_pause() */
	void (*resume)(ao_data_t*);
} ao_functions_t;

extern const ao_functions_t* audio_out_drivers[]; /**< NULL terminated array of all drivers */

enum {
    AOCONTROL_SET_DEVICE	=1, /**< Sets new audio device (example: /dev/dsp2) */
    AOCONTROL_GET_DEVICE	=2, /**< Query current audio device (example: /dev/dsp) */
    AOCONTROL_QUERY_FORMAT	=3, /**< Test for support of given format */
    AOCONTROL_QUERY_CHANNELS	=4, /**< Test for support of a given number of channels */
    AOCONTROL_QUERY_RATE	=5, /**< Test for support of given rate */
    AOCONTROL_GET_VOLUME	=6, /**< Query volume level */
    AOCONTROL_SET_VOLUME	=7 /**< Sets new volume level */
};
typedef struct ao_control_vol_s {
	float left;
	float right;
} ao_control_vol_t;

/* prototypes */
extern char *		 __FASTCALL__ ao_format_name(int format);
extern int		 __FASTCALL__ ao_format_bits(int format);

extern void		ao_print_help( void );
extern const ao_functions_t* __FASTCALL__ ao_register(const char *driver_name);
extern const ao_info_t*	ao_get_info( void );
extern ao_data_t*	 __FASTCALL__ ao_init(unsigned flags,const char *subdevice);
extern int		 __FASTCALL__ ao_configure(ao_data_t* priv,unsigned rate,unsigned channels,unsigned format);
extern void		ao_uninit(ao_data_t* priv);
extern void		ao_reset(ao_data_t* priv);
extern unsigned		ao_get_space(ao_data_t* priv);
extern unsigned		 __FASTCALL__ ao_play(ao_data_t* priv,any_t* data,unsigned len,unsigned flags);
extern float		ao_get_delay(ao_data_t* priv);
extern void		ao_pause(ao_data_t* priv);
extern void		ao_resume(ao_data_t* priv);
extern int		 __FASTCALL__ ao_control(ao_data_t* priv,int cmd,long arg);
#endif
