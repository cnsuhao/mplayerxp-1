#include "mp_config.h"
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
} // namespace mpxp
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

typedef struct priv_s {
    char		antiviral_hole[RND_CHAR5];
    const ao_info_t*	info;
    AO_Interface*	driver;
}priv_t;

const char * __FASTCALL__ ao_format_name(int format)
{
    switch (format)
    {
	case AFMT_MU_LAW:
	    return("Mu-Law");
	case AFMT_A_LAW:
	    return("A-Law");
	case AFMT_IMA_ADPCM:
	    return("Ima-ADPCM");
	case AFMT_S8:
	    return("Signed 8-bit");
	case AFMT_U8:
	    return("Unsigned 8-bit");
	case AFMT_U16_LE:
	    return("Unsigned 16-bit (Little-Endian)");
	case AFMT_U16_BE:
	    return("Unsigned 16-bit (Big-Endian)");
	case AFMT_S16_LE:
	    return("Signed 16-bit (Little-Endian)");
	case AFMT_S16_BE:
	    return("Signed 16-bit (Big-Endian)");
	case AFMT_U24_LE:
	    return("Unsigned 24-bit (Little-Endian)");
	case AFMT_U24_BE:
	    return("Unsigned 24-bit (Big-Endian)");
	case AFMT_S24_LE:
	    return("Signed 24-bit (Little-Endian)");
	case AFMT_S24_BE:
	    return("Signed 24-bit (Big-Endian)");
	case AFMT_U32_LE:
	    return("Unsigned 32-bit (Little-Endian)");
	case AFMT_U32_BE:
	    return("Unsigned 32-bit (Big-Endian)");
	case AFMT_S32_LE:
	    return("Signed 32-bit (Little-Endian)");
	case AFMT_S32_BE:
	    return("Signed 32-bit (Big-Endian)");
	case AFMT_MPEG:
	    return("MPEG (2) audio");
	case AFMT_AC3:
	    return("AC3");
	case AFMT_FLOAT32:
	    return("Float32");
/*
  the following two formats are not available with old linux kernel
  headers (e.g. in 2.2.16)
*/
    }
    return("Unknown");
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


void ao_print_help( void )
{
    unsigned i;
    MSG_INFO("Available audio output drivers:\n");
    i=0;
    while (audio_out_drivers[i]) {
	const ao_info_t *info = audio_out_drivers[i++];
	MSG_INFO("\t%s\t%s\n", info->short_name, info->name);
    }
    MSG_INFO("\n");
}

MPXP_Rc __FASTCALL__ ao_register(ao_data_t* ao,const char *driver_name,unsigned flags)
{
    priv_t* priv=reinterpret_cast<priv_t*>(ao->opaque);
    unsigned i;
    if(!driver_name) {
	priv->info=audio_out_drivers[0];
	priv->driver=audio_out_drivers[0]->query_interface(ao->subdevice);
    }
    else
    for (i=0; audio_out_drivers[i] != &audio_out_null; i++) {
	const ao_info_t *info = audio_out_drivers[i];
	if(strcmp(info->short_name,driver_name) == 0){
	    priv->info = audio_out_drivers[i];
	    priv->driver = audio_out_drivers[i]->query_interface(ao->subdevice?ao->subdevice:"");
	    break;
	}
    }
    if(priv->driver->open(flags)==MPXP_Ok) return MPXP_Ok;
    return MPXP_False;
}

const ao_info_t* ao_get_info( const ao_data_t* ao )
{
    priv_t* priv=reinterpret_cast<priv_t*>(ao->opaque);
    return priv->info;
}

ao_data_t* __FASTCALL__ ao_init(const char *subdevice)
{
    ao_data_t* ao;
    ao=new(zeromem) ao_data_t;
    if(subdevice) ao->subdevice=mp_strdup(subdevice);
    priv_t* priv=new(zeromem) priv_t;
    ao->opaque=priv;
    fill_false_pointers(ao->antiviral_hole,offsetof(ao_data_t,opaque)-offsetof(ao_data_t,antiviral_hole));
    priv->driver=NULL;
    return ao;
}

MPXP_Rc __FASTCALL__ ao_configure(ao_data_t*ao,unsigned rate,unsigned channels,unsigned format)
{
    priv_t* priv=reinterpret_cast<priv_t*>(ao->opaque);
    return priv->driver->configure(rate,channels,format);
}

void ao_uninit(ao_data_t*ao)
{
    priv_t* priv=reinterpret_cast<priv_t*>(ao->opaque);
    delete priv->driver;
    if(ao->subdevice) delete ao->subdevice;
    delete priv;
    delete ao;
    ao=NULL;
}

unsigned ao_channels(ao_data_t* ao) {
    if(ao) {
	priv_t* priv=reinterpret_cast<priv_t*>(ao->opaque);
	return priv->driver->channels();
    }
    return 0;
}
unsigned ao_samplerate(ao_data_t* ao) {
    if(ao) {
	priv_t* priv=reinterpret_cast<priv_t*>(ao->opaque);
	return priv->driver->samplerate();
    }
    return 0;
}
unsigned ao_format(ao_data_t* ao) {
    if(ao) {
	priv_t* priv=reinterpret_cast<priv_t*>(ao->opaque);
	return priv->driver->format();
    }
    return 0;
}

MPXP_Rc ao_test_channels(ao_data_t* ao,unsigned c) {
    if(ao) {
	priv_t* priv=reinterpret_cast<priv_t*>(ao->opaque);
	return priv->driver->test_channels(c);
    }
    return MPXP_False;
}
MPXP_Rc ao_test_rate(ao_data_t* ao,unsigned s) {
    if(ao) {
	priv_t* priv=reinterpret_cast<priv_t*>(ao->opaque);
	return priv->driver->test_rate(s);
    }
    return MPXP_False;
}
MPXP_Rc ao_test_format(ao_data_t* ao,unsigned f) {
    if(ao) {
	priv_t* priv=reinterpret_cast<priv_t*>(ao->opaque);
	return priv->driver->test_format(f);
    }
    return MPXP_False;
}

unsigned ao_bps(ao_data_t* ao) {
    if(ao) {
	priv_t* priv=reinterpret_cast<priv_t*>(ao->opaque);
	return	priv->driver->channels()*
		priv->driver->samplerate()*
		afmt2bps(priv->driver->format());
    }
    return 0;
}

unsigned ao_buffersize(ao_data_t* ao) {
    if(ao) {
	priv_t* priv=reinterpret_cast<priv_t*>(ao->opaque);
	return priv->driver->buffersize();
    }
    return 0;
}

unsigned ao_outburst(ao_data_t* ao) {
    if(ao) {
	priv_t* priv=reinterpret_cast<priv_t*>(ao->opaque);
	return priv->driver->outburst();
    }
    return 0;
}

void ao_reset(ao_data_t*ao)
{
    if(ao) {
	priv_t* priv=reinterpret_cast<priv_t*>(ao->opaque);
	priv->driver->reset();
    }
}

unsigned ao_get_space(const ao_data_t*ao)
{
    if(ao) {
	priv_t* priv=reinterpret_cast<priv_t*>(ao->opaque);
	return priv->driver->get_space();
    }
    return 0;
}

float ao_get_delay(const ao_data_t*ao)
{
    if(ao) {
	priv_t* priv=reinterpret_cast<priv_t*>(ao->opaque);
	return priv->driver->get_delay();
    }
    return 0;
}

unsigned __FASTCALL__ ao_play(ao_data_t*ao,const any_t* data,unsigned len,unsigned flags)
{
    if(ao) {
	priv_t* priv=reinterpret_cast<priv_t*>(ao->opaque);
	return priv->driver->play(data,len,flags);
    } return 0;
}

void ao_pause(ao_data_t*ao)
{
    if(ao) {
	priv_t* priv=reinterpret_cast<priv_t*>(ao->opaque);
	priv->driver->pause();
    }
}

void ao_resume(ao_data_t*ao)
{
    if(ao) {
	priv_t* priv=reinterpret_cast<priv_t*>(ao->opaque);
	priv->driver->resume();
    }
}

MPXP_Rc __FASTCALL__ ao_control(const ao_data_t*ao,int cmd,long arg)
{
    if(ao) {
	priv_t* priv=reinterpret_cast<priv_t*>(ao->opaque);
	return priv->driver->ctrl(cmd,arg);
    }
    return MPXP_Error;
}
