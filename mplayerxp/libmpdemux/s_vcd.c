/*
    s_vcd - VCD's stream interface
*/
#include "../mp_config.h"
#ifdef HAVE_LIBCDIO
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <cdio/cdio.h>

#include "stream.h"
#include "help_mp.h"
#include "mrl.h"
#include "demux_msg.h"

typedef struct vcd_priv_s
{
    CdIo_t *cd;
    off_t spos;
    unsigned ssect,esect;
}vcd_priv_t;

static int __FASTCALL__ _vcd_open(stream_t *stream,const char *filename,unsigned flags)
{
    int vcd_track;
    vcd_priv_t *priv;
    const char *param;
    char *dev,*device=NULL;
    UNUSED(flags);
/* originally was vcd:// but playtree parser replaced it */
    if(strcmp(filename,"help") == 0)
    {
	MSG_HINT("Usage: vcd://<@device><#trackno>\n");
	return 0;
    }
    param=mrl_parse_line(filename,NULL,NULL,&device,NULL);
    vcd_track=atoi(param);
    if(!device) dev=DEFAULT_CDROM_DEVICE;
    else	dev=device;
    priv=stream->priv=malloc(sizeof(vcd_priv_t));
    cdio_init();
    priv->cd=cdio_open(dev,DRIVER_UNKNOWN);
    if(device) free(device);
    if(!priv->cd)
    {
	MSG_ERR(MSGTR_CdDevNotfound,device);
	return 0;
    }
    vcd_track=cdio_get_num_tracks(priv->cd);
    MSG_V("CD contains: %d tracks\n",vcd_track);
    priv->ssect=cdio_get_track_pregap_lsn(priv->cd,2);
    priv->esect=cdio_get_track_pregap_lsn(priv->cd,vcd_track)+cdio_get_track_sec_count(priv->cd,vcd_track);

    stream->type = STREAMTYPE_SEEKABLE|STREAMTYPE_PROGRAM;
    stream->sector_size=VCD_SECTOR_SIZE;
    stream->start_pos=priv->ssect*VCD_SECTOR_DATA;
    stream->end_pos=priv->esect*VCD_SECTOR_DATA;
    ((vcd_priv_t *)stream->priv)->spos=stream->start_pos;
    return 1;
}

static int __FASTCALL__ _vcd_read(stream_t *stream,stream_packet_t*sp)
{
    vcd_priv_t *p=stream->priv;
    ssize_t len;
    sp->type=0;
    len=cdio_read(p->cd,sp->buf,sp->len);
    p->spos += len;
    return len;
}

static off_t __FASTCALL__ _vcd_seek(stream_t *stream,off_t pos)
{
    vcd_priv_t *p=stream->priv;
    off_t newpos=pos;
    if(newpos<stream->start_pos) newpos=stream->start_pos;
    if(newpos>stream->end_pos) newpos=stream->end_pos;
    newpos=pos/VCD_SECTOR_DATA;
    if(newpos<p->ssect) newpos=p->ssect;
    if(newpos>p->esect) newpos=p->esect;
    newpos=cdio_lseek(p->cd,newpos*VCD_SECTOR_DATA,SEEK_SET);
    p->spos=newpos*VCD_SECTOR_DATA;
    return p->spos;
}

static off_t __FASTCALL__ _vcd_tell(stream_t *stream)
{
    vcd_priv_t *p=stream->priv;
    return p->spos;
}

static void __FASTCALL__ _vcd_close(stream_t*stream)
{
    vcd_priv_t *p=stream->priv;
    cdio_destroy(p->cd);
    free(stream->priv);
}
static int __FASTCALL__ _vcd_ctrl(stream_t *s,unsigned cmd,void *args) {
    UNUSED(s);
    UNUSED(cmd);
    UNUSED(args);
    return SCTRL_UNKNOWN;
}

const stream_driver_t vcd_stream=
{
    "vcd://",
    "reads multimedia stream using low-level Video-CD access",
    _vcd_open,
    _vcd_read,
    _vcd_seek,
    _vcd_tell,
    _vcd_close,
    _vcd_ctrl
};
#endif
