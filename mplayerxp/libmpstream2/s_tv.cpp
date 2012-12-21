#include "mpxp_config.h"
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
#include "libao3/afmt.h"
#include "mrl.h"

#include "tvi/tv.h"
#include "tvi/frequencies.h"
#include "stream_msg.h"
#include "mpxp_help.h"

#include "stream.h"
#include "libmpdemux/demuxer.h"
#include "libmpdemux/stheader.h"

#include "libvo2/img_format.h"
#include "osdep/fastmemcpy.h"
#include "libao3/audio_out.h"

#include "stream_msg.h"

namespace mpxp {
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

/* ================== DEMUX_TV ===================== */
/*
  Return value:
    0 = EOF(?) or no stream
    1 = successfully read a packet
*/

int __FASTCALL__ demux_tv_fill_buffer(Demuxer *demux, Demuxer_Stream *ds, tvi_handle_t *tvh)
{
    Demuxer_Packet* dp;
    u_int len;

    len = 0;

    /* ================== ADD AUDIO PACKET =================== */

    if (ds==demux->audio && tv_param.noaudio == 0 &&
	tvh->functions->control(tvh->priv,
				TVI_CONTROL_IS_AUDIO, 0) == TVI_CONTROL_TRUE)
	{
	len = tvh->functions->get_audio_framesize(tvh->priv);

	dp=new(zeromem) Demuxer_Packet(len);
	dp->pts=tvh->functions->grab_audio_frame(tvh->priv, dp->buffer(),len);
	demux->audio->add_packet(dp);
	}

    /* ================== ADD VIDEO PACKET =================== */

    if (ds==demux->video && tvh->functions->control(tvh->priv,
			    TVI_CONTROL_IS_VIDEO, 0) == TVI_CONTROL_TRUE)
	{
	len = tvh->functions->get_video_framesize(tvh->priv);
	dp=new(zeromem) Demuxer_Packet(len);
	dp->pts=tvh->functions->grab_video_frame(tvh->priv, dp->buffer(), len);
	demux->video->add_packet(dp);
	 }

    return 1;
}

extern int __FASTCALL__ tv_set_freq(tvi_handle_t *tvh, unsigned long freq);
int __FASTCALL__ stream_open_tv(tvi_handle_t *tvh)
{
    int i;
    const tvi_functions_t *funcs = tvh->functions;
    int picture_format = 0;
    if (funcs->control(tvh->priv, TVI_CONTROL_IS_VIDEO, 0) != TVI_CONTROL_TRUE)
    {
	mpxp_err<<"Error: no video input present!"<<std::endl;
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
	mpxp_err<<"Unknown format given: "<<tv_param.outfmt<<std::endl;
	mpxp_v<<"Using default: Planar YV12"<<std::endl;
	picture_format = IMGFMT_YV12;
    }
    funcs->control(tvh->priv, TVI_CONTROL_VID_SET_FORMAT, &picture_format);

    /* set width */
    if (tv_param.width != -1)
    {
	if (funcs->control(tvh->priv, TVI_CONTROL_VID_CHK_WIDTH, &tv_param.width) == TVI_CONTROL_TRUE)
	    funcs->control(tvh->priv, TVI_CONTROL_VID_SET_WIDTH, &tv_param.width);
	else
	{
	    mpxp_err<<"Unable set requested width: "<<tv_param.width<<std::endl;
	    funcs->control(tvh->priv, TVI_CONTROL_VID_GET_WIDTH, &tv_param.width);
	}
    }

    /* set height */
    if (tv_param.height != -1)
    {
	if (funcs->control(tvh->priv, TVI_CONTROL_VID_CHK_HEIGHT, &tv_param.height) == TVI_CONTROL_TRUE)
	    funcs->control(tvh->priv, TVI_CONTROL_VID_SET_HEIGHT, &tv_param.height);
	else
	{
	    mpxp_err<<"Unable set requested height: "<<tv_param.height<<std::endl;
	    funcs->control(tvh->priv, TVI_CONTROL_VID_GET_HEIGHT, &tv_param.height);
	}
    }

    /* set some params got from cmdline */
    funcs->control(tvh->priv, TVI_CONTROL_SPC_SET_INPUT, &tv_param.input);

    /* select video norm */
    if (!strcasecmp(tv_param.norm, "pal"))
	tvh->norm = TV_NORM_PAL;
    else if (!strcasecmp(tv_param.norm, "ntsc"))
	tvh->norm = TV_NORM_NTSC;
    else if (!strcasecmp(tv_param.norm, "secam"))
	tvh->norm = TV_NORM_SECAM;

    mpxp_v<<"Selected norm: "<<tv_param.norm<<std::endl;
    funcs->control(tvh->priv, TVI_CONTROL_TUN_SET_NORM, &tvh->norm);

    if (funcs->control(tvh->priv, TVI_CONTROL_IS_TUNER, 0) != TVI_CONTROL_TRUE)
    {
	mpxp_warn<<"Selected input hasn't got a tuner!"<<std::endl;
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
	mpxp_warn<<"Unable to find selected channel list! ("<<tv_param.chanlist<<")"<<std::endl;
    else
	mpxp_v<<"Selected channel list: "<<chanlists[tvh->chanlist].name<<" (including "<<chanlists[tvh->chanlist].count<<" channels)"<<std::endl;
    if (tv_param.freq && tv_param.channel) {
	mpxp_warn<<"You can't set frequency and channel simultanly!"<<std::endl;
	goto done;
    }

    /* we need to set frequency */
    if (tv_param.freq)
    {
	unsigned long freq = atof(tv_param.freq)*16;

	/* set freq in MHz */
	funcs->control(tvh->priv, TVI_CONTROL_TUN_SET_FREQ, &freq);

	funcs->control(tvh->priv, TVI_CONTROL_TUN_GET_FREQ, &freq);
	mpxp_v<<"Selected frequency: "<<freq<<" ("<<((float)freq/16)<<")"<<std::endl;
    }

    if (tv_param.channel)
    {
	struct CHANLIST cl;

	mpxp_v<<"Requested channel: "<<tv_param.channel<<std::endl;
	for (i = 0; i < chanlists[tvh->chanlist].count; i++)
	{
	    cl = tvh->chanlist_s[i];
	    if (!strcasecmp(cl.name, tv_param.channel))
	    {
		tvh->channel = i;
		mpxp_v<<"Selected channel: "<<cl.name<<" (freq: "<<((float)cl.freq/1000)<<")"<<std::endl;
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
    funcs->control(tvh->priv, TVI_CONTROL_VID_GET_FORMAT, &sh_video->fourcc);
//    if (IMGFMT_IS_RGB(sh_video->wtag) || IMGFMT_IS_BGR(sh_video->wtag))
//	sh_video->wtag = 0x0;

    /* set FPS  */

    if(!sh_video->fps)
    {
	int tmp;
	if (funcs->control(tvh->priv, TVI_CONTROL_VID_GET_FPS, &tmp) != TVI_CONTROL_TRUE)
	     sh_video->fps = 25.0f; /* on PAL */
	else sh_video->fps = tmp;
    }

    if (tv_param.fps != -1.0f)
	sh_video->fps = tv_param.fps;

    mpxp_v<<"fps: "<<sh_video->fps<<", frametime: "<<(1.0f/sh_video->fps)<<std::endl;

#ifdef HAVE_TV_BSDBT848
    /* If playback only mode, go to immediate mode, fail silently */
    if(tv_param.immediate == 1)
	{
	funcs->control(tvh->priv, TVI_CONTROL_IMMEDIATE, 0);
	tv_param.noaudio = 1;
	}
#endif

    /* set width */
    funcs->control(tvh->priv, TVI_CONTROL_VID_GET_WIDTH, &sh_video->src_w);

    /* set height */
    funcs->control(tvh->priv, TVI_CONTROL_VID_GET_HEIGHT, &sh_video->src_h);

    mpxp_v<<"Output size: "<<sh_video->src_w<<"x"<<sh_video->src_h<<std::endl;

    demuxer->video->sh = sh_video;
    sh_video->ds = demuxer->video;
    demuxer->video->id = 0;

//    demuxer->seekable = 0;

    /* here comes audio init */

    if (tv_param.noaudio == 0 && funcs->control(tvh->priv, TVI_CONTROL_IS_AUDIO, 0) == TVI_CONTROL_TRUE)
    {
	int audio_format;
	int sh_audio_format;

	/* yeah, audio is present */

	funcs->control(tvh->priv, TVI_CONTROL_AUD_SET_SAMPLERATE,
				  &tv_param.audiorate);

	if (funcs->control(tvh->priv, TVI_CONTROL_AUD_GET_FORMAT, &audio_format) != TVI_CONTROL_TRUE)
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
		mpxp_err<<"Audio type '"<<ao_format_name(audio_format)<<" ("<<std::hex<<audio_format<<")' unsupported!"<<std::endl;
		goto no_audio;
	}

	sh_audio = demuxer->new_sh_audio(0);

	funcs->control(tvh->priv, TVI_CONTROL_AUD_GET_SAMPLERATE,
		   &sh_audio->rate);
	funcs->control(tvh->priv, TVI_CONTROL_AUD_GET_CHANNELS,
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

	mpxp_v<<"  TV audio: "<<sh_audio->wf->nChannels<<" channels, "<<sh_audio->wf->wBitsPerSample<<" bits, "<<sh_audio->wf->nSamplesPerSec<<" Hz"<<std::endl;

	demuxer->audio->sh = sh_audio;
	sh_audio->ds = demuxer->audio;
	demuxer->audio->id = 0;
    }
no_audio:

    return funcs->start(tvh->priv);
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

    mpxp_err<< "No such driver: "<<tv_param.driver<<std::endl;
    return(NULL);
}

int __FASTCALL__ tv_init(tvi_handle_t *tvh)
{
    mpxp_v<<"Selected driver: "<<tvh->info->short_name<<std::endl;
    mpxp_v<<" name: "<<tvh->info->name<<std::endl;
    mpxp_v<<" author: "<<tvh->info->author<<std::endl;
    if (tvh->info->comment)
	mpxp_v<<" comment: "<<tvh->info->comment<<std::endl;

    return tvh->functions->init(tvh->priv);
}

int __FASTCALL__ tv_uninit(tvi_handle_t *tvh)
{
    return tvh->functions->uninit(tvh->priv);
}

/* utilities for mplayer (not mencoder!!) */
int __FASTCALL__ tv_set_color_options(tvi_handle_t *tvh, int opt, int value)
{
    const tvi_functions_t *funcs = tvh->functions;

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
	    funcs->control(tvh->priv, TVI_CONTROL_VID_SET_BRIGHTNESS, &value);
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
	    funcs->control(tvh->priv, TVI_CONTROL_VID_SET_HUE, &value);
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
	    funcs->control(tvh->priv, TVI_CONTROL_VID_SET_SATURATION, &value);
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
	    funcs->control(tvh->priv, TVI_CONTROL_VID_SET_CONTRAST, &value);
	    break;
	default:
	    mpxp_warn<<"Unknown color option ("<<opt<<") specified!"<<std::endl;
    }

    return(1);
}

int __FASTCALL__ tv_set_freq(tvi_handle_t *tvh, unsigned long freq)
{
    if (tvh->functions->control(tvh->priv, TVI_CONTROL_IS_TUNER, 0) == TVI_CONTROL_TRUE)
    {
//	unsigned long freq = atof(tv_param.freq)*16;

	/* set freq in MHz */
	tvh->functions->control(tvh->priv, TVI_CONTROL_TUN_SET_FREQ, &freq);

	tvh->functions->control(tvh->priv, TVI_CONTROL_TUN_GET_FREQ, &freq);
	mpxp_v<<"Current frequency: "<<freq<<" ("<<((float)freq/16)<<")"<<std::endl;
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
	    mpxp_v<<"Selected channel: "<<cl.name<<" (freq: "<<((float)cl.freq/1000)<<")"<<std::endl;
	    tv_set_freq(tvh, (unsigned long)(((float)cl.freq/1000)*16));
	}
    }

    if (direction == TV_CHANNEL_HIGHER)
    {
	if (tvh->channel+1 < chanlists[tvh->chanlist].count)
	{
	    cl = tvh->chanlist_s[++tvh->channel];
	    mpxp_v<<"Selected channel: "<<cl.name<<" (freq: "<<((float)cl.freq/1000)<<")"<<std::endl;
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

/* fill demux->video and demux->audio */
    class Tv_Stream_Interface : public Stream_Interface {
	public:
	    Tv_Stream_Interface(libinput_t& libinput);
	    virtual ~Tv_Stream_Interface();

	    virtual MPXP_Rc	open(const std::string& filename,unsigned flags);
	    virtual int		read(stream_packet_t * sp);
	    virtual off_t	seek(off_t off);
	    virtual off_t	tell() const;
	    virtual void	close();
	    virtual MPXP_Rc	ctrl(unsigned cmd,any_t* param);
	    virtual Stream::type_e type() const;
	    virtual off_t	size() const;
	    virtual off_t	sector_size() const;
	    virtual std::string mime_type() const;
	private:
	    void		cmd_handler(unsigned cmd) const;
	    tvi_handle_t*	priv;
    };

Tv_Stream_Interface::Tv_Stream_Interface(libinput_t&libinput):Stream_Interface(libinput) {}
Tv_Stream_Interface::~Tv_Stream_Interface() { delete priv; }

MPXP_Rc Tv_Stream_Interface::open(const std::string& filename,unsigned flags)
{
    UNUSED(flags);
    mrl_parse_params(filename,tvopts_conf);
    /* create tvi handler */
    if(!(priv = tv_begin())) goto tv_err;
    /* preinit */
    if (!tv_init(priv))	goto tv_err;
    if (!stream_open_tv(priv)) goto tv_err;
    return MPXP_Ok;

    /* something went wrong - uninit */
tv_err:
    mpxp_err<<"Can not initialize TV"<<std::endl;
    tv_uninit(priv);
    return MPXP_False;
}

int Tv_Stream_Interface::read(stream_packet_t*sp)
{
    UNUSED(sp);
    return 0;
}

off_t Tv_Stream_Interface::seek(off_t pos) { return pos; }
off_t Tv_Stream_Interface::tell() const { return 0; }
void Tv_Stream_Interface::close() {}

void Tv_Stream_Interface::cmd_handler(unsigned cmd) const
{
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

MPXP_Rc Tv_Stream_Interface::ctrl(unsigned cmd,any_t*args)
{
    switch(cmd) {
	case SCRTL_MPXP_CMD:
	    cmd_handler((unsigned long)args);
	    return MPXP_Ok;
	default:
	    break;
    }
    return MPXP_Unknown;
}

Stream::type_e Tv_Stream_Interface::type() const { return Stream::Type_Stream; }
off_t	Tv_Stream_Interface::size() const { return -1; }
off_t	Tv_Stream_Interface::sector_size() const { return 0; }
std::string Tv_Stream_Interface::mime_type() const { return "application/octet-stream"; }

static Stream_Interface* query_interface(libinput_t& libinput) { return new(zeromem) Tv_Stream_Interface(libinput); }

extern const stream_interface_info_t tv_stream =
{
    "tv://",
    "reads multimedia stream directly from TV tunner",
    query_interface,
};
} // namespace mpxp
#endif

