#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;
#include <algorithm>

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
void		Stream::type(Stream::type_e t) { _type=t; }
off_t		Stream::pos() const { return _pos; }
void		Stream::pos(off_t p) { _pos = p; }
int		Stream::eof() const { return _eof; }
void		Stream::eof(int e) { _eof = e; }

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
int	Stream::read(stream_packet_t * sp) { return driver->read(sp); }
int	Stream::read(any_t* mem,int total) {
    stream_packet_t sp;
    sp.type=0;
    sp.buf=(char *)mem;
    sp.len=total;
    return read(&sp);
}
off_t	Stream::seek(off_t off) { return driver->seek(off); }
int	Stream::skip(off_t off) { return driver->seek(_pos+off); }
off_t	Stream::tell() const { return driver->tell(); }
void	Stream::close() { driver->close(); }

void Stream::reset(){
    if(_eof){
	_pos=0;
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

Buffered_Stream::Buffered_Stream(Stream::type_e _t)
		:Stream(_t),
		buf_pos(0),buf_len(0),buffer(NULL)
{
}

Buffered_Stream::~Buffered_Stream() {
    if(buffer) delete buffer;
}

//=================== STREAMER =========================
MPXP_Rc Buffered_Stream::open(libinput_t*libinput,const char* filename,int* ff)
{
    MPXP_Rc rc=Stream::open(libinput,filename,ff);
    if(rc==MPXP_Ok) {
	delete buffer;
	buf_len=sector_size();
	buffer= new unsigned char [buf_len];
	buf_pos=0;
    }
    return rc;
}

int Buffered_Stream::read_cbuffer(){
    int len,legacy_eof;
    stream_packet_t sp;
    if(eof()) { buf_pos=buf_len=0; return 0; }
    while(1) {
	sp.type=0;
	sp.len=sector_size();
	sp.buf=(char *)buffer;
	len = Stream::read(&sp);
	if(sp.type) {
//	    if(event_handler) event_handler(this,&sp);
	    continue;
	}
	if(ctrl(SCTRL_EOF,NULL)==MPXP_Ok)legacy_eof=1;
	else				legacy_eof=0;
	if(sp.len<=0 || legacy_eof) {
	    MSG_DBG3("nc_stream_read_cbuffer: Guess EOF\n");
	    eof(1);
	    buf_pos=buf_len=0;
	    if(errno) { MSG_WARN("nc_stream_read_cbuffer(drv:%s) error: %s\n",driver_info->mrl,strerror(errno)); errno=0; }
	    return 0;
	}
	break;
    }
    buf_pos=0;
    buf_len=sp.len;
    pos(pos()+sp.len);
    MSG_DBG3("nc_stream_read_cbuffer(%s) done[sector_size=%i len=%i]: buf_pos=%u buf_len=%u pos=%llu\n",driver_info->mrl,sector_size(),len,buf_pos,buf_len,pos());
    return buf_len;
}

off_t Buffered_Stream::seek_long(off_t _p)
{
    off_t newpos=_p;
    unsigned _sector_size=sector_size();

    buf_pos=buf_len=0;
//  newpos=pos&(~((long long)sector_size-1));
    newpos=(_p/(long long)_sector_size)*(long long)_sector_size;

    _p-=newpos;
    MSG_DBG3("nc_stream_seek_long to %llu\n",newpos);
    if(newpos==0 || newpos!=pos()) {
	pos(Stream::seek(newpos));
	if(errno) { MSG_WARN("nc_stream_seek(drv:%s) error: %s\n",driver_info->mrl,strerror(errno)); errno=0; }
    }
    MSG_DBG3("nc_stream_seek_long after: %llu\n",pos());

    if(pos()<0) eof(1);
    else {
	eof(0);
	read_cbuffer();
	if(_p>=0 && _p<=buf_len){
	    buf_pos=_p;
	    MSG_DBG3("nc_stream_seek_long done: pos=%llu buf_pos=%lu buf_len=%lu\n",pos(),buf_pos,buf_len);
	    if(buf_pos==buf_len) {
		MSG_DBG3("nc_stream_seek_long: Guess EOF\n");
		eof(1);
	    }
	    return _p;
	}
    }
    MSG_V("stream_seek: WARNING! Can't seek to 0x%llX !\n",(long long)(_p+newpos));
    return 0;
}

int Buffered_Stream::read_char()
{
    unsigned char retval;
    read(&retval,1);
    return eof()?-256:retval;
}

int Buffered_Stream::read(stream_packet_t* sp) {
    return Stream::read(sp);
}

int Buffered_Stream::read(any_t* _mem,int total){
    int i,x,ilen,_total=total,got_len;
    char *mem=reinterpret_cast<char *>(_mem);
    MSG_DBG3( "nc_stream_read  %u bytes from %llu\n",total,file_pos()+buf_pos);
    if(eof()) return 0;
    x=buf_len-buf_pos;
    if(x>0) {
	ilen=std::min(_total,x);
	memcpy(mem,&buffer[buf_pos],ilen);
	MSG_DBG3("nc_stream_read:  copy prefetched %u bytes\n",ilen);
	buf_pos+=ilen;
	mem+=ilen; _total-=ilen;
    }
    ilen=_total;
    ilen /= sector_size();
    ilen *= sector_size();
    /*
      Perform direct reading to avoid an additional memcpy().
      This case happens for un-compressed streams or for movies with large image.
      Note: for stream with high compression-ratio stream reading is invisible
      from point of CPU usage.
    */
    got_len=0;
    if(ilen) {
	int rlen,stat,tile;
	any_t*smem;
	smem=buffer;
	rlen=ilen;
	stat=0;
	tile=0;
	eof(0);
	got_len=0;
	while(rlen) {
	    buffer=(unsigned char *)mem;
	    buf_len=rlen;
	    read_cbuffer();
	    mem += std::min(rlen,(int)buf_len);
	    tile=buf_len-rlen;
	    rlen -= std::min(rlen,(int)buf_len);
	    got_len += std::min(rlen,(int)buf_len);
	    if(eof()) break;
	    stat++;
	}
	buffer=reinterpret_cast<unsigned char *>(smem);
	buf_len=0;
	buf_pos=0;
	ilen += rlen;
	MSG_DBG2("nc_stream_read  got %u bytes directly for %u calls\n",got_len,stat);
	if(tile && !eof()) {
	    /* should never happen. Store data back to native cache! */
	    MSG_DBG3("nc_stream_read:  we have tile %u bytes\n",tile);
	    buf_pos=0;
	    memcpy(buffer,&mem[buf_len-tile],std::min(int(STREAM_BUFFER_SIZE),tile));
	    buf_len=std::min(int(STREAM_BUFFER_SIZE),tile);
	}
    }
    ilen=_total-ilen;
    if(eof()) return got_len;
    while(ilen) {
	if(buf_pos>=buf_len){
	    read_cbuffer();
	    if(buf_len<=0) return -1; // EOF
	}
	x=buf_len-buf_pos;
	if(buf_pos>buf_len) MSG_WARN( "stream_read: WARNING! buf_pos(%i)>buf_len(%i)\n",buf_pos,buf_len);
	if(x>ilen) x=ilen;
	memcpy(mem,&buffer[buf_pos],x);
	buf_pos+=x;
	mem+=x; ilen-=x;
    }
    MSG_DBG3( "nc_stream_read  got %u bytes ",total);
    for(i=0;i<std::min(8,total);i++) MSG_DBG3("%02X ",(int)((unsigned char)mem[i]));
    MSG_DBG3("\n");
    return total;
}

off_t Buffered_Stream::tell() const {
  off_t retval;
  retval = file_pos()+buf_pos;
  return retval;
}

off_t Buffered_Stream::seek(off_t _p){
    MSG_DBG3( "nc_stream_seek to %llu\n",(long long)_p);
    if(type()&Stream::Type_Memory) {
	buf_pos=_p;
	return _p;
    } else if(_p>=file_pos() && _p<file_pos()+buf_len) {
	buf_pos=_p-file_pos();
	return _p;
    }
    return (type()&Stream::Type_Seekable)?seek_long(_p):_p;
}

int Buffered_Stream::skip(off_t len){
    if(len<0 || (len>2*STREAM_BUFFER_SIZE && type()&Stream::Type_Seekable)) {
	/* negative or big skip! */
	return seek(tell()+len);
    }
    while(len>0){
	int x=buf_len-buf_pos;
	if(x==0){
	    if(!read_cbuffer()) return 0; // EOF
	    x=buf_len-buf_pos;
	}
	if(x>len) x=len;
	buf_pos+=x; len-=x;
    }
    return 1;
}

unsigned int Buffered_Stream::read_word(){
  unsigned short retval;
  read((char *)&retval,2);
  return me2be_16(retval);
}

unsigned int Buffered_Stream::read_dword(){
  unsigned int retval;
  read((char *)&retval,4);
  return me2be_32(retval);
}

uint64_t Buffered_Stream::read_qword(){
  uint64_t retval;
  read((char *)&retval,8);
  return me2be_64(retval);
}

unsigned int Buffered_Stream::read_word_le(){
  unsigned short retval;
  read((char *)&retval,2);
  return me2le_16(retval);
}

unsigned int Buffered_Stream::read_dword_le(){
  unsigned int retval;
  read((char *)&retval,4);
  return me2le_32(retval);
}

uint64_t Buffered_Stream::read_qword_le(){
  uint64_t retval;
  read((char *)&retval,8);
  return me2le_64(retval);
}

unsigned int Buffered_Stream::read_int24(){
  unsigned int y;
  y = read_char();
  y=(y<<8)|read_char();
  y=(y<<8)|read_char();
  return y;
}

Memory_Stream::Memory_Stream(const unsigned char* data,unsigned len)
		:Stream(Stream::Type_Memory),
		_len(len)
{
  pin=STREAM_PIN;
// may be methods of class Memory_Stream : public Stream
  buffer=new uint8_t [len];
  reset();
  pos(0);
  memcpy(buffer,data,len);
}

Memory_Stream::~Memory_Stream() { delete buffer; }
unsigned Memory_Stream::sector_size() const { return 1; }
off_t	Memory_Stream::start_pos() const { return 0; }
off_t	Memory_Stream::end_pos() const { return _len; }

int	Memory_Stream::read(any_t* mem,int total) {
    memcpy(mem,buffer,total);
    pos(pos()+total);
    return total;
}

off_t	Memory_Stream::tell() const { return pos(); }
off_t	Memory_Stream::seek(off_t p) { pos(p); return pos(); }
int	Memory_Stream::skip(off_t len) { pos(pos()+len); return 1; }
int	Memory_Stream::eof() const { return pos()>=_len; }
void	Memory_Stream::eof(int e) { UNUSED(e); }
void	Memory_Stream::reset() { pos(0); }

int Memory_Stream::read_char(){
  int retval;
  read((char *)&retval,1);
  return eof()?-256:retval;
}

unsigned int Memory_Stream::read_word(){
  unsigned short retval;
  read((char *)&retval,2);
  return me2be_16(retval);
}

unsigned int Memory_Stream::read_dword(){
  unsigned int retval;
  read((char *)&retval,4);
  return me2be_32(retval);
}

uint64_t Memory_Stream::read_qword(){
  uint64_t retval;
  read((char *)&retval,8);
  return me2be_64(retval);
}

unsigned int Memory_Stream::read_word_le(){
  unsigned short retval;
  read((char *)&retval,2);
  return me2le_16(retval);
}

unsigned int Memory_Stream::read_dword_le(){
  unsigned int retval;
  read((char *)&retval,4);
  return me2le_32(retval);
}

uint64_t Memory_Stream::read_qword_le(){
  uint64_t retval;
  read((char *)&retval,8);
  return me2le_64(retval);
}

unsigned int Memory_Stream::read_int24(){
  unsigned int y;
  y = read_char();
  y=(y<<8)|read_char();
  y=(y<<8)|read_char();
  return y;
}

} //namespace mpxp
