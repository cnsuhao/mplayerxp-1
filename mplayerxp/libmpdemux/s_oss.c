/*
    s_oss - stream interface for oss capturing.
*/
#include "../mp_config.h"
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

#include "mplayer.h"
#include "postproc/af_format.h"
#include "postproc/af_mp.h"
#include "postproc/af.h"
#include "libao2/afmt.h"
#include "libao2/audio_out.h"
#include "loader/wine/mmreg.h"
#include "osdep/mplib.h"
#include "stream.h"
#include "mrl.h"


typedef struct oss_priv_s
{
    unsigned nchannels; /* 1,2,6 */
    unsigned samplerate; /* 32000, 44100, 48000 */
    unsigned sampleformat; /* S32_LE, S16_BE, ... */
    unsigned bps;
    off_t spos;
}oss_priv_t;

static int __FASTCALL__ oss_open(stream_t *stream,const char *filename,unsigned flags)
{
    const char *args;
    char *oss_device,*comma;
    oss_priv_t *oss_priv;
    unsigned tmp,param;
    int err;
    UNUSED(flags);
    if(!(stream->priv = mp_malloc(sizeof(oss_priv_t)))) return 0;
    oss_priv=stream->priv;
    if(strcmp(filename,"help") == 0)
    {
	MSG_HINT("Usage: oss://<@device>#<channels>,<samplerate>,<sampleformat>\n");
	return 0;
    }
    args=mrl_parse_line(filename,NULL,NULL,&oss_device,NULL);
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
	oss_priv->sampleformat=str2fmt(args,&oss_priv->bps);
    else
    {
	/* Default to S16_NE */
	oss_priv->sampleformat=AF_FORMAT_NE|AF_FORMAT_SI|AF_FORMAT_I;
	oss_priv->bps=2;
    }
    stream->fd = open(oss_device?oss_device:PATH_DEV_DSP,O_RDONLY);
    if(stream->fd<0) { mp_free(stream->priv); return 0; }
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
	af_data_t afd;
	int oss_fmt;
	afd.rate=oss_priv->samplerate;
	afd.nch=oss_priv->nchannels;
	afd.format=oss_priv->sampleformat;
	afd.bps=oss_priv->bps;
	oss_fmt=af_format_encode(&afd);
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
	stream->sector_size = 4096*oss_priv->nchannels*oss_priv->bps;
    } else
    if (stream->sector_size < 4096*oss_priv->nchannels*oss_priv->bps) {
	stream->sector_size *= 4096*oss_priv->nchannels*oss_priv->bps/stream->sector_size;
    }
    MSG_DBG2("[o_oss] Correct blocksize as %u\n",stream->sector_size);
    return 1;
}

#ifndef TEMP_FAILURE_RETRY
#define TEMP_FAILURE_RETRY(x) (x)
#endif

static int __FASTCALL__ oss_read(stream_t*stream,stream_packet_t*sp)
{
/*
    Should we repeate read() again on these errno: `EAGAIN', `EIO' ???
*/
    oss_priv_t*p=stream->priv;
    sp->type=0;
    sp->len = TEMP_FAILURE_RETRY(read(stream->fd,sp->buf,sp->len));
    if(sp->len<=0) stream->_Errno=errno;
    else p->spos+=sp->len;
    return sp->len;
}

static off_t __FASTCALL__ oss_seek(stream_t*stream,off_t pos)
{
    UNUSED(pos);
    oss_priv_t *p=stream->priv;
    stream->_Errno=ENOSYS;
    return p->spos;
}

static off_t __FASTCALL__ oss_tell(stream_t*stream)
{
    oss_priv_t *p=stream->priv;
    return p->spos;
}

static void __FASTCALL__ oss_close(stream_t *stream)
{
    ioctl(stream->fd, SNDCTL_DSP_RESET, NULL);
    close(stream->fd);
    mp_free(stream->priv);
}

static int __FASTCALL__ oss_ctrl(stream_t *s,unsigned cmd,any_t*args)
{
    int rval;
    oss_priv_t *oss_priv = s->priv;
    if(args) *(int *)args=0;
    switch(cmd)
    {
	case SCTRL_AUD_GET_CHANNELS:
	    
	    rval = oss_priv->nchannels;
	    if (rval > 2) {
		if ( ioctl(s->fd, SNDCTL_DSP_CHANNELS, &rval) == -1 ||
		(unsigned)rval != oss_priv->nchannels ) return SCTRL_FALSE;
		*(int *)args=rval;
		return SCTRL_OK;
	    }
	    else {
		int c = rval-1;
		if (ioctl (s->fd, SNDCTL_DSP_STEREO, &c) == -1) return SCTRL_FALSE;
		*(int *)args=c+1;
		return SCTRL_OK;
	    }
	    break;
	case SCTRL_AUD_GET_SAMPLERATE:
	    rval=oss_priv->samplerate;
	    if (ioctl(s->fd, SNDCTL_DSP_SPEED, &rval) != -1)
	    {
		*(int *)args = rval;
		return SCTRL_OK;
	    }
	    return SCTRL_FALSE;
	    break;
	case SCTRL_AUD_GET_SAMPLESIZE:
	    *(int *)args=2;
	    if (ioctl (s->fd, SNDCTL_DSP_GETFMTS, &rval) != -1)
	    {
		switch(rval)
		{
		    case AFMT_MU_LAW:
		    case AFMT_A_LAW:
		    case AFMT_IMA_ADPCM:
		    case AFMT_MPEG:
		    case AFMT_AC3:
		    case AFMT_U8:
		    case AFMT_S8:
			*(int *)args=1;
			return SCTRL_OK;
		    default:
		    case AFMT_S16_LE:
		    case AFMT_S16_BE:
		    case AFMT_U16_LE:
		    case AFMT_U16_BE:
			*(int *)args=2;
			return SCTRL_OK;
		    case AFMT_S24_LE:
		    case AFMT_S24_BE:
		    case AFMT_U24_LE:
		    case AFMT_U24_BE:
			*(int *)args=3;
			return SCTRL_OK;
		    case AFMT_S32_LE:
		    case AFMT_S32_BE:
		    case AFMT_U32_LE:
		    case AFMT_U32_BE:
			*(int *)args=4;
			return SCTRL_OK;
		}
		break;
	    }
	    return SCTRL_FALSE;
	case SCTRL_AUD_GET_FORMAT:
	    *(int *)args=0x01; /* Raw PCM */
	    if (ioctl (s->fd, SNDCTL_DSP_GETFMTS, &rval) != -1)
	    {
		switch(rval)
		{
		    case AFMT_MU_LAW: *(int *)args=WAVE_FORMAT_MULAW; return SCTRL_OK;
		    case AFMT_A_LAW:  *(int *)args=WAVE_FORMAT_ALAW; return SCTRL_OK;
		    case AFMT_IMA_ADPCM: *(int *)args=WAVE_FORMAT_ADPCM; return SCTRL_OK;
		    case AFMT_MPEG: *(int *)args=WAVE_FORMAT_MPEG; return SCTRL_OK; /* 0x55? */
		    case AFMT_AC3:  *(int *)args=0x2000; return SCTRL_OK;
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
		    case AFMT_U32_BE: *(int *)args=0x01;/* Raw PCM */ return SCTRL_OK;
		}
		break;
	    }
	    return SCTRL_FALSE;
	default:
	    break;
    }
    return SCTRL_UNKNOWN;
}

const stream_driver_t oss_stream =
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
