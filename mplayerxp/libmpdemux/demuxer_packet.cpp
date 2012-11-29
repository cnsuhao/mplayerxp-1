#include "demuxer_packet.h"
#include "osdep/fastmemcpy.h"
#include <string.h>

namespace mpxp {
Demuxer_Packet::Demuxer_Packet(unsigned len)
	    :type(Demuxer_Packet::Generic),
	    pts(0),
	    pos(0),
	    flags(DP_NONKEYFRAME),
	    lang_id(0),
	    next(NULL)
{
  _len=len;
  _buf=new uint8_t [_len];
}

Demuxer_Packet::~Demuxer_Packet(){
    if(_buf) delete _buf;
}

void Demuxer_Packet::resize(unsigned newlen)
{
    if(_len!=newlen) {
	if(newlen) _buf=(uint8_t *)mp_realloc(_buf,newlen);
	else {
	    if(_buf) delete _buf;
	    _buf=NULL;
	}
	_len=newlen;
    }
}

Demuxer_Packet* Demuxer_Packet::clone() const {
  Demuxer_Packet* dp=new Demuxer_Packet(_len);
  dp->pts=pts;
  dp->pos=pos;
  dp->flags=flags;
  dp->next=next;
  memcpy(dp->buffer(),_buf,_len);
  return dp;
}
} // namespace mpxp
