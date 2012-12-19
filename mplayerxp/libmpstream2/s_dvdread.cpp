#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
/*
    s_dvdread - DVDREAD stream interface
*/
#include <limits>

#include "mplayerxp.h"
/* fake stdint */
#define UINT8_MAX std::numeric_limits<uint8_t>::max()
#define UINT16_MAX std::numeric_limits<uint16_t>::max()
#define INT32_MAX std::numeric_limits<int32_t>::max()
#ifdef USE_DVDREAD
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "stream.h"
#include "stream_internal.h"
#include "mpxp_help.h"
#include "stream_msg.h"

#include <dvdread/dvd_reader.h>
#include <dvdread/ifo_types.h>
#include <dvdread/ifo_read.h>
#include <dvdread/nav_read.h>
#include "mrl.h"

namespace mpxp {
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

    struct stream_language_t {
	int id; // 0 - 31 mpeg; 128 - 159 ac3; 160 - 191 pcm
	int language;
	int type;
	int channels;
    };

    class DvdRead_Stream_Interface : public Stream_Interface {
	public:
	    DvdRead_Stream_Interface(libinput_t& libinput);
	    virtual ~DvdRead_Stream_Interface();

	    virtual MPXP_Rc	open(const std::string& filename,unsigned flags);
	    virtual int		read(stream_packet_t * sp);
	    virtual off_t	seek(off_t off);
	    virtual off_t	tell() const;
	    virtual void	close();
	    virtual MPXP_Rc	ctrl(unsigned cmd,any_t* param);
	    virtual Stream::type_e type() const;
	    virtual off_t	start_pos() const;
	    virtual off_t	size() const;
	    virtual off_t	sector_size() const;
	    virtual float	stream_pts() const;
	    virtual std::string mime_type() const;
	private:
	    int			chapter_from_cell(int title,int cell);
	    int			number_of_subs() const;
	    int			aid_from_lang(const std::string& lang) const;
	    int			sid_from_lang(const std::string& lang) const;
	    int			lang_from_sid(int id) const;
	    int			next_title(int dvd_title);
	    int			next_cell();
	    int			read_sector(unsigned char* data);
	    void		dvd_seek(int pos);
	    unsigned*		get_palette() const;

	    dvd_reader_t*	dvd;
	    dvd_file_t*		title;
	    ifo_handle_t*	vmg_file;
	    tt_srpt_t*		tt_srpt;
	    ifo_handle_t*	vts_file;
	    vts_ptt_srpt_t*	vts_ptt_srpt;
	    pgc_t*		cur_pgc;
	    /* title sets */
	    unsigned		first_title,cur_title,last_title;
	    //
	    int			cur_cell;
	    int			last_cell;
	    int			cur_pack;
	    int			cell_last_pack;
	    // Navi:
	    int			packs_left;
	    dsi_t		dsi_pack;
	    pci_t		pci_pack;
	    int			angle_seek;
	    float		vobu_s_pts,vobu_e_pts;
	    // audio datas
	    int			nr_of_channels;
	    stream_language_t	audio_streams[32];
	    // subtitles
	    int			nr_of_subtitles;
	    stream_language_t	subtitles[32];
	    off_t		spos;
	    int			dvd_chapter;
	    int			dvd_last_chapter;
	    int			dvd_angle; /**< some DVD discs contain scenes that can be viewed from multiple angles */
	    float		_stream_pts;
	    off_t		_end_pos;
    };

DvdRead_Stream_Interface::DvdRead_Stream_Interface(libinput_t& libinput)
			:Stream_Interface(libinput),
			dvd_chapter(1),dvd_angle(1) {}
DvdRead_Stream_Interface::~DvdRead_Stream_Interface() {}

static const char * dvd_audio_stream_types[8] =
	{ "ac3","unknown","mpeg1","mpeg2ext","lpcm","unknown","dts" };

static const char * dvd_audio_stream_channels[8] =
	{ "unknown", "stereo", "unknown", "unknown", "unknown", "5.1", "6.1", "7.1" };

int DvdRead_Stream_Interface::chapter_from_cell(int _title,int cell)
{
    ptt_info_t* ptt;
    int chapter = cell;
    int pgc_id,pgn;
    if(_title < 0 || cell < 0){
	return 0;
    }
    /* for most DVD's chapter == cell */
    /* but there are more complecated cases... */
    if(chapter >= vmg_file->tt_srpt->title[_title].nr_of_ptts){
	chapter = vmg_file->tt_srpt->title[_title].nr_of_ptts-1;
    }
    _title = tt_srpt->title[_title].vts_ttn-1;
    ptt = vts_file->vts_ptt_srpt->title[_title].ptt;
    while(chapter >= 0){
	pgc_id = ptt[chapter].pgcn;
	pgn = ptt[chapter].pgn;
	cur_pgc = vts_file->vts_pgcit->pgci_srp[pgc_id-1].pgc;
	if(cell >= cur_pgc->program_map[pgn-1]-1){
	    return chapter;
	}
	--chapter;
    }
    /* didn't find a chapter ??? */
    return chapter;
}

int DvdRead_Stream_Interface::number_of_subs() const { return nr_of_subtitles; }

int DvdRead_Stream_Interface::aid_from_lang(const std::string& _lang) const {
    int code;
    unsigned i;
    const char* lang=_lang.c_str();
    while(lang && strlen(lang)>=2){
	code=lang[1]|(lang[0]<<8);
	for(i=0;i<unsigned(nr_of_channels);i++){
	    if(audio_streams[i].language==code){
		MSG_V("Selected DVD audio channel: %d language: %c%c\n",
			audio_streams[i].id, lang[0],lang[1]);
		return audio_streams[i].id;
	    }
	}
	lang+=2; while (lang[0]==',' || lang[0]==' ') ++lang;
    }
    MSG_WARN("No matching DVD audio language found!\n");
    return -1;
}

int DvdRead_Stream_Interface::sid_from_lang(const std::string& _lang) const {
    int code;
    unsigned i;
    const char* lang=_lang.c_str();
    while(lang && strlen(lang)>=2){
	code=lang[1]|(lang[0]<<8);
	for(i=0;i<unsigned(nr_of_subtitles);i++){
	    if(subtitles[i].language==code){
		MSG_V("Selected DVD subtitle channel: %d language: %c%c\n",
			subtitles[i].id, lang[0],lang[1]);
		return subtitles[i].id;
	    }
	}
	lang+=2; while (lang[0]==',' || lang[0]==' ') ++lang;
    }
    MSG_WARN("No matching DVD subtitle language found!\n");
    return -1;
}

int DvdRead_Stream_Interface::lang_from_sid(int id) const
{
    if (id >= nr_of_subtitles) return 0;
    return subtitles[id].language;
}

int DvdRead_Stream_Interface::next_title(int dvd_title)
{
    int ttn,pgc_id,pgn;

    MSG_V("dvd_next_title %d\n",dvd_title);

    if(vts_file) ifoClose(vts_file);
    if(title)    DVDCloseFile(title);
    /**
     * Load the VTS information for the title set our title is in.
     */
    vts_file = ifoOpen(dvd, tt_srpt->title[dvd_title].title_set_nr );
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
    vts_file=vts_file;
    title=title;

    ttn = tt_srpt->title[dvd_title].vts_ttn - 1;

    /**
     * Check number of audio channels and types
     */
    const int ac3aid = 128;
    const int dtsaid = 136;
    const int mpegaid = 0;
    const int pcmaid = 160;

    nr_of_channels=0;

    if ( vts_file->vts_pgcit ) {
	int i;
	for ( i=0;i<8;i++ ) {
	    if ( vts_file->vts_pgcit->pgci_srp[0].pgc->audio_control[i] & 0x8000 ) {
		audio_attr_t * audio = &vts_file->vtsi_mat->vts_audio_attr[i];
		int language = 0;
		char tmp[] = "unknown";

		if ( audio->lang_type == 1 ) {
		    language=audio->lang_code;
		    tmp[0]=language>>8;
		    tmp[1]=language&0xff;
		    tmp[2]=0;
		}

		audio_streams[nr_of_channels].language=language;
		audio_streams[nr_of_channels].id=vts_file->vts_pgcit->pgci_srp[ttn].pgc->audio_control[i] >> 8 & 7;
		switch ( audio->audio_format ) {
		    case 0: // ac3
			audio_streams[nr_of_channels].id+=ac3aid;
			break;
		    case 6: // dts
			audio_streams[nr_of_channels].id+=dtsaid;
			break;
		    case 2: // mpeg layer 1/2/3
		    case 3: // mpeg2 ext
			audio_streams[nr_of_channels].id+=mpegaid;
			break;
		    case 4: // lpcm
			audio_streams[nr_of_channels].id+=pcmaid;
			break;
		}

		audio_streams[nr_of_channels].type=audio->audio_format;
		// Pontscho: to my mind, tha channels:
		//  1 - stereo
		//  5 - 5.1
		audio_streams[nr_of_channels].channels=audio->channels;
		MSG_V("[open] audio stream: %d audio format: %s (%s) language: %s aid: %d\n",
		    nr_of_channels,
		    dvd_audio_stream_types[ audio->audio_format ],
		    dvd_audio_stream_channels[ audio->channels ],
		    tmp,
		    audio_streams[nr_of_channels].id
		);
		nr_of_channels++;
	    }
	}
	MSG_V("[open] number of audio channels on disk: %d.\n",nr_of_channels );
    }

    /**
     * Check number of subtitles and language
     */
    int i;

    nr_of_subtitles=0;
    for ( i=0;i<32;i++ ) {
	if ( vts_file->vts_pgcit->pgci_srp[0].pgc->subp_control[i] & 0x80000000 ) {
	    subp_attr_t * subtitle = &vts_file->vtsi_mat->vts_subp_attr[i];
	    video_attr_t *video = &vts_file->vtsi_mat->vts_video_attr;
	    int language = 0;
	    char tmp[] = "unknown";

	    if ( subtitle->type == 1 ) {
		language=subtitle->lang_code;
		tmp[0]=language>>8;
		tmp[1]=language&0xff;
		tmp[2]=0;
	    }

	    subtitles[ nr_of_subtitles ].language=language;
	    subtitles[ nr_of_subtitles ].id=nr_of_subtitles;
	    if(video->display_aspect_ratio == 0) /* 4:3 */
		subtitles[nr_of_subtitles].id = vts_file->vts_pgcit->pgci_srp[ttn].pgc->subp_control[i] >> 24 & 31;
	    else if(video->display_aspect_ratio == 3) /* 16:9 */
		subtitles[nr_of_subtitles].id = vts_file->vts_pgcit->pgci_srp[ttn].pgc->subp_control[i] >> 8 & 31;

	    MSG_V("[open] subtitle ( sid ): %d language: %s\n",
		nr_of_subtitles,
		tmp );
	    nr_of_subtitles++;
	}
    }
    MSG_V("[open] number of subtitles on disk: %d\n",nr_of_subtitles );
    /**
     * Determine which program chain we want to watch.  This is based on the
     * chapter number.
     */
    pgc_id = vts_file->vts_ptt_srpt->title[ttn].ptt[dvd_chapter].pgcn; // local
    pgn    = vts_file->vts_ptt_srpt->title[ttn].ptt[dvd_chapter].pgn;  // local
    cur_pgc = vts_file->vts_pgcit->pgci_srp[pgc_id-1].pgc;
    cur_cell = cur_pgc->program_map[pgn-1] - 1; // start playback here
    packs_left=-1;      // for Navi stuff
    angle_seek=0;
    /* XXX dvd_last_chapter is in the range 1..nr_of_ptts */
    if ( dvd_last_chapter > 0 && dvd_last_chapter < tt_srpt->title[dvd_title].nr_of_ptts ) {
	pgn=vts_file->vts_ptt_srpt->title[ttn].ptt[dvd_last_chapter].pgn;
	last_cell=cur_pgc->program_map[pgn-1] - 1;
    }
    else
	last_cell=cur_pgc->nr_of_cells;

    if( cur_pgc->cell_playback[cur_cell].block_type == BLOCK_TYPE_ANGLE_BLOCK ) cur_cell+=dvd_angle;
    cur_pack = cur_pgc->cell_playback[ cur_cell ].first_sector;
    cell_last_pack=cur_pgc->cell_playback[ cur_cell ].last_sector;
    MSG_V( "DVD start cell: %d  pack: 0x%X-0x%X  \n",cur_cell,cur_pack,cell_last_pack);
    return 1;
}

int DvdRead_Stream_Interface::next_cell(){
    int _next_cell=cur_cell;

    MSG_V( "dvd_next_cell: next1=0x%X  \n",_next_cell);

    if( cur_pgc->cell_playback[ _next_cell ].block_type == BLOCK_TYPE_ANGLE_BLOCK ) {
	while(_next_cell<last_cell) {
	    if( cur_pgc->cell_playback[_next_cell].block_mode == BLOCK_MODE_LAST_CELL ) break;
	    ++_next_cell;
	}
    }
    MSG_V( "dvd_next_cell: next2=0x%X  \n",_next_cell);

    ++_next_cell;
    if(_next_cell>=last_cell) return -1; // EOF
    if( cur_pgc->cell_playback[_next_cell].block_type == BLOCK_TYPE_ANGLE_BLOCK ) {
	_next_cell+=dvd_angle;
	if(_next_cell>=last_cell) return -1; // EOF
    }
    MSG_V( "dvd_next_cell: next3=0x%X  \n",_next_cell);
    return _next_cell;
}

int DvdRead_Stream_Interface::read_sector(unsigned char* data){
    int len;
read_first:
    if(packs_left==0){
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
	if( dsi_pack.vobu_sri.next_vobu != SRI_END_OF_CELL ) {
	    cur_pack= dsi_pack.dsi_gi.nv_pck_lbn +
	    ( dsi_pack.vobu_sri.next_vobu & 0x3fffffff );
	    MSG_V( "Navi  new pos=0x%X  \n",cur_pack);
	} else {
	    // end of cell! find next cell!
	    MSG_V( "--- END OF CELL !!! ---\n");
	    cur_pack=cell_last_pack+1;
	}
    }
read_next:
    if(cur_pack>cell_last_pack){
	// end of cell!
	int next=next_cell();
	if(next>=0){
	    cur_cell=next;
	    //    if( cur_pgc->cell_playback[cur_cell].block_type
	    //	== BLOCK_TYPE_ANGLE_BLOCK ) cur_cell+=dvd_angle;
	    cur_pack = cur_pgc->cell_playback[ cur_cell ].first_sector;
	    cell_last_pack=cur_pgc->cell_playback[ cur_cell ].last_sector;
	    MSG_V( "DVD next cell: %d  pack: 0x%X-0x%X  \n",cur_cell,cur_pack,cell_last_pack);
	} else if(cur_title<last_title) {
	    cur_title++;
	    next_title(cur_title);
	    goto read_first;
	}
	else return -1; // EOF
    }

    len = DVDReadBlocks( title, cur_pack, 1, data );
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
	navRead_DSI( &dsi_pack, &(data[DSI_START_BYTE]) );
	navRead_PCI( &pci_pack, &(data[DSI_START_BYTE-0x3DA]) );
#else
	navRead_DSI( &dsi_pack, &(data[DSI_START_BYTE]), sizeof(dsi_t));
	navRead_PCI( &pci_pack, &(data[DSI_START_BYTE-0x3DA]), sizeof(pci_t));
#endif
	/*
	    TODO!: if num_angles!=0 and d->dsi_pack.sml_agli.data[dvd_angle].address
	    doesn't work :(
	    > Invalid NAVIpacket! lba=30 gsi_navi=8002F pci_navi=8002F angle=0
	*/
	if(cur_pack != dsi_pack.dsi_gi.nv_pck_lbn){
	    MSG_V( "Invalid NAVIpacket! lba=%X gsi_navi=%X pci_navi=%X angle=%X\n"
		,cur_pack
		,dsi_pack.dsi_gi.nv_pck_lbn
		,pci_pack.pci_gi.nv_pck_lbn
		,dsi_pack.sml_agli.data[dvd_angle].address
		);
	} else {
	    // process!
	    vobu_s_pts=pci_pack.pci_gi.vobu_s_ptm/90000.;
	    vobu_e_pts=pci_pack.pci_gi.vobu_e_ptm/90000.;
	    packs_left = dsi_pack.dsi_gi.vobu_ea;
	    MSG_V( "Found NAVI packet! lba=%X angle=%X len=%d vobu_s_pts=%f vobu_e_pts=%f\n"
		,cur_pack,packs_left
		,dsi_pack.sml_agli.data[dvd_angle].address
		,vobu_s_pts,vobu_e_pts);

	    if(angle_seek){
		int skip=0;
		unsigned i;
#if defined(__GNUC__) && ( defined(__sparc__) || defined(hpux) )
		// workaround for a bug in the sparc/hpux version of gcc 2.95.X ... 3.2,
		// it generates incorrect code for unaligned access to a packed
		// structure member, resulting in an mplayer crash with a SIGBUS
		// signal.
		//
		// See also gcc problem report PR c/7847:
		// http://gcc.gnu.org/cgi-bin/gnatsweb.pl?database=gcc&cmd=view+audit-trail&pr=7847
		for(i=0;i<9;i++){	// check if all values zero:
		    typeof(dsi_pack.sml_agli.data[i].address) tmp_addr;
		    memcpy(&tmp_addr,&dsi_pack.sml_agli.data[i].address,sizeof(tmp_addr));
		    if((skip=tmp_addr)!=0) break;
		}
#else
		for(i=0;i<9;i++)	// check if all values zero:
		    if((skip=dsi_pack.sml_agli.data[i].address)!=0) break;
#endif
		if(skip){
		    // sml_agli table has valid data (at least one non-zero):
		    cur_pack=dsi_pack.dsi_gi.nv_pck_lbn+
				dsi_pack.sml_agli.data[dvd_angle].address;
		    angle_seek=0;
		    MSG_V("Angle-seek synced using sml_agli map!  new_lba=0x%X\n",cur_pack);
		} else {
		    // check if we're in the right cell, jump otherwise:
		    if( (dsi_pack.dsi_gi.vobu_c_idn==cur_pgc->cell_position[cur_cell].cell_nr) &&
			(dsi_pack.dsi_gi.vobu_vob_idn==cur_pgc->cell_position[cur_cell].vob_id_nr) ){
			angle_seek=0;
			MSG_V("Angle-seek synced by cell/vob IDN search!\n");
		    } else {
			// wrong angle, skip this vobu:
			cur_pack=dsi_pack.dsi_gi.nv_pck_lbn+
				    dsi_pack.dsi_gi.vobu_ea;
			angle_seek=2; // DEBUG
		    }
		}
	    }
	    if(vobu_s_pts < vobu_e_pts) {
		_stream_pts += vobu_e_pts-vobu_s_pts;
		MSG_V("DVD's discontinuities found! Applying delta: %f\n",_stream_pts);
	    }
	    else _stream_pts = vobu_s_pts;
	}
	++cur_pack;
	goto read_next;
    }

    ++cur_pack;
    if(packs_left>=0) --packs_left;

    if(angle_seek) goto read_next; // searching for Navi packet

    return cur_pack-1;
}

void DvdRead_Stream_Interface::dvd_seek(int pos) {
    int dir=1;
    packs_left=-1;
    if(cur_pack>pos) dir=-1;
    cur_pack=pos;

    reseek:
// check if we stay in current cell (speedup things, and avoid angle skip)
    if(cur_pack>cell_last_pack ||
	cur_pack<cur_pgc->cell_playback[ cur_cell ].first_sector) {
	// ok, cell change, find the right cell!
	cur_cell=0;
	if( cur_pgc->cell_playback[cur_cell].block_type == BLOCK_TYPE_ANGLE_BLOCK ) cur_cell+=dvd_angle;
	while(1){
	    int next;
	    cell_last_pack=cur_pgc->cell_playback[ cur_cell ].last_sector;
	    if(cur_pack<cur_pgc->cell_playback[ cur_cell ].first_sector) {
		cur_pack=cur_pgc->cell_playback[ cur_cell ].first_sector;
		break;
	    }
	    if(cur_pack<=cell_last_pack) break; // ok, we find it! :)
	    next=next_cell();
	    if(next<0) {
		// we're after the last cell
		if(dir>0) {// FF
		    if(cur_title<last_title) {
			cur_title++;
			next_title(cur_title);
			goto reseek;
		    }
		} else {// BACK
		    if(cur_title>first_title) {
			cur_title--;
			next_title(cur_title);
			goto reseek;
		    }
		}
		break; // EOF
	    }
	    cur_cell=next;
	}
	_stream_pts=vobu_s_pts=vobu_e_pts=0.;
    }

    MSG_V( "DVD Seek! lba=0x%X  cell=%d  packs: 0x%X-0x%X  \n",
	cur_pack,cur_cell,cur_pgc->cell_playback[ cur_cell ].first_sector,cell_last_pack);

    // if we're in interleaved multi-angle cell, find the right angle chain!
    // (read Navi block, and use the seamless angle jump table)
    angle_seek=1;
}

void DvdRead_Stream_Interface::close() {
    ifoClose(vts_file);
    ifoClose(vmg_file);
    DVDCloseFile(title);
    DVDClose(dvd);
    /* for reenterability */
    dvd_chapter=1;
    dvd_last_chapter=0;
    dvd_angle=1;
}

MPXP_Rc DvdRead_Stream_Interface::open(const std::string& filename,unsigned flags)
{
    UNUSED(flags);
    int dvd_title;
    const char *args;
    char *dvd_device,*tilde,*comma,*par;
    char param[256];

    last_title=-1;
    if(filename=="help" || filename.length()==10) {
	MSG_HINT("Usage: dvdread://<@device>#<titleno>-<lasttitle>,<chapter>-<lastchapter>,<angle>\n");
	return MPXP_False;
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
    if(comma) {
	comma++;
	par = comma;
	tilde=strchr(comma,'-');
	comma=strchr(comma,',');
	dvd_chapter=atoi(par);
	if(tilde) {
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
	if(dvd_device) delete dvd_device;
	return MPXP_False;
    }
    if(dvd_device) delete dvd_device;
    MSG_V(MSGTR_DVDwait);

    /**
     * Load the video manager to find out the information about the titles on
     * this disc.
     */
    vmg_file = ifoOpen( dvd, 0 );
    if( !vmg_file ) {
	MSG_ERR( "Can't open VMG info!\n");
	DVDClose( dvd );
	return MPXP_False;
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
	return MPXP_False;
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
	return MPXP_False;
    }
    if( dvd_last_chapter>0 ) {
	if ( dvd_last_chapter<dvd_chapter || dvd_last_chapter>tt_srpt->title[dvd_title].nr_of_ptts ) {
	    MSG_ERR( "Invalid DVD last chapter number: %d\n", dvd_last_chapter);
	    ifoClose( vmg_file );
	    DVDClose( dvd );
	    return MPXP_False;
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
	return MPXP_False;
    }
    --dvd_angle; // remap 1.. -> 0..

    cur_title=first_title=dvd_title;

    if(!next_title(dvd_title)) {
	ifoClose( vmg_file );
	DVDClose( dvd );
	return MPXP_False;
    }
    _end_pos= (off_t)(cur_pgc->cell_playback[last_cell-1].last_sector)*2048;
    if(next_title(last_title))
	_end_pos=(off_t)(cur_pgc->cell_playback[last_cell-1].last_sector)*2048;
    next_title(dvd_title);

    MSG_V( MSGTR_DVDopenOk);

    spos=0;
    return MPXP_Ok;
}
Stream::type_e DvdRead_Stream_Interface::type() const { return Stream::Type_Seekable|Stream::Type_Program; }
off_t	DvdRead_Stream_Interface::start_pos() const { return (off_t)cur_pack*2048; }
off_t	DvdRead_Stream_Interface::size() const { return _end_pos; }
off_t	DvdRead_Stream_Interface::sector_size() const { return 2048; }
float	DvdRead_Stream_Interface::stream_pts() const { return _stream_pts; }
std::string DvdRead_Stream_Interface::mime_type() const { return "application/octet-stream"; }

int DvdRead_Stream_Interface::read(stream_packet_t *sp)
{
    off_t pos=read_sector(reinterpret_cast<unsigned char*>(sp->buf));
    sp->type=0;
    if(pos>=0){
	sp->len=2048; // full sector
	spos += 2048;
    } else sp->len= -1; // error
    return sp->len;
}

off_t DvdRead_Stream_Interface::seek(off_t newpos)
{
    off_t pos=newpos/2048;
    dvd_seek(pos);
    spos=pos*2048;
    return spos;
}

off_t DvdRead_Stream_Interface::tell() const
{
    return spos;
}

unsigned* DvdRead_Stream_Interface::get_palette() const
{
    if(cur_pgc)
	return cur_pgc->palette;
    return 0;
}

MPXP_Rc DvdRead_Stream_Interface::ctrl(unsigned cmd,any_t*args)
{
    switch(cmd) {
	case SCTRL_VID_GET_PALETTE: {
	    unsigned* pal;
	    pal=get_palette();
	    *((unsigned **)args)=pal;
	    return MPXP_Ok;
	}
	break;
	case SCTRL_LNG_GET_AID: {
	    int aid;
	    aid=aid_from_lang(reinterpret_cast<char*>(args));
	    *((int *)args)=aid;
	    return MPXP_Ok;
	}
	break;
	case SCTRL_LNG_GET_SID: {
	    int aid;
	    aid=sid_from_lang(reinterpret_cast<char*>(args));
	    *((int *)args)=aid;
	    return MPXP_Ok;
	}
	break;
	default: break;
    }
    return MPXP_False;
}

static Stream_Interface* query_interface(libinput_t& libinput) { return new(zeromem) DvdRead_Stream_Interface(libinput); }

extern const stream_interface_info_t dvdread_stream =
{
    "dvdread://",
    "reads multimedia stream using low-level libdvdread access",
    query_interface
};
} // namespace mpxp
#endif

