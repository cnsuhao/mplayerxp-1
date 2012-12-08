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
    buffer=new unsigned char [STREAM_BUFFER_SIZE];
    buf_len=STREAM_BUFFER_SIZE;
    reset();
}

Stream::~Stream(){
    MSG_INFO("\n*** free_stream(drv:%s) called [errno: %s]***\n",driver_info->mrl,strerror(errno));
    if(driver) close();
    delete buffer;
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

MPXP_Rc		Stream::open(libinput_t*libinput,const char* filename,int* ff,stream_callback eh)
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
		event_handler=eh;
		buffer=(unsigned char*)mp_realloc(buffer,sector_size());
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
	event_handler=eh;
	buffer=(unsigned char *)mp_realloc(buffer,sector_size());
	return MPXP_Ok;
    }
    delete file_drv;
    delete buffer;
    return MPXP_False;
}
MPXP_Rc Stream::ctrl(unsigned cmd,any_t* param) { return driver->ctrl(cmd,param); }
int	Stream::read(stream_packet_t * sp) { return driver->read(sp); }
off_t	Stream::seek(off_t off) { return driver->seek(off); }
off_t	Stream::tell() const { return driver->tell(); }
void	Stream::close() { driver->close(); }

void Stream::reset(){
    if(_eof){
	_pos=0;
	_eof=0;
    }
}

void print_stream_drivers( void )
{
    unsigned i;
    MSG_INFO("Available stream drivers:\n");
    for(i=0;sdrivers[i];i++) {
	MSG_INFO(" %-10s %s\n",sdrivers[i]->mrl,sdrivers[i]->descr);
    }
}

static inline off_t FILE_POS(Stream* s) { return s->pos()-s->buf_len; }

//=================== STREAMER =========================

int __FASTCALL__ nc_stream_read_cbuffer(Stream *s){
  int len,legacy_eof;
  stream_packet_t sp;
  if(s->eof()){ s->buf_pos=s->buf_len=0; return 0; }
  while(1)
  {
    sp.type=0;
    sp.len=s->sector_size();
    sp.buf=(char *)s->buffer;
    if(s->type()==Stream::Type_DS) len = reinterpret_cast<Demuxer_Stream*>(s)->read_data(s->buffer,s->sector_size());
    else len = s->read(&sp);
    if(sp.type)
    {
	if(s->event_handler) s->event_handler(s,&sp);
	continue;
    }
    if(s->ctrl(SCTRL_EOF,NULL)==MPXP_Ok)legacy_eof=1;
    else				legacy_eof=0;
    if(sp.len<=0 || legacy_eof)
    {
	MSG_DBG3("nc_stream_read_cbuffer: Guess EOF\n");
	s->eof(1);
	s->buf_pos=s->buf_len=0;
	if(errno) { MSG_WARN("nc_stream_read_cbuffer(drv:%s) error: %s\n",s->driver_info->mrl,strerror(errno)); errno=0; }
	return 0;
    }
    break;
  }
  s->buf_pos=0;
  s->buf_len=sp.len;
  s->pos(s->pos()+sp.len);
  MSG_DBG3("nc_stream_read_cbuffer(%s) done[sector_size=%i len=%i]: buf_pos=%u buf_len=%u pos=%llu\n",s->driver_info->mrl,s->sector_size(),len,s->buf_pos,s->buf_len,s->pos());
  return s->buf_len;
}

int __FASTCALL__ nc_stream_seek_long(Stream *s,off_t pos)
{
  off_t newpos=pos;
  unsigned sector_size=s->sector_size();

  s->buf_pos=s->buf_len=0;
//  newpos=pos&(~((long long)sector_size-1));
  newpos=(pos/(long long)sector_size)*(long long)sector_size;

  pos-=newpos;
  MSG_DBG3("nc_stream_seek_long to %llu\n",newpos);
  if(newpos==0 || newpos!=s->pos())
  {
    s->pos(s->seek(newpos));
    if(errno) { MSG_WARN("nc_stream_seek(drv:%s) error: %s\n",s->driver_info->mrl,strerror(errno)); errno=0; }
  }
  MSG_DBG3("nc_stream_seek_long after: %llu\n",s->pos());

  if(s->pos()<0) s->eof(1);
  else
  {
    s->eof(0);
    nc_stream_read_cbuffer(s);
    if(pos>=0 && pos<=s->buf_len){
	s->buf_pos=pos;
	MSG_DBG3("nc_stream_seek_long done: pos=%llu buf_pos=%lu buf_len=%lu\n",s->pos(),s->buf_pos,s->buf_len);
	if(s->buf_pos==s->buf_len)
	{
	    MSG_DBG3("nc_stream_seek_long: Guess EOF\n");
	    s->eof(1);
	}
	return 1;
    }
  }

  MSG_V("stream_seek: WARNING! Can't seek to 0x%llX !\n",(long long)(pos+newpos));
  return 0;

}

Stream* __FASTCALL__ new_memory_stream(const unsigned char* data,int len){
  Stream *s=new(zeromem) Stream;
  if(s==NULL) return NULL;
  s->pin=STREAM_PIN;
  s->buf_pos=0; s->buf_len=len;
// msy be methods of class Memory_Stream : public Stream
//  s->type=STREAMTYPE_MEMORY;
//  s->start_pos=0; s->end_pos=len;
//  s->sector_size=1;
  s->buffer=new unsigned char [len];
  if(s->buffer==NULL) { delete s; return NULL; }
  stream_reset(s);
  s->pos(len);
  memcpy(s->buffer,data,len);
  return s;
}

int __FASTCALL__ nc_stream_read_char(Stream *s)
{
    unsigned char retval;
    nc_stream_read(s,&retval,1);
    return stream_eof(s)?-256:retval;
}

int __FASTCALL__ nc_stream_read(Stream *s,any_t* _mem,int total){
  int i,x,ilen,_total=total,got_len;
  char *mem=reinterpret_cast<char *>(_mem);
  MSG_DBG3( "nc_stream_read  %u bytes from %llu\n",total,FILE_POS(s)+s->buf_pos);
  if(stream_eof(s)) return 0;
  x=s->buf_len-s->buf_pos;
  if(x>0)
  {
    ilen=std::min(_total,x);
    memcpy(mem,&s->buffer[s->buf_pos],ilen);
    MSG_DBG3("nc_stream_read:  copy prefetched %u bytes\n",ilen);
    s->buf_pos+=ilen;
    mem+=ilen; _total-=ilen;
  }
  ilen=_total;
  ilen /= s->sector_size();
  ilen *= s->sector_size();
  /*
      Perform direct reading to avoid an additional memcpy().
      This case happens for un-compressed streams or for movies with large image.
      Note: for stream with high compression-ratio stream reading is invisible
      from point of CPU usage.
  */
  got_len=0;
  if(ilen)
  {
    int rlen,stat,tile,eof;
    any_t*smem;
    smem=s->buffer;
    rlen=ilen;
    stat=0;
    tile=0;
    eof=0;
    got_len=0;
    while(rlen)
    {
	s->buffer=(unsigned char *)mem;
	s->buf_len=rlen;
	nc_stream_read_cbuffer(s);
	mem += std::min(rlen,(int)s->buf_len);
	tile=s->buf_len-rlen;
	rlen -= std::min(rlen,(int)s->buf_len);
	got_len += std::min(rlen,(int)s->buf_len);
	eof=stream_eof(s);
	if(eof) break;
	stat++;
    }
    s->buffer=reinterpret_cast<unsigned char *>(smem);
    s->buf_len=0;
    s->buf_pos=0;
    ilen += rlen;
    MSG_DBG2("nc_stream_read  got %u bytes directly for %u calls\n",got_len,stat);
    if(tile && !eof)
    {
	/* should never happen. Store data back to native cache! */
	MSG_DBG3("nc_stream_read:  we have tile %u bytes\n",tile);
	s->buf_pos=0;
	memcpy(s->buffer,&mem[s->buf_len-tile],std::min(int(STREAM_BUFFER_SIZE),tile));
	s->buf_len=std::min(int(STREAM_BUFFER_SIZE),tile);
    }
  }
  ilen=_total-ilen;
  if(stream_eof(s)) return got_len;
  while(ilen){
    if(s->buf_pos>=s->buf_len){
	nc_stream_read_cbuffer(s);
	if(s->buf_len<=0) return -1; // EOF
    }
    x=s->buf_len-s->buf_pos;
    if(s->buf_pos>s->buf_len) MSG_WARN( "stream_read: WARNING! s->buf_pos(%i)>s->buf_len(%i)\n",s->buf_pos,s->buf_len);
    if(x>ilen) x=ilen;
    memcpy(mem,&s->buffer[s->buf_pos],x);
    s->buf_pos+=x;
    mem+=x; ilen-=x;
  }
  MSG_DBG3( "nc_stream_read  got %u bytes ",total);
  for(i=0;i<std::min(8,total);i++) MSG_DBG3("%02X ",(int)((unsigned char)mem[i]));
  MSG_DBG3("\n");
  return total;
}

off_t __FASTCALL__ nc_stream_tell(Stream *s){
  off_t retval;
  retval = FILE_POS(s)+s->buf_pos;
  return retval;
}

int __FASTCALL__ nc_stream_seek(Stream *s,off_t pos){

  MSG_DBG3( "nc_stream_seek to %llu\n",(long long)pos);
  if(s->type()&Stream::Type_Memory)
  {
    s->buf_pos=pos;
    return 1;
  }
  else
  if(pos>=FILE_POS(s) && pos<FILE_POS(s)+s->buf_len)
  {
    s->buf_pos=pos-FILE_POS(s);
    return 1;
  }
  return (s->type()&Stream::Type_Seekable)?nc_stream_seek_long(s,pos):pos;
}

int __FASTCALL__ nc_stream_skip(Stream *s,off_t len){
  if(len<0 || (len>2*STREAM_BUFFER_SIZE && s->type()&Stream::Type_Seekable)){
    /* negative or big skip! */
    return nc_stream_seek(s,nc_stream_tell(s)+len);
  }
  while(len>0){
    int x=s->buf_len-s->buf_pos;
    if(x==0){
      if(!nc_stream_read_cbuffer(s)) return 0; // EOF
      x=s->buf_len-s->buf_pos;
    }
    if(x>len) x=len;
    s->buf_pos+=x; len-=x;
  }
  return 1;
}

} //namespace mpxp
