#ifndef __CDD_H__
#define __CDD_H__
#include <cdio/cdda.h>
#include "libmpconf/cfgparser.h"

namespace mpxp {
    struct libinput_t;

    struct cddb_data_t {
	char cddb_hello[1024];
	unsigned long disc_id;
	unsigned int tracks;
	char *cache_dir;
	char *freedb_server;
	int freedb_proto_level;
	int anonymous;
	char category[100];
	char *xmcd_file;
	size_t xmcd_file_size;
	any_t*user_data;
	libinput_t*libinput;
    };

    struct cd_toc_t {
	unsigned int min, sec, frame;
    };

    struct cd_track_t {
	char *name;
	unsigned int track_nb;
	unsigned int min;
	unsigned int sec;
	unsigned int msec;
	unsigned long frame_begin;
	unsigned long frame_length;
	cd_track_t *prev;
	cd_track_t *next;
    };

    struct cd_info_t {
	char *artist;
	char *album;
	char *genre;
	unsigned int nb_tracks;
	unsigned int min;
	unsigned int sec;
	unsigned msec;
	cd_track_t *first;
	cd_track_t *last;
	cd_track_t *current;
    };

    struct my_track_t {
	int play;
	lsn_t start_sector;
	lsn_t end_sector;
    };

    struct cdda_priv : public Opaque {
	public:
	    cdda_priv();
	    virtual ~cdda_priv();

	    cdrom_drive_t* cd;
	    my_track_t tracks[256]; /* hope that's enough */
	    unsigned min;
	    unsigned sec;
	    unsigned msec;
	    lsn_t sector;
	    lsn_t start_sector;
	    lsn_t end_sector;
    };

    cd_info_t*  __FASTCALL__ cd_info_new();
    void	    __FASTCALL__ cd_info_free(cd_info_t *cd_info);
    cd_track_t* __FASTCALL__ cd_info_add_track(cd_info_t *cd_info, char *track_name, unsigned int track_nb, unsigned int min, unsigned int sec, unsigned int msec, unsigned long frame_begin, unsigned long frame_length);
    cd_track_t* __FASTCALL__ cd_info_get_track(cd_info_t *cd_info, unsigned int track_nb);

    cdda_priv* __FASTCALL__	open_cdda(const char* dev,const char* track);
    cdda_priv* __FASTCALL__	open_cddb(libinput_t*,const char* dev,const char* track);
    int     __FASTCALL__	read_cdda(cdda_priv* s,char *buf,track_t* trackidx);
    void    __FASTCALL__	seek_cdda(cdda_priv* s,off_t pos,track_t *trackidx);
    off_t   __FASTCALL__	tell_cdda(const cdda_priv* s);
    void    __FASTCALL__	close_cdda(cdda_priv*);
    off_t	__FASTCALL__	cdda_start(cdda_priv*);
    off_t	__FASTCALL__	cdda_size(cdda_priv*);
    void cdda_register_options(m_config_t* cfg);
} // namespace mpxp
#endif // __CDD_H__
