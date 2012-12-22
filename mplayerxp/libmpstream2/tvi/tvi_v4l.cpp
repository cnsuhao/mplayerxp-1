#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
/*
  Video 4 Linux input

  (C) Alex Beregszaszi <alex@naxine.org>

  Some ideas are based on xawtv/libng's grab-v4l.c written by
    Gerd Knorr <kraxel@bytesex.org>

  CODE IS UNDER DEVELOPMENT, NO FEATURE REQUESTS PLEASE!
*/
#if defined(USE_TV) && defined(HAVE_TV_V4L)

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <linux/videodev.h>
#include <linux/soundcard.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <string.h>

#include "libao3/afmt.h"
#include "libao3/audio_out.h"
#include "libvo2/img_format.h"

#include "tv.h"
#include "stream_msg.h"

static const tvi_info_t info = {
	"Video 4 Linux input",
	"v4l",
	"Alex Beregszaszi <alex@naxine.org>",
	"under development"
};

#define MAX_AUDIO_CHANNELS	10

struct priv_s {
    /* general */
    char			*video_device;
    int				video_fd;
    struct video_capability	capability;
    struct video_channel	*channels;
    int				act_channel;
    struct video_tuner		tuner;

    /* video */
    struct video_picture	picture;
    int				format;		/* output format */
    int				width;
    int				height;
    int				bytesperline;
    int				fps;

    struct video_mbuf		mbuf;
    unsigned char		*mmap;
    struct video_mmap		*buf;
    int				nbuf;
    int				queue;

    /* audio */
    int				audio_id;
    char			*audio_device;
    struct video_audio		audio[MAX_AUDIO_CHANNELS];
    int				audio_fd;
    int				audio_channels[MAX_AUDIO_CHANNELS];
    int				audio_format[MAX_AUDIO_CHANNELS];
    int				audio_samplesize[MAX_AUDIO_CHANNELS];
    int				audio_samplerate[MAX_AUDIO_CHANNELS];
    int				audio_blocksize;

    /* other */
    double			starttime;
};

#include "tvi_def.h"

static const char *device_cap2name[] = {
    "capture", "tuner", "teletext", "overlay", "chromakey", "clipping",
    "frameram", "scales", "monochrome", "subcapture", "mpeg-decoder",
    "mpeg-encoder", "mjpeg-decoder", "mjpeg-encoder", NULL
};

#if 0
static const char *device_palette2name[] = {
    "-", "grey", "hi240", "rgb16", "rgb24", "rgb32", "rgb15", "yuv422",
    "yuyv", "uyvy", "yuv420", "yuv411", "raw", "yuv422p", "yuv411p",
    "yuv420p", "yuv410p", NULL
};
#endif
#define PALETTE(x) ((x < sizeof(device_pal)/sizeof(char*)) ? device_pal[x] : "UNKNOWN")

static const char *audio_mode2name[] = {
    "unknown", "mono", "stereo", "language1", "language2", NULL
};

static int palette2depth(int palette)
{
    switch(palette)
    {
	/* component */
	case VIDEO_PALETTE_RGB555:
	    return 15;
	case VIDEO_PALETTE_RGB565:
	    return 16;
	case VIDEO_PALETTE_RGB24:
	    return 24;
	case VIDEO_PALETTE_RGB32:
	    return 32;
	/* planar */
	case VIDEO_PALETTE_YUV411P:
	case VIDEO_PALETTE_YUV420P:
	case VIDEO_PALETTE_YUV410P:
	    return 12;
	/* packed */
	case VIDEO_PALETTE_YUV422P:
	case VIDEO_PALETTE_YUV422:
	case VIDEO_PALETTE_YUYV:
	case VIDEO_PALETTE_UYVY:
	case VIDEO_PALETTE_YUV420:
	case VIDEO_PALETTE_YUV411:
	    return 16;
    }
    return -1;
}

static int format2palette(int format)
{
    switch(format)
    {
	case IMGFMT_RGB15:
	    return VIDEO_PALETTE_RGB555;
	case IMGFMT_RGB16:
	    return VIDEO_PALETTE_RGB565;
	case IMGFMT_RGB24:
	    return VIDEO_PALETTE_RGB24;
	case IMGFMT_RGB32:
	    return VIDEO_PALETTE_RGB32;
	case IMGFMT_YV12:
	case IMGFMT_I420:
	    return VIDEO_PALETTE_YUV420P;
	case IMGFMT_UYVY:
	    return VIDEO_PALETTE_YUV422;
	case IMGFMT_YUY2:
	    return VIDEO_PALETTE_YUYV;
    }
    return -1;
}

tvi_handle_t *tvi_init_v4l(const char *device)
{
    tvi_handle_t *h;
    struct priv_s *priv;

    h = new_handle();
    if (!h)
	return NULL;

    priv = h->priv;

    /* set video device name */
    if (!device)
	priv->video_device = mp_strdup("/dev/video");
    else
	priv->video_device = mp_strdup(device);

    /* allocation failed */
    if (!priv->video_device) {
	free_handle(h);
	return NULL;
    }

    /* set audio device name */
    priv->audio_device = mp_strdup("/dev/dsp");

    return h;
}

static int init(struct priv_s *priv)
{
    int i;

    priv->video_fd = open(priv->video_device, O_RDWR);
    mpxp_dbg2<<"Video fd: "<<priv->video_fd<<", %x"<<std::hex<<priv->video_device<<std::endl;
    if (priv->video_fd == -1)
    {
	mpxp_err<<"unable to open '"<<priv->video_device<<"': "<<strerror(errno)<<std::endl;
	goto err;
    }

    priv->fps = 25; /* pal */

    /* get capabilities (priv->capability is needed!) */
    if (ioctl(priv->video_fd, VIDIOCGCAP, &priv->capability) == -1)
    {
	mpxp_err<<"ioctl get capabilites failed: "<<strerror(errno)<<std::endl;
	goto err;
    }

    fcntl(priv->video_fd, F_SETFD, FD_CLOEXEC);

    mpxp_v<<"Selected device: "<<priv->capability.name<<std::endl;
    mpxp_v<<" Capabilites: ";
    for (i = 0; device_cap2name[i] != NULL; i++)
	if (priv->capability.type & (1 << i))
	    mpxp_v<<device_cap2name[i]<<" ";
    mpxp_v<<std::endl;
    mpxp_v<<" Device type: "<<priv->capability.type<<std::endl;
    mpxp_v<<" Supported sizes: "<<priv->capability.minwidth<<"x"<<priv->capability.minheight<<" => "<<priv->capability.maxwidth<<"x"<<priv->capability.maxheight<<std::endl;
    priv->width = priv->capability.minwidth;
    priv->height = priv->capability.minheight;
    mpxp_v<<" Inputs: "<<priv->capability.channels<<std::endl;

    priv->channels = (struct video_channel *)mp_mallocz(sizeof(struct video_channel)*priv->capability.channels);
    if (!priv->channels)
	goto malloc_failed;
    for (i = 0; i < priv->capability.channels; i++)
    {
	priv->channels[i].channel = i;
	if (ioctl(priv->video_fd, VIDIOCGCHAN, &priv->channels[i]) == -1)
	{
	    mpxp_err<<"ioctl get channel failed: "<<strerror(errno)<<std::endl;
	    break;
	}
	mpxp_v<<"  "<<priv->channels[i].name
		<<": "<<((priv->channels[i].flags & VIDEO_VC_TUNER) ? "tuner " : "")
		<<": "<<((priv->channels[i].flags & VIDEO_VC_AUDIO) ? "audio " : "")
		<<((priv->channels[i].flags & VIDEO_TYPE_TV) ? "tv " : "")
		<<((priv->channels[i].flags & VIDEO_TYPE_CAMERA) ? "camera " : "")
		<<" (tuner:"<<priv->channels[i].tuners
		<<", norm:"<<priv->channels[i].norm<<")"<<std::endl;
    }

    /* audio chanlist */
    if (priv->capability.audios) {
	mpxp_v<<" Audio devices: "<<priv->capability.audios<<std::endl;

	for (i = 0; i < priv->capability.audios; i++) {
	    if (i >= MAX_AUDIO_CHANNELS) {
		mpxp_err<<"no space for more audio channels (incrase in source!) ("<<i<<" > "<<MAX_AUDIO_CHANNELS<<")"<<std::endl;
		i = priv->capability.audios;
		break;
	    }

	    priv->audio[i].audio = i;
	    if (ioctl(priv->video_fd, VIDIOCGAUDIO, &priv->audio[i]) == -1) {
		mpxp_err<<"ioctl get audio failed: "<<strerror(errno)<<std::endl;
		break;
	    }

	    if (priv->audio[i].volume <= 0)
		priv->audio[i].volume = 100;
	    priv->audio[i].flags &= ~VIDEO_AUDIO_MUTE;
	    ioctl(priv->video_fd, VIDIOCSAUDIO, &priv->audio[i]);

	    switch(priv->audio[i].mode)
	    {
		case VIDEO_SOUND_MONO:
		case VIDEO_SOUND_LANG1:
		case VIDEO_SOUND_LANG2:
		    priv->audio_channels[i] = 1;
		    break;
		case VIDEO_SOUND_STEREO:
		    priv->audio_channels[i] = 2;
		    break;
	    }

	    priv->audio_format[i] = bps2afmt(2);
	    priv->audio_samplerate[i] = 44100;
	    priv->audio_samplesize[i] =
		priv->audio_samplerate[i]/8/priv->fps*
		priv->audio_channels[i];

	    /* display stuff */
	    mpxp_v<<"  "<<priv->audio[i].audio<<": "<<priv->audio[i].name<<":";
	    if (priv->audio[i].flags & VIDEO_AUDIO_MUTABLE)
		mpxp_v<<"muted="<<((priv->audio[i].flags & VIDEO_AUDIO_MUTE)?"yes":"no"));
	    mpxp_v<<"volume="<<priv->audio[i].volume<<" bass="<<priv->audio[i].bass<<" treble="<<priv->audio[i].treble<<" balance="<<priv->audio[i].balance<<" mode="<<audio_mode2name[priv->audio[i].mode]<<std::endl;
	    mpxp_v<<" channels: "<<priv->audio_channels[i]<<", samplerate: "<<priv->audio_samplerate[i]<<", samplesize: "<<priv->audio_samplesize[i]<<", format: "<<ao_format_name(priv->audio_format[i])<<std::endl;
	}
    }

    if (!(priv->capability.type & VID_TYPE_CAPTURE)) {
	mpxp_err<<"Only grabbing supported (for overlay use another program)"<<std::endl;
	goto err;
    }

    /* map grab buffer */
    if (ioctl(priv->video_fd, VIDIOCGMBUF, &priv->mbuf) == -1)
    {
	mpxp_err<<"ioctl get mbuf failed: "<<strerror(errno)<<std::endl;
	goto err;
    }

    mpxp_v<<"mbuf: size="<<priv->mbuf.size<<", frames="<<priv->mbuf.frames<<std::endl;
    priv->mmap = mmap(0, priv->mbuf.size, PROT_READ|PROT_WRITE,MAP_SHARED, priv->video_fd, 0);
    if (priv->mmap == (unsigned char *)-1) {
	mpxp_err<<"Unable to map memory for buffers: "<<strerror(errno)<<std::endl;
	goto err;
    }

    /* num of buffers */
    priv->nbuf = priv->mbuf.frames;

    /* video buffers */
    priv->buf = (struct video_mmap *)mp_mallocz(priv->nbuf * sizeof(struct video_mmap));
    if (!priv->buf)
	goto malloc_failed;
    /* audio init */
#if 1
    priv->audio_fd = open(priv->audio_device, O_RDONLY);
    if (priv->audio_fd < 0) mpxp_err<<"unable to open '"<<priv->audio_device<<"': "<<strerror(errno)<<std::endl;
    else {
	int ioctl_param;

	fcntl(priv->audio_fd, F_SETFL, O_NONBLOCK);

	ioctl_param = 0 ;
	mpxp_v<<"ioctl dsp getfmt: "<<ioctl(priv->audio_fd, SNDCTL_DSP_GETFMTS, &ioctl_param)<<std::endl;

	mpxp_v<<"Supported formats: %"<<std::hex<<ioctl_param<<std::endl;
	if (!(ioctl_param & priv->audio_format[priv->audio_id]))
	    mpxp_warn<<"notsupported format"<<std::endl;

	ioctl_param = priv->audio_format[priv->audio_id];
	mpxp_v<<"ioctl dsp setfmt: "<<ioctl(priv->audio_fd, SNDCTL_DSP_SETFMT, &ioctl_param)<<std::endl;

	if (priv->audio_channels[priv->audio_id] > 2) {
	    ioctl_param = priv->audio_channels[priv->audio_id];
	    mpxp_v<<"ioctl dsp channels: "<<ioctl(priv->audio_fd, SNDCTL_DSP_CHANNELS, &ioctl_param)<<std::endl;
	} else {
	    ioctl_param = (priv->audio_channels[priv->audio_id] == 2);
	    mpxp_v<<"ioctl dsp stereo: "<<ioctl(priv->audio_fd, SNDCTL_DSP_STEREO, &ioctl_param)<<" (req:"<<ioctl_param<<")"<<std::endl;
	}

	ioctl_param = priv->audio_samplerate[priv->audio_id];
	mpxp_v<<"ioctl dsp speed: "<<ioctl(priv->audio_fd, SNDCTL_DSP_SPEED, &ioctl_param)<<std::endl;

	mpxp_v<<"ioctl dsp trigger: "<<ioctl(priv->audio_fd, SNDCTL_DSP_GETTRIGGER, &ioctl_param)<<std::endl;
	mpxp_v<<"trigger: "<<std::hex<<ioctl_param;
	ioctl_param = PCM_ENABLE_INPUT;
	mpxp_v<<"ioctl dsp trigger: "<<ioctl(priv->audio_fd, SNDCTL_DSP_SETTRIGGER, &ioctl_param)<<std::endl;

	mpxp_v<<"ioctl dsp getblocksize: "<<ioctl(priv->audio_fd, SNDCTL_DSP_GETBLKSIZE, &priv->audio_blocksize)<<std::endl;
	mpxp_v<<"blocksize: "<<priv->audio_blocksize<<std::endl;
    }
#endif
    return 1;


malloc_failed:
    if (priv->channels)
	delete priv->channels;
    if (priv->buf)
	delete priv->buf;
err:
    if (priv->video_fd != -1)
	close(priv->video_fd);
    return 0;
}

static int uninit(struct priv_s *priv)
{
    close(priv->video_fd);

    priv->audio[priv->audio_id].volume = 0;
    priv->audio[priv->audio_id].flags |= VIDEO_AUDIO_MUTE;
    ioctl(priv->video_fd, VIDIOCSAUDIO, &priv->audio[priv->audio_id]);
    close(priv->audio_fd);

    return 1;
}

static int start(struct priv_s *priv)
{
    int i;


    if (ioctl(priv->video_fd, VIDIOCGPICT, &priv->picture) == -1) {
	mpxp_err<<"ioctl get picture failed: "<<strerror(errno)<<std::endl;
	return 0;
    }

    priv->picture.palette = format2palette(priv->wtag);
    priv->picture.depth = palette2depth(priv->picture.palette);
    priv->bytesperline = priv->width * priv->picture.depth / 8;

    mpxp_v<<"palette: "<<priv->picture.palette<<", depth: "<<priv->picture.depth<<", bytesperline: "<<priv->bytesperline<<std::endl;

    mpxp_v<<"Picture values:"<<std::endl;
    mpxp_v<<" Depth: "<<priv->picture.depth<<", Palette: "<<priv->picture.palette<<" (Format: "<<vo_format_name(priv->wtag)<<")"<<std::endl;
    mpxp_v<<" Brightness: "<<priv->picture.brightness<<", Hue: "<<priv->picture.hue<<", Colour: "<<priv->picture.colour<<", Contrast: "<<priv->picture.contrast<<std::endl;

    if (ioctl(priv->video_fd, VIDIOCSPICT, &priv->picture) == -1) {
	mpxp_err<<"ioctl set picture failed: "<<strerror(errno)<<std::endl;
	return 0;
    }

    priv->nbuf = priv->mbuf.frames;
    for (i=0; i < priv->nbuf; i++) {
	priv->buf[i].wtag = priv->picture.palette;
	priv->buf[i].frame = i;
	priv->buf[i].width = priv->width;
	priv->buf[i].height = priv->height;
    }

      struct timeval curtime;
      gettimeofday(&curtime, NULL);
      priv->starttime=curtime.tv_sec + curtime.tv_usec*.000001;

    return 1;
}

static int control(struct priv_s *priv, int cmd, any_t*arg)
{
    switch(cmd) {
	/* ========== GENERIC controls =========== */
	case TVI_CONTROL_IS_VIDEO: {
	    if (priv->capability.type & VID_TYPE_CAPTURE)
		return TVI_CONTROL_TRUE;
	    return TVI_CONTROL_FALSE;
	}
	case TVI_CONTROL_IS_AUDIO:
	    if (priv->channels[priv->act_channel].flags & VIDEO_VC_AUDIO)
	    {
		return TVI_CONTROL_TRUE;
	    }
	    return TVI_CONTROL_TRUE;
	case TVI_CONTROL_IS_TUNER:
	{
//	    if (priv->capability.type & VID_TYPE_TUNER)
	    if (priv->channels[priv->act_channel].flags & VIDEO_VC_TUNER)
		return TVI_CONTROL_TRUE;
	    return TVI_CONTROL_FALSE;
	}

	/* ========== VIDEO controls =========== */
	case TVI_CONTROL_VID_GET_FORMAT:
	{
	    int output_fmt = -1;

	    output_fmt = priv->wtag;
	    *(int *)arg = output_fmt;
	    mpxp_v<<"Output format: "<<vo_format_name(output_fmt)<<std::endl;
	    return TVI_CONTROL_TRUE;
	}
	case TVI_CONTROL_VID_SET_FORMAT:
	    priv->wtag = (int)*(any_t**)arg;
	    return TVI_CONTROL_TRUE;
	case TVI_CONTROL_VID_GET_PLANES:
	    *(int *)arg = 1; /* FIXME, also not needed at this time */
	    return TVI_CONTROL_TRUE;
	case TVI_CONTROL_VID_GET_BITS:
	    *(int *)arg = palette2depth(format2palette(priv->wtag));
	    return TVI_CONTROL_TRUE;
	case TVI_CONTROL_VID_GET_WIDTH:
	    *(int *)arg = priv->width;
	    return TVI_CONTROL_TRUE;
	case TVI_CONTROL_VID_CHK_WIDTH:
	{
	    int req_width = *(int *)arg;

	    mpxp_v<<"Requested width: "<<req_width<<std::endl;
	    if ((req_width >= priv->capability.minwidth) &&
		(req_width <= priv->capability.maxwidth))
		return TVI_CONTROL_TRUE;
	    return TVI_CONTROL_FALSE;
	}
	case TVI_CONTROL_VID_SET_WIDTH:
	    priv->width = *(int *)arg;
	    return TVI_CONTROL_TRUE;
	case TVI_CONTROL_VID_GET_HEIGHT:
	    *(int *)arg = priv->height;
	    return TVI_CONTROL_TRUE;
	case TVI_CONTROL_VID_CHK_HEIGHT:
	{
	    int req_height = *(int *)arg;

	    mpxp_v<<"Requested height: "<<req_height<<std::endl;
	    if ((req_height >= priv->capability.minheight) &&
		(req_height <= priv->capability.maxheight))
		return TVI_CONTROL_TRUE;
	    return TVI_CONTROL_FALSE;
	}
	case TVI_CONTROL_VID_SET_HEIGHT:
	    priv->height = *(int *)arg;
	    return TVI_CONTROL_TRUE;
	case TVI_CONTROL_VID_GET_PICTURE:
	    if (ioctl(priv->video_fd, VIDIOCGPICT, &priv->picture) == -1)
	    {
		mpxp_err<<"ioctl get picture failed: "<<strerror(errno)<<std::endl;
		return TVI_CONTROL_FALSE;
	    }
	    return TVI_CONTROL_TRUE;
	case TVI_CONTROL_VID_SET_PICTURE:
	    if (ioctl(priv->video_fd, VIDIOCSPICT, &priv->picture) == -1)
	    {
		mpxp_err<<"ioctl get picture failed: "<<strerror(errno)<<std::endl;
		return TVI_CONTROL_FALSE;
	    }
	    return TVI_CONTROL_TRUE;
	case TVI_CONTROL_VID_SET_BRIGHTNESS:
	    priv->picture.brightness = *(int *)arg;
	    control(priv, TVI_CONTROL_VID_SET_PICTURE, 0);
	    return TVI_CONTROL_TRUE;
	case TVI_CONTROL_VID_SET_HUE:
	    priv->picture.hue = *(int *)arg;
	    control(priv, TVI_CONTROL_VID_SET_PICTURE, 0);
	    return TVI_CONTROL_TRUE;
	case TVI_CONTROL_VID_SET_SATURATION:
	    priv->picture.colour = *(int *)arg;
	    control(priv, TVI_CONTROL_VID_SET_PICTURE, 0);
	    return TVI_CONTROL_TRUE;
	case TVI_CONTROL_VID_SET_CONTRAST:
	    priv->picture.contrast = *(int *)arg;
	    control(priv, TVI_CONTROL_VID_SET_PICTURE, 0);
	    return TVI_CONTROL_TRUE;
	case TVI_CONTROL_VID_GET_FPS:
	    *(int *)arg=priv->fps;
	    return TVI_CONTROL_TRUE;

	/* ========== TUNER controls =========== */
	case TVI_CONTROL_TUN_GET_FREQ:
	{
	    unsigned long freq;

	    if (ioctl(priv->video_fd, VIDIOCGFREQ, &freq) == -1)
	    {
		mpxp_err<<"ioctl get freq failed: "<<strerror(errno)<<std::endl;
		return TVI_CONTROL_FALSE;
	    }

	    /* tuner uses khz not mhz ! */
//	    if (priv->tuner.flags & VIDEO_TUNER_LOW)
//	        freq /= 1000;
	    *(unsigned long *)arg = freq;
	    return TVI_CONTROL_TRUE;
	}
	case TVI_CONTROL_TUN_SET_FREQ:
	{
	    /* argument is in MHz ! */
	    unsigned long freq = (unsigned long)*(any_t**)arg;

	    mpxp_v<<"requested frequency: "<<((float)freq/16)<<std::endl;

	    /* tuner uses khz not mhz ! */
//	    if (priv->tuner.flags & VIDEO_TUNER_LOW)
//	        freq *= 1000;
	    if (ioctl(priv->video_fd, VIDIOCSFREQ, &freq) == -1)
	    {
		mpxp_err<<"ioctl set freq failed: "<<strerror(errno)<<std::endl;
		return TVI_CONTROL_FALSE;
	    }
	    return TVI_CONTROL_TRUE;
	}
	case TVI_CONTROL_TUN_GET_TUNER:
	{
	    if (ioctl(priv->video_fd, VIDIOCGTUNER, &priv->tuner) == -1)
	    {
		mpxp_err<<"ioctl get tuner failed: "<<strerror(errno)<<std::endl;
		return TVI_CONTROL_FALSE;
	    }

	    mpxp_v<<"Tuner ("<<priv->tuner.name<<") range: "<<priv->tuner.rangelow<<" ->"<<priv->tuner.rangehigh<<std::endl;
	    return TVI_CONTROL_TRUE;
	}
	case TVI_CONTROL_TUN_SET_TUNER:
	{
	    if (ioctl(priv->video_fd, VIDIOCSTUNER, &priv->tuner) == -1)
	    {
		mpxp_err<<"ioctl get tuner failed: "<<strerror(errno)<<std::endl;
		return TVI_CONTROL_FALSE;
	    }
	    return TVI_CONTROL_TRUE;
	}
	case TVI_CONTROL_TUN_SET_NORM:
	{
	    int req_mode = *(int *)arg;

	    if ((!(priv->tuner.flags & VIDEO_TUNER_NORM)) ||
		((req_mode == VIDEO_MODE_PAL) && !(priv->tuner.flags & VIDEO_TUNER_PAL)) ||
		((req_mode == VIDEO_MODE_NTSC) && !(priv->tuner.flags & VIDEO_TUNER_NTSC)) ||
		((req_mode == VIDEO_MODE_SECAM) && !(priv->tuner.flags & VIDEO_TUNER_SECAM)))
	    {
		mpxp_err<<"Tuner isn't capable to set norm!"<<std::endl;
		return TVI_CONTROL_FALSE;
	    }

	    priv->tuner.mode = req_mode;

	    if (control(priv, TVI_CONTROL_TUN_SET_TUNER, &priv->tuner) != TVI_CONTROL_TRUE)
		return TVI_CONTROL_FALSE;
	    return TVI_CONTROL_TRUE;
	}
	case TVI_CONTROL_TUN_GET_NORM:
	{
	    *(int *)arg = priv->tuner.mode;

	    return TVI_CONTROL_TRUE;
	}

	/* ========== AUDIO controls =========== */
	case TVI_CONTROL_AUD_GET_FORMAT:
	{
	    *(int *)arg = priv->audio_format[priv->audio_id];
	    return TVI_CONTROL_TRUE;
	}
	case TVI_CONTROL_AUD_GET_CHANNELS:
	{
	    *(int *)arg = priv->audio_channels[priv->audio_id];
	    return TVI_CONTROL_TRUE;
	}
	case TVI_CONTROL_AUD_GET_SAMPLERATE:
	{
	    *(int *)arg = priv->audio_samplerate[priv->audio_id];
	    return TVI_CONTROL_TRUE;
	}
	case TVI_CONTROL_AUD_GET_SAMPLESIZE:
	{
	    *(int *)arg = priv->audio_samplesize[priv->audio_id]/8;
	    return TVI_CONTROL_TRUE;
	}
	case TVI_CONTROL_AUD_SET_SAMPLERATE:
	{
	    int tmp = priv->audio_samplerate[priv->audio_id] = *(int *)arg;

	    if (ioctl(priv->audio_fd, SNDCTL_DSP_SPEED, &tmp) == -1)
		return TVI_CONTROL_FALSE;
	    priv->audio_samplesize[priv->audio_id] =
		priv->audio_samplerate[priv->audio_id]/8/priv->fps*
		priv->audio_channels[priv->audio_id];
	    return TVI_CONTROL_TRUE;
	}
	/* ========== SPECIFIC controls =========== */
	case TVI_CONTROL_SPC_GET_INPUT:
	{
	    int req_chan = *(int *)arg;
	    int i;

	    for (i = 0; i < priv->capability.channels; i++)
	    {
		if (priv->channels[i].channel == req_chan)
		    break;
	    }

	    priv->act_channel = i;

	    if (ioctl(priv->video_fd, VIDIOCGCHAN, &priv->channels[i]) == -1)
	    {
		mpxp_err<<"ioctl get channel failed: "<<strerror(errno)<<std::endl;
		return TVI_CONTROL_FALSE;
	    }
	    return TVI_CONTROL_TRUE;
	}

	case TVI_CONTROL_SPC_SET_INPUT:
	{
	    struct video_channel chan;
	    int req_chan = *(int *)arg;
	    int i;

	    if (req_chan >= priv->capability.channels)
	    {
		mpxp_err<<"Invalid input requested: "<<req_chan<<", valid: 0-"<<priv->capability.channels<<std::endl;
		return TVI_CONTROL_FALSE;
	    }

	    for (i = 0; i < priv->capability.channels; i++)
	    {
		if (priv->channels[i].channel == req_chan)
		    chan = priv->channels[i];
	    }

	    if (ioctl(priv->video_fd, VIDIOCSCHAN, &chan) == -1)
	    {
		mpxp_err<<"ioctl set chan failed: "<<strerror(errno)<<std::endl;
		return TVI_CONTROL_FALSE;
	    }
	    mpxp_v<<"Using input: "<<chan.name<<std::endl;

	    priv->act_channel = i;

	    /* update tuner state */
//	    if (priv->capability.type & VID_TYPE_TUNER)
	    if (priv->channels[priv->act_channel].flags & VIDEO_VC_TUNER)
		control(priv, TVI_CONTROL_TUN_GET_TUNER, 0);

	    /* update local channel list */
	    control(priv, TVI_CONTROL_SPC_GET_INPUT, &req_chan);
	    return TVI_CONTROL_TRUE;
	}
    }

    return TVI_CONTROL_UNKNOWN;
}

static double grab_video_frame(struct priv_s *priv,unsigned char *buffer, int len)
{
    struct timeval curtime;
    double timestamp;
    int frame = priv->queue % priv->nbuf;
    int nextframe = (priv->queue+1) % priv->nbuf;

    if (ioctl(priv->video_fd, VIDIOCMCAPTURE, &priv->buf[nextframe]) == -1)
    {
	mpxp_err<<"ioctl mcapture failed: "<<strerror(errno)<<std::endl;
	return 0;
    }

    while (ioctl(priv->video_fd, VIDIOCSYNC, &priv->buf[frame].frame) < 0 &&
	(errno == EAGAIN || errno == EINTR));
	mpxp_dbg3<<"picture sync failed"<<std::endl;

    priv->queue++;
    gettimeofday(&curtime, NULL);
    timestamp=curtime.tv_sec + curtime.tv_usec*.000001;

    /* XXX also directrendering would be nicer! */
    /* 3 times copying the same picture to other buffer :( */

    /* copy the actual frame */
    memcpy(buffer, priv->mmap+priv->mbuf.offsets[frame], len);

    return timestamp-priv->starttime;
}

static int get_video_framesize(struct priv_s *priv)
{
    return priv->bytesperline * priv->height;
}

static double grab_audio_frame(struct priv_s *priv,unsigned char *buffer, int len)
{
    int in_len = 0;
    int max_tries = 2;

    while (--max_tries > 0)
    {
	in_len = read(priv->audio_fd, buffer, len);

	if (in_len > 0)
	    break;
	if (!((in_len == 0) || (in_len == -1 && (errno == EAGAIN || errno == EINTR))))
	{
	    in_len = 0; /* -EIO */
	    break;
	}
    }
    return 0; //(in_len); // FIXME!
}

static int get_audio_framesize(struct priv_s *priv)
{
    return priv->audio_blocksize;
//    return priv->audio_samplesize[priv->audio_id];
}

#endif /* USE_TV */
