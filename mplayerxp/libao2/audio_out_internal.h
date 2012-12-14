#ifndef __AO_INTERNAL_H_INCLUDED
#define __AO_INTERNAL_H_INCLUDED 1
#include "mp_config.h"

#include <string>

namespace mpxp {
    /** AO-driver interface */
    class AO_Interface : public Opaque {
	public:
	    AO_Interface(const std::string& _subdevice):subdevice(_subdevice) {}
	    virtual ~AO_Interface() {}

	    /** Preinitializes driver
	      * @param flag	currently unused
	      * @return	1 on successful initialization, 0 on error.
	     **/
	    virtual MPXP_Rc	open(unsigned flags) = 0;
	    /** Configures the audio driver.
	      * @param rate	specifies samplerate in Hz
	      * @param channels	specifies number of channels (1 - mono, 2 - stereo, 6 - surround)
	      * @param format	specifies format of audio samples (see AFMT_* for detail)
	      * @return		1 on successful configuration, 0 on error.
	     **/
	    virtual MPXP_Rc	configure(unsigned rate,unsigned channels,unsigned format) = 0;

	    virtual unsigned	samplerate() const = 0;	/**< rate of samples in Hz */
	    virtual unsigned	channels() const = 0;	/**< number of audio channels */
	    virtual unsigned	format() const = 0;	/**< format of audio samples */
	    virtual unsigned	outburst() const = 0;	/**< outburst */
	    virtual unsigned	buffersize() const = 0;	/**< size of audio buffer */
	    virtual MPXP_Rc	test_rate(unsigned r) const = 0;	/**< rate of samples in Hz */
	    virtual MPXP_Rc	test_channels(unsigned c) const = 0;	/**< number of audio channels */
	    virtual MPXP_Rc	test_format(unsigned f) const = 0;	/**< format of audio samples */

	    /** Stops playing and empties buffers (for seeking/pause) **/
	    virtual void	reset() = 0;

	    /** Returns how many bytes can be played without blocking **/
	    virtual unsigned	get_space() = 0;

	    /** Returns delay in seconds between first and last sample in buffer **/
	    virtual float	get_delay() = 0;

	    /** Plays decoded (PCM) audio buffer
	      * @param data		buffer with PCM data
	      * @param len		length of byffer in bytes
	      * @param flags	currently unused
	      * return		number of bytes which were copied into audio card
	     **/
	    virtual unsigned	play(const any_t* data,unsigned len,unsigned flags) = 0;

	    /** Stops playing, keep buffers (for pause) */
	    virtual void	pause() = 0;

	    /** Resumes playing, after audio_pause() */
	    virtual void	resume() = 0;

	    /** Control interface
	      * @param cmd	command. See AOCONTROL_** for detail
	      * @param arg	argument associated with command
	      * @return	MPXP_Ok if success MPXP_False MPXP_Error MPXP_NA otherwise
	     **/
	    virtual MPXP_Rc	ctrl(int cmd,long arg) const = 0;
	protected:
	    std::string		subdevice;
    };
} // namespace mpxp
#endif
