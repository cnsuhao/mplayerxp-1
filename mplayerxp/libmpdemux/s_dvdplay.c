/*
    s_dvdplay - DVDPlay's stream interface
    can be found at:
    http://developers.videolan.org/libdvdplay/
    Note: Please use it with '-nocache' option.
	  Requires PTS fixing :(
*/
#include "../mp_config.h"
#include <stdlib.h>
#include <string.h>
#include "stream.h"
#include "demux_msg.h"
#include "help_mp.h"

#ifdef USE_DVDPLAY
typedef unsigned char byte_t;
#include <dvdread/ifo_types.h>
#include <dvdplay/dvdplay.h>
#include <dvdplay/info.h>
#include <dvdplay/nav.h>
#include <dvdplay/state.h>
#define DVD_BLOCK_SIZE 2048
#include "mrl.h"

/* Logical block size for DVD-VIDEO */
#define LB2OFF(x) ((off_t)(x) * (off_t)(DVD_BLOCK_SIZE))
#define OFF2LB(x) ((x) >> 11)

typedef struct dvdplay_priv_s
{
    dvdplay_ptr dvdplay;
    off_t	cur_pos;
}dvdplay_priv_t;

static void my_callback(void *s,dvdplay_event_t event)
{
    stream_t *stream=s;
    dvdplay_priv_t *priv = stream->priv;
    MSG_DBG2("dvdplay: callback with event %i\n",event);
}

static int __FASTCALL__ __dvdplay_open(stream_t *stream,const char *filename,unsigned flags)
{
    const char *param;
    char *dvd_device;

    if(strncmp(filename,"dvdplay://",10)!=0) return 0;
    stream->type = STREAMTYPE_SEEKABLE|STREAMTYPE_PROGRAM;

    if(!(stream->priv=malloc(sizeof(dvdplay_priv_t))))
    {
	MSG_ERR(MSGTR_OutOfMemory);
	return 0;
    }
    param=mrl_parse_line(&filename[10],NULL,NULL,&dvd_device,NULL);
    if (!(((dvdplay_priv_t *)stream->priv)->dvdplay=dvdplay_open(dvd_device?dvd_device:DEFAULT_DVD_DEVICE,my_callback,stream))) {
	MSG_ERR(MSGTR_CantOpenDVD,dvd_device);
	if(!dvd_device)
	{
	    if (!(((dvdplay_priv_t *)stream->priv)->dvdplay=dvdplay_open(DEFAULT_CDROM_DEVICE,my_callback,stream)))
		MSG_ERR(MSGTR_CantOpenDVD,dvd_device);
	    else
		goto dvd_ok;
	}
	free(stream->priv);
	if(dvd_device) free(dvd_device);
        return 0;
    }
    dvd_ok:
    if(dvd_device) free(dvd_device);
    /* libdvdplay can read blocks of any size (unlike libdvdnav) so
       increasing block size in 10 times speedups cache */
    stream->sector_size=DVD_BLOCK_SIZE*10;
    ((dvdplay_priv_t *)stream->priv)->cur_pos=0;
    dvdplay_start(((dvdplay_priv_t *)stream->priv)->dvdplay,0);
    return 1;
}

static int __FASTCALL__ __dvdplay_read(stream_t *stream,stream_packet_t *sp)
{
    dvdplay_priv_t *priv = stream->priv;
    sp->type=0;
    sp->len = LB2OFF(dvdplay_read(priv->dvdplay,sp->buf,OFF2LB(sp->len)));
    priv->cur_pos += sp->len;
    return sp->len;
}


static off_t __FASTCALL__ __dvdplay_tell(stream_t *stream)
{
    dvdplay_priv_t *priv = stream->priv;
/*    return LB2OFF(dvdplay_position(priv->dvdplay));*/
    return priv->cur_pos;
}

static off_t __FASTCALL__ __dvdplay_seek(stream_t *stream,off_t pos)
{
    dvdplay_priv_t *priv = stream->priv;
    dvdplay_seek(priv->dvdplay,OFF2LB(pos));
    priv->cur_pos=LB2OFF(dvdplay_position(priv->dvdplay));
    return priv->cur_pos;
}

static void __FASTCALL__ __dvdplay_close(stream_t *stream)
{
    dvdplay_priv_t *priv = stream->priv;
    dvdplay_close(priv->dvdplay);
    free(stream->priv);
}
#else
static int __FASTCALL__ __dvdplay_open(stream_t *stream,const char *filename,unsigned flags)
{
    if(strncmp(filename,"dvdplay://",10)==0)
	MSG_ERR("MplayerXP has been compiled without DVDPlay support\n");
    return 0;
}
static int __FASTCALL__ __dvdplay_read(stream_t *stream,stream_packet_t *sp)
{
    return 0;
}
static off_t __FASTCALL__ __dvdplay_seek(stream_t *stream,off_t pos)
{
    return 0;
}
static off_t __FASTCALL__ __dvdplay_tell(stream_t *stream)
{
    return 0;
}
static void __FASTCALL__ __dvdplay_close(stream_t *stream)
{
}
#endif
static int __FASTCALL__ __dvdplay_ctrl(stream_t *s,unsigned cmd,void *args) { return SCTRL_UNKNOWN; }

const stream_driver_t dvdplay_stream=
{
    "dvdplay",
    __dvdplay_open,
    __dvdplay_read,
    __dvdplay_seek,
    __dvdplay_tell,
    __dvdplay_close,
    __dvdplay_ctrl
};
