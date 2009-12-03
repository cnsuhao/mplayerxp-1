#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../config.h"
#include "audio_out.h"
#include "afmt.h"
#include "ao_msg.h"

// there are some globals:
ao_data_t ao_data={0,0,0,0,OUTBURST,-1,0};
char *ao_subdevice = NULL;

#ifdef USE_OSS_AUDIO
extern const ao_functions_t audio_out_oss;
#endif
extern const ao_functions_t audio_out_null;
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

const ao_functions_t* audio_out_drivers[] =
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
        &audio_out_null,
	NULL
};

static const ao_functions_t *audio_out=NULL;
static int ao_inited=0;

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

const ao_functions_t* __FASTCALL__ ao_register(const char *driver_name)
{
  unsigned i;
  if(!driver_name)
    audio_out=audio_out_drivers[0];
  else
  for (i=0; audio_out_drivers[i] != NULL; i++)
  {
    const ao_info_t *info = audio_out_drivers[i]->info;
    if(strcmp(info->short_name,driver_name) == 0){
      audio_out = audio_out_drivers[i];break;
    }
  }
  return audio_out;
}

const ao_info_t* ao_get_info( void )
{
    return audio_out->info;
}

int __FASTCALL__ ao_init(int flags)
{
    int retval;
    retval = audio_out->init(flags);
    if(retval) ao_inited=1;
    return retval;
}

int __FASTCALL__ ao_configure(int rate,int channels,int format)
{
    int retval;
    retval=audio_out->configure(rate,channels,format);
    return retval;
}

void ao_uninit(void)
{
    audio_out->uninit();
    ao_inited=0;
}

void ao_reset(void)
{
    if(ao_inited) audio_out->reset();
}

int ao_get_space(void)
{
    return ao_inited?audio_out->get_space():0;
}

float ao_get_delay(void)
{
    return ao_inited?audio_out->get_delay():0;
}

int __FASTCALL__ ao_play(void* data,int len,int flags)
{
    return ao_inited?audio_out->play(data,len,flags):0;
}

void ao_pause(void)
{
    if(ao_inited) audio_out->pause();
}

void ao_resume(void)
{
    if(ao_inited) audio_out->resume();
}

int __FASTCALL__ ao_control(int cmd,long arg)
{
    return ao_inited?audio_out->control(cmd,arg):CONTROL_ERROR;
}
