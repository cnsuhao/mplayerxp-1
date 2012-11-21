/*
    s_vcdnav - libVCD's stream interface (based on xine's input plugin)
*/
#include "mp_config.h"
#ifdef USE_LIBVCD
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "stream.h"
#include "stream_msg.h"

#include <libvcd/inf.h>
#include <libvcd/logging.h>
#include "osdep/mplib.h"
#include "mrl.h"
#include "help_mp.h"

typedef struct {
    uint8_t subheader	[CDIO_CD_SUBHEADER_SIZE];
    uint8_t data	[M2F2_SECTOR_SIZE];
    uint8_t spare	[4];
} vcdsector_t;

typedef struct {
  lsn_t  start_LSN; /* LSN where play item starts */
  size_t size;      /* size in sector units of play item. */
} vcd_item_info_t;

typedef struct vcd_priv_s
{
    vcdinfo_obj_t* fd;
    unsigned ntracks;
    vcd_item_info_t *track;
    unsigned nentries;
    vcd_item_info_t *entry;
    unsigned nsegments;
    vcd_item_info_t *segment;
    unsigned nlids;
    lsn_t    start;
    lsn_t    lsn;
    unsigned total;
    /* cache */
    vcdsector_t vcd_sector;
    lsn_t	vcd_sector_lsn;
}vcd_priv_t;

static void __FASTCALL__ _cdio_detect_media(char *device)
{
  CdIo_t *img;
  img=cdio_open(device,DRIVER_UNKNOWN);
  if(img)
  {
    discmode_t mode=cdio_get_discmode(img);
    MSG_HINT("Detected %s disk\n",discmode2str[mode]);
  }
}

static int __FASTCALL__ _vcdnav_read(stream_t *stream,stream_packet_t*sp);

static MPXP_Rc __FASTCALL__ _vcdnav_open(any_t*libinput,stream_t *stream,const char *filename,unsigned flags)
{
    vcd_priv_t *priv;
    const char *param;
    char *device,*dev;
    unsigned i;
    int vcd_track=-1;
    vcdinfo_open_return_t open_rc;
    UNUSED(flags);
    UNUSED(libinput);
    if(strcmp(filename,"help") == 0) {
	MSG_HINT("Usage: vcdnav://<@device><#trackno>\n");
	return MPXP_False;
    }
    param=mrl_parse_line(filename,NULL,NULL,&device,NULL);
    if(param) vcd_track=atoi(param);
    stream->priv=priv=new(zeromem) vcd_priv_t;
//    vcdinfo_init(priv->fd);
    if(mp_conf.verbose>1) vcd_loglevel_default=VCD_LOG_DEBUG;
    else if(mp_conf.verbose) vcd_loglevel_default=VCD_LOG_INFO;
    open_rc=vcdinfo_open(&priv->fd,&device,DRIVER_UNKNOWN,NULL);
    if(!priv->fd) {
	dev=DEFAULT_CDROM_DEVICE;
	open_rc=vcdinfo_open(&priv->fd,device?&device:&dev,DRIVER_UNKNOWN,NULL);
	delete device;
	if(!priv->fd) {
	    MSG_ERR("Can't open stream\n");
	    delete priv;
	    _cdio_detect_media(device?device:dev);
	    return MPXP_False;
	}
    }
    priv->nlids=vcdinfo_get_num_LIDs(priv->fd);
    if(vcdinfo_read_psd(priv->fd)) vcdinfo_visit_lot (priv->fd, false);
    MSG_DBG2("VCDNAV geometry:\n");
    if((priv->ntracks=vcdinfo_get_num_tracks(priv->fd))>0) {
	priv->track=new(zeromem) vcd_item_info_t[priv->ntracks];
	for(i=0;i<priv->ntracks;i++) {
	    priv->track[i].size=vcdinfo_get_track_sect_count(priv->fd,i+1);
	    priv->track[i].start_LSN=vcdinfo_get_track_lsn(priv->fd,i+1);
	    MSG_DBG2("track=%i start=%i size=%i\n",i,priv->track[i].start_LSN,priv->track[i].size);
	}
	priv->start=priv->track[0].start_LSN;
	priv->total=priv->track[i-1].size;
    }
    if((priv->nentries=vcdinfo_get_num_entries(priv->fd))>0) {
	priv->entry=new(zeromem) vcd_item_info_t [priv->nentries];
	for(i=0;i<priv->nentries;i++) {
	    priv->entry[i].size=vcdinfo_get_entry_sect_count(priv->fd,i);
	    priv->entry[i].start_LSN=vcdinfo_get_entry_lsn(priv->fd,i);
	    MSG_DBG2("entry=%i start=%i size=%i\n",i,priv->entry[i].start_LSN,priv->entry[i].size);
	}
    }
    if((priv->nsegments=vcdinfo_get_num_segments(priv->fd))>0) {
	priv->segment=new(zeromem) vcd_item_info_t[priv->nsegments];
	for(i=0;i<priv->nsegments;i++) {
	    priv->segment[i].size=vcdinfo_get_seg_sector_count(priv->fd,i);
	    priv->segment[i].start_LSN=vcdinfo_get_seg_lsn(priv->fd,i);
	    MSG_DBG2("segment=%i start=%i size=%i\n",i,priv->segment[i].start_LSN,priv->segment[i].size);
	}
    }
    MSG_INFO("This VCD contains: tracks=%i entries=%i segments=%i\n"
    ,priv->ntracks
    ,priv->nentries
    ,priv->nsegments);
    if(vcd_track!=-1) {
	if(vcd_track>0 && (unsigned)vcd_track<=priv->ntracks) {
	    priv->start=priv->track[vcd_track-1].start_LSN;
	    priv->total=priv->track[vcd_track-1].size;
	}
	else MSG_ERR("Wrong track number %i Playing whole VCD!\n",vcd_track);
    }
    priv->lsn=priv->start;
    _vcdnav_read(stream,NULL); /* Find first non empty segment */
    priv->lsn--;
    priv->start=priv->lsn;
    stream->type = STREAMTYPE_SEEKABLE|STREAMTYPE_PROGRAM;
    stream->sector_size=sizeof(vcdsector_t);
    stream->start_pos=priv->start*sizeof(vcdsector_t);
    stream->end_pos=(priv->start+priv->total)*sizeof(vcdsector_t);
    MSG_DBG2("vcdnav_open start=%i end=%i ssize=%i\n",priv->lsn,priv->total,stream->sector_size);
    check_pin("stream",stream->pin,STREAM_PIN);
    return MPXP_Ok;
}

static void __FASTCALL__ _vcdnav_inc_lsn(vcd_priv_t *p)
{
    unsigned i;
    int j=-1;
    for(i=0;i<p->ntracks;i++)
    {
	if(p->lsn >= p->track[i].start_LSN && p->lsn < p->track[i].start_LSN+p->track[i].size)
	{
	    j=i;
	    break;
	}
    }
    if(j!=-1 && p->lsn>=p->track[j].start_LSN+p->track[j].size)
	    p->lsn=(j==p->ntracks)?p->lsn+1:p->track[j+1].start_LSN;
    else p->lsn++;
}

static int __FASTCALL__ _vcdnav_read(stream_t *stream,stream_packet_t*sp)
{
    vcd_priv_t *p=reinterpret_cast<vcd_priv_t*>(stream->priv);
    CdIo *img=vcdinfo_get_cd_image(p->fd);
    MSG_DBG2("vcdnav_read: lsn=%i total=%i\n",p->lsn,p->total);
    if(sp) sp->type=0;
    do {
      if(p->lsn!=p->vcd_sector_lsn)
      {
	if (cdio_read_mode2_sector(img, &p->vcd_sector, p->lsn, true)!=0) {
	    MSG_ERR("vcdnav: read error\n");
	    _vcdnav_inc_lsn(p);
	    return 0;
	}
	MSG_DBG3("LSN=%i SUBHEADER: %02X %02X %02X %02X %02X %02X %02X %02X\n"
	,p->lsn
	,p->vcd_sector.subheader[0]
	,p->vcd_sector.subheader[1]
	,p->vcd_sector.subheader[2]
	,p->vcd_sector.subheader[3]
	,p->vcd_sector.subheader[4]
	,p->vcd_sector.subheader[5]
	,p->vcd_sector.subheader[6]
	,p->vcd_sector.subheader[7]);
	MSG_DBG3("DATA: %02X %02X %02X %02X %02X %02X %02X %02X ...\n"
	,p->vcd_sector.data[0]
	,p->vcd_sector.data[1]
	,p->vcd_sector.data[2]
	,p->vcd_sector.data[3]
	,p->vcd_sector.data[4]
	,p->vcd_sector.data[5]
	,p->vcd_sector.data[6]
	,p->vcd_sector.data[7]);
	MSG_DBG3("SPARE: %02X %02X %02X %02X\n"
	,p->vcd_sector.spare[0]
	,p->vcd_sector.spare[1]
	,p->vcd_sector.spare[2]
	,p->vcd_sector.spare[3]);
      }
      p->vcd_sector_lsn=p->lsn;
      _vcdnav_inc_lsn(p);

      if ( p->lsn >= p->start+p->total ) {
	/* We've run off of the end of this entry. Do we continue or stop? */
	MSG_DBG2("end reached in reading, cur: %u, end: %u\n", p->lsn, p->total);
	break;
      }

      /* Check header ID for a padding sector and simply discard
	 these.  It is alleged that VCD's put these in to keep the
	 bitrate constant.
      */
    } while((p->vcd_sector.subheader[2]&~0x01)==0x60);

    if(sp) /* sp may be NULL in case of internal usage */
    {
	memcpy (sp->buf, p->vcd_sector.data, M2F2_SECTOR_SIZE);
	sp->len=M2F2_SECTOR_SIZE;
    }
    return sp?sp->len:0;
}

static off_t __FASTCALL__ _vcdnav_seek(stream_t *stream,off_t pos)
{
    vcd_priv_t *p=reinterpret_cast<vcd_priv_t*>(stream->priv);
    lsn_t oldlsn=p->lsn;
    CdIo *img = vcdinfo_get_cd_image(p->fd);
    p->lsn=pos/sizeof(vcdsector_t);
    if(p->lsn < p->start) p->lsn=p->start;
    if(p->lsn > p->start+p->total) p->lsn=p->start+p->total;
    p->lsn--;
    _vcdnav_inc_lsn(p);
    cdio_lseek(img,p->lsn,SEEK_SET);
    MSG_DBG2("vcdnav_seek: lsn=%i newlsn=%i pos=%lli\n",oldlsn,p->lsn,pos);
    return p->lsn*sizeof(vcdsector_t);
}

static off_t __FASTCALL__ _vcdnav_tell(const stream_t *stream)
{
    vcd_priv_t *p=reinterpret_cast<vcd_priv_t*>(stream->priv);
    MSG_DBG2("vcdnav_tell: lsn=%i\n",p->lsn);
    return p->lsn*sizeof(vcdsector_t);
}

static void __FASTCALL__ _vcdnav_close(stream_t*stream)
{
    vcd_priv_t*priv=reinterpret_cast<vcd_priv_t*>(stream->priv);
    MSG_DBG2("vcdnav_close\n");
    vcdinfo_close(((vcd_priv_t *)stream->priv)->fd);
    if(priv->track) mp_free(priv->track);
    if(priv->entry) mp_free(priv->entry);
    if(priv->segment) mp_free(priv->segment);
    mp_free(stream->priv);
}
static MPXP_Rc __FASTCALL__ _vcdnav_ctrl(const stream_t *s,unsigned cmd,any_t*args) {
    UNUSED(s);
    UNUSED(cmd);
    UNUSED(args);
    return MPXP_Unknown;
}

extern const stream_driver_t vcdnav_stream=
{
    "vcdnav://",
    "reads multimedia stream from libVCD's interface",
    _vcdnav_open,
    _vcdnav_read,
    _vcdnav_seek,
    _vcdnav_tell,
    _vcdnav_close,
    _vcdnav_ctrl
};
#endif
