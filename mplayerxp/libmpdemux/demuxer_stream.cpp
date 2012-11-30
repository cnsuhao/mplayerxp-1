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
    rnd_fill(antiviral_hole,reinterpret_cast<long>(&pin)-reinterpret_cast<long>(&antiviral_hole));
    pin=DS_PIN;
    buffer_pos=buffer_size=0;
    buffer=NULL;
    pts=0;
    pts_bytes=0;
    eof=0;
    pos=0;
    dpos=0;
    pack_no=0;
//---------------
    packs=0;
    bytes=0;
    first=last=current=NULL;
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
	(buffer_pos<buffer_size) ? buffer[buffer_pos++]:
	((!fill_buffer())? (-1) : buffer[buffer_pos++]);
}
#else
int Demuxer_Stream::getch(){
  if(buffer_pos>=buffer_size){
    if(!fill_buffer()){
      return -1; // EOF
    }
  }
  return buffer[buffer_pos++];
}
#endif

void Demuxer_Stream::add_packet(Demuxer_Packet* dp){
    if(dp->length()>0) {
	++packs;
	bytes+=dp->length();
	if(last) {
	    // next packet in stream
	    last->next=dp;
	    last=dp;
	} else {
	    // first packet in stream
	    first=last=dp;
	}
	MSG_DBG2("DEMUX: Append packet: len=%d  pts=%5.3f  pos=%u  [packs: A=%d V=%d]\n",
	    dp->length(),dp->pts,(unsigned int)dp->pos,demuxer->audio->packs,demuxer->video->packs);
    }
    else
	MSG_DBG2("DEMUX: Skip packet: len=%d  pts=%5.3f  pos=%u  [packs: A=%d V=%d]\n",
	    dp->length(),dp->pts,(unsigned int)dp->pos,demuxer->audio->packs,demuxer->video->packs);
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
    if (buffer) delete buffer;
/*  free_packs(ds); */
    if(mp_conf.verbose>2) {
	if(this==demuxer->audio) MSG_DBG3("ds_fill_buffer(d_audio) called\n");
	else if(this==demuxer->video) MSG_DBG3("ds_fill_buffer(d_video) called\n");
	else if(this==demuxer->sub) MSG_DBG3("ds_fill_buffer(d_sub) called\n");
	else MSG_DBG3("ds_fill_buffer(unknown %p) called\n",this);
    }
    while(1){
	if(packs){
	    Demuxer_Packet *p=first;
	    // copy useful data:
	    buffer=p->buffer();
	    buffer_pos=0;
	    buffer_size=p->length();
	    pos=p->pos;
	    dpos+=p->length(); // !!!
	    ++pack_no;
	    if(p->pts){
		pts=p->pts;
		pts_bytes=0;
	    }
	    pts_bytes+=p->length(); // !!!
	    flags=p->flags;
	    // mp_free packet:
	    bytes-=p->length();
	    current=p;
	    first=p->next;
	    if(!first) last=NULL;
	    --packs;
	    check_pin("demuxer",pin,DS_PIN);
	    return 1; //ds->buffer_size;
	}
	if(demuxer->audio->bytes>=MAX_PACK_BYTES){
	    MSG_ERR(MSGTR_TooManyAudioInBuffer,demuxer->audio->packs,demuxer->audio->bytes);
	    MSG_HINT(MSGTR_MaybeNI);
	    break;
	}
	if(demuxer->video->bytes>=MAX_PACK_BYTES){
	    MSG_ERR(MSGTR_TooManyVideoInBuffer,demuxer->video->packs,demuxer->video->bytes);
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
    buffer_pos=buffer_size=0;
    buffer=NULL;
    current=NULL;
    MSG_V("ds_fill_buffer: EOF reached (stream: %s)  \n",this==demuxer->audio?"audio":"video");
    eof=1;
    check_pin("demuxer",pin,DS_PIN);
    return 0;
}

int Demuxer_Stream::read_data(unsigned char* mem,int len) {
    int x;
    int _bytes=0;
    while(len>0){
	x=buffer_size-buffer_pos;
	if(x==0){
	    if(!fill_buffer()) return _bytes;
	} else {
	    if(x>len) x=len;
	    if(x<0) return _bytes; /* BAD!!! sometime happens. Broken stream, driver, gcc ??? */
	    if(mem) memcpy(mem+_bytes,&buffer[buffer_pos],x);
	    _bytes+=x;len-=x;buffer_pos+=x;
	}
    }
    return _bytes;
}

void Demuxer_Stream::free_packs() {
    Demuxer_Packet *dp=first;
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
    first=last=NULL;
    packs=0; // !!!!!
    bytes=0;
    if(current) delete current;
    current=NULL;
    buffer=NULL;
    buffer_pos=buffer_size;
    pts=0;
    pts_bytes=0;
}

void Demuxer_Stream::free_packs_until_pts(float _pts) {
    Demuxer_Packet *dp=first;
    unsigned _packs,_bytes;
    _packs=_bytes=0;
    while(dp) {
	Demuxer_Packet *dn=dp->next;
	if(dp->pts >= _pts) break;
	_packs++;
	_bytes+=dp->length();
	delete dp;
	dp=dn;
    }
    if(!dp) {
	if(asf_packet){
	    // mp_free unfinished .asf fragments:
	    delete asf_packet;
	    asf_packet=NULL;
	}
	first=last=NULL;
	packs=0; // !!!!!
	bytes=0;
	pts=0;
    } else {
	first=dp;
	packs-=_packs;
	bytes-=_bytes;
	pts=dp->pts;
    }
    if(current) delete current;
    current=NULL;
    buffer=NULL;
    buffer_pos=buffer_size;
    pts_bytes=0;
}

int Demuxer_Stream::get_packet(unsigned char **start){
    while(1){
	int len;
	if(buffer_pos>=buffer_size){
	  if(!fill_buffer()){
	    // EOF
	    *start = NULL;
	    return -1;
	  }
	}
	len=buffer_size-buffer_pos;
	*start = &buffer[buffer_pos];
	buffer_pos+=len;
	return len;
    }
}

int Demuxer_Stream::get_packet_sub(unsigned char **start){
    while(1){
	int len;
	if(buffer_pos>=buffer_size){
	  *start = NULL;
	  if(!packs) return -1; // no sub
	  if(!fill_buffer()) return -1; // EOF
	}
	len=buffer_size-buffer_pos;
	*start = &buffer[buffer_pos];
	buffer_pos+=len;
	return len;
    }
}

float Demuxer_Stream::get_next_pts() {
    while(!first) {
	if(demuxer->audio->bytes>=MAX_PACK_BYTES){
	    MSG_ERR(MSGTR_TooManyAudioInBuffer,demuxer->audio->packs,demuxer->audio->bytes);
	    MSG_HINT(MSGTR_MaybeNI);
	    return -1;
	}
	if(demuxer->video->bytes>=MAX_PACK_BYTES){
	    MSG_ERR(MSGTR_TooManyVideoInBuffer,demuxer->video->packs,demuxer->video->bytes);
	    MSG_HINT(MSGTR_MaybeNI);
	    return -1;
	}
	if(!demux_fill_buffer(demuxer,this)) return -1;
    }
    return first->pts;
}

} // namespace mpxp