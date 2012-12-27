#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "audio_out.h"
#include "audio_out_internal.h"
#include "afmt.h"
#include "ao_msg.h"

namespace mpxp {
extern const ao_info_t audio_out_wav;
extern const ao_info_t audio_out_null;
#ifdef USE_OSS_AUDIO
extern const ao_info_t audio_out_oss;
#endif
#ifdef HAVE_ALSA
extern const ao_info_t audio_out_alsa;
#endif
#ifdef HAVE_SDL
extern const ao_info_t audio_out_sdl;
#endif
#ifdef HAVE_ARTS
extern const ao_info_t audio_out_arts;
#endif
#ifdef HAVE_ESD
extern const ao_info_t audio_out_esd;
#endif
#ifdef HAVE_OPENAL
extern const ao_info_t audio_out_openal;
#endif
#ifdef HAVE_NAS
extern const ao_info_t audio_out_nas;
#endif
#ifdef HAVE_JACK
extern const ao_info_t audio_out_jack;
#endif

static const ao_info_t* audio_out_drivers[] =
{
#ifdef USE_OSS_AUDIO
	&audio_out_oss,
#endif
#ifdef HAVE_SDL
	&audio_out_sdl,
#endif
#ifdef HAVE_ALSA
	&audio_out_alsa,
#endif
#ifdef HAVE_ARTS
	&audio_out_arts,
#endif
#ifdef HAVE_ESD
	&audio_out_esd,
#endif
#ifdef HAVE_OPENAL
	&audio_out_openal,
#endif
#ifdef HAVE_NAS
	&audio_out_nas,
#endif
#ifdef HAVE_JACK
	&audio_out_jack,
#endif
	&audio_out_wav,
	&audio_out_null,
	NULL
};

struct priv_t : public Opaque {
    public:
	priv_t() {}
	virtual ~priv_t() {}

	char		antiviral_hole[RND_CHAR5];
	const ao_info_t*info;
	AO_Interface*	driver;
	int		muted;
	float		mute_l,mute_r;
};

const char * __FASTCALL__ ao_format_name(int format)
{
    switch (format)
    {
	case AFMT_MU_LAW:
	    return "Mu-Law";
	case AFMT_A_LAW:
	    return "A-Law";
	case AFMT_IMA_ADPCM:
	    return "Ima-ADPCM";
	case AFMT_S8:
	    return "Signed 8-bit";
	case AFMT_U8:
	    return "Unsigned 8-bit";
	case AFMT_U16_LE:
	    return "Unsigned 16-bit (Little-Endian)";
	case AFMT_U16_BE:
	    return "Unsigned 16-bit (Big-Endian)";
	case AFMT_S16_LE:
	    return "Signed 16-bit (Little-Endian)";
	case AFMT_S16_BE:
	    return "Signed 16-bit (Big-Endian)";
	case AFMT_U24_LE:
	    return "Unsigned 24-bit (Little-Endian)";
	case AFMT_U24_BE:
	    return "Unsigned 24-bit (Big-Endian)";
	case AFMT_S24_LE:
	    return "Signed 24-bit (Little-Endian)";
	case AFMT_S24_BE:
	    return "Signed 24-bit (Big-Endian)";
	case AFMT_U32_LE:
	    return "Unsigned 32-bit (Little-Endian)";
	case AFMT_U32_BE:
	    return "Unsigned 32-bit (Big-Endian)";
	case AFMT_S32_LE:
	    return "Signed 32-bit (Little-Endian)";
	case AFMT_S32_BE:
	    return "Signed 32-bit (Big-Endian)";
	case AFMT_MPEG:
	    return "MPEG (2) audio";
	case AFMT_AC3:
	    return "AC3";
	case AFMT_FLOAT32:
	    return "Float32";
/*
  the following two formats are not available with old linux kernel
  headers (e.g. in 2.2.16)
*/
    }
    return "Unknown";
}

// return number of bits for 1 sample in one channel, or 8 bits for compressed
int __FASTCALL__ ao_format_bits(int format){
    switch (format)
    {
/*
  the following two formats are not available with old linux kernel
  headers (e.g. in 2.2.16)
*/
#ifdef AFMT_S32_LE
	case AFMT_S32_LE:
	case AFMT_U32_LE:
	return 32;
#endif
#ifdef AFMT_S32_BE
	case AFMT_S32_BE:
	case AFMT_U32_BE:
	return 32;
#endif
#ifdef AFMT_S24_LE
	case AFMT_S24_LE:
	case AFMT_U24_LE:
	return 24;
#endif
#ifdef AFMT_S24_BE
	case AFMT_S24_BE:
	case AFMT_U24_BE:
	return 24;
#endif

	case AFMT_U16_LE:
	case AFMT_U16_BE:
	case AFMT_S16_LE:
	case AFMT_S16_BE:
	return 16;//16 bits

	case AFMT_MU_LAW:
	case AFMT_A_LAW:
	case AFMT_IMA_ADPCM:
	case AFMT_S8:
	case AFMT_U8:
	case AFMT_MPEG:
	case AFMT_AC3:
	default:
	    return 8;//default 1 byte

    }
    return 8;
}

Audio_Output::Audio_Output(const char* _subdevice)
	    :subdevice(mp_strdup(_subdevice)),
	    opaque(*new(zeromem) priv_t){}

Audio_Output::~Audio_Output()
{
    priv_t& priv=static_cast<priv_t&>(opaque);
    delete priv.driver;
    if(subdevice) delete subdevice;
    delete &priv;
}

void Audio_Output::print_help() {
    unsigned i;
    mpxp_info<<"Available audio output drivers:"<<std::endl;
    i=0;
    while (audio_out_drivers[i]) {
	const ao_info_t *info = audio_out_drivers[i++];
	mpxp_info<<"\t"<<info->short_name<<"\t"<<info->name<<std::endl;
    }
    mpxp_info<<std::endl;
}

MPXP_Rc Audio_Output::_register(const std::string& driver_name,unsigned flags) const {
    priv_t& priv=static_cast<priv_t&>(opaque);
    unsigned i;
    if(driver_name.empty()) {
	priv.info=audio_out_drivers[0];
	priv.driver=audio_out_drivers[0]->query_interface(subdevice?subdevice:"");
    }
    else
    for (i=0; audio_out_drivers[i] != &audio_out_null; i++) {
	const ao_info_t *info = audio_out_drivers[i];
	if(info->short_name==driver_name){
	    priv.info = audio_out_drivers[i];
	    priv.driver = audio_out_drivers[i]->query_interface(subdevice?subdevice:"");
	    break;
	}
    }
    if(priv.driver) { if(priv.driver->open(flags)==MPXP_Ok) return MPXP_Ok; }
    return MPXP_False;
}

const ao_info_t* Audio_Output::get_info() const {
    priv_t& priv=static_cast<priv_t&>(opaque);
    return priv.info;
}

MPXP_Rc Audio_Output::configure(unsigned r,unsigned c,unsigned f) const {
    priv_t& priv=static_cast<priv_t&>(opaque);
    return priv.driver->configure(r,c,f);
}

unsigned Audio_Output::channels() const {
    priv_t& priv=static_cast<priv_t&>(opaque);
    return priv.driver->channels();
}
unsigned Audio_Output::samplerate() const {
    priv_t& priv=static_cast<priv_t&>(opaque);
    return priv.driver->samplerate();
}
unsigned Audio_Output::format() const {
    priv_t& priv=static_cast<priv_t&>(opaque);
    return priv.driver->format();
}

MPXP_Rc Audio_Output::test_channels(unsigned c) const {
    priv_t& priv=static_cast<priv_t&>(opaque);
    return priv.driver->test_channels(c);
}
MPXP_Rc Audio_Output::test_rate(unsigned s) const {
    priv_t& priv=static_cast<priv_t&>(opaque);
    return priv.driver->test_rate(s);
}
MPXP_Rc Audio_Output::test_format(unsigned f) const {
    priv_t& priv=static_cast<priv_t&>(opaque);
    return priv.driver->test_format(f);
}

unsigned Audio_Output::bps() const {
    priv_t& priv=static_cast<priv_t&>(opaque);
    return	priv.driver->channels()*
		priv.driver->samplerate()*
		afmt2bps(priv.driver->format());
}

unsigned Audio_Output::buffersize() const {
    priv_t& priv=static_cast<priv_t&>(opaque);
    return priv.driver->buffersize();
}

unsigned Audio_Output::outburst() const {
    priv_t& priv=static_cast<priv_t&>(opaque);
    return priv.driver->outburst();
}

void Audio_Output::reset() const {
    priv_t& priv=static_cast<priv_t&>(opaque);
    priv.driver->reset();
}

unsigned Audio_Output::get_space() const {
    priv_t& priv=static_cast<priv_t&>(opaque);
    return priv.driver->get_space();
}

float Audio_Output::get_delay() const {
    priv_t& priv=static_cast<priv_t&>(opaque);
    return priv.driver->get_delay();
}

unsigned Audio_Output::play(const any_t* data,unsigned len,unsigned flags) const {
    priv_t& priv=static_cast<priv_t&>(opaque);
    return priv.driver->play(data,len,flags);
}

void Audio_Output::pause() const {
    priv_t& priv=static_cast<priv_t&>(opaque);
    priv.driver->pause();
}

void Audio_Output::resume() const {
    priv_t& priv=static_cast<priv_t&>(opaque);
    priv.driver->resume();
}

MPXP_Rc Audio_Output::ctrl(int cmd,long arg) const {
    priv_t& priv=static_cast<priv_t&>(opaque);
    return priv.driver->ctrl(cmd,arg);
}

void Audio_Output::mixer_getvolume(float *l,float *r) const {
    ao_control_vol_t vol;
    *l=0; *r=0;
    if(MPXP_Ok != ctrl(AOCONTROL_GET_VOLUME,(long)&vol)) return;
    *r=vol.right;
    *l=vol.left;
}

void Audio_Output::mixer_setvolume(float l,float r) const {
    ao_control_vol_t vol;
    vol.right=r; vol.left=l;
    ctrl(AOCONTROL_SET_VOLUME,(long)&vol);
}

#define MIXER_CHANGE 3

void Audio_Output::mixer_incvolume() const {
    float mixer_l, mixer_r;
    mixer_getvolume(&mixer_l,&mixer_r );
    mixer_l += MIXER_CHANGE;
    if ( mixer_l > 100 ) mixer_l = 100;
    mixer_r += MIXER_CHANGE;
    if ( mixer_r > 100 ) mixer_r = 100;
    mixer_setvolume(mixer_l,mixer_r );
}

void Audio_Output::mixer_decvolume() const {
    float mixer_l, mixer_r;
    mixer_getvolume(&mixer_l,&mixer_r );
    mixer_l -= MIXER_CHANGE;
    if ( mixer_l < 0 ) mixer_l = 0;
    mixer_r -= MIXER_CHANGE;
    if ( mixer_r < 0 ) mixer_r = 0;
    mixer_setvolume(mixer_l,mixer_r);
}

float Audio_Output::mixer_getbothvolume() const {
    float mixer_l, mixer_r;
    mixer_getvolume(&mixer_l,&mixer_r);
    return ( mixer_l + mixer_r ) / 2;
}

void Audio_Output::mixer_mute() const {
    priv_t& priv=static_cast<priv_t&>(opaque);
    if ( priv.muted ) { mixer_setvolume(priv.mute_l,priv.mute_r ); priv.muted=0; }
    else {
	mixer_getvolume(&priv.mute_l,&priv.mute_r );
	mixer_setvolume(0,0);
	priv.muted=1;
    }
}
} // namespace mpxp
