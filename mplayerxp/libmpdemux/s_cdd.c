/*
    s_cdd - cdda & cddb streams interface
*/
#include "../mp_config.h"
#include <stdlib.h>
#include <string.h>
#include "stream.h"

#ifdef HAVE_CDDA
#include "cdd.h"
#include "mrl.h"

static int track_idx=-1;
static int __FASTCALL__ cdd_open(stream_t *stream,const char *filename,unsigned flags)
{
    const char *param;
    char *device;
    int retval;
    stream->type=STREAMTYPE_RAWAUDIO|STREAMTYPE_SEEKABLE;
    stream->sector_size=CD_FRAMESIZE_RAW;
    if(!(strncmp(filename,"cdda://",7)==0 || strncmp(filename,"cddb://",7)==0)) return 0;
    if(strcmp(&filename[7],"help") == 0)
    {
	MSG_HINT("Usage: cdda://<@device><#trackno> or cddb://<@device><#trackno>\n");
	return 0;
    }
    param=mrl_parse_line(&filename[7],NULL,NULL,&device,NULL);
    retval =
    strncmp(filename,"cdda://",7)==0?open_cdda(stream,device ? device : DEFAULT_CDROM_DEVICE,param):
				     open_cddb(stream,device ? device : DEFAULT_CDROM_DEVICE,param);
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
    seek_cdda(stream,pos);
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

static int __FASTCALL__ cdd_ctrl(stream_t *s,unsigned cmd,void *args)
{
    cdda_priv *priv=s->priv;
    cd_track_t*tr=NULL;
    switch(cmd)
    {
	case SCTRL_TXT_GET_STREAM_NAME:
	{
	    if(track_idx!=-1) tr=cd_info_get_track(priv->cd_info,track_idx);
	    if(tr)
	    {
		strncpy(args,tr->name,256);
		((char *)args)[255]=0;
	    }
	    return SCTRL_OK;
	}
	break;
	case SCTRL_AUD_GET_CHANNELS:
		*(int *)args=2;
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
#elif defined( HAVE_SDL )
#include <SDL/SDL.h>
static SDL_CD* sdlcd;
static int __FASTCALL__ cdd_open(stream_t *stream,const char *filename,unsigned flags)
{
    int start_track = 0;
    int end_track = 0;
    const char *track;
    char* end;
    stream->type=STREAMTYPE_RAWAUDIO|STREAMTYPE_SEEKABLE;
    stream->sector_size=2352;
    if(!(strncmp(filename,"cdda://",7)==0 || strncmp(filename,"cddb://",7)==0)) return 0;
    if(strcmp(&filename[7],"help") == 0)
    {
	MSG_HINT("Usage: cdda://<trackno> or cddb://<trackno>\n");
	return 0;
    }
    track=&filename[7];
    end = strchr(track,'-');
    if(!end) start_track = end_track = atoi(track);
    else {
	int st_len = end - track;
	int ed_len = strlen(track) - 1 - st_len;

	if(st_len) {
	    char st[st_len + 1];
	    strncpy(st,track,st_len);
	    st[st_len] = '\0';
	    start_track = atoi(st);
	}
	if(ed_len) {
	    char ed[ed_len + 1];
	    strncpy(ed,end+1,ed_len);
	    ed[ed_len] = '\0';
	end_track = atoi(ed);
	}
    }
    if (!SDL_WasInit(SDL_INIT_CDROM)) {
        if (SDL_Init (SDL_INIT_CDROM)) {
            MSG_ERR("SDL: Initializing of SDL failed: %s.\n", SDL_GetError());
            return -1;
        }
    }
    /* TODO: selectable cd number */
    sdlcd = SDL_CDOpen(0);
    if(CD_INDRIVE(SDL_CDStatus(sdlcd)))
	SDL_CDPlayTracks(sdlcd,start_track,0,end_track-start_track,0);
    return 1;
}

static int __FASTCALL__ cdd_read(stream_t*stream,stream_packet_t*sp)
{
    return 0;
}

static off_t __FASTCALL__ cdd_seek(stream_t*stream,off_t pos)
{
    return 0;
}

static off_t __FASTCALL__ cdd_tell(stream_t*stream)
{
    return 0;
}
static void __FASTCALL__ cdd_close(stream_t*stream)
{
    SDL_CDStop(sdlcd);
    SDL_CDClose(sdlcd);
    if (SDL_WasInit(SDL_INIT_CDROM));
        SDL_QuitSubSystem(SDL_INIT_CDROM);
}

static int __FASTCALL__ cdd_ctrl(stream_t *s,unsigned cmd,void *args)
{
    switch(cmd)
    {
	case SCTRL_AUD_GET_CHANNELS:
		*(int *)args=2;
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
    return SCTRL_UNKNOWN;
}
#else
#include "demux_msg.h"
static int __FASTCALL__ cdd_open(stream_t *stream,const char *filename,unsigned flags)
{
    if(strncmp(filename,"cdda://",7)==0 || strncmp(filename,"cddb://",7)==0)
	MSG_ERR("MplayerXP has been compiled without CDDA(B) support\n");
    return 0;
}

static int __FASTCALL__ cdd_read(stream_t*stream,char *buf,unsigned size)
{
    return 0;
}

static off_t __FASTCALL__ cdd_seek(stream_t*stream,off_t pos)
{
    return 0;
}

static off_t __FASTCALL__ cdd_tell(stream_t*stream)
{
    return 0;
}
static void __FASTCALL__ cdd_close(stream_t*stream) {}

static int __FASTCALL__ cdd_ctrl(stream_t *s,unsigned cmd,void *args) { return SCTRL_UNKNOWN; }
#endif

const stream_driver_t cdd_stream=
{
    "cdd",
    cdd_open,
    cdd_read,
    cdd_seek,
    cdd_tell,
    cdd_close,
    cdd_ctrl
};
