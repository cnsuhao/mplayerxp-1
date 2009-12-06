/*
    s_vcd - VCD's stream interface
*/
#include "../mp_config.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "stream.h"
#include "demux_msg.h"

#ifdef HAVE_VCD

#ifdef __FreeBSD__
#include <sys/cdrio.h>
#endif
#include <sys/ioctl.h>

#include <fcntl.h>
#include <unistd.h>
#include <fcntl.h>
#ifdef __FreeBSD__
#include "vcd_read_fbsd.h" 
#elif defined(__NetBSD__)
#include "vcd_read_nbsd.h" 
#else
#include "vcd_read.h"
#endif
#include "help_mp.h"
#include "mrl.h"

typedef struct vcd_priv_s
{
    off_t spos;
    unsigned ssect,esect;
}vcd_priv_t;

static int __FASTCALL__ _vcd_open(stream_t *stream,const char *filename,unsigned flags)
{
    int ret,ret2;
    int vcd_track;
    vcd_priv_t *priv;
    const char *param;
    char *device;
#ifdef __FreeBSD__
    int bsize=VCD_SECTOR_SIZE;
#endif


/* originally was vcd:// but playtree parser replaced it */
    if(strncmp(filename,"vcd://",6)!=0) return 0;
    if(strcmp(&filename[6],"help") == 0)
    {
	MSG_HINT("Usage: vcd://<@device><#trackno>\n");
	return 0;
    }
    param=mrl_parse_line(&filename[6],NULL,NULL,&device,NULL);
    vcd_track=atoi(param);
    if(!device) device=DEFAULT_CDROM_DEVICE;
    stream->fd=open(device,O_RDONLY);
    if(device) free(device);
    if(stream->fd<0)
    {
	MSG_ERR(MSGTR_CdDevNotfound,device);
	return 0;
    }
    vcd_read_toc(stream->fd);
    ret2=vcd_get_track_end(stream->fd,vcd_track);
    if(ret2<0){ MSG_ERR(MSGTR_ErrTrackSelect " (get)\n");return 0;}
    ret=vcd_seek_to_track(stream->fd,vcd_track);
    if(ret<0){ MSG_ERR(MSGTR_ErrTrackSelect " (seek)\n");return 0;}
    priv=stream->priv=malloc(sizeof(vcd_priv_t));
    vcd_read(stream->fd,NULL);
    vcd_dec_msf();
    priv->ssect=vcd_get_msf();
    priv->esect=ret2/VCD_SECTOR_DATA;
    ret=VCD_SECTOR_DATA*priv->ssect;
    MSG_V("VCD start byte position: 0x%X  end: 0x%X\n",ret,ret2);
#ifdef __FreeBSD__
    if (ioctl (stream->f, CDRIOCSETBLOCKSIZE, &bsize) == -1) {
        MSG_ERR ("Error in CDRIOCSETBLOCKSIZE");
    }
#endif
    stream->type = STREAMTYPE_SEEKABLE|STREAMTYPE_PROGRAM;
    stream->sector_size=VCD_SECTOR_SIZE;
    stream->start_pos=ret;
    stream->end_pos=ret2;
    ((vcd_priv_t *)stream->priv)->spos=ret;
    return 1;
}

static int __FASTCALL__ _vcd_read(stream_t *stream,stream_packet_t*sp)
{
    vcd_priv_t *p=stream->priv;
    sp->type=0;
    sp->len = vcd_read(stream->fd,sp->buf);
    p->spos += sp->len;
    return sp->len;
}

static off_t __FASTCALL__ _vcd_seek(stream_t *stream,off_t pos)
{
    vcd_priv_t *p=stream->priv;
    off_t newpos;
    if(newpos<stream->start_pos) newpos=stream->start_pos;
    if(newpos>stream->end_pos) newpos=stream->end_pos;
    newpos=pos/VCD_SECTOR_DATA;
    if(newpos<p->ssect) newpos=p->ssect;
    if(newpos>p->esect) newpos=p->esect;
    vcd_set_msf(newpos);
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
    free(stream->priv);
    close(stream->fd);
}
#else
static int __FASTCALL__ _vcd_open(stream_t *stream,const char *filename,unsigned flags)
{
    if(strncmp(filename,"vcd://",6)==0)
	MSG_ERR("MPlayerXP has been compiled without VCD support\n");
    return 0;
}
static int __FASTCALL__ _vcd_read(stream_t *stream,stream_packet_t *sp)
{
    return 0;
}
static off_t __FASTCALL__ _vcd_seek(stream_t *stream,off_t pos)
{
    return 0;
}
static off_t __FASTCALL__ _vcd_tell(stream_t *stream)
{
    return 0;
}
static void __FASTCALL__ _vcd_close(stream_t*stream) {}
#endif
static int __FASTCALL__ _vcd_ctrl(stream_t *s,unsigned cmd,void *args) { return SCTRL_UNKNOWN; }

const stream_driver_t vcd_stream=
{
    "vcd",
    _vcd_open,
    _vcd_read,
    _vcd_seek,
    _vcd_tell,
    _vcd_close,
    _vcd_ctrl
};
