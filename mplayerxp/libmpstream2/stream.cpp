#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
#include <algorithm>
#include <iomanip>

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>

#include "mplayerxp.h"
#include "mpxp_help.h"

#include "osdep/fastmemcpy.h"
#include "osdep/bswap.h"

#include "stream.h"
#include "stream_internal.h"
#include "stream_msg.h"

namespace	usr {
#ifdef HAVE_LIBCDIO_CDDA
extern const stream_interface_info_t cdda_stream;
extern const stream_interface_info_t cddb_stream;
#endif
#ifdef USE_DVDNAV
extern const stream_interface_info_t dvdnav_stream;
#endif
#ifdef USE_DVDREAD
extern const stream_interface_info_t dvdread_stream;
#endif
#ifdef USE_TV
extern const stream_interface_info_t tv_stream;
#endif
#ifdef HAVE_STREAMING
extern const stream_interface_info_t ftp_stream;
extern const stream_interface_info_t rtsp_stream;
extern const stream_interface_info_t network_stream;
extern const stream_interface_info_t udp_stream;
#endif
#ifdef USE_OSS_AUDIO
extern const stream_interface_info_t oss_stream;
#endif
#ifdef USE_LIBVCD
extern const stream_interface_info_t vcdnav_stream;
#endif
extern const stream_interface_info_t lavs_stream;
extern const stream_interface_info_t stdin_stream;
extern const stream_interface_info_t file_stream;
extern const stream_interface_info_t null_stream;

Stream::Stream(Stream::type_e t)
	:_type(t)
{
#ifdef HAVE_LIBCDIO_CDDA
    list.push_back(&cdda_stream);
#ifdef HAVE_STREAMING
    list.push_back(&cddb_stream);
#endif
#endif
#ifdef USE_DVDNAV
    list.push_back(&dvdnav_stream);
#endif
#ifdef USE_DVDREAD
    list.push_back(&dvdread_stream);
#endif
#ifdef USE_TV
    list.push_back(&tv_stream);
#endif
#ifdef USE_LIBVCD
    list.push_back(&vcdnav_stream);
#endif
#ifdef USE_OSS_AUDIO
    list.push_back(&oss_stream);
#endif
#ifdef HAVE_STREAMING
    list.push_back(&ftp_stream);
    list.push_back(&rtsp_stream);
    list.push_back(&udp_stream);
    list.push_back(&network_stream);
#endif
    list.push_back(&lavs_stream);
    list.push_back(&stdin_stream);
    list.push_back(&file_stream);
    list.push_back(&null_stream);
    pin=STREAM_PIN;
    reset();
}

Stream::~Stream(){
    mpxp_info<<std::endl<<"*** free_stream(drv:"<<driver_info->mrl<<") called [errno: "<<strerror(errno)<<"]***"<<std::endl;
    if(driver) close();
    delete driver;
}

Stream::type_e	Stream::type() {
    Stream::type_e re=_type;
    if(_type!=Stream::Type_Unknown) {
	_type=Stream::Type_Unknown;
    } else re = driver->type();
    return re;
}
off_t		Stream::start_pos() const { return driver->start_pos(); }
off_t		Stream::end_pos() const { return driver->size(); }
unsigned	Stream::sector_size() const { return driver->sector_size(); }
float		Stream::stream_pts() const { return driver->stream_pts(); }
std::string	Stream::mime_type() const { return driver->mime_type(); }
void		Stream::type(Stream::type_e t) { _type=t; }
int		Stream::eof() const { return _eof; }
void		Stream::eof(int e) { if(!e) reset(); _eof = e; }

MPXP_Rc		Stream::open(libinput_t&libinput,const std::string& filename,int* ff)
{
    size_t i,sz=list.size();
    unsigned mrl_len;
    file_format=*ff;
    for(i=0;i<sz;i++) {
	mrl_len=strlen(list[i]->mrl);
	if(filename.substr(0,mrl_len)==list[i]->mrl||list[i]->mrl[0]=='*') {
	    mpxp_v<<"[Stream]: "<<"Opening "<<list[i]->mrl<<" ... ";
	    Stream_Interface* drv = list[i]->query_interface(libinput);
	    if(list[i]->mrl[0]=='*') mrl_len=0;
	    if(drv->open(&filename[mrl_len],0)==MPXP_Ok) {
		mpxp_v<<"Ok"<<std::endl;
		*ff = file_format;
		driver_info=list[i];
		driver=drv;
		return MPXP_Ok;
	    }
	    delete drv;
	    mpxp_v<<"False"<<std::endl;
	}
    }
    Stream_Interface* file_drv = file_stream.query_interface(libinput);
    /* Last hope if not mrl specified */
    if(file_drv->open(filename,0)) {
	*ff = file_format;
	driver_info=&file_stream;
	driver=file_drv;
	return MPXP_Ok;
    }
    delete file_drv;
    return MPXP_False;
}
MPXP_Rc Stream::ctrl(unsigned cmd,any_t* param) { return driver->ctrl(cmd,param); }
int	Stream::read(stream_packet_t* sp) { return driver->read(sp); }
binary_packet Stream::read(size_t total) {
    off_t _off;
    int rc;
    stream_packet_t sp;
    binary_packet bp(total);
    sp.type=0;
    sp.buf=(char *)bp.data();
    sp.len=total;
    _off=tell();
    rc=read(&sp);
    if(rc<=0) eof(1);
    /* ------------ print packet ---------- */
    if(mp_conf.verbose>3) {
        unsigned j,_lim=std::min(sp.len,20);
        int printable=1;
	mpxp_dbg4<<rc<<"=[stream.read("<<sp.buf<<","<<sp.len<<")] ["<<std::hex<<std::setfill('0')<<std::setw(16)<<_off<<"] ";
	for(j=0;j<_lim;j++) { if(!isprint(sp.buf[j])) { printable=0; break; } }
	    if(printable) mpxp_dbg4<<std::string(sp.buf).substr(0,20);
	    else for(j=0;j<_lim;j++)
		mpxp_dbg4<<std::hex<<std::setfill('0')<<std::setw(2)<<(unsigned)sp.buf[j];
	mpxp_dbg4<<std::endl;
    }
    /* ------------ print packet ---------- */
    return bp;
}
off_t	Stream::seek(off_t off) { return driver->seek(off); }
int	Stream::skip(off_t off) { return driver->seek(tell()+off)?1:0; }
off_t	Stream::tell() const { return driver->tell(); }
void	Stream::close() { driver->close(); }

void Stream::reset() {
    if(_eof){
	seek(0);
	_eof=0;
    }
}

int Stream::read_char(){
    binary_packet rc=read(1);
    return eof()?-256:rc.cdata()[0];
}

unsigned int Stream::read_word(){
    binary_packet rc=read(2);
    return me2be_16(*(uint16_t*)rc.data());
}

unsigned int Stream::read_dword(){
    binary_packet rc=read(4);
    return me2be_32(*(uint32_t*)rc.data());
}

uint64_t Stream::read_qword(){
    binary_packet rc=read(8);
    return me2be_64(*(uint64_t*)rc.data());
}

unsigned int Stream::read_word_le(){
    binary_packet rc=read(2);
    return me2le_16(*(uint16_t*)rc.data());
}

unsigned int Stream::read_dword_le(){
    binary_packet rc=read(4);
    return me2le_32(*(uint32_t*)rc.data());
}

uint64_t Stream::read_qword_le(){
    binary_packet rc=read(8);
    return me2le_64(*(uint64_t*)rc.data());
}

unsigned int Stream::read_int24(){
    unsigned int y;
    y = read_char();
    y=(y<<8)|read_char();
    y=(y<<8)|read_char();
    return y;
}

static const stream_interface_info_t* sdrivers[] =
{
#ifdef HAVE_LIBCDIO_CDDA
    &cdda_stream,
#ifdef HAVE_STREAMING
    &cddb_stream,
#endif
#endif
#ifdef USE_DVDNAV
    &dvdnav_stream,
#endif
#ifdef USE_DVDREAD
    &dvdread_stream,
#endif
#ifdef USE_TV
    &tv_stream,
#endif
#ifdef USE_LIBVCD
    &vcdnav_stream,
#endif
#ifdef USE_OSS_AUDIO
    &oss_stream,
#endif
#ifdef HAVE_STREAMING
    &ftp_stream,
    &rtsp_stream,
    &udp_stream,
    &network_stream,
#endif
    &lavs_stream,
    &stdin_stream,
    &file_stream,
    &null_stream,
    NULL
};
void Stream::print_drivers()
{
    unsigned i;
    mpxp_info<<"[Stream]: "<<"Available stream drivers:"<<std::endl;
    for(i=0;sdrivers[i];i++) {
	mpxp_info<<std::left<<std::setw(10)<<sdrivers[i]->mrl<<" "<<sdrivers[i]->descr<<std::endl;
    }
}

/* ================================================ */

Memory_Stream::Memory_Stream(const unsigned char* data,unsigned len)
		:Stream(Stream::Type_Memory),
		_len(len)
{
    buffer=new uint8_t [len];
    reset();
    _pos=0;
    memcpy(buffer,data,len);
}

Memory_Stream::~Memory_Stream() { delete buffer; }
unsigned Memory_Stream::sector_size() const { return 1; }
off_t	Memory_Stream::start_pos() const { return 0; }
off_t	Memory_Stream::end_pos() const { return _len; }

binary_packet Memory_Stream::read(size_t total) {
    binary_packet rc(buffer,total);
    _pos+=total;
    return rc;
}

off_t	Memory_Stream::tell() const { return _pos; }
off_t	Memory_Stream::seek(off_t p) { return _pos=p; }
int	Memory_Stream::skip(off_t len) { _pos+=len; return 1; }
int	Memory_Stream::eof() const { return _pos>=_len; }
void	Memory_Stream::eof(int e) { e?_pos=_len+1:_pos=0; }
void	Memory_Stream::reset() { _pos=0; }
std::string	Memory_Stream::mime_type() const { return "application/octet-stream"; }

} //namespace	usr
