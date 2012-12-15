#ifndef __AUDIO_OUT_H
#define __AUDIO_OUT_H 1
#include "mp_config.h"
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
}

/** Global data used by mplayerxp and plugins */
struct ao_data_t
{
    char*	subdevice;
    char	antiviral_hole[RND_CHAR2];
    any_t*	opaque;		/**< for internal use */
    any_t*	priv;
    float	pts;		/**< PTS of audio buffer  */
};

enum {
    AOCONTROL_GET_VOLUME=1, /**< Query volume level */
    AOCONTROL_SET_VOLUME    /**< Sets new volume level */
};

struct ao_control_vol_t {
    float left;
    float right;
};

/* prototypes */
extern const char *	 __FASTCALL__ ao_format_name(int format);
extern int		 __FASTCALL__ ao_format_bits(int format);

extern void		ao_print_help( void );
extern MPXP_Rc	__FASTCALL__ ao_register(ao_data_t* ao,const char *driver_name,unsigned flags);
extern const ao_info_t*	ao_get_info( const ao_data_t* ao );
extern ao_data_t*	__FASTCALL__ ao_init(const char *subdevice);
extern MPXP_Rc		__FASTCALL__ ao_configure(ao_data_t* priv,unsigned rate,unsigned channels,unsigned format);
extern void		__FASTCALL__ ao_uninit(ao_data_t* priv);
extern unsigned		__FASTCALL__ ao_buffersize(ao_data_t* priv);
extern unsigned		__FASTCALL__ ao_outburst(ao_data_t* priv);
extern unsigned		__FASTCALL__ ao_channels(ao_data_t* priv);
extern unsigned		__FASTCALL__ ao_samplerate(ao_data_t* priv);
extern unsigned		__FASTCALL__ ao_format(ao_data_t* priv);
extern unsigned		__FASTCALL__ ao_bps(ao_data_t* priv);
extern MPXP_Rc		__FASTCALL__ ao_test_channels(ao_data_t* priv,unsigned c);
extern MPXP_Rc		__FASTCALL__ ao_test_rate(ao_data_t* priv,unsigned s);
extern MPXP_Rc		__FASTCALL__ ao_test_format(ao_data_t* priv,unsigned f);

extern void		__FASTCALL__ ao_reset(ao_data_t* priv);
extern unsigned		__FASTCALL__ ao_get_space(const ao_data_t* priv);
extern unsigned		__FASTCALL__ ao_play(ao_data_t* priv,const any_t* data,unsigned len,unsigned flags);
extern float		__FASTCALL__ ao_get_delay(const ao_data_t* priv);
extern void		__FASTCALL__ ao_pause(ao_data_t* priv);
extern void		__FASTCALL__ ao_resume(ao_data_t* priv);
extern MPXP_Rc		__FASTCALL__ ao_control(const ao_data_t* priv,int cmd,long arg);

extern void mixer_getvolume(const ao_data_t* ao,float *l,float *r );
extern void mixer_setvolume(const ao_data_t* ao,float l,float r );
extern void mixer_incvolume(const ao_data_t* ao);
extern void mixer_decvolume(const ao_data_t* ao);
extern float mixer_getbothvolume(const ao_data_t* ao);
void mixer_mute(const ao_data_t* ao);

//extern void mixer_setbothvolume( int v );
static inline void mixer_setbothvolume(const ao_data_t* ao, float v ) { mixer_setvolume(ao,v,v); }

#endif
