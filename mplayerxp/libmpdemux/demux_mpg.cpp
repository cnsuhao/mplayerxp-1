#include "mpxp_config.h"
#include "osdep/mplib.h"
using namespace	usr;
/*
    MPG/VOB file parser for DEMUXER v2.5  by A'rpi/ESP-team
    Some code was borrowed from ffmpeg project by Nick.
    TODO: MPEG-TS and MPEG-ES support

    TODO: demuxer->movi_length
*/
#include <algorithm>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "mpxp_help.h"
#include "osdep/bswap.h"

#include "libmpstream2/stream.h"
#include "demuxer.h"
#include "demuxer_internal.h"
#include "parse_es.h"
#include "stheader.h"
#include "mp3_hdr.h"

#include "libmpcodecs/dec_audio.h"
#include "demux_msg.h"

static const int MAX_PS_PACKETSIZE=(224*1024);

enum {
    UNKNOWN         =0,
    VIDEO_MPEG1     =0x10000001,
    VIDEO_MPEG2     =0x10000002,
    VIDEO_MPEG4     =0x10000004,
    VIDEO_H264      =0x10000005,
    AUDIO_MP2       =0x50,
    AUDIO_MP3       =0x55,
    AUDIO_A52       =0x2000,
    AUDIO_DTS       =0x2001,
    AUDIO_LPCM_BE   =0x10001,
    AUDIO_AAC       =mmioFOURCC('M', 'P', '4', 'A')
};
static const int MPGPES_BAD_PTS=-1;

typedef int (*alt_demuxer_t)(Demuxer *demux,Demuxer_Stream *__ds);
struct mpg_demuxer_t : public Opaque {
    public:
	mpg_demuxer_t() {}
	virtual ~mpg_demuxer_t() {}

	float last_pts;
	float final_pts;
	int has_valid_timestamps;
	unsigned int es_map[0x40];	//es map of stream types (associated to the pes id) from 0xb0 to 0xef
	int num_a_streams;
	int a_stream_ids[MAX_A_STREAMS];
	int last_sub_pts;
	alt_demuxer_t alt_demuxer;
};

static struct mpg_stat_s {
 int num_elementary_packets100;
 int num_elementary_packets101;
 int num_elementary_packets1B3;
 int num_elementary_packets1B6;
 int num_elementary_packetsPES;
 int num_elementary_a_packets;
 int num_elementary_v_packets;
 int num_elementary_packets12x;
 int num_h264_slice;//combined slice
 int num_h264_dpa;//DPA Slice
 int num_h264_dpb;//DPB Slice
 int num_h264_dpc;//DPC Slice
 int num_h264_idr;//IDR Slice
 int num_h264_sps;
 int num_h264_pps;
 int num_mp3audio_packets;
}mpg_stat;

static unsigned int read_mpeg_timestamp(Stream* s,int c){
  int d,e;
  unsigned int pts;
  d=s->read(type_word);
  e=s->read(type_word);
  if( ((c&1)!=1) || ((d&1)!=1) || ((e&1)!=1) ){
    bad_pts:
    return MPGPES_BAD_PTS;
  }
  pts=(((c>>1)&7)<<30)|((d>>1)<<15)|(e>>1);
  /* NK: Ugly workaround for bad streams */
  if((int)pts<0) goto bad_pts;
  MSG_DBG3("{%d}\n",pts);
  return pts;
}

static int parse_psm(Demuxer *demux, int len) {
  unsigned char c, id, type;
  unsigned int plen, prog_len, es_map_len;
  mpg_demuxer_t *priv = static_cast<mpg_demuxer_t*>(demux->priv);

  MSG_DBG2("PARSE_PSM, len=%d\n", len);
  if(! len)
    return 0;

  c = demux->stream->read(type_byte);
  if(! (c & 0x80)) {
    demux->stream->skip( len - 1);  //not yet valid, discard
    return 0;
  }
  demux->stream->skip( 1);
  prog_len = demux->stream->read(type_word);		//length of program descriptors
  demux->stream->skip( prog_len);			//.. that we ignore
  es_map_len = demux->stream->read(type_word);		//length of elementary streams map
  es_map_len = std::min(es_map_len, len - prog_len - 8);	//sanity check
  while(es_map_len > 0) {
    type = demux->stream->read(type_byte);
    id = demux->stream->read(type_byte);
    if(id >= 0xB0 && id <= 0xEF && priv) {
      int idoffset = id - 0xB0;
      switch(type) {
	case 0x1:
	  priv->es_map[idoffset] = VIDEO_MPEG1;
	  break;
	case 0x2:
	  priv->es_map[idoffset] = VIDEO_MPEG2;
	  break;
	case 0x3:
	case 0x4:
	  priv->es_map[idoffset] = AUDIO_MP2;
	  break;
	case 0x0f:
	case 0x11:
	  priv->es_map[idoffset] = AUDIO_AAC;
	  break;
	case 0x10:
	  priv->es_map[idoffset] = VIDEO_MPEG4;
	  break;
	case 0x1b:
	  priv->es_map[idoffset] = VIDEO_H264;
	  break;
	case 0x81:
	  priv->es_map[idoffset] = AUDIO_A52;
	  break;
      }
      MSG_DBG2("PSM ES, id=0x%x, type=%x, stype: %x\n", id, type, priv->es_map[idoffset]);
    }
    plen = demux->stream->read(type_word);		//length of elementary stream descriptors
    plen = std::min(plen, es_map_len);			//sanity check
    demux->stream->skip( plen);			//skip descriptors for now
    es_map_len -= 4 + plen;
  }
  demux->stream->skip( 4);			//skip crc32
  return 1;
}

static void new_audio_stream(Demuxer *demux, int aid){
  if(!demux->get_sh_audio(aid)){
    mpg_demuxer_t *mpg_d=static_cast<mpg_demuxer_t*>(demux->priv);
    sh_audio_t* sh_a;
    demux->new_sh_audio(aid);
    sh_a = demux->get_sh_audio(aid);
    switch(aid & 0xE0){  // 1110 0000 b  (high 3 bit: type  low 5: id)
      case 0x00: sh_a->wtag=AUDIO_MP2;break;
      case 0xA0: sh_a->wtag=AUDIO_LPCM_BE;break;
      case 0x80: if((aid & 0xF8) == 0x88) sh_a->wtag=AUDIO_DTS;
		 else sh_a->wtag=AUDIO_A52;
		 break;
    }
    if (mpg_d) mpg_d->a_stream_ids[mpg_d->num_a_streams++] = aid;
  }
  if(demux->audio->id==-1) demux->audio->id=aid;
}

static int is_mpg_keyframe(uint32_t fourcc, int i,uint8_t* buf)
{
    int rval=DP_NONKEYFRAME;
    switch(fourcc)
    {
	case VIDEO_H264:
	    if((i & ~0x60) == 0x101 || (i & ~0x60) == 0x102 || (i & ~0x60) == 0x105) rval=DP_KEYFRAME;
	    break;
	case VIDEO_MPEG4:
	    if(i==0x1B6 && (buf[4]&0x3F)==0) rval=DP_KEYFRAME;
	    break;
	default: // VIDEO_MPEG1/VIDEO_MPEG2
	    if(i==0x1B3 || i==0x1B8) rval=DP_KEYFRAME;
	    break;
    }
    return rval;
}

static int demux_mpg_read_packet(Demuxer *demux,int id){
  int d;
  int len;
  unsigned char c=0;
  int pts=MPGPES_BAD_PTS;
  unsigned int dts=0;
  Demuxer_Stream *ds=NULL;
  mpg_demuxer_t *priv = static_cast<mpg_demuxer_t*>(demux->priv);
    binary_packet bp(1);

  MSG_DBG3("demux_read_packet: %X\n",id);

//  if(id==0x1F0){
//    demux->synced=0; // force resync after 0x1F0
//    return -1;
//}

//  if(id==0x1BA) return -1;
  len=demux->stream->read(type_word);
  MSG_DBG3("PACKET len=%d\n",len);
  if(id<0x1BC || id>=0x1F0 || id==0x1BE || id==0x1BF || id==0x1BA)
  {
    /* NK: We would be able to skip a few bytes (len-2) here but not every header (0x1BA)
       has explicitly given length an not every header is documented well.
       So let it burn cpu */
    return -1;
  }
//  if(len==62480){ demux->flags=0;return -1;} /* :) */
  if(len==0 || len>MAX_PS_PACKETSIZE){
    MSG_DBG2("Invalid PS packet len: %d\n",len);
    return -2;  // invalid packet !!!!!!
  }

  if(id==0x1BC) {
    parse_psm(demux, len);
    return 0;
  }

  while(len>0){   // Skip stuFFing bytes
    c=demux->stream->read(type_byte);--len;
    if(c!=0xFF)break;
  }
  if((c>>6)==1){  // Read (skip) STD scale & size value
    d=((c&0x1F)<<8)|demux->stream->read(type_byte);
    len-=2;
    c=demux->stream->read(type_byte);
  }
  // Read System-1 stream timestamps:
  if((c>>4)==2){
    pts=read_mpeg_timestamp(demux->stream,c);
    len-=4;
  } else
  if((c>>4)==3){
    pts=read_mpeg_timestamp(demux->stream,c);
    c=demux->stream->read(type_byte);
    if((c>>4)!=1) pts=0;
    dts=read_mpeg_timestamp(demux->stream,c);
    len-=4+1+4;
  } else
  if((c>>6)==2){
    int pts_flags;
    int hdrlen;
    // System-2 (.VOB) stream:
    if((c>>4)&3) MSG_WARN("Encrypted VOB found! That should never happens");

    c=demux->stream->read(type_byte); pts_flags=c>>6;
    c=demux->stream->read(type_byte); hdrlen=c;
    len-=2;
    MSG_DBG3("  hdrlen=%d  (len=%d)",hdrlen,len);
    if(hdrlen>len){ MSG_V("demux_mpg: invalid header length  \n"); return -1;}
    if(pts_flags==2 && hdrlen>=5){
      c=demux->stream->read(type_byte);
      pts=read_mpeg_timestamp(demux->stream,c);
      len-=5;hdrlen-=5;
    } else
    if(pts_flags==3 && hdrlen>=10){
      c=demux->stream->read(type_byte);
      pts=read_mpeg_timestamp(demux->stream,c);
      c=demux->stream->read(type_byte);
      dts=read_mpeg_timestamp(demux->stream,c);
      len-=10;hdrlen-=10;
    }
    len-=hdrlen;
    if(hdrlen>0) demux->stream->skip(hdrlen); // skip header bytes

    //============== DVD Audio sub-stream ======================
    if(id==0x1BD){
      int aid=demux->stream->read(type_byte);--len;
      if(len<3) return -1; // invalid audio packet

      // AID:
      // 0x20..0x3F  subtitle
      // 0x80..0x9F  AC3 audio
      // 0xA0..0xBF  PCM audio

      if((aid & 0xE0) == 0x20){
	// subtitle:
	aid&=0x1F;

	if(!demux->get_sh_sub(aid)){
	    MSG_V("==> Found subtitle: %d\n",aid);
	    demux->new_sh_sub(aid);
	}

	if(demux->sub->id==aid){
	    ds=demux->sub;
	}

      } else if((aid & 0xC0) == 0x80 || (aid & 0xE0) == 0x00) {

//        aid=128+(aid&0x7F);
	// aid=0x80..0xBF

	new_audio_stream(demux, aid);

      if(demux->audio->id==aid){
	int type;
	ds=demux->audio;
	if(!ds->sh) ds->sh=demux->get_sh_audio(aid);
	// READ Packet: Skip additional audio header data:
	c=demux->stream->read(type_byte);//num of frames
	type=demux->stream->read(type_byte);//startpos hi
	type=(type<<8)|demux->stream->read(type_byte);//startpos lo
	len-=3;
	if((aid&0xE0)==0xA0 && len>=3){
	  unsigned char* hdr;
	  // save audio header as codecdata!
	  if(!((sh_audio_t*)(ds->sh))->codecdata_len){
	      ((sh_audio_t*)(ds->sh))->codecdata=new unsigned char [3];
	      ((sh_audio_t*)(ds->sh))->codecdata_len=3;
	  }
	  hdr=((sh_audio_t*)(ds->sh))->codecdata;
	  // read LPCM header:
	  // emphasis[1], mute[1], rvd[1], frame number[5]:
	  hdr[0]=demux->stream->read(type_byte);
//          printf(" [%01X:%02d]",c>>5,c&31);
	  // quantization[2],freq[2],rvd[1],channels[3]
	  hdr[1]=demux->stream->read(type_byte);
//          printf("[%01X:%01X] ",c>>4,c&15);
	  // dynamic range control (0x80=off):
	  hdr[2]=demux->stream->read(type_byte);
//          printf("[%02X] ",c);
	  len-=3;
	  if(len<=0) MSG_V("End of packet while searching for PCM header\n");
	}
      } //  if(demux->audio->id==aid)

      } else MSG_V("Unknown 0x1BD substream: 0x%02X  \n",aid);

    } //if(id==0x1BD)

  } else {
    if(c!=0x0f){
      MSG_V("  {ERROR5,c=%d}  \n",c);
      return -1;  // invalid packet !!!!!!
    }
  }
  MSG_DBG3(" => len=%d\n",len);

//  if(len<=0 || len>MAX_PS_PACKETSIZE) return -1;  // Invalid packet size
  if(len<=0 || len>MAX_PS_PACKETSIZE){
    MSG_DBG2("Invalid PS data len: %d\n",len);
    return -1;  // invalid packet !!!!!!
  }

  if(id>=0x1C0 && id<=0x1DF){
    // mpeg audio
    int aid=id-0x1C0;
    new_audio_stream(demux, aid);
    if(demux->audio->id==aid){
      ds=demux->audio;
      if(!ds->sh) ds->sh=demux->get_sh_audio(aid);
      if(priv && ds->sh) {
	sh_audio_t *sh = (sh_audio_t *)ds->sh;
	if(priv->es_map[id - 0x1B0]) {
	  sh->wtag = priv->es_map[id - 0x1B0];
	  MSG_DBG2("ASSIGNED TO STREAM %d CODEC %x\n", id, priv->es_map[id - 0x1B0]);
	}
      }
    }
  } else
  if(id>=0x1E0 && id<=0x1EF){
    // mpeg video
    int aid=id-0x1E0;
    if(!demux->get_sh_video(aid)) demux->new_sh_video(aid);
    if(demux->video->id==-1) demux->video->id=aid;
    if(demux->video->id==aid){
      ds=demux->video;
      if(!ds->sh) ds->sh=demux->get_sh_video(aid);
      if(priv && ds->sh) {
	sh_video_t *sh = (sh_video_t *)ds->sh;
	if(priv->es_map[id - 0x1B0]) {
	  sh->fourcc = priv->es_map[id - 0x1B0];
	  MSG_DBG2("ASSIGNED TO STREAM %d CODEC %x\n", id, priv->es_map[id - 0x1B0]);
	}
      }
    }
  }

  if(ds){
    MSG_DBG2("DEMUX_MPG: Read %d data bytes from packet %04X\n",len,id);

    if(ds==ds->demuxer->sub) {
	if (pts == MPGPES_BAD_PTS) {
	    pts = priv->last_sub_pts;
	} else
	    priv->last_sub_pts = pts;
    }
    if(pts==MPGPES_BAD_PTS && ds->asf_packet)
    {
	Demuxer_Packet* dp=ds->asf_packet;
	dp->resize(dp->length()+len);
	bp=demux->stream->read(len); memcpy(dp->buffer()+dp->length(),bp.data(),bp.size());
    }
    else
    {
	sh_video_t *sh;
	if(ds->asf_packet) ds->add_packet(ds->asf_packet);
	Demuxer_Packet* dp=new(zeromem) Demuxer_Packet(len);
	bp=demux->stream->read(len); memcpy(dp->buffer(),bp.data(),bp.size());
	len=bp.size();
	dp->resize(len);
	dp->pts=pts/90000.0f;
	if(ds==demux->video)	sh=(sh_video_t *)ds->sh;
	else			sh=NULL;
	dp->flags=sh?is_mpg_keyframe(sh->fourcc,id,dp->buffer())?DP_KEYFRAME:DP_NONKEYFRAME:DP_NONKEYFRAME;
	dp->pos=demux->filepos;
	ds->asf_packet=dp;
	if (ds==ds->demuxer->sub) {
	    // Add sub packets at ones
	    ds->add_packet(ds->asf_packet);
	    ds->asf_packet=NULL;
	}
    }
//    ds_read_packet(ds,demux->stream,len,pts/90000.0f,demux->filepos,0);
//    if(ds==demux->sub) parse_dvdsub(ds->last->buffer,ds->last->len);
    return 1;
  }
  MSG_DBG2("DEMUX_MPG: Skipping %d data bytes from packet %04X\n",len,id);
  if(len<=2356) demux->stream->skip(len);
  return 0;
}

static int mpges_demux(Demuxer *demux,Demuxer_Stream *__ds){
  /* Elementary video stream */
  if(demux->stream->eof()) return 0;
  demux->filepos=demux->stream->tell();
  demux->video->read_packet(demux->stream,STREAM_BUFFER_SIZE,0,demux->filepos,DP_NONKEYFRAME);
  return 1;
}

static int mpgps_demux(Demuxer *demux,Demuxer_Stream *__ds){
    mpg_demuxer_t* mpg_d = static_cast<mpg_demuxer_t*>(demux->priv);
    if(mpg_d->alt_demuxer) return mpg_d->alt_demuxer(demux,__ds);
    unsigned int head=0;
    int skipped=0;
    int max_packs=256; // 512kbyte
    int ret=0;

// System stream
do{
  demux->filepos=demux->stream->tell();
  head=demux->stream->read(type_dword);
  if((head&0xFFFFFF00)!=0x100){
   // sync...
   demux->filepos-=skipped;
   while(1){
    int c=demux->stream->read(type_byte);
    if(c<0) break; //EOF
    head<<=8;
    if(head!=0x100){
      head|=c;
      if(mp_check_mp3_header(head,NULL,NULL,NULL,NULL)) ++mpg_stat.num_mp3audio_packets;
      ++skipped; //++demux->filepos;
      continue;
    }
    head|=c;
    break;
   }
   demux->filepos+=skipped;
  }
  if(demux->stream->eof()) break;
  // sure: head=0x000001XX
  MSG_DBG2("*** head=0x%X\n",head);
  if(demux->synced==0){
    if(head==0x1BA) demux->synced=1; //else
//    if(head==0x1BD || (head>=0x1C0 && head<=0x1EF)) demux->synced=3; // PES?
  } else
  if(demux->synced==1){
    if(head==0x1BB || head==0x1BD || (head>=0x1C0 && head<=0x1EF)){
      demux->synced=2;
      MSG_V("system stream synced at 0x%X (%d)!\n",demux->filepos,demux->filepos);
      mpg_stat.num_elementary_packets100=0; // requires for re-sync!
      mpg_stat.num_elementary_packets101=0; // requires for re-sync!
    } else demux->synced=0;
  } // else
  if(demux->synced>=2){
      ret=demux_mpg_read_packet(demux,head);
      if(!ret)
	if(--max_packs==0){
	  demux->stream->eof(1);
	  MSG_ERR(MSGTR_DoesntContainSelectedStream);
	  return 0;
	}
      if(demux->synced==3) demux->synced=(ret==1)?2:0; // PES detect
  } else {
    if(head>=0x100 && head<0x1B0){
      if(head==0x100) ++mpg_stat.num_elementary_packets100; else
      if(head==0x101) ++mpg_stat.num_elementary_packets101; else
      if(head>=0x120 && head<=0x12F) ++mpg_stat.num_elementary_packets12x;

      if((head&~0x60) == 0x101) ++mpg_stat.num_h264_slice; else
      if((head&~0x60) == 0x102) ++mpg_stat.num_h264_dpa; else
      if((head&~0x60) == 0x103) ++mpg_stat.num_h264_dpb; else
      if((head&~0x60) == 0x104) ++mpg_stat.num_h264_dpc; else
      if((head&~0x60) == 0x105 && head != 0x105) ++mpg_stat.num_h264_idr; else
      if((head&~0x60) == 0x107 && head != 0x107) ++mpg_stat.num_h264_sps; else
      if((head&~0x60) == 0x108 && head != 0x108) ++mpg_stat.num_h264_pps;

      MSG_DBG3("Opps... elementary video packet found: %03X\n",head);
    } else
    if((head>=0x1C0 && head<0x1F0) || head==0x1BD){
      if(head>=0x1C0 && head<=0x1DF) ++mpg_stat.num_elementary_a_packets;
      if(head>=0x1E0 && head<=0x1EF) ++mpg_stat.num_elementary_v_packets;
      ++mpg_stat.num_elementary_packetsPES;
      MSG_DBG3("Opps... PES packet found: %03X\n",head);
    } else
    {
      if(head==0x1B6) ++mpg_stat.num_elementary_packets1B6;
      if(head==0x1B3) ++mpg_stat.num_elementary_packets1B3;
    }
#if 1
    if( ( (mpg_stat.num_elementary_packets100>50 && mpg_stat.num_elementary_packets101>50) ||
	  (mpg_stat.num_elementary_packetsPES>50) ) && skipped>4000000){
	MSG_V("sync_mpeg_ps: seems to be ES/PES stream...\n");
	demux->stream->eof(1);
	break;
    }
    if(mpg_stat.num_mp3audio_packets>100 && mpg_stat.num_elementary_packets100<10){
	MSG_V("sync_mpeg_ps: seems to be MP3 stream...\n");
	demux->stream->eof(1);
	break;
    }
#endif
  }
} while(ret!=1);
  MSG_DBG2("demux: %d bad bytes skipped\n",skipped);
  if(demux->stream->eof()){
    MSG_V("MPEG Stream reached EOF\n");
    return 0;
  }
  return 1;
}

static void mpgps_seek(Demuxer *demuxer,const seek_args_t* seeka){
    Demuxer_Stream *d_audio=demuxer->audio;
    Demuxer_Stream *d_video=demuxer->video;
    sh_audio_t *sh_audio=reinterpret_cast<sh_audio_t*>(d_audio->sh);
    sh_video_t *sh_video=reinterpret_cast<sh_video_t*>(d_video->sh);

  //================= seek in MPEG ==========================
    off_t newpos=(seeka->flags&DEMUX_SEEK_SET)?demuxer->movi_start:demuxer->filepos;

    if(seeka->flags&DEMUX_SEEK_PERCENTS){
	// float seek 0..1
	newpos+=(demuxer->movi_end-demuxer->movi_start)*seeka->secs;
    } else {
	// time seek (secs)
	newpos+=2324*75*seeka->secs; // 174.3 kbyte/sec
    }

	if(newpos<demuxer->movi_start){
	    if(!(demuxer->stream->type()&Stream::Type_Program)) demuxer->movi_start=0; // for VCD
	    if(newpos<demuxer->movi_start) newpos=demuxer->movi_start;
	}

#ifdef _LARGEFILE_SOURCE
	newpos&=~((long long)STREAM_BUFFER_SIZE-1);  /* sector boundary */
#else
	newpos&=~(STREAM_BUFFER_SIZE-1);  /* sector boundary */
#endif
	demuxer->stream->seek(newpos);

	// re-sync video:
	videobuf_code_len=0; // reset ES stream buffer

	d_video->fill_buffer();
	if(sh_audio){
	  d_audio->fill_buffer();
//	  mpca_resync_stream(sh_audio->decoder);
	}

	while(1){
	  int i;
	  if(sh_audio && !d_audio->eof && d_video->pts && d_audio->pts){
	    float a_pts=d_audio->pts;
	    a_pts+=(d_audio->tell_pts()-sh_audio->a_in_buffer_len)/(float)sh_audio->i_bps;
	    if(d_video->pts>a_pts){
//	      mpca_skip_frame(sh_audio->decoder);  // sync audio
	      continue;
	    }
	  }
	  i=sync_video_packet(*d_video);
	  if(sh_video->fourcc == VIDEO_MPEG4) {
	    if(i==0x1B6) {			//vop (frame) startcode
	      int pos = videobuf_len;
	      if(!read_video_packet(*d_video)) break; // EOF
	      if((videobuffer[pos+4] & 0x3F) == 0) break;//I-frame
	    }
	  } else if(sh_video->fourcc == VIDEO_H264){
	    if((i & ~0x60) == 0x101 || (i & ~0x60) == 0x102 || (i & ~0x60) == 0x105) break;
	  } else { //default VIDEO_MPEG1/VIDEO_MPEG2
	    if(i==0x1B3 || i==0x1B8) break; // found it!
	  }
	  if(!i || !skip_video_packet(*d_video)) break; // EOF?
	}
}

/*
 headers:
 0x00000100		- picture_start_code
 0x00000101-0x000001AF	- slice_start_code
 0x000001B0-0x000001B1	- reserved
 0x000001B2		- user_data_start_code
 0x000001B3		- sequence_header_code
 0x000001B4		- sequence_error_code
 0x000001B5		- extension_start_code
 0x000001B6		- reserved
 0x000001B7		- sequence_end_code
 0x000001B8		- group_start_code
 0x000001B9-0x000001FF	- system_start_code
  0x000001B9		- ISO_111172_end_code
  0x000001BA		- pack_start_code
  0x000001BB		- system_header_start_code
  0x000001BC		- program_stream_map
  0x000001BD		- private_stream_1
  0x000001BE		- padding_stream
  0x000001BF		- private_stream_2
  0x000001C0-0x000001DF - audio_stream_prefixes
  0x000001E0-0x000001EF - video_stream_prefixes
  0x000001F0		- ECM_stream
  0x000001F1		- EMM_stream
  0x000001F2		- DSM_CC_stream
  0x000001F3		- ISO_13522_stream
  0x000001FF		- prog_stream_dir
*/
extern const demuxer_driver_t demux_mpgps;
static MPXP_Rc mpgps_probe(Demuxer*demuxer)
{
    uint32_t code,id;
    int pes=2,rval;
    uint32_t tmp;
    off_t tmppos;
    mpg_demuxer_t* mpg_d;

    mpg_d = new(zeromem) mpg_demuxer_t;
    demuxer->priv = mpg_d;

    code = demuxer->stream->read_le(type_dword);
    if(code == mmioFOURCC('R','I','F','F')) {
	demuxer->stream->read_le(type_dword); /*filesize */
	id=demuxer->stream->read_le(type_dword); /* "CDXA" */
	if(id == mmioFOURCC('C','D','X','A')) {
	    MSG_V("RIFF CDXA has been found\n");
	    id=demuxer->stream->read_le(type_dword); /* "fmt " */
	    if(id == mmioFOURCC('f','m','t',' ')) {
		tmp=demuxer->stream->read_le(type_dword); /* "head len" */
		demuxer->stream->skip(tmp);
		id=demuxer->stream->read_le(type_dword); /* "data" */
		if(id == mmioFOURCC('d','a','t','a')) {
		    demuxer->stream->read(type_dword); /* size of data */
		    code=demuxer->stream->read_le(type_dword);
		    while((bswap_32(code) & 0xffffff00) != 0x100) code=demuxer->stream->read_le(type_dword);
		}
		else { delete mpg_d; return MPXP_False; }
	    }
	    else { delete mpg_d; return MPXP_False; }
	}
	else { delete mpg_d; return MPXP_False; }
    }

    code = bswap_32(code);
    /* test stream only if stream is started from 0000001XX */
    if ((code & 0xffffff00) == 0x100) {
	demuxer->stream->seek(demuxer->stream->start_pos());
	memset(&mpg_stat,0,sizeof(struct mpg_stat_s));

	while(pes>=0){
	    /* try to pre-detect PES:*/
	    tmppos=demuxer->stream->tell();
	    tmp=demuxer->stream->read(type_dword);
	    if(tmp==0x1E0 || tmp==0x1C0){
		tmp=demuxer->stream->read(type_word);
		if(tmp>1 && tmp<=2048) pes=0; /* demuxer->synced=3; // PES...*/
	    }
	    demuxer->stream->seek(tmppos);

	    if(!pes) demuxer->synced=3; /* hack! */

	    mpg_stat.num_elementary_packets100=
	    mpg_stat.num_elementary_packets101=
	    mpg_stat.num_elementary_packets1B6=
	    mpg_stat.num_elementary_packets12x=
	    mpg_stat.num_elementary_packetsPES=
	    mpg_stat.num_h264_slice=
	    mpg_stat.num_h264_dpa=
	    mpg_stat.num_h264_dpb=
	    mpg_stat.num_h264_dpc=
	    mpg_stat.num_h264_idr=
	    mpg_stat.num_h264_sps=
	    mpg_stat.num_h264_pps=
	    mpg_stat.num_mp3audio_packets=0;

	    rval=mpgps_demux(demuxer,demuxer->video);
	    MSG_V("MPEG packet stats: p100: %d  p101: %d p1B3: %d p1B6: %d p12x: %d PES: %d\n"
		"MPEG packet stats: sli: %d dpa: %d dpb: %d dpc: %d idr: %d sps: %d pps: %d\n"
		"MPEG packet stats: MP3: %d a: %d v: %d\n",
		mpg_stat.num_elementary_packets100,mpg_stat.num_elementary_packets101,
		mpg_stat.num_elementary_packets1B3,mpg_stat.num_elementary_packets1B6,
		mpg_stat.num_elementary_packets12x,mpg_stat.num_elementary_packetsPES,
		mpg_stat.num_h264_slice,mpg_stat.num_h264_dpa,mpg_stat.num_h264_dpb,mpg_stat.num_h264_dpc,
		mpg_stat.num_h264_idr,mpg_stat.num_h264_sps,mpg_stat.num_h264_pps,
		mpg_stat.num_mp3audio_packets,
		mpg_stat.num_elementary_a_packets,
		mpg_stat.num_elementary_v_packets);

	    if(rval){
		demuxer->file_format=Demuxer::Type_MPEG_PS;
	    } else {

		/*MPEG packet stats: p100: 458  p101: 458  PES: 0  MP3: 1103  (.m2v)*/
		if(mpg_stat.num_mp3audio_packets>50 && mpg_stat.num_mp3audio_packets>2*mpg_stat.num_elementary_packets100
		    && abs(mpg_stat.num_elementary_packets100-mpg_stat.num_elementary_packets101)>2)
		{
		    MSG_V("MPEG: Seems to be an .mp3\n");
		    break; /* it's .MP3 */
		}
		/* some hack to get meaningfull error messages to our unhappy users:*/
		if(mpg_stat.num_elementary_packets100>=2 && mpg_stat.num_elementary_packets101>=2 &&
		    abs(mpg_stat.num_elementary_packets101+8-mpg_stat.num_elementary_packets100)<16){
			if(mpg_stat.num_elementary_packetsPES>=4 && mpg_stat.num_elementary_packetsPES>=mpg_stat.num_elementary_packets100-4){
			    --pes;
			    continue; /* tricky... */
			}
			demuxer->file_format=Demuxer::Type_MPEG_ES; /*  <-- hack is here :) */
		} else if(mpg_stat.num_elementary_packets1B6>3 && mpg_stat.num_elementary_packets12x>=1 &&
		    mpg_stat.num_elementary_packetsPES==0 && mpg_stat.num_elementary_packets100<=mpg_stat.num_elementary_packets12x &&
		    demuxer->synced<2) {
			/* fuzzy mpeg4-es detection. do NOT enable without heavy testing of mpeg formats detection! */
			demuxer->file_format=Demuxer::Type_MPEG4_ES;
		    } else if((mpg_stat.num_h264_slice>3 || (mpg_stat.num_h264_dpa>3 && mpg_stat.num_h264_dpb>3 && mpg_stat.num_h264_dpc>3)) &&
		    /* FIXME mpg_stat.num_h264_sps>=1 && */ mpg_stat.num_h264_pps>=1 && mpg_stat.num_h264_idr>=1 &&
		    mpg_stat.num_elementary_packets1B6==0 && mpg_stat.num_elementary_packetsPES==0 &&
		    demuxer->synced<2) {
			/* fuzzy h264-es detection. do NOT enable without heavy testing of mpeg formats detection!*/
			demuxer->file_format=Demuxer::Type_H264_ES;
		    } else {
			if(demuxer->synced==2)
			    MSG_ERR("MPEG: " MSGTR_MissingVideoStreamBug);
			else {
			    MSG_V("Not MPEG System Stream format...\n");
			    return MPXP_False;
			}
		    }
	    }
	    break;
	}
	if( demuxer->file_format==Demuxer::Type_MPEG_ES ||
	    demuxer->file_format==Demuxer::Type_MPEG4_ES ||
	    demuxer->file_format==Demuxer::Type_H264_ES){ /* little hack, see above! */
		mpg_d->alt_demuxer=mpges_demux;
		if(!demuxer->get_sh_video()) demuxer->new_sh_video();
		if(demuxer->video->id==-1) demuxer->video->id=0;
		demuxer->video->sh=demuxer->get_sh_video();
		demuxer->stream->seek(demuxer->stream->start_pos());
		return mpges_demux(demuxer,demuxer->video)?MPXP_Ok:MPXP_False;
	} else {
	    /*
	    NK: Main hack is here !!!
	    We have something packetized - means both audio and video should be present!
	    But some streams are "badly" interleaved.
	    Since mpgps_demux() reads stream until first audio or video packet only
	    - we need force stream reading several times at least.
	    */
	    unsigned attempts=64;
	    while((!demuxer->video->sh || !demuxer->audio->sh) && attempts) {
		mpgps_demux(demuxer,demuxer->video); /* try it again */
		attempts--;
	    }
	    return MPXP_Ok;
	}
    }
    delete mpg_d;
    return MPXP_False;
}

static Opaque* mpgps_open(Demuxer*demuxer)
{
    sh_video_t *sh_video=reinterpret_cast<sh_video_t*>(demuxer->video->sh);
    mpg_demuxer_t* mpg_d=static_cast<mpg_demuxer_t*>(demuxer->priv);

    if(!sh_video)	MSG_WARN("MPEG: " MSGTR_MissingVideoStream);
    else		sh_video->ds=demuxer->video;

    mpg_d->has_valid_timestamps = 1;
    mpg_d->num_a_streams = 0;

    if(demuxer->audio->id!=-2) {
	if(!demuxer->audio->fill_buffer()){
	    MSG_WARN("MPEG: " MSGTR_MissingAudioStream);
	    demuxer->audio->sh=NULL;
	} else {
	    sh_audio_t *sh_audio=reinterpret_cast<sh_audio_t*>(demuxer->audio->sh);
	    sh_audio->ds=demuxer->audio;
	}
    }
    check_pin("demuxer",demuxer->pin,DEMUX_PIN);
    return mpg_d;
}

static void mpgps_close(Demuxer*demuxer)
{
    mpg_demuxer_t* mpg_d = static_cast<mpg_demuxer_t*>(demuxer->priv);
    if (mpg_d) delete mpg_d;
}

static MPXP_Rc mpgps_control(const Demuxer *demuxer,int cmd,any_t*arg)
{
    mpg_demuxer_t *mpg_d=static_cast<mpg_demuxer_t*>(demuxer->priv);
    switch(cmd) {
	case Demuxer::Switch_Audio:
	    if (mpg_d && mpg_d->num_a_streams > 1 && demuxer->audio && demuxer->audio->sh) {
		Demuxer_Stream *d_audio = demuxer->audio;
		sh_audio_t *sh_audio = reinterpret_cast<sh_audio_t*>(d_audio->sh);
		sh_audio_t *sh_a=NULL;
		int i;
		if (*((int*)arg) < 0) {
		    for (i = 0; i < mpg_d->num_a_streams; i++) {
			if (d_audio->id == mpg_d->a_stream_ids[i]) break;
		    }
		    do {
			i = (i+1) % mpg_d->num_a_streams;
			sh_a = demuxer->get_sh_audio(mpg_d->a_stream_ids[i]);
		    } while (sh_a->wtag != sh_audio->wtag);
		} else {
		    for (i = 0; i < mpg_d->num_a_streams; i++)
			if (*((int*)arg) == mpg_d->a_stream_ids[i]) break;
			if (i < mpg_d->num_a_streams)
			    sh_a = demuxer->get_sh_audio(*((int*)arg));
			if (sh_a->wtag != sh_audio->wtag)
			    i = mpg_d->num_a_streams;
		}
		if (i < mpg_d->num_a_streams && d_audio->id != mpg_d->a_stream_ids[i]) {
		    d_audio->id = mpg_d->a_stream_ids[i];
		    d_audio->sh = sh_a;
		    d_audio->free_packs();
		}
	    }
	    *((int*)arg) = demuxer->audio->id;
	    return MPXP_Ok;
    }
    return MPXP_Unknown;
}

extern const demuxer_driver_t demux_mpgps =
{
    "mpg",
    "MPG/VOB PS (Packet stream) parser",
    ".mpg",
    NULL,
    mpgps_probe,
    mpgps_open,
    mpgps_demux,
    mpgps_seek,
    mpgps_close,
    mpgps_control
};
