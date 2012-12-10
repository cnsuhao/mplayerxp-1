#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
#include <algorithm>

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
#include "help_mp.h"

#include "osdep/fastmemcpy.h"
#include "osdep/bswap.h"

#include "stream.h"
#include "stream_internal.h"
#include "libmpdemux/demuxer.h"
#include "stream_msg.h"

namespace mpxp {
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
extern const stream_interface_info_t network_stream;
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
    &network_stream,
#endif
    &lavs_stream,
    &stdin_stream,
    &file_stream,
    &null_stream,
    NULL
};

Stream::Stream(Stream::type_e t)
	:_type(t)
{
    fill_false_pointers(antiviral_hole,reinterpret_cast<long>(&pin)-reinterpret_cast<long>(&antiviral_hole));
    pin=STREAM_PIN;
    reset();
}

Stream::~Stream(){
    MSG_INFO("\n*** free_stream(drv:%s) called [errno: %s]***\n",driver_info->mrl,strerror(errno));
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

MPXP_Rc		Stream::open(libinput_t*libinput,const char* filename,int* ff)
{
    unsigned i,done;
    unsigned mrl_len;
    file_format=*ff;
    done=0;
    for(i=0;sdrivers[i]!=&null_stream;i++) {
	mrl_len=strlen(sdrivers[i]->mrl);
	if(strncmp(filename,sdrivers[i]->mrl,mrl_len)==0||sdrivers[i]->mrl[0]=='*') {
	    MSG_V("Opening %s ... ",sdrivers[i]->mrl);
	    Stream_Interface* drv = sdrivers[i]->query_interface(libinput);
	    if(sdrivers[i]->mrl[0]=='*') mrl_len=0;
	    if(drv->open(&filename[mrl_len],0)==MPXP_Ok) {
		MSG_V("OK\n");
		*ff = file_format;
		driver_info=sdrivers[i];
		driver=drv;
		return MPXP_Ok;
	    }
	    delete drv;
	    MSG_V("False\n");
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
int	Stream::read(any_t* mem,int total) {
    off_t _off;
    int rc;
    stream_packet_t sp;
    sp.type=0;
    sp.buf=(char *)mem;
    sp.len=total;
    _off=tell();
    rc=read(&sp);
    if(rc<=0) eof(1);
    /* ------------ print packet ---------- */
    unsigned j,_lim=std::min(sp.len,20);
    int printable=1;
    MSG_DBG4("%i=[stream.read(%p,%i)] [%016X]",rc,sp.buf,sp.len,_off);
    for(j=0;j<_lim;j++) { if(!isprint(sp.buf[j])) { printable=0; break; } }
	if(printable) MSG_DBG4("%20s",sp.buf);
	else for(j=0;j<_lim;j++) MSG_DBG4("%02X ",(unsigned char)sp.buf[j]);
    /* ------------ print packet ---------- */
    return rc;
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
  int retval;
  read((char *)&retval,1);
  return eof()?-256:retval;
}

unsigned int Stream::read_word(){
  unsigned short retval;
  read((char *)&retval,2);
  return me2be_16(retval);
}

unsigned int Stream::read_dword(){
  unsigned int retval;
  read((char *)&retval,4);
  return me2be_32(retval);
}

uint64_t Stream::read_qword(){
  uint64_t retval;
  read((char *)&retval,8);
  return me2be_64(retval);
}

unsigned int Stream::read_word_le(){
  unsigned short retval;
  read((char *)&retval,2);
  return me2le_16(retval);
}

unsigned int Stream::read_dword_le(){
  unsigned int retval;
  read((char *)&retval,4);
  return me2le_32(retval);
}

uint64_t Stream::read_qword_le(){
  uint64_t retval;
  read((char *)&retval,8);
  return me2le_64(retval);
}

unsigned int Stream::read_int24(){
  unsigned int y;
  y = read_char();
  y=(y<<8)|read_char();
  y=(y<<8)|read_char();
  return y;
}

void Stream::print_drivers()
{
    unsigned i;
    MSG_INFO("Available stream drivers:\n");
    for(i=0;sdrivers[i];i++) {
	MSG_INFO(" %-10s %s\n",sdrivers[i]->mrl,sdrivers[i]->descr);
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

int	Memory_Stream::read(any_t* mem,int total) {
    memcpy(mem,buffer,total);
    _pos+=total;
    return total;
}

off_t	Memory_Stream::tell() const { return _pos; }
off_t	Memory_Stream::seek(off_t p) { return _pos=p; }
int	Memory_Stream::skip(off_t len) { _pos+=len; return 1; }
int	Memory_Stream::eof() const { return _pos>=_len; }
void	Memory_Stream::eof(int e) { e?_pos=_len+1:_pos=0; }
void	Memory_Stream::reset() { _pos=0; }
std::string	Memory_Stream::mime_type() const { return "application/octet-stream"; }

} //namespace mpxp
