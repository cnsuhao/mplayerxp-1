/*
    s_cdd - cdda & cddb streams interface
*/
#include "../mp_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stream.h"

#ifdef HAVE_LIBCDIO
#include "cdd.h"
#include "mrl.h"

static track_t track_idx=255;
static int __FASTCALL__ _cdda_open(stream_t *stream,const char *filename,unsigned flags)
{
    const char *param;
    char *device;
    int retval;
    UNUSED(flags);
    stream->type=STREAMTYPE_RAWAUDIO|STREAMTYPE_SEEKABLE;
    stream->sector_size=CD_FRAMESIZE_RAW;
    if(strcmp(filename,"help") == 0)
    {
	MSG_HINT("Usage: cdda://<@device><#trackno>\n");
	return 0;
    }
    param=mrl_parse_line(filename,NULL,NULL,&device,NULL);
    retval = open_cdda(stream,device ? device : DEFAULT_CDROM_DEVICE,param);
    if(device) free(device);
    return retval;
}

static int __FASTCALL__ _cddb_open(stream_t *stream,const char *filename,unsigned flags)
{
    const char *param;
    char *device;
    int retval;
    UNUSED(flags);
    stream->type=STREAMTYPE_RAWAUDIO|STREAMTYPE_SEEKABLE;
    stream->sector_size=CD_FRAMESIZE_RAW;
    if(strcmp(filename,"help") == 0)
    {
	MSG_HINT("Usage: cddb://<@device><#trackno>\n");
	return 0;
    }
    param=mrl_parse_line(filename,NULL,NULL,&device,NULL);
    retval = open_cddb(stream,device ? device : DEFAULT_CDROM_DEVICE,param);
    if(device) free(device);
    return retval;
}

static int __FASTCALL__ cdd_read(stream_t*stream,stream_packet_t*sp)
{
    sp->type=0;
    sp->len=read_cdda(stream,sp->buf,&track_idx);
    return sp->len;
}

static off_t __FASTCALL__ cdd_seek(stream_t*stream,off_t pos)
{
    seek_cdda(stream,pos,&track_idx);
    return pos;
}

static off_t __FASTCALL__ cdd_tell(stream_t*stream)
{
    return tell_cdda(stream);
}

static void __FASTCALL__ cdd_close(stream_t*stream)
{
    close_cdda(stream);
}

static int __FASTCALL__ cdd_ctrl(stream_t *s,unsigned cmd,any_t*args)
{
    cdda_priv *p=s->priv;
    switch(cmd)
    {
	case SCTRL_TXT_GET_STREAM_NAME:
	{
	    if(track_idx!=255)
		sprintf((char *)args,"Track %d",track_idx);
	    return SCTRL_OK;
	}
	break;
	case SCTRL_AUD_GET_CHANNELS:
		*(int *)args=cdio_cddap_track_channels(p->cd, track_idx);
		if(*(int *)args<=0) *(int *)args=2;
		MSG_V("cdda channels: %u\n",*(int *)args);
		return SCTRL_OK;
	case SCTRL_AUD_GET_SAMPLERATE:
		*(int *)args = 44100;
		return SCTRL_OK;
	case SCTRL_AUD_GET_SAMPLESIZE:
		*(int *)args=2;
		return SCTRL_OK;
	case SCTRL_AUD_GET_FORMAT:
		*(int *)args=0x01; /* Raw PCM */
		return SCTRL_OK;
	default: break;
    }
    return SCTRL_FALSE;
}

const stream_driver_t cdda_stream=
{
    "cdda://",
    "reads multimedia stream directly from Digital Audio Compact Disc [CD-DA]",
    _cdda_open,
    cdd_read,
    cdd_seek,
    cdd_tell,
    cdd_close,
    cdd_ctrl
};

const stream_driver_t cddb_stream=
{
    "cddb://",
    "reads multimedia stream from CD-DA but tracks names from CDDB servers",
    _cddb_open,
    cdd_read,
    cdd_seek,
    cdd_tell,
    cdd_close,
    cdd_ctrl
};
#endif
