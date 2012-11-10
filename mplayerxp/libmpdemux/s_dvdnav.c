/*
    s_dvdnav - DVDNAV's stream interface
*/
#include "../mp_config.h"
#ifdef USE_DVDNAV
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "stream.h"
#include "help_mp.h"
#include "demuxer.h"
#include "libmpsub/spudec.h"
#include "libvo/sub.h"
#include "input/input.h"
#include "mplayerxp.h"
#include "demux_msg.h"

#include <dvdnav/dvdnav.h>
#include <stdio.h>
#include <unistd.h>
#include "osdep/timer.h"
#include "osdep/mplib.h"
#include "mrl.h"
#define DVD_BLOCK_SIZE 2048

#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif
#ifndef max
#define max(a,b) ((a)>(b)?(a):(b))
#endif

extern vo_data_t* vo_data;

typedef struct {
  dvdnav_t *       dvdnav;              /* handle to libdvdnav stuff */
  char *           filename;            /* path */
  int              ignore_timers;       /* should timers be skipped? */
  int              sleeping;            /* are we sleeping? */
  unsigned int     sleep_until;         /* timer */
  int              started;             /* Has mplayer initialization finished? */
  unsigned char    prebuf[STREAM_BUFFER_SIZE]; /* prefill buffer */
  int              prelen;              /* length of prefill buffer */
  off_t		   cpos;
  float		   vobu_s_pts,vobu_e_pts;
  int		   menu_mode;
  dvdnav_highlight_event_t hlev;
} dvdnav_priv_t;

typedef struct {
  int event;             /* event number fromd dvdnav_events.h */
  any_t* details;        /* event details */
  int len;               /* bytes in details */
} dvdnav_event_t;

static int dvd_nav_still=0;            /* are we on a still picture? */
static int dvd_nav_skip_opening=0;     /* skip opening stalls? */

static void __FASTCALL__ dvdnav_stream_ignore_timers(stream_t * stream, int ignore) {
  dvdnav_priv_t *dvdnav_priv=stream->priv;
  dvdnav_priv->ignore_timers=ignore;
}

static dvdnav_priv_t * __FASTCALL__ new_dvdnav_stream(stream_t *stream,char * filename) {
  const char * title_str;
  dvdnav_priv_t *dvdnav_priv;

  if (!filename)
    return NULL;

  if (!(dvdnav_priv=(dvdnav_priv_t*)mp_calloc(1,sizeof(*dvdnav_priv))))
    return NULL;

  if (!(dvdnav_priv->filename=mp_strdup(filename))) {
    mp_free(dvdnav_priv);
    return NULL;
  }

  if(dvdnav_open(&(dvdnav_priv->dvdnav),dvdnav_priv->filename)!=DVDNAV_STATUS_OK)
  {
    mp_free(dvdnav_priv->filename);
    mp_free(dvdnav_priv);
    return NULL;
  }

  if (!dvdnav_priv->dvdnav) {
    mp_free(dvdnav_priv);
    return NULL;
  }

  stream->priv=dvdnav_priv;
  dvdnav_stream_ignore_timers(stream,dvd_nav_skip_opening);

  if(1)	//from vlc: if not used dvdnav from cvs will fail
  {
    int len, event;
    char buf[2048];

    dvdnav_get_next_block(dvdnav_priv->dvdnav,buf,&event,&len);
    dvdnav_sector_search(dvdnav_priv->dvdnav, 0, SEEK_SET);
  }

  /* turn on/off dvdnav caching */
  dvdnav_set_readahead_flag(dvdnav_priv->dvdnav,mp_conf.s_cache_size?0:1);

  /* report the title?! */
  if (dvdnav_get_title_string(dvdnav_priv->dvdnav,&title_str)==DVDNAV_STATUS_OK) {
    MSG_INFO("Title: '%s'\n",title_str);
  }
  return dvdnav_priv;
}

static void __FASTCALL__ dvdnav_stream_sleep(stream_t * stream, int seconds) {
    dvdnav_priv_t *dvdnav_priv=stream->priv;

    if (!dvdnav_priv->started) return;

    dvdnav_priv->sleeping=0;
    switch (seconds) {
    case 0:
            return;
    case 0xff:
            MSG_V( "Sleeping indefinately\n" );
            dvdnav_priv->sleeping=2;
            break;
    default:
            MSG_V( "Sleeping %d sec(s)\n", seconds );
            dvdnav_priv->sleep_until = GetTimer();// + seconds*1000000;
            dvdnav_priv->sleeping=1;
            break;
    }
    //if (dvdnav_priv->started) dvd_nav_still=1;
}

static int __FASTCALL__ dvdnav_stream_sleeping(stream_t * stream) {
    dvdnav_priv_t *dvdnav_priv=stream->priv;
    unsigned int now;

    if (!dvdnav_priv) return 0;

    if(dvdnav_priv->sleeping)
    {
      now=GetTimer();
      while(dvdnav_priv->sleeping>1 || now<dvdnav_priv->sleep_until) {
//        usec_sleep(1000); /* 1ms granularity */
        return 1; 
      }
      dvdnav_still_skip(dvdnav_priv->dvdnav); // continue past...
      dvdnav_priv->sleeping=0;
      MSG_V("%s: woke up!\n",__FUNCTION__);
    }
    dvd_nav_still=0;
    MSG_V("%s: active\n",__FUNCTION__);
    return 0;
}

static unsigned int * __FASTCALL__ dvdnav_stream_get_palette(stream_t * stream) {
#if 0 /* latest versions if libdvdnav don't provide such info */
  dvdnav_priv_t *dvdnav_priv=stream->priv;
  if (!dvdnav_priv) {
    MSG_V("%s: NULL dvdnav_priv\n",__FUNCTION__);
    return NULL;
  }
  if (!dvdnav_priv->dvdnav) {
    MSG_V("%s: NULL dvdnav_priv->dvdnav\n",__FUNCTION__);
    return NULL;
  }
  if (!dvdnav_priv->dvdnav->vm) {
    MSG_V("%s: NULL dvdnav_priv->dvdnav->vm\n",__FUNCTION__);
    return NULL;
  }
  if (!dvdnav_priv->dvdnav->vm->state.pgc) {
    MSG_V("%s: NULL dvdnav_priv->dvdnav->vm->state.pgc\n",__FUNCTION__);
    return NULL;
  }
  return dvdnav_priv->dvdnav->vm->state.pgc->palette;
#else
  UNUSED(stream);
#endif
  return 0;
}

static dvdnav_event_t *tevent;
static int tevent_full=0;
static int dvd_title=-1,dvd_chapter=-1;

static const mrl_config_t dvdnavopts_conf[]={
	{ "skipopening", &dvd_nav_skip_opening, MRL_TYPE_BOOL, 0, 1 },
	{ "T", &dvd_title, MRL_TYPE_INT, 0, 999 },
	{ "C", &dvd_chapter, MRL_TYPE_INT, 0, 999 },
	{ NULL, NULL, 0, 0, 0 }
};

static int __FASTCALL__ __dvdnav_open(any_t*libinput,stream_t *stream,const char *filename,unsigned flags)
{
    const char *param;
    char *dvd_device;
    int ntitles;
    UNUSED(flags);
    UNUSED(libinput);
    stream->type = STREAMTYPE_SEEKABLE|STREAMTYPE_PROGRAM;
    param=mrl_parse_line(filename,NULL,NULL,&dvd_device,NULL);
    if(strcmp(param,"help") == 0)
    {
	MSG_HINT("Usage: dvdnav://<title>,<chapter>\n");
	return 0;
    }
    param=mrl_parse_params(param,dvdnavopts_conf);
    if (!(stream->priv=new_dvdnav_stream(stream,dvd_device?dvd_device:DEFAULT_DVD_DEVICE))) {
	MSG_ERR(MSGTR_CantOpenDVD,dvd_device?dvd_device:DEFAULT_DVD_DEVICE);
	if(!dvd_device)
	{
	    if (!(stream->priv=new_dvdnav_stream(stream,DEFAULT_CDROM_DEVICE)))
		MSG_ERR(MSGTR_CantOpenDVD,DEFAULT_CDROM_DEVICE);
	    else
		goto dvd_ok;
	}
	mp_free(stream->priv);
	if(dvd_device) mp_free(dvd_device);
        return 0;
    }
    dvd_ok:
    if(dvd_device) mp_free(dvd_device);
    ((dvdnav_priv_t *)stream->priv)->started=1;
    if(mp_conf.s_cache_size)
    {
	tevent = mp_malloc(sizeof(dvdnav_event_t));
	if(tevent) 
	    if((tevent->details=mp_malloc(DVD_BLOCK_SIZE))==NULL)
	    {
		mp_free(tevent);
		tevent=NULL;
	    }
    }
    tevent_full=0;
    /* By rumours 1 PGC == whole movie */
    dvdnav_set_PGC_positioning_flag(((dvdnav_priv_t *)stream->priv)->dvdnav,1);
    ntitles=0;
    dvdnav_get_number_of_titles(((dvdnav_priv_t *)stream->priv)->dvdnav,&ntitles);
    MSG_INFO(MSGTR_DVDnumTitles,ntitles);
    if(dvd_title != -1)
    {
	int nparts;
	dvdnav_get_number_of_parts(((dvdnav_priv_t *)stream->priv)->dvdnav,dvd_title,&nparts);
	MSG_INFO(MSGTR_DVDnumChapters,dvd_title,nparts);
	if(dvd_chapter != -1)	dvdnav_part_play(((dvdnav_priv_t *)stream->priv)->dvdnav,dvd_title,dvd_chapter);
	else			dvdnav_title_play(((dvdnav_priv_t *)stream->priv)->dvdnav,dvd_title);
	((dvdnav_priv_t *)stream->priv)->cpos=stream->start_pos=2048; /* disallow dvdnav_reset */
	dvdnav_current_title_info(((dvdnav_priv_t *)stream->priv)->dvdnav,&dvd_title,&dvd_chapter);
	MSG_INFO("Playing %i part of %i title\n",dvd_chapter,dvd_title);
    }
    stream->sector_size=tevent?DVD_BLOCK_SIZE*10:DVD_BLOCK_SIZE;
    if(	dvdnav_is_domain_vmgm(((dvdnav_priv_t *)stream->priv)->dvdnav) ||
	dvdnav_is_domain_vtsm(((dvdnav_priv_t *)stream->priv)->dvdnav))
		stream->type = STREAMTYPE_MENU|STREAMTYPE_SEEKABLE;
    return 1;
}

static void __FASTCALL__ dvdnav_stream_read(stream_t * stream, dvdnav_event_t*de) {
  dvdnav_priv_t *dvdnav_priv=stream->priv;
  int event = DVDNAV_NOP;
  int done;

  if (!de->len) return;
  de->len=-1;
  if (!dvdnav_priv) return;
  if (!de->details) return;

  if (dvd_nav_still) {
    MSG_V("%s: got a stream_read while I should be asleep!\n",__FUNCTION__);
    de->event=DVDNAV_STILL_FRAME;
    de->len=0;
    return;
  }
  done=0;
  while(!done)
  {
    if (dvdnav_get_next_block(dvdnav_priv->dvdnav,de->details,&event,&de->len)!=DVDNAV_STATUS_OK)
    {
	MSG_ERR( "Error getting next block from DVD (%s)\n",dvdnav_err_to_string(dvdnav_priv->dvdnav) );
	de->len=-1;
    }
    if(event == DVDNAV_STILL_FRAME)
    {
        dvdnav_still_skip(dvdnav_priv->dvdnav); /* don't let dvdnav stall on this image */
	while (dvdnav_stream_sleeping(stream)) usleep(1000); /* 1ms */
    }
#ifdef DVDNAV_WAIT
    else
    if(event == DVDNAV_WAIT)
    {
	usleep(1000);
        dvdnav_wait_skip(dvdnav_priv->dvdnav); /* don't let dvdnav stall on this image */
    }
#endif
    else
    if(event == DVDNAV_NAV_PACKET)
    {
	/* Try to suppress PTS discontinuity here!!! */
	pci_t *_this;
	float vobu_s_pts,vobu_e_pts;
	_this=dvdnav_get_current_nav_pci(dvdnav_priv->dvdnav);
	vobu_s_pts=_this->pci_gi.vobu_s_ptm/90000.;
	vobu_e_pts=_this->pci_gi.vobu_e_ptm/90000.;
	MSG_V("Handling NAV_PACKET: vobu_s_ptm=%f vobu_e_ptm=%f e_eltm=%f\n"
		,vobu_s_pts
		,vobu_e_pts
		,(float)_this->pci_gi.e_eltm.second+_this->pci_gi.e_eltm.minute*60.+_this->pci_gi.e_eltm.hour*3600.);
	if(vobu_s_pts < dvdnav_priv->vobu_e_pts)
	{
	    stream->stream_pts += dvdnav_priv->vobu_e_pts-vobu_s_pts;
	    MSG_V("DVD's discontinuities found! Applying delta: %f\n",stream->stream_pts);
	}
	else stream->stream_pts = vobu_s_pts;
	dvdnav_priv->vobu_s_pts = vobu_s_pts;
	dvdnav_priv->vobu_e_pts = vobu_e_pts;
    }
    else
    if(event == DVDNAV_CELL_CHANGE)
    {
	int ct,cc;
	dvdnav_current_title_info(dvdnav_priv->dvdnav, &ct, &cc);
	if(ct<=0) {
	    dvdnav_priv->menu_mode=1;
	    MSG_V("entering menu mode: %i %i\n",ct,cc);
	    MSG_V("vmgm: %i vtsm: %i\n",
		dvdnav_is_domain_vmgm(dvdnav_priv->dvdnav),
		dvdnav_is_domain_vtsm(dvdnav_priv->dvdnav));
	}
	else {
	    dvdnav_priv->menu_mode=0;
	    MSG_V("leaving menu mode: %i %i\n",ct,cc);
	}
	/**/
	if(dvdnav_priv->menu_mode)
		stream->type = STREAMTYPE_MENU|STREAMTYPE_SEEKABLE;
	else	stream->type = STREAMTYPE_SEEKABLE|STREAMTYPE_PROGRAM;
    }
    else done=1;
  }
  if(!event) dvdnav_priv->cpos += DVD_BLOCK_SIZE;
  de->event=event;
}

static int __FASTCALL__ __dvdnav_read(stream_t *stream,stream_packet_t *sp)
{
    dvdnav_event_t de;
    unsigned len=sp->len;
    if(tevent && tevent_full)
    {
	sp->len=tevent->len;
	sp->type=tevent->event;
	memcpy(sp->buf,tevent->details,tevent->len);
	tevent_full=0;
	return sp->len;
    }
    de.len=sp->len;
    de.details=sp->buf;
    dvdnav_stream_read(stream,&de);
    sp->len=de.len;
    sp->type=de.event;
    if(tevent && !sp->type)
    {
	len -= sp->len;
	while(len)
	{
	    de.len=len;
	    de.details=&sp->buf[sp->len];
	    dvdnav_stream_read(stream,&de);
	    if(de.event)
	    {
		tevent->len=de.len;
		tevent->event=de.event;
		memcpy(tevent->details,de.details,de.len);
		tevent_full=1;
		break;
	    }
	    if(de.len<0 || (!de.event&&de.len==0)) break;
	    sp->len += de.len;
	    len-=de.len;
	}
    }
    return sp->len;
}

static off_t __FASTCALL__ __dvdnav_seek(stream_t *stream,off_t pos)
{
  dvdnav_priv_t *dvdnav_priv=stream->priv;
  uint32_t newpos=0;
  uint32_t length=1;
  uint32_t sector;

  if (pos==0)
  {
	dvdnav_priv->started=0;
	dvdnav_priv->cpos=0;
	return 0;
  }
  sector=pos/DVD_BLOCK_SIZE;
  dvdnav_sector_search(dvdnav_priv->dvdnav,sector,SEEK_SET);
  usleep(0); /* wait for HOP_CHANNEL event */
  dvdnav_get_position(dvdnav_priv->dvdnav, &newpos, &length);
  if(newpos > sector) newpos=sector;
  dvdnav_priv->cpos = (newpos)*2048;
  /* reset pts_fix after seeking */
  {
	dvdnav_priv->vobu_s_pts=
	dvdnav_priv->vobu_e_pts=
	stream->stream_pts=0;
  }
  return dvdnav_priv->cpos;
}

static off_t __FASTCALL__ __dvdnav_tell(stream_t *stream)
{
  dvdnav_priv_t *dvdnav_priv=stream->priv;
  return (off_t)dvdnav_priv->cpos;
}

static void __FASTCALL__ __dvdnav_close(stream_t *stream)
{
  dvdnav_priv_t *dvdnav_priv=stream->priv;
  dvdnav_close(dvdnav_priv->dvdnav);
  mp_free(dvdnav_priv);
  if(tevent) { mp_free(tevent->details); mp_free(tevent); }
}

/**
 * \brief mp_dvdnav_get_highlight() get dvdnav highlight struct
 * \param stream: - stream pointer
 * \param hl    : - highlight struct pointer
 */
static void mp_dvdnav_get_highlight (stream_t *stream, rect_highlight_t *hl) {
  dvdnav_priv_t *priv = (dvdnav_priv_t *) stream->priv;
  int button;
  dvdnav_highlight_area_t ha;
  pci_t *pnavpci = NULL;

  dvdnav_get_current_highlight(priv->dvdnav, &button);
  pnavpci = dvdnav_get_current_nav_pci (priv->dvdnav);
  /* highlight mode: 0 - hide, 1 - show, 2 - activate, currently always 1 */
  dvdnav_get_highlight_area(pnavpci, button, 1, &ha);

  hl->sx = ha.sx;
  hl->sy = ha.sy;
  hl->ex = ha.ex;
  hl->ey = ha.ey;
}

static void __FASTCALL__ dvdnav_event_handler(stream_t* s,const stream_packet_t*sp)
{
    demux_stream_t *d_audio=s->demuxer->audio;
    dvdnav_priv_t *priv=s->priv;
    switch(sp->type) {
	    case DVDNAV_BLOCK_OK: /* be silent about this one */
            			break;
	    case DVDNAV_HIGHLIGHT: {
				pci_t *pnavpci = NULL;
				dvdnav_highlight_event_t *_hlev = (dvdnav_highlight_event_t*)(sp->buf);
				int btnum;
				int display_mode=1;
				MSG_V("DVDNAV_HIGHLIGHT: %i %i %i %i\n",_hlev->sx,_hlev->sy,_hlev->ex,_hlev->ey);
				if (!priv || !priv->dvdnav)  return;
				memcpy(&priv->hlev,_hlev,sizeof(dvdnav_highlight_event_t));
				pnavpci = dvdnav_get_current_nav_pci (priv->dvdnav);
				if (!pnavpci)   return;

				dvdnav_get_current_highlight (priv->dvdnav, &(priv->hlev.buttonN));
				priv->hlev.display = display_mode; /* show */

				if (priv->hlev.buttonN > 0 && pnavpci->hli.hl_gi.btn_ns > 0 && priv->hlev.display) {
				for (btnum = 0; btnum < pnavpci->hli.hl_gi.btn_ns; btnum++) {
					btni_t *btni = &(pnavpci->hli.btnit[btnum]);

					if (priv->hlev.buttonN == (unsigned)btnum + 1) {
					    priv->hlev.sx = min (btni->x_start, btni->x_end);
					    priv->hlev.ex = max (btni->x_start, btni->x_end);
					    priv->hlev.sy = min (btni->y_start, btni->y_end);
					    priv->hlev.ey = max (btni->y_start, btni->y_end);

					    priv->hlev.palette = (btni->btn_coln == 0) ? 0 :
							pnavpci->hli.btn_colit.btn_coli[btni->btn_coln - 1][0];
					    break;
					}
				}
				} else { /* hide button or no button */
					priv->hlev.sx = priv->hlev.ex = 0;
					priv->hlev.sy = priv->hlev.ey = 0;
					priv->hlev.palette = priv->hlev.buttonN = 0;
		    }
		    break;
		}
	    case DVDNAV_STILL_FRAME: {
		const dvdnav_still_event_t *still_event = (const dvdnav_still_event_t*)(sp->buf);
		    MSG_DBG2( "######## DVDNAV Event: Still Frame: %d sec(s)\n", still_event->length );
			while (dvdnav_stream_sleeping(s)) {
		    usleep(1000); /* 1ms */
		}
		dvdnav_stream_sleep(s,still_event->length);
		break;
		}
	    case DVDNAV_STOP:
		MSG_DBG2( "DVDNAV Event: Nav Stop\n" );
		break;
	    case DVDNAV_NOP:
		MSG_V("DVDNAV Event: Nav NOP\n");
		break;
#if 0
	    case DVDNAV_SPU_STREAM_CHANGE: {
		const dvdnav_spu_stream_change_event_t * stream_change=(const dvdnav_spu_stream_change_event_t*)(sp->buf);
		MSG_DBG2("DVDNAV Event: Nav SPU Stream Change: phys_wide: %d phys_letterbox: %d phys_panscan: %d logical: %d\n",
		stream_change->physical_wide,
		stream_change->physical_letterbox,
		stream_change->physical_pan_scan,
		stream_change->logical);
		if (vo_data->spudec && mp_conf.dvdsub_id!=stream_change->physical_wide) {
		    MSG_DBG2("d_dvdsub->id change: was %d is now %d\n",
			    d_dvdsub->id,stream_change->physical_wide);
		    // FIXME: need a better way to change SPU id
		    d_dvdsub->id=mp_conf.dvdsub_id=stream_change->physical_wide;
		    if (vo_data->spudec) spudec_reset(vo_data->spudec);
		}
		break;
		}
#endif
	    case DVDNAV_AUDIO_STREAM_CHANGE: {
		int aid_temp;
		const dvdnav_audio_stream_change_event_t *stream_change = (const dvdnav_audio_stream_change_event_t*)(sp->buf);
		MSG_DBG2("DVDNAV Event: Nav Audio Stream Change: phys: %d logical: %d\n",
		    stream_change->physical,
		    stream_change->logical);
		aid_temp=stream_change->physical;
		if (aid_temp>=0) aid_temp+=128; // FIXME: is this sane?
		if (d_audio && mp_conf.audio_id!=aid_temp) {
		    MSG_DBG2("d_audio->id change: was %d is now %d\n",
			d_audio->id,aid_temp);
		    // FIXME: need a bettery way to change audio stream id
		    d_audio->id=mp_conf.dvdsub_id=aid_temp;
		    mpxp_resync_audio_stream();
		}
		break;
		}
	    case DVDNAV_VTS_CHANGE:{
		const dvdnav_vts_change_event_t *evts = (const dvdnav_vts_change_event_t *)(sp->buf);
		MSG_V("DVDNAV Event: Nav VTS Change %u\n",evts->new_domain);
		}
		break;
	    case DVDNAV_CELL_CHANGE: {
		const dvdnav_cell_change_event_t *ecell=(const dvdnav_cell_change_event_t*)(sp->buf);
		MSG_V("DVDNAV_CELL_CHANGE: N=%i pgN=%i cell_start=%f pg_start=%f cell_length=%f pg_length=%f pgc_length=%f\n"
		,ecell->cellN
		,ecell->pgN
		,ecell->cell_start/90000.
		,ecell->pg_start/90000.
		,ecell->cell_length/90000.
		,ecell->pg_length/90000.
		,ecell->pgc_length/90000.);
		}
		break;
	    case DVDNAV_NAV_PACKET:
		MSG_V("DVDNAV Event: Nav Packet\n");
		break;
	    case DVDNAV_SPU_CLUT_CHANGE:
		MSG_DBG2("DVDNAV Event: Nav SPU CLUT Change\n");
		if(sp->len!=64) MSG_WARN("DVDNAV Event: Nav SPU CLUT Change: %i bytes <> 64\n",sp->len);
		// send new palette to SPU decoder
		if (vo_data->spudec) spudec_update_palette(vo_data->spudec,(const unsigned int *)(sp->buf));
		break;
    }
}

static void __FASTCALL__ dvdnav_cmd_handler(stream_t* s,unsigned cmd)
{
    dvdnav_priv_t *dvdnav_priv=s->priv;
    int button;
    pci_t *pci = dvdnav_get_current_nav_pci(dvdnav_priv->dvdnav);
    switch (cmd) {
        case MP_CMD_DVDNAV_UP:
          dvdnav_upper_button_select(dvdnav_priv->dvdnav,pci);
          break;
        case MP_CMD_DVDNAV_DOWN:
          dvdnav_lower_button_select(dvdnav_priv->dvdnav,pci);
          break;
        case MP_CMD_DVDNAV_LEFT:
          dvdnav_left_button_select(dvdnav_priv->dvdnav,pci);
          break;
        case MP_CMD_DVDNAV_RIGHT:
          dvdnav_right_button_select(dvdnav_priv->dvdnav,pci);
          break;
        case MP_CMD_DVDNAV_MENU: {
	    int title,part;
	    MSG_V("Menu call\n");
	    dvdnav_current_title_info(dvdnav_priv->dvdnav, &title, &part);
	    if(title>0) {
		if(dvdnav_menu_call(dvdnav_priv->dvdnav, DVD_MENU_Part) == DVDNAV_STATUS_OK
		|| dvdnav_menu_call(dvdnav_priv->dvdnav, DVD_MENU_Title) == DVDNAV_STATUS_OK)
			break;
	    }
	    dvdnav_menu_call(dvdnav_priv->dvdnav, DVD_MENU_Root);
	    dvdnav_button_select(dvdnav_priv->dvdnav, pci, 1);
        }
        break;
        case MP_CMD_DVDNAV_SELECT:
          dvdnav_button_activate(dvdnav_priv->dvdnav,pci);
          break;
        default:
          MSG_V("Weird DVD Nav cmd %d\n",cmd);
          break;
      }
    dvdnav_get_current_highlight(dvdnav_priv->dvdnav, &button);
    dvdnav_button_select(dvdnav_priv->dvdnav,pci,button);
}

static int __FASTCALL__ __dvdnav_ctrl(stream_t *s,unsigned cmd,any_t*args)
{
    dvdnav_priv_t *dvdnav_priv=s->priv;
    switch(cmd)
    {
	case SCTRL_TXT_GET_STREAM_NAME:
	{
	    const char *title_str;
	    if (dvdnav_get_title_string(dvdnav_priv->dvdnav,&title_str)==DVDNAV_STATUS_OK)
	    {
		strncpy(args,title_str,256);
		((char *)args)[255]=0;
		return SCTRL_OK;
	    }
	}
	break;
	case SCTRL_VID_GET_PALETTE:
	{
	    unsigned* pal;
	    pal=dvdnav_stream_get_palette(s);
	    *((unsigned **)args)=pal;
	    return SCTRL_OK;
	}
	break;
	case SCTRL_VID_GET_HILIGHT: {
	    mp_dvdnav_get_highlight (s,args);
	    return SCTRL_OK;
	}
	case SCRTL_EVT_HANDLE:
	{
	    dvdnav_event_handler(s,args);
	    return SCTRL_OK;
	}
	break;
	case SCRTL_MPXP_CMD:
	{
	    dvdnav_cmd_handler(s,(unsigned)args);
	    return SCTRL_OK;
	}
	default: break;
    }
    return SCTRL_FALSE;
}

const stream_driver_t dvdnav_stream=
{
    "dvdnav://",
    "reads multimedia stream with using of libdvdnav library",
    __dvdnav_open,
    __dvdnav_read,
    __dvdnav_seek,
    __dvdnav_tell,
    __dvdnav_close,
    __dvdnav_ctrl
};
#endif
