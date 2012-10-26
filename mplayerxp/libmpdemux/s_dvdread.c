/*
    s_dvdread - DVDREAD stream interface
*/

#include "mp_config.h"
#include "mplayer.h"
#ifdef USE_DVDREAD
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "stream.h"
#include "help_mp.h"
#include "demux_msg.h"


#include <dvdread/dvd_reader.h>
#include <dvdread/ifo_types.h>
#include <dvdread/ifo_read.h>
#include <dvdread/nav_read.h>
#include "mrl.h"

#undef DVDREAD_VERSION
#define	DVDREAD_VERSION(maj,min,micro)	((maj)*10000 + (min)*100 + (micro))

/*
 * Try to autodetect the libdvd-0.9.0 library
 * (0.9.0 removed the <dvdread/dvd_udf.h> header, and moved the two defines
 * DVD_VIDEO_LB_LEN and MAX_UDF_FILE_NAME_LEN from it to
 * <dvdread/dvd_reader.h>)
 */
#if defined(DVD_VIDEO_LB_LEN) && defined(MAX_UDF_FILE_NAME_LEN)
#define	LIBDVDREAD_VERSION	DVDREAD_VERSION(0,9,0)
#else
#define	LIBDVDREAD_VERSION	DVDREAD_VERSION(0,8,0)
#endif

typedef struct {
 int id; // 0 - 31 mpeg; 128 - 159 ac3; 160 - 191 pcm
 int language; 
 int type;
 int channels;
} stream_language_t;

typedef struct {
  dvd_reader_t *dvd;
  dvd_file_t *title;
  ifo_handle_t *vmg_file;
  tt_srpt_t *tt_srpt;
  ifo_handle_t *vts_file;
  vts_ptt_srpt_t *vts_ptt_srpt;
  pgc_t *cur_pgc;
  /* title sets */
  unsigned first_title,cur_title,last_title;
//
  int cur_cell;
  int last_cell;
  int cur_pack;
  int cell_last_pack;
// Navi:
  int packs_left;
  dsi_t dsi_pack;
  pci_t pci_pack;
  int angle_seek;
  float vobu_s_pts,vobu_e_pts;
// audio datas
  int nr_of_channels;
  stream_language_t audio_streams[32];
// subtitles
  int nr_of_subtitles;
  stream_language_t subtitles[32];
  
  off_t  spos;
} dvd_priv_t;

static int dvd_chapter=1;
static int dvd_last_chapter=0;
static int dvd_angle=1; /**< some DVD discs contain scenes that can be viewed from multiple angles */

static const char * dvd_audio_stream_types[8] =
        { "ac3","unknown","mpeg1","mpeg2ext","lpcm","unknown","dts" };

static const char * dvd_audio_stream_channels[8] =
	{ "unknown", "stereo", "unknown", "unknown", "unknown", "5.1", "6.1", "7.1" };

static int __FASTCALL__ dvd_chapter_from_cell(dvd_priv_t* dvd,int title,int cell)
{
  pgc_t * cur_pgc;
  ptt_info_t* ptt;
  int chapter = cell;
  int pgc_id,pgn;
  if(title < 0 || cell < 0){
    return 0;
  }
  /* for most DVD's chapter == cell */
  /* but there are more complecated cases... */
  if(chapter >= dvd->vmg_file->tt_srpt->title[title].nr_of_ptts){
    chapter = dvd->vmg_file->tt_srpt->title[title].nr_of_ptts-1;
  }
  title = dvd->tt_srpt->title[title].vts_ttn-1;
  ptt = dvd->vts_file->vts_ptt_srpt->title[title].ptt;
  while(chapter >= 0){
    pgc_id = ptt[chapter].pgcn;
    pgn = ptt[chapter].pgn;
    cur_pgc = dvd->vts_file->vts_pgcit->pgci_srp[pgc_id-1].pgc;
    if(cell >= cur_pgc->program_map[pgn-1]-1){
      return chapter;
    }
    --chapter;
  }
  /* didn't find a chapter ??? */
  return chapter;
}

static int __FASTCALL__ dvd_number_of_subs(stream_t *stream)
{
  dvd_priv_t *d;
  if (!stream) return -1;
  d = stream->priv;
  if (!d) return -1;
  return d->nr_of_subtitles;
}

static int __FASTCALL__ dvd_aid_from_lang(stream_t *stream, unsigned char* lang){
dvd_priv_t *d=stream->priv;
int code,i;
  while(lang && strlen(lang)>=2){
    code=lang[1]|(lang[0]<<8);
    for(i=0;i<d->nr_of_channels;i++){
	if(d->audio_streams[i].language==code){
	    MSG_V("Selected DVD audio channel: %d language: %c%c\n",
		d->audio_streams[i].id, lang[0],lang[1]);
	    return d->audio_streams[i].id;
	}
    }
    lang+=2; while (lang[0]==',' || lang[0]==' ') ++lang;
  }
  MSG_WARN("No matching DVD audio language found!\n");
  return -1;
}

static int __FASTCALL__ dvd_sid_from_lang(stream_t *stream, unsigned char* lang){
dvd_priv_t *d=stream->priv;
int code,i;
  while(lang && strlen(lang)>=2){
    code=lang[1]|(lang[0]<<8);
    for(i=0;i<d->nr_of_subtitles;i++){
	if(d->subtitles[i].language==code){
	    MSG_V("Selected DVD subtitle channel: %d language: %c%c\n",
		d->subtitles[i].id, lang[0],lang[1]);
	    return d->subtitles[i].id;
	}
    }
    lang+=2; while (lang[0]==',' || lang[0]==' ') ++lang;
  }
  MSG_WARN("No matching DVD subtitle language found!\n");
  return -1;
}

static int __FASTCALL__ dvd_lang_from_sid(stream_t *stream, int id)
{
  dvd_priv_t *d;
  if (!stream) return 0;
  d = stream->priv;
  if (!d) return 0;
  if (id >= d->nr_of_subtitles) return 0;
  return d->subtitles[id].language;
}

static int __FASTCALL__ dvd_next_title(dvd_priv_t *d,int dvd_title)
{
    int ttn,pgc_id,pgn;
    dvd_reader_t *dvd;
    dvd_file_t *title;
    ifo_handle_t *vmg_file;
    tt_srpt_t *tt_srpt;
    ifo_handle_t *vts_file;
    
    MSG_V("dvd_next_title %d\n",dvd_title);
    
    tt_srpt = d->tt_srpt;
    vmg_file = d->vmg_file;
    dvd = d->dvd;
    if(d->vts_file) ifoClose(d->vts_file);
    if(d->title)    DVDCloseFile(d->title);
    /**
     * Load the VTS information for the title set our title is in.
     */
    vts_file = ifoOpen( dvd, tt_srpt->title[dvd_title].title_set_nr );
    if( !vts_file ) {
	MSG_ERR( MSGTR_DVDnoIFO,
                 tt_srpt->title[dvd_title].title_set_nr );
        ifoClose( vmg_file );
        DVDClose( dvd );
        return 0;
    }
    /**
     * We've got enough info, time to open the title set data.
     */
    title = DVDOpenFile( dvd, tt_srpt->title[dvd_title].title_set_nr,
                         DVD_READ_TITLE_VOBS );
    if( !title ) {
	MSG_ERR( MSGTR_DVDnoVOBs,
                 tt_srpt->title[dvd_title].title_set_nr );
        ifoClose( vts_file );
        ifoClose( vmg_file );
        DVDClose( dvd );
        return 0;
    }
    d->vts_file=vts_file;
    d->title=title;

    ttn = tt_srpt->title[dvd_title].vts_ttn - 1;

    /**
     * Check number of audio channels and types
     */
    {
     const int ac3aid = 128;
     const int dtsaid = 136;
     const int mpegaid = 0;
     const int pcmaid = 160;
     
     d->nr_of_channels=0;
     
     if ( vts_file->vts_pgcit ) 
      {
       int i;
       for ( i=0;i<8;i++ )
        if ( vts_file->vts_pgcit->pgci_srp[0].pgc->audio_control[i] & 0x8000 )
	 {
	  audio_attr_t * audio = &vts_file->vtsi_mat->vts_audio_attr[i];
	  int language = 0;
	  char tmp[] = "unknown";
	  
	  if ( audio->lang_type == 1 ) 
	   {
	    language=audio->lang_code;
	    tmp[0]=language>>8;
	    tmp[1]=language&0xff;
	    tmp[2]=0;
	   }
	  
          d->audio_streams[d->nr_of_channels].language=language;
          d->audio_streams[d->nr_of_channels].id=vts_file->vts_pgcit->pgci_srp[ttn].pgc->audio_control[i] >> 8 & 7;
	  switch ( audio->audio_format )
	   {
	    case 0: // ac3
	  	    d->audio_streams[d->nr_of_channels].id+=ac3aid;
		    break;
	    case 6: // dts
	            d->audio_streams[d->nr_of_channels].id+=dtsaid;
		    break;
	    case 2: // mpeg layer 1/2/3
	    case 3: // mpeg2 ext
	            d->audio_streams[d->nr_of_channels].id+=mpegaid;
		    break;
	    case 4: // lpcm
	            d->audio_streams[d->nr_of_channels].id+=pcmaid;
		    break;
	   }

	  d->audio_streams[d->nr_of_channels].type=audio->audio_format;
	  // Pontscho: to my mind, tha channels:
	  //  1 - stereo
	  //  5 - 5.1
	  d->audio_streams[d->nr_of_channels].channels=audio->channels;
          MSG_V("[open] audio stream: %d audio format: %s (%s) language: %s aid: %d\n",
	    d->nr_of_channels,
            dvd_audio_stream_types[ audio->audio_format ],
	    dvd_audio_stream_channels[ audio->channels ],
	    tmp,
	    d->audio_streams[d->nr_of_channels].id
	    );
	    
	  d->nr_of_channels++;
	 }
      }
     MSG_V("[open] number of audio channels on disk: %d.\n",d->nr_of_channels );
    }

    /**
     * Check number of subtitles and language
     */
    {
     int i;

     d->nr_of_subtitles=0;
     for ( i=0;i<32;i++ )
      if ( vts_file->vts_pgcit->pgci_srp[0].pgc->subp_control[i] & 0x80000000 )
       {
        subp_attr_t * subtitle = &vts_file->vtsi_mat->vts_subp_attr[i];
        video_attr_t *video = &vts_file->vtsi_mat->vts_video_attr;
	int language = 0;
	char tmp[] = "unknown";
	
	if ( subtitle->type == 1 )
	 {
	  language=subtitle->lang_code;
	  tmp[0]=language>>8;
	  tmp[1]=language&0xff;
	  tmp[2]=0;
	 }
	 
	d->subtitles[ d->nr_of_subtitles ].language=language;
	d->subtitles[ d->nr_of_subtitles ].id=d->nr_of_subtitles;
        if(video->display_aspect_ratio == 0) /* 4:3 */
          d->subtitles[d->nr_of_subtitles].id = vts_file->vts_pgcit->pgci_srp[ttn].pgc->subp_control[i] >> 24 & 31;
        else if(video->display_aspect_ratio == 3) /* 16:9 */
          d->subtitles[d->nr_of_subtitles].id = vts_file->vts_pgcit->pgci_srp[ttn].pgc->subp_control[i] >> 8 & 31;
	
        MSG_V("[open] subtitle ( sid ): %d language: %s\n",
	  d->nr_of_subtitles,
	  tmp
	  );
        d->nr_of_subtitles++;
       }
     MSG_V("[open] number of subtitles on disk: %d\n",d->nr_of_subtitles );
    }

    /**
     * Determine which program chain we want to watch.  This is based on the
     * chapter number.
     */
    pgc_id = vts_file->vts_ptt_srpt->title[ttn].ptt[dvd_chapter].pgcn; // local
    pgn    = vts_file->vts_ptt_srpt->title[ttn].ptt[dvd_chapter].pgn;  // local
    d->cur_pgc = vts_file->vts_pgcit->pgci_srp[pgc_id-1].pgc;
    d->cur_cell = d->cur_pgc->program_map[pgn-1] - 1; // start playback here
    d->packs_left=-1;      // for Navi stuff
    d->angle_seek=0;
    /* XXX dvd_last_chapter is in the range 1..nr_of_ptts */
    if ( dvd_last_chapter > 0 && dvd_last_chapter < tt_srpt->title[dvd_title].nr_of_ptts ) {
	pgn=vts_file->vts_ptt_srpt->title[ttn].ptt[dvd_last_chapter].pgn;
	d->last_cell=d->cur_pgc->program_map[pgn-1] - 1;
    }
    else
	d->last_cell=d->cur_pgc->nr_of_cells;
    
    if( d->cur_pgc->cell_playback[d->cur_cell].block_type == BLOCK_TYPE_ANGLE_BLOCK ) d->cur_cell+=dvd_angle;
    d->cur_pack = d->cur_pgc->cell_playback[ d->cur_cell ].first_sector;
    d->cell_last_pack=d->cur_pgc->cell_playback[ d->cur_cell ].last_sector;
    MSG_V( "DVD start cell: %d  pack: 0x%X-0x%X  \n",d->cur_cell,d->cur_pack,d->cell_last_pack);

    return 1;
}

static int __FASTCALL__ dvd_next_cell(dvd_priv_t *d){
    int next_cell=d->cur_cell;

    MSG_V( "dvd_next_cell: next1=0x%X  \n",next_cell);
    
    if( d->cur_pgc->cell_playback[ next_cell ].block_type
                                        == BLOCK_TYPE_ANGLE_BLOCK ) {
	    while(next_cell<d->last_cell){
                if( d->cur_pgc->cell_playback[next_cell].block_mode
                                          == BLOCK_MODE_LAST_CELL ) break;
		++next_cell;
            }
    }
    MSG_V( "dvd_next_cell: next2=0x%X  \n",next_cell);
    
    ++next_cell;
    if(next_cell>=d->last_cell) return -1; // EOF
    if( d->cur_pgc->cell_playback[next_cell].block_type == BLOCK_TYPE_ANGLE_BLOCK ){
	next_cell+=dvd_angle;
	if(next_cell>=d->last_cell) return -1; // EOF
    }
    MSG_V( "dvd_next_cell: next3=0x%X  \n",next_cell);
    return next_cell;
}

static void __FASTCALL__ dvd_seek(stream_t* stream,dvd_priv_t *d,int pos);
static int __FASTCALL__ dvd_read_sector(stream_t* stream,dvd_priv_t *d,unsigned char* data){
    int len;
read_first:
    if(d->packs_left==0){
            /**
             * If we're not at the end of this cell, we can determine the next
             * VOBU to display using the VOBU_SRI information section of the
             * DSI.  Using this value correctly follows the current angle,
             * avoiding the doubled scenes in The Matrix, and makes our life
             * really happy.
             *
             * Otherwise, we set our next address past the end of this cell to
             * force the code above to go to the next cell in the program.
             */
            if( d->dsi_pack.vobu_sri.next_vobu != SRI_END_OF_CELL ) {
                d->cur_pack= d->dsi_pack.dsi_gi.nv_pck_lbn +
		( d->dsi_pack.vobu_sri.next_vobu & 0x3fffffff );
		MSG_V( "Navi  new pos=0x%X  \n",d->cur_pack);
            } else {
		// end of cell! find next cell!
		MSG_V( "--- END OF CELL !!! ---\n");
		d->cur_pack=d->cell_last_pack+1;
            }
    }

read_next:

    if(d->cur_pack>d->cell_last_pack){
	// end of cell!
	int next=dvd_next_cell(d);
	if(next>=0){
	    d->cur_cell=next;
	    //    if( d->cur_pgc->cell_playback[d->cur_cell].block_type 
	    //	== BLOCK_TYPE_ANGLE_BLOCK ) d->cur_cell+=dvd_angle;
	    d->cur_pack = d->cur_pgc->cell_playback[ d->cur_cell ].first_sector;
	    d->cell_last_pack=d->cur_pgc->cell_playback[ d->cur_cell ].last_sector;
	    MSG_V( "DVD next cell: %d  pack: 0x%X-0x%X  \n",d->cur_cell,d->cur_pack,d->cell_last_pack);
	} else
	if(d->cur_title<d->last_title)
	{
	    d->cur_title++;
	    dvd_next_title(d,d->cur_title);
	    goto read_first;
	}
	else return -1; // EOF
    }

    len = DVDReadBlocks( d->title, d->cur_pack, 1, data );
    if(!len) return -1; //error
/*
Navigation packets are PES packets with a stream id 0xbf, i.e.
private stream 2.  It's made up of PCI, Presentation Control Information
and DSI, Data Search Information.

   0      6         7                  0x3d4+0x6 0x3d4+0xb 0x3d4+0xc
   +------+---------+----------------------+------+---------+-----------------+
   |Packet|Substream|        PCI           |Packet|Substream|       DSI       |
   |Header|  ID     |        Data          |Header|   ID    |       Data      |
   +------+---------+----------------------+------+---------+-----------------+

  The packet head is just a PES packet header, a packet start code, a stream id
and a packet length.  The first packet lenght is 0x3d4, the length of the PCI data
plus one.  The second packet length is 0x3fa, the length of DSI data plus one.
The substream id for PCI packet is 0x00 and 0x01 for DSI.
*/
    if(data[38]==0 && data[39]==0 && data[40]==1 && data[41]==0xBF &&
       data[1024]==0 && data[1025]==0 && data[1026]==1 && data[1027]==0xBF){
	// found a Navi packet!!!
#if LIBDVDREAD_VERSION >= DVDREAD_VERSION(0,9,0)
        navRead_DSI( &d->dsi_pack, &(data[DSI_START_BYTE]) );
        navRead_PCI( &d->pci_pack, &(data[DSI_START_BYTE-0x3DA]) );
#else
        navRead_DSI( &d->dsi_pack, &(data[DSI_START_BYTE]), sizeof(dsi_t));
        navRead_PCI( &d->pci_pack, &(data[DSI_START_BYTE-0x3DA]), sizeof(pci_t));
#endif
	/*
	    TODO!: if num_angles!=0 and d->dsi_pack.sml_agli.data[dvd_angle].address
	    doesn't work :(
	    > Invalid NAVIpacket! lba=30 gsi_navi=8002F pci_navi=8002F angle=0
	*/
	if(d->cur_pack != d->dsi_pack.dsi_gi.nv_pck_lbn){
	    MSG_V( "Invalid NAVIpacket! lba=%X gsi_navi=%X pci_navi=%X angle=%X\n"
		,d->cur_pack
		,d->dsi_pack.dsi_gi.nv_pck_lbn
		,d->pci_pack.pci_gi.nv_pck_lbn
		,d->dsi_pack.sml_agli.data[dvd_angle].address
		);
	} else {
	    // process!
	    float vobu_s_pts,vobu_e_pts;
	    vobu_s_pts=d->pci_pack.pci_gi.vobu_s_ptm/90000.;
	    vobu_e_pts=d->pci_pack.pci_gi.vobu_e_ptm/90000.;
    	    d->packs_left = d->dsi_pack.dsi_gi.vobu_ea;
	    MSG_V( "Found NAVI packet! lba=%X angle=%X len=%d vobu_s_pts=%f vobu_e_pts=%f\n"
	    ,d->cur_pack,d->packs_left
	    ,d->dsi_pack.sml_agli.data[dvd_angle].address
	    ,vobu_s_pts,vobu_e_pts);

	    if(d->angle_seek){
		int i,skip=0;
#if defined(__GNUC__) && ( defined(__sparc__) || defined(hpux) )
		// workaround for a bug in the sparc/hpux version of gcc 2.95.X ... 3.2,
		// it generates incorrect code for unaligned access to a packed
		// structure member, resulting in an mplayer crash with a SIGBUS
		// signal.
		//
		// See also gcc problem report PR c/7847:
		// http://gcc.gnu.org/cgi-bin/gnatsweb.pl?database=gcc&cmd=view+audit-trail&pr=7847
		for(i=0;i<9;i++){	// check if all values zero:
		    typeof(d->dsi_pack.sml_agli.data[i].address) tmp_addr;
		    memcpy(&tmp_addr,&d->dsi_pack.sml_agli.data[i].address,sizeof(tmp_addr));
		    if((skip=tmp_addr)!=0) break;
		}
#else
		for(i=0;i<9;i++)	// check if all values zero:
		    if((skip=d->dsi_pack.sml_agli.data[i].address)!=0) break;
#endif
		if(skip){
		    // sml_agli table has valid data (at least one non-zero):
		    d->cur_pack=d->dsi_pack.dsi_gi.nv_pck_lbn+
				d->dsi_pack.sml_agli.data[dvd_angle].address;
		    d->angle_seek=0;
		    MSG_V("Angle-seek synced using sml_agli map!  new_lba=0x%X\n",d->cur_pack);
		} else {
		    // check if we're in the right cell, jump otherwise:
		    if( (d->dsi_pack.dsi_gi.vobu_c_idn==d->cur_pgc->cell_position[d->cur_cell].cell_nr) &&
		        (d->dsi_pack.dsi_gi.vobu_vob_idn==d->cur_pgc->cell_position[d->cur_cell].vob_id_nr) ){
			d->angle_seek=0;
			MSG_V("Angle-seek synced by cell/vob IDN search!\n");
		    } else {
			// wrong angle, skip this vobu:
			d->cur_pack=d->dsi_pack.dsi_gi.nv_pck_lbn+
				    d->dsi_pack.dsi_gi.vobu_ea;
			d->angle_seek=2; // DEBUG
		    }
		}
	    }
	    if(vobu_s_pts < d->vobu_e_pts)
	    {
		stream->stream_pts += d->vobu_e_pts-vobu_s_pts;
		MSG_V("DVD's discontinuities found! Applying delta: %f\n",stream->stream_pts);
	    }
	    else stream->stream_pts = vobu_s_pts;
	    d->vobu_s_pts = vobu_s_pts;
	    d->vobu_e_pts = vobu_e_pts;
	}
	++d->cur_pack;
	goto read_next;
    }

    ++d->cur_pack;
    if(d->packs_left>=0) --d->packs_left;
    
    if(d->angle_seek) goto read_next; // searching for Navi packet

    return d->cur_pack-1;
}

static void __FASTCALL__ dvd_seek(stream_t* stream,dvd_priv_t *d,int pos){
    int dir=1;
    d->packs_left=-1;
    if(d->cur_pack>pos) dir=-1;
    d->cur_pack=pos;
    
    reseek:
// check if we stay in current cell (speedup things, and avoid angle skip)
if(d->cur_pack>d->cell_last_pack ||
   d->cur_pack<d->cur_pgc->cell_playback[ d->cur_cell ].first_sector){

    // ok, cell change, find the right cell!
    d->cur_cell=0;
    if( d->cur_pgc->cell_playback[d->cur_cell].block_type 
	== BLOCK_TYPE_ANGLE_BLOCK ) d->cur_cell+=dvd_angle;

  while(1){
    int next;
    d->cell_last_pack=d->cur_pgc->cell_playback[ d->cur_cell ].last_sector;
    if(d->cur_pack<d->cur_pgc->cell_playback[ d->cur_cell ].first_sector){
	d->cur_pack=d->cur_pgc->cell_playback[ d->cur_cell ].first_sector;
	break;
    }
    if(d->cur_pack<=d->cell_last_pack) break; // ok, we find it! :)
    next=dvd_next_cell(d);
    if(next<0){
	// we're after the last cell
	if(dir>0) // FF
	{
	    if(d->cur_title<d->last_title)
	    {
		d->cur_title++;
		dvd_next_title(d,d->cur_title);
		goto reseek;
	    }
	}
	else // BACK
	{
	    if(d->cur_title>d->first_title)
	    {
		d->cur_title--;
		dvd_next_title(d,d->cur_title);
		goto reseek;
	    }
	}
	break; // EOF
    }
    d->cur_cell=next;
  }
  stream->stream_pts=d->vobu_s_pts=d->vobu_e_pts=0.;
}

MSG_V( "DVD Seek! lba=0x%X  cell=%d  packs: 0x%X-0x%X  \n",
    d->cur_pack,d->cur_cell,d->cur_pgc->cell_playback[ d->cur_cell ].first_sector,d->cell_last_pack);

// if we're in interleaved multi-angle cell, find the right angle chain!
// (read Navi block, and use the seamless angle jump table)
d->angle_seek=1;

}

static void __FASTCALL__ dvd_close(dvd_priv_t *d) {
  ifoClose(d->vts_file);
  ifoClose(d->vmg_file);
  DVDCloseFile(d->title);
  DVDClose(d->dvd);
  /* for reenterability */
  dvd_chapter=1;
  dvd_last_chapter=0;
  dvd_angle=1;
}

static int __FASTCALL__ __dvdread_open(stream_t *stream,const char *filename,unsigned flags)
{
    int dvd_title,last_title=-1;
    const char *args;
    char *dvd_device,*tilde,*comma,*par;
    char param[256];
//  int ret,ret2;
    dvd_priv_t *d;
    dvd_reader_t *dvd;
    ifo_handle_t *vmg_file;
    tt_srpt_t *tt_srpt;

    if(strcmp(filename,"help") == 0 || strlen(filename)==10)
    {
	MSG_HINT("Usage: dvdread://<@device>#<titleno>-<lasttitle>,<chapter>-<lastchapter>,<angle>\n");
	return 0;
    }
    args=mrl_parse_line(filename,NULL,NULL,&dvd_device,NULL);
    strncpy(param,args,sizeof(param));
    comma=strchr(param,',');
    tilde=strchr(param,'-');
    if(comma) *comma=0;
    if(tilde) *tilde=0;
    dvd_title=atoi(param);
    if(tilde) last_title=atoi(tilde+1);
    else
    if(comma)
    {
	comma++;
	par = comma;
	tilde=strchr(comma,'-');
	comma=strchr(comma,',');
	dvd_chapter=atoi(par);
	if(tilde)
	{
	    tilde++;
	    dvd_last_chapter=atoi(tilde);
	}
	if(comma) { comma++; dvd_angle=atoi(comma); }
    }
    /**
     * Open the disc.
     */
    dvd = DVDOpen(dvd_device?dvd_device:DEFAULT_DVD_DEVICE);
    if( !dvd ) {
        MSG_ERR(MSGTR_CantOpenDVD,dvd_device?dvd_device:DEFAULT_DVD_DEVICE);
	if(dvd_device) free(dvd_device);
        return 0;
    }
    if(dvd_device) free(dvd_device);
    MSG_V(MSGTR_DVDwait);

    /**
     * Load the video manager to find out the information about the titles on
     * this disc.
     */
    vmg_file = ifoOpen( dvd, 0 );
    if( !vmg_file ) {
        MSG_ERR( "Can't open VMG info!\n");
        DVDClose( dvd );
        return 0;
    }
    tt_srpt = vmg_file->tt_srpt;
    /**
     * Make sure our title number is valid.
     */
    MSG_INFO( MSGTR_DVDnumTitles,
             tt_srpt->nr_of_srpts );
    if( dvd_title < 1 || dvd_title > tt_srpt->nr_of_srpts ) {
	MSG_ERR( MSGTR_DVDinvalidTitle, dvd_title);
        ifoClose( vmg_file );
        DVDClose( dvd );
        return 0;
    }
    --dvd_title; // remap 1.. -> 0..
    --last_title;// remap 1.. -> 0..
    if( last_title < 0 || last_title >= tt_srpt->nr_of_srpts ) last_title=dvd_title;
    /**
     * Make sure the chapter number is valid for this title.
     */
    MSG_INFO( MSGTR_DVDnumChapters,
             tt_srpt->title[dvd_title].nr_of_ptts );
    if( dvd_chapter<1 || dvd_chapter>tt_srpt->title[dvd_title].nr_of_ptts ) {
	MSG_ERR( MSGTR_DVDinvalidChapter, dvd_chapter);
        ifoClose( vmg_file );
        DVDClose( dvd );
        return 0;
    }
    if( dvd_last_chapter>0 ) {
	if ( dvd_last_chapter<dvd_chapter || dvd_last_chapter>tt_srpt->title[dvd_title].nr_of_ptts ) {
	    MSG_ERR( "Invalid DVD last chapter number: %d\n", dvd_last_chapter);
	    ifoClose( vmg_file );
	    DVDClose( dvd );
	    return 0;
	}
    }
    --dvd_chapter; // remap 1.. -> 0..
    /* XXX No need to remap dvd_last_chapter */
    /**
     * Make sure the angle number is valid for this title.
     */
    MSG_V( MSGTR_DVDnumAngles,
             tt_srpt->title[dvd_title].nr_of_angles );
    if( dvd_angle<1 || dvd_angle>tt_srpt->title[dvd_title].nr_of_angles ) {
	MSG_ERR( MSGTR_DVDinvalidAngle, dvd_angle);
        ifoClose( vmg_file );
        DVDClose( dvd );
        return 0;
    }
    --dvd_angle; // remap 1.. -> 0..
    
    // store data
    d=malloc(sizeof(dvd_priv_t)); memset(d,0,sizeof(dvd_priv_t));
    d->dvd=dvd;
    d->title=0;
    d->vmg_file=vmg_file;
    d->tt_srpt=tt_srpt;
    d->vts_file=0;
    d->cur_title=d->first_title=dvd_title;
    d->last_title=last_title;

    if(!dvd_next_title(d,dvd_title))
    {
	free(d);
        ifoClose( vmg_file );
        DVDClose( dvd );
	return 0;
    }

    MSG_V( MSGTR_DVDopenOk);

  stream->start_pos=(off_t)d->cur_pack*2048;
  stream->end_pos=(off_t)(d->cur_pgc->cell_playback[d->last_cell-1].last_sector)*2048;
  if(dvd_next_title(d,last_title))
    stream->end_pos=(off_t)(d->cur_pgc->cell_playback[d->last_cell-1].last_sector)*2048;
  MSG_V("DVD start=%d end=%d  \n",d->cur_pack,d->cur_pgc->cell_playback[d->last_cell-1].last_sector);
  dvd_next_title(d,dvd_title);
  stream->priv=(any_t*)d;
  stream->type = STREAMTYPE_SEEKABLE|STREAMTYPE_PROGRAM;
  stream->sector_size=2048;
  d->spos=0;
  return 1;
}

static int __FASTCALL__ __dvdread_read(stream_t *stream,stream_packet_t *sp)
{
    dvd_priv_t *d=(dvd_priv_t *)stream->priv;
    off_t pos=dvd_read_sector(stream,stream->priv,sp->buf);
    sp->type=0;
    if(pos>=0){
	sp->len=2048; // full sector
	d->spos += 2048;
    } else sp->len= -1; // error
    return sp->len;
}

static off_t __FASTCALL__ __dvdread_seek(stream_t *stream,off_t newpos)
{
    dvd_priv_t *d=(dvd_priv_t *)stream->priv;
    off_t pos=newpos/2048;
    dvd_seek(stream,stream->priv,pos);
    d->spos=pos*2048;
    return d->spos;
}

static off_t __FASTCALL__ __dvdread_tell(stream_t *stream)
{
    dvd_priv_t *d = (dvd_priv_t *)stream->priv;
    return d->spos;
}

static void __FASTCALL__ __dvdread_close(stream_t *stream)
{
    dvd_close(stream->priv);
    free(stream->priv);
}

static unsigned int * __FASTCALL__ dvdread_stream_get_palette(stream_t *stream)
{
  dvd_priv_t *d=(dvd_priv_t *)stream->priv;
  if(d)
    if(d->cur_pgc)
	return d->cur_pgc->palette;
  return 0;
}

static int __FASTCALL__ __dvdread_ctrl(stream_t *s,unsigned cmd,any_t*args)
{
    dvd_priv_t *dvd_priv=s->priv;
    switch(cmd)
    {
	case SCTRL_VID_GET_PALETTE:
	{
	    unsigned* pal;
	    pal=dvdread_stream_get_palette(s);
	    *((unsigned **)args)=pal;
	    return SCTRL_OK;
	}
	break;
	case SCTRL_LNG_GET_AID:
	{
	    int aid;
	    aid=dvd_aid_from_lang(s,args);
	    *((int *)args)=aid;
	    return SCTRL_OK;
	}
	break;
	case SCTRL_LNG_GET_SID:
	{
	    int aid;
	    aid=dvd_sid_from_lang(s,args);
	    *((int *)args)=aid;
	    return SCTRL_OK;
	}
	break;
	default: break;
    }
    return SCTRL_FALSE;
}

const stream_driver_t dvdread_stream =
{
    "dvdread://",
    "reads multimedia stream using low-level libdvdread access",
    __dvdread_open,
    __dvdread_read,
    __dvdread_seek,
    __dvdread_tell,
    __dvdread_close,
    __dvdread_ctrl
};
#endif

