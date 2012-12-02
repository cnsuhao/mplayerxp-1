#include "mp_config.h"
#include "osdep/mplib.h"
using namespace mpxp;

#include "demuxer_stream.h"
#include "demuxer.h"
#include "demuxer_internal.h"
#include "help_mp.h"
#include "demux_msg.h"

namespace mpxp {
Demuxer_Stream::~Demuxer_Stream(){
    free_packs();
}

Demuxer_Stream::Demuxer_Stream(demuxer_t *_demuxer,int _id)
{
    fill_false_pointers(antiviral_hole,reinterpret_cast<long>(&pin)-reinterpret_cast<long>(&antiviral_hole));
    pin=DS_PIN;
    _buffer_pos=_buffer_size=0;
    _buffer=NULL;
    pts=0;
    _pts_bytes=0;
    eof=0;
    pos=0;
    dpos=0;
    pack_no=0;
//---------------
    _packs=0;
    _bytes=0;
    _first=_last=_current=NULL;
    id=_id;
    demuxer=_demuxer;
//----------------
    asf_seq=-1;
    asf_packet=NULL;
//----------------
    sh=NULL;
    pts_flags=0;
    prev_pts=pts_corr=0;
}

#if 1
int Demuxer_Stream::getch() {
    return
	(_buffer_pos<_buffer_size) ? _buffer[_buffer_pos++]:
	((!fill_buffer())? (-1) : _buffer[_buffer_pos++]);
}
#else
int Demuxer_Stream::getch(){
  if(_buffer_pos>=_buffer_size){
    if(!fill_buffer()){
      return -1; // EOF
    }
  }
  return _buffer[_buffer_pos++];
}
#endif

void Demuxer_Stream::add_packet(Demuxer_Packet* dp){
    if(dp->length()>0) {
	++_packs;
	_bytes+=dp->length();
	if(_last) {
	    // next packet in stream
	    _last->next=dp;
	    _last=dp;
	} else {
	    // _first packet in stream
	    _first=_last=dp;
	}
	MSG_DBG2("DEMUX: Append packet: len=%d  pts=%5.3f  pos=%u  [_packs: A=%d V=%d]\n",
	    dp->length(),dp->pts,(unsigned int)dp->pos,demuxer->audio->_packs,demuxer->video->_packs);
    }
    else
	MSG_DBG2("DEMUX: Skip packet: len=%d  pts=%5.3f  pos=%u  [_packs: A=%d V=%d]\n",
	    dp->length(),dp->pts,(unsigned int)dp->pos,demuxer->audio->_packs,demuxer->video->_packs);
}

void Demuxer_Stream::read_packet(stream_t *stream,int len,float _pts,off_t _pos,dp_flags_e _flags){
    Demuxer_Packet* dp=new(zeromem) Demuxer_Packet(len);
    len=stream_read(stream,dp->buffer(),len);
    dp->resize(len);
    dp->pts=_pts; //(float)pts/90000.0f;
    dp->pos=_pos;
    dp->flags=_flags;
    // append packet to DS stream:
    add_packet(dp);
    MSG_DBG2("ds_read_packet(%u,%f,%llu,%i)\n",len,pts,pos,flags);
}

// return value:
//     0 = EOF
//     1 = succesfull
int Demuxer_Stream::fill_buffer() {
    if (_buffer) delete _buffer;
/*  free_packs(ds); */
    if(mp_conf.verbose>2) {
	if(this==demuxer->audio) MSG_DBG3("ds_fill_buffer(d_audio) called\n");
	else if(this==demuxer->video) MSG_DBG3("ds_fill_buffer(d_video) called\n");
	else if(this==demuxer->sub) MSG_DBG3("ds_fill_buffer(d_sub) called\n");
	else MSG_DBG3("ds_fill_buffer(unknown %p) called\n",this);
    }
    while(1){
	if(_packs){
	    Demuxer_Packet *p=_first;
	    // copy useful data:
	    _buffer=p->buffer();
	    _buffer_pos=0;
	    _buffer_size=p->length();
	    pos=p->pos;
	    dpos+=p->length(); // !!!
	    ++pack_no;
	    if(p->pts){
		pts=p->pts;
		_pts_bytes=0;
	    }
	    _pts_bytes+=p->length(); // !!!
	    flags=p->flags;
	    // mp_free packet:
	    _bytes-=p->length();
	    _current=p;
	    _first=p->next;
	    if(!_first) _last=NULL;
	    --_packs;
	    check_pin("demuxer",pin,DS_PIN);
	    return 1; //ds->_buffer_size;
	}
	if(demuxer->audio->_bytes>=MAX_PACK_BYTES){
	    MSG_ERR(MSGTR_TooManyAudioInBuffer,demuxer->audio->_packs,demuxer->audio->_bytes);
	    MSG_HINT(MSGTR_MaybeNI);
	    break;
	}
	if(demuxer->video->_bytes>=MAX_PACK_BYTES){
	    MSG_ERR(MSGTR_TooManyVideoInBuffer,demuxer->video->_packs,demuxer->video->_bytes);
	    MSG_HINT(MSGTR_MaybeNI);
	    break;
	}
	if(!demuxer->driver){
	     MSG_DBG2("ds_fill_buffer: demux->driver==NULL failed\n");
	    break; // EOF
	}
	if(!demuxer->driver->demux(demuxer,this)){
	    MSG_DBG2("ds_fill_buffer: demux->driver->demux() failed\n");
	    break; // EOF
	}
    }
    _buffer_pos=_buffer_size=0;
    _buffer=NULL;
    _current=NULL;
    MSG_V("ds_fill_buffer: EOF reached (stream: %s)  \n",this==demuxer->audio?"audio":"video");
    eof=1;
    check_pin("demuxer",pin,DS_PIN);
    return 0;
}

int Demuxer_Stream::read_data(unsigned char* mem,int len) {
    int x;
    int __bytes=0;
    while(len>0){
	x=_buffer_size-_buffer_pos;
	if(x==0){
	    if(!fill_buffer()) return __bytes;
	} else {
	    if(x>len) x=len;
	    if(x<0) return __bytes; /* BAD!!! sometime happens. Broken stream, driver, gcc ??? */
	    if(mem) memcpy(mem+__bytes,&_buffer[_buffer_pos],x);
	    __bytes+=x;len-=x;_buffer_pos+=x;
	}
    }
    return __bytes;
}

void Demuxer_Stream::free_packs() {
    Demuxer_Packet *dp=_first;
    while(dp) {
	Demuxer_Packet *dn=dp->next;
	delete dp;
	dp=dn;
    }
    if(asf_packet) {
	// mp_free unfinished .asf fragments:
	delete asf_packet;
	asf_packet=NULL;
    }
    _first=_last=NULL;
    _packs=0; // !!!!!
    _bytes=0;
    if(_current) delete _current;
    _current=NULL;
    _buffer=NULL;
    _buffer_pos=_buffer_size;
    pts=0;
    _pts_bytes=0;
}

void Demuxer_Stream::free_packs_until_pts(float _pts) {
    Demuxer_Packet *dp=_first;
    unsigned __packs,__bytes;
    __packs=__bytes=0;
    while(dp) {
	Demuxer_Packet *dn=dp->next;
	if(dp->pts >= _pts) break;
	__packs++;
	__bytes+=dp->length();
	delete dp;
	dp=dn;
    }
    if(!dp) {
	if(asf_packet){
	    // mp_free unfinished .asf fragments:
	    delete asf_packet;
	    asf_packet=NULL;
	}
	_first=_last=NULL;
	_packs=0; // !!!!!
	_bytes=0;
	pts=0;
    } else {
	_first=dp;
	_packs-=__packs;
	_bytes-=__bytes;
	pts=dp->pts;
    }
    if(_current) delete _current;
    _current=NULL;
    _buffer=NULL;
    _buffer_pos=_buffer_size;
    _pts_bytes=0;
}

int Demuxer_Stream::get_packet(unsigned char **start){
    while(1){
	int len;
	if(_buffer_pos>=_buffer_size){
	  if(!fill_buffer()){
	    // EOF
	    *start = NULL;
	    return -1;
	  }
	}
	len=_buffer_size-_buffer_pos;
	*start = &_buffer[_buffer_pos];
	_buffer_pos+=len;
	return len;
    }
}

int Demuxer_Stream::get_packet_sub(unsigned char **start){
    while(1){
	int len;
	if(_buffer_pos>=_buffer_size){
	  *start = NULL;
	  if(!_packs) return -1; // no sub
	  if(!fill_buffer()) return -1; // EOF
	}
	len=_buffer_size-_buffer_pos;
	*start = &_buffer[_buffer_pos];
	_buffer_pos+=len;
	return len;
    }
}

float Demuxer_Stream::get_next_pts() {
    while(!_first) {
	if(demuxer->audio->_bytes>=MAX_PACK_BYTES){
	    MSG_ERR(MSGTR_TooManyAudioInBuffer,demuxer->audio->_packs,demuxer->audio->_bytes);
	    MSG_HINT(MSGTR_MaybeNI);
	    return -1;
	}
	if(demuxer->video->_bytes>=MAX_PACK_BYTES){
	    MSG_ERR(MSGTR_TooManyVideoInBuffer,demuxer->video->_packs,demuxer->video->_bytes);
	    MSG_HINT(MSGTR_MaybeNI);
	    return -1;
	}
	if(!demux_fill_buffer(demuxer,this)) return -1;
    }
    return _first->pts;
}

void Demuxer_Stream::buffer_roll_back(int x) { _buffer_pos-=x; }

} // namespace mpxp
