#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "mp_config.h"
#include "osdep/mplib.h"
#include "audio_out.h"
#include "afmt.h"
#include "ao_msg.h"

extern const ao_functions_t audio_out_wav;
extern const ao_functions_t audio_out_null;
#ifdef USE_OSS_AUDIO
extern const ao_functions_t audio_out_oss;
#endif
#ifdef HAVE_ALSA
extern const ao_functions_t audio_out_alsa;
#endif
#ifdef HAVE_SDL
extern const ao_functions_t audio_out_sdl;
#endif
#ifdef HAVE_ARTS
extern const ao_functions_t audio_out_arts;
#endif
#ifdef HAVE_ESD
extern const ao_functions_t audio_out_esd;
#endif
#ifdef HAVE_OPENAL
extern const ao_functions_t audio_out_openal;
#endif
#ifdef HAVE_NAS
extern const ao_functions_t audio_out_nas;
#endif
#ifdef HAVE_JACK
extern const ao_functions_t audio_out_jack;
#endif

static const ao_functions_t* audio_out_drivers[] =
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

static const ao_functions_t *RND_RENAME8(audio_out)=NULL;

char * __FASTCALL__ ao_format_name(int format)
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
	const ao_info_t *info = audio_out_drivers[i++]->info;
	MSG_INFO("\t%s\t%s\n", info->short_name, info->name);
    }
    MSG_INFO("\n");
}

const ao_functions_t* __FASTCALL__ RND_RENAME4(ao_register)(const char *driver_name)
{
    unsigned i;
    if(!driver_name)
	RND_RENAME8(audio_out)=audio_out_drivers[0];
    else
    for (i=0; audio_out_drivers[i] != &audio_out_null; i++) {
	const ao_info_t *info = audio_out_drivers[i]->info;
	if(strcmp(info->short_name,driver_name) == 0){
	    RND_RENAME8(audio_out) = audio_out_drivers[i];break;
	}
    }
    return RND_RENAME8(audio_out);
}

const ao_info_t* ao_get_info( void )
{
    return RND_RENAME8(audio_out)->info;
}

ao_data_t* __FASTCALL__ RND_RENAME5(ao_init)(unsigned flags,const char *subdevice)
{
    ao_data_t* ao;
    ao=mp_mallocz(sizeof(ao_data_t));
    if(subdevice) ao->subdevice=mp_strdup(subdevice);
    ao->outburst=OUTBURST;
    ao->buffersize=-1;
    RND_RENAME0(rnd_fill)(ao->antiviral_hole,sizeof(ao->antiviral_hole));
    if(RND_RENAME8(audio_out)->init(ao,flags)==MPXP_Ok) return ao;
    mp_free(ao);
    return NULL;
}

MPXP_Rc __FASTCALL__ ao_configure(ao_data_t*ao,unsigned rate,unsigned channels,unsigned format)
{
    return RND_RENAME8(audio_out)->configure(ao,rate,channels,format);
}

void ao_uninit(ao_data_t*ao)
{
    RND_RENAME8(audio_out)->uninit(ao);
    if(ao->subdevice) mp_free(ao->subdevice);
    mp_free(ao);
    ao=NULL;
}

void ao_reset(ao_data_t*ao)
{
    if(ao) RND_RENAME8(audio_out)->reset(ao);
}

unsigned ao_get_space(ao_data_t*ao)
{
    return ao?RND_RENAME8(audio_out)->get_space(ao):0;
}

float ao_get_delay(ao_data_t*ao)
{
    return ao?RND_RENAME8(audio_out)->get_delay(ao):0;
}

unsigned __FASTCALL__ RND_RENAME6(ao_play)(ao_data_t*ao,any_t* data,unsigned len,unsigned flags)
{
    return ao?RND_RENAME8(audio_out)->play(ao,data,len,flags):0;
}

void ao_pause(ao_data_t*ao)
{
    if(ao) RND_RENAME8(audio_out)->pause(ao);
}

void ao_resume(ao_data_t*ao)
{
    if(ao) RND_RENAME8(audio_out)->resume(ao);
}

MPXP_Rc __FASTCALL__ RND_RENAME7(ao_control)(ao_data_t*ao,int cmd,long arg)
{
    return ao?RND_RENAME8(audio_out)->control(ao,cmd,arg):MPXP_Error;
}
