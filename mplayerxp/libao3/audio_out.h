#ifndef __AUDIO_OUT_H
#define __AUDIO_OUT_H 1
#include "mpxp_config.h"
#include "xmpcore/xmp_enums.h"

#include <string>

namespace mpxp {
    class AO_Interface;

    /** Text description of AO-driver */
    struct ao_info_t {
	const char *name;	/**< driver name ("alsa driver") */
	const char *short_name; /**< short name (for config strings) ("alsa") */
	const char *author;	/**< author ("Aaron Holtzman <aholtzma@ess.engr.uvic.ca>") */
	const char *comment;/**< any additional comments */
	AO_Interface* (*query_interface)(const std::string& subdevice);
    };

    enum {
	AOCONTROL_GET_VOLUME=1, /**< Query volume level */
	AOCONTROL_SET_VOLUME    /**< Sets new volume level */
    };

    struct ao_control_vol_t {
	float left;
	float right;
    };

    /** Global data used by mplayerxp and plugins */
    struct Audio_Output : public Opaque {
	public:
	    Audio_Output(const char* subdevice);
	    virtual ~Audio_Output();

	    virtual void		print_help() const;
	    virtual MPXP_Rc		_register(const char *driver_name,unsigned flags) const;
	    virtual const ao_info_t*	get_info() const;
	    virtual MPXP_Rc		configure(unsigned rate,unsigned channels,unsigned format) const;
	    virtual unsigned		buffersize() const;
	    virtual unsigned		outburst() const;
	    virtual unsigned		channels() const;
	    virtual unsigned		samplerate() const;
	    virtual unsigned		format() const;
	    virtual unsigned		bps() const;
	    virtual MPXP_Rc		test_channels(unsigned c) const;
	    virtual MPXP_Rc		test_rate(unsigned s) const;
	    virtual MPXP_Rc		test_format(unsigned f) const;

	    virtual void		reset() const;
	    virtual unsigned		get_space() const;
	    virtual unsigned		play(const any_t* data,unsigned len,unsigned flags) const;
	    virtual float		get_delay() const;
	    virtual void		pause() const;
	    virtual void		resume() const;
	    virtual MPXP_Rc		ctrl(int cmd,long arg) const;

	    virtual void		mixer_getvolume(float *l,float *r ) const;
	    virtual void		mixer_setvolume(float l,float r ) const;
	    virtual void		mixer_incvolume() const;
	    virtual void		mixer_decvolume() const;
	    virtual float		mixer_getbothvolume() const;
	    void			mixer_mute() const;
	    //virtual void mixer_setbothvolume( int v );
	    inline void			mixer_setbothvolume(float v) const { mixer_setvolume(v,v); }

	    char*	subdevice;
	    float	pts;		/**< PTS of audio buffer  */
	private:
	    Opaque&	opaque;		/**< for internal use */
    };
    /* prototypes */
    extern const char *	 __FASTCALL__ ao_format_name(int format);
    extern int		 __FASTCALL__ ao_format_bits(int format);
} // namespace mpxp
#endif
