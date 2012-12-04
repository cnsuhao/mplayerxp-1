#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
/*
    s_tv - TV stream interface
    --------------------------
    TV Interface for MPlayer

    (C) Alex Beregszaszi <alex@naxine.org>

    API idea based on libvo2

    Feb 19, 2002: Significant rewrites by Charles R. Henrich (henrich@msu.edu)
				to add support for audio, and bktr *BSD support.
*/
#include "mplayerxp.h"
#ifdef USE_TV
#include <stdlib.h>
#include <string.h>
#include "stream.h"
#include "stream_internal.h"
#include "input2/input.h"
#include "libao2/afmt.h"
#include "mrl.h"

/* some default values */
struct tv_param : public Opaque {
    public:
	tv_param();
	virtual ~tv_param() {}

	int audiorate;
	int noaudio;
#ifdef HAVE_TV_BSDBT848
	int immediate;
#endif
	const char *freq;
	const char *channel;
	const char *norm;
	const char *chanlist;
	const char *device;
	const char *driver;
	int width;
	int height;
	int input; /* used in v4l and bttv */
	const char *outfmt;
	float fps;
};
static tv_param tv_param;
tv_param::tv_param() {
    audiorate = 44100;
    noaudio = 0;
#ifdef HAVE_TV_BSDBT848
    immediate = 0;
#endif
    freq = NULL;
    channel = NULL;
    norm = "pal";
    chanlist = "europe-east";
    device = NULL;
    driver = "dummy";
    width = -1;
    height = -1;
    input = 0; /* used in v4l and bttv */
    outfmt = "yv12";
    fps = -1.0;
}

static const mrl_config_t tvopts_conf[]={
    {"driver", &tv_param.driver, MRL_TYPE_STRING, 0, 0 },
    {"device", &tv_param.device, MRL_TYPE_STRING, 0, 0 },
    {"freq", &tv_param.freq, MRL_TYPE_STRING, 0, 0 },
    {"channel", &tv_param.channel, MRL_TYPE_STRING, 0, 0 },
    {"chanlist", &tv_param.chanlist, MRL_TYPE_STRING, 0, 0 },
    {"norm", &tv_param.norm, MRL_TYPE_STRING, 0, 0 },
    {"width", &tv_param.width, MRL_TYPE_INT, 0, 4096 },
    {"height", &tv_param.height, MRL_TYPE_INT, 0, 4096 },
    {"input", &tv_param.input, MRL_TYPE_INT, 0, 20 },
    {"outfmt", &tv_param.outfmt, MRL_TYPE_STRING, 0, 0 },
    {"fps", &tv_param.fps, MRL_TYPE_FLOAT, 0, 100.0 },
    {NULL, NULL, 0, 0, 0 }
};

#include "tvi/tv.h"
#include "tvi/frequencies.h"
#include "stream_msg.h"
#include "help_mp.h"

#include "stream.h"
#include "libmpdemux/demuxer.h"
#include "libmpdemux/stheader.h"

#include "libao2/afmt.h"
#include "libvo/img_format.h"
#include "osdep/fastmemcpy.h"
#include "libao2/audio_out.h"

#include "stream_msg.h"

/* ================== DEMUX_TV ===================== */
/*
  Return value:
    0 = EOF(?) or no stream
    1 = successfully read a packet
*/
/* fill demux->video and demux->audio */

int __FASTCALL__ demux_tv_fill_buffer(Demuxer *demux, Demuxer_Stream *ds, tvi_handle_t *tvh)
{
    Demuxer_Packet* dp;
    u_int len;

    len = 0;

    /* ================== ADD AUDIO PACKET =================== */

    if (ds==demux->audio && tv_param.noaudio == 0 &&
	tvh->functions->control(reinterpret_cast<priv_s*>(tvh->priv),
				TVI_CONTROL_IS_AUDIO, 0) == TVI_CONTROL_TRUE)
	{
	len = tvh->functions->get_audio_framesize(reinterpret_cast<priv_s*>(tvh->priv));

	dp=new(zeromem) Demuxer_Packet(len);
	dp->pts=tvh->functions->grab_audio_frame(reinterpret_cast<priv_s*>(tvh->priv), dp->buffer(),len);
	demux->audio->add_packet(dp);
	}

    /* ================== ADD VIDEO PACKET =================== */

    if (ds==demux->video && tvh->functions->control(reinterpret_cast<priv_s*>(tvh->priv),
			    TVI_CONTROL_IS_VIDEO, 0) == TVI_CONTROL_TRUE)
	{
	len = tvh->functions->get_video_framesize(reinterpret_cast<priv_s*>(tvh->priv));
	dp=new(zeromem) Demuxer_Packet(len);
	dp->pts=tvh->functions->grab_video_frame(reinterpret_cast<priv_s*>(tvh->priv), dp->buffer(), len);
	demux->video->add_packet(dp);
	 }

    return 1;
}

extern int __FASTCALL__ tv_set_freq(tvi_handle_t *tvh, unsigned long freq);
int __FASTCALL__ stream_open_tv(stream_t *stream, tvi_handle_t *tvh)
{
    int i;
    const tvi_functions_t *funcs = tvh->functions;
    int picture_format = 0;
    UNUSED(stream);
    if (funcs->control(reinterpret_cast<priv_s*>(tvh->priv), TVI_CONTROL_IS_VIDEO, 0) != TVI_CONTROL_TRUE)
    {
	MSG_ERR( "Error: no video input present!\n");
	return 0;
    }

    if (!strcasecmp(tv_param.outfmt, "yv12"))
	picture_format = IMGFMT_YV12;
    else if (!strcasecmp(tv_param.outfmt, "i420"))
	picture_format = IMGFMT_I420;
    else if (!strcasecmp(tv_param.outfmt, "uyvy"))
	picture_format = IMGFMT_UYVY;
    else if (!strcasecmp(tv_param.outfmt, "yuy2"))
	picture_format = IMGFMT_YUY2;
    else if (!strcasecmp(tv_param.outfmt, "rgb32"))
	picture_format = IMGFMT_RGB32;
    else if (!strcasecmp(tv_param.outfmt, "rgb24"))
	picture_format = IMGFMT_RGB24;
    else if (!strcasecmp(tv_param.outfmt, "rgb16"))
	picture_format = IMGFMT_RGB16;
    else if (!strcasecmp(tv_param.outfmt, "rgb15"))
	picture_format = IMGFMT_RGB15;
    else
    {
	MSG_ERR( "Unknown format given: %s\n", tv_param.outfmt);
	MSG_V( "Using default: Planar YV12\n");
	picture_format = IMGFMT_YV12;
    }
    funcs->control(reinterpret_cast<priv_s*>(tvh->priv), TVI_CONTROL_VID_SET_FORMAT, &picture_format);

    /* set width */
    if (tv_param.width != -1)
    {
	if (funcs->control(reinterpret_cast<priv_s*>(tvh->priv), TVI_CONTROL_VID_CHK_WIDTH, &tv_param.width) == TVI_CONTROL_TRUE)
	    funcs->control(reinterpret_cast<priv_s*>(tvh->priv), TVI_CONTROL_VID_SET_WIDTH, &tv_param.width);
	else
	{
	    MSG_ERR( "Unable set requested width: %d\n", tv_param.width);
	    funcs->control(reinterpret_cast<priv_s*>(tvh->priv), TVI_CONTROL_VID_GET_WIDTH, &tv_param.width);
	}
    }

    /* set height */
    if (tv_param.height != -1)
    {
	if (funcs->control(reinterpret_cast<priv_s*>(tvh->priv), TVI_CONTROL_VID_CHK_HEIGHT, &tv_param.height) == TVI_CONTROL_TRUE)
	    funcs->control(reinterpret_cast<priv_s*>(tvh->priv), TVI_CONTROL_VID_SET_HEIGHT, &tv_param.height);
	else
	{
	    MSG_ERR( "Unable set requested height: %d\n", tv_param.height);
	    funcs->control(reinterpret_cast<priv_s*>(tvh->priv), TVI_CONTROL_VID_GET_HEIGHT, &tv_param.height);
	}
    }

    /* set some params got from cmdline */
    funcs->control(reinterpret_cast<priv_s*>(tvh->priv), TVI_CONTROL_SPC_SET_INPUT, &tv_param.input);

    /* select video norm */
    if (!strcasecmp(tv_param.norm, "pal"))
	tvh->norm = TV_NORM_PAL;
    else if (!strcasecmp(tv_param.norm, "ntsc"))
	tvh->norm = TV_NORM_NTSC;
    else if (!strcasecmp(tv_param.norm, "secam"))
	tvh->norm = TV_NORM_SECAM;

    MSG_V( "Selected norm: %s\n", tv_param.norm);
    funcs->control(reinterpret_cast<priv_s*>(tvh->priv), TVI_CONTROL_TUN_SET_NORM, &tvh->norm);

    if (funcs->control(reinterpret_cast<priv_s*>(tvh->priv), TVI_CONTROL_IS_TUNER, 0) != TVI_CONTROL_TRUE)
    {
	MSG_WARN( "Selected input hasn't got a tuner!\n");
	goto done;
    }

    /* select channel list */
    for (i = 0; chanlists[i].name != NULL; i++)
    {
	if (!strcasecmp(chanlists[i].name, tv_param.chanlist))
	{
	    tvh->chanlist = i;
	    tvh->chanlist_s = chanlists[i].list;
	    break;
	}
    }

    if (tvh->chanlist == -1)
	MSG_WARN( "Unable to find selected channel list! (%s)\n",
	    tv_param.chanlist);
    else
	MSG_V( "Selected channel list: %s (including %d channels)\n",
	    chanlists[tvh->chanlist].name, chanlists[tvh->chanlist].count);

    if (tv_param.freq && tv_param.channel)
    {
	MSG_WARN( "You can't set frequency and channel simultanly!\n");
	goto done;
    }

    /* we need to set frequency */
    if (tv_param.freq)
    {
	unsigned long freq = atof(tv_param.freq)*16;

	/* set freq in MHz */
	funcs->control(reinterpret_cast<priv_s*>(tvh->priv), TVI_CONTROL_TUN_SET_FREQ, &freq);

	funcs->control(reinterpret_cast<priv_s*>(tvh->priv), TVI_CONTROL_TUN_GET_FREQ, &freq);
	MSG_V( "Selected frequency: %lu (%.3f)\n",
	    freq, (float)freq/16);
    }

    if (tv_param.channel)
    {
	struct CHANLIST cl;

	MSG_V( "Requested channel: %s\n", tv_param.channel);
	for (i = 0; i < chanlists[tvh->chanlist].count; i++)
	{
	    cl = tvh->chanlist_s[i];
	    if (!strcasecmp(cl.name, tv_param.channel))
	    {
		tvh->channel = i;
		MSG_V( "Selected channel: %s (freq: %.3f)\n",
		    cl.name, (float)cl.freq/1000);
		tv_set_freq(tvh, (unsigned long)(((float)cl.freq/1000)*16));
		break;
	    }
	}
    }

done:
    /* also start device! */
	return 1;
}

int __FASTCALL__ demux_open_tv(Demuxer *demuxer, tvi_handle_t *tvh)
{
    sh_video_t *sh_video = NULL;
    sh_audio_t *sh_audio = NULL;
    const tvi_functions_t *funcs = tvh->functions;

    sh_video = demuxer->new_sh_video(0);

    /* get IMAGE FORMAT */
    funcs->control(reinterpret_cast<priv_s*>(tvh->priv), TVI_CONTROL_VID_GET_FORMAT, &sh_video->fourcc);
//    if (IMGFMT_IS_RGB(sh_video->wtag) || IMGFMT_IS_BGR(sh_video->wtag))
//	sh_video->wtag = 0x0;

    /* set FPS  */

    if(!sh_video->fps)
    {
	int tmp;
	if (funcs->control(reinterpret_cast<priv_s*>(tvh->priv), TVI_CONTROL_VID_GET_FPS, &tmp) != TVI_CONTROL_TRUE)
	     sh_video->fps = 25.0f; /* on PAL */
	else sh_video->fps = tmp;
    }

    if (tv_param.fps != -1.0f)
	sh_video->fps = tv_param.fps;

    MSG_V("fps: %f, frametime: %f\n", sh_video->fps, 1.0f/sh_video->fps);

#ifdef HAVE_TV_BSDBT848
    /* If playback only mode, go to immediate mode, fail silently */
    if(tv_param.immediate == 1)
	{
	funcs->control(reinterpret_cast<priv_s*>(tvh->priv), TVI_CONTROL_IMMEDIATE, 0);
	tv_param.noaudio = 1;
	}
#endif

    /* set width */
    funcs->control(reinterpret_cast<priv_s*>(tvh->priv), TVI_CONTROL_VID_GET_WIDTH, &sh_video->src_w);

    /* set height */
    funcs->control(reinterpret_cast<priv_s*>(tvh->priv), TVI_CONTROL_VID_GET_HEIGHT, &sh_video->src_h);

    MSG_V( "Output size: %dx%d\n", sh_video->src_w, sh_video->src_h);

    demuxer->video->sh = sh_video;
    sh_video->ds = demuxer->video;
    demuxer->video->id = 0;

//    demuxer->seekable = 0;

    /* here comes audio init */

    if (tv_param.noaudio == 0 && funcs->control(reinterpret_cast<priv_s*>(tvh->priv), TVI_CONTROL_IS_AUDIO, 0) == TVI_CONTROL_TRUE)
    {
	int audio_format;
	int sh_audio_format;

	/* yeah, audio is present */

	funcs->control(reinterpret_cast<priv_s*>(tvh->priv), TVI_CONTROL_AUD_SET_SAMPLERATE,
				  &tv_param.audiorate);

	if (funcs->control(reinterpret_cast<priv_s*>(tvh->priv), TVI_CONTROL_AUD_GET_FORMAT, &audio_format) != TVI_CONTROL_TRUE)
	    goto no_audio;

	switch(audio_format)
	{
	    case AFMT_U8:
	    case AFMT_S8:
	    case AFMT_U16_LE:
	    case AFMT_U16_BE:
	    case AFMT_S16_LE:
	    case AFMT_S16_BE:
	    case AFMT_S32_LE:
	    case AFMT_S32_BE:
		sh_audio_format = 0x1; /* PCM */
		break;
	    case AFMT_IMA_ADPCM:
	    case AFMT_MU_LAW:
	    case AFMT_A_LAW:
	    case AFMT_MPEG:
	    case AFMT_AC3:
	    default:
		MSG_ERR( "Audio type '%s (%x)' unsupported!\n",
		    ao_format_name(audio_format), audio_format);
		goto no_audio;
	}

	sh_audio = demuxer->new_sh_audio(0);

	funcs->control(reinterpret_cast<priv_s*>(tvh->priv), TVI_CONTROL_AUD_GET_SAMPLERATE,
		   &sh_audio->rate);
	funcs->control(reinterpret_cast<priv_s*>(tvh->priv), TVI_CONTROL_AUD_GET_CHANNELS,
		   &sh_audio->nch);

	sh_audio->wtag = sh_audio_format;
	sh_audio->afmt = audio_format;

	sh_audio->i_bps = sh_audio->o_bps =
	    sh_audio->rate * afmt2bps(sh_audio->afmt) * sh_audio->nch;

	// emulate WF for win32 codecs:
	sh_audio->wf = (WAVEFORMATEX *)mp_malloc(sizeof(WAVEFORMATEX));
	sh_audio->wf->wFormatTag = sh_audio->wtag;
	sh_audio->wf->nChannels = sh_audio->nch;
	sh_audio->wf->wBitsPerSample = afmt2bps(sh_audio->afmt) * 8;
	sh_audio->wf->nSamplesPerSec = sh_audio->rate;
	sh_audio->wf->nBlockAlign = afmt2bps(sh_audio->afmt) * sh_audio->nch;
	sh_audio->wf->nAvgBytesPerSec = sh_audio->i_bps;

	MSG_V( "  TV audio: %d channels, %d bits, %d Hz\n",
	  sh_audio->wf->nChannels, sh_audio->wf->wBitsPerSample,
	  sh_audio->wf->nSamplesPerSec);

	demuxer->audio->sh = sh_audio;
	sh_audio->ds = demuxer->audio;
	demuxer->audio->id = 0;
    }
no_audio:

    return(funcs->start(reinterpret_cast<priv_s*>(tvh->priv)));
}

/* ================== STREAM_TV ===================== */
extern tvi_handle_t *tvi_init_dummy(const char *device);
extern tvi_handle_t *tvi_init_v4l(const char *device);
tvi_handle_t * __FASTCALL__ tv_begin(void)
{
    if (!strcmp(tv_param.driver, "dummy"))
	return (tvi_handle_t *)tvi_init_dummy(tv_param.device);
#ifdef HAVE_TV_V4L
    if (!strcmp(tv_param.driver, "v4l"))
	return (tvi_handle_t *)tvi_init_v4l(tv_param.device);
#endif
#ifdef HAVE_TV_BSDBT848
    if (!strcmp(tv_param.driver, "bsdbt848"))
	return (tvi_handle_t *)tvi_init_bsdbt848(tv_param.device);
#endif

    MSG_ERR( "No such driver: %s\n", tv_param.driver);
    return(NULL);
}

int __FASTCALL__ tv_init(tvi_handle_t *tvh)
{
    MSG_V( "Selected driver: %s\n", tvh->info->short_name);
    MSG_V( " name: %s\n", tvh->info->name);
    MSG_V( " author: %s\n", tvh->info->author);
    if (tvh->info->comment)
	MSG_V( " comment: %s\n", tvh->info->comment);

    return(tvh->functions->init(reinterpret_cast<priv_s*>(tvh->priv)));
}

int __FASTCALL__ tv_uninit(tvi_handle_t *tvh)
{
    return(tvh->functions->uninit(reinterpret_cast<priv_s*>(tvh->priv)));
}

/* utilities for mplayer (not mencoder!!) */
int __FASTCALL__ tv_set_color_options(tvi_handle_t *tvh, int opt, int value)
{
    const tvi_functions_t *funcs = reinterpret_cast<const tvi_functions_t*>(tvh->functions);

    switch(opt)
    {
	case TV_COLOR_BRIGHTNESS:
	    if (value == 50)
		value = 32768;
	    if (value > 50)
	    {
		value *= 100;
		value += 32768;
	    }
	    if (value < 50)
	    {
		int i;
		value *= 100;
		i = value;
		value = 32768 - i;
	    }
	    funcs->control(reinterpret_cast<priv_s*>(tvh->priv), TVI_CONTROL_VID_SET_BRIGHTNESS, &value);
	    break;
	case TV_COLOR_HUE:
	    if (value == 50)
		value = 32768;
	    if (value > 50)
	    {
		value *= 100;
		value += 32768;
	    }
	    if (value < 50)
	    {
		int i;
		value *= 100;
		i = value;
		value = 32768 - i;
	    }
	    funcs->control(reinterpret_cast<priv_s*>(tvh->priv), TVI_CONTROL_VID_SET_HUE, &value);
	    break;
	case TV_COLOR_SATURATION:
	    if (value == 50)
		value = 32512;
	    if (value > 50)
	    {
		value *= 100;
		value += 32512;
	    }
	    if (value < 50)
	    {
		int i;
		value *= 100;
		i = value;
		value = 32512 - i;
	    }
	    funcs->control(reinterpret_cast<priv_s*>(tvh->priv), TVI_CONTROL_VID_SET_SATURATION, &value);
	    break;
	case TV_COLOR_CONTRAST:
	    if (value == 50)
		value = 27648;
	    if (value > 50)
	    {
		value *= 100;
		value += 27648;
	    }
	    if (value < 50)
	    {
		int i;
		value *= 100;
		i = value;
		value = 27648 - i;
	    }
	    funcs->control(reinterpret_cast<priv_s*>(tvh->priv), TVI_CONTROL_VID_SET_CONTRAST, &value);
	    break;
	default:
	    MSG_WARN( "Unknown color option (%d) specified!\n", opt);
    }

    return(1);
}

int __FASTCALL__ tv_set_freq(tvi_handle_t *tvh, unsigned long freq)
{
    if (tvh->functions->control(reinterpret_cast<priv_s*>(tvh->priv), TVI_CONTROL_IS_TUNER, 0) == TVI_CONTROL_TRUE)
    {
//	unsigned long freq = atof(tv_param.freq)*16;

	/* set freq in MHz */
	tvh->functions->control(reinterpret_cast<priv_s*>(tvh->priv), TVI_CONTROL_TUN_SET_FREQ, &freq);

	tvh->functions->control(reinterpret_cast<priv_s*>(tvh->priv), TVI_CONTROL_TUN_GET_FREQ, &freq);
	MSG_V( "Current frequency: %lu (%.3f)\n",
	    freq, (float)freq/16);
    }
    return 0;
}

int __FASTCALL__ tv_step_channel(tvi_handle_t *tvh, int direction)
{
    struct CHANLIST cl;

    if (direction == TV_CHANNEL_LOWER)
    {
	if (tvh->channel-1 >= 0)
	{
	    cl = tvh->chanlist_s[--tvh->channel];
	    MSG_V( "Selected channel: %s (freq: %.3f)\n",
		cl.name, (float)cl.freq/1000);
	    tv_set_freq(tvh, (unsigned long)(((float)cl.freq/1000)*16));
	}
    }

    if (direction == TV_CHANNEL_HIGHER)
    {
	if (tvh->channel+1 < chanlists[tvh->chanlist].count)
	{
	    cl = tvh->chanlist_s[++tvh->channel];
	    MSG_V( "Selected channel: %s (freq: %.3f)\n",
		cl.name, (float)cl.freq/1000);
	    tv_set_freq(tvh, (unsigned long)(((float)cl.freq/1000)*16));
	}
    }
    return 0;
}

int __FASTCALL__ tv_step_norm(tvi_handle_t *tvh)
{
    UNUSED(tvh);
    return 0;
}

int __FASTCALL__ tv_step_chanlist(tvi_handle_t *tvh)
{
    UNUSED(tvh);
    return 0;
}

static MPXP_Rc __FASTCALL__ _tv_open(any_t*libinput,stream_t*stream,const char *filename,unsigned flags)
{
    UNUSED(flags);
    UNUSED(libinput);
    mrl_parse_params(filename,tvopts_conf);
    /* create tvi handler */
    if(!(stream->priv = tv_begin())) goto tv_err;

    /* preinit */
    if (!tv_init(reinterpret_cast<tvi_handle_t*>(stream->priv)))	goto tv_err;

    if (!stream_open_tv(stream, reinterpret_cast<tvi_handle_t*>(stream->priv)))
	goto tv_err;

    stream->type = STREAMTYPE_STREAM;
    check_pin("stream",stream->pin,STREAM_PIN);
    return MPXP_Ok;

    /* something went wrong - uninit */
tv_err:
    MSG_ERR("Can not initialize TV\n");
    tv_uninit(reinterpret_cast<tvi_handle_t*>(stream->priv));
    return MPXP_False;
}

static int __FASTCALL__ _tv_read(stream_t *stream,stream_packet_t*sp)
{
    UNUSED(stream);
    UNUSED(sp);
    return 0;
}

static off_t __FASTCALL__ _tv_seek(stream_t *stream,off_t pos)
{
    UNUSED(stream);
    return pos;
}

static off_t __FASTCALL__ _tv_tell(const stream_t *stream)
{
    UNUSED(stream);
    return 0;
}

static void __FASTCALL__ _tv_close(stream_t*stream)
{
    delete stream->priv;
}

static void __FASTCALL__ _tv_cmd_handler(const stream_t *s,unsigned cmd)
{
    tvi_handle_t* priv=reinterpret_cast<tvi_handle_t*>(s->priv);
    switch(cmd)
    {
    case MP_CMD_TV_STEP_CHANNEL_UP:
	  tv_step_channel(priv, TV_CHANNEL_HIGHER);
	break;
    case MP_CMD_TV_STEP_CHANNEL_DOWN:
	  tv_step_channel(priv, TV_CHANNEL_LOWER);
	break;
    case MP_CMD_TV_STEP_NORM:
	tv_step_norm(priv);
	break;
    case MP_CMD_TV_STEP_CHANNEL_LIST:
	tv_step_chanlist(priv);
	break;
    }
}

static MPXP_Rc __FASTCALL__ _tv_ctrl(const stream_t *s,unsigned cmd,any_t*args)
{
    switch(cmd) {
	case SCRTL_MPXP_CMD:
	    _tv_cmd_handler(s,(unsigned long)args);
	    return MPXP_Ok;
	default:
	    break;
    }
    return MPXP_Unknown;
}

extern const stream_driver_t tv_stream=
{
    "tv://",
    "reads multimedia stream directly from TV tunner",
    _tv_open,
    _tv_read,
    _tv_seek,
    _tv_tell,
    _tv_close,
    _tv_ctrl
};
#endif

