#include "config.h"

#ifdef HAVE_CDDA

#include "stream.h"
#include "../cfgparser.h"

#include <stdio.h>
#include <stdlib.h>

#include "cdd.h"
#include "demux_msg.h"
static int speed = -1;
static int paranoia_mode = 1;
static char* generic_dev = NULL;
static int sector_size = 0;
static int search_overlap = -1;
static int toc_bias = 0;
static int toc_offset = 0;
static int no_skip = 0;

static const config_t cdda_opts[] = {
  { "speed", &speed, CONF_TYPE_INT, CONF_RANGE,1,100, NULL },
  { "paranoia", &paranoia_mode, CONF_TYPE_INT,CONF_RANGE, 0, 2, NULL },
  { "generic-dev", &generic_dev, CONF_TYPE_STRING, 0, 0, 0, NULL },
  { "sector-size", &sector_size, CONF_TYPE_INT, CONF_RANGE,1,100, NULL },
  { "overlap", &search_overlap, CONF_TYPE_INT, CONF_RANGE,0,75, NULL },
  { "toc-bias", &toc_bias, CONF_TYPE_INT, 0, 0, 0, NULL },
  { "toc-offset", &toc_offset, CONF_TYPE_INT, 0, 0, 0, NULL },
  { "noskip", &no_skip, CONF_TYPE_FLAG, 0 , 0, 1, NULL },
  { "skip", &no_skip, CONF_TYPE_FLAG, 0 , 1, 0, NULL },
  {NULL, NULL, 0, 0, 0, 0, NULL}
};

static const config_t cdda_conf[] = {
  { "cdda", &cdda_opts, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
  { NULL,NULL, 0, 0, 0, 0, NULL}
};

void cdda_register_options(m_config_t* cfg) {
  m_config_register_options(cfg,cdda_conf);
}

int __FASTCALL__ open_cdda(stream_t *st,const char* dev,const char* track) {
  int start_track = 0;
  int end_track = 0;
  int mode = paranoia_mode;
  int offset = toc_offset;
  unsigned cd_tracks;
  cdrom_drive* cdd = NULL;
  cdda_priv* priv;
  char* end = strchr(track,'-');
  cd_info_t *cd_info;
  unsigned int audiolen=0;
  int i;

  if(!end)
    start_track = end_track = atoi(track);
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
    
  if(generic_dev)
    cdd = cdda_identify_scsi(generic_dev,dev,0,NULL);
  else
    cdd = cdda_identify(dev,0,NULL);

  if(!cdd) {
    MSG_ERR("Can't open cdda device\n");
    return 0;
  }

  cdda_verbose_set(cdd, CDDA_MESSAGE_FORGETIT, CDDA_MESSAGE_FORGETIT);

  if(sector_size) {
    cdd->nsectors = sector_size;
    cdd->bigbuff = sector_size * CD_FRAMESIZE_RAW;
  }

  if(cdda_open(cdd) != 0) {
    MSG_ERR("Can't open disc\n");
    cdda_close(cdd);
    return 0;
  }

  cd_info = cd_info_new();
  cd_tracks=cdda_tracks(cdd);
  for(i=0;i<cdd->tracks;i++) {
	  char track_name[80];
	  long sec=cdda_track_firstsector(cdd,i+1);
	  long off=cdda_track_lastsector(cdd,i+1)-sec+1;

	  sprintf(track_name, "Track %d/%d", i+1,cd_tracks);
	  cd_info_add_track(cd_info, track_name, i+1, (unsigned int)(off/(60*75)), (unsigned int)((off/75)%60), (unsigned int)(off%75), sec, off );
	  audiolen += off;
  }
  cd_info->min  = (unsigned int)(audiolen/(60*75));
  cd_info->sec  = (unsigned int)((audiolen/75)%60);
  cd_info->msec = (unsigned int)(audiolen%75);

  priv = (cdda_priv*)malloc(sizeof(cdda_priv));
  memset(priv, 0, sizeof(cdda_priv));
  priv->cd = cdd;
  priv->cd_info = cd_info;

  if(toc_bias)
    offset -= cdda_track_firstsector(cdd,1);

  if(offset) {
    int i;
    for(i = 0 ; i < cdd->tracks + 1 ; i++)
      cdd->disc_toc[i].dwStartSector += offset;
  }

  if(speed)
    cdda_speed_set(cdd,speed);

  if(start_track)
    priv->start_sector = cdda_track_firstsector(cdd,start_track);
  else
    priv->start_sector = cdda_disc_firstsector(cdd);

  if(end_track) {
    int last = cdda_tracks(cdd);
    if(end_track > last) end_track = last;
    priv->end_sector = cdda_track_lastsector(cdd,end_track);
  } else
    priv->end_sector = cdda_disc_lastsector(cdd);

  priv->cdp = paranoia_init(cdd);
  if(priv->cdp == NULL) {
    cdda_close(cdd);
    free(priv);
    return 0;
  }

  if(mode == 0)
    mode = PARANOIA_MODE_DISABLE;
  else if(mode == 1)
    mode = PARANOIA_MODE_OVERLAP;
  else
    mode = PARANOIA_MODE_FULL;
  
  if(no_skip)
    mode |= PARANOIA_MODE_NEVERSKIP;

  if(search_overlap >= 0)
    paranoia_overlapset(cdd,search_overlap);

  paranoia_seek(priv->cdp,priv->start_sector,SEEK_SET);
  priv->sector = priv->start_sector;

  st->type = STREAMTYPE_SEEKABLE|STREAMTYPE_RAWAUDIO;
  st->priv = priv;
  st->start_pos = priv->start_sector*CD_FRAMESIZE_RAW;
  st->end_pos = priv->end_sector*CD_FRAMESIZE_RAW;

  return 1;
}

static void cdparanoia_callback(long inpos, int function) {
}

int __FASTCALL__ read_cdda(stream_t* s,char *buf,int *tr) {
  cdda_priv* p = (cdda_priv*)s->priv;
  cd_track_t *cd_track;
  int16_t * _buf;
  unsigned int i;
  
  *tr==-1;
  _buf = paranoia_read(p->cdp,cdparanoia_callback);

  p->sector++;
  memcpy(buf,_buf,CD_FRAMESIZE_RAW);

  if(p->sector == p->end_sector) return -1;

  for(i=0;i<p->cd->tracks;i++){
	  if(p->cd->disc_toc[i].dwStartSector==p->sector-1) {
		  cd_track = cd_info_get_track(p->cd_info, i+1);
//printf("Track %d, sector=%d\n", i, p->sector-1);
		  *tr=i+1;
		  if( cd_track!=NULL ) {
			  MSG_V("%s\n", cd_track->name ); 
		  }
		  break;
	  }
  }

  
  return CD_FRAMESIZE_RAW;
}

void __FASTCALL__ seek_cdda(stream_t* s,off_t pos) {
  cdda_priv* p = (cdda_priv*)s->priv;
  cd_track_t *cd_track;
  int sec;
  int current_track=0, seeked_track=0;
  int i;

  sec = pos/CD_FRAMESIZE_RAW;
 
  for(i=0;i<p->cd->tracks;i++){
	if( p->sector>p->cd->disc_toc[i].dwStartSector && p->sector<p->cd->disc_toc[i+1].dwStartSector ) {
		current_track = i;
	}
	if( sec>p->cd->disc_toc[i].dwStartSector && sec<p->cd->disc_toc[i+1].dwStartSector ) {
		seeked_track = i;
	}
  }
//printf("current: %d, seeked: %d\n", current_track, seeked_track);
	if( current_track!=seeked_track ) {
//printf("Track %d, sector=%d\n", seeked_track, sec);
		  cd_track = cd_info_get_track(p->cd_info, seeked_track+1);
		  if( cd_track!=NULL ) {
			  MSG_V("%s\n", cd_track->name ); 
		  }

	}
 
  if(sec < p->start_sector)
    sec = p->start_sector;
  else if(sec > p->end_sector)
    sec = p->end_sector;

  p->sector = sec;

  paranoia_seek(p->cdp,sec,SEEK_SET);
}

off_t __FASTCALL__ tell_cdda(stream_t* s) {
  cdda_priv* p = (cdda_priv*)s->priv;
  return p->sector*CD_FRAMESIZE_RAW;
}

void __FASTCALL__ close_cdda(stream_t* s) {
  cdda_priv* p = (cdda_priv*)s->priv;
  paranoia_free(p->cdp);
  cdda_close(p->cd);
}

#endif
