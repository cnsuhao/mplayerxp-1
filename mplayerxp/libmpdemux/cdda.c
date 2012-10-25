#include "mp_config.h"
#include "mplayer.h"

#ifdef HAVE_LIBCDIO

#include "stream.h"
#include "libmpconf/cfgparser.h"

#include <stdio.h>
#include <stdlib.h>

#include "cdio/cdtext.h"
#include "cdd.h"
#include "demux_msg.h"
static int speed = -1;
static int search_overlap = -1;
static int no_skip = 0;

static const config_t cdda_opts[] = {
  { "speed", &speed, CONF_TYPE_INT, CONF_RANGE,1,100, NULL, "sets driver speed" },
  { "overlap", &search_overlap, CONF_TYPE_INT, CONF_RANGE,0,75, NULL, "reserved" },
  { "noskip", &no_skip, CONF_TYPE_FLAG, 0 , 0, 1, NULL, "reserved" },
  { "skip", &no_skip, CONF_TYPE_FLAG, 0 , 1, 0, NULL, "reserved" },
  {NULL, NULL, 0, 0, 0, 0, NULL, NULL}
};

static const config_t cdda_conf[] = {
  { "cdda", &cdda_opts, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL, "CD-DA related options"},
  { NULL,NULL, 0, 0, 0, 0, NULL, NULL}
};

void cdda_register_options(m_config_t* cfg) {
  m_config_register_options(cfg,cdda_conf);
}

static unsigned cdda_parse_tracks(unsigned char *arr,unsigned nelem,const char *arg)
{
    const char *st,*end;
    unsigned rval=0;
    unsigned slen=strlen(arg);
    st=arg;
    memset(arr,0,sizeof(unsigned char)*nelem);
    do {
	size_t datalen,value,evalue,i;
	char data[100],*range;
	end=strchr(st,',');
	if(!end) end = &st[strlen(st)];
	datalen=end-st;
	memcpy(data,st,datalen);
	data[datalen]='\0';
	range=strchr(data,'-');
	if(range) {
	    *range='\0';
	    value=atoi(data);
	    evalue=atoi(&range[1]);
	    if(evalue>value && evalue<nelem) {
		for(i=value;i<=evalue;i++) arr[i]=1;
		rval=evalue;
	    }
	    else break;
	}
	else {
	    value=atoi(data);
	    if(value>nelem) break;
	    arr[value]=1;
	    rval=value;
	}
	st=end+1;
	if(st>arg+slen) break;
    }while(end);
    return rval;
}

int __FASTCALL__ open_cdda(stream_t *st,const char* dev,const char* arg) {
  unsigned cd_tracks;
  cdda_priv* priv;
  unsigned int audiolen=0;
  unsigned i;
  unsigned char arr[256];
  int st_inited;

  priv = (cdda_priv*)malloc(sizeof(cdda_priv));
  memset(priv, 0, sizeof(cdda_priv));

  priv->cd = cdio_cddap_identify(dev,mp_conf.verbose?1:0,NULL);

  if(!priv->cd) {
    MSG_ERR("Can't open cdda device: %s\n",dev);
    free(priv);
    return 0;
  }

  cdio_cddap_verbose_set(priv->cd, mp_conf.verbose?CDDA_MESSAGE_PRINTIT:CDDA_MESSAGE_FORGETIT, mp_conf.verbose?CDDA_MESSAGE_PRINTIT:CDDA_MESSAGE_FORGETIT);

  if(cdio_cddap_open(priv->cd) != 0) {
    MSG_ERR("Can't open disc\n");
    cdda_close(priv->cd);
    free(priv);
    return 0;
  }

  cd_tracks=cdio_cddap_tracks(priv->cd);
  MSG_V("Found %d tracks on disc\n",cd_tracks);
  if(!arg[0])
    for(i=1;i<=cd_tracks;i++) priv->tracks[i-1].play=1;
  cdda_parse_tracks(arr,sizeof(arr)/sizeof(unsigned),arg);
  for(i=1;i<=256;i++) if (arr[i]) priv->tracks[i-1].play=1;

  st_inited=0;
  MSG_V("[CDDA] Queued tracks:");
  for(i=0;i<cd_tracks;i++) {
    if(priv->tracks[i].play) {
	priv->tracks[i].start_sector=cdio_cddap_track_firstsector(priv->cd,i+1);
	priv->tracks[i].end_sector=cdio_cddap_track_lastsector(priv->cd,i+1);
	MSG_V(" %d[%d-%d]",i+1,priv->tracks[i].start_sector,priv->tracks[i].end_sector);
	if(!st_inited) { priv->start_sector=priv->tracks[i].start_sector; st_inited=1; }
	priv->end_sector=priv->tracks[i].end_sector;
	audiolen += priv->tracks[i].end_sector-priv->tracks[i].start_sector+1;
    }
  }
  for(;i<256;i++) priv->tracks[i].play=0;
  MSG_V("\n");
  priv->min  = (unsigned int)(audiolen/(60*75));
  priv->sec  = (unsigned int)((audiolen/75)%60);
  priv->msec = (unsigned int)(audiolen%75);

  if(speed)
    cdio_cddap_speed_set(priv->cd,speed);

  priv->sector = priv->start_sector;
  st->type = STREAMTYPE_SEEKABLE|STREAMTYPE_RAWAUDIO;
  st->priv = priv;
  st->start_pos = priv->start_sector*CDIO_CD_FRAMESIZE_RAW;
  st->end_pos = priv->end_sector*CDIO_CD_FRAMESIZE_RAW;
  return 1;
}

static lsn_t map_sector(cdda_priv*p,lsn_t sector,track_t *tr)
{
    unsigned i,j;
    lsn_t cd_track=sector;
    for(i=0;i<256;i++){
	if(p->tracks[i].play && p->tracks[i].end_sector==sector) {
		cd_track=0;
		MSG_V("Found track changing. old track=%u Sector=%u",i,sector);
		for(j=i+1;j<256;j++) {
		    if(p->tracks[j].play && p->tracks[j].start_sector==sector+1) {
			cd_track=p->tracks[j].start_sector;
			if(tr) *tr=j;
			MSG_V("new track=%u Sector=%u",j,cd_track);
		    }
		}
	}
    }
    return cd_track;
}

/* return physical sector address */
static unsigned long psa(cdda_priv*p,unsigned long sector)
{
    unsigned i;
    unsigned long got_sectors=p->start_sector,track_len;
    for(i=0;i<256;i++){
	if(p->tracks[i].play) {
	    track_len=p->tracks[i].end_sector-p->tracks[i].start_sector;
	    if(sector>=got_sectors && sector <= track_len) return sector+p->tracks[i].start_sector;
	    got_sectors+=track_len;
	}
    }
    return 0;
}

int __FASTCALL__ read_cdda(stream_t* s,char *buf,track_t *tr) {
  cdda_priv* p = (cdda_priv*)s->priv;
  track_t i=255;

  if(cdio_cddap_read(p->cd, buf, p->sector, 1)==0) {
    MSG_ERR("[CD-DA]: read error occured\n");
    return -1; /* EOF */
  }
  p->sector++;
  if(p->sector == p->end_sector) {
    MSG_DBG2("EOF was reached\n");
    return -1; /* EOF */
  }

  p->sector=map_sector(p,p->sector,&i);
  if(!p->sector) return -1;
  if(i!=255) {
    *tr=i+1;
    MSG_V("Track %d, sector=%d\n", *tr, p->sector);
  }
  else MSG_DBG2("Track %d, sector=%d\n", *tr, p->sector);
  return CDIO_CD_FRAMESIZE_RAW;
}

void __FASTCALL__ seek_cdda(stream_t* s,off_t pos,track_t *tr) {
    cdda_priv* p = (cdda_priv*)s->priv;
    long sec;
    long seeked_track=0;
    track_t j=255;

    sec = pos/CDIO_CD_FRAMESIZE_RAW;
    MSG_DBG2("[CDDA] prepare seek to %ld\n",sec);
    seeked_track=sec;
    *tr=255;
    if( p->sector!=seeked_track ) {
	seeked_track = map_sector(p,seeked_track,&j);
	if(seeked_track) *tr=j+1;
    }
    p->sector=seeked_track;
}

off_t __FASTCALL__ tell_cdda(stream_t* s) {
  cdda_priv* p = (cdda_priv*)s->priv;
  return p->sector*CDIO_CD_FRAMESIZE_RAW;
}

void __FASTCALL__ close_cdda(stream_t* s) {
  cdda_priv* p = (cdda_priv*)s->priv;
  cdio_cddap_close(p->cd);
  free(p);
}

#endif
