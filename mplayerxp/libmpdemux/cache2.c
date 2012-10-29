#include "../mp_config.h"

#define READ_USLEEP_TIME 10000
#define FILL_USLEEP_TIME 50000
#define PREFILL_SLEEP_TIME 200

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <limits.h>
#include <unistd.h>
#include <pthread.h>

#include "stream.h"
#include "../sig_hand.h"
#include "../osdep/timer.h"
#include "../cpudetect.h"
#include "bswap.h"
#include "../libvo/fastmemcpy.h"
#include "../help_mp.h"
#include "mpdemux.h"
#include "../mplayer.h"
#include "demux_msg.h"

#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif

#define CPF_EMPTY	0x00000001UL
#define CPF_EOF		0x80000000UL
#define CPF_DONE	0x40000000UL /* special case for dvd packets to exclude them from sending again */
typedef struct cache_packet_s
{
    off_t filepos;  /* some nav-packets have length so we need to know real pos of data packet */
    unsigned state; /* consists from of CPF_* */
    stream_packet_t sp;
    pthread_mutex_t cp_mutex;
}cache_packet_t;

typedef struct {
  unsigned first;	/* index of the first packet */
  unsigned last;	/* index of the last packet */
  unsigned buffer_size; /* size of the allocated buffer memory (for statistic only) */
  unsigned sector_size; /* size of a single sector (2048/2324) */
  unsigned npackets;	/* number of packets in cache */
  int back_size;	/* for backward seek */
  int prefill;		/* min prefill bytes if cache is empty. TODO: remove */
  int eof;
  /* reader's pointers: */
  off_t read_filepos;
  stream_t* stream; /* parent stream */
  /* thread related stuff */
  int in_fill;
  pthread_mutex_t mutex;
  mpxp_thread_t*  pth;
  /* for optimization: */
  cache_packet_t *packets;
  char * mem;
} cache_vars_t;

#if 0
#define MSG_CH MSG_V
#else
#define MSG_CH(...)
#endif

#define CACHE2_LOCK(cv) { MSG_CH("CACHE2_LOCK\n"); pthread_mutex_lock(&cv->mutex); }
#define CACHE2_UNLOCK(cv) { MSG_CH("CACHE2_UNLOCK\n");pthread_mutex_unlock(&cv->mutex); }

#define CACHE2_TLOCK(cv) { MSG_CH("CACHE2_TLOCK\n"); pthread_mutex_lock(&cv->mutex); }
#define CACHE2_TUNLOCK(cv) { MSG_CH("CACHE2_TUNLOCK\n");pthread_mutex_unlock(&cv->mutex); }

#define CACHE2_PACKET_LOCK(cidx) { MSG_CH("CACHE2_PACKET_LOCK\n"); pthread_mutex_lock(&c->packets[cidx].cp_mutex); }
#define CACHE2_PACKET_UNLOCK(cidx) { MSG_CH("CACHE2_PACKET_UNLOCK\n");pthread_mutex_unlock(&c->packets[cidx].cp_mutex); }

#define CACHE2_PACKET_TLOCK(cidx) { MSG_CH("CACHE2_PACKET_TLOCK\n"); pthread_mutex_lock(&c->packets[cidx].cp_mutex); }
#define CACHE2_PACKET_TUNLOCK(cidx) { MSG_CH("CACHE2_PACKET_TUNLOCK\n");pthread_mutex_unlock(&c->packets[cidx].cp_mutex); }

#define START_FILEPOS(c) ((c)->packets[(c)->first].filepos)
#define END_FILEPOS(c) ((c)->packets[(c)->last].filepos+(c)->packets[(c)->last].sp.len)
#define CP_NEXT(c,idx) (((idx)+1)%(c)->npackets)

#ifdef __i386__
#define COREDUMP() { __asm __volatile(".short 0xffff":::"memory"); }
#else
#define COREDUMP()
#endif

#if 0
#define C2_ASSERT(cond) if(cond) { MSG_FATAL("internal error at cache2.c:%u: (%s)\n",__LINE__,#cond); COREDUMP(); }
#else
#define C2_ASSERT(cond) if(cond) MSG_FATAL("internal error at cache2.c:%u: (%s)\n",__LINE__,#cond);
#endif

static int __FASTCALL__ c2_cache_fill(cache_vars_t* c){
  int len,in_cache,legacy_eof,seek_eof;
  off_t readpos,new_start;
  unsigned cidx,cp;

  CACHE2_TLOCK(c);
  readpos=c->read_filepos;
  in_cache=(readpos>=START_FILEPOS(c)&&readpos<END_FILEPOS(c));
  new_start = readpos - c->back_size;
  if(new_start<c->stream->start_pos) new_start=c->stream->start_pos;
  seek_eof=0;
  if(!in_cache && c->stream->type&STREAMTYPE_SEEKABLE)
  {
	/* seeking... */
	MSG_DBG2("Out of boundaries... seeking to %lli {in_cache(%i) %lli<%lli>%lli} \n"
	,new_start,in_cache,START_FILEPOS(c),readpos,END_FILEPOS(c));
	if(c->stream->eof || c->eof) nc_stream_reset(c->stream);
	c->stream->driver->seek(c->stream,new_start);
	if(c->stream->_Errno) { MSG_WARN("c2_seek(drv:%s) error: %s\n",c->stream->driver->mrl,strerror(c->stream->_Errno)); c->stream->_Errno=0; }
	if((c->packets[c->first].filepos=c->stream->driver->tell(c->stream))<0) seek_eof=1;
	c->last=c->first;
	if(c->packets[c->first].filepos < new_start-(off_t)c->stream->sector_size)
	    MSG_WARN("CACHE2: found wrong offset after seeking %lli (wanted: %lli)\n",c->packets[c->first].filepos,new_start);
	MSG_DBG2("Seek done. new pos: %lli\n",START_FILEPOS(c));
  }
  else
  {
    /* find new start of buffer according on readpos */
    cidx=c->first;
    do
    {
	if((new_start>=c->packets[cidx].filepos&&new_start<c->packets[cidx].filepos+c->packets[cidx].sp.len)
	   && !c->packets[cidx].sp.type) break;
	cidx=CP_NEXT(c,cidx);
    }while(cidx!=c->first);
    MSG_DBG2("CACHE2: Assigning first as %p for %lli\n",c->first,START_FILEPOS(c));
    c->first=cidx;
  }
  CACHE2_TUNLOCK(c);
  if(CP_NEXT(c,c->last) == c->first || c->eof)
  {
    MSG_DBG2("CACHE2: cache full\n");
    return 0; /* cache full */
  }
  len=0;
  cp=cidx=c->last==c->first?c->first:CP_NEXT(c,c->last);
  do { CACHE2_PACKET_TLOCK(cidx); c->packets[cidx].state|=CPF_EMPTY; CACHE2_PACKET_TUNLOCK(cidx); cidx=CP_NEXT(c,cidx); } while(cidx!=c->first);
  cidx=cp;
  c->in_fill=1;
  while(1)
  {
    CACHE2_PACKET_TLOCK(cidx);
    c->packets[cidx].sp.len=c->sector_size;
    c->packets[cidx].filepos = c->stream->driver->tell(c->stream);
    c->stream->driver->read(c->stream,&c->packets[cidx].sp);
    MSG_DBG2("CACHE2: read_packet at %lli (wanted %u got %u type %i)",c->packets[cidx].filepos,c->sector_size,c->packets[cidx].sp.len,c->packets[cidx].sp.type);
    if(mp_conf.verbose>1)
	if(c->packets[cidx].sp.len>8) 
	{ 
	    int i;
	    for(i=0;i<8;i++)
		MSG_DBG2("%02X ",(int)(unsigned char)c->packets[cidx].sp.buf[i]);
	} 
    MSG_DBG2("\n");
    if(c->stream->driver->control(c->stream,SCTRL_EOF,NULL)==SCTRL_OK) legacy_eof=1;
    else	legacy_eof=0;
    if(c->packets[cidx].sp.len < 0 || (c->packets[cidx].sp.len == 0 && c->packets[cidx].sp.type == 0) || legacy_eof || seek_eof)
    {
	/* EOF */
	MSG_DBG2("CACHE2: guess EOF: %lli %lli\n",START_FILEPOS(c),END_FILEPOS(c));
	c->packets[cidx].state|=CPF_EOF;
	c->eof=1;
	c->packets[cidx].state&=~CPF_EMPTY;
	c->packets[cidx].state&=~CPF_DONE;
	if(c->stream->_Errno) { MSG_WARN("c2_fill_buffer(drv:%s) error: %s\n",c->stream->driver->mrl,strerror(c->stream->_Errno)); c->stream->_Errno=0; }
	CACHE2_PACKET_TUNLOCK(cidx);
	break;
    }
    if(c->packets[cidx].sp.type == 0) len += c->packets[cidx].sp.len;
    c->last=cidx;
    c->packets[cidx].state&=~CPF_EMPTY;
    c->packets[cidx].state&=~CPF_DONE;
    CACHE2_PACKET_TUNLOCK(cidx);
    cidx=CP_NEXT(c,cidx);
    MSG_DBG2("CACHE2: start=%lli end_filepos = %lli\n",START_FILEPOS(c),END_FILEPOS(c));
    if(cidx==c->first)
    {
	MSG_DBG2("CACHE2: end of queue is reached: %p\n",c->first);
	break;
    }
    CACHE2_TUNLOCK(c);
  }
  c->in_fill=0;
  MSG_DBG2("CACHE2: totally got %u bytes\n",len);
  return len;
}

static cache_vars_t* __FASTCALL__  c2_cache_init(int size,int sector){
  pthread_mutex_t tmpl=PTHREAD_MUTEX_INITIALIZER;
  cache_vars_t* c=malloc(sizeof(cache_vars_t));
  char *pmem;
  unsigned i,num;
  memset(c,0,sizeof(cache_vars_t));
  c->npackets=num=size/sector;
  /* collection of all c2_packets in continuous memory area minimizes cache pollution
     and speedups cache as C+D=3.27% instead of 4.77% */
  i=sizeof(cache_packet_t)*num;
  c->packets=malloc(i);
  c->mem=malloc(num*sector);
  if(!c->packets || !c->mem)
  {
    MSG_ERR(MSGTR_OutOfMemory);
    free(c);
    return 0;
  }
  memset(c->packets,0,i);
  pmem = c->mem;
  MSG_DBG2("For cache navigation was allocated %u bytes as %u packets (%u/%u)\n",i,num,size,sector);
  c->first=c->last=0;
  for(i=0;i<num;i++)
  {
    c->packets[i].sp.buf=pmem;
    c->packets[i].state|=CPF_EMPTY;
    memcpy(&c->packets[i].cp_mutex,&tmpl,sizeof(tmpl));
    pmem += sector;
  }
  if(mp_conf.verbose>1)
  for(i=0;i<num;i++)
  {
    MSG_DBG2("sizeof(c)=%u c=%i c->sp.buf=%p\n",sizeof(cache_packet_t),i,c->packets[i].sp.buf);
  }
  c->buffer_size=num*sector;
  c->sector_size=sector;
  c->back_size=size/4; /* default 25% */
  c->prefill=size/20;
  memcpy(&c->mutex,&tmpl,sizeof(tmpl));
  return c;
}

static void sig_cache2( void )
{
    MSG_V("cache2 segfault\n");
    mp_msg_flush();
    xmp_killall_threads(pthread_self());
    __exit_sighandler();
}

static int was_killed=0;
static void stream_unlink_cache(int force)
{
  if(force) was_killed=1;
}

static any_t*cache2_routine(any_t*arg)
{
    mpxp_thread_t* priv=arg;

    double tt;
    unsigned int t=0;
    unsigned int t2;
    int cfill;
    cache_vars_t* c=(cache_vars_t*)arg;

    priv->state=Pth_Run;
    priv->pid = getpid();

    while(1) {
	if(mp_conf.benchmark) t=GetTimer();
	cfill=c2_cache_fill(c);
	if(mp_conf.benchmark) {
	    t2=GetTimer();t=t2-t;
	    tt = t*0.000001f;
	    mp_data->bench->c2+=tt;
	    if(tt > mp_data->bench->max_c2) mp_data->bench->max_c2=tt;
	    if(tt < mp_data->bench->min_c2) mp_data->bench->min_c2=tt;
	}
	if(!cfill) usleep(FILL_USLEEP_TIME); // idle
	if(priv->state==Pth_Canceling) break;
    }
    priv->state=Pth_Stand;
    return arg;
}

int stream_enable_cache(stream_t *stream,int size,int _min,int prefill){
  int ss=stream->sector_size>1?stream->sector_size:STREAM_BUFFER_SIZE;
  cache_vars_t* c;

  if (!(stream->type&STREAMTYPE_SEEKABLE) && stream->fd < 0) {
    // The stream has no 'fd' behind it, so is non-cacheable
    MSG_WARN("\rThis stream is non-cacheable\n");
    return 1;
  }

  if(size<32*1024) size=32*1024; // 32kb min
  c=c2_cache_init(size,ss);
  stream->cache_data=c;
  if(!c) return 0;
  c->stream=stream;
  c->prefill=size*prefill;
  c->read_filepos=stream->start_pos;

  unsigned rc;
  if((rc=xmp_register_thread(NULL,sig_cache2,cache2_routine,"cache2"))==UINT_MAX) return 0;
  c->pth=&xp_core.mpxp_threads[rc];
  // wait until cache is filled at least prefill_init %
  MSG_V("CACHE_PRE_INIT: %lld [%lld] %lld  pre:%d  eof:%d SS=%u \n",
	START_FILEPOS(c),c->read_filepos,END_FILEPOS(c),_min,c->eof,ss);
  while((c->read_filepos<START_FILEPOS(c) || END_FILEPOS(c)-c->read_filepos<_min)
	&& !c->eof && CP_NEXT(c,c->last)!=c->first){
	if(!(stream->type&STREAMTYPE_SEEKABLE))
	MSG_STATUS("\rCache fill: %5.2f%% (%d bytes)    ",
	    100.0*(float)(END_FILEPOS(c)-c->read_filepos)/(float)(c->buffer_size),
	    END_FILEPOS(c)-c->read_filepos);
	else
	MSG_V("\rCache fill: %5.2f%% (%d bytes)    ",
	    100.0*(float)(END_FILEPOS(c)-c->read_filepos)/(float)(c->buffer_size),
	    END_FILEPOS(c)-c->read_filepos);
	if(c->eof) break; // file is smaller than prefill size
	if(mpdemux_check_interrupt(PREFILL_SLEEP_TIME))
	  return 0;
    }
    MSG_STATUS("cache info: size=%u min=%u prefill=%u\n",size,_min,prefill);
    return 1; // parent exits
}

void stream_disable_cache(stream_t *st)
{
  cache_vars_t* c;
  c=st->cache_data;
  if(c) {
    if(c->pth && c->pth->state==Pth_Run) {
	c->pth->state=Pth_Canceling;
	while(c->pth->state==Pth_Canceling && !was_killed) usleep(0);
    }
    free(c->packets);
    free(c->mem);
    free(c);
  }
}

static int __FASTCALL__ c2_stream_fill_buffer(cache_vars_t* c)
{
  MSG_DBG2( "c2_stream_fill_buffer\n");
  if(c->eof) return 0;
  while(c->read_filepos>=END_FILEPOS(c) || c->read_filepos<START_FILEPOS(c))
  {
	if(c->eof) break;
	usleep(READ_USLEEP_TIME); // 10ms
	MSG_DBG2("Waiting for %lli in %lli %lli\n",c->read_filepos,START_FILEPOS(c),END_FILEPOS(c));
	continue; // try again...
  }
  return c->eof?0:1;
}

static void __FASTCALL__ c2_stream_reset(cache_vars_t* c)
{
    unsigned cidx;
    int was_eof;
    MSG_DBG2("c2_stream_reset\n");
    nc_stream_reset(c->stream);
    cidx=c->first;
    was_eof=0;
    do{ was_eof |= (c->packets[cidx].state&CPF_EOF); c->packets[cidx].state&=~CPF_EOF; cidx=CP_NEXT(c,cidx); }while(cidx!=c->first);
    c->eof=0;    
    if(was_eof)
    {
	cidx=c->first;
	do{ c->packets[cidx].state|=CPF_EMPTY; cidx=CP_NEXT(c,cidx); }while(cidx!=c->first);
	c->last=c->first;
	c->read_filepos=c->stream->start_pos;
	c->stream->driver->seek(c->stream,c->read_filepos);
    }
}

static int __FASTCALL__ c2_stream_seek_long(cache_vars_t* c,off_t pos){
  
  MSG_DBG2("CACHE2_SEEK: %lli,%lli,%lli <> %lli\n",START_FILEPOS(c),c->read_filepos,END_FILEPOS(c),pos);
  if(pos<0/* || pos>END_FILEPOS(c)*/) { c->eof=1; return 0; }
  while(c->in_fill) usleep(0);
  CACHE2_LOCK(c);  
  if(c->eof) c2_stream_reset(c);
  C2_ASSERT(pos < c->stream->start_pos);
  c->read_filepos=pos;
  CACHE2_UNLOCK(c);
  c2_stream_fill_buffer(c);
  return c->eof?pos<END_FILEPOS(c)?1:0:1;
}

static unsigned __FASTCALL__ c2_find_packet(cache_vars_t* c,off_t pos)
{
    unsigned retval;
    CACHE2_LOCK(c);
    retval = c->first;
    CACHE2_UNLOCK(c);
    while(1)
    {
	CACHE2_PACKET_LOCK(retval);
	while(c->packets[retval].state&CPF_EMPTY) { CACHE2_PACKET_UNLOCK(retval); usleep(0); CACHE2_PACKET_LOCK(retval); }
	if((pos >= c->packets[retval].filepos && 
	    pos < c->packets[retval].filepos+c->packets[retval].sp.len && 
	    !c->packets[retval].sp.type) ||
	    (c->packets[retval].state&CPF_EOF))
			break; /* packet is locked */
	if(c->packets[retval].sp.type && 
	   !(c->packets[retval].state&CPF_DONE) && 
	   c->stream->event_handler)
	{
	   c->stream->event_handler(c->stream,&c->packets[retval].sp);
	   c->packets[retval].state|=CPF_DONE;
	}
	CACHE2_PACKET_UNLOCK(retval);
	CACHE2_LOCK(c);
	retval=CP_NEXT(c,retval);
	if(retval==c->first)
	{
	    MSG_DBG2("Can't find packet for offset %lli\n",pos);
	    CACHE2_UNLOCK(c);
	    return UINT_MAX;
	}
	CACHE2_UNLOCK(c);
    }
    return retval;
}

/* try return maximal continious memory to copy (speedups C+D on 30%)*/
static void __FASTCALL__ c2_get_continious_mem(cache_vars_t* c,unsigned cidx,int *len,unsigned *npackets)
{
    *npackets=1;
    *len=c->packets[cidx].sp.len;
    if(cidx != UINT_MAX)
    {
	unsigned i;
	int req_len;
	req_len = *len;
	for(i=cidx+1;i<c->npackets;i++)
	{
	    if(	*len >= req_len ||
		c->packets[i].sp.type ||
		c->packets[i].sp.len<0 ||
		c->packets[i].state&CPF_EMPTY ||
		(c->packets[i].sp.len==0 && c->packets[i].sp.type)) break;
	    CACHE2_PACKET_LOCK(i);
	    *len += c->packets[i].sp.len;
	    (*npackets)++;
	}
    }
}

static unsigned __FASTCALL__ c2_wait_packet(cache_vars_t* c,off_t pos,int *len,unsigned *npackets)
{
  unsigned cidx;
  while(1)
  {
    cidx = c2_find_packet(c,pos);
    if(cidx!=UINT_MAX || c->eof) break;
    if(cidx!=UINT_MAX) CACHE2_PACKET_UNLOCK(cidx);
    c2_stream_fill_buffer(c);
  }
  c2_get_continious_mem(c,cidx,len,npackets);
  return cidx;
}

static unsigned c2_next_packet(cache_vars_t* c,unsigned cidx,int *len,unsigned *npackets)
{
    MSG_DBG2("next_packet: start=%p cur=%i\n",c->first,cidx);
    while(1)
    {
	CACHE2_LOCK(c);
	cidx=CP_NEXT(c,cidx);
	CACHE2_UNLOCK(c);
	CACHE2_PACKET_LOCK(cidx);
	while(c->packets[cidx].state&CPF_EMPTY) { CACHE2_PACKET_UNLOCK(cidx); usleep(0); CACHE2_PACKET_LOCK(cidx); }
	if(cidx==c->first)
	{
	    CACHE2_PACKET_UNLOCK(cidx);
	    c2_stream_fill_buffer(c);
	    cidx = c2_find_packet(c,c->read_filepos);
	    break;
	}
	if(!c->packets[cidx].sp.type) break; /* packet is locked */
	if(!(c->packets[cidx].state&CPF_DONE) && c->stream->event_handler)
	{
	    c->stream->event_handler(c->stream,&c->packets[cidx].sp);
	    c->packets[cidx].state|=CPF_DONE;
	}
	CACHE2_PACKET_UNLOCK(cidx);
    }
    c2_get_continious_mem(c,cidx,len,npackets);
    MSG_DBG2("next_packet: rp: %lli fp: %lli len %lu type %i\n",c->read_filepos,c->packets[cidx].filepos,c->packets[cidx].sp.len,c->packets[cidx].sp.type);
    return cidx;
}

static int __FASTCALL__ c2_stream_read(cache_vars_t* c,char* _mem,int total){
  int len=total,eof,mlen;
  char *mem=_mem;
  unsigned buf_pos;
  unsigned cur,i,npackets;
  cur=c2_wait_packet(c,c->read_filepos,&mlen,&npackets);
  eof = cur!=UINT_MAX?((int)(c->packets[cur].state&CPF_EOF)):c->eof;
  if(cur==UINT_MAX||eof) { if(cur!=UINT_MAX) CACHE2_PACKET_UNLOCK(cur); return 0; }
  MSG_DBG2( "c2_stream_read  %i bytes from %lli\n",total,c->read_filepos);
  while(len){
    int x;
    if(c->read_filepos>=c->packets[cur].filepos+mlen){
	for(i=0;i<npackets;i++) CACHE2_PACKET_UNLOCK(cur+i);
	mlen=len;
	cur=c2_next_packet(c,cur,&mlen,&npackets);
	eof = cur!=UINT_MAX?(c->packets[cur].state&CPF_EOF):1;
	if(eof)
	{
	    CACHE2_PACKET_UNLOCK(cur);
	    return total-len; // EOF
	}
    }
    buf_pos=c->read_filepos-c->packets[cur].filepos;
    x=mlen-buf_pos;
    C2_ASSERT(buf_pos>=(unsigned)mlen);
    if(x>len) x=len;
    if(!x) MSG_WARN( "c2_read: dead-lock\n");
    memcpy(mem,&c->packets[cur].sp.buf[buf_pos],x);
    buf_pos+=x;
    mem+=x; len-=x;
    c->read_filepos+=x;
  }
  CACHE2_PACKET_UNLOCK(cur);
  if(mp_conf.verbose>2)
  {
    int i;
    MSG_DBG2( "c2_stream_read  got %u bytes ",total);
    for(i=0;i<min(8,total);i++) MSG_DBG2("%02X ",(int)((unsigned char)_mem[i]));
    MSG_DBG2("\n");
  }
  return total;
}


inline static off_t c2_stream_tell(cache_vars_t* c){
  return c->read_filepos;
}

static int __FASTCALL__ c2_stream_seek(cache_vars_t* c,off_t pos)
{
  MSG_DBG2( "c2_seek to %lli (%lli %lli) %i\n",(long long)pos,(long long)START_FILEPOS(c),(long long)END_FILEPOS(c),c->first);
  if(pos>=START_FILEPOS(c) && pos < END_FILEPOS(c))
  {
	c->read_filepos=pos;
	return pos;
  }
  return c2_stream_seek_long(c,pos);
}

inline static int __FASTCALL__ c2_stream_skip(cache_vars_t* c,off_t len)
{
    return c2_stream_seek(c,c2_stream_tell(c)+len);
}

static int __FASTCALL__ c2_stream_eof(cache_vars_t*c)
{
    unsigned cur;
    int retval;
    cur = c2_find_packet(c,c->read_filepos);
    if(cur!=UINT_MAX) CACHE2_PACKET_UNLOCK(cur);
    retval = cur!=UINT_MAX?((int)(c->packets[cur].state&CPF_EOF)):c->eof;
    MSG_DBG2("stream_eof: %i\n",retval);
    return retval;
}

static void __FASTCALL__ c2_stream_set_eof(cache_vars_t*c,int eof)
{
    unsigned cur;
    cur = c2_find_packet(c,c->read_filepos);
    if(cur != UINT_MAX)
    {
	if(eof) c->packets[cur].state|=CPF_EOF;
	else	c->packets[cur].state&=~CPF_EOF;
	CACHE2_PACKET_UNLOCK(cur);
    }
    c->eof=eof;
    MSG_DBG2("stream_set_eof: %i\n",eof);
}

/*
    main interface here!
*/
int __FASTCALL__ stream_read(stream_t *s,any_t* _mem,int total)
{
    char *mem = _mem;
    if(s->cache_data)	return c2_stream_read(s->cache_data,mem,total);
    else		return nc_stream_read(s,mem,total);
}

int __FASTCALL__ stream_eof(stream_t *s)
{
    if(s->cache_data) return c2_stream_eof(s->cache_data);
    else return s->eof;
}

void __FASTCALL__ stream_set_eof(stream_t *s,int eof)
{
    if(!eof) stream_reset(s);
    else
    {
	if(s->cache_data)	c2_stream_set_eof(s->cache_data,eof);
	else			s->eof=eof;
    }
}

int __FASTCALL__ stream_read_char(stream_t *s)
{
    if(s->cache_data)
    {
	unsigned char retval;
	c2_stream_read(s->cache_data,&retval,1);
	return stream_eof(s)?-256:retval;
    }
    else return nc_stream_read_char(s);
}

off_t __FASTCALL__ stream_tell(stream_t *s)
{
    if(s->cache_data)	return c2_stream_tell(s->cache_data);
    else		return nc_stream_tell(s);
}

int __FASTCALL__ stream_seek(stream_t *s,off_t pos)
{
    if(s->cache_data)	return c2_stream_seek(s->cache_data,pos);
    else		return nc_stream_seek(s,pos);
}

int __FASTCALL__ stream_skip(stream_t *s,off_t len)
{
    if(s->cache_data)	return c2_stream_skip(s->cache_data,len);
    else		return nc_stream_skip(s,len);
}


void __FASTCALL__ stream_reset(stream_t *s)
{
    if(s->cache_data)	c2_stream_reset(s->cache_data);
    else		nc_stream_reset(s);
}

unsigned int __FASTCALL__ stream_read_word(stream_t *s){
  unsigned short retval;
  stream_read(s,(char *)&retval,2);
  return me2be_16(retval);
}

unsigned int __FASTCALL__ stream_read_dword(stream_t *s){
  unsigned int retval;
  stream_read(s,(char *)&retval,4);
  return me2be_32(retval);
}

uint64_t __FASTCALL__ stream_read_qword(stream_t *s){
  uint64_t retval;
  stream_read(s,(char *)&retval,8);
  return me2be_64(retval);
}

unsigned int __FASTCALL__ stream_read_word_le(stream_t *s){
  unsigned short retval;
  stream_read(s,(char *)&retval,2);
  return me2le_16(retval);
}

unsigned int __FASTCALL__ stream_read_dword_le(stream_t *s){
  unsigned int retval;
  stream_read(s,(char *)&retval,4);
  return me2le_32(retval);
}

uint64_t __FASTCALL__ stream_read_qword_le(stream_t *s){
  uint64_t retval;
  stream_read(s,(char *)&retval,8);
  return me2le_64(retval);
}

unsigned int __FASTCALL__ stream_read_int24(stream_t *s){
  unsigned int y;
  y = stream_read_char(s);
  y=(y<<8)|stream_read_char(s);
  y=(y<<8)|stream_read_char(s);
  return y;
}

