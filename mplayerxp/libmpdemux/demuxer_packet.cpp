#include "demuxer_packet.h"
#include "osdep/fastmemcpy.h"
#include <string.h>

Demux_Packet::Demux_Packet(unsigned _len)
	    :pts(0),
	    pos(0),
	    flags(0),
	    next(NULL)
{
  len=_len;
  buffer=new unsigned char [len];
}

Demux_Packet::~Demux_Packet(){
    if(buffer) delete buffer;
}

void Demux_Packet::resize(unsigned newlen)
{
    if(len!=newlen) {
	if(newlen) buffer=(unsigned char *)mp_realloc(buffer,newlen);
	else {
	    if(buffer) delete buffer;
	    buffer=NULL;
	}
	len=newlen;
    }
}

Demux_Packet* Demux_Packet::clone() const {
  Demux_Packet* dp=new Demux_Packet(len);
  dp->pts=pts;
  dp->pos=pos;
  dp->flags=flags;
  dp->next=next;
  memcpy(dp->buffer,buffer,len);
  return dp;
}

