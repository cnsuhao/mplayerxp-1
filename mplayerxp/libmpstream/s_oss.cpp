#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
/*
    s_oss - stream interface for oss capturing.
*/
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#include "mplayerxp.h"
#include "xmpcore/mp_aframe.h"
#include "postproc/af.h"
#include "libao2/afmt.h"
#include "libao2/audio_out.h"
#include "loader/wine/mmreg.h"
#include "stream.h"
#include "mrl.h"
#include "stream_msg.h"


struct oss_priv_t : public Opaque {
    public:
	oss_priv_t() {}
	virtual ~oss_priv_t() {}

	unsigned nchannels; /* 1,2,6 */
	unsigned samplerate; /* 32000, 44100, 48000 */
	mpaf_format_e sampleformat; /* S32_LE, S16_BE, ... */
	off_t spos;
};

static MPXP_Rc __FASTCALL__ oss_open(any_t*libinput,stream_t *stream,const char *filename,unsigned flags)
{
    char *args;
    char *oss_device,*comma;
    oss_priv_t *oss_priv;
    unsigned tmp,param;
    int err;
    UNUSED(flags);
    UNUSED(libinput);
    if(!(oss_priv = new(zeromem) oss_priv_t)) return MPXP_False;
    stream->priv=oss_priv;
    if(strcmp(filename,"help") == 0) {
	MSG_HINT("Usage: oss://<@device>#<channels>,<samplerate>,<sampleformat>\n");
	return MPXP_False;
    }
    args=mp_strdup(mrl_parse_line(filename,NULL,NULL,&oss_device,NULL));
    comma=strchr(args,',');
    if(comma) *comma=0;
    oss_priv->nchannels=args[0]?atoi(args):2;
    if(comma) args=comma+1;
    comma=strchr(args,',');
    if(comma) *comma=0;
    oss_priv->samplerate=args[0]?atoi(args):44100;
    if(comma) args=comma+1;
    comma=strchr(args,',');
    if(comma) *comma=0;
    if(args[0])
	oss_priv->sampleformat=mpaf_str2fmt(args);
    else {
	/* Default to S16_NE */
	oss_priv->sampleformat=MPAF_NE|MPAF_SI|MPAF_I|MPAF_BPS_2;
    }
    stream->fd = open(oss_device?oss_device:PATH_DEV_DSP,O_RDONLY);
    if(stream->fd<0) { delete oss_priv; delete args; return MPXP_False; }
    ioctl(stream->fd, SNDCTL_DSP_RESET, NULL);
//    ioctl(stream->fd, SNDCTL_DSP_SYNC, NULL);
    stream->type = STREAMTYPE_STREAM|STREAMTYPE_RAWAUDIO;
    stream->start_pos = 0;
    stream->end_pos = -1;
    stream->eof = 0;
    oss_priv->spos=0;
    /* Configure OSS */
    tmp = oss_priv->samplerate;
    err=0;
    if (ioctl(stream->fd, SNDCTL_DSP_SPEED, &oss_priv->samplerate)<0)
	MSG_ERR("[s_oss] Can't set samplerate to %u (will use %u)\n",tmp,oss_priv->samplerate);
    else
	MSG_DBG2("[o_oss] Did set samplerate to %u\n",oss_priv->samplerate);
    tmp = oss_priv->nchannels;
    if(tmp>2)
	err=ioctl(stream->fd, SNDCTL_DSP_CHANNELS, &oss_priv->nchannels);
    else
    {
	param=(oss_priv->nchannels==2?1:0);
	err=ioctl(stream->fd, SNDCTL_DSP_STEREO, &param);
	oss_priv->nchannels=param?2:1;
    }
    if(err<0) MSG_ERR("[s_oss] Can't set channels to %u (will use %u)\n",tmp,oss_priv->nchannels);
    else MSG_DBG2("[o_oss] Did set channels to %u\n",oss_priv->nchannels);
    {
	mp_aframe_t afd;
	int oss_fmt;
	afd.rate=oss_priv->samplerate;
	afd.nch=oss_priv->nchannels;
	afd.format=oss_priv->sampleformat;
	oss_fmt=mpaf2afmt(oss_priv->sampleformat);
	tmp=oss_fmt;
	if(ioctl(stream->fd, SNDCTL_DSP_SETFMT, &oss_fmt)<0)
	    MSG_ERR("[s_oss] Can't set format %s (will use %s)\n",ao_format_name(tmp),ao_format_name(oss_fmt));
	else
	    MSG_DBG2("[o_oss] Did set format to %s\n",ao_format_name(oss_fmt));
    }
    tmp = PCM_ENABLE_INPUT;
    if(ioctl(stream->fd, SNDCTL_DSP_SETTRIGGER, &tmp)<0)
	MSG_ERR("[s_oss] Can't enable input\n");
    else
	MSG_DBG2("[o_oss] Did set trigger to %u\n",tmp);
    stream->sector_size = 0;
    err = ioctl(stream->fd, SNDCTL_DSP_GETBLKSIZE, &stream->sector_size);
    if (err < 0)
	MSG_ERR("[s_oss] Can't get blocksize\n");
    else
	MSG_DBG2("[o_oss] Did get blocksize as %u\n",stream->sector_size);
    // correct the blocksize to a reasonable value
    if (stream->sector_size <= 0) {
	stream->sector_size = 4096*oss_priv->nchannels*(oss_priv->sampleformat&MPAF_BPS_MASK);
    } else
    if (stream->sector_size < 4096*oss_priv->nchannels*(oss_priv->sampleformat&MPAF_BPS_MASK)) {
	stream->sector_size *= 4096*oss_priv->nchannels*(oss_priv->sampleformat&MPAF_BPS_MASK)/stream->sector_size;
    }
    MSG_DBG2("[o_oss] Correct blocksize as %u\n",stream->sector_size);
    check_pin("stream",stream->pin,STREAM_PIN);
    delete args;
    return MPXP_Ok;
}

#ifndef TEMP_FAILURE_RETRY
#define TEMP_FAILURE_RETRY(x) (x)
#endif

static int __FASTCALL__ oss_read(stream_t*stream,stream_packet_t*sp)
{
/*
    Should we repeate read() again on these errno: `EAGAIN', `EIO' ???
*/
    oss_priv_t*p=static_cast<oss_priv_t*>(stream->priv);
    sp->type=0;
    sp->len = TEMP_FAILURE_RETRY(read(stream->fd,sp->buf,sp->len));
    if(!errno) p->spos+=sp->len;
    return sp->len;
}

static off_t __FASTCALL__ oss_seek(stream_t*stream,off_t pos)
{
    UNUSED(pos);
    oss_priv_t *p=static_cast<oss_priv_t*>(stream->priv);
    errno=ENOSYS;
    return p->spos;
}

static off_t __FASTCALL__ oss_tell(const stream_t*stream)
{
    oss_priv_t *p=static_cast<oss_priv_t*>(stream->priv);
    return p->spos;
}

static void __FASTCALL__ oss_close(stream_t *stream)
{
    ioctl(stream->fd, SNDCTL_DSP_RESET, NULL);
    close(stream->fd);
    delete stream->priv;
}

static MPXP_Rc __FASTCALL__ oss_ctrl(const stream_t *s,unsigned cmd,any_t*args)
{
    int rval;
    oss_priv_t *oss_priv = static_cast<oss_priv_t*>(s->priv);
    if(args) *(int *)args=0;
    switch(cmd) {
	case SCTRL_AUD_GET_CHANNELS:
	    rval = oss_priv->nchannels;
	    if (rval > 2) {
		if ( ioctl(s->fd, SNDCTL_DSP_CHANNELS, &rval) == -1 ||
		(unsigned)rval != oss_priv->nchannels ) return MPXP_False;
		*(int *)args=rval;
		return MPXP_Ok;
	    }
	    else {
		int c = rval-1;
		if (ioctl (s->fd, SNDCTL_DSP_STEREO, &c) == -1) return MPXP_False;
		*(int *)args=c+1;
		return MPXP_Ok;
	    }
	    break;
	case SCTRL_AUD_GET_SAMPLERATE:
	    rval=oss_priv->samplerate;
	    if (ioctl(s->fd, SNDCTL_DSP_SPEED, &rval) != -1) {
		*(int *)args = rval;
		return MPXP_Ok;
	    }
	    return MPXP_False;
	    break;
	case SCTRL_AUD_GET_SAMPLESIZE:
	    *(int *)args=2;
	    if (ioctl (s->fd, SNDCTL_DSP_GETFMTS, &rval) != -1) {
		switch(rval) {
		    case AFMT_MU_LAW:
		    case AFMT_A_LAW:
		    case AFMT_IMA_ADPCM:
		    case AFMT_MPEG:
		    case AFMT_AC3:
		    case AFMT_U8:
		    case AFMT_S8:
			*(int *)args=1;
			return MPXP_Ok;
		    default:
		    case AFMT_S16_LE:
		    case AFMT_S16_BE:
		    case AFMT_U16_LE:
		    case AFMT_U16_BE:
			*(int *)args=2;
			return MPXP_Ok;
		    case AFMT_S24_LE:
		    case AFMT_S24_BE:
		    case AFMT_U24_LE:
		    case AFMT_U24_BE:
			*(int *)args=3;
			return MPXP_Ok;
		    case AFMT_S32_LE:
		    case AFMT_S32_BE:
		    case AFMT_U32_LE:
		    case AFMT_U32_BE:
			*(int *)args=4;
			return MPXP_Ok;
		}
		break;
	    }
	    return MPXP_False;
	case SCTRL_AUD_GET_FORMAT:
	    *(int *)args=0x01; /* Raw PCM */
	    if (ioctl (s->fd, SNDCTL_DSP_GETFMTS, &rval) != -1) {
		switch(rval) {
		    case AFMT_MU_LAW: *(int *)args=WAVE_FORMAT_MULAW; return MPXP_Ok;
		    case AFMT_A_LAW:  *(int *)args=WAVE_FORMAT_ALAW; return MPXP_Ok;
		    case AFMT_IMA_ADPCM: *(int *)args=WAVE_FORMAT_ADPCM; return MPXP_Ok;
		    case AFMT_MPEG: *(int *)args=WAVE_FORMAT_MPEG; return MPXP_Ok; /* 0x55? */
		    case AFMT_AC3:  *(int *)args=0x2000; return MPXP_Ok;
		    default:
		    case AFMT_U8:
		    case AFMT_S8:
		    case AFMT_S16_LE:
		    case AFMT_S16_BE:
		    case AFMT_U16_LE:
		    case AFMT_U16_BE:
		    case AFMT_S24_LE:
		    case AFMT_S24_BE:
		    case AFMT_U24_LE:
		    case AFMT_U24_BE:
		    case AFMT_S32_LE:
		    case AFMT_S32_BE:
		    case AFMT_U32_LE:
		    case AFMT_U32_BE: *(int *)args=0x01;/* Raw PCM */ return MPXP_Ok;
		}
		break;
	    }
	    return MPXP_False;
	default:
	    break;
    }
    return MPXP_Unknown;
}

extern const stream_driver_t oss_stream =
{
    "oss://",
    "reads multimedia stream from OSS audio capturing interface",
    oss_open,
    oss_read,
    oss_seek,
    oss_tell,
    oss_close,
    oss_ctrl
};
