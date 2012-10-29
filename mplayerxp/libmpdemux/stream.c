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

#include "mp_config.h"
#include "mplayer.h"
#include "help_mp.h"

#include "osdep/fastmemcpy.h"

#include "stream.h"
#include "demuxer.h"
#include "demux_msg.h"

#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif

#ifdef HAVE_LIBCDIO_CDDA
extern const stream_driver_t cdda_stream;
extern const stream_driver_t cddb_stream;
#endif
#ifdef USE_DVDNAV
extern const stream_driver_t dvdnav_stream;
#endif
#ifdef USE_DVDREAD
extern const stream_driver_t dvdread_stream;
#endif
#ifdef USE_TV
extern const stream_driver_t tv_stream;
#endif
#ifdef HAVE_STREAMING
extern const stream_driver_t ftp_stream;
extern const stream_driver_t network_stream;
#endif
#ifdef USE_OSS_AUDIO
extern const stream_driver_t oss_stream;
#endif
#ifdef USE_LIBVCD
extern const stream_driver_t vcdnav_stream;
#endif
extern const stream_driver_t ffmpeg_stream;
extern const stream_driver_t stdin_stream;
extern const stream_driver_t file_stream;

static const stream_driver_t *sdrivers[] =
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
    &ffmpeg_stream,
    &stdin_stream,
    &file_stream,
};

static const unsigned int nsdrivers=sizeof(sdrivers)/sizeof(stream_driver_t*);

stream_t* __FASTCALL__ open_stream(const char* filename,int* file_format,stream_callback event_handler)
{
  unsigned i,done;
  unsigned mrl_len;
  stream_t* stream=new_stream(STREAMTYPE_STREAM); /* No flags here */
  stream->file_format=*file_format;
  done=0;
  for(i=0;i<nsdrivers;i++)
  {
	mrl_len=strlen(sdrivers[i]->mrl);
	if(strncmp(filename,sdrivers[i]->mrl,mrl_len)==0) {
	    MSG_V("Opening %s ... ",sdrivers[i]->mrl);
	    if(sdrivers[i]->open(stream,&filename[mrl_len],0)) {
		MSG_V("OK\n");
		*file_format = stream->file_format;
		stream->driver=sdrivers[i];
		stream->event_handler=event_handler;
		stream->buffer=realloc(stream->buffer,stream->sector_size);
		return stream;
	    }
	    MSG_V("False\n");
	}
  }
  /* Last hope */
  if(file_stream.open(stream,filename,0)) {
	*file_format = stream->file_format;
	stream->driver=&file_stream;
	stream->event_handler=event_handler;
	stream->buffer=realloc(stream->buffer,stream->sector_size);
	return stream;
  }
  free(stream->buffer);
  free(stream);
  return NULL;
}

void print_stream_drivers( void )
{
  unsigned i;
  MSG_INFO("Available stream drivers:\n");
  for(i=0;i<nsdrivers;i++) {
    MSG_INFO(" %-10s %s\n",sdrivers[i]->mrl,sdrivers[i]->descr);
  }
}

#define FILE_POS(s) (s->pos-s->buf_len)

//=================== STREAMER =========================

int __FASTCALL__ nc_stream_read_cbuffer(stream_t *s){
  int len,legacy_eof;
  stream_packet_t sp;
  if(s->eof){ s->buf_pos=s->buf_len=0; return 0; }
  while(1)
  {
    sp.type=0;
    sp.len=s->sector_size;
    sp.buf=(char *)s->buffer;
    if(s->type==STREAMTYPE_DS) len = demux_read_data((demux_stream_t*)s->priv,s->buffer,s->sector_size);
    else { if(!s->driver) { s->eof=1; return 0; } len = s->driver->read(s,&sp); }
    if(sp.type)
    {
	if(s->event_handler) s->event_handler(s,&sp);
	continue;
    }
    if(s->driver->control(s,SCTRL_EOF,NULL)==SCTRL_OK)	legacy_eof=1;
    else						legacy_eof=0;
    if(sp.len<=0 || legacy_eof)
    {
	MSG_DBG3("nc_stream_read_cbuffer: Guess EOF\n");
	s->eof=1;
	s->buf_pos=s->buf_len=0;
	if(s->_Errno) { MSG_WARN("nc_stream_read_cbuffer(drv:%s) error: %s\n",s->driver->mrl,strerror(s->_Errno)); s->_Errno=0; }
	return 0;
    }
    break;
  }
  s->buf_pos=0;
  s->buf_len=sp.len;
  s->pos += sp.len;
  MSG_DBG3("nc_stream_read_cbuffer(%s) done[sector_size=%i len=%i]: buf_pos=%u buf_len=%u pos=%llu\n",s->driver->mrl,s->sector_size,len,s->buf_pos,s->buf_len,s->pos);
  return s->buf_len;
}

int __FASTCALL__ nc_stream_seek_long(stream_t *s,off_t pos)
{
  off_t newpos=pos;
  unsigned sector_size=s->sector_size;

  s->buf_pos=s->buf_len=0;
//  newpos=pos&(~((long long)sector_size-1));
  newpos=(pos/(long long)sector_size)*(long long)sector_size;

  pos-=newpos;
  MSG_DBG3("nc_stream_seek_long to %llu\n",newpos);
  if(newpos==0 || newpos!=s->pos)
  {
    if(!s->driver) { s->eof=1; return 0; }
    s->pos = s->driver->seek(s,newpos);
    if(s->_Errno) { MSG_WARN("nc_stream_seek(drv:%s) error: %s\n",s->driver->mrl,strerror(s->_Errno)); s->_Errno=0; }
  }
  MSG_DBG3("nc_stream_seek_long after: %llu\n",s->pos);

  if(s->pos<0) s->eof=1;
  else
  {
    s->eof=0;
    nc_stream_read_cbuffer(s);
    if(pos>=0 && pos<=s->buf_len){
	s->buf_pos=pos;
	MSG_DBG3("nc_stream_seek_long done: pos=%llu buf_pos=%lu buf_len=%lu\n",s->pos,s->buf_pos,s->buf_len);
	if(s->buf_pos==s->buf_len)
	{
	    MSG_DBG3("nc_stream_seek_long: Guess EOF\n");
	    s->eof=1;
	}
	return 1;
    }
  }
  
  MSG_V("stream_seek: WARNING! Can't seek to 0x%llX !\n",(long long)(pos+newpos));
  return 0;

}

void __FASTCALL__ nc_stream_reset(stream_t *s){
  if(s->eof){
    s->pos=0;
    s->eof=0;
  }
}

stream_t* __FASTCALL__ new_memory_stream(const unsigned char* data,int len){
  stream_t *s=malloc(sizeof(stream_t)+len);
  if(s==NULL) return NULL;
  memset(s,0,sizeof(stream_t));
  s->fd=-1;
  s->type=STREAMTYPE_MEMORY;
  s->buf_pos=0; s->buf_len=len;
  s->start_pos=0; s->end_pos=len;
  s->sector_size=1;
  s->buffer=malloc(len);
  if(s->buffer==NULL) { free(s); return NULL; }
  stream_reset(s);
  s->pos=len;
  memcpy(s->buffer,data,len);
  return s;
}

stream_t* __FASTCALL__ new_stream(int type){
  stream_t *s=malloc(sizeof(stream_t));
  if(s==NULL) return NULL;
  memset(s,0,sizeof(stream_t));
  
  s->fd=-1;
  s->type=type;
  s->sector_size=STREAM_BUFFER_SIZE;
  s->buffer=malloc(STREAM_BUFFER_SIZE);
  if(s->buffer==NULL) { free(s); return NULL; }
  stream_reset(s);
  return s;
}

void __FASTCALL__ free_stream(stream_t *s){
  MSG_INFO("\n*** free_stream(drv:%s) called [errno: %s]***\n",s->driver->mrl,s->_Errno);
  if(s->cache_data) stream_disable_cache(s);
  if(s->driver) s->driver->close(s);
  free(s->buffer);
  free(s);
}

stream_t* __FASTCALL__ new_ds_stream(demux_stream_t *ds) {
  stream_t* s = new_stream(STREAMTYPE_DS);
  s->driver=ds->demuxer->stream->driver;
  s->priv = ds;
  return s;
}

int __FASTCALL__ nc_stream_read_char(stream_t *s)
{
    unsigned char retval;
    nc_stream_read(s,&retval,1);
    return stream_eof(s)?-256:retval;
}

int __FASTCALL__ nc_stream_read(stream_t *s,any_t* _mem,int total){
  int i,x,ilen,_total=total,got_len;
  char *mem=_mem;
  MSG_DBG3( "nc_stream_read  %u bytes from %llu\n",total,FILE_POS(s)+s->buf_pos);
  if(stream_eof(s)) return 0;
  x=s->buf_len-s->buf_pos;
  if(x>0) 
  {
    ilen=min(_total,x);
    memcpy(mem,&s->buffer[s->buf_pos],ilen);
    MSG_DBG3("nc_stream_read:  copy prefetched %u bytes\n",ilen);
    s->buf_pos+=ilen;
    mem+=ilen; _total-=ilen;
  }
  ilen=_total;
  ilen /= s->sector_size;
  ilen *= s->sector_size;
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
	mem += min(rlen,(int)s->buf_len);
	tile=s->buf_len-rlen;
	rlen -= min(rlen,(int)s->buf_len);
	got_len += min(rlen,(int)s->buf_len);
	eof=stream_eof(s);
	if(eof) break;
	stat++;
    }
    s->buffer=smem;
    s->buf_len=0;
    s->buf_pos=0;
    ilen += rlen;
    MSG_DBG2("nc_stream_read  got %u bytes directly for %u calls\n",got_len,stat);
    if(tile && !eof)
    {
	/* should never happen. Store data back to native cache! */
	MSG_DBG3("nc_stream_read:  we have tile %u bytes\n",tile);
	s->buf_pos=0;
	memcpy(s->buffer,&mem[s->buf_len-tile],min(STREAM_BUFFER_SIZE,tile));
	s->buf_len=min(STREAM_BUFFER_SIZE,tile);
    }
  }
  ilen=_total-ilen;
  if(stream_eof(s)) return got_len;
  while(ilen){
    int x;
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
  for(i=0;i<min(8,total);i++) MSG_DBG3("%02X ",(int)((unsigned char)mem[i]));
  MSG_DBG3("\n");
  return total;
}

off_t __FASTCALL__ nc_stream_tell(stream_t *s){
  off_t retval;
  retval = FILE_POS(s)+s->buf_pos;
  return retval;
}

int __FASTCALL__ nc_stream_seek(stream_t *s,off_t pos){

  MSG_DBG3( "nc_stream_seek to %llu\n",(long long)pos);
  if(s->type&STREAMTYPE_MEMORY)
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
  return (s->type&STREAMTYPE_SEEKABLE)?nc_stream_seek_long(s,pos):pos;
}

int __FASTCALL__ nc_stream_skip(stream_t *s,off_t len){
  if(len<0 || (len>2*STREAM_BUFFER_SIZE && s->type&STREAMTYPE_SEEKABLE)){
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

