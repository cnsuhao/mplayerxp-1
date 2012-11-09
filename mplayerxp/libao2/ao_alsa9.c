/*
  ao_alsa9 - ALSA-0.9.x output plugin for MPlayer

  (C) Alex Beregszaszi <alex@naxine.org>

  modified for real alsa-0.9.0-support by Joy Winter <joy@pingfm.org>
  additional AC3 passthrough support by Andy Lo A Foe <andy@alsaplayer.org>
  08/22/2002 iec958-init rewritten and merged with common init, joy

  Any bugreports regarding to this driver are welcome.
*/

#include <errno.h>
#include <sys/time.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <sys/poll.h>

#include "mp_config.h"
#include "mplayerxp.h"
#define ALSA_PCM_NEW_HW_PARAMS_API
#define ALSA_PCM_NEW_SW_PARAMS_API
#include <alsa/asoundlib.h>

#include "audio_out.h"
#include "audio_out_internal.h"
#include "afmt.h"
#include "ao_msg.h"
#include "libmpdemux/mrl.h"
#include "osdep/mplib.h"

static ao_info_t info =
{
    "ALSA-1.x audio output",
    "alsa",
    "Alex Beregszaszi <alex@naxine.org>, Joy Winter <joy@pingfm.org>",
    "under developement"
};

LIBAO_EXTERN(alsa)

typedef struct priv_s {
    snd_pcm_t*		handler;
    snd_pcm_format_t	format;
    snd_pcm_hw_params_t*hwparams;
    snd_pcm_sw_params_t*swparams;
    size_t		bytes_per_sample;
    int			first;
}priv_t;

typedef struct priv_conf_s {
    int mmap;
    int noblock;
}priv_conf_t;
static priv_conf_t priv_conf;
static const mrl_config_t alsaconf[]={
    { "mmap", &priv_conf.mmap, MRL_TYPE_BOOL, 0, 1 },
    { "noblock", &priv_conf.noblock, MRL_TYPE_BOOL, 0, 1 },
    { NULL, NULL, 0, 0, 0 }
};

#define ALSA_DEVICE_SIZE	48

#define BUFFERTIME // else SET_CHUNK_SIZE
#undef USE_POLL

static int __FASTCALL__ fmt2alsa(int format)
{
    switch (format)
    {
      case AFMT_S8:
	return SND_PCM_FORMAT_S8;
	break;
      case AFMT_U8:
	return SND_PCM_FORMAT_U8;
	break;
      case AFMT_U16_LE:
	return SND_PCM_FORMAT_U16_LE;
	break;
      case AFMT_U16_BE:
	return SND_PCM_FORMAT_U16_BE;
	break;
#ifndef WORDS_BIGENDIAN
      case AFMT_AC3:
#endif
      case AFMT_S16_LE:
	return SND_PCM_FORMAT_S16_LE;
	break;
#ifdef WORDS_BIGENDIAN
      case AFMT_AC3:
#endif
      case AFMT_S16_BE:
	return SND_PCM_FORMAT_S16_BE;
	break;
      case AFMT_S32_LE:
	return SND_PCM_FORMAT_S32_LE;
	break;
      case AFMT_S32_BE:
	return SND_PCM_FORMAT_S32_BE;
	break;
      case AFMT_U32_LE:
	return SND_PCM_FORMAT_U32_LE;
	break;
      case AFMT_U32_BE:
	return SND_PCM_FORMAT_U32_BE;
	break;
      case AFMT_S24_LE:
	return SND_PCM_FORMAT_S24_LE;
	break;
      case AFMT_S24_BE:
	return SND_PCM_FORMAT_S24_BE;
	break;
      case AFMT_U24_LE:
	return SND_PCM_FORMAT_U24_LE;
	break;
      case AFMT_U24_BE:
	return SND_PCM_FORMAT_U24_BE;
	break;
      case AFMT_FLOAT32:
#ifdef WORDS_BIGENDIAN
	return SND_PCM_FORMAT_FLOAT_BE;
#else
	return SND_PCM_FORMAT_FLOAT_LE;
#endif
	break;
      default:
	return SND_PCM_FORMAT_MPEG;
	break;
    }
}

/* to set/get/query special features/parameters */
static MPXP_Rc __FASTCALL__ control(ao_data_t* ao,int cmd, long arg)
{
    priv_t*priv=ao->priv;
    int rval;
    switch(cmd) {
	case AOCONTROL_QUERY_FORMAT:
	    rval=fmt2alsa(arg);
	    return snd_pcm_hw_params_test_format(priv->handler, priv->hwparams,rval)==0?
		    MPXP_True:MPXP_False;
	case AOCONTROL_QUERY_CHANNELS:
	    rval=arg;
	    return snd_pcm_hw_params_test_channels(priv->handler, priv->hwparams,rval)==0?
		    MPXP_True:MPXP_False;
	case AOCONTROL_QUERY_RATE:
	    rval=arg;
	    return snd_pcm_hw_params_test_rate(priv->handler, priv->hwparams,rval,0)==0?
		    MPXP_True:MPXP_False;
	case AOCONTROL_GET_VOLUME:
	case AOCONTROL_SET_VOLUME:
#ifndef WORDS_BIGENDIAN
	{ //seems to be a problem on macs?
	    ao_control_vol_t *vol = (ao_control_vol_t *)arg;

	    int err;
	    snd_mixer_t *handle;
	    snd_mixer_elem_t *elem;
	    snd_mixer_selem_id_t *sid;

	    const char *mix_name = "PCM";
	    char *card = "default";

	    long pmin, pmax;
	    long get_vol, set_vol;
	    float calc_vol, diff, f_multi;

	    if(ao->format == AFMT_AC3) return MPXP_True;

	    //allocate simple id
	    snd_mixer_selem_id_alloca(&sid);

	    //sets simple-mixer index and name
	    snd_mixer_selem_id_set_index(sid, 0);
	    snd_mixer_selem_id_set_name(sid, mix_name);

	    if ((err = snd_mixer_open(&handle, 0)) < 0) {
		MSG_ERR("alsa-control: mixer open error: %s\n", snd_strerror(err));
		return MPXP_Error;
	    }

	    if ((err = snd_mixer_attach(handle, card)) < 0) {
		MSG_ERR("alsa-control: mixer attach %s error: %s", card, snd_strerror(err));
		snd_mixer_close(handle);
		return MPXP_Error;
	    }

	    if ((err = snd_mixer_selem_register(handle, NULL, NULL)) < 0) {
		MSG_ERR("alsa-control: mixer register error: %s", snd_strerror(err));
		snd_mixer_close(handle);
		return MPXP_Error;
	    }
	    if ((err = snd_mixer_load(handle)) < 0) {
		MSG_ERR("alsa-control: mixer load error: %s", snd_strerror(err));
		snd_mixer_close(handle);
		return MPXP_Error;
	    }

	    elem = snd_mixer_find_selem(handle, sid);
	    if (!elem) {
		MSG_ERR("alsa-control: unable to find simple control '%s',%i\n", snd_mixer_selem_id_get_name(sid), snd_mixer_selem_id_get_index(sid));
		snd_mixer_close(handle);
		return MPXP_Error;
	    }

	    snd_mixer_selem_get_playback_volume_range(elem,&pmin,&pmax);
	    f_multi = (100 / (float)pmax);

	    if (cmd == AOCONTROL_SET_VOLUME) {

		diff = (vol->left+vol->right) / 2;
		set_vol = rint(diff / f_multi);

		if (set_vol < 0) set_vol = 0;
		else if (set_vol > pmax) set_vol = pmax;

		//setting channels
		if ((err = snd_mixer_selem_set_playback_volume(elem, 0, set_vol)) < 0) {
		    MSG_ERR("alsa-control: error setting left channel, %s",snd_strerror(err));
		    return MPXP_Error;
		}
		if ((err = snd_mixer_selem_set_playback_volume(elem, 1, set_vol)) < 0) {
		    MSG_ERR("alsa-control: error setting right channel, %s",snd_strerror(err));
		    return MPXP_Error;
		}
	    } else {
		snd_mixer_selem_get_playback_volume(elem, 0, &get_vol);
		calc_vol = get_vol;
		calc_vol = rintf(calc_vol * f_multi);

		vol->left = vol->right = (int)calc_vol;

		//printf("get_vol = %i, calc=%i\n",get_vol, calc_vol);
	    }
	    snd_mixer_close(handle);
	    return MPXP_Ok;
	}
#else // end big-endian
	return MPXP_Unknown;
#endif
    } //end witch
    return MPXP_Unknown;
}

static void __FASTCALL__ show_caps(unsigned device)
{
    snd_pcm_info_t *alsa_info;
    snd_pcm_t *pcm;
    snd_pcm_hw_params_t *hw_params;
    snd_output_t *sout;
    int err,cards=-1;
    unsigned rmin,rmax;
    unsigned j,sdmin,sdmax;
    char adevice[ALSA_DEVICE_SIZE];
    if ((err = snd_card_next(&cards)) < 0 || cards < 0)
    {
	MSG_ERR("AO-INFO: alsa-init: no soundcards found: %s\n", snd_strerror(err));
	return;
    }
    snd_pcm_info_malloc(&alsa_info);
    snd_pcm_info_set_device(alsa_info,device);
    sdmin=snd_pcm_info_get_subdevice(alsa_info);
    sdmax=sdmin+snd_pcm_info_get_subdevices_count(alsa_info);
    MSG_INFO("AO-INFO: show caps for device %i:%i-%i\n",device,sdmin,sdmax);
    for(j=sdmin;j<=sdmax;j++)
    {
	int i;
	snd_pcm_info_set_subdevice(alsa_info,j);
	sprintf(adevice,"hw:%u,%u",snd_pcm_info_get_device(alsa_info),snd_pcm_info_get_subdevice(alsa_info));
	MSG_INFO("AO-INFO: %s %s.%s.%s\n\n",adevice,snd_pcm_info_get_id(alsa_info),snd_pcm_info_get_name(alsa_info),snd_pcm_info_get_subdevice_name(alsa_info));
	if(snd_pcm_open(&pcm,adevice,SND_PCM_STREAM_PLAYBACK,SND_PCM_NONBLOCK)<0)
	{
	    MSG_ERR("alsa-init: playback open error: %s\n", snd_strerror(err));
	    return;
	}
	snd_pcm_hw_params_malloc(&hw_params);
	if(snd_pcm_hw_params_any(pcm, hw_params)<0)
	{
	    MSG_ERR("alsa-init: can't get initial parameters: %s\n", snd_strerror(err));
	    return;
	}
	MSG_INFO("    AO-INFO: List of access type: ");
	for(i=0;i<SND_PCM_ACCESS_LAST;i++)
	    if(!snd_pcm_hw_params_test_access(pcm,hw_params,i))
		MSG_INFO("%s ",snd_pcm_access_name(i));
	MSG_INFO("\n");
	MSG_INFO("    AO-INFO: List of supported formats: ");
	for(i=0;i<SND_PCM_FORMAT_LAST;i++)
	    if(!snd_pcm_hw_params_test_format(pcm,hw_params,i))
		MSG_INFO("%s ",snd_pcm_format_name(i));
	MSG_INFO("\n");
	MSG_INFO("    AO-INFO: List of supported channels: ");
	for(i=0;i<64;i++)
	    if(!snd_pcm_hw_params_test_format(pcm,hw_params,i))
		MSG_INFO("%u ",i);
	MSG_INFO("\n");
	snd_pcm_hw_params_get_rate_min(hw_params,&rmin,&err);
	snd_pcm_hw_params_get_rate_max(hw_params,&rmax,&err);
	MSG_INFO("    AO-INFO: Rates range: %u %u\n",rmin,rmax);
	snd_output_stdio_attach(&sout, stderr, 0);
	snd_pcm_hw_params_dump(hw_params, sout);
	if(hw_params) snd_pcm_hw_params_free(hw_params);
	if(pcm) snd_pcm_close(pcm);
    }
    snd_pcm_info_free(alsa_info);
}

/*
    open & setup audio device
    return: 1=success 0=fail
*/
static MPXP_Rc __FASTCALL__ init(ao_data_t* ao,unsigned flags)
{
    int err;
    int cards = -1;
    snd_pcm_info_t *alsa_info;
    char *str_block_mode;
    char *alsa_dev=NULL;
    char *alsa_port=NULL;
    char alsa_device[ALSA_DEVICE_SIZE];
    UNUSED(flags);
    ao->priv=mp_mallocz(sizeof(priv_t));
    priv_t*priv=ao->priv;
    priv->first=1;

    priv->handler = NULL;
    alsa_device[0]='\0';

    MSG_V("alsa-init: compiled for ALSA-%s\n", SND_LIB_VERSION_STR);

    if (ao->subdevice) {
	const char *param;
	char *p;
	// example: -ao alsa:hw:0#mmap=1
	param=mrl_parse_line(ao->subdevice,NULL,NULL,&alsa_dev,&alsa_port);
	mrl_parse_params(param,alsaconf);
	if(alsa_port) {
	    p=strchr(alsa_port,',');
	    if(p) {
		if(strcmp(p+1,"-1")==0) {
		    *p='\0';
		    show_caps(atoi(alsa_port));
		    return MPXP_False;
		}
	    }
	    if(alsa_port) snprintf(alsa_device,sizeof(alsa_device),"%s:%s",alsa_dev,alsa_port);
	    else	  strncpy(alsa_device,alsa_dev,sizeof(alsa_device));
	    MSG_V("alsa-init: soundcard set to %s\n", alsa_device);
	} //end parsing ao->subdevice
    }

    if ((err = snd_card_next(&cards)) < 0 || cards < 0) {
	MSG_ERR("alsa-init: no soundcards found: %s\n", snd_strerror(err));
	return MPXP_False;
    }

    if (alsa_device[0] == '\0') {
	int tmp_device, tmp_subdevice;

	if ((err = snd_pcm_info_malloc(&alsa_info)) < 0) {
	    MSG_ERR("alsa-init: memory allocation error: %s\n", snd_strerror(err));
	    return MPXP_False;
	}
	
	if ((tmp_device = snd_pcm_info_get_device(alsa_info)) < 0) {
	    MSG_ERR("alsa-init: cant get device\n");
	    return MPXP_False;
	}

	if ((tmp_subdevice = snd_pcm_info_get_subdevice(alsa_info)) < 0) {
	    MSG_ERR("alsa-init: cant get subdevice\n");
	    return MPXP_False;
	}
	MSG_V("alsa-init: got device=%i, subdevice=%i\n", tmp_device, tmp_subdevice);

	if ((err = snprintf(alsa_device, ALSA_DEVICE_SIZE, "hw:%1d,%1d", tmp_device, tmp_subdevice)) <= 0) {
	    MSG_ERR("alsa-init: cant wrote device-id\n");
	}
	snd_pcm_info_free(alsa_info);
    }

    MSG_WARN("alsa-init: Testing & bugs are welcome. Found %d cards, use: %s\n",cards+1,alsa_device);
    //setting modes for block or nonblock-mode
    int open_mode,block_mode;
    if (priv_conf.noblock) {
	open_mode = SND_PCM_NONBLOCK;
	block_mode = 1;
	str_block_mode = "nonblock-mode";
    } else {
	open_mode = 0;
	block_mode = 0;
	str_block_mode = "block-mode";
    }

    if (!priv->handler) {
	//modes = 0, SND_PCM_NONBLOCK, SND_PCM_ASYNC
	if ((err = snd_pcm_open(&priv->handler, alsa_device, SND_PCM_STREAM_PLAYBACK, open_mode)) < 0) {
	    if (priv_conf.noblock) {
		MSG_ERR("alsa-init: open in nonblock-mode failed, trying to open in block-mode\n");
		if ((err = snd_pcm_open(&priv->handler, alsa_device, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
		    MSG_ERR("alsa-init: playback open error: %s\n", snd_strerror(err));
		    alsa_device[0]='\0';
		    return MPXP_False;
		} else {
		    block_mode = 0;
		    str_block_mode = "block-mode";
		}
	    } else {
		MSG_ERR("alsa-init: playback open error: %s\n", snd_strerror(err));
		alsa_device[0]='\0';
		return MPXP_False;
	    }
	}
      alsa_device[0]='\0';
      if ((err = snd_pcm_nonblock(priv->handler, block_mode)) < 0) {
	MSG_ERR("alsa-init: error set block-mode %s\n", snd_strerror(err));
      }
      else MSG_V("alsa-init: pcm opend in %s\n", str_block_mode);

      snd_pcm_hw_params_malloc(&priv->hwparams);
      snd_pcm_sw_params_malloc(&priv->swparams);

      // setting hw-parameters
      if ((err = snd_pcm_hw_params_any(priv->handler, priv->hwparams)) < 0)
	{
	  MSG_ERR("alsa-init: unable to get initial parameters: %s\n",
		 snd_strerror(err));
	  return MPXP_False;
	}
    MSG_DBG2("snd_pcm_hw_params_any()\n");
      if (priv_conf.mmap) {
	snd_pcm_access_mask_t *mask = alloca(snd_pcm_access_mask_sizeof());
	snd_pcm_access_mask_none(mask);
	snd_pcm_access_mask_set(mask, SND_PCM_ACCESS_MMAP_INTERLEAVED);
	snd_pcm_access_mask_set(mask, SND_PCM_ACCESS_MMAP_NONINTERLEAVED);
	snd_pcm_access_mask_set(mask, SND_PCM_ACCESS_MMAP_COMPLEX);
	err = snd_pcm_hw_params_set_access_mask(priv->handler, priv->hwparams, mask);
	MSG_ERR("alsa-init: mmap set\n");
      } else {
	err = snd_pcm_hw_params_set_access(priv->handler, priv->hwparams,SND_PCM_ACCESS_RW_INTERLEAVED);
	MSG_DBG2("snd_pcm_hw_params_set_access(SND_PCM_ACCESS_RW_INTERLEAVED)\n");
      }
      if (err < 0) {
	MSG_ERR("alsa-init: unable to set access type: %s\n", snd_strerror(err));
	return MPXP_False;
      }
    } // end switch priv->handler (spdif)
    return MPXP_Ok;
} // end init

static MPXP_Rc __FASTCALL__ configure(ao_data_t* ao,unsigned rate_hz,unsigned channels,unsigned format)
{
    priv_t*priv=ao->priv;
    int err,i;
    size_t chunk_size=0,chunk_bytes,bits_per_sample,bits_per_frame;
    snd_pcm_uframes_t dummy;

    MSG_V("alsa-conf: requested format: %d Hz, %d channels, %s\n", rate_hz,
	channels, ao_format_name(format));

    ao->samplerate = rate_hz;
    ao->bps = channels * rate_hz;
    ao->format = format;
    ao->channels = channels;
    ao->outburst = OUTBURST;
    //ao->buffersize = MAX_OUTBURST; // was 16384

    priv->format=fmt2alsa(format);

    switch(priv->format) {
      case SND_PCM_FORMAT_S16_LE:
      case SND_PCM_FORMAT_U16_LE:
      case SND_PCM_FORMAT_S16_BE:
      case SND_PCM_FORMAT_U16_BE:
	ao->bps *= 2;
	break;
      case SND_PCM_FORMAT_S32_LE:
      case SND_PCM_FORMAT_S32_BE:
      case SND_PCM_FORMAT_U32_LE:
      case SND_PCM_FORMAT_U32_BE:
      case SND_PCM_FORMAT_FLOAT_BE:
      case SND_PCM_FORMAT_FLOAT_LE:
	ao->bps *= 4;
	break;
      case SND_PCM_FORMAT_S24_LE:
      case SND_PCM_FORMAT_S24_BE:
      case SND_PCM_FORMAT_U24_LE:
      case SND_PCM_FORMAT_U24_BE:
	ao->bps *= 3;
	break;
      case -1:
	MSG_ERR("alsa-conf: invalid format (%s) requested - output disabled\n",
	       ao_format_name(format));
	return MPXP_False;
      default:
	break;
    }
    priv->bytes_per_sample = ao->bps / ao->samplerate;

    if ((err = snd_pcm_hw_params_set_format(priv->handler, priv->hwparams,
					      priv->format)) < 0) {
	MSG_ERR("alsa-conf: unable to set format(%s): %s\n",
		 snd_pcm_format_name(priv->format),
		 snd_strerror(err));
	MSG_HINT("Please try one of: ");
	for(i=0;i<SND_PCM_FORMAT_LAST;i++)
	    if (!(snd_pcm_hw_params_test_format(priv->handler, priv->hwparams, i)))
		MSG_HINT("%s ",snd_pcm_format_name(i));
	MSG_HINT("\n");
	return MPXP_False;
    }
    MSG_DBG2("snd_pcm_hw_params_set_format(%i)\n",priv->format);

    if ((err = snd_pcm_hw_params_set_rate_near(priv->handler, priv->hwparams, &ao->samplerate, 0)) < 0) {
	MSG_ERR("alsa-conf: unable to set samplerate %u: %s\n",
		ao->samplerate,
		snd_strerror(err));
	return MPXP_False;
    }
    MSG_DBG2("snd_pcm_hw_params_set_rate_near(%i)\n",ao->samplerate);

    if ((err = snd_pcm_hw_params_set_channels(priv->handler, priv->hwparams,
						ao->channels)) < 0) {
	MSG_ERR("alsa-conf: unable to set %u channels: %s\n",
		ao->channels,
		snd_strerror(err));
	return MPXP_False;
    }
    MSG_DBG2("snd_pcm_hw_params_set_channels(%i)\n",ao->channels);
#ifdef BUFFERTIME
    {
	int dir;
	unsigned period_time,alsa_buffer_time = 500000; /* buffer time in us */

	if ((err = snd_pcm_hw_params_set_buffer_time_near(priv->handler, priv->hwparams, &alsa_buffer_time, &dir)) < 0) {
	    MSG_ERR("alsa-init: unable to set buffer time near: %s\n",
		snd_strerror(err));
	    return MPXP_False;
	}
	MSG_DBG2("snd_pcm_hw_set_buffer_time_near(%i)\n",alsa_buffer_time);

	period_time = alsa_buffer_time/4;
	if ((err = snd_pcm_hw_params_set_period_time_near(priv->handler, priv->hwparams, &period_time, &dir)) < 0) {
	  /* original: alsa_buffer_time/ao->bps */
	    MSG_ERR("alsa-init: unable to set period time: %s\n",
		snd_strerror(err));
	    return MPXP_False;
	}
	MSG_DBG2("snd_pcm_hw_set_period_time_near(%i)\n",period_time);
	MSG_V("alsa-init: buffer_time: %d, period_time :%d\n",alsa_buffer_time, period_time);
    }
#else
    {
        int dir=0;
	unsigned period_time=100000; /* period time in us */
	snd_pcm_uframes_t size;
	if ((err = snd_pcm_hw_params_set_period_time_near(priv->handler, priv->hwparams, &period_time, &dir)) < 0) {
	    MSG_ERR("alsa-init: unable to set period_time: %s\n", snd_strerror(err));
	    return MPXP_False;
	}
	MSG_DBG2("snd_pcm_hw_set_period_time(%i)\n",period_time);

	//get chunksize
	if ((err = snd_pcm_hw_params_get_period_size(priv->hwparams, &size, &dir)) < 0) {
	    MSG_ERR("alsa-init: unable to get period_size: %s\n", snd_strerror(err));
	    return MPXP_False;
	}
	MSG_DBG2("snd_pcm_hw_get_period_size(%i)\n",size);
	chunk_size=size;
    }
#endif
        // gets buffersize for control
    if ((err = snd_pcm_hw_params_get_buffer_size(priv->hwparams,&dummy)) < 0) {
	MSG_ERR("alsa-conf: unable to get buffersize: %s\n", snd_strerror(err));
	return MPXP_False;
    } else {
	ao->buffersize = dummy * priv->bytes_per_sample;
	MSG_V("alsa-conf: got buffersize=%i\n", ao->buffersize);
    }
    MSG_DBG2("snd_pcm_hw_params_get_buffer_size(%i)\n",dummy);
    bits_per_sample = snd_pcm_format_physical_width(priv->format);
    MSG_DBG2("%i=snd_pcm_hw_format_pohysical_width()\n",bits_per_sample);
    bits_per_frame = bits_per_sample * channels;
    chunk_bytes = chunk_size * bits_per_frame / 8;

    MSG_V("alsa-conf: bits per sample (bps)=%i, bits per frame (bpf)=%i, chunk_bytes=%i\n",bits_per_sample,bits_per_frame,chunk_bytes);

    /* finally install hardware parameters */
    if ((err = snd_pcm_hw_params(priv->handler, priv->hwparams)) < 0) {
	MSG_ERR("alsa-conf: unable to set hw-parameters: %s\n",
		 snd_strerror(err));
	return MPXP_False;
    }
    MSG_DBG2("snd_pcm_hw_params()\n");
    // setting sw-params (only avail-min) if noblocking mode was choosed
    if (priv_conf.noblock) {
	if ((err = snd_pcm_sw_params_current(priv->handler, priv->swparams)) < 0) {
	    MSG_ERR("alsa-conf: unable to get parameters: %s\n",snd_strerror(err));
	    return MPXP_False;
	}

	//set min available frames to consider pcm ready (4)
	//increased for nonblock-mode should be set dynamically later
	if ((err = snd_pcm_sw_params_set_avail_min(priv->handler, priv->swparams, 4)) < 0) {
	    MSG_ERR("alsa-conf: unable to set avail_min %s\n",snd_strerror(err));
	    return MPXP_False;
	}

	if ((err = snd_pcm_sw_params(priv->handler, priv->swparams)) < 0) {
	    MSG_ERR("alsa-conf: unable to install sw-params\n");
	    return MPXP_False;
	}

    }//end swparams

    if ((err = snd_pcm_prepare(priv->handler)) < 0) {
	MSG_ERR("alsa-conf: pcm prepare error: %s\n", snd_strerror(err));
	return MPXP_False;
    }
    // end setting hw-params
    MSG_V("alsa-conf: %d Hz/%d channels/%d bpf/%d bytes buffer/%s\n",
	ao->samplerate, ao->channels, priv->bytes_per_sample, ao->buffersize,
	snd_pcm_format_description(priv->format));
    return MPXP_Ok;
} // end configure

/* close audio device */
static void uninit(ao_data_t* ao)
{
    int err;
    priv_t*priv=ao->priv;
    if(!priv->handler) {
	MSG_ERR("alsa-uninit: no handler defined!\n");
	mp_free(priv);
	return;
    }

    if (!priv_conf.noblock) {
	if ((err = snd_pcm_drain(priv->handler)) < 0) {
	    MSG_ERR("alsa-uninit: pcm drain error: %s\n", snd_strerror(err));
	    mp_free(priv);
	    return;
	}
    }

    if ((err = snd_pcm_close(priv->handler)) < 0) {
	MSG_ERR("alsa-uninit: pcm close error: %s\n", snd_strerror(err));
	mp_free(priv);
	return;
    } else {
	priv->handler = NULL;
	MSG_V("alsa-uninit: pcm closed\n");
    }
    snd_pcm_hw_params_free(priv->hwparams);
    snd_pcm_sw_params_free(priv->swparams);
    mp_free(priv);
}

static void audio_pause(ao_data_t* ao)
{
    priv_t*priv=ao->priv;
    int err;

    if (!priv_conf.noblock) {
	//drain causes error in nonblock-mode!
	if ((err = snd_pcm_drain(priv->handler)) < 0) {
	    MSG_ERR("alsa-pause: pcm drain error: %s\n", snd_strerror(err));
	    return;
	}
    } else {
	MSG_V("alsa-pause: paused nonblock\n");
	return;
    }
}

static void audio_resume(ao_data_t* ao)
{
    priv_t*priv=ao->priv;
    int err;

    if ((err = snd_pcm_prepare(priv->handler)) < 0) {
	MSG_ERR("alsa-resume: pcm prepare error: %s\n", snd_strerror(err));
	return;
    }
}

/* stop playing and empty buffers (for seeking/pause) */
static void reset(ao_data_t* ao)
{
    priv_t*priv=ao->priv;
    int err;

    if ((err = snd_pcm_drop(priv->handler)) < 0) {
	MSG_ERR("alsa-reset: pcm drop error: %s\n", snd_strerror(err));
	return;
    }
    if ((err = snd_pcm_prepare(priv->handler)) < 0) {
	MSG_ERR("alsa-reset: pcm prepare error: %s\n", snd_strerror(err));
	return;
    }
    return;
}

#ifdef USE_POLL
static int __FASTCALL__ wait_for_poll(snd_pcm_t *handle, struct pollfd *ufds, unsigned int count)
{
    unsigned short revents;

    while (1) {
	poll(ufds, count, -1);
	snd_pcm_poll_descriptors_revents(handle, ufds, count, &revents);
	if (revents & POLLERR) return -EIO;
	if (revents & POLLOUT) return 0;
    }
}
#endif

#ifndef timersub
#define timersub(a, b, result) \
do { \
    (result)->tv_sec = (a)->tv_sec - (b)->tv_sec; \
    (result)->tv_usec = (a)->tv_usec - (b)->tv_usec; \
    if ((result)->tv_usec < 0) { \
		--(result)->tv_sec; \
		(result)->tv_usec += 1000000; \
    } \
} while (0)
#endif

/* I/O error handler */
static int __FASTCALL__ xrun(ao_data_t* ao,const char *str_mode)
{
    priv_t*priv=ao->priv;
    int err;
    snd_pcm_status_t *status;

    snd_pcm_status_alloca(&status);

    if ((err = snd_pcm_status(priv->handler, status))<0) {
	MSG_ERR("status error: %s", snd_strerror(err));
	return 0;
    }

    if (snd_pcm_status_get_state(status) == SND_PCM_STATE_XRUN) {
	struct timeval now, diff, tstamp;
	gettimeofday(&now, 0);
	snd_pcm_status_get_trigger_tstamp(status, &tstamp);
	timersub(&now, &tstamp, &diff);
	MSG_V("alsa-%s: xrun of at least %.3f msecs. resetting stream\n",
	   str_mode,
	   diff.tv_sec * 1000 + diff.tv_usec / 1000.0);
    }

    if ((err = snd_pcm_prepare(priv->handler))<0) {
	MSG_ERR("xrun: prepare error: %s", snd_strerror(err));
	return 0;
    }

    return 1; /* ok, data should be accepted again */
}

static unsigned __FASTCALL__ play_normal(ao_data_t* ao,any_t* data, unsigned len);
static unsigned __FASTCALL__ play_mmap(ao_data_t* ao,any_t* data, unsigned len);

static unsigned __FASTCALL__ play(ao_data_t* ao,any_t* data, unsigned len, unsigned flags)
{
    unsigned result;
    UNUSED(flags);
    MSG_DBG2("[ao_alsa] %s playing %i bytes\n",priv_conf.mmap?"mmap":"normal",len);
    if (priv_conf.mmap)	result = play_mmap(ao,data, len);
    else		result = play_normal(ao,data, len);
    return result;
}

/*
    plays 'len' bytes of 'data'
    returns: number of bytes played
    modified last at 29.06.02 by jp
    thanxs for marius <marius@rospot.com> for giving us the light ;)
*/

static unsigned __FASTCALL__ play_normal(ao_data_t* ao,any_t* data, unsigned len)
{
    priv_t*priv=ao->priv;
    //priv->bytes_per_sample is always 4 for 2 chn S16_LE
    unsigned num_frames = len / priv->bytes_per_sample;
    char *output_samples = (char *)data;
    snd_pcm_sframes_t res = 0;

    //fprintf(stderr,"alsa-play: frames=%i, len=%i\n",num_frames,len);

    if (!priv->handler) {
	MSG_ERR("alsa-play: device configuration error");
	return 0;
    }

    while (num_frames > 0) {
	res = snd_pcm_writei(priv->handler, (any_t*)output_samples, num_frames);
	if (res == -EAGAIN) {
	    snd_pcm_wait(priv->handler, 1000);
	} else if (res == -EPIPE) { /* underrun */
	    if (xrun(ao,"play") <= 0) {
		MSG_ERR("alsa-play: xrun reset error");
		return 0;
	    }
        } else if (res == -ESTRPIPE) { /* suspend */
	    MSG_WARN("alsa-play: pcm in suspend mode. trying to resume\n");
	    while ((res = snd_pcm_resume(priv->handler)) == -EAGAIN) sleep(1);
        } else if (res < 0) {
	    MSG_ERR("alsa-play: unknown status, trying to reset soundcard\n");
	    if ((res = snd_pcm_prepare(priv->handler)) < 0) {
		MSG_ERR("alsa-play: snd prepare error");
		return 0;
		break;
	    }
	}

	if (res > 0) {
	    /* output_samples += ao->channels * res; */
	    output_samples += res * priv->bytes_per_sample;
	    num_frames -= res;
	}
    } //end while

    if (res < 0) {
	MSG_ERR("alsa-play: write error %s", snd_strerror(res));
	return 0;
    }
    return res < 0 ? 0 : len;
}

/* mmap-mode mainly based on descriptions by Joshua Haberman <joshua@haberman.com>
 * 'An overview of the ALSA API' http://people.debian.org/~joshua/x66.html
 * and some help by Paul Davis <pbd@op.net> */

static unsigned __FASTCALL__ play_mmap(ao_data_t* ao,any_t* data, unsigned len)
{
    priv_t*priv=ao->priv;
    snd_pcm_sframes_t commitres, frames_available;
    snd_pcm_uframes_t frames_transmit, size, offset;
    const snd_pcm_channel_area_t *area;
    any_t*outbuffer;
    unsigned result;

#ifdef USE_POLL //seems not really be needed
    struct pollfd *ufds;
    int count;

    count = snd_pcm_poll_descriptors_count (priv->handler);
    ufds = mp_malloc(sizeof(struct pollfd) * count);
    snd_pcm_poll_descriptors(priv->handler, ufds, count);

    //first wait_for_poll
    if (err = (wait_for_poll(priv->handler, ufds, count) < 0)) {
	if (snd_pcm_state(priv->handler) == SND_PCM_STATE_XRUN ||
	    snd_pcm_state(priv->handler) == SND_PCM_STATE_SUSPENDED) {
		xrun("play");
	}
    }
#endif

    outbuffer = alloca(ao->buffersize);

    //don't trust get_space() ;)
    frames_available = snd_pcm_avail_update(priv->handler) * priv->bytes_per_sample;
    if (frames_available < 0) xrun(ao,"play");

    if (frames_available < 4) {
	if (priv->first) {
	    priv->first = 0;
	    snd_pcm_start(priv->handler);
	} else { //FIXME should break and return 0?
	    snd_pcm_wait(priv->handler, -1);
	    priv->first = 1;
	}
    }

    /* len is simply the available bufferspace got by get_space()
     * but real avail_buffer in frames is ab/priv->bytes_per_sample */
    size = len / priv->bytes_per_sample;

    //if (verbose)
    //printf("len: %i size %i, f_avail %i, bps %i ...\n", len, size, frames_available, priv->bytes_per_sample);

    frames_transmit = size;

  /* prepare areas and set sw-pointers
   * frames_transmit returns the real available buffer-size
   * sometimes != frames_available cause of ringbuffer 'emulation' */
    snd_pcm_mmap_begin(priv->handler, &area, &offset, &frames_transmit);

  /* this is specific to interleaved streams (or non-interleaved
   * streams with only one channel) */
    outbuffer = ((char *) area->addr + (area->first + area->step * offset) / 8); //8

    //write data
    memcpy(outbuffer, data, (frames_transmit * priv->bytes_per_sample));
    commitres = snd_pcm_mmap_commit(priv->handler, offset, frames_transmit);

    if (commitres < 0 || (snd_pcm_uframes_t)commitres != frames_transmit) {
	if (snd_pcm_state(priv->handler) == SND_PCM_STATE_XRUN ||
	snd_pcm_state(priv->handler) == SND_PCM_STATE_SUSPENDED) {
	    xrun(ao,"play");
	}
    }

    //if (verbose)
    //printf("mmap ft: %i, cres: %i\n", frames_transmit, commitres);

    /* 	err = snd_pcm_area_copy(&area, offset, &data, offset, len, priv->format); */
    /* 	if (err < 0) { */
    /* 	  printf("area-copy-error\n"); */
    /* 	  return 0; */
    /* 	} */

    //calculate written frames!
    result = commitres * priv->bytes_per_sample;


    /* if (verbose) { */
    /* if (len == result) */
    /* printf("result: %i, frames written: %i ...\n", result, frames_transmit); */
    /* else */
    /* printf("result: %i, frames written: %i, result != len ...\n", result, frames_transmit); */
    /* } */

    //mplayer doesn't like -result
    if ((int)result < 0) result = 0;

#ifdef USE_POLL
    mp_free(ufds);
#endif

    return result;
}

typedef enum space_status_e {
    GET_SPACE_OPEN,
    GET_SPACE_PREPARED,
    GET_SPACE_RUNNING,
    GET_SPACE_PAUSED,
    GET_SPACE_XRUN,
    GET_SPACE_UNDEFINED
}space_status;
/* how many byes are mp_free in the buffer */
static unsigned get_space(ao_data_t* ao)
{
    priv_t*priv=ao->priv;
    snd_pcm_status_t *status;
    int ret,st;
    space_status e_status=GET_SPACE_UNDEFINED;

    //snd_pcm_sframes_t avail_frames = 0;

    if ((ret = snd_pcm_status_malloc(&status)) < 0) {
	MSG_ERR("alsa-space: memory allocation error: %s\n", snd_strerror(ret));
	return 0;
    }

    if ((ret = snd_pcm_status(priv->handler, status)) < 0) {
	MSG_ERR("alsa-space: cannot get pcm status: %s\n", snd_strerror(ret));
	return 0;
    }

    switch((st=snd_pcm_status_get_state(status))) {
	case SND_PCM_STATE_OPEN:
	    e_status = GET_SPACE_OPEN;
	case SND_PCM_STATE_PREPARED:
	    if (e_status!=GET_SPACE_OPEN) {
		e_status = GET_SPACE_PREPARED;
		priv->first = 1;
		ret = snd_pcm_status_get_avail(status) * priv->bytes_per_sample;
		if (ret == 0) //ugly workaround for hang in mmap-mode
		    ret = 10;
		break;
	    }
	case SND_PCM_STATE_RUNNING:
	    ret = snd_pcm_status_get_avail(status) * priv->bytes_per_sample;
	    //avail_frames = snd_pcm_avail_update(priv->handler) * priv->bytes_per_sample;
	    if (e_status!=GET_SPACE_OPEN && e_status!=GET_SPACE_PREPARED)
		e_status = GET_SPACE_RUNNING;
	    break;
	case SND_PCM_STATE_PAUSED:
	    MSG_V("alsa-space: paused");
	    e_status = GET_SPACE_PAUSED;
	    ret = 0;
	    break;
	case SND_PCM_STATE_XRUN:
	    xrun(ao,"space");
	    e_status = GET_SPACE_XRUN;
	    priv->first = 1;
	    ret = 0;
	    break;
	default:
	    e_status = GET_SPACE_UNDEFINED;
	    ret = snd_pcm_status_get_avail(status) * priv->bytes_per_sample;
	    if (ret <= 0) {
		xrun(ao,"space");
	    }
    }

    if (e_status!=GET_SPACE_RUNNING)
	MSG_V("alsa-space: mp_free space = %i, status=%i, %i --\n", ret, st, e_status);
    snd_pcm_status_free(status);

    if (ret < 0) {
	MSG_ERR("negative value!!\n");
	ret = 0;
    }

    return ret;
}

/* delay in seconds between first and last sample in buffer */
static float get_delay(ao_data_t* ao)
{
    priv_t*priv=ao->priv;
    if (priv->handler) {
	snd_pcm_status_t *status;
	int r;
	float ret;

	if ((ret = snd_pcm_status_malloc(&status)) < 0) {
	    MSG_ERR("alsa-delay: memory allocation error: %s\n", snd_strerror(ret));
	    return 0;
	}

	if ((ret = snd_pcm_status(priv->handler, status)) < 0) {
	    MSG_ERR("alsa-delay: cannot get pcm status: %s\n", snd_strerror(ret));
	    return 0;
	}

	switch(snd_pcm_status_get_state(status)) {
	    case SND_PCM_STATE_OPEN:
	    case SND_PCM_STATE_PREPARED:
	    case SND_PCM_STATE_RUNNING:
		r=snd_pcm_status_get_delay(status);
		ret = (float)r/(float)ao->samplerate;
		break;
	    default:
		ret = 0;
	}
	snd_pcm_status_free(status);

	if (ret < 0) ret = 0;
	return ret;
    } else return 0;
}
